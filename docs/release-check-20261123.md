# Release v0.1.0 checked for private names (2026-11-23)

The public release `v0.1.0` (tag at `5c01ae2`, published 2026-04-19)
was checked with HAVEN's public-asset check (`haven/scripts/release/
check-public-assets.py`), the denied words set only for the command from
the Mac's login and host name, as for ELMOS's demo releases.

[F] Results:

- All 13 assets download; every hash in `SHA256SUMS` matches.
- No Mac login, host name or home path in any asset (rules (b), (c) and
  (d) find nothing).
- Rule (a) flags 280 tar entries, 56 in each of the five binary
  tarballs: they carry owner names. The names are the CI runners'
  (`runner`, `staff`, `runner`) and `root` in the source tarball, not
  personal ones. Nothing to withdraw.
- `CHANGELOG.md` dates 0.1.0 to 2025-06-09; the tag and the GitHub
  release are of 2026-04-19. The changelog now says both.

[P] For the next release: write tar entries with uid and gid 0 and no
names (as ELMOS's `demo-release.yml` does), so the check passes whole;
`CHANGELOG.md`'s Unreleased section holds 22 entries since 0.1.0, a
candidate for 0.2.0 when the owner wants one.

The local tag `ci-kernel-v6.18-kunit` (its release and remote tag were
deleted by the owner on 2026-11-13) was removed from the Mac's clone.
