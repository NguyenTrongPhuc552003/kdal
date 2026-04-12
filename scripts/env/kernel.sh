#!/bin/sh
# Build Linux 6.6 LTS kernel for the QEMU aarch64 virt machine.
#
# Cross-compiles from macOS or native-builds on Linux, producing a
# kernel Image suitable for QEMU -kernel boot.
#
# Usage: ./scripts/env/kernel.sh [work_dir]

set -eu

WORK_DIR="${1:-$HOME/kdal-qemu}"
KERNEL_VER="6.6.80"
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${KERNEL_VER}.tar.xz"
KERNEL_DIR="$WORK_DIR/kernel/linux-${KERNEL_VER}"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

# Detect cross-compiler
if [ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]; then
	CROSS=""
	ARCH="arm64"
else
	# Try common cross-compiler prefixes
	if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
		CROSS="aarch64-linux-gnu-"
	elif command -v aarch64-elf-gcc >/dev/null 2>&1; then
		CROSS="aarch64-elf-"
	else
		echo "Error: no aarch64 cross-compiler found" >&2
		echo "Install with: brew install aarch64-elf-gcc (macOS)" >&2
		echo "  or: apt install gcc-aarch64-linux-gnu (Linux)" >&2
		exit 1
	fi
	ARCH="arm64"
fi

# Download kernel source
if [ ! -d "$KERNEL_DIR" ]; then
	info "Downloading Linux $KERNEL_VER..."
	mkdir -p "$WORK_DIR/kernel"
	curl -L "$KERNEL_URL" | tar -xJ -C "$WORK_DIR/kernel"
fi

info "Building kernel (ARCH=$ARCH, CROSS=$CROSS, -j$NPROC)..."
cd "$KERNEL_DIR"

# Configure for QEMU virt machine
if [ ! -f .config ]; then
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS" defconfig
	# Enable KUnit for in-kernel testing
	scripts/config --enable CONFIG_KUNIT
	scripts/config --enable CONFIG_KUNIT_ALL_TESTS
	scripts/config --enable CONFIG_MODULES
	scripts/config --enable CONFIG_MODULE_UNLOAD
	scripts/config --enable CONFIG_DEBUG_FS
	scripts/config --enable CONFIG_VIRTIO
	scripts/config --enable CONFIG_VIRTIO_MMIO
	scripts/config --enable CONFIG_9P_FS
	scripts/config --enable CONFIG_9P_FS_POSIX_ACL
	scripts/config --enable CONFIG_NET_9P
	scripts/config --enable CONFIG_NET_9P_VIRTIO
	make ARCH="$ARCH" CROSS_COMPILE="$CROSS" olddefconfig
fi

make ARCH="$ARCH" CROSS_COMPILE="$CROSS" -j"$NPROC" Image modules

info "Kernel built successfully."
info "  Image: $KERNEL_DIR/arch/arm64/boot/Image"
info "  Build dir for KDAL module: KDIR=$KERNEL_DIR"
info ""
info "Build KDAL module with:"
info "  make KDIR=$KERNEL_DIR ARCH=$ARCH CROSS_COMPILE=$CROSS module"
