#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        if command -v pwsh &>/dev/null; then
            exec pwsh -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_DIR/check-tidy.ps1" "$@"
        fi

        echo "PowerShell 7 not found. Run scripts/check-tidy.ps1 with pwsh."
        exit 2
        ;;
    *)
        echo "clang-tidy skipped: HalfSwordEnhancer needs a Windows MSVC environment for reliable analysis."
        echo "Run scripts/check-tidy.ps1 from the development PC after installing LLVM."
        exit 2
        ;;
esac
