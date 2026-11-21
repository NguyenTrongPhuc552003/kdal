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
| O5 | the extraction on #38's failing console log (`evidence/kunit-demand-interleave-20261121.log`) | valid JSON, the same report as the passing run |
| O6 | 200 KUnit boots in CI (a stress job like HAVEN's console drain) | 0 failing extractions |
