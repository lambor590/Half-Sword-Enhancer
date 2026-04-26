#!/usr/bin/env bash
set -uo pipefail

if ! command -v cppcheck &>/dev/null; then
    echo "cppcheck not found. Install with:"
    echo "  Windows: choco install cppcheck   or   winget install cppcheck"
    echo "  Linux:   sudo apt-get install cppcheck"
    echo "  macOS:   brew install cppcheck"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SOURCES=(
    "$PROJECT_ROOT/Mod/src/"
    "$PROJECT_ROOT/Mod/include/"
    "$PROJECT_ROOT/Launcher/src/"
    "$PROJECT_ROOT/Proxy/src/"
)

EXCLUDES=(
    "$PROJECT_ROOT/Mod/SDK/"
    "$PROJECT_ROOT/Mod/ext/"
    "$PROJECT_ROOT/ext/"
    "$PROJECT_ROOT/Launcher/ext/"
    "$PROJECT_ROOT/Proxy/ext/"
)

EXCLUDE_ARGS=()
for dir in "${EXCLUDES[@]}"; do
    EXCLUDE_ARGS+=(-i "$dir")
done

echo "Running dead code analysis with cppcheck..."
echo ""

OUTPUT=$(cppcheck \
    "${SOURCES[@]}" \
    "${EXCLUDE_ARGS[@]}" \
    --enable=unusedFunction \
    --suppress=unusedFunction:*DllMain.cpp \
    --suppress=unusedFunction:*trampolines.asm \
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
# cppcheck cannot trace calls through the descriptor-table / CRTP path used by
# Preset.h, so these specific reports are false positives.
OUTPUT=$(echo "$OUTPUT" | grep -vE 'Mod/src/Override\.cpp:[0-9]+: .*SerializeAll.*\[unusedFunction\]$' || true)
OUTPUT=$(echo "$OUTPUT" | grep -vE 'Mod/src/Override\.cpp:[0-9]+: .*DeserializeAll.*\[unusedFunction\]$' || true)
OUTPUT=$(echo "$OUTPUT" | grep -vE 'Mod/src/Preset\.cpp:[0-9]+: .*SerializePresetFields.*\[unusedFunction\]$' || true)
OUTPUT=$(echo "$OUTPUT" | grep -vE 'Mod/src/Preset\.cpp:[0-9]+: .*DeserializePresetFields.*\[unusedFunction\]$' || true)

WARNINGS=$(echo "$OUTPUT" | grep -c '\(unusedFunction\|unusedVariable\|unreachableCode\)' || true)

if [ -n "$OUTPUT" ] && [ "$WARNINGS" -gt 0 ]; then
    echo "$OUTPUT"
fi

echo ""
if [ "$WARNINGS" -gt 0 ]; then
    echo "WARNING: $WARNINGS dead code issue(s) found"
    exit 1
fi

echo "No dead code issues detected."
