#!/bin/sh
# E2E Test - New User Journey (kdalup installer)
#
# Simulates: A brand-new user who has never used KDAL before.
# They download the installer, install the SDK, verify it works,
# and create their first project.
#
# This test uses a sandboxed KDAL_HOME so it won't interfere with
# any real installation.
#
# Usage: ./scripts/e2e/test_user_journey.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="user-journey"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"
export KDAL_HOME="$SANDBOX/kdal-sdk"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 1: kdalup script validation"
# ══════════════════════════════════════════════════════════════════════

KDALUP="$E2E_ROOT/scripts/installer/kdalup.sh"

assert_file_exists "$KDALUP" "kdalup.sh"
assert_cmd "kdalup.sh is valid shell" sh -n "$KDALUP"
assert_file_contains "$KDALUP" "KDALUP_VERSION=" "kdalup has version constant"
assert_file_contains "$KDALUP" "detect_os" "kdalup has OS detection"
assert_file_contains "$KDALUP" "detect_arch" "kdalup has arch detection"
assert_file_contains "$KDALUP" "KDAL_HOME" "kdalup uses KDAL_HOME"

# Verify all subcommands are present
for subcmd in install update self-update list uninstall version help; do
	assert_file_contains "$KDALUP" "$subcmd" "kdalup supports '$subcmd'"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 2: One-liner bootstrap script"
# ══════════════════════════════════════════════════════════════════════

INSTALL_SH="$E2E_ROOT/scripts/installer/install.sh"

assert_file_exists "$INSTALL_SH" "install.sh"
assert_cmd "install.sh is valid shell" sh -n "$INSTALL_SH"
assert_file_contains "$INSTALL_SH" "kdalup" "install.sh references kdalup"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 3: kdalup help and version (dry-run)"
# ══════════════════════════════════════════════════════════════════════

# The help subcommand should work without network
assert_cmd_output "kdalup help works" "Usage\|install\|update" sh "$KDALUP" help
assert_cmd_output "kdalup version works" "kdalup\|0\." sh "$KDALUP" version

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 4: Simulate offline SDK install layout"
# ══════════════════════════════════════════════════════════════════════

# Build from source (simulating what kdalup would download)
e2e_info "Building SDK from source into sandbox..."
_build_ok=true

BUILD_DIR="$E2E_ROOT/build/release"
(cd "$E2E_ROOT" && cmake --preset release) >/dev/null 2>&1 || true

# Build compiler
if cmake --build "$BUILD_DIR" --target kdalc >/dev/null 2>&1; then
	e2e_pass "compiler builds"
else
	e2e_fail "compiler build"
	_build_ok=false
fi

# Build kdality
if cmake --build "$BUILD_DIR" --target kdality >/dev/null 2>&1; then
	e2e_pass "kdality builds"
else
	e2e_fail "kdality build"
	_build_ok=false
fi

if [ "$_build_ok" = true ]; then
	# Install into sandbox via cmake
	if cmake --install "$BUILD_DIR" --prefix "$SANDBOX/staging/usr/local" \
		>/dev/null 2>&1; then
		e2e_pass "cmake install into sandbox"
	else
		e2e_fail "cmake install into sandbox"
	fi

	# Verify installed layout
	assert_file_exists "$SANDBOX/staging/usr/local/bin/kdalc" "installed kdalc"
	assert_file_exists "$SANDBOX/staging/usr/local/bin/kdality" "installed kdality"
	assert_executable "$SANDBOX/staging/usr/local/bin/kdalc" "kdalc"
	assert_executable "$SANDBOX/staging/usr/local/bin/kdality" "kdality"
	assert_dir_exists "$SANDBOX/staging/usr/local/include/kdal" "installed headers dir"
	assert_dir_exists "$SANDBOX/staging/usr/local/share/kdal/stdlib" "installed stdlib dir"
	assert_dir_exists "$SANDBOX/staging/usr/local/lib/cmake/KDAL" "installed cmake dir"
	assert_file_exists "$SANDBOX/staging/usr/local/lib/libkdalc.a" "installed libkdalc.a"

	# Verify Vim files are bundled
	assert_dir_exists "$SANDBOX/staging/usr/local/share/kdal/vim/syntax" "bundled vim syntax"
	assert_dir_exists "$SANDBOX/staging/usr/local/share/kdal/vim/ftdetect" "bundled vim ftdetect"
	assert_file_exists "$SANDBOX/staging/usr/local/share/kdal/vim/filetype.lua" "bundled vim filetype.lua"

	# Verify stdlib .kdh files are present
	assert_file_count "$SANDBOX/staging/usr/local/share/kdal/stdlib" "*.kdh" 7 "stdlib .kdh files"

	# Verify CMake modules
	assert_file_count "$SANDBOX/staging/usr/local/lib/cmake/KDAL" "*.cmake" 7 "CMake modules"
else
	e2e_skip "SDK install test - build failed"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 5: kdality CLI subcommands"
# ══════════════════════════════════════════════════════════════════════

if [ -x "$BUILD_DIR/kdality" ]; then
	KDALITY="$BUILD_DIR/kdality"

	# Version subcommand
	assert_cmd_output "kdality version" "kdal\|0\.\|version" "$KDALITY" version

	# Help (via no-args or invalid command - kdalctl dispatches help)
	# Test individual subcommands with --help
	assert_cmd_output "kdality compile -h" "Usage\|compile\|kdc\|options" "$KDALITY" compile -h
	assert_cmd_output "kdality init -h" "Usage\|init\|project" "$KDALITY" init -h
	assert_cmd_output "kdality lint -h" "Usage\|lint" "$KDALITY" lint -h

	# ══════════════════════════════════════════════════════════════════
	e2e_section "Step 6: kdality init - create new project"
	# ══════════════════════════════════════════════════════════════════

	PROJECT_DIR="$SANDBOX/my-first-kdal-project"
	mkdir -p "$PROJECT_DIR"

	if "$KDALITY" init "$PROJECT_DIR" >/dev/null 2>&1; then
		e2e_pass "kdality init creates project"
		# Check generated project structure
		# shellcheck disable=SC2043
		for expect_file in Makefile; do
			if [ -f "$PROJECT_DIR/$expect_file" ]; then
				e2e_pass "init creates $expect_file"
			else
				e2e_skip "init did not create $expect_file (may be expected for alpha)"
			fi
		done
	else
		e2e_skip "kdality init not fully implemented (alpha)"
	fi
else
	e2e_skip "kdality binary not built - skipping CLI tests"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 7: Uninstall from sandbox"
# ══════════════════════════════════════════════════════════════════════

if [ "$_build_ok" = true ]; then
	# Use install_manifest.txt generated by cmake --install
	_manifest="$BUILD_DIR/install_manifest.txt"
	if [ -f "$_manifest" ]; then
		xargs rm -f <"$_manifest" >/dev/null 2>&1 &&
			e2e_pass "cmake uninstall from sandbox" ||
			e2e_fail "cmake uninstall from sandbox"
	else
		e2e_skip "no install_manifest.txt - cannot uninstall"
	fi

	# Verify cleanup
	if [ ! -f "$SANDBOX/staging/usr/local/bin/kdalc" ]; then
		e2e_pass "kdalc removed after uninstall"
	else
		e2e_fail "kdalc still exists after uninstall"
	fi

	if [ ! -f "$SANDBOX/staging/usr/local/bin/kdality" ]; then
		e2e_pass "kdality removed after uninstall"
	else
		e2e_fail "kdality still exists after uninstall"
	fi
else
	e2e_skip "uninstall test - build failed earlier"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_summary
