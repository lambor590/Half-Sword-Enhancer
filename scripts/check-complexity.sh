#!/usr/bin/env bash
set -uo pipefail

if ! command -v lizard &>/dev/null; then
    echo "lizard not found. Install with: pip install lizard"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SUPPRESSIONS="$SCRIPT_DIR/complexity-suppressions.txt"

echo "Running complexity analysis..."
echo ""

WARNINGS=$(lizard Mod/src/ Mod/include/ Launcher/src/ Proxy/src/ \
    --exclude "Mod/SDK/*" --exclude "Mod/ext/*" --exclude "ext/*" \
    --CCN 15 --length 100 --arguments 10 \
    --sort cyclomatic_complexity -w 2>&1)

if [ -z "$WARNINGS" ]; then
    echo "All functions within complexity thresholds."
    exit 0
fi

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
        FILTERED=$(echo "$WARNINGS" | grep -Ev "$FILTER_PATTERN")
    else
        FILTERED="$WARNINGS"
    fi
else
    FILTERED="$WARNINGS"
fi

if [ -z "$FILTERED" ]; then
    SUPPRESSED=$(echo "$WARNINGS" | wc -l)
    echo "All warnings are suppressed ($SUPPRESSED known exceptions in complexity-suppressions.txt)."
    exit 0
fi

echo "$FILTERED"
REMAINING=$(echo "$FILTERED" | wc -l)
echo ""
echo "WARNING: $REMAINING function(s) exceed complexity thresholds (CCN>15, length>100, args>10)"

if [ -f "$SUPPRESSIONS" ]; then
    TOTAL=$(echo "$WARNINGS" | wc -l)
    SUPPRESSED=$((TOTAL - REMAINING))
    if [ "$SUPPRESSED" -gt 0 ]; then
        echo "  ($SUPPRESSED additional warnings suppressed via complexity-suppressions.txt)"
    fi
fi

exit 1
