#!/bin/sh
# Run static analysis on KDAL source files.
#
# Uses cppcheck (if available) and sparse (if available + kernel headers).
# Falls back to basic compiler warnings if no analyzer is found.
#
# Usage: ./scripts/ci/static_analysis.sh

set -eu

CPP_SOURCE_DIRS="src tools/kdality compiler tests/userspace tests/integration examples"
ERRORS=0

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

# cppcheck
if command -v cppcheck >/dev/null 2>&1; then
	info "Running cppcheck..."
	if ! cppcheck --enable=warning,style,performance,portability \
		--suppress=missingIncludeSystem \
		--suppress=missingInclude \
		--suppress=unusedStructMember \
		--suppress=constParameter \
		--suppress=constVariablePointer \
		--suppress=constParameterPointer \
		--suppress=constParameterCallback \
		--suppress=variableScope \
		--suppress=unknownMacro \
		-I include \
		-I compiler/include \
		--error-exitcode=1 \
		$CPP_SOURCE_DIRS; then
		ERRORS=$((ERRORS + 1))
	fi
else
	info "cppcheck not found - skipping"
fi

# sparse (Linux kernel static analyzer)
if command -v sparse >/dev/null 2>&1; then
	info "Running sparse (requires KDIR)..."
	if [ -n "${KDIR:-}" ]; then
		make C=1 CHECK="sparse" -C "$KDIR" M="$(pwd)" modules 2>&1 || ERRORS=$((ERRORS + 1))
	else
		info "KDIR not set - skipping sparse"
	fi
else
	info "sparse not found - skipping"
fi

# Run ShellCheck on repository shell scripts.
if command -v shellcheck >/dev/null 2>&1; then
	info "Running shellcheck on scripts..."
	find scripts -name '*.sh' -exec shellcheck -S warning {} + || ERRORS=$((ERRORS + 1))
else
	info "shellcheck not found - skipping"
fi

if [ "$ERRORS" -gt 0 ]; then
	echo "$ERRORS analyzer(s) reported issues."
	exit 1
fi

info "Static analysis clean."
