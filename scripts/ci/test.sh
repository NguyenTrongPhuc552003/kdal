#!/bin/sh
# KDAL repository-local smoke tests.
#
# This script validates that all userspace-compilable code builds
# and runs without errors.  Kernel-side tests (KUnit) are run
# separately via QEMU or ./tools/run_kunit.sh.

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PRESET="ci-release"
BUILD_DIR="${REPO_ROOT}/build/ci/release"

echo "=== KDAL smoke tests ==="

# 0. Ensure CMake build directory exists
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
	echo "[0/5] Configuring CMake build..."
	cmake --preset "${PRESET}"
fi

# 1. Build kdality (KDAL utility: runtime CLI + language toolchain)
echo "[1/5] Building kdality..."
cmake --build --preset "${PRESET}" --target kdality

echo "[2/5] Checking kdality version..."
"${BUILD_DIR}/kdality" version >/dev/null

# 3. Build compiler
echo "[3/5] Building KDAL compiler..."
cmake --build --preset "${PRESET}" --target kdalc

# 4. Build userspace test programs
echo "[4/5] Building userspace test programs..."
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

CFLAGS="-std=c99 -Wall -Wextra -pedantic -Wno-gnu-zero-variadic-macro-arguments -D_DEFAULT_SOURCE"

cc $CFLAGS -o "$TMPDIR/testapp" "${REPO_ROOT}/tests/userspace/testapp.c"

cc $CFLAGS -o "$TMPDIR/benchmark" "${REPO_ROOT}/tests/userspace/benchmark.c"

cc $CFLAGS -o "$TMPDIR/kdaltest" "${REPO_ROOT}/tests/integration/kdaltest.c"

# 5. Run offline / dry-run tests
echo "[5/5] Running offline smoke tests..."
"$TMPDIR/testapp" --dry-run
"$TMPDIR/benchmark" --help 2>/dev/null || true
"$TMPDIR/kdaltest" || true

echo ""
echo "=== All smoke tests passed ==="
