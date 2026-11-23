#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check files before they become public (docs/methodology/stage2/
20261113-e7-releases, design item 2). Standard library only.

Usage: check-public-assets.py PATH...   (files or directories)

Archives (.tar.gz, .tgz) are opened and every member is read; other .gz
files are read decompressed. A failure is:
  (a) a tar entry not owned by uid 0 and gid 0, or with a user or group
      name other than empty or "root";
  (b) a /Users/<name> path (a macOS home directory);
  (c) a "Linux version" banner whose builder is not haven@ci;
  (d) any word of the environment variable RELEASE_DENY (comma separated,
      case-insensitive) in file contents, member names or entry owners.
/home/<name> paths are listed as notes, not failures (CI's /home/runner).
Exit 0 if nothing failed, 1 otherwise.
"""
import gzip
import os
import re
import sys
import tarfile

USERS = re.compile(rb"/Users/([A-Za-z0-9._-]+)")
HOME = re.compile(rb"/home/([A-Za-z0-9._-]+)")
BANNER = re.compile(rb"Linux version \S+ \(([^)\s]+)\)")


def deny_words():
    words = os.environ.get("RELEASE_DENY", "")
    return [w.strip().lower().encode() for w in words.split(",") if w.strip()]


def scan(where, data, deny, out):
    """Rules (b) to (d) on the bytes DATA of WHERE; notes for /home/."""
    for m in sorted(set(USERS.findall(data))):
        out["fail"].append(f"{where}: macOS home path /Users/{m.decode(errors='replace')}")
    for m in sorted(set(BANNER.findall(data))):
        if m != b"haven@ci":
            out["fail"].append(f"{where}: Linux banner built by {m.decode(errors='replace')}")
    low = data.lower()
    for w in deny:
        if w in low:
            out["fail"].append(f"{where}: a denied word ({len(w)} characters)")
    for m in sorted(set(HOME.findall(data))):
        out["note"].append(f"{where}: /home/{m.decode(errors='replace')}")


def check_tar(path, deny, out):
    with tarfile.open(path, "r:*") as tar:
        for m in tar.getmembers():
            where = f"{path}:{m.name}"
            if m.uid != 0 or m.gid != 0 or m.uname not in ("", "root") or m.gname not in ("", "root"):
                out["fail"].append(f"{where}: owned by {m.uid}:{m.gid} with names")
            names = " ".join((m.name, m.uname, m.gname)).lower().encode()
            for w in deny:
                if w in names:
                    out["fail"].append(f"{where}: a denied word in the entry's name or owner")
            if m.isfile():
                f = tar.extractfile(m)
                scan(where, f.read() if f else b"", deny, out)


def check_file(path, deny, out):
    if path.endswith((".tar.gz", ".tgz")):
        check_tar(path, deny, out)
    elif path.endswith(".gz"):
        with gzip.open(path, "rb") as f:
            scan(path, f.read(), deny, out)
    else:
        with open(path, "rb") as f:
            scan(path, f.read(), deny, out)


def check(paths, deny=None):
    """{"fail": [...], "note": [...]} for the files under PATHS."""
    deny = deny_words() if deny is None else deny
    out = {"fail": [], "note": []}
    for p in paths:
        files = [os.path.join(d, n) for d, _, ns in os.walk(p) for n in sorted(ns)] if os.path.isdir(p) else [p]
        for f in sorted(files):
            check_file(f, deny, out)
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = check(sys.argv[1:])
    for n in out["note"]:
        print(f"note: {n}")
    for f in out["fail"]:
        print(f"FAIL: {f}")
    print(f"check-public-assets: {len(out['fail'])} failure(s), {len(out['note'])} note(s)")
    sys.exit(1 if out["fail"] else 0)


if __name__ == "__main__":
    main()
