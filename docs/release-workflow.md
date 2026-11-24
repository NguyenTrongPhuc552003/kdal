# Releases built by CI as drafts (design)

**Status:** design and expectations, 2026-11-24, before any code.
Owner-approved ("Approve", 2026-11-24: a CI release workflow like
ELMOS's that writes clean tar entries, then a draft of 0.2.0 for the
owner's yes).

## Facts [F]

- `v0.1.0` (2026-04-19) was built by a `release.yml` that ran on a pushed
  tag: five SDK tarballs (linux x86_64, aarch64, riscv64; darwin aarch64,
  x86_64), a source tarball, a deb, an rpm and a VS Code extension,
  `SHA256SUMS`, published at once, and the extension pushed to the VS Code
  Marketplace with a token. The workflow was removed with the move to
  Codeberg (`a14f6c6`) and not restored.
- `release-check-20261123.md`: that release has no private names, but
  its tarballs carry the CI runners' owner names, which HAVEN's
  public-asset check flags.
- `scripts/release/bump_version.sh` sets the version in
  `include/kdal/core/version.h`, `packaging/debian/changelog`,
  `packaging/rpm/kdal.spec`, `CITATION.cff` and `VERSION`, not in
  `editor/vscode/kdal-lang/package.json`.
- ELMOS's `release.yml` (elmos `docs/developer/release.md`) is the model:
  on demand, a draft, a name check, a test from the draft on other hosts,
  then the owner publishes.

## Design [P]

`.github/workflows/release.yml`, on demand only (input `version`, for
example `0.2.0`):

1. **gate**: `VERSION`, `version.h`, the deb changelog, the rpm spec and
   the extension's `package.json` all say the version; `CHANGELOG.md` has
   a `## [X.Y.Z] - DATE` section; the tag does not exist yet; the CI test
   script passes.
2. **build**: the old workflow's jobs (SDK matrix, source, deb, rpm,
   extension), with every tarball written by one script
   (`scripts/release/tar-clean.py`): entries sorted, uid and gid 0, no
   owner names, the commit's time as mtime. The source tarball is `git
   archive` passed through the same script. No Marketplace step.
3. **draft**: `SHA256SUMS`; notes from the changelog section; a copy of
   HAVEN's public-asset check (`scripts/release/check-public-assets.py`)
   over every asset and the notes; then `gh release create vX.Y.Z
   --draft --target SHA`, title `vX.Y.Z [untested]`.
4. **test from the draft** (ubuntu-24.04, linux x86_64; macos-15, darwin
   aarch64): download through the API, check the hash, the old
   workflow's layout checks, and `kdalc -h` prints `KDAL compiler
   X.Y.Z`.
5. **mark**: the title loses `[untested]`, or gets `[FAILED: do not
   publish]`.

Publishing (which makes the tag) and the Marketplace stay the owner's.

## Expectations

| Row | Check | Expected |
| - | - | - |
| K1 | the gate on today's `main` with `version=0.2.0` | refuses (nothing says 0.2.0 yet); no draft |
| K2 | after the 0.2.0 bump, the build | ten assets and `SHA256SUMS`; every tar entry uid 0, gid 0, no names; the name check 0 failures |
| K3 | the draft | `draft: true`, target the dispatched commit, no Git tag, title with `[untested]` |
| K4 | test from the draft | hashes match on both hosts; the layout checks pass; `kdalc -h` gives the version |
| K5 | mark | title `v0.2.0` |
| K6 | the workflow file | no tag trigger, no publish step, no Marketplace token |

## Runs (2026-11-24)

- **K1 holds** (run 45236442873, `main` at `bb14918`): the gate refuses
  with six failures (VERSION, `version.h`, the Debian changelog, the rpm
  spec, `package.json`, CHANGELOG); nothing else runs, no draft.
- **Run 2** (run 45241981465, `64e61ee`, after the 0.2.0 bump in #51):
  the gate, the packages and three SDKs pass; the linux-aarch64 and
  linux-riscv64 SDKs stop at CMake's compiler check (`cannot find
  Scrt1.o`): with `--no-install-recommends` the cross compilers come
  without their C library (`libc6-dev-arm64-cross`,
  `libc6-dev-riscv64-cross`). No draft was made. **Fix**: the matrix
  installs both packages. K2 to K6 are read from the next run.
- **Run 3** (run 45246903053, `73ad670`): the gate, all five SDKs and
  the packages pass; the name check stops the draft with **2 failures**:
  the two macOS `kdality` binaries hold the runner's macOS home path, and
  the three Linux ones the same path under the Linux runner's home (a
  note, since the check allows CI's Linux home). The path is lint's source-tree fallback
  for the standard library (`KDALITY_STDLIB_SOURCE`, the build machine's
  `lang/stdlib`), meaningless on a user's machine. No draft was made, so
  K2 does not hold yet. **Fix**: option `KDALITY_SOURCE_STDLIB` (on by
  default, as before), set off in the `release-matrix` preset; checked on
  the Mac: a `release-matrix` `kdality` has no source path, a default
  build still has it.
- **Run 4** (run 45250312798, `4176345`): every build passes and the
  SDK binaries are clean (no failure or note from them); the name check
  fails once, on this record in the source tarball: run 3's entry quoted
  the macOS runner's home path literally. My error; the entry now
  describes the path instead. No draft.
- **Run 5** (run 45253065422, `8596442`): every job passes after two
  reruns of macOS jobs that no runner picked up (`The job was not
  acquired by Runner`). The first rerun's test was abandoned the same
  way, and the mark step titled the draft `[FAILED: do not publish]`, as
  designed; the second passed and the title became `v0.2.0`.
  - **K2, the count does not hold as written; the rest holds.** 13
    assets and `SHA256SUMS`, not ten: the rpm build also makes `-devel`,
    `-debuginfo` and `-debugsource` packages, and the deb build `kdal-dev`
    (my count left them out). Every hash matches; every tar entry is uid
    0, gid 0, no user or group name (all six tarballs, 565 entries); the
    name check has 0 failures; the deb, rpm and vsix contents (134 files)
    hold no macOS home path and neither the Mac's login nor its host
    name (checked on the Mac).
  - **K3 holds**: `draft: true`, target `8596442`, no Git tag
    `v0.2.0` on the remote.
  - **K4 holds**: on linux-x86_64 and darwin-aarch64 the hash matches,
    the layout checks pass and `kdalc -h` says `KDAL compiler 0.2.0`.
  - **K5 holds**: title `v0.2.0`.
  - **K6 holds**: the only trigger is `workflow_dispatch`; no publish
    step; `vsce` only packages; no Marketplace token.
  - The notes carry two result lines for the same run (`abandoned`,
    then `success`): the mark step only removes text after a marker the
    first mark already removed.

## Published (2026-11-24) [F]

Owner's yes on 2026-11-24. Before publishing, the notes lost run 5's
`abandoned` line (the name check passed on the new notes). Publishing
with only `draft=false` left the release `untagged-...` for about a
minute although the draft named `v0.2.0`; a second edit with
`tag_name=v0.2.0` and the target `8596442` made the tag. The published
release has its 14 assets and every hash in `SHA256SUMS` matches; the
installer (`kdalup install`, under a stand-in HOME on the Mac) resolves
the latest release to v0.2.0, checks the hash, and its `kdalc` prints
0.2.0.

**Next time:** publish in one edit with the tag and the target:
`gh api -X PATCH repos/OWNER/kdal/releases/ID -F draft=false -f
tag_name=vX.Y.Z -f target_commitish=SHA`.
