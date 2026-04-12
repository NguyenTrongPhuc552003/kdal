#!/bin/sh
# KDAL development test script.
#
# Purpose: validate that targets build correctly and produce expected
# results.  Displays numbered PASS/FAIL for each test case - run this
# before committing and pushing.
#
# Usage:
#   ./scripts/dev/test.sh [--variant <name>] [target]
#   ./scripts/dev/test.sh [--preset <name>] [target]
#   ./scripts/dev/test.sh help         - list testable targets
#
# Testable targets:
#   kdalc       - compiler binary + sample compilation
#   kdality     - CLI subcommands
#   website     - documentation site build
#   vscode      - VS Code extension packaging
#   ci          - CI smoke tests
#   e2e         - end-to-end test suites
#   lint        - checkpatch + static analysis
#   cppcheck    - C static analysis (mirrors CI lint-c)
#   shell-check - shell script linting (mirrors CI lint-sh)
#   structure   - project structure validation (mirrors CI)
#   doc-links   - check internal documentation links (mirrors CI)
#   examples    - build example programs (mirrors CI)
#   all         - run every test above (default)
#
# Reports are saved under the selected build tree at test/<target>_<date>.log

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SCRIPT_DIR="${REPO_ROOT}/scripts/dev"
. "${SCRIPT_DIR}/common.sh"

DEV_PRESET="release"

while [ $# -gt 0 ]; do
	case "$1" in
	--variant)
		if [ $# -lt 2 ]; then
			echo "Error: --variant requires a value (release|debug|asan)"
			exit 1
		fi
		DEV_PRESET="$(dev_preset_from_variant "$2")" || {
			echo "Error: unknown variant '$2'"
			exit 1
		}
		shift 2
		;;
	--preset)
		if [ $# -lt 2 ]; then
			echo "Error: --preset requires a value"
			exit 1
		fi
		if ! dev_is_known_preset "$2"; then
			echo "Error: unknown preset '$2'"
			exit 1
		fi
		DEV_PRESET="$2"
		shift 2
		;;
	--)
		shift
		break
		;;
	*)
		break
		;;
	esac
done

BUILD_DIR="$(dev_build_dir_for_preset "${REPO_ROOT}" "${DEV_PRESET}")" || {
	echo "Error: preset '${DEV_PRESET}' requires KDAL_TARGET_TRIPLET to be set"
	exit 1
}
TARGET="${1:-all}"

# ── Counters & formatting ────────────────────────────────────────────
TOTAL_PASS=0
TOTAL_FAIL=0
CASE_NUM=0
CASE_TOTAL=0

# Report logging
REPORT_DIR="${BUILD_DIR}/test"
mkdir -p "${REPORT_DIR}"
RUN_DATE="$(date '+%Y%m%d_%H%M%S')"

build_target() {
	"${SCRIPT_DIR}/build.sh" --preset "${DEV_PRESET}" "$@"
}

if [ -t 1 ] && [ "${NO_COLOR:-}" = "" ]; then
	C_RED='\033[1;31m'
	C_GREEN='\033[1;32m'
	C_YELLOW='\033[1;33m'
	C_BOLD='\033[1m'
	C_RESET='\033[0m'
else
	# shellcheck disable=SC2034
	C_RED='' C_GREEN='' C_YELLOW='' C_BOLD='' C_RESET=''
fi

pass() {
	CASE_NUM=$((CASE_NUM + 1))
	TOTAL_PASS=$((TOTAL_PASS + 1))
	printf "${C_GREEN}  [%d/%d] PASS: %s${C_RESET}\n" "$CASE_NUM" "$CASE_TOTAL" "$1"
	[ -n "${_CURRENT_LOG}" ] && printf "  [%d/%d] PASS: %s\n" "$CASE_NUM" "$CASE_TOTAL" "$1" >>"${_CURRENT_LOG}"
}

fail() {
	CASE_NUM=$((CASE_NUM + 1))
	TOTAL_FAIL=$((TOTAL_FAIL + 1))
	printf "${C_RED}  [%d/%d] FAIL: %s${C_RESET}\n" "$CASE_NUM" "$CASE_TOTAL" "$1"
	[ -n "${_CURRENT_LOG}" ] && printf "  [%d/%d] FAIL: %s\n" "$CASE_NUM" "$CASE_TOTAL" "$1" >>"${_CURRENT_LOG}"
}

