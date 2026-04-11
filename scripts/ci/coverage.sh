#!/bin/sh
# Collect code coverage data from KUnit test runs.
#
# Requires a kernel built with CONFIG_GCOV_KERNEL=y and a QEMU guest
# that exposes gcov data via debugfs.
#
# Usage: ./scripts/ci/coverage.sh [work_dir]

set -eu

WORK_DIR="${1:-$HOME/kdal-qemu}"

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

info "KDAL Coverage Collection"
info ""
info "Prerequisites:"
info "  1. Build kernel with: scripts/config --enable CONFIG_GCOV_KERNEL"
info "  2. Build KDAL with:   make KCFLAGS='-fprofile-arcs -ftest-coverage'"
info "  3. Boot QEMU and run: insmod kdal.ko"
info "  4. Mount debugfs:     mount -t debugfs none /sys/kernel/debug"
info ""

# If running inside a QEMU guest with gcov data:
GCOV_DIR="/sys/kernel/debug/gcov"
if [ -d "$GCOV_DIR" ]; then
    info "Extracting gcov data..."
    OUTDIR="$WORK_DIR/coverage/$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$OUTDIR"

    # Extract .gcda files
    find "$GCOV_DIR" -name '*.gcda' -exec sh -c '
        dir=$(dirname "$1" | sed "s|^/sys/kernel/debug/gcov||")
        mkdir -p "'"$OUTDIR"'$dir"
        cat "$1" > "'"$OUTDIR"'$dir/$(basename $1)"
    ' _ {} \;

    # Extract .gcno files
    find "$GCOV_DIR" -name '*.gcno' -exec sh -c '
        dir=$(dirname "$1" | sed "s|^/sys/kernel/debug/gcov||")
        mkdir -p "'"$OUTDIR"'$dir"
        cat "$1" > "'"$OUTDIR"'$dir/$(basename $1)"
    ' _ {} \;

    info "Data saved to $OUTDIR"

    if command -v lcov >/dev/null 2>&1; then
        info "Generating HTML report..."
        lcov --capture --directory "$OUTDIR" --output-file "$OUTDIR/coverage.info" 2>/dev/null || true
        genhtml "$OUTDIR/coverage.info" --output-directory "$OUTDIR/html" 2>/dev/null || true
        info "Report: $OUTDIR/html/index.html"
    fi
else
    info "No gcov data found at $GCOV_DIR"
    info "Run this script inside the QEMU guest after loading kdal.ko"
fi
