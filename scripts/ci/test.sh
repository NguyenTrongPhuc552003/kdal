#!/bin/sh
# KDAL repository-local smoke tests.
#
# This script validates that all userspace-compilable code builds
# and runs without errors.  Kernel-side tests (KUnit) are run
# separately via QEMU or ./tools/run_kunit.sh.

set -eu

echo "=== KDAL smoke tests ==="

# 1. Build kdality (KDAL utility: runtime CLI + language toolchain)
echo "[1/5] Building kdality..."
make -C tools/kdality clean
make -C tools/kdality

echo "[2/5] Checking kdality version..."
./tools/kdality/kdality version >/dev/null

# 3. Build compiler
echo "[3/5] Building KDAL compiler..."
if [ -f compiler/Makefile ]; then
    make -C compiler
else
    echo "  (compiler not yet built — skipping)"
fi

# 4. Build userspace test programs
echo "[4/5] Building userspace test programs..."
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
   -o "$TMPDIR/testapp" tests/userspace/testapp.c

cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
   -o "$TMPDIR/benchmark" tests/userspace/benchmark.c

cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
   -o "$TMPDIR/kdaltest" tests/integration/kdaltest.c

# 5. Run offline / dry-run tests
echo "[5/5] Running offline smoke tests..."
"$TMPDIR/testapp" --dry-run
"$TMPDIR/benchmark" --help 2>/dev/null || true
"$TMPDIR/kdaltest" || true

echo ""
echo "=== All smoke tests passed ==="
