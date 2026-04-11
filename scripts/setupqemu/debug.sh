#!/bin/sh
# Launch QEMU in debug mode with GDB server for KDAL kernel debugging.
#
# Starts QEMU with -S (frozen at start) and -s (GDB on :1234).
# Attach with: aarch64-linux-gnu-gdb vmlinux -ex "target remote :1234"
#
# Usage: ./scripts/setupqemu/debug.sh [work_dir]

set -eu

WORK_DIR="${1:-$HOME/kdal-qemu}"
KERNEL_VER="6.6.80"
KERNEL_DIR="$WORK_DIR/kernel/linux-${KERNEL_VER}"
IMAGE="$WORK_DIR/images/guest-rootfs.qcow2"
SHARED="$WORK_DIR/shared"

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

for f in "$KERNEL_DIR/arch/arm64/boot/Image" "$IMAGE"; do
    if [ ! -f "$f" ]; then
        echo "Error: $f not found" >&2
        exit 1
    fi
done

info "Starting QEMU in debug mode (frozen, GDB on :1234)..."
info "Attach with:"
info "  aarch64-linux-gnu-gdb $KERNEL_DIR/vmlinux \\"
info "    -ex 'target remote :1234'"
info ""
info "Type 'c' in GDB to continue boot."

exec qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a72 \
    -smp 4 \
    -m 2048 \
    -kernel "$KERNEL_DIR/arch/arm64/boot/Image" \
    -append "root=/dev/vda1 console=ttyAMA0 nokaslr" \
    -drive file="$IMAGE",format=qcow2,if=virtio \
    -fsdev local,id=kdal-fsdev,path="$SHARED",security_model=mapped-xattr \
    -device virtio-9p-device,fsdev=kdal-fsdev,mount_tag=kdal-share \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-device,netdev=net0 \
    -nographic \
    -S -s
