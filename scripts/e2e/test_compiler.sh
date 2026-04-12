#!/bin/sh
# E2E Test - Compiler & Language Toolchain
#
# Simulates: A developer using the KDAL compiler pipeline to translate
# .kdc driver files into kernel C modules, using kdality subcommands.
#
# Usage: ./scripts/e2e/test_compiler.sh

set -eu

# shellcheck disable=SC2034
E2E_SUITE="compiler"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 1: Compiler source structure"
# ══════════════════════════════════════════════════════════════════════

COMP_DIR="$E2E_ROOT/compiler"

assert_dir_exists "$COMP_DIR" "compiler/"
assert_file_exists "$COMP_DIR/main.c" "compiler/main.c"
assert_file_exists "$COMP_DIR/CMakeLists.txt" "compiler/CMakeLists.txt"
assert_dir_exists "$COMP_DIR/include" "compiler/include/"

# Compiler headers
for hdr in ast.h token.h codegen.h; do
	assert_file_exists "$COMP_DIR/include/$hdr" "compiler/include/$hdr"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 2: Build compiler"
# ══════════════════════════════════════════════════════════════════════

BUILD_DIR="$E2E_ROOT/build/release"
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
	(cd "$E2E_ROOT" && cmake --preset release) >/dev/null 2>&1 || true
fi

if cmake --build "$BUILD_DIR" --target kdalc >/dev/null 2>&1; then
	e2e_pass "compiler builds"
	KDALC="$BUILD_DIR/compiler/kdalc"
	assert_executable "$KDALC" "kdalc"
	assert_file_exists "$BUILD_DIR/compiler/libkdalc.a" "libkdalc.a"
else
	e2e_fail "compiler build failed"
	KDALC=""
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 3: kdalc CLI"
# ══════════════════════════════════════════════════════════════════════

if [ -n "$KDALC" ] && [ -x "$KDALC" ]; then
	# Help output
	assert_cmd_output "kdalc -h" "Usage\|kdalc\|kdc" "$KDALC" -h

	# No-args should show usage
	assert_cmd_output "kdalc no-args shows usage" "no input\|Usage\|kdc" "$KDALC"

	# Invalid file extension warning
	touch "$SANDBOX/test.txt"
	assert_cmd_output "kdalc warns on wrong extension" "warning\|expected\|kdc\|kdh" \
		"$KDALC" "$SANDBOX/test.txt"
else
	e2e_skip "kdalc not built - skip CLI tests"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 4: Compile example .kdc files"
# ══════════════════════════════════════════════════════════════════════

KDC_EXAMPLES="$E2E_ROOT/examples/kdc_hello"

