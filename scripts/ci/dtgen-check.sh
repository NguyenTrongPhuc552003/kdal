#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# kdality dt-gen on both .kdh syntaxes: the full one
# (examples/kdc_hello/uart_hello.kdh) and the simplified one
# (tests/dtgen/led.kdh, from docs/language_guide.md). Each must give
# <Name>.dtso with its compatible string, every register listed and the
# reg size it prints; dtc, when installed, must compile it. A missing
# output directory and a file that does not parse must fail with a message.
# Run by scripts/ci/test.sh (CI) and scripts/e2e/test_compiler.sh; rows T1
# to T4 and T6 of HAVEN's docs/methodology/stage2/20261114-kdal-dtgen.
#
# Usage: dtgen-check.sh KDALITY

set -eu

KDALITY=$1
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
FAILS=0

bad() {
	echo "FAIL: $*"
	FAILS=$((FAILS + 1))
}

# check FILE.kdh NAME COMPATIBLE NREGS
check() {
	before=$FAILS
	if ! "$KDALITY" dt-gen "$1" -o "$OUT" >"$OUT/$2.out" 2>&1; then
		bad "dt-gen $(basename "$1"): $(cat "$OUT/$2.out")"
		return 0
	fi
	f="$OUT/$2.dtso"
	if [ ! -f "$f" ]; then
		bad "$(basename "$1"): no $2.dtso"
		return 0
	fi
	grep -q "compatible = \"$3\";" "$f" || bad "$2.dtso: no compatible \"$3\""
	n=$(grep -c '^ *\*   [A-Za-z_0-9]* @ 0x' "$f" || true)
	[ "$n" = "$4" ] || bad "$2.dtso: $n registers listed, expected $4"
	size=$(sed -n 's/.*reg = <0x0 0x[0-9a-f]* 0x0 \(0x[0-9a-f]*\)>;.*/\1/p' "$f")
	grep -q "reg size: *$size\$" "$OUT/$2.out" ||
		bad "$2: the printed reg size differs from the file's ($size)"
	if command -v dtc >/dev/null 2>&1; then
		dtc -q -@ -I dts -O dtb -o "$OUT/$2.dtbo" "$f" || bad "dtc rejects $2.dtso"
	else
		echo "note: dtc not installed; $2.dtso not compiled"
	fi
	[ "$FAILS" -ne "$before" ] || echo "ok: $(basename "$1") -> $2.dtso"
}

check "$ROOT/examples/kdc_hello/uart_hello.kdh" UartHello "kdal,UartHello" 6
check "$ROOT/tests/dtgen/led.kdh" led "example,led-gpio" 2

# a missing output directory: an error that names it
if "$KDALITY" dt-gen "$ROOT/tests/dtgen/led.kdh" -o "$OUT/absent" >"$OUT/dir.out" 2>&1; then
	bad "dt-gen accepted a missing output directory"
elif ! grep -q "absent" "$OUT/dir.out"; then
	bad "the missing-directory message does not name it: $(cat "$OUT/dir.out")"
else
	echo "ok: a missing output directory is refused"
fi

# a file that does not parse: an error, no output file
printf 'device ../x {\n}\n' >"$OUT/bad.kdh"
if "$KDALITY" dt-gen "$OUT/bad.kdh" -o "$OUT" >"$OUT/bad.out" 2>&1; then
	bad "dt-gen accepted a file that does not parse"
elif [ ! -s "$OUT/bad.out" ]; then
	bad "dt-gen refused a bad file without a message"
else
	echo "ok: a file that does not parse is refused"
fi

if [ "$FAILS" -ne 0 ]; then
	echo "dt-gen check: $FAILS failed"
	exit 1
fi
echo "dt-gen check: all passed"
