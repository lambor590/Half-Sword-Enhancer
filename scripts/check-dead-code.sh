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

SUPPRESSIONS="$SCRIPT_DIR/dead-code-suppressions.txt"

if [ -f "$SUPPRESSIONS" ]; then
    FILTER_PATTERN=""
    while IFS= read -r line; do
        line="${line%%#*}"
        line="$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
        [ -z "$line" ] && continue
        if [ -z "$FILTER_PATTERN" ]; then
            FILTER_PATTERN="$line"
        else
            FILTER_PATTERN="$FILTER_PATTERN|$line"
        fi
    done < "$SUPPRESSIONS"

    if [ -n "$FILTER_PATTERN" ]; then
        FILTERED=$(echo "$OUTPUT" | grep -Ev "$FILTER_PATTERN")
    else
        FILTERED="$OUTPUT"
    fi
else
    FILTERED="$OUTPUT"
fi

TOTAL=$(echo "$OUTPUT" | grep -c '\(unusedFunction\|unusedVariable\|unreachableCode\)' || true)
WARNINGS=$(echo "$FILTERED" | grep -c '\(unusedFunction\|unusedVariable\|unreachableCode\)' || true)

if [ -n "$FILTERED" ] && [ "$WARNINGS" -gt 0 ]; then
    echo "$FILTERED"
fi

echo ""
if [ "$WARNINGS" -gt 0 ]; then
    echo "WARNING: $WARNINGS dead code issue(s) found (unused functions, variables, or unreachable code)"
    SUPPRESSED=$((TOTAL - WARNINGS))
    if [ "$SUPPRESSED" -gt 0 ]; then
        echo "  ($SUPPRESSED additional warnings suppressed via dead-code-suppressions.txt)"
    fi
    exit 1
fi

if [ "$TOTAL" -gt 0 ]; then
    echo "All warnings are suppressed ($TOTAL known false positives in dead-code-suppressions.txt)."
else
    echo "No dead code issues detected."
fi
