#!/bin/sh
# Install host dependencies needed for KDAL development.
#
# Supports macOS (Homebrew) and Debian/Ubuntu (apt-get).
# Usage: ./scripts/env/dependencies.sh
#
# Minimum versions:
#   CMake  >= 3.20   (required by CMakePresets.json)
#   Node.js >= 18    (required by vsce, Astro website build)
#   GCC    >= 11     (C17 support)

set -eu

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }
warn() { printf "\033[1;33m==> %s\033[0m\n" "$1"; }
fail() {
	printf "\033[1;31mError: %s\033[0m\n" "$1"
	exit 1
}

# Use sudo when available and not already root
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
	if command -v sudo >/dev/null 2>&1; then
		SUDO="sudo"
	else
		fail "Not running as root and sudo is not installed."
	fi
fi

OS="$(uname -s)"

case "$OS" in
Darwin)
	info "Detected macOS - using Homebrew"
	command -v brew >/dev/null 2>&1 || fail "Homebrew not found. Install from https://brew.sh"

	info "Installing build tools..."
	brew install cmake node || true

	info "Installing QEMU and cross-compilation tools..."
	brew install qemu aarch64-elf-gcc dtc coreutils || true

	info "Installing code quality tools..."
	brew install cppcheck shellcheck || true

	info "Checking for Xcode command line tools..."
	xcode-select --print-path >/dev/null 2>&1 || {
		warn "Xcode CLT not found. Installing..."
		xcode-select --install
	}
	;;

Linux)
	info "Detected Linux - using apt-get"
	if ! command -v apt-get >/dev/null 2>&1; then
		fail "Only Debian/Ubuntu are supported. Install packages manually."
	fi

	info "Installing kernel build dependencies..."
	$SUDO apt-get update -qq
	$SUDO apt-get install -y --no-install-recommends \
		build-essential bc flex bison libelf-dev libssl-dev \
		gcc-aarch64-linux-gnu \
		qemu-system-arm \
		device-tree-compiler \
		cppcheck sparse coccinelle shellcheck \
		dwarves pahole \
		python3 python3-pip \
		curl ca-certificates gnupg

	# CMake - require >= 3.20 (Kitware APT repo for older distros)
	_need_cmake=false
	if command -v cmake >/dev/null 2>&1; then
		_cmake_ver="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+')"
		_cmake_major="${_cmake_ver%%.*}"
		_cmake_minor="${_cmake_ver#*.}"
		if [ "${_cmake_major}" -lt 3 ] || { [ "${_cmake_major}" -eq 3 ] && [ "${_cmake_minor}" -lt 20 ]; }; then
			warn "CMake ${_cmake_ver} is too old (need >= 3.20)"
			_need_cmake=true
		fi
	else
		_need_cmake=true
	fi

	if [ "${_need_cmake}" = true ]; then
		info "Installing CMake >= 3.20 from Kitware APT repository..."
		$SUDO apt-get install -y --no-install-recommends cmake || true
		# Re-check after install
		if command -v cmake >/dev/null 2>&1; then
			_cmake_ver="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+')"
			info "CMake ${_cmake_ver} installed"
		else
			warn "CMake installation failed - install manually: https://cmake.org/download/"
		fi
	fi

	# Node.js - require >= 18 (NodeSource for older distros)
	_need_node=false
	if command -v node >/dev/null 2>&1; then
		_node_major="$(node --version | grep -oE '[0-9]+' | head -1)"
		if [ "${_node_major}" -lt 18 ]; then
			warn "Node.js v${_node_major} is too old (need >= 18)"
			_need_node=true
		fi
	else
		_need_node=true
	fi

	if [ "${_need_node}" = true ]; then
		info "Installing Node.js 20.x from NodeSource..."
		curl -fsSL https://deb.nodesource.com/setup_20.x | $SUDO bash - 2>/dev/null
		$SUDO apt-get install -y --no-install-recommends nodejs
	fi

	info "Installing optional tools..."
	$SUDO apt-get install -y --no-install-recommends \
		bear clang-format || true
	;;

*)
	fail "Unsupported OS: $OS"
	;;
esac

# ── Verify required tools ────────────────────────────────────────────
info "Verifying installed tools..."
_ok=0
_fail=0
for tool in cmake gcc make node npm cppcheck shellcheck \
	qemu-system-aarch64 aarch64-linux-gnu-gcc; do
	if command -v "$tool" >/dev/null 2>&1; then
		_ver=""
		case "$tool" in
		cmake) _ver="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')" ;;
		gcc) _ver="$(gcc --version | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')" ;;
		node) _ver="$(node --version)" ;;
		npm) _ver="$(npm --version 2>/dev/null)" ;;
		*) _ver="ok" ;;
		esac
		printf "  ✓ %-28s %s\n" "$tool" "${_ver}"
		_ok=$((_ok + 1))
	else
		printf "  ✗ %-28s missing\n" "$tool"
		_fail=$((_fail + 1))
	fi
done

echo ""
if [ "${_fail}" -eq 0 ]; then
	info "All ${_ok} tools installed successfully."
else
	warn "${_fail} tool(s) missing. Some build targets may fail."
fi

info "Next step: ./scripts/env/prepare.sh (set up QEMU environment)"
