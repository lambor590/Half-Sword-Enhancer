#!/usr/bin/env bash
set -uo pipefail

if ! command -v cppcheck &>/dev/null; then
    echo "cppcheck not found. Install with:"
    echo "  Windows: choco install cppcheck   or   winget install cppcheck"
    echo "  Linux:   sudo apt-get install cppcheck"
    echo "  macOS:   brew install cppcheck"
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

echo "Running dead code analysis with cppcheck..."
echo ""

OUTPUT=$(cppcheck \
    Mod/src/ Mod/include/ Launcher/src/ Launcher/include/ Proxy/src/ UE4SSBridge/src/ \
    --enable=unusedFunction \
    --suppress=unusedFunction:*DllMain.cpp \
    --suppress='*:*SimpleIni.h' \
    --suppress='*:*/ext/*' \
    --suppress=unknownMacro \
    --suppress=ctuOneDefinitionRuleViolation \
    --suppress=toomanyconfigs \
    --std=c++23 \
    --language=c++ \
    --platform=win64 \
    --quiet \
    --template='{file}:{line}: {severity}: {message} [{id}]' \
    2>&1)
CPPCHECK_EXIT=$?
if [ "$CPPCHECK_EXIT" -ne 0 ]; then
    echo "$OUTPUT"
    exit "$CPPCHECK_EXIT"
fi

FALSE_POSITIVE_PATTERN='Mod/src/Override\.cpp:[0-9]+: .*(SerializeAll|DeserializeAll).*\[unusedFunction\]$|Mod/src/Preset\.cpp:[0-9]+: .*(SerializePresetFields|DeserializePresetFields).*\[unusedFunction\]$'
OUTPUT=$(grep -vE "$FALSE_POSITIVE_PATTERN" <<< "$OUTPUT")

if [ -n "$OUTPUT" ]; then
    echo "$OUTPUT"
    echo ""
    ISSUES=$(wc -l <<< "$OUTPUT")
    echo "WARNING: $ISSUES dead-code analysis issue(s) found"
    exit 1
fi

echo "No dead code issues detected."
