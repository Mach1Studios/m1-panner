#!/usr/bin/env bash

# Test harness for the Intel macOS helper-socket SIGPIPE fix.
#
# Modes:
#   preflight  Check whether the AAX plug-ins, helper apps, and launch agents
#              needed for the Pro Tools repro are installed and signed.
#   probe      Compile and run a tiny x86_64 socket client against a fake helper
#              socket that accepts and closes immediately.
#   pro-tools  Run the same fake helper socket while you open a session in
#              Pro Tools Developer, with focused unified-log capture.
#
# Run with bash if the executable bit is not set:
#   bash Resources/test_sigpipe_helper_socket.sh probe
#   bash Resources/test_sigpipe_helper_socket.sh pro-tools --session "/path/to/session.ptx"

set -euo pipefail

SOCKET_PATH="${M1_HELPER_SOCKET:-/tmp/com.mach1.spatial.helper.socket}"
DEFAULT_PT_DEV_APP="/Applications/Pro Tools Developer.app"
PT_DEV_APP="${PT_DEV_APP:-$DEFAULT_PT_DEV_APP}"
TARGET_ARCH="x86_64"
LAUNCH_ARCH="native"
SESSION_PATH=""
DURATION_SECONDS=0
ASSUME_YES=0
RUN_UNSAFE_BASELINE=1
WORK_ROOT="${M1_SIGPIPE_WORK_ROOT:-${TMPDIR:-/tmp}}"
LOG_ROOT="${M1_SIGPIPE_LOG_ROOT:-${TMPDIR:-/tmp}}"

BAD_SOCKET_PID=""
LOG_STREAM_PID=""
WORK_DIR=""
LOG_DIR=""
CREATED_SOCKET=0

usage() {
    cat <<'EOF'
Usage:
  test_sigpipe_helper_socket.sh preflight [options]
  test_sigpipe_helper_socket.sh probe [options]
  test_sigpipe_helper_socket.sh pro-tools [options]

Options:
  --arch x86_64|native       Build/run probe for Intel slice or native host.
                             Default: x86_64
  --no-unsafe-baseline       Skip the intentionally unsafe SIGPIPE baseline.
  --app PATH                 Pro Tools app path.
                             Default: /Applications/Pro Tools Developer.app
  --launch-arch ARCH         Launch Pro Tools using this architecture:
                             x86_64, arm64, or native. Default: native.
  --session PATH             Optional Pro Tools session to open.
  --duration SECONDS         In pro-tools mode, wait this long instead of
                             prompting. Default: prompt.
  --yes                      Replace the helper socket without prompting.
  -h, --help                 Show this help.

Examples:
  bash Resources/test_sigpipe_helper_socket.sh preflight
  bash Resources/test_sigpipe_helper_socket.sh probe
  bash Resources/test_sigpipe_helper_socket.sh pro-tools --session "$HOME/Desktop/Repro.ptx"

Environment:
  M1_HELPER_SOCKET        Override helper socket path.
  M1_SIGPIPE_WORK_ROOT    Override temporary build/work directory root.
  M1_SIGPIPE_LOG_ROOT     Override Pro Tools log output root.
EOF
}

die() {
    echo "ERROR: $*" >&2
    exit 1
}

info() {
    echo "[m1-sigpipe-test] $*"
}

warn() {
    echo "[m1-sigpipe-test] WARNING: $*" >&2
}

cleanup() {
    local status=$?

    if [[ -n "${LOG_STREAM_PID}" ]]; then
        kill "${LOG_STREAM_PID}" >/dev/null 2>&1 || true
        wait "${LOG_STREAM_PID}" >/dev/null 2>&1 || true
    fi

    if [[ -n "${BAD_SOCKET_PID}" ]]; then
        kill "${BAD_SOCKET_PID}" >/dev/null 2>&1 || true
        wait "${BAD_SOCKET_PID}" >/dev/null 2>&1 || true
    fi

    if [[ "${CREATED_SOCKET}" -eq 1 && -S "${SOCKET_PATH}" ]]; then
        rm -f "${SOCKET_PATH}"
    fi

    if [[ -n "${WORK_DIR}" && -d "${WORK_DIR}" ]]; then
        rm -rf "${WORK_DIR}"
    fi

    exit "${status}"
}

trap cleanup EXIT INT TERM

ensure_macos() {
    [[ "$(uname -s)" == "Darwin" ]] || die "This test script is macOS-only."
}

print_check() {
    local status="$1"
    local message="$2"
    printf '[%s] %s\n' "${status}" "${message}"
}