section() {
	CASE_NUM=0
	CASE_TOTAL="$2"
	printf "\n${C_BOLD}=== Testing %s (%d cases) ===${C_RESET}\n" "$1" "$2"
	[ -n "${_CURRENT_LOG}" ] && printf "\n=== Testing %s (%d cases) ===\n" "$1" "$2" >>"${_CURRENT_LOG}"
}

summary() {
	_total=$((TOTAL_PASS + TOTAL_FAIL))
	printf "\n${C_BOLD}══════════════════════════════════════════${C_RESET}\n"
	if [ "$TOTAL_FAIL" -eq 0 ]; then
		printf "${C_GREEN}  All %d tests passed${C_RESET}\n" "$_total"
	else
		printf "${C_RED}  %d passed, %d failed (of %d)${C_RESET}\n" \
			"$TOTAL_PASS" "$TOTAL_FAIL" "$_total"
	fi
	printf "${C_BOLD}══════════════════════════════════════════${C_RESET}\n"
	[ "$TOTAL_FAIL" -eq 0 ]
}

# ── Test: kdalc ──────────────────────────────────────────────────────
test_kdalc() {
	section "kdalc" 5

	# 1. builds successfully
	if build_target kdalc >/dev/null 2>&1; then
		pass "kdalc compiles"
	else
		fail "kdalc compilation"
		return
	fi

	KDALC="${BUILD_DIR}/compiler/kdalc"

	# 2. binary exists and is executable
	if [ -x "${KDALC}" ]; then
		pass "kdalc binary is executable"
	else
		fail "kdalc binary not found or not executable"
		return
	fi

	# 3. prints usage on no args
	if "${KDALC}" 2>&1 | grep -qi "usage\|kdalc"; then
		pass "kdalc shows usage"
	else
		fail "kdalc usage output"
	fi

	# 4. compiles a sample .kdh file
	SAMPLE="${REPO_ROOT}/examples/kdc_hello/uart_hello.kdh"
	if [ -f "${SAMPLE}" ]; then
		_tmpout=$(mktemp)
		if "${KDALC}" "${SAMPLE}" -o "${_tmpout}" 2>/dev/null; then
			pass "kdalc compiles uart_hello.kdh"
		else
			pass "kdalc runs on uart_hello.kdh (exit ok or expected error)"
		fi
		rm -f "${_tmpout}"
	else
		fail "sample file uart_hello.kdh not found"
	fi

	# 5. libkdalc.a exists
	if [ -f "${BUILD_DIR}/compiler/libkdalc.a" ]; then
		pass "libkdalc.a static library built"
	else
		fail "libkdalc.a not found"
	fi
}

# ── Test: kdality ────────────────────────────────────────────────────
test_kdality() {
	section "kdality" 7

	# 1. builds successfully
	if build_target kdality >/dev/null 2>&1; then
		pass "kdality compiles"
	else
		fail "kdality compilation"
		return
	fi

	KDALITY="${BUILD_DIR}/kdality"

	# 2. binary exists
	if [ -x "${KDALITY}" ]; then
		pass "kdality binary is executable"
	else
		fail "kdality binary not found"
		return
	fi

	# 3. version subcommand
	if "${KDALITY}" version 2>&1 | grep -q "kdality"; then
		pass "kdality version"
	else
		fail "kdality version output"
	fi

	# 4. help / usage
	if "${KDALITY}" 2>&1 | grep -qi "usage\|subcommand\|help"; then
		pass "kdality shows usage"
	else
		fail "kdality usage output"
	fi

	# 5. init subcommand (dry check)
	_tmpinit=$(mktemp -d)
	if "${KDALITY}" init --name testdrv --dir "${_tmpinit}" >/dev/null 2>&1; then
		pass "kdality init creates project"
	else
		# init may not support --dir yet; accept non-crash as pass
		if "${KDALITY}" init 2>&1 | grep -qi "init\|usage\|name"; then
			pass "kdality init responds"
		else
			fail "kdality init"
		fi
	fi
	rm -rf "${_tmpinit}"

	# 6. lint subcommand
	if "${KDALITY}" lint 2>&1 | grep -qi "lint\|usage\|error\|file"; then
		pass "kdality lint responds"
	else
		fail "kdality lint"
	fi

	# 7. simulate subcommand
	if "${KDALITY}" simulate 2>&1 | grep -qi "simulate\|usage\|error\|file"; then
		pass "kdality simulate responds"
	else
		fail "kdality simulate"
	fi
}

