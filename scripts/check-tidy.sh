#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UNAME="$(uname -s 2>/dev/null || echo unknown)"

case "$UNAME" in
    MINGW*|MSYS*|CYGWIN*)
        if command -v pwsh &>/dev/null; then
            pwsh -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/check-tidy.ps1" "$@"
            exit $?
        fi

        if command -v powershell.exe &>/dev/null; then
            SCRIPT_PATH="$SCRIPT_DIR/check-tidy.ps1"
            if command -v cygpath &>/dev/null; then
                SCRIPT_PATH="$(cygpath -w "$SCRIPT_PATH")"
            fi
            powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_PATH" "$@"
            exit $?
        fi

        echo "PowerShell not found. Run scripts/check-tidy.ps1 from Windows PowerShell."
        exit 2
        ;;
    *)
        echo "clang-tidy skipped: HalfSwordEnhancer needs a Windows MSVC environment for reliable analysis."
        echo "Run scripts/check-tidy.ps1 from the development PC after installing LLVM."
        exit 2
        ;;
esac
