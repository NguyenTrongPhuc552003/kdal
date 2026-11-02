#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Write a newc initramfs for the KUnit QEMU run.

Usage: mkcpio.py OUT.cpio INIT KDAL_KO

Contents: /init (mode 0755), /kdal.ko, /dev/console (char 5:1), all owned
by root. Written directly, so no root, fakeroot or cpio tool is needed.
"""
import sys


def entry(out, ino, name, mode, data=b"", rdev=(0, 0)):
    name_b = name.encode() + b"\0"
    fields = [ino, mode, 0, 0, 1, 0, len(data), 0, 0, rdev[0], rdev[1],
              len(name_b), 0]
    hdr = b"070701" + b"".join(b"%08X" % f for f in fields)
    out.write(hdr + name_b)
    out.write(b"\0" * ((4 - (len(hdr) + len(name_b)) % 4) % 4))
    out.write(data)
    out.write(b"\0" * ((4 - len(data) % 4) % 4))


def main():
    out_path, init_path, ko_path = sys.argv[1:4]
    with open(out_path, "wb") as out:
        entry(out, 1, "dev", 0o040755)
        entry(out, 2, "dev/console", 0o020600, rdev=(5, 1))
        entry(out, 3, "init", 0o100755, open(init_path, "rb").read())
        entry(out, 4, "kdal.ko", 0o100644, open(ko_path, "rb").read())
        entry(out, 0, "TRAILER!!!", 0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