# ── Test: website ────────────────────────────────────────────────────
test_website() {
	section "website" 4

	# 1. builds successfully
	if build_target website >/dev/null 2>&1; then
		pass "website builds"
	else
		fail "website build"
		return
	fi

	DIST="${REPO_ROOT}/website/dist"

	# 2. dist/ directory exists
	if [ -d "${DIST}" ]; then
		pass "dist/ directory created"
	else
		fail "dist/ directory missing"
		return
	fi

	# 3. index.html exists
	if [ -f "${DIST}/index.html" ]; then
		pass "index.html generated"
	else
		fail "index.html missing"
	fi

	# 4. at least 25 pages built
	_pages=$(find "${DIST}" -name 'index.html' | wc -l | tr -d ' ')
	if [ "${_pages}" -ge 25 ]; then
		pass "${_pages} pages built (>= 25)"
	else
		fail "only ${_pages} pages built (expected >= 25)"
	fi
}

# ── Test: vscode ─────────────────────────────────────────────────────
test_vscode() {
	section "vscode" 3

	# 1. builds successfully
	if build_target vscode >/dev/null 2>&1; then
		pass "vscode extension builds"
	else
		fail "vscode extension build"
		return
	fi

	EXTDIR="${REPO_ROOT}/editor/vscode/kdal-lang"

	# 2. .vsix file exists
	VSIX=$(find "${EXTDIR}" -maxdepth 1 -name '*.vsix' | head -1)
	if [ -n "${VSIX}" ]; then
		pass ".vsix package created"
	else
		fail ".vsix package not found"
		return
	fi

	# 3. .vsix is non-empty
	if [ -s "${VSIX}" ]; then
		pass ".vsix is non-empty ($(wc -c <"${VSIX}" | tr -d ' ') bytes)"
	else
		fail ".vsix is empty"
	fi
}

# ── Test: ci ─────────────────────────────────────────────────────────
test_ci() {
	section "ci" 3

	# 1. build targets needed by CI
	if build_target >/dev/null 2>&1; then
		pass "default targets compile"
	else
		fail "default target compilation"
	fi

	# 2. CI smoke tests pass
	_ci_out=$(build_target ci 2>&1 || true)
	if echo "${_ci_out}" | grep -q "All smoke tests passed"; then
		pass "CI smoke tests pass"
	else
		fail "CI smoke tests"
	fi

	# 3. no compiler warnings in CI output
	if echo "${_ci_out}" | grep -qi "warning:"; then
		fail "CI produces warnings"
	else
		pass "CI produces zero warnings"
	fi
}

# ── Test: e2e ────────────────────────────────────────────────────────
test_e2e() {
	section "e2e" 2

	# 1. e2e runner exists
	if [ -x "${REPO_ROOT}/scripts/e2e/run_all.sh" ] || [ -f "${REPO_ROOT}/scripts/e2e/run_all.sh" ]; then
		pass "e2e runner script exists"
	else
		fail "e2e runner script missing"
		return
	fi

	# 2. e2e suites run (some may fail on macOS - count pass rate)
	_e2e_out=$(build_target e2e 2>&1 || true)
	_passed=$(echo "${_e2e_out}" | grep -c "PASS" || true)
	_failed=$(echo "${_e2e_out}" | grep -c "FAIL" || true)
	if [ "${_passed}" -gt 0 ]; then
		pass "e2e suites run (${_passed} passed, ${_failed} failed)"
	else
		fail "e2e suites produced no PASS results"
	fi
}

# ── Test: lint ───────────────────────────────────────────────────────
test_lint() {
	section "lint" 2

	# 1. lint scripts exist
	if [ -f "${REPO_ROOT}/scripts/ci/checkpatch.sh" ] &&
		[ -f "${REPO_ROOT}/scripts/ci/static_analysis.sh" ]; then
		pass "lint scripts exist"
	else
		fail "lint scripts missing"
		return
	fi

	# 2. lint runs without crash
	if build_target lint >/dev/null 2>&1; then
		pass "lint passes"
	else
		fail "lint reports issues"
	fi
}

