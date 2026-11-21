#!/usr/bin/env python3
"""KDAL's demand report from the KUnit run's console log (kunit-qemu.sh).

docs/open-findings-20261121.md, finding F3: the init program's own lines
and the kernel's (printed later, by the console's printing thread) can
interleave on the UART. The init program now writes each report line to
/dev/kmsg with the tag "KDAL-DEMAND| ", so the lines keep the kernel log's
order and are never split; those lines are taken, tag and timestamp
removed. An older log (the report written to the console between
KDAL-DEMAND-BEGIN and KDAL-DEMAND-END) is still read: the lines between
the markers that are kernel log lines ("[   0.123456] ...") are dropped.

Usage: demand-extract.py CONSOLE_LOG OUT_JSON
"""

import json
import re
import sys

TAG = "KDAL-DEMAND| "
KERNEL_LINE = re.compile(r"^\[ *\d+\.\d+\] ")


def extract(lines):
    tagged = [ln.split(TAG, 1)[1] for ln in lines if TAG in ln]
    if tagged:
        return [ln for ln in tagged if ln not in ("KDAL-DEMAND-BEGIN", "KDAL-DEMAND-END")]
    out, inside = [], False
    for ln in lines:
        if ln == "KDAL-DEMAND-BEGIN":
            inside = True
        elif ln == "KDAL-DEMAND-END":
            break
        elif inside and not KERNEL_LINE.match(ln):
            out.append(ln)
    return out


def main(argv):
    with open(argv[1], errors="replace") as f:
        lines = [ln.rstrip("\r\n") for ln in f]
    text = "\n".join(extract(lines)) + "\n"
    json.loads(text)  # a report that is not JSON fails here, not later
    with open(argv[2], "w") as f:
        f.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
