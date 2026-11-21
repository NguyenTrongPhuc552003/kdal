#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The first-project journey (docs/first-project-journey.md, rows J1 to J7):
# a new user's first commands, in a fresh project made by `kdality init`.
# Every command that runs and fails is a FAIL; only a missing build is one.

set -eu

# shellcheck disable=SC2034
E2E_SUITE="first-project"
. "$(dirname "$0")/lib.sh"

e2e_mktemp
SANDBOX="$E2E_TMPDIR"
BUILD_DIR="${KDAL_BUILD_DIR:-$E2E_ROOT/build/release}"
KDALITY="$BUILD_DIR/kdality"

if [ ! -x "$KDALITY" ]; then
	(cd "$E2E_ROOT" && cmake --preset release && cmake --build --preset release) >/dev/null 2>&1 || true
fi
if [ ! -x "$KDALITY" ]; then
	e2e_fail "kdality not built in $BUILD_DIR"
	e2e_summary
	exit 1
fi

# run CMD...: pass when it exits 0, fail (with its output) otherwise
run() {
	_label="$1"
	shift
	if _out="$("$@" 2>&1)"; then
		e2e_pass "$_label"
	else
		e2e_fail "$_label: $(printf '%s' "$_out" | tail -3 | tr '\n' ' ')"
	fi
}

# refuse EXPECT CMD...: pass when it exits non-zero and says EXPECT
refuse() {
	_label="$1"
	_want="$2"
	shift 2
	if _out="$("$@" 2>&1)"; then
		e2e_fail "$_label: exit 0"
	elif printf '%s' "$_out" | grep -q "$_want"; then
		e2e_pass "$_label"
	else
		e2e_fail "$_label: no '$_want' in: $(printf '%s' "$_out" | head -2 | tr '\n' ' ')"
	fi
}

e2e_section "J1: init, then make compile"
cd "$SANDBOX"
run "kdality init mydrv --class uart" "$KDALITY" init mydrv --class uart
cd "$SANDBOX/mydrv"
run "make compile" make compile KDALITY="$KDALITY"
assert_file_exists "output/mydrv_driver.c" "output/mydrv_driver.c"
assert_file_exists "output/Makefile.kbuild" "output/Makefile.kbuild"

e2e_section "J2: simulate the new driver"
assert_cmd_output "simulate names the driver" "Driver: mydrv_driver" "$KDALITY" simulate mydrv_driver.kdc
run "simulate exits 0" "$KDALITY" simulate mydrv_driver.kdc

e2e_section "J3: lint and test-gen in the new project"
run "lint mydrv.kdh" "$KDALITY" lint mydrv.kdh
run "lint mydrv_driver.kdc" "$KDALITY" lint mydrv_driver.kdc
run "test-gen -o tests (a new directory)" "$KDALITY" test-gen -o tests mydrv_driver.kdc
assert_file_exists "tests/test_mydrv_driver.c" "tests/test_mydrv_driver.c"

e2e_section "J4: no input"
refuse "lint with no input" "no input" "$KDALITY" lint
refuse "simulate with no input" "no input" "$KDALITY" simulate
refuse "compile with no input" "no input" "$KDALITY" compile
refuse "init with no name" "name required" "$KDALITY" init

e2e_section "J5: unknown options and a second input"
refuse "lint --bogus" "unknown option" "$KDALITY" lint --bogus mydrv.kdh
refuse "lint with two inputs" "more than one input" "$KDALITY" lint mydrv.kdh mydrv.kdh
refuse "simulate --bogus" "unknown option" "$KDALITY" simulate --bogus mydrv_driver.kdc
refuse "simulate with two inputs" "more than one input" "$KDALITY" simulate mydrv_driver.kdc mydrv_driver.kdc

e2e_section "J6: absolute and parent paths"
run "compile -o an absolute directory" "$KDALITY" compile -o "$SANDBOX/abs-out" mydrv_driver.kdc
run "compile -o ../rel-out" "$KDALITY" compile -o ../rel-out mydrv_driver.kdc
run "lint an absolute path" "$KDALITY" lint "$SANDBOX/mydrv/mydrv.kdh"
run "test-gen -o an absolute directory" "$KDALITY" test-gen -o "$SANDBOX/abs-tests" mydrv_driver.kdc
run "dt-gen -o dts (a new directory)" "$KDALITY" dt-gen mydrv.kdh -o "$SANDBOX/mydrv/dts"
run "lint the I2C example from the repository root (stdlib import)" "$KDALITY" lint "$E2E_ROOT/examples/kdc_hello/i2c_sensor.kdc"

e2e_section "J7: names inside generated files are still checked"
refuse "init with a name containing /" "invalid name" "$KDALITY" init "a/b"

e2e_summary
