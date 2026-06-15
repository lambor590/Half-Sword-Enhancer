#!/usr/bin/env bash
set -uo pipefail

if ! command -v npx &>/dev/null; then
    echo "npx not found. Install Node.js."
    exit 2
fi

echo "Running duplicate code detection..."
echo ""

npx --yes jscpd Mod/src/ Mod/include/ Launcher/src/ Proxy/src/
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -ne 0 ]; then
    echo "WARNING: Duplicate code exceeds threshold (see .jscpd.json)"
    exit 1
fi

echo "No excessive code duplication detected."
