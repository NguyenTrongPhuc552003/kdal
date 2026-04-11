#!/bin/sh
# Launch QEMU aarch64 virt machine for KDAL development.
#
# Boots the cross-compiled kernel with the cloud image and mounts
# a 9p shared directory so the KDAL module can be loaded without
# rebuilding the image.
#
# Usage: ./scripts/setupqemu/run.sh [work_dir]

set -eu

WORK_DIR="${1:-$HOME/kdal-qemu}"
KERNEL_VER="6.6.80"
KERNEL_DIR="$WORK_DIR/kernel/linux-${KERNEL_VER}"
IMAGE="$WORK_DIR/images/guest-rootfs.qcow2"
SEED="$WORK_DIR/images/seed.img"
SHARED="$WORK_DIR/shared"

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

# Validate prerequisites
for f in "$KERNEL_DIR/arch/arm64/boot/Image" "$IMAGE"; do
    if [ ! -f "$f" ]; then
        echo "Error: $f not found" >&2
        echo "Run prepare.sh and buildkernel.sh first." >&2
        exit 1
    fi
done

SEED_ARGS=""
if [ -f "$SEED" ]; then
    SEED_ARGS="-drive file=$SEED,format=raw,if=virtio"
fi

info "Launching QEMU aarch64 virt machine..."
info "  Login: ubuntu / kdal"
info "  Shared dir inside guest: mount -t 9p -o trans=virtio kdal-share /mnt"
info "  Then: insmod /mnt/modules/kdal.ko"
info ""
info "Press Ctrl-A X to exit QEMU."

exec qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a72 \
    -smp 4 \
    -m 2048 \
    -kernel "$KERNEL_DIR/arch/arm64/boot/Image" \
    -append "root=/dev/vda1 console=ttyAMA0 nokaslr" \
    -drive file="$IMAGE",format=qcow2,if=virtio \
    $SEED_ARGS \
    -fsdev local,id=kdal-fsdev,path="$SHARED",security_model=mapped-xattr \
    -device virtio-9p-device,fsdev=kdal-fsdev,mount_tag=kdal-share \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-device,netdev=net0 \
    -nographic
