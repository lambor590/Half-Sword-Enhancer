#!/usr/bin/env bash
set -uo pipefail

if ! command -v clang-tidy &>/dev/null; then
    echo "clang-tidy not found. Install with:"
    echo "  Windows: choco install llvm   or   winget install LLVM.LLVM"
    echo "  Linux:   sudo apt-get install clang-tidy"
    echo "  macOS:   brew install llvm"
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Running clang-tidy analysis..."
echo ""

FAILED=0
CHECKED=0

run_tidy() {
    local dir="$1"
    local includes="$2"

    while IFS= read -r file; do
        [ -z "$file" ] && continue
        ((CHECKED++))
        if ! clang-tidy "$file" \
            -p "$PROJECT_ROOT" \
            --quiet \
            -- -std=c++23 -x c++ \
            -DNDEBUG -DNOMINMAX -DWIN32_LEAN_AND_MEAN \
            $includes \
            2>&1 | grep -v "^$"; then
            : # no output means clean
        else
            FAILED=1
        fi
    done < <(find "$dir" -name '*.cpp' -o -name '*.h' -o -name '*.hpp' 2>/dev/null)
}

# Mod
MOD_INCLUDES="-I$PROJECT_ROOT/Mod/include -I$PROJECT_ROOT/Mod/include/imgui -I$PROJECT_ROOT/Mod/ext -I$PROJECT_ROOT/Mod/SDK -I$PROJECT_ROOT/ext"
for dir in "$PROJECT_ROOT/Mod/src" "$PROJECT_ROOT/Mod/include"; do
    [ -d "$dir" ] && run_tidy "$dir" "$MOD_INCLUDES"
done

# Launcher
LAUNCHER_INCLUDES="-I$PROJECT_ROOT/Launcher/include -I$PROJECT_ROOT/Launcher/ext -I$PROJECT_ROOT/ext"
for dir in "$PROJECT_ROOT/Launcher/src" "$PROJECT_ROOT/Launcher/include"; do
    [ -d "$dir" ] && run_tidy "$dir" "$LAUNCHER_INCLUDES"
done

# Proxy
PROXY_INCLUDES="-I$PROJECT_ROOT/Proxy/include -I$PROJECT_ROOT/ext"
for dir in "$PROJECT_ROOT/Proxy/src" "$PROJECT_ROOT/Proxy/include"; do
    [ -d "$dir" ] && run_tidy "$dir" "$PROXY_INCLUDES"
done

echo ""
if [ $FAILED -ne 0 ]; then
    echo "WARNING: clang-tidy found issues in $CHECKED file(s) checked"
    exit 1
fi

echo "No clang-tidy issues detected ($CHECKED files checked)."
