#!/bin/sh
# Install KDAL development git hooks.
#
# Copies hooks from scripts/dev/hooks/ into .git/hooks/ and makes them
# executable. Safe to run multiple times (overwrites existing hooks).
#
# Usage:
#   ./scripts/dev/install-hooks.sh

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
HOOKS_SRC="$SCRIPT_DIR/hooks"
HOOKS_DST="$REPO_ROOT/.git/hooks"

if [ -t 1 ]; then
    GREEN='\033[1;32m' YELLOW='\033[1;33m' RED='\033[1;31m' RESET='\033[0m'
else
    GREEN='' YELLOW='' RED='' RESET=''
fi

ok()   { printf "${GREEN}  ✓ %s${RESET}\n" "$*"; }
warn() { printf "${YELLOW}  ! %s${RESET}\n" "$*"; }
die()  { printf "${RED}Error: %s${RESET}\n" "$*" >&2; exit 1; }

[ -d "$REPO_ROOT/.git" ] || die "Not a git repository: $REPO_ROOT"
[ -d "$HOOKS_SRC" ]      || die "Hooks source directory not found: $HOOKS_SRC"

printf 'Installing KDAL git hooks ...\n'

for hook in "$HOOKS_SRC"/*; do
    name=$(basename "$hook")
    dst="$HOOKS_DST/$name"

    if [ -f "$dst" ] && ! diff -q "$hook" "$dst" >/dev/null 2>&1; then
        warn "$name: overwriting existing hook (previous version backed up as $name.bak)"
        cp "$dst" "${dst}.bak"
    fi

    cp "$hook" "$dst"
    chmod +x "$dst"
    ok "$name"
done

printf '\nHooks installed. Run this script again after updating hooks in scripts/dev/hooks/.\n'
