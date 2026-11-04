#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
#
# Build kdal.ko with its KUnit suites, boot it in QEMU (arm64 virt) and
# check the KTAP results.
#
# Usage: scripts/ci/kunit-qemu.sh KDIR IMAGE [OUTDIR]
#   KDIR   prepared arm64 kernel tree (modules_prepare done, Module.symvers)
#   IMAGE  arm64 kernel Image built from the same tree, CONFIG_KUNIT=y
#   OUTDIR where the log and summary go (default: build/kunit)
#
# Needs clang, ld.lld, GNU make 4+, python3, dtc and qemu-system-aarch64.
# The guest device tree gets one "kdal,device" node over QEMU's PL061 GPIO
# (MMIO 0x9030000, SPI 7 = INTID 39), so the run also checks the resource
# demand KDAL reports for it (debugfs "demand", FF-A vocabulary).
# Exit 0 only if every expected suite ran and no case failed.
set -euo pipefail

KDIR=$(cd "$1" && pwd)
IMAGE=$2
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${3:-$ROOT/build/kunit}
SUITES="kdal-registry kdal-event kdal-core kdal-driver kdal-accel kdal-demand"
mkdir -p "$OUT"

echo "== build kdal.ko (KDAL_KUNIT=y) against $KDIR"
make -C "$KDIR" M="$ROOT" ARCH=arm64 LLVM=1 KDAL_KUNIT=y modules >"$OUT/build.log" 2>&1 ||
	{ tail -30 "$OUT/build.log"; exit 1; }

echo "== build /init and the initramfs"
clang --target=aarch64-linux-gnu -ffreestanding -nostdlib -static -O2 \
	-fno-stack-protector -fuse-ld=lld -Wl,-e,_start \
	-o "$OUT/init" "$ROOT/scripts/ci/kunit-init.c"
python3 "$ROOT/scripts/ci/mkcpio.py" "$OUT/initramfs.cpio" "$OUT/init" "$ROOT/kdal.ko"

echo "== device tree with a kdal,device node"
QEMU_MACHINE=(-M virt -cpu cortex-a72 -smp 2 -m 512 -nographic -no-reboot -nic none)
qemu-system-aarch64 "${QEMU_MACHINE[@]}" -machine dumpdtb="$OUT/virt.dtb" >/dev/null 2>&1
dtc -q -I dtb -O dts -o "$OUT/virt.dts" "$OUT/virt.dtb"
python3 - "$OUT/virt.dts" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read().rstrip()
assert s.endswith("};")
node = """
	kdal-gpio@9030000 {
		compatible = "kdal,device";
		reg = <0x00 0x9030000 0x00 0x1000>;
		interrupts = <0x00 0x07 0x04>;
	};
"""
open(p, "w").write(s[:-2] + node + "};\n")
PY
dtc -q -I dts -O dtb -o "$OUT/kdal.dtb" "$OUT/virt.dts"

echo "== boot in QEMU"
timeout 300 qemu-system-aarch64 "${QEMU_MACHINE[@]}" -dtb "$OUT/kdal.dtb" \
	-kernel "$IMAGE" -initrd "$OUT/initramfs.cpio" \
	-append "console=ttyAMA0 panic=-1 rdinit=/init" >"$OUT/console.raw" 2>&1 || true
tr -d '\r' <"$OUT/console.raw" >"$OUT/console.log"

echo "== results"
fail=0
for s in $SUITES; do
	# KTAP: "    # Subtest: <suite>" ... "# Totals: pass:N fail:N skip:N total:N"
	totals=$(awk -v s="$s" '
		$0 ~ "# Subtest: "s"$" { in_s = 1 }
		in_s && /# Totals:/ { sub(/.*# Totals: /, ""); print; exit }' "$OUT/console.log")
	if [ -z "$totals" ]; then
		echo "MISSING $s"
		fail=1
		continue
	fi
	echo "$s: $totals"
	case "$totals" in
	*"fail:0 "*) ;;
	*) fail=1 ;;
	esac
done
sed -n '/^KDAL-DEMAND-BEGIN$/,/^KDAL-DEMAND-END$/p' "$OUT/console.log" | sed '1d;$d' >"$OUT/demand.json"
if python3 - "$OUT/demand.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
regs = [r for r in d["device-regions"] if r["description"].startswith("kdal-gpio")]
assert regs, d
r = regs[0]
assert r["base-address"] == "0x9030000" and r["pages-count"] == 1, r
assert r["interrupts"] == [{"id": 39}], r
print("demand: kdal-gpio@9030000 reports MMIO 0x9030000 (1 page) and INTID 39")
PY
then :; else echo "demand: FAIL ($OUT/demand.json)"; fail=1; fi
grep -E "not ok [0-9]+ test_" "$OUT/console.log" || true
grep -E "Oops|BUG:|KDAL-CI:" "$OUT/console.log" || true
{
	echo "kernel: $(set +o pipefail; strings "$IMAGE" 2>/dev/null | grep -m1 '^Linux version')"
	for s in $SUITES; do
		awk -v s="$s" '$0 ~ "# Subtest: "s"$" {f=1} f && /# Totals:/ {sub(/.*# Totals: /,""); print s": "$0; exit}' "$OUT/console.log"
	done
} >"$OUT/summary.txt"
if [ "$fail" -ne 0 ]; then
	echo "KUnit: FAIL (log: $OUT/console.log)"
	exit 1
fi
echo "KUnit: PASS"
