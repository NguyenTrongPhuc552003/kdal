# Open findings, 2026-11-21

**Status:** findings and expectations, written before any change (owner,
2026-11-21: resolve the existing issues). Found while doing
`first-project-journey.md`. **[F]** observed, **[I]** inference, **[P]**
proposal.

## F1. `kdality compile` does not read imported headers [F]

A `.kdc` that imports a header that does not exist compiles (exit 0), and
the generated C names the register as written: `write(D.CTRL, 1)` becomes
`kdal_reg_write(&ctx->kdal, ctx->regs.CTRL, 1)`. Neither `kdalc` nor
`kdality compile` opens the import. A misspelt register is caught by
`kdality lint` (it now reads imports) or by the C compiler, never by the
KDAL compiler, and the header's offsets and access modes do not reach the
generated code from the compiler [I].

**Proposal [P]:** the compiler resolves imports the way `lint` does (next
to the `.kdc`, then the standard library) and stops on a missing header
or an unknown register (an error, with the line). How register offsets
reach `ctx->regs` today is read and recorded before the change.

| Row | Check | Expected |
| - | - | - |
| O1 | compile a `.kdc` whose import does not exist | exit 1, "cannot find imported header", the line |
| O2 | compile `write(DEV.NOPE, 1)` against a header without `NOPE` | exit 1, "unknown register 'NOPE'", the line |
| O3 | every example and a fresh `kdality init` project | compile as today (no new error) |

## F2. `kdality simulate` prints log strings with doubled quotes [F]

`log("hello_led: initialized\n")` is traced as
`log(""hello_led: initialized\n"")`, and `wait(LED.toggle, 1000)` as
`wait()`.

| Row | Check | Expected |
| - | - | - |
| O4 | `simulate --trace` on the quickstart driver | `log("hello_led: initialized\n")` (one pair of quotes); `wait(toggle, 1000)` |

## F3. The KUnit job's demand report can mix with KUnit's console lines [F]

On KDAL #38 (a docs-only change) the KUnit job failed once:
`demand.json: Extra data`. In its console the init program's
`KDAL-DEMAND-BEGIN` came before four KUnit lines (`# Subtest:
kdal-demand` to `ok 6 kdal-demand`), which the extraction copied into the
JSON. The rerun passed. Cause [I]: Linux 6.18 prints kernel messages to
the console from a printing thread, after the init program's own writes
may already have reached the UART; the extraction takes every line
between the markers.

**Proposal [P]:** the extraction keeps only the lines that are not
kernel log lines (`[   0.123456] ...`) between the markers; and the init
program waits for KUnit's last line before it prints the report
(`kdal-demand`'s "ok" in `/dev/kmsg`, read with a time limit).

| Row | Check | Expected |
| - | - | - |
| O5 | the extraction on #38's failing console log (`evidence/kunit-demand-interleave-20261121.txt`) | valid JSON, the same report as the passing run |
| O6 | 200 KUnit boots in CI (a stress job like HAVEN's console drain) | 0 failing extractions |

## F4. The modules `kdality compile` writes may not build at all [F, I]

Found while reading F1 (2026-11-21, afternoon). The generated C for
`examples/kdc_hello/uart_hello.kdc`:

- includes `<linux/kdal.h>`; KDAL has no such header (it has
  `include/kdal/core/kdal.h` and `include/kdal/api/*.h`) [F];
- calls `kdal_reg_read`, `kdal_reg_write` and `kdal_wait_signal`; no KDAL
  source defines them, and `src/` exports no symbol at all
  (`EXPORT_SYMBOL` appears nowhere), so a separate module could not call
  `kdal.ko` even if the names matched [F];
- reads `ctx->regs.DATA`, `ctx->regs.CTRL.enable`; the generated
  `struct UartHelloDriver_ctx` has no `regs` member [F].

So the language's last step (`.kdc` to a loadable module) is not
implemented end to end, and the docs that end in `make module` / `insmod`
describe a step that has not been shown to work [I]. No CI job compiles a
generated module today (`ci.yml` builds only the userspace demos) [F].

**First, the evidence [P]:** `scripts/ci/codegen-check.sh` and the
on-demand workflow `codegen-check.yml` run `kdality compile` on every
example and then Kbuild against the pinned Linux 6.18 tree, KDAL's
`include/` on the path.

| Row | Check | Expected (predicted before the run) |
| - | - | - |
| O7 | `codegen-check.yml` on `main` | both examples: `kdality compile` succeeds; the module build **fails**, among the first errors `linux/kdal.h` not found |

**Then [P], after O7's result and the owner's view:** F1 and F4 are one
design: the compiler reads the header and emits a register table
(offsets, widths, access), and the generated module uses a runtime that
exists (either an API `kdal.ko` exports, or self-contained `readl`/
`writel` on an `ioremap`ed base). Its record and expectations come before
any code; until then the docs say plainly that the module step is not
working yet.

## Results (2026-11-21) [F]

| Row | Result | Evidence |
| - | - | - |
| O4 | holds | #41: `log("hello_led: initialized\n")` and `wait(toggle, 1000)` in the trace; the first-project suite checks both in CI |
| O5 | holds | #42: `demand-extract.py` on `evidence/kunit-demand-interleave-20261121.txt` gives the same report as a passing run; the smoke tests run it |
| O6 | holds | `kunit-stress.yml` at `7d0ebae`: 0 of 200 stress boots failed (four shards of 50), plus each shard's first boot. No stress run was made before the fix, so this shows the new path does not fail, not how often the old one did |
| O7 | holds as predicted | `codegen-check.yml` at `db187e8` (the first run, at `679db7d`, stopped before the module build because of the check's own relative output directory, fixed in #45): both examples compile with `kdality compile`; both module builds stop at `fatal error: 'linux/kdal.h' file not found`. The other gaps (undefined `kdal_reg_*`, no `regs` member, no exported symbols) are behind that first error: read in the code, not yet shown by a build |
| O1 to O3 | not started | see below |

## F1 and F4 and the freeze [F, ?]

The README and `stages/` (decision of 2026-11-01) freeze `lang/`,
`compiler/` and `editor/`: "kept and still built by CI, but take no new
work unless the thesis needs them". The F1 and F4 fix is compiler work,
so it is not started; it needs the owner's decision (unfreeze the
compiler for it, or park it and say so in the docs). One change already
went into `compiler/`: #39 relaxed `kdalc`'s source path check (the same
change as `kdality`'s paths), made without noticing the freeze; the
owner decides whether it stays. `CHANGELOG.md` listed `tools/` as frozen
too; #44 corrected it to the README's list (`tools/`, the `kdality`
runtime CLI, is maintained).

## Decisions (2026-11-21, delegated by the owner: "up to you")

1. **F1 and F4 are parked with the freeze**, not fixed now. The thesis
   uses KDAL's kernel module (`kdal.ko`, the Linux partition's driver and
   its demand report), not the `.kdc`-to-module path; making that path
   work is a runtime API, exported symbols and a code generator change,
   a project of its own in frozen code. What a user needs now is to know
   the limit: the README and the docs site's quickstart say it, and
   `codegen-check.yml` stays on demand to show when it changes. Revisited
   at the next review gate with the other parked KDAL items.
2. **#39's change to `compiler/main.c` stays.** It makes `kdalc` accept
   the same paths as `kdality` (any path but an empty one or one with
   control characters), ten lines, CI green; undoing it would bring back
   the difference between the two tools that #39 removed. Recorded here
   as an exception to the freeze, made before the freeze was noticed.
