#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAILED=0
PASSED=0
SKIPPED=0
RESULTS=()

CHECKS=(
    "Clang-Tidy:$SCRIPT_DIR/check-tidy.sh"
    "Cyclomatic Complexity:$SCRIPT_DIR/check-complexity.sh"
    "Dead Code Detection:$SCRIPT_DIR/check-dead-code.sh"
)

run_check() {
    local name="$1"
    local script="$2"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo " $name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    if bash "$script"; then
        RESULTS+=("[PASS] $name")
        ((PASSED++))
    else
        local exit_code=$?
        if [ "$exit_code" -eq 2 ]; then
            RESULTS+=("[SKIP] $name")
            ((SKIPPED++))
        else
            RESULTS+=("[FAIL] $name")
            ((FAILED++))
        fi
    fi
    echo ""
}

echo ""
echo "Running all code quality checks..."
echo ""

for check in "${CHECKS[@]}"; do
    run_check "${check%%:*}" "${check#*:}"
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for result in "${RESULTS[@]}"; do
    echo "  $result"
done
echo ""
echo "  Passed: $PASSED | Failed: $FAILED | Skipped: $SKIPPED"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
