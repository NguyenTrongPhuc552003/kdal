#!/bin/sh
# Run checkpatch.pl on KDAL source files for kernel coding style compliance.
#
# If the kernel source tree is available, uses the real checkpatch.pl.
# Otherwise, performs basic style checks with grep/awk.
#
# Usage: ./scripts/ci/checkpatch.sh [kernel_dir]

set -eu

KDIR="${1:-${KDIR:-}}"
SRC_DIRS="src include"
ERRORS=0

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }
warn() { printf "\033[1;33m  WARN: %s\033[0m\n" "$1"; }
fail() { printf "\033[1;31m  FAIL: %s\033[0m\n" "$1"; ERRORS=$((ERRORS + 1)); }

# Try real checkpatch.pl first
if [ -n "$KDIR" ] && [ -f "$KDIR/scripts/checkpatch.pl" ]; then
    info "Running kernel checkpatch.pl..."
    find $SRC_DIRS -name '*.c' -o -name '*.h' | while read -r f; do
        if ! "$KDIR/scripts/checkpatch.pl" --no-tree --file "$f" \
             --ignore LONG_LINE,FILE_PATH_CHANGES 2>/dev/null; then
            fail "$f: checkpatch errors"
        fi
    done
else
    info "No kernel tree found — running basic style checks"

    # Check for tabs vs spaces (kernel uses tabs)
    info "Checking indentation (kernel style: tabs)..."
    find $SRC_DIRS -name '*.c' -o -name '*.h' | while read -r f; do
        if grep -Pn '^ {4}' "$f" >/dev/null 2>&1; then
            warn "$f: spaces used for indentation (kernel uses tabs)"
        fi
    done

    # Check for trailing whitespace
    info "Checking for trailing whitespace..."
    find $SRC_DIRS -name '*.c' -o -name '*.h' | while read -r f; do
        if grep -Pn '\s+$' "$f" >/dev/null 2>&1; then
            fail "$f: trailing whitespace"
        fi
    done

    # Check line length (120 cols for kernel)
    info "Checking line length (>120 chars)..."
    find $SRC_DIRS -name '*.c' -o -name '*.h' | while read -r f; do
        long=$(awk 'length > 120 { count++ } END { print count+0 }' "$f")
        if [ "$long" -gt 0 ]; then
            warn "$f: $long lines exceed 120 characters"
        fi
    done
fi

if [ "$ERRORS" -gt 0 ]; then
    echo ""
    echo "$ERRORS error(s) found"
    exit 1
fi

echo ""
info "Style checks passed."
