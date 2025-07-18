#include "M1SystemHelperManager.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <cstring>  // for strerror

#ifdef __APPLE__
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #include <windows.h>
    #include <winsvc.h>
    #include <tchar.h>
#else  // Linux
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
#endif

namespace Mach1 {

// =============================================================================
// PLATFORM-SPECIFIC CONSTANTS
// =============================================================================

#ifdef __APPLE__
    const char* const M1SystemHelperManager::SERVICE_LABEL = "com.mach1.spatial.helper";
    const char* const M1SystemHelperManager::SOCKET_PATH = "/tmp/com.mach1.spatial.helper.socket";
    const char* const M1SystemHelperManager::PLIST_PATH = "/Library/LaunchAgents/com.mach1.spatial.helper.plist";
#elif defined(_WIN32)
    const char* const M1SystemHelperManager::SERVICE_NAME = "M1-System-Helper";
    const char* const M1SystemHelperManager::PIPE_NAME = "\\\\.\\pipe\\M1SystemHelper";
#else  // Linux
    const char* const M1SystemHelperManager::SERVICE_NAME = "m1-system-helper";
    const char* const M1SystemHelperManager::SOCKET_PATH = "/tmp/m1-system-helper.socket";
#endif

// =============================================================================
// CROSS-PLATFORM IMPLEMENTATION
// =============================================================================

M1SystemHelperManager& M1SystemHelperManager::getInstance()
{
    static M1SystemHelperManager instance;
    return instance;
}

bool M1SystemHelperManager::requestHelperService(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Add this app to the active instances list
    auto insertResult = m_activeInstances.insert(appName);
    bool wasNewInstance = insertResult.second;

    std::cout << "[M1SystemHelperManager] " << appName << " requested service. "
              << "Active instances: " << m_activeInstances.size()
              << (wasNewInstance ? " (new instance)" : " (existing instance)") << std::endl;

    // If service is already running, we're done
    if (isHelperServiceRunning()) {
        std::cout << "[M1SystemHelperManager] Service already running for " << appName << std::endl;
        return true;
    }

    std::cout << "[M1SystemHelperManager] Starting helper service for " << appName << std::endl;

    // Platform-specific service startup
    if (!startHelperService()) {
        std::cerr << "[M1SystemHelperManager] Failed to start service" << std::endl;
        return false;
    }

    // Give service time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return true;
}

void M1SystemHelperManager::releaseHelperService(const std::string& appName)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove this app from the active instances list
    m_activeInstances.erase(appName);

    std::cout << "[M1SystemHelperManager] " << appName << " released helper service. "
              << "Active instances: " << m_activeInstances.size() << std::endl;

    if (m_activeInstances.empty()) {
        std::cout << "[M1SystemHelperManager] No more active instances. Service will auto-exit." << std::endl;

        // On Windows/Linux, we might want to stop immediately
        #if defined(_WIN32) || defined(__linux__)
            // Optional: Stop service immediately instead of letting it auto-exit
            // stopHelperService();
        #endif
    }
}

bool M1SystemHelperManager::isHelperServiceRunning() const
{
    // Reduced verbosity for routine health checks
#ifdef __APPLE__
    return triggerSocketActivation();
#elif defined(_WIN32)
    return triggerNamedPipeActivation();
#else  // Linux
    return triggerSocketActivation();
#endif
}