bundle_binary_path() {
    local bundle_path="$1"
    local binary_name="$2"
    echo "${bundle_path}/Contents/MacOS/${binary_name}"
}

check_bundle_arch() {
    local binary_path="$1"
    local expected_arch="$2"

    if [[ ! -f "${binary_path}" ]]; then
        print_check "FAIL" "Missing binary: ${binary_path}"
        return 1
    fi

    local archs
    archs="$(lipo -archs "${binary_path}" 2>/dev/null || true)"
    if [[ " ${archs} " == *" ${expected_arch} "* ]]; then
        print_check "OK" "${binary_path} contains ${expected_arch} (${archs})"
        return 0
    fi

    print_check "FAIL" "${binary_path} does not contain ${expected_arch} (${archs:-unknown archs})"
    return 1
}

check_codesign() {
    local path="$1"

    if codesign --verify --deep --strict "${path}" >/dev/null 2>&1; then
        print_check "OK" "codesign verify passed: ${path}"
        return 0
    fi

    print_check "FAIL" "codesign verify failed: ${path}"
    codesign --verify --deep --strict --verbose=2 "${path}" 2>&1 | sed 's/^/      /' || true
    return 1
}

check_entitlements_hint() {
    local path="$1"
    local label="$2"
    local entitlements

    entitlements="$(codesign -d --entitlements :- "${path}" 2>/dev/null || true)"
    if [[ -z "${entitlements}" ]]; then
        print_check "WARN" "No entitlements dump available for ${label}"
        return 0
    fi

    if echo "${entitlements}" | grep -q "group.com.mach1.spatial.shared"; then
        print_check "OK" "${label} has Mach1 shared app group entitlement"
    else
        print_check "WARN" "${label} does not show group.com.mach1.spatial.shared entitlement"
    fi
}

launch_agent_program_path() {
    local label="$1"
    local plist_path="/Library/LaunchAgents/${label}.plist"

    [[ -f "${plist_path}" ]] || return 0
    /usr/libexec/PlistBuddy -c "Print :ProgramArguments:0" "${plist_path}" 2>/dev/null || true
}

