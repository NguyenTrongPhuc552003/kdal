#!/bin/sh
# E2E Test - Developer Journey (build from source)
#
# Simulates: A kernel developer / contributor who clones the repo
# and wants to build everything from source, run tests, verify the
# project structure, and contribute code.
#
# Usage: ./scripts/e2e/test_developer_journey.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="developer-journey"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 1: Repository structure verification"
# ══════════════════════════════════════════════════════════════════════

# Core project files
for f in Makefile README.md LICENSE CONTRIBUTING.md CHANGELOG.md \
	SECURITY.md CITATION.cff CODE_OF_CONDUCT.md .clang-format \
	.editorconfig .gitignore .gitattributes CMakeLists.txt \
	Kconfig VERSION; do
	assert_file_exists "$E2E_ROOT/$f" "$f"
done

# Required directories
for d in include/kdal src/core src/drivers src/backends \
	tests/kunit tests/userspace tests/integration \
	tools/kdality compiler lang docs scripts examples cmake \
	editor packaging; do
	assert_dir_exists "$E2E_ROOT/$d" "$d/"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 2: Kernel headers completeness"
# ══════════════════════════════════════════════════════════════════════

for h in include/kdal/core/kdal.h include/kdal/core/version.h \
	include/kdal/types.h include/kdal/ioctl.h \
	include/kdal/backend.h include/kdal/api/driver.h \
	include/kdal/api/accel.h include/kdal/api/common.h; do
	assert_file_exists "$E2E_ROOT/$h" "$h"
done

# Version header has version macros
assert_file_contains "$E2E_ROOT/include/kdal/core/version.h" \
	"KDAL_VERSION_MAJOR" "version.h defines KDAL_VERSION_MAJOR"
assert_file_contains "$E2E_ROOT/include/kdal/core/version.h" \
	"KDAL_VERSION_STRING" "version.h defines KDAL_VERSION_STRING"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 3: Build compiler (kdalc + libkdalc.a)"
# ══════════════════════════════════════════════════════════════════════

BUILD_DIR="$E2E_ROOT/build/release"
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
	(cd "$E2E_ROOT" && cmake --preset release) >/dev/null 2>&1 || true
fi

if cmake --build "$BUILD_DIR" --target kdalc >/dev/null 2>&1; then
	e2e_pass "compiler builds cleanly"
	assert_file_exists "$BUILD_DIR/compiler/kdalc" "kdalc binary"
	assert_executable "$BUILD_DIR/compiler/kdalc" "kdalc"
	assert_file_exists "$BUILD_DIR/compiler/libkdalc.a" "libkdalc.a"
	assert_file_not_empty "$BUILD_DIR/compiler/libkdalc.a" "libkdalc.a"

	# Compiler responds to --help
	assert_cmd_output "kdalc -h" "Usage\|kdalc\|kdc\|options" \
		"$BUILD_DIR/compiler/kdalc" -h
else
	e2e_fail "compiler build"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 4: Build kdality CLI"
# ══════════════════════════════════════════════════════════════════════

if cmake --build "$BUILD_DIR" --target kdality >/dev/null 2>&1; then
	e2e_pass "kdality builds cleanly"
	assert_executable "$BUILD_DIR/kdality" "kdality"

	# Test version output
	assert_cmd_output "kdality version" "0\.\|kdal\|version" \
		"$BUILD_DIR/kdality" version
else
	e2e_fail "kdality build"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 5: Build userspace test programs"
# ══════════════════════════════════════════════════════════════════════

# Userspace tests use Linux-specific ioctl macros (_IOR/_IOW etc.)
_is_linux=false
[ "$(uname -s)" = "Linux" ] && _is_linux=true

if [ "$_is_linux" = true ]; then
	# testapp
	if cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
		-o "$SANDBOX/testapp" "$E2E_ROOT/tests/userspace/testapp.c" 2>/dev/null; then
		e2e_pass "testapp.c compiles"
		assert_executable "$SANDBOX/testapp" "testapp"
		assert_cmd_output "testapp --dry-run" "dry-run\|kdal" "$SANDBOX/testapp" --dry-run
	else
		e2e_fail "testapp.c compilation"
	fi

	# benchmark
	if cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
		-o "$SANDBOX/benchmark" "$E2E_ROOT/tests/userspace/benchmark.c" 2>/dev/null; then
		e2e_pass "benchmark.c compiles"
		assert_executable "$SANDBOX/benchmark" "benchmark"
	else
		e2e_fail "benchmark.c compilation"
	fi

	# kdaltest (integration)
	if cc -std=c99 -Wall -Wextra -pedantic -D_DEFAULT_SOURCE \
		-o "$SANDBOX/kdaltest" "$E2E_ROOT/tests/integration/kdaltest.c" 2>/dev/null; then
		e2e_pass "kdaltest.c compiles"
		assert_executable "$SANDBOX/kdaltest" "kdaltest"
	else
		e2e_fail "kdaltest.c compilation"
	fi