bool M1SystemHelperManager::startHelperService()
{
#ifdef __APPLE__
    std::cout << "[M1SystemHelperManager] Starting helper service..." << std::endl;

    // For LaunchAgents, try loading with user context first
    if (!isServiceLoaded()) {
        std::cout << "[M1SystemHelperManager] Service not loaded, attempting to load..." << std::endl;

        // Try loading the plist file
        if (!executeLaunchctlCommand("load " + std::string(PLIST_PATH))) {
            std::cerr << "[M1SystemHelperManager] Failed to load service plist" << std::endl;
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Give more time for LaunchAgents
    } else {
        std::cout << "[M1SystemHelperManager] Service already loaded" << std::endl;
    }

    // Try to start the service explicitly
    std::cout << "[M1SystemHelperManager] Attempting to start service..." << std::endl;
    bool startResult = executeLaunchctlCommand("start " + std::string(SERVICE_LABEL));

    // Give the service time to start up
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Test if we can actually connect to it
    std::cout << "[M1SystemHelperManager] Testing connection..." << std::endl;
    bool connectionResult = triggerSocketActivation();

    if (!connectionResult && startResult) {
        std::cout << "[M1SystemHelperManager] Service started but connection failed - service may be starting up" << std::endl;
        // Try again after another brief delay
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        connectionResult = triggerSocketActivation();
    }

    return connectionResult;

#elif defined(_WIN32)
    return executeServiceCommand("start");

#else  // Linux
    // Enable service if not enabled
    if (!isServiceEnabled()) {
        if (!executeSystemctlCommand("enable " + std::string(SERVICE_NAME))) {
            return false;
        }
    }
    return executeSystemctlCommand("start " + std::string(SERVICE_NAME));
#endif
}

bool M1SystemHelperManager::stopHelperService()
{
#ifdef __APPLE__
    return executeLaunchctlCommand("stop " + std::string(SERVICE_LABEL));
#elif defined(_WIN32)
    return executeServiceCommand("stop");
#else  // Linux
    return executeSystemctlCommand("stop " + std::string(SERVICE_NAME));
#endif
}

// =============================================================================
// MACOS IMPLEMENTATION
// =============================================================================

#ifdef __APPLE__

bool M1SystemHelperManager::executeLaunchctlCommand(const std::string& command) const
{
    std::string fullCommand = "launchctl " + command + " 2>/dev/null";
    int result = std::system(fullCommand.c_str());

    if (result == 0) {
        std::cout << "[M1SystemHelperManager] launchctl " << command << " - SUCCESS" << std::endl;
        return true;
    } else {
        std::cerr << "[M1SystemHelperManager] launchctl " << command << " - FAILED (exit code: "
                  << result << ")" << std::endl;
        return false;
    }
}

bool M1SystemHelperManager::isServiceLoaded() const
{
    std::string command = "launchctl list | grep " + std::string(SERVICE_LABEL) + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool M1SystemHelperManager::triggerSocketActivation() const
{
    // Reduced verbosity - only log connection failures and first success
    static bool hasLoggedSuccess = false;

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "[M1SystemHelperManager] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    bool success = false;
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        success = true;
        const char* ping = "PING\n";
        ssize_t sent = send(sockfd, ping, strlen(ping), 0);

        if (sent > 0) {
            char buffer[256] = {0};
            ssize_t received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (received > 0 && !hasLoggedSuccess) {
                std::cout << "[M1SystemHelperManager] Socket connection established and verified" << std::endl;
                hasLoggedSuccess = true;
            }
        }
    } else {
        // Only log connection failures during startup attempts
        // std::cerr << "[M1SystemHelperManager] Socket connection failed: " << strerror(errno) << std::endl;
    }

    close(sockfd);
    return success;
}

#endif

// =============================================================================
// WINDOWS IMPLEMENTATION
// =============================================================================

#ifdef _WIN32

bool M1SystemHelperManager::executeServiceCommand(const std::string& command) const
{
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scManager) {
        std::cerr << "[M1SystemHelperManager] Failed to open Service Control Manager" << std::endl;
        return false;
    }

    SC_HANDLE service = OpenService(scManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!service) {
        std::cerr << "[M1SystemHelperManager] Failed to open service: " << SERVICE_NAME << std::endl;
        CloseServiceHandle(scManager);
        return false;
    }

    bool success = false;
    if (command == "start") {
        success = StartService(service, 0, NULL);
        if (success) {
            std::cout << "[M1SystemHelperManager] Windows service started: " << SERVICE_NAME << std::endl;
        }
    } else if (command == "stop") {
        SERVICE_STATUS status;
        success = ControlService(service, SERVICE_CONTROL_STOP, &status);
        if (success) {
            std::cout << "[M1SystemHelperManager] Windows service stopped: " << SERVICE_NAME << std::endl;
        }
    }

    if (!success) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING && command == "start") {
            std::cout << "[M1SystemHelperManager] Service already running" << std::endl;
            success = true;
        } else {
            std::cerr << "[M1SystemHelperManager] Service command failed. Error: " << error << std::endl;
        }
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return success;
}

bool M1SystemHelperManager::isServiceInstalled() const
{
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scManager) return false;

    SC_HANDLE service = OpenService(scManager, SERVICE_NAME, SERVICE_QUERY_STATUS);
    bool installed = (service != NULL);

    if (service) CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return installed;
}

bool M1SystemHelperManager::triggerNamedPipeActivation() const
{
    HANDLE pipe = CreateFile(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Send ping message
    const char* message = "PING";
    DWORD bytesWritten;
    bool success = WriteFile(pipe, message, strlen(message), &bytesWritten, NULL);

    if (success) {
        // Read response
        char buffer[256];
        DWORD bytesRead;
        ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
    }

    CloseHandle(pipe);
    return success;
}

#endif

// =============================================================================
// LINUX IMPLEMENTATION
// =============================================================================

#if defined(__linux__) && !defined(__APPLE__)

bool M1SystemHelperManager::executeSystemctlCommand(const std::string& command) const
{
    std::string fullCommand = "systemctl " + command + " 2>/dev/null";
    int result = std::system(fullCommand.c_str());

    if (result == 0) {
        std::cout << "[M1SystemHelperManager] systemctl " << command << " - SUCCESS" << std::endl;
        return true;
    } else {
        std::cerr << "[M1SystemHelperManager] systemctl " << command << " - FAILED (exit code: "
                  << result << ")" << std::endl;
        return false;
    }
}

bool M1SystemHelperManager::isServiceEnabled() const
{
    std::string command = "systemctl is-enabled " + std::string(SERVICE_NAME) + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

bool M1SystemHelperManager::triggerSocketActivation() const
{
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    bool success = false;
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        success = true;
        const char* ping = "PING\n";
        send(sockfd, ping, strlen(ping), 0);

        char buffer[256];
        recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    }

    close(sockfd);
    return success;
}

#endif

} // namespace Mach1