codesign_target_for_program() {
    local program_path="$1"

    case "${program_path}" in
        *.app/Contents/MacOS/*)
            echo "${program_path%%.app/Contents/MacOS/*}.app"
            ;;
        *)
            echo "${program_path}"
            ;;
    esac
}

check_launch_agent() {
    local label="$1"
    local plist_path="/Library/LaunchAgents/${label}.plist"
    local user_domain="gui/$(id -u)"

    if [[ -f "${plist_path}" ]]; then
        print_check "OK" "LaunchAgent exists: ${plist_path}"
        if plutil -lint "${plist_path}" >/dev/null 2>&1; then
            print_check "OK" "LaunchAgent plist is valid: ${label}"
        else
            print_check "FAIL" "LaunchAgent plist is invalid: ${plist_path}"
        fi
    else
        print_check "FAIL" "Missing LaunchAgent: ${plist_path}"
    fi

    if launchctl print "${user_domain}/${label}" >/dev/null 2>&1; then
        print_check "OK" "LaunchAgent is loaded for current user: ${label}"
    else
        print_check "WARN" "LaunchAgent is not currently loaded for current user: ${label}"
    fi
}

check_launch_agent_program() {
    local label="$1"
    local expected_arch="$2"
    local program_path
    program_path="$(launch_agent_program_path "${label}")"

    if [[ -z "${program_path}" ]]; then
        print_check "FAIL" "Could not read ProgramArguments[0] for ${label}"
        return 1
    fi

    if [[ ! -f "${program_path}" ]]; then
        print_check "FAIL" "LaunchAgent program missing for ${label}: ${program_path}"
        return 1
    fi

    print_check "OK" "LaunchAgent program exists for ${label}: ${program_path}"
    check_bundle_arch "${program_path}" "${expected_arch}" || return 1

    local signing_target
    signing_target="$(codesign_target_for_program "${program_path}")"
    check_codesign "${signing_target}" || return 1
    check_entitlements_hint "${signing_target}" "$(basename "${signing_target}")"
}

run_preflight_mode() {
    ensure_macos

    local failures=0
    local avid_plugin_dir="/Library/Application Support/Avid/Audio/Plug-Ins"
    local panner_aax="${avid_plugin_dir}/M1-Panner.aaxplugin"
    local monitor_aax="${avid_plugin_dir}/M1-Monitor.aaxplugin"

    echo "=== M1 Pro Tools Install Preflight ==="
    echo "Checking AAX plug-ins, helper apps, launch agents, x86_64 slices, and codesign."
    echo

    for bundle in "${panner_aax}" "${monitor_aax}"; do
        if [[ -d "${bundle}" ]]; then
            print_check "OK" "AAX bundle installed: ${bundle}"
        else
            print_check "FAIL" "AAX bundle missing: ${bundle}"
            failures=$((failures + 1))
            continue
        fi

        local binary_name
        binary_name="$(basename "${bundle}" .aaxplugin)"
        check_bundle_arch "$(bundle_binary_path "${bundle}" "${binary_name}")" "x86_64" || failures=$((failures + 1))
        check_codesign "${bundle}" || failures=$((failures + 1))
    done

    echo
    check_launch_agent "com.mach1.spatial.helper"
    check_launch_agent_program "com.mach1.spatial.helper" "x86_64" || failures=$((failures + 1))
    echo
    check_launch_agent "com.mach1.spatial.orientationmanager"
    check_launch_agent_program "com.mach1.spatial.orientationmanager" "x86_64" || failures=$((failures + 1))

    echo
    if [[ "${failures}" -eq 0 ]]; then
        print_check "OK" "Preflight completed without hard failures."
    else
        print_check "FAIL" "Preflight found ${failures} hard failure(s). Fix install/signing before trusting a Pro Tools repro."
    fi

    return "${failures}"
}

confirm_socket_replacement() {
    if [[ -e "${SOCKET_PATH}" && "${ASSUME_YES}" -ne 1 ]]; then
        echo "The helper socket already exists:"
        echo "  ${SOCKET_PATH}"
        echo
        echo "For this test, the script replaces it with a fake helper socket."
        echo "Quit m1-system-helper first if it is running, then continue."
        read -r -p "Replace this socket for the test? [y/N] " answer
        case "${answer}" in
            y|Y|yes|YES) ;;
            *) die "Aborted before replacing helper socket." ;;
        esac
    fi
}

start_bad_socket() {
    command -v python3 >/dev/null 2>&1 || die "python3 is required to run the fake helper socket."

    confirm_socket_replacement

    rm -f "${SOCKET_PATH}"
    CREATED_SOCKET=1
    mkdir -p "${WORK_ROOT}"
    WORK_DIR="$(mktemp -d "${WORK_ROOT%/}/m1-sigpipe-test.XXXXXX")"

    python3 - "${SOCKET_PATH}" >"${WORK_DIR}/bad_socket.log" 2>&1 <<'PY' &
import os
import signal
import socket
import sys

path = sys.argv[1]

try:
    os.unlink(path)
except FileNotFoundError:
    pass

signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))
signal.signal(signal.SIGINT, lambda *_: sys.exit(0))

server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(path)
os.chmod(path, 0o666)
server.listen(64)
print(f"BAD_SOCKET_READY {path}", flush=True)

while True:
    conn, _ = server.accept()
    # Close immediately to emulate a stale/restarting helper endpoint. A client
    # that writes without SO_NOSIGPIPE can be terminated by SIGPIPE here.
    conn.close()
PY

    BAD_SOCKET_PID=$!

    for _ in {1..50}; do
        if [[ -S "${SOCKET_PATH}" ]]; then
            info "Fake helper socket listening at ${SOCKET_PATH} (pid ${BAD_SOCKET_PID})"
            return 0
        fi
        sleep 0.1
    done

    if [[ -f "${WORK_DIR}/bad_socket.log" ]]; then
        echo "Fake socket log:" >&2
        cat "${WORK_DIR}/bad_socket.log" >&2
    fi
    die "Fake helper socket did not become ready. See ${WORK_DIR}/bad_socket.log"
}

compile_probe() {
    command -v clang++ >/dev/null 2>&1 || die "clang++ is required for probe mode."

    local src="${WORK_DIR}/sigpipe_probe.cpp"
    local bin="${WORK_DIR}/sigpipe_probe"

    cat > "${src}" <<'CPP'
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "usage: sigpipe_probe SOCKET_PATH protected|unsafe\n";
        return 64;
    }

    const char* socketPath = argv[1];
    const std::string mode = argv[2];
    const bool protectedMode = mode == "protected";

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << "\n";
        return 1;
    }

#if defined(__APPLE__) && defined(__x86_64__)
    if (protectedMode) {
        int noSigPipe = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe)) != 0) {
            std::cerr << "setsockopt(SO_NOSIGPIPE) failed: " << std::strerror(errno) << "\n";
            close(sockfd);
            return 1;
        }
    }
#else
    if (protectedMode) {
        std::cerr << "protected mode requested, but this binary is not the x86_64 macOS slice\n";
    }
#endif

    sockaddr_un addr {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "connect failed: " << std::strerror(errno) << "\n";
        close(sockfd);
        return 2;
    }

    usleep(200000);

    const char* ping = "PING\n";
    const ssize_t sent = send(sockfd, ping, std::strlen(ping), 0);
    if (sent < 0) {
        std::cerr << "send failed normally: errno=" << errno
                  << " (" << std::strerror(errno) << ")\n";
        close(sockfd);
        return 3;
    }

    std::cout << "send succeeded: bytes=" << sent << "\n";
    close(sockfd);
    return 0;
}
CPP

    local arch_flags=()
    if [[ "${TARGET_ARCH}" != "native" ]]; then
        arch_flags=(-arch "${TARGET_ARCH}")
    fi

    clang++ -std=c++17 -Wall -Wextra -O0 "${arch_flags[@]}" "${src}" -o "${bin}"
    echo "${bin}"
}

run_arch() {
    if [[ "${TARGET_ARCH}" == "x86_64" ]]; then
        arch -x86_64 "$@"
    else
        "$@"
    fi
}

run_probe_mode() {
    ensure_macos
    start_bad_socket

    local probe_bin
    probe_bin="$(compile_probe)"

    info "Probe binary: ${probe_bin}"
    file "${probe_bin}" || true

    if [[ "${RUN_UNSAFE_BASELINE}" -eq 1 ]]; then
        info "Running unsafe baseline. Exit 141 means SIGPIPE reproduced."
        set +e
        run_arch "${probe_bin}" "${SOCKET_PATH}" unsafe
        local unsafe_status=$?
        set -e
        info "Unsafe baseline exit code: ${unsafe_status}"
    fi

    info "Running protected probe. Expected: no process death; EPIPE is OK."
    set +e
    run_arch "${probe_bin}" "${SOCKET_PATH}" protected
    local protected_status=$?
    set -e

    if [[ "${protected_status}" -eq 141 ]]; then
        die "Protected probe still died with SIGPIPE."
    fi

    info "Protected probe exit code: ${protected_status}"
    info "PASS: protected probe did not terminate from SIGPIPE."
}

start_log_capture() {
    mkdir -p "${LOG_ROOT}"
    LOG_DIR="${LOG_ROOT%/}/m1-protools-sigpipe-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "${LOG_DIR}"

    local stream_log="${LOG_DIR}/focused-log-stream.log"
    local predicate='process == "Pro Tools" OR process == "m1-system-helper" OR process == "m1-orientationmanager" OR eventMessage CONTAINS[c] "com.mach1.spatial" OR eventMessage CONTAINS[c] "SIGPIPE" OR eventMessage CONTAINS[c] "SO_NOSIGPIPE"'

    info "Capturing focused unified log to ${stream_log}"
    log stream --style compact --predicate "${predicate}" >"${stream_log}" 2>&1 &
    LOG_STREAM_PID=$!
}

validate_session_path() {
    [[ -e "${SESSION_PATH}" ]] || die "Session path does not exist: ${SESSION_PATH}"
    [[ -f "${SESSION_PATH}" ]] || die "Session path is not a file: ${SESSION_PATH}"
    [[ -s "${SESSION_PATH}" ]] || die "Session file is empty: ${SESSION_PATH}. Recreate it from the template in Pro Tools before running this test."

    case "${SESSION_PATH}" in
        *.ptx|*.ptxt) ;;
        *) warn "Session path does not end in .ptx or .ptxt: ${SESSION_PATH}" ;;
    esac
}

app_executable_path() {
    local app_path="$1"
    local executable

    executable="$(defaults read "${app_path}/Contents/Info" CFBundleExecutable 2>/dev/null || true)"
    [[ -n "${executable}" ]] || return 1
    echo "${app_path}/Contents/MacOS/${executable}"
}

validate_launch_arch() {
    [[ "${LAUNCH_ARCH}" == "native" ]] && return 0

    local executable_path
    executable_path="$(app_executable_path "${PT_DEV_APP}")" || die "Could not read executable name for: ${PT_DEV_APP}"
    [[ -f "${executable_path}" ]] || die "Pro Tools executable not found: ${executable_path}"

    local archs
    archs="$(lipo -archs "${executable_path}" 2>/dev/null || true)"
    if [[ " ${archs} " != *" ${LAUNCH_ARCH} "* ]]; then
        die "${PT_DEV_APP} cannot launch as ${LAUNCH_ARCH}; executable archs are: ${archs:-unknown}. Use a Pro Tools app with that slice."
    fi
}

launch_pro_tools() {
    local open_args=()

    if [[ "${LAUNCH_ARCH}" != "native" ]]; then
        open_args+=(--arch "${LAUNCH_ARCH}")
    fi

    if [[ -n "${SESSION_PATH}" ]]; then
        /usr/bin/open "${open_args[@]}" -a "${PT_DEV_APP}" "${SESSION_PATH}"
    else
        /usr/bin/open "${open_args[@]}" "${PT_DEV_APP}"
    fi
}

run_pro_tools_mode() {
    ensure_macos

    [[ -d "${PT_DEV_APP}" ]] || die "Pro Tools app not found at: ${PT_DEV_APP}"
    validate_launch_arch
    if [[ -n "${SESSION_PATH}" ]]; then
        validate_session_path
    fi

    echo "Running install preflight before Pro Tools test..."
    if ! run_preflight_mode; then
        warn "Preflight reported hard failures. Continuing because this may be intentional during development."
    fi
    echo

    start_bad_socket
    start_log_capture

    info "Launching Pro Tools:"
    info "  ${PT_DEV_APP}"
    info "  launch arch: ${LAUNCH_ARCH}"
    launch_pro_tools

    echo
    echo "Now reproduce the issue in Pro Tools:"
    echo "  1. Make sure the fixed M1-Panner AAX build is installed."
    echo "  2. Open the session that used to crash with the active plug-in."
    echo "  3. Let Pro Tools finish opening, or wait for a crash."
    echo

    if [[ "${DURATION_SECONDS}" -gt 0 ]]; then
        info "Waiting ${DURATION_SECONDS} seconds..."
        sleep "${DURATION_SECONDS}"
    else
        read -r -p "Press Return after the test completes or Pro Tools exits..."
    fi

    local summary_log="${LOG_DIR}/summary-last-10m.log"
    local summary_predicate='eventMessage CONTAINS[c] "Pro Tools" OR eventMessage CONTAINS[c] "com.mach1.spatial" OR eventMessage CONTAINS[c] "m1-system-helper" OR eventMessage CONTAINS[c] "m1-orientationmanager" OR eventMessage CONTAINS[c] "SIGPIPE"'
    log show --last 10m --style compact --predicate "${summary_predicate}" >"${summary_log}" 2>&1 || true

    echo
    info "Logs saved:"
    info "  ${LOG_DIR}/focused-log-stream.log"
    info "  ${summary_log}"
    echo
    echo "Pass condition:"
    echo "  No line like: Pro Tools[...] exited due to SIGPIPE"
    echo
    echo "Useful check:"
    echo "  log show --last 10m --predicate 'eventMessage CONTAINS \"SIGPIPE\"'"
}

parse_args() {
    [[ $# -gt 0 ]] || { usage; exit 64; }

    if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        usage
        exit 0
    fi

    MODE="$1"
    shift

    while [[ $# -gt 0 ]]; do
        case "$1" in
            --arch)
                [[ $# -ge 2 ]] || die "--arch requires x86_64 or native"
                TARGET_ARCH="$2"
                shift 2
                ;;
            --no-unsafe-baseline)
                RUN_UNSAFE_BASELINE=0
                shift
                ;;
            --app)
                [[ $# -ge 2 ]] || die "--app requires a path"
                PT_DEV_APP="$2"
                shift 2
                ;;
            --launch-arch)
                [[ $# -ge 2 ]] || die "--launch-arch requires x86_64, arm64, or native"
                LAUNCH_ARCH="$2"
                shift 2
                ;;
            --session)
                [[ $# -ge 2 ]] || die "--session requires a path"
                SESSION_PATH="$2"
                shift 2
                ;;
            --duration)
                [[ $# -ge 2 ]] || die "--duration requires seconds"
                DURATION_SECONDS="$2"
                shift 2
                ;;
            --yes)
                ASSUME_YES=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
    done

    case "${TARGET_ARCH}" in
        x86_64|native) ;;
        *) die "--arch must be x86_64 or native" ;;
    esac

    case "${LAUNCH_ARCH}" in
        x86_64|arm64|native) ;;
        *) die "--launch-arch must be x86_64, arm64, or native" ;;
    esac

    case "${MODE}" in
        preflight) run_preflight_mode ;;
        probe) run_probe_mode ;;
        pro-tools) run_pro_tools_mode ;;
        *) usage; exit 64 ;;
    esac
}

parse_args "$@"