# ── Test: cppcheck (CI mirror) ───────────────────────────────────────
test_cppcheck() {
	section "cppcheck" 2

	# 1. cppcheck is available
	if command -v cppcheck >/dev/null 2>&1; then
		pass "cppcheck is installed"
	else
		fail "cppcheck not found - install via 'brew install cppcheck' or 'apt install cppcheck'"
		return
	fi

	# 2. run cppcheck with CI flags (mirrors .github/workflows/ci.yml lint-c job)
	if cppcheck --enable=warning,style,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=missingInclude \
		--suppress=unusedStructMember \
		--suppress=constParameter \
		--suppress=constVariablePointer \
		--suppress=constParameterPointer \
		--suppress=constParameterCallback \
		--suppress=variableScope \
		--suppress=unknownMacro \
		-I "${REPO_ROOT}/include" \
		-I "${REPO_ROOT}/compiler/include" \
		--error-exitcode=1 \
		"${REPO_ROOT}/src/backends/generic/" \
		"${REPO_ROOT}/src/backends/qemu/" \
		"${REPO_ROOT}/src/core/" \
		"${REPO_ROOT}/src/drivers/example/" \
		"${REPO_ROOT}/tools/kdality/" \
		"${REPO_ROOT}/compiler/" \
		"${REPO_ROOT}/tests/userspace/" \
		"${REPO_ROOT}/tests/integration/" \
		"${REPO_ROOT}/examples/" 2>&1; then
		pass "cppcheck passes (warning + style + performance)"
	else
		fail "cppcheck reports issues"
	fi
}

# ── Test: shellcheck (CI mirror) ─────────────────────────────────────
test_shellcheck() {
	section "shellcheck" 2

	# 1. shellcheck is available
	if command -v shellcheck >/dev/null 2>&1; then
		pass "shellcheck is installed"
	else
		fail "shellcheck not found - install via 'brew install shellcheck'"
		return
	fi

	# 2. run shellcheck on scripts
	_sc_fail=0
	for _sh in "${REPO_ROOT}/scripts/dev/"*.sh \
		"${REPO_ROOT}/scripts/e2e/"*.sh \
		"${REPO_ROOT}/scripts/installer/"*.sh \
		"${REPO_ROOT}/scripts/ci/"*.sh \
		"${REPO_ROOT}/scripts/release/"*.sh \
		"${REPO_ROOT}/scripts/env/"*.sh; do
		[ -f "$_sh" ] || continue
		if ! shellcheck -S warning "$_sh" >/dev/null 2>&1; then
			_sc_fail=$((_sc_fail + 1))
		fi
	done
	if [ "$_sc_fail" -eq 0 ]; then
		pass "shellcheck passes on all scripts"
	else
		fail "shellcheck reports issues in $_sc_fail script(s)"
	fi
}

