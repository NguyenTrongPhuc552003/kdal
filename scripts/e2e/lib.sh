#!/bin/sh
# E2E test framework - shared library.
#
# Source this file at the top of every test script:
#   . "$(dirname "$0")/lib.sh"
#
# Provides: colored output, pass/fail counters, assertions, cleanup.

set -eu

# ── State ─────────────────────────────────────────────────────────────
E2E_PASS=0
E2E_FAIL=0
E2E_SKIP=0
E2E_SUITE="${E2E_SUITE:-$(basename "$0" .sh)}"
E2E_TMPDIR=""
# shellcheck disable=SC2034
E2E_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

# ── Colors ────────────────────────────────────────────────────────────
if [ -t 1 ] && [ "${NO_COLOR:-}" = "" ]; then
	C_RED='\033[1;31m'
	C_GREEN='\033[1;32m'
	C_YELLOW='\033[1;33m'
	C_BLUE='\033[1;34m'
	C_BOLD='\033[1m'
	C_RESET='\033[0m'
else
	C_RED='' C_GREEN='' C_YELLOW='' C_BLUE='' C_BOLD='' C_RESET=''
fi

# ── Output helpers ────────────────────────────────────────────────────
e2e_info() { printf "${C_BLUE}==> %s${C_RESET}\n" "$1"; }
e2e_pass() {
	printf "${C_GREEN}  PASS: %s${C_RESET}\n" "$1"
	E2E_PASS=$((E2E_PASS + 1))
}
e2e_fail() {
	printf "${C_RED}  FAIL: %s${C_RESET}\n" "$1"
	E2E_FAIL=$((E2E_FAIL + 1))
}
e2e_skip() {
	printf "${C_YELLOW}  SKIP: %s${C_RESET}\n" "$1"
	E2E_SKIP=$((E2E_SKIP + 1))
}
e2e_section() { printf "\n${C_BOLD}[%s] %s${C_RESET}\n" "$E2E_SUITE" "$1"; }

# ── Temp directory ────────────────────────────────────────────────────
# Call directly (not in subshell): e2e_mktemp
# Then use $E2E_TMPDIR
e2e_mktemp() {
	E2E_TMPDIR="$(mktemp -d)"
	# shellcheck disable=SC2064
	trap "rm -rf '$E2E_TMPDIR'" EXIT INT TERM
}

# ── Assertions ────────────────────────────────────────────────────────

# assert_file_exists <path> [label]
assert_file_exists() {
	_label="${2:-$1}"
	if [ -f "$1" ]; then
		e2e_pass "$_label exists"
	else
		e2e_fail "$_label does not exist: $1"
	fi
}

# assert_dir_exists <path> [label]
assert_dir_exists() {
	_label="${2:-$1}"
	if [ -d "$1" ]; then
		e2e_pass "$_label exists"
	else
		e2e_fail "$_label does not exist: $1"
	fi
}

# assert_executable <path> [label]
assert_executable() {
	_label="${2:-$1}"
	if [ -x "$1" ]; then
		e2e_pass "$_label is executable"
	else
		e2e_fail "$_label is not executable: $1"
	fi
}

# assert_file_contains <path> <pattern> [label]
assert_file_contains() {
	_label="${3:-$1 contains $2}"
	if grep -q "$2" "$1" 2>/dev/null; then
		e2e_pass "$_label"
	else
		e2e_fail "$_label"
	fi
}

# assert_file_not_empty <path> [label]
assert_file_not_empty() {
	_label="${2:-$1}"
	if [ -s "$1" ]; then
		e2e_pass "$_label is not empty"
	else
		e2e_fail "$_label is empty or missing"
	fi
}

# assert_cmd <description> <cmd...>
# Runs a command and checks exit code == 0.
assert_cmd() {
	_desc="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		e2e_pass "$_desc"
	else
		e2e_fail "$_desc (exit $?)"
	fi
}

# assert_cmd_fails <description> <cmd...>
# Runs a command and checks exit code != 0.
assert_cmd_fails() {
	_desc="$1"
	shift
	if "$@" >/dev/null 2>&1; then
		e2e_fail "$_desc (expected failure but got success)"
	else
		e2e_pass "$_desc (correctly failed)"
	fi
}

# assert_cmd_output <description> <pattern> <cmd...>
# Runs a command and checks stdout/stderr contains pattern.
assert_cmd_output() {
	_desc="$1"
	_pattern="$2"
	shift 2
	_out="$("$@" 2>&1 || true)"
	if echo "$_out" | grep -q "$_pattern"; then
		e2e_pass "$_desc"
	else
		e2e_fail "$_desc (pattern '$_pattern' not in output)"
	fi
}

# assert_file_count <dir> <pattern> <min_count> [label]
assert_file_count() {
	_dir="$1"
	_pattern="$2"
	_min="$3"
	_label="${4:-$_dir/$_pattern}"
	_count="$(find "$_dir" -name "$_pattern" 2>/dev/null | wc -l | tr -d ' ')"
	if [ "$_count" -ge "$_min" ]; then
		e2e_pass "$_label: $_count files (>= $_min)"
	else
		e2e_fail "$_label: only $_count files (expected >= $_min)"
	fi
}

# assert_symlink <path> [label]
assert_symlink() {
	_label="${2:-$1}"
	if [ -L "$1" ]; then
		e2e_pass "$_label is a symlink"
	else
		e2e_fail "$_label is not a symlink"
	fi
}

# ── Summary ───────────────────────────────────────────────────────────
e2e_summary() {
	_total=$((E2E_PASS + E2E_FAIL + E2E_SKIP))
	printf "\n${C_BOLD}=== %s: %d passed, %d failed, %d skipped (of %d) ===${C_RESET}\n" \
		"$E2E_SUITE" "$E2E_PASS" "$E2E_FAIL" "$E2E_SKIP" "$_total"

	if [ "$E2E_FAIL" -gt 0 ]; then
		return 1
	fi
	return 0
}

# ── require_tool ──────────────────────────────────────────────────────
# Check if a command is available; skip test group if missing.
require_tool() {
	if ! command -v "$1" >/dev/null 2>&1; then
		e2e_skip "required tool not found: $1"
		return 1
	fi
	return 0
}
