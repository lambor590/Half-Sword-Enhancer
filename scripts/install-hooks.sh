#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
HOOK_SRC="$SCRIPT_DIR/pre-commit"
HOOK_DST="$REPO_ROOT/.git/hooks/pre-commit"

if [ ! -f "$HOOK_SRC" ]; then
    echo "ERROR: $HOOK_SRC not found"
    exit 1
fi

if [ -f "$HOOK_DST" ] || [ -L "$HOOK_DST" ]; then
    echo "Replacing existing pre-commit hook..."
    rm "$HOOK_DST"
fi

ln -s "$HOOK_SRC" "$HOOK_DST"
echo "Pre-commit hook installed (symlinked to scripts/pre-commit)."
