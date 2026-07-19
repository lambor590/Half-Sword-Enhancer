#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
HOOK_SRC="$SCRIPT_DIR/pre-commit"
HOOK_DST="$REPO_ROOT/.git/hooks/pre-commit"

ln -sf "$HOOK_SRC" "$HOOK_DST"
echo "Pre-commit hook installed (symlinked to scripts/pre-commit)."
