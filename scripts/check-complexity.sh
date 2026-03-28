#!/usr/bin/env bash
set -uo pipefail

if ! command -v lizard &>/dev/null; then
    echo "lizard not found. Install with: pip install lizard"
    exit 1
fi

echo "Running complexity analysis..."
echo ""

lizard Mod/src/ Mod/include/ Launcher/src/ Proxy/src/ \
    --exclude "Mod/SDK/*" --exclude "Mod/ext/*" --exclude "ext/*" \
    --CCN 15 --length 100 --arguments 10 \
    --sort cyclomatic_complexity
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -ne 0 ]; then
    echo "WARNING: Some functions exceed complexity thresholds (CCN>15, length>100, args>10)"
    exit 1
fi

echo "All functions within complexity thresholds."
