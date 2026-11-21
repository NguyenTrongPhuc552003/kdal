# The first-project journey: kdality's CLI, init templates and e2e

**Status:** findings and expectations, 2026-11-21, written before the
change (owner, 2026-11-21: maintain the ecosystem, resolve existing issues,
improve the UX). **[F]** observed here, **[I]** inference, **[P]** proposal.

## What a new user meets today [F]

Observed on `main` (`1553924`) with the release build (`build/ci/release`):

1. **`kdality init` makes a project that does not build.** In a fresh
   `kdality init mydrv --class uart`, `make compile` stops with
   `kdalc: cannot open output ''` (the output directory does not exist);
   with the directory made by hand it writes `unknown.c`, and `kdality
   simulate mydrv_driver.kdc` says `no driver found`. The `.kdc` template
   uses `use "mydrv.kdh";` and `on probe { reg_write(CTRL, 0x01); }`,
   which the parser does not accept (`use` is not a keyword; the grammar
   says `import "x.kdh" as DEV;`). The `.kdh` template's simplified form
   (`register_map`, `power_states`) does parse. The same `.kdh` with a
   driver in the grammar's form compiles to `mydrv_driver.c` and simulates.
2. **`lint`, `simulate` and `compile` read `argv[0]` (the subcommand's
   name) as an input.** `kdality lint` with no file says `lint: failed to
   resolve input path` instead of `lint: no input file`. `lint` and
   `simulate` also ignore unknown options, and a second input silently
   replaces the first.
3. **Absolute and parent paths are refused.** `kdality compile -o
   /abs/out x.kdc`, `-o ../build`, `kdality lint /abs/x.kdh` and
   `test-gen -o /abs/out` stop with "invalid output directory" or "must be
   a safe relative path". These checks came with the CodeQL path-alert
   fixes (CHANGELOG).
4. **`test-gen -o tests` and `compile -o out` fail when the directory does
   not exist.**
5. **The e2e suites hide failures.** `scripts/e2e/test_compiler.sh`
   reports a failing `compile`, `lint` and `test-gen` as SKIP ("may be
   alpha limitation"); all three fail only because of item 3 (the suite
   passes absolute paths). `test_user_journey.sh` passes an absolute path
   as `init`'s project *name*, so `init` "is not fully implemented" (SKIP)
   though it works. `test_release.sh` checks six workflows that no longer
   exist (`build.yml` to `nightly.yml`; CI is `ci.yml` and
   `weekly-deep.yml`): 14 FAIL. `test_developer_journey.sh` looks for
   `docs/getting-started.md`, now on the HAVEN docs site. No CI job runs
   the e2e suites.
6. **The docs teach `use`.** The HAVEN docs site's KDAL quickstart,
   `language/driver-code.md` and `language/stdlib.md`, and this
   repository's `docs/language_guide.md`, show `use "x.kdh";`, which does
   not parse.

## Changes [P]

- **Paths.** `kdality` and `kdalc` run with the user's own rights on paths
  the user typed: there is no trust boundary for a path check to defend,
  and the checks break ordinary use. Inputs and output directories are
  taken as given (absolute, relative, `..`); empty strings and control
  characters are refused. Names that end up *inside* generated files or
  file names (`dt-gen`'s device name, `test-gen`'s driver name, `init`'s
  project name and class) keep their checks. Output directories are
  created (one level, mode 0755) when missing. CodeQL path alerts that
  return are dismissed with this reasoning, not silenced in code.
- **Arguments.** Every subcommand parses from `argv[1]`; an unknown option
  and a second input are errors with the usage line.
- **Templates.** `init`'s `.kdc` uses the grammar's form (`kdal_version`,
  `import ... as DEV`, `probe`, `remove`, `on read(DEV.DATA)`,
  `write(DEV.CTRL, ...)`); its Makefile and README say the same.
- **e2e.** A failing command is a FAIL; only a missing tool is a SKIP. The
  release suite checks the workflows that exist; the stale docs check goes.
  A new journey suite: `init`, `make compile`, `simulate`, `lint`,
  `test-gen`, all in the new project. CI runs the e2e suites on Linux
  (job `e2e`), failing on any FAIL.
- **Docs.** `use` becomes `import ... as` (HAVEN docs, a separate pull
  request there; `docs/language_guide.md` here).

## Expectations

| Row | Check | Expected |
| - | - | - |
| J1 | `kdality init mydrv --class uart && cd mydrv && make compile` | exit 0; `output/mydrv_driver.c` and `output/Makefile.kbuild` exist |
| J2 | `kdality simulate mydrv_driver.kdc` in that project | names `mydrv_driver`, runs probe and remove, exit 0 |
| J3 | `kdality lint mydrv.kdh` and `kdality test-gen -o tests mydrv_driver.kdc` there | exit 0; `tests/` created and holds the generated test |
| J4 | `kdality lint`, `kdality simulate`, `kdality compile` with no input | exit 1 with "no input" and the usage |
| J5 | `lint --bogus x.kdh`; `lint a.kdh b.kdh`; the same for `simulate` | exit 1, "unknown option" or "more than one input" |
| J6 | `compile -o /abs/out x.kdc`, `-o ../out`, `lint /abs/x.kdh`, `test-gen -o /abs/out` | exit 0 |
| J7 | `dt-gen` and `test-gen` with a device or driver name containing `/` | still refused (names inside generated files) |
| J8 | e2e on Linux in CI | every suite passes with no SKIP of a command that ran and failed; the job fails on a FAIL |
| J9 | KUnit, smoke tests, the quality gate | unchanged, green |
| J10 | the HAVEN docs site | no `use "` left in KDAL pages; the quickstart's driver compiles with `kdality compile` |
