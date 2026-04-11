#!/bin/sh
# Install host dependencies needed for KDAL development.
#
# Supports macOS (Homebrew) and Debian/Ubuntu (apt-get).
# Usage: ./scripts/devsetup/install_deps.sh

set -eu

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }
warn() { printf "\033[1;33m==> %s\033[0m\n" "$1"; }
fail() { printf "\033[1;31mError: %s\033[0m\n" "$1"; exit 1; }

OS="$(uname -s)"

case "$OS" in
Darwin)
    info "Detected macOS — using Homebrew"
    command -v brew >/dev/null 2>&1 || fail "Homebrew not found. Install from https://brew.sh"

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
    info "Detected Linux — using apt-get"
    if ! command -v apt-get >/dev/null 2>&1; then
        fail "Only Debian/Ubuntu are supported. Install packages manually."
    fi

    info "Installing kernel build dependencies..."
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        build-essential bc flex bison libelf-dev libssl-dev \
        gcc-aarch64-linux-gnu \
        qemu-system-arm \
        device-tree-compiler \
        cppcheck sparse coccinelle shellcheck \
        dwarves pahole \
        python3 python3-pip

    info "Installing optional tools..."
    sudo apt-get install -y --no-install-recommends \
        bear clang-format || true
    ;;

*)
    fail "Unsupported OS: $OS"
    ;;
esac

info "Verifying key tools..."
for tool in qemu-system-aarch64 make gcc; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf "  ✓ %s (%s)\n" "$tool" "$(command -v "$tool")"
    else
        warn "  ✗ $tool not found"
    fi
done

info "Done. Run 'scripts/setupqemu/prepare.sh' to set up the QEMU environment."
