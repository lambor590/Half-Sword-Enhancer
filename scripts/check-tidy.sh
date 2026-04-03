#!/usr/bin/env bash
set -uo pipefail

if ! command -v clang-tidy &>/dev/null; then
    echo "clang-tidy not found. Install with:"
    echo "  Windows: choco install llvm   or   winget install LLVM.LLVM"
    echo "  Linux:   sudo apt-get install clang-tidy"
    echo "  macOS:   brew install clang-tidy"
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

echo "Running clang-tidy analysis..."
echo ""

COMMON_FLAGS="-std=c++23 -x c++ -DNDEBUG -DNOMINMAX -DWIN32_LEAN_AND_MEAN"
TMP_OUT=$(mktemp)
trap 'rm -f "$TMP_OUT"' EXIT

collect_files() {
    for dir in "$@"; do
        [ -d "$dir" ] && find "$dir" \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
            -not -path '*/ext/*' -not -path '*/SDK/*'
    done
}

run_tidy() {
    local label="$1"; shift
    local includes="$1"; shift
    local files
    files=$(collect_files "$@")
    [ -z "$files" ] && return 0

    local count
    count=$(echo "$files" | wc -l)
    echo "  [$label] Checking $count files..."

    while IFS= read -r file; do
        [ -z "$file" ] && continue
        # shellcheck disable=SC2086
        clang-tidy --quiet "$file" -- $COMMON_FLAGS $includes 2>&1 \
            | grep -v 'warnings generated' \
            | grep -v 'Use -header-filter' \
            | grep -v '^$'
    done <<< "$files" >> "$TMP_OUT"
}

run_tidy "Mod" \
    "-IMod/include -IMod/include/imgui -IMod/ext -IMod/SDK -Iext" \
    Mod/src Mod/include

run_tidy "Launcher" \
    "-ILauncher/include -ILauncher/ext -Iext" \
    Launcher/src Launcher/include

run_tidy "Proxy" \
    "-IProxy/include -Iext" \
    Proxy/src Proxy/include

# Filter to only project-owned files
OUTPUT=$(grep -E '(Mod|Launcher|Proxy)/(include|src)/' "$TMP_OUT" \
    | grep -v '/ext/' | grep -v '/SDK/' \
    | grep ' error: ' \
    | sort -t: -k1,1 -k2,2n -u || true)

ERRORS=$(echo "$OUTPUT" | grep -c ' error: ' || true)

if [ -n "$OUTPUT" ] && [ "$ERRORS" -gt 0 ]; then
    echo "$OUTPUT"
fi

echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "WARNING: clang-tidy found $ERRORS error(s)"
    exit 1
fi

echo "No clang-tidy issues detected."
