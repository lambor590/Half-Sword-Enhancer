#!/usr/bin/env bash
set -uo pipefail

if ! command -v lizard &>/dev/null; then
    echo "lizard not found. Install with: pip install lizard"
    exit 2
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

FILTERED="$WARNINGS"
if [ -f "$SUPPRESSIONS" ]; then
    SUPPRESSION_PATTERN=$(awk '
        {
            sub(/#.*/, "")
            gsub(/^[[:space:]]+|[[:space:]]+$/, "")
            if ($0 != "") print
        }
    ' "$SUPPRESSIONS" | paste -sd '|' -)

    if [ -n "$SUPPRESSION_PATTERN" ]; then
        FILTERED=$(grep -Ev "$SUPPRESSION_PATTERN" <<< "$WARNINGS" || true)
    fi
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
