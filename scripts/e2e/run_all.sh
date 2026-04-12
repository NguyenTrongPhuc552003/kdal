#!/bin/sh
# E2E Master Runner - runs all end-to-end test suites.
#
# Usage:
#   ./scripts/e2e/run_all.sh              Run all suites
#   ./scripts/e2e/run_all.sh --quick      Skip slow tests (website build)
#   ./scripts/e2e/run_all.sh <suite>      Run a single suite
#
# Available suites:
#   developer-journey  user-journey  compiler
#   editor-support     packaging     website    release
#
# Exit code: 0 if all suites pass, 1 if any suite fails.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

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

# ── Configuration ─────────────────────────────────────────────────────

# Default suite order (fast → slow)
ALL_SUITES="developer-journey compiler packaging editor-support user-journey release website"

# Quick mode skips website (requires npm install + build)
QUICK_SUITES="developer-journey compiler packaging editor-support user-journey release"

SUITES="$ALL_SUITES"
QUICK=false

# Parse args
for arg in "$@"; do
	case "$arg" in
	--quick | -q)
		QUICK=true
		SUITES="$QUICK_SUITES"
		;;
	--help | -h)
		echo "Usage: $0 [--quick] [suite-name...]"
		echo ""
		echo "Suites: $ALL_SUITES"
		echo ""
		echo "Options:"
		echo "  --quick, -q    Skip slow tests (website build)"
		echo "  --help, -h     Show this help"
		exit 0
		;;
	-*)
		echo "Unknown option: $arg" >&2
		exit 1
		;;
	*)
		# Specific suite
		SUITES="$arg"
		;;
	esac
done

# ── Run suites ────────────────────────────────────────────────────────

TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_SKIP=0
SUITE_RESULTS=""
FAILED_SUITES=""

printf "${C_BOLD}╔══════════════════════════════════════════════════╗${C_RESET}\n"
printf "${C_BOLD}║         KDAL End-to-End Test Runner              ║${C_RESET}\n"
printf "${C_BOLD}╚══════════════════════════════════════════════════╝${C_RESET}\n\n"

if [ "$QUICK" = true ]; then
	printf "${C_YELLOW}==> Quick mode: skipping slow suites${C_RESET}\n\n"
fi

for suite in $SUITES; do
	script="$SCRIPT_DIR/test_${suite}.sh"

	# Normalize suite name (convert hyphens to underscores for filename)
	script_underscore="$SCRIPT_DIR/test_$(echo "$suite" | tr '-' '_').sh"

	if [ -f "$script" ]; then
		_script="$script"
	elif [ -f "$script_underscore" ]; then
		_script="$script_underscore"
	else
		printf "${C_YELLOW}  SKIP: suite '%s' not found${C_RESET}\n" "$suite"
		TOTAL_SKIP=$((TOTAL_SKIP + 1))
		SUITE_RESULTS="${SUITE_RESULTS}\n  ${C_YELLOW}SKIP${C_RESET}  $suite (not found)"
		continue
	fi

	printf "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RESET}\n"
	printf "${C_BOLD}Running: %s${C_RESET}\n" "$suite"
	printf "${C_BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${C_RESET}\n"

	if sh "$_script"; then
		TOTAL_PASS=$((TOTAL_PASS + 1))
		SUITE_RESULTS="${SUITE_RESULTS}\n  ${C_GREEN}PASS${C_RESET}  $suite"
	else
		TOTAL_FAIL=$((TOTAL_FAIL + 1))
		FAILED_SUITES="${FAILED_SUITES} $suite"
		SUITE_RESULTS="${SUITE_RESULTS}\n  ${C_RED}FAIL${C_RESET}  $suite"
	fi

	echo ""
done

# ── Summary ───────────────────────────────────────────────────────────

_total=$((TOTAL_PASS + TOTAL_FAIL + TOTAL_SKIP))

printf "${C_BOLD}╔══════════════════════════════════════════════════╗${C_RESET}\n"
printf "${C_BOLD}║              E2E Test Summary                    ║${C_RESET}\n"
printf "${C_BOLD}╠══════════════════════════════════════════════════╣${C_RESET}\n"
printf "${SUITE_RESULTS}\n"
printf "${C_BOLD}╠══════════════════════════════════════════════════╣${C_RESET}\n"
printf "${C_BOLD}║  Total: %d suites | ${C_GREEN}%d passed${C_BOLD} | ${C_RED}%d failed${C_BOLD} | ${C_YELLOW}%d skipped${C_BOLD}  ║${C_RESET}\n" \
	"$_total" "$TOTAL_PASS" "$TOTAL_FAIL" "$TOTAL_SKIP"
printf "${C_BOLD}╚══════════════════════════════════════════════════╝${C_RESET}\n"

if [ "$TOTAL_FAIL" -gt 0 ]; then
	printf "\n${C_RED}Failed suites:%s${C_RESET}\n" "$FAILED_SUITES"
	exit 1
fi

printf "\n${C_GREEN}All E2E test suites passed!${C_RESET}\n"
exit 0
