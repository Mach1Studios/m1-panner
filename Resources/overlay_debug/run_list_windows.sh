#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! python3 -c "from Quartz import CGWindowListCopyWindowInfo" 2>/dev/null; then
    echo "pyobjc-framework-Quartz is required."
    echo "Install with: pip3 install pyobjc-framework-Quartz"
    exit 1
fi

python3 list_windows.py "$@"
