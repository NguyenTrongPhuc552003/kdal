#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Write a release tarball whose entries carry nothing of the machine that
built it (docs/release-workflow.md): entries sorted, uid and gid 0, no
owner names, every mtime the commit's time (SOURCE_DATE_EPOCH, else 0),
gzip without a name or time in its header.

Usage: tar-clean.py OUT.tar.gz DIR [PREFIX]
       tar-clean.py OUT.tar.gz IN.tar
  DIR     a directory; its contents go under PREFIX/ (default: DIR's name)
  IN.tar  an existing tar (for example `git archive`), rewritten entry by entry
"""
import gzip
import io
import os
import sys
import tarfile


def clean(info, mtime):
    info.uid = info.gid = 0
    info.uname = info.gname = ""
    info.mtime = mtime
    if info.isdir():
        info.mode = 0o755
    elif info.isfile():
        info.mode = 0o755 if info.mode & 0o111 else 0o644
    return info


def from_dir(tf, src, prefix, mtime):
    paths = [src]
    for root, dirs, files in os.walk(src):
        dirs.sort()
        paths += [os.path.join(root, d) for d in dirs] + [os.path.join(root, f) for f in sorted(files)]
    for p in sorted(paths, key=lambda x: os.path.relpath(x, src)):
        rel = os.path.relpath(p, src)
        name = prefix if rel == "." else f"{prefix}/{rel}"
        info = clean(tf.gettarinfo(p, arcname=name), mtime)
        if info.isfile():
            with open(p, "rb") as f:
                tf.addfile(info, f)
        else:
            tf.addfile(info)


def from_tar(tf, src, mtime):
    with tarfile.open(src) as old:
        for info in sorted(old.getmembers(), key=lambda i: i.name):
            data = old.extractfile(info) if info.isfile() else None
            tf.addfile(clean(info, mtime), data)


def main(argv):
    if len(argv) not in (2, 3):
        sys.exit(__doc__)
    out, src = argv[0], argv[1]
    mtime = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.PAX_FORMAT) as tf:
        if os.path.isdir(src):
            from_dir(tf, src, argv[2] if len(argv) == 3 else os.path.basename(os.path.normpath(src)), mtime)
        else:
            from_tar(tf, src, mtime)
    with open(out, "wb") as f, gzip.GzipFile(filename="", mode="wb", fileobj=f, mtime=0) as gz:
        gz.write(buf.getvalue())
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
