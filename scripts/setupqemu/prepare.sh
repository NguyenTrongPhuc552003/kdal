#!/bin/sh
# Prepare the QEMU aarch64 guest environment for KDAL development.
#
# Downloads a minimal Debian/Ubuntu cloud image (or creates a rootfs)
# and prepares the shared directory for module deployment.
#
# Usage: ./scripts/setupqemu/prepare.sh [work_dir]

set -eu

WORK_DIR="${1:-$HOME/kdal-qemu}"
KERNEL_VER="6.6"
IMAGE_URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-arm64.img"
IMAGE_NAME="guest-rootfs.qcow2"

info() { printf "\033[1;34m==> %s\033[0m\n" "$1"; }

info "Preparing KDAL QEMU workspace at $WORK_DIR"
mkdir -p "$WORK_DIR"/{shared,kernel,images}

# Download cloud image if not present
if [ ! -f "$WORK_DIR/images/$IMAGE_NAME" ]; then
    info "Downloading Ubuntu cloud image for aarch64..."
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$WORK_DIR/images/cloud-base.img" "$IMAGE_URL"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$WORK_DIR/images/cloud-base.img" "$IMAGE_URL"
    else
        echo "Error: curl or wget required" >&2
        exit 1
    fi

    info "Creating writable overlay..."
    qemu-img create -f qcow2 -b "$WORK_DIR/images/cloud-base.img" \
        -F qcow2 "$WORK_DIR/images/$IMAGE_NAME" 8G
else
    info "Guest image already exists, skipping download."
fi

# Create cloud-init metadata for login
if [ ! -f "$WORK_DIR/images/seed.img" ]; then
    info "Creating cloud-init seed image..."
    TMPDIR=$(mktemp -d)
    cat > "$TMPDIR/user-data" <<'EOF'
#cloud-config
password: kdal
chpasswd: { expire: false }
ssh_pwauth: true
EOF
    cat > "$TMPDIR/meta-data" <<EOF
instance-id: kdal-dev
local-hostname: kdal-qemu
EOF

    if command -v cloud-localds >/dev/null 2>&1; then
        cloud-localds "$WORK_DIR/images/seed.img" "$TMPDIR/user-data" "$TMPDIR/meta-data"
    else
        info "cloud-localds not found — seed.img skipped (use manual login)"
    fi
    rm -rf "$TMPDIR"
fi

# Create shared directory layout
mkdir -p "$WORK_DIR/shared/modules"
info "Place compiled kdal.ko in $WORK_DIR/shared/modules/"

info "Preparation complete."
info "  Workspace: $WORK_DIR"
info "  Next step: ./scripts/setupqemu/buildkernel.sh $WORK_DIR"
