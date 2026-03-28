#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ISSUES=0

# Map linked .lib to a grep pattern proving it is actually used
lib_usage_pattern() {
    case "$1" in
        d3d11.lib)       echo '#include.*d3d11\.h';;
        d3d12.lib)       echo '#include.*d3d12\.h';;
        d3dcompiler.lib) echo '#include.*d3dcompiler\.h';;
        Version.lib)     echo 'GetFileVersionInfo|VerQueryValue';;
        Winhttp.lib)     echo '#include.*winhttp\.h';;
        winhttp.lib)     echo '#include.*winhttp\.h';;
        Advapi32.lib)    echo 'RegOpenKey|RegQueryValue|RegGetValue|RegCloseKey|RegSetValue|RegCreateKey';;
        advapi32.lib)    echo 'RegOpenKey|RegQueryValue|RegGetValue|RegCloseKey|RegSetValue|RegCreateKey';;
        *)               echo '';;
    esac
}

echo "Running unused dependency detection..."
echo ""

# ── Check 1: Linked libraries from .vcxproj ──────────────────────────
echo "=== Linked Libraries ==="
LIB_ISSUES=0

for vcxproj in "$PROJECT_ROOT"/*/*.vcxproj; do
    [ ! -f "$vcxproj" ] && continue
    PROJECT_NAME=$(basename "$(dirname "$vcxproj")")
    PROJECT_DIR="$(dirname "$vcxproj")"

    LIBS=$(grep -oP '<AdditionalDependencies>\K[^<]+' "$vcxproj" 2>/dev/null \
        | tr ';' '\n' | grep -v '%(AdditionalDependencies)' | sort -u)
    [ -z "$LIBS" ] && continue

    while IFS= read -r lib; do
        [ -z "$lib" ] && continue
        PATTERN=$(lib_usage_pattern "$lib")
        [ -z "$PATTERN" ] && continue

        FOUND=""
        for search_dir in "$PROJECT_DIR/src" "$PROJECT_DIR/include" "$PROJECT_DIR/ext"; do
            [ ! -d "$search_dir" ] && continue
            if grep -rlE "$PATTERN" "$search_dir" >/dev/null 2>&1; then
                FOUND=1
                break
            fi
        done

        if [ -z "$FOUND" ]; then
            echo "  [UNUSED] $PROJECT_NAME links $lib but no usage found"
            ((ISSUES++))
            ((LIB_ISSUES++))
        fi
    done <<< "$LIBS"
done

[ $LIB_ISSUES -eq 0 ] && echo "  All linked libraries are in use."
echo ""

# ── Check 2: Vendored dependencies ───────────────────────────────────
echo "=== Vendored Dependencies ==="
VENDOR_ISSUES=0

ALL_PROJECT_SOURCES=(
    "$PROJECT_ROOT/Mod/src"
    "$PROJECT_ROOT/Mod/include"
    "$PROJECT_ROOT/Launcher/src"
    "$PROJECT_ROOT/Launcher/include"
    "$PROJECT_ROOT/Proxy/src"
    "$PROJECT_ROOT/Proxy/include"
)

# Filter to directories that actually exist
EXISTING_SOURCES=()
for d in "${ALL_PROJECT_SOURCES[@]}"; do
    [ -d "$d" ] && EXISTING_SOURCES+=("$d")
done

check_vendored_header() {
    local file="$1"
    local label="$2"
    local name
    name=$(basename "$file")
    local stem="${name%.*}"

    if ! grep -rlE "#include.*${stem}" "${EXISTING_SOURCES[@]}" >/dev/null 2>&1; then
        echo "  [UNUSED] Vendored $label is not referenced by any project"
        ((ISSUES++))
        ((VENDOR_ISSUES++))
    fi
}

check_vendored_dir() {
    local dir="$1"
    local label="$2"
    local project_dir="$3"
    local name
    name=$(basename "$dir")

    local search_dirs=("$project_dir/src" "$project_dir/include")
    local existing=()
    for d in "${search_dirs[@]}"; do
        [ -d "$d" ] && existing+=("$d")
    done
    [ ${#existing[@]} -eq 0 ] && return

    if ! grep -rlE "#include.*${name}" "${existing[@]}" >/dev/null 2>&1; then
        echo "  [UNUSED] Vendored $label is not referenced by its project"
        ((ISSUES++))
        ((VENDOR_ISSUES++))
    fi
}

# Root ext/ — shared vendored headers
for entry in "$PROJECT_ROOT"/ext/*.h "$PROJECT_ROOT"/ext/*.hpp; do
    [ ! -f "$entry" ] && continue
    check_vendored_header "$entry" "ext/$(basename "$entry")"
done

# Per-project ext/ — vendored directories and headers
for project in Mod Launcher Proxy; do
    EXT_DIR="$PROJECT_ROOT/$project/ext"
    [ ! -d "$EXT_DIR" ] && continue

    for entry in "$EXT_DIR"/*/; do
        [ ! -d "$entry" ] && continue
        check_vendored_dir "$entry" "$project/ext/$(basename "$entry")/" "$PROJECT_ROOT/$project"
    done

    for entry in "$EXT_DIR"/*.h "$EXT_DIR"/*.hpp; do
        [ ! -f "$entry" ] && continue
        check_vendored_header "$entry" "$project/ext/$(basename "$entry")"
    done
done

[ $VENDOR_ISSUES -eq 0 ] && echo "  All vendored dependencies are in use."
echo ""

# ── Check 3: Orphan project headers ──────────────────────────────────
echo "=== Orphan Project Headers ==="
ORPHAN_ISSUES=0

SKIP_HEADERS="resource.h winres.h"

for project in Mod Launcher Proxy; do
    INC_DIR="$PROJECT_ROOT/$project/include"
    SRC_DIR="$PROJECT_ROOT/$project/src"
    [ ! -d "$INC_DIR" ] && continue

    while IFS= read -r header; do
        [ -z "$header" ] && continue
        FILENAME=$(basename "$header")

        skip=0
        for s in $SKIP_HEADERS; do
            [ "$FILENAME" = "$s" ] && skip=1 && break
        done
        [ $skip -eq 1 ] && continue

        SEARCH_DIRS=()
        [ -d "$SRC_DIR" ] && SEARCH_DIRS+=("$SRC_DIR")
        [ -d "$INC_DIR" ] && SEARCH_DIRS+=("$INC_DIR")
        [ ${#SEARCH_DIRS[@]} -eq 0 ] && continue

        FOUND=$(grep -rlF "$FILENAME" "${SEARCH_DIRS[@]}" 2>/dev/null \
            | grep -v "^${header}$" | head -1)

        if [ -z "$FOUND" ]; then
            REL_PATH="${header#"$PROJECT_ROOT"/}"
            echo "  [ORPHAN] $REL_PATH is never included"
            ((ISSUES++))
            ((ORPHAN_ISSUES++))
        fi
    done < <(find "$INC_DIR" -name '*.h' -o -name '*.hpp' 2>/dev/null)
done

[ $ORPHAN_ISSUES -eq 0 ] && echo "  All project headers are referenced."
echo ""

# ── Summary ───────────────────────────────────────────────────────────
if [ $ISSUES -gt 0 ]; then
    echo "WARNING: $ISSUES unused dependency issue(s) found"
    exit 1
fi

echo "No unused dependency issues detected."