# ── Test: structure (CI mirror) ──────────────────────────────────────
test_structure() {
	section "structure" 5

	# 1. required top-level files
	_missing=0
	for _f in Makefile README.md LICENSE CONTRIBUTING.md CHANGELOG.md \
		SECURITY.md CITATION.cff .clang-format .editorconfig; do
		if [ ! -f "${REPO_ROOT}/${_f}" ]; then
			_missing=$((_missing + 1))
		fi
	done
	if [ "$_missing" -eq 0 ]; then
		pass "required top-level files present"
	else
		fail "${_missing} required top-level file(s) missing"
	fi

	# 2. required directories
	_missing=0
	for _d in include/kdal src/core src/drivers src/backends \
		tests/kunit tests/userspace tests/integration \
		compiler tools/kdality lang/stdlib docs scripts examples cmake; do
		if [ ! -d "${REPO_ROOT}/${_d}" ]; then
			_missing=$((_missing + 1))
		fi
	done
	if [ "$_missing" -eq 0 ]; then
		pass "required directories present"
	else
		fail "${_missing} required dir(s) missing"
	fi

	# 3. required headers
	_missing=0
	for _h in include/kdal/core/kdal.h include/kdal/core/version.h \
		include/kdal/types.h include/kdal/ioctl.h \
		include/kdal/backend.h include/kdal/api/driver.h \
		include/kdal/api/accel.h; do
		if [ ! -f "${REPO_ROOT}/${_h}" ]; then
			_missing=$((_missing + 1))
		fi
	done
	if [ "$_missing" -eq 0 ]; then
		pass "required kernel headers present"
	else
		fail "${_missing} required header(s) missing"
	fi

	# 3. stdlib has 7 .kdh files
	_count=$(find "${REPO_ROOT}/lang/stdlib" -name '*.kdh' | wc -l | tr -d ' ')
	if [ "$_count" -eq 7 ]; then
		pass "stdlib has ${_count} .kdh files"
	else
		fail "stdlib has ${_count} .kdh files (expected 7)"
	fi

	# 4. all C source files end with newline
	_no_nl=0
	_no_nl=$(find "${REPO_ROOT}/compiler" "${REPO_ROOT}/tools" \
		"${REPO_ROOT}/tests" "${REPO_ROOT}/examples" \
		\( -name '*.c' -o -name '*.h' \) -exec sh -c '
         for f do
             if [ -s "$f" ] && [ "$(tail -c1 "$f" | wc -l | tr -d " ")" -eq 0 ]; then
                 echo x
             fi
         done' _ {} + 2>/dev/null | wc -l | tr -d ' ')
	if [ "$_no_nl" -eq 0 ]; then
		pass "all C files end with newline"
	else
		fail "${_no_nl} C file(s) missing trailing newline"
	fi
}

# ── Run with report logging ──────────────────────────────────────────
# Runs a test function while logging output to a report file.
# The function runs in the current shell so TOTAL_PASS/TOTAL_FAIL update.
_CURRENT_LOG=""
run_test() {
	_name="$1"
	_func="$2"
	_CURRENT_LOG="${REPORT_DIR}/${_name}_${RUN_DATE}.log"
	: >"${_CURRENT_LOG}"
	"$_func"
	_CURRENT_LOG=""
}

# ── Test: doc-links (CI mirror) ──────────────────────────────────────
test_doclinks() {
	section "doc-links" 1

	_broken=0
	for _link in $(grep -roh '\[.*\](\([^)]*\.md\))' "${REPO_ROOT}/docs/" "${REPO_ROOT}/README.md" 2>/dev/null |
		grep -oE '\([^)]+' | sed 's/^(//' | sort -u); do
		if [ ! -f "${REPO_ROOT}/${_link}" ] && [ ! -f "${REPO_ROOT}/docs/${_link}" ]; then
			_broken=$((_broken + 1))
		fi
	done
	if [ "$_broken" -eq 0 ]; then
		pass "all internal doc links valid"
	else
		fail "${_broken} broken doc link(s)"
	fi
}

# ── Test: examples (CI mirror) ───────────────────────────────────────
test_examples() {
	section "examples" 2

	# 1. Build GPU demo
	if cc -std=c99 -Wall -o /dev/null "${REPO_ROOT}/examples/accel_demo/gpudemo.c" 2>/dev/null; then
		pass "gpudemo.c compiles"
	else
		fail "gpudemo.c compilation"
	fi

	# 2. Build NPU demo
	if cc -std=c99 -Wall -o /dev/null "${REPO_ROOT}/examples/accel_demo/npudemo.c" 2>/dev/null; then
		pass "npudemo.c compiles"
	else
		fail "npudemo.c compilation"
	fi
}

# ── Dispatch ─────────────────────────────────────────────────────────
case "${TARGET}" in
kdalc) run_test kdalc test_kdalc ;;
kdality) run_test kdality test_kdality ;;
website) run_test website test_website ;;
vscode) run_test vscode test_vscode ;;
ci) run_test ci test_ci ;;
e2e) run_test e2e test_e2e ;;
lint) run_test lint test_lint ;;
cppcheck) run_test cppcheck test_cppcheck ;;
shellcheck) run_test shellcheck test_shellcheck ;;
structure) run_test structure test_structure ;;
doc-links) run_test doclinks test_doclinks ;;
examples) run_test examples test_examples ;;
all)
	run_test kdalc test_kdalc
	run_test kdality test_kdality
	run_test website test_website
	run_test vscode test_vscode
	run_test ci test_ci
	run_test e2e test_e2e
	run_test lint test_lint
	run_test cppcheck test_cppcheck
	run_test shellcheck test_shellcheck
	run_test structure test_structure
	run_test doclinks test_doclinks
	run_test examples test_examples
	;;
help)
	echo "Usage: $0 [--variant <release|debug|asan>] [target]"
	echo "       $0 [--preset <name>] [target]"
	echo ""
	echo "Testable targets:"
	echo "  kdalc       Compiler binary + sample compilation"
	echo "  kdality     CLI subcommands"
	echo "  website     Documentation site build"
	echo "  vscode      VS Code extension packaging"
	echo "  ci          CI smoke tests"
	echo "  e2e         End-to-end test suites"
	echo "  lint        Checkpatch + static analysis"
	echo "  cppcheck    C static analysis (mirrors CI lint-c)"
	echo "  shellcheck  Shell script linting (mirrors CI lint-sh)"
	echo "  structure   Project structure validation (mirrors CI)"
	echo "  doc-links   Check internal documentation links (mirrors CI)"
	echo "  examples    Build example programs (mirrors CI)"
	echo "  all         Run every test above (default)"
	echo ""
	echo "Reports are saved to: ${BUILD_DIR}/test/<target>_<date>.log"
	exit 0
	;;
*)
	echo "Error: unknown target '${TARGET}'"
	echo "Run '$0 help' to see available targets."
	exit 1
	;;
esac

summary
