#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Do the kernel modules kdality compiles from the examples build against the
# pinned kernel tree? (docs/open-findings-20261121.md, finding F4, row O7)
#
# Usage: scripts/ci/codegen-check.sh KDALITY KDIR [OUTDIR]
# For every examples/kdc_hello/*.kdc: kdality compile, then the module's
# Kbuild against KDIR (arm64, LLVM), with KDAL's include/ on the path.
# Prints one line per example and the compiler's first errors; exit 0 only
# if every example builds.
set -uo pipefail

KDALITY=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
KDIR=$(cd "$2" && pwd)
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${3:-$ROOT/build/codegen}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd) # absolute: kdality runs in each example's directory
fail=0
for kdc in "$ROOT"/examples/kdc_hello/*.kdc; do
	name=$(basename "$kdc" .kdc)
	dir="$OUT/$name"
	rm -rf "$dir"
	if ! (cd "$(dirname "$kdc")" && "$KDALITY" compile -o "$dir" "$(basename "$kdc")") >"$OUT/$name.compile.log" 2>&1; then
		echo "$name: kdality compile FAIL"
		tail -5 "$OUT/$name.compile.log"
		fail=1
		continue
	fi
	cp "$dir/Makefile.kbuild" "$dir/Kbuild"
	if make -C "$KDIR" M="$dir" ARCH=arm64 LLVM=1 ccflags-y="-I$ROOT/include" modules >"$OUT/$name.kbuild.log" 2>&1; then
		for ko in "$dir"/*.ko; do echo "$name: module builds (${ko##*/})"; done
	else
		echo "$name: module build FAIL"
		grep -E "error:" "$OUT/$name.kbuild.log" | head -8
		echo "  ($(grep -c "error:" "$OUT/$name.kbuild.log") errors)"
		fail=1
	fi
done
exit "$fail"
