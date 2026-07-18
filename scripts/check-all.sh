#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKS=(
    "Clang-Tidy:$SCRIPT_DIR/check-tidy.sh"
    "Cyclomatic Complexity:$SCRIPT_DIR/check-complexity.sh"
    "Dead Code Detection:$SCRIPT_DIR/check-dead-code.sh"
)

RESULT_DIR="$(mktemp -d)"
trap 'rm -rf "$RESULT_DIR"' EXIT

echo ""
echo "Running all code quality checks..."
echo ""

PIDS=()
for index in "${!CHECKS[@]}"; do
    check="${CHECKS[$index]}"
    bash "${check#*:}" >"$RESULT_DIR/$index.log" 2>&1 &
    PIDS+=("$!")
done

FAILED=0
PASSED=0
SKIPPED=0
RESULTS=()
for index in "${!CHECKS[@]}"; do
    check="${CHECKS[$index]}"
    name="${check%%:*}"
    if wait "${PIDS[$index]}"; then
        exit_code=0
    else
        exit_code=$?
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo " $name"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    cat "$RESULT_DIR/$index.log"
    echo ""

    if [ "$exit_code" -eq 0 ]; then
        RESULTS+=("[PASS] $name")
        ((PASSED++))
    elif [ "$exit_code" -eq 2 ]; then
        RESULTS+=("[SKIP] $name")
        ((SKIPPED++))
    else
        RESULTS+=("[FAIL] $name")
        ((FAILED++))
    fi
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