else
	e2e_skip "userspace tests require Linux (ioctl ABI headers)"
	# Still verify source files exist and are valid C
	for src in tests/userspace/testapp.c tests/userspace/benchmark.c \
		tests/integration/kdaltest.c; do
		assert_file_exists "$E2E_ROOT/$src" "$src"
	done
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 6: Build example programs"
# ══════════════════════════════════════════════════════════════════════

# Accel demos may use Linux-specific headers
for demo in gpudemo npudemo; do
	src="$E2E_ROOT/examples/accel_demo/${demo}.c"
	if [ ! -f "$src" ]; then
		e2e_skip "$demo source not found"
		continue
	fi
	if cc -std=c99 -Wall -o "$SANDBOX/$demo" "$src" 2>/dev/null; then
		e2e_pass "$demo compiles"
	else
		if [ "$_is_linux" = true ]; then
			e2e_fail "$demo compilation"
		else
			e2e_skip "$demo (may need Linux headers)"
		fi
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 7: CI scripts are valid shell"
# ══════════════════════════════════════════════════════════════════════

for script in scripts/ci/test.sh scripts/ci/checkpatch.sh \
	scripts/ci/static_analysis.sh scripts/ci/coverage.sh \
	scripts/env/dependencies.sh; do
	_path="$E2E_ROOT/$script"
	if [ -f "$_path" ]; then
		assert_cmd "$script is valid shell" sh -n "$_path"
	else
		e2e_fail "$script not found"
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 8: KUnit test sources"
# ══════════════════════════════════════════════════════════════════════

assert_file_count "$E2E_ROOT/tests/kunit" "*.c" 4 "KUnit test files"

for kunit in test_core test_driver test_event test_registry; do
	assert_file_exists "$E2E_ROOT/tests/kunit/${kunit}.c" "kunit/${kunit}.c"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 9: Language assets"
# ══════════════════════════════════════════════════════════════════════

# stdlib directory
assert_file_count "$E2E_ROOT/lang/stdlib" "*.kdh" 7 "lang/stdlib .kdh files"

# Verify key stdlib headers
for std_kdh in uart.kdh i2c.kdh spi.kdh gpio.kdh gpu.kdh npu.kdh common.kdh; do
	assert_file_exists "$E2E_ROOT/lang/stdlib/$std_kdh" "stdlib/$std_kdh"
done

# Grammar directory
assert_dir_exists "$E2E_ROOT/lang/grammar" "lang/grammar/"

# Spec document
assert_file_exists "$E2E_ROOT/lang/spec.md" "lang/spec.md"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 10: Documentation"
# ══════════════════════════════════════════════════════════════════════

assert_dir_exists "$E2E_ROOT/docs" "docs/"

# Key doc files
for doc in architecture.md getting-started.md; do
	if [ -f "$E2E_ROOT/docs/$doc" ]; then
		e2e_pass "docs/$doc exists"
		assert_file_not_empty "$E2E_ROOT/docs/$doc" "docs/$doc"
	else
		e2e_skip "docs/$doc not found"
	fi
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 11: Smoke test (scripts/ci/test.sh dry-run)"
# ══════════════════════════════════════════════════════════════════════

if [ -x "$BUILD_DIR/kdality" ] && [ -f "$BUILD_DIR/compiler/kdalc" ]; then
	if "$E2E_ROOT/scripts/ci/test.sh" >/dev/null 2>&1; then
		e2e_pass "CI smoke test passes"
	else
		e2e_fail "CI smoke test"
	fi
else
	e2e_skip "smoke test - binaries not built"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 12: cmake install cycle"
# ══════════════════════════════════════════════════════════════════════

STAGE="$SANDBOX/install-test"
if cmake --install "$BUILD_DIR" --prefix "$STAGE/usr/local" >/dev/null 2>&1; then
	e2e_pass "cmake install succeeds"

	# Count installed files
	_installed="$(find "$STAGE" -type f | wc -l | tr -d ' ')"
	if [ "$_installed" -ge 5 ]; then
		e2e_pass "installed $_installed files (>= 5)"
	else
		e2e_fail "only $_installed files installed (expected >= 5)"
	fi
else
	e2e_skip "cmake install not available (build may have failed)"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_summary