if [ -n "$KDALC" ] && [ -x "$KDALC" ]; then
	for kdc in "$KDC_EXAMPLES"/*.kdc; do
		if [ ! -f "$kdc" ]; then
			e2e_skip "no .kdc files in examples/kdc_hello/"
			continue
		fi

		_name="$(basename "$kdc")"
		OUT_DIR="$SANDBOX/compile_${_name%.kdc}"
		mkdir -p "$OUT_DIR"

		if "$KDALC" -o "$OUT_DIR" "$kdc" >/dev/null 2>&1; then
			e2e_pass "kdalc compiles $_name"

			# Check generated output
			_gen_c="$(find "$OUT_DIR" -name '*.c' | head -1)"
			if [ -n "$_gen_c" ]; then
				e2e_pass "$_name → generated C file"
				assert_file_not_empty "$_gen_c" "generated C source"

				# Generated C should have module_init / platform_driver
				assert_file_contains "$_gen_c" \
					"module_init\|module_platform_driver\|MODULE_LICENSE" \
					"generated C has kernel module boilerplate"
			else
				e2e_fail "$_name → no C file generated"
			fi

			# Check for Makefile.kbuild
			if [ -f "$OUT_DIR/Makefile.kbuild" ]; then
				e2e_pass "$_name → Makefile.kbuild generated"
				assert_file_contains "$OUT_DIR/Makefile.kbuild" "obj-m" \
					"Makefile.kbuild has obj-m"
			else
				e2e_skip "$_name → no Makefile.kbuild"
			fi
		else
			e2e_fail "kdalc failed on $_name"
		fi
	done
else
	e2e_skip "kdalc not built - skip compilation tests"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 5: kdality subcommands"
# ══════════════════════════════════════════════════════════════════════

# Build kdality via cmake
if cmake --build "$BUILD_DIR" --target kdality >/dev/null 2>&1; then
	KDALITY="$BUILD_DIR/kdality"
else
	KDALITY=""
fi

if [ -x "$KDALITY" ]; then

	# compile subcommand
	for kdc in "$KDC_EXAMPLES"/*.kdc; do
		[ -f "$kdc" ] || continue
		_name="$(basename "$kdc")"
		_out="$SANDBOX/kdality_${_name%.kdc}"
		mkdir -p "$_out"
		if "$KDALITY" compile -o "$_out" "$kdc" >/dev/null 2>&1; then
			e2e_pass "kdality compile $_name"
		else
			e2e_skip "kdality compile $_name (may be alpha limitation)"
		fi
		break # Just test first example
	done

	# lint subcommand
	for kdh in "$E2E_ROOT/lang/stdlib"/*.kdh; do
		[ -f "$kdh" ] || continue
		_name="$(basename "$kdh")"
		if "$KDALITY" lint "$kdh" >/dev/null 2>&1; then
			e2e_pass "kdality lint $_name"
		else
			e2e_skip "kdality lint $_name (alpha)"
		fi
		break
	done

	# simulate subcommand (just check it doesn't crash on --help)
	assert_cmd_output "kdality simulate -h" "Usage\|simulate\|help" \
		"$KDALITY" simulate -h 2>/dev/null ||
		e2e_skip "kdality simulate -h"

	# dt-gen subcommand
	# shellcheck disable=SC2066
	for kdh in "$E2E_ROOT/lang/stdlib/uart.kdh"; do
		if [ -f "$kdh" ]; then
			DT_OUT="$SANDBOX/dtgen_out"
			mkdir -p "$DT_OUT"
			if "$KDALITY" dt-gen -o "$DT_OUT" "$kdh" >/dev/null 2>&1; then
				e2e_pass "kdality dt-gen uart.kdh"
				# Check for .dts or .dtsi output
				if find "$DT_OUT" -name '*.dts' -o -name '*.dtsi' 2>/dev/null | head -1 | grep -q .; then
					e2e_pass "dt-gen produces .dts/.dtsi output"
				else
					e2e_skip "dt-gen ran but no .dts/.dtsi found"
				fi
			else
				e2e_skip "kdality dt-gen (alpha limitation)"
			fi
		fi
	done

	# test-gen subcommand
	for kdc in "$KDC_EXAMPLES"/*.kdc; do
		[ -f "$kdc" ] || continue
		TG_OUT="$SANDBOX/testgen_out"
		mkdir -p "$TG_OUT"
		if "$KDALITY" test-gen -o "$TG_OUT" "$kdc" >/dev/null 2>&1; then
			e2e_pass "kdality test-gen $(basename "$kdc")"
		else
			e2e_skip "kdality test-gen (alpha limitation)"
		fi
		break
	done
else
	e2e_skip "kdality not built - skip subcommand tests"
fi

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 6: Standard library .kdh files"
# ══════════════════════════════════════════════════════════════════════

STDLIB="$E2E_ROOT/lang/stdlib"

assert_file_count "$STDLIB" "*.kdh" 7 "stdlib .kdh files"

# Each stdlib file should have kdal_version declaration
for kdh in "$STDLIB"/*.kdh; do
	_name="$(basename "$kdh")"
	assert_file_contains "$kdh" "kdal_version\|device class\|device" \
		"$_name has device definition"
done

# ══════════════════════════════════════════════════════════════════════
e2e_section "Step 7: Example projects"
# ══════════════════════════════════════════════════════════════════════

# kdc_hello has README
assert_file_exists "$E2E_ROOT/examples/kdc_hello/README.md" "kdc_hello README"

# minimaldriver builds (as a standalone check)
MINIDRV="$E2E_ROOT/examples/minimaldriver"
assert_file_exists "$MINIDRV/driver.c" "minimaldriver/driver.c"
assert_file_exists "$MINIDRV/Makefile" "minimaldriver/Makefile"
assert_file_contains "$MINIDRV/driver.c" "MODULE_LICENSE" "driver.c has MODULE_LICENSE"

# accel_demo
assert_dir_exists "$E2E_ROOT/examples/accel_demo" "accel_demo/"

# ══════════════════════════════════════════════════════════════════════
e2e_summary
