#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FAILED=0
PASSED=0
SKIPPED=0
RESULTS=()

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
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 1 ]; then
            RESULTS+=("[FAIL] $name")
            ((FAILED++))
        else
            RESULTS+=("[SKIP] $name (tool not installed)")
            ((SKIPPED++))
        fi
    fi
    echo ""
}

echo ""
echo "Running all code quality checks..."
echo ""

run_check "Cyclomatic Complexity"   "$SCRIPT_DIR/check-complexity.sh"
run_check "Dead Code Detection"    "$SCRIPT_DIR/check-dead-code.sh"
run_check "Duplicate Code"         "$SCRIPT_DIR/check-duplicates.sh"
run_check "Unused Dependencies"    "$SCRIPT_DIR/check-unused-deps.sh"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo " Summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
for result in "${RESULTS[@]}"; do
    echo "  $result"
done
echo ""
echo "  Passed: $PASSED | Failed: $FAILED | Skipped: $SKIPPED"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
