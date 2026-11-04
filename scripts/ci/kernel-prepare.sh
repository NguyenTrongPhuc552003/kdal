#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Prepare the pinned arm64 kernel that KDAL's KUnit run builds against.
#
# The kernel itself (Image, .config, Module.symvers) is built once by ELMOS
# (Linux v6.18, arm64 defconfig + CONFIG_KUNIT=y, LLVM) and published as the
# release "ci-kernel-v6.18-kunit". This script downloads it, checks the
# hashes, fetches the matching v6.18 source and runs modules_prepare, so a
# small CI runner never compiles the kernel.
#
# Usage: scripts/ci/kernel-prepare.sh [WORKDIR]   (default: build/ci-kernel)
# Prints the KDIR and IMAGE paths on its last two lines.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WORK=${1:-$ROOT/build/ci-kernel}
TAG=ci-kernel-v6.18-kunit
BASE=${KDAL_CI_KERNEL_URL:-https://github.com/NguyenTrongPhuc552003/kdal/releases/download/$TAG}
SRC_URL=${KDAL_CI_KERNEL_SRC:-https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.18.tar.xz}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
cd "$WORK"

MIRROR=https://codeberg.org/NguyenTrongPhuc552003/kdal/releases/download/$TAG
for f in SHA256SUMS Image.gz config Module.symvers kernel.release; do
	# GitHub first; the Codeberg copy of the release if it is not there yet
	[ -s "$f" ] || curl -fsSL -o "$f" "$BASE/$f" || curl -fsSL -o "$f" "$MIRROR/$f"
done
sha256sum -c SHA256SUMS

if [ ! -d linux-6.18 ]; then
	curl -fsSL "$SRC_URL" | tar -xJ
fi
KDIR=$WORK/linux-6.18
cp config "$KDIR/.config"
# The release string must match the Image (it carries ELMOS's "-dirty"
# from its macOS host patch); LOCALVERSION reproduces it on a clean tree.
REL=$(cat kernel.release)
LV=${REL#6.18.0}
make -C "$KDIR" ARCH=arm64 LLVM=1 LOCALVERSION="$LV" olddefconfig >/dev/null
make -C "$KDIR" ARCH=arm64 LLVM=1 LOCALVERSION="$LV" modules_prepare >/dev/null
cp Module.symvers "$KDIR/Module.symvers"
got=$(cat "$KDIR/include/config/kernel.release")
if [ "$got" != "$REL" ]; then
	echo "kernel release $got does not match the Image ($REL)" >&2
	exit 1
fi
[ -f Image ] || gunzip -k Image.gz
echo "$KDIR"
echo "$WORK/Image"
