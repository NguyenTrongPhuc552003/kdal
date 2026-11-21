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
	echo "[0/6] Configuring CMake build..."
	cmake --preset "${PRESET}"
fi

# 1. Build kdality (KDAL utility: runtime CLI + language toolchain)
echo "[1/6] Building kdality..."
cmake --build --preset "${PRESET}" --target kdality

echo "[2/6] Checking kdality version..."
"${BUILD_DIR}/kdality" version >/dev/null

# 3. Build compiler
echo "[3/6] Building KDAL compiler..."
cmake --build --preset "${PRESET}" --target kdalc

# 4. Build userspace test programs
echo "[4/6] Building userspace test programs..."
TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

CFLAGS="-std=c99 -Wall -Wextra -pedantic -Wno-gnu-zero-variadic-macro-arguments -D_DEFAULT_SOURCE"

cc $CFLAGS -o "$TMPDIR/testapp" "${REPO_ROOT}/tests/userspace/testapp.c"

cc $CFLAGS -o "$TMPDIR/benchmark" "${REPO_ROOT}/tests/userspace/benchmark.c"

cc $CFLAGS -o "$TMPDIR/kdaltest" "${REPO_ROOT}/tests/integration/kdaltest.c"

# 5. Run offline / dry-run tests
echo "[5/6] Running offline smoke tests..."
"$TMPDIR/testapp" --dry-run
"$TMPDIR/benchmark" --help 2>/dev/null || true
"$TMPDIR/kdaltest" || true

# 6. dt-gen on both .kdh syntaxes
echo "[6/6] Checking kdality dt-gen..."
sh "${REPO_ROOT}/scripts/ci/dtgen-check.sh" "${BUILD_DIR}/kdality"

# docs/open-findings-20261121.md, row O5: the demand report from the KUnit
# console log in which KUnit's lines landed between the markers
python3 "${REPO_ROOT}/scripts/ci/demand-extract.py" \
	"${REPO_ROOT}/docs/evidence/kunit-demand-interleave-20261121.txt" "${BUILD_DIR}/demand-o5.json"
grep -q '"kdal-gpio@9030000"' "${BUILD_DIR}/demand-o5.json"
echo "ok: the demand report is extracted from the interleaved log (O5)"

echo ""
echo "=== All smoke tests passed ==="
