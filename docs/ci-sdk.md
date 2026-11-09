# KUnit CI on the ELMOS SDK (ecosystem step E3)

**Status:** design and expectations, 2026-11-09, written before the change
(owner-approved E-gate; owner rules of 2026-11-09: CI first, CI outputs
private to the project's repositories).

## Why

The KUnit job builds `kdal.ko` against a Linux 6.18 kernel that was built
on the owner's Mac and published by hand as the public release
`ci-kernel-v6.18-kunit`. ELMOS's CI now builds the same kernel (arm64
`defconfig` plus `CONFIG_KUNIT=y`, LLVM, neutral build names) and
publishes it as the private package `ghcr.io/nguyentrongphuc552003/elmos-sdk`
(ELMOS `docs/developer/sdk-ci.md`).

## Change

In `.github/workflows/ci.yml`, job `kunit`:

- `permissions: packages: read` for the job;
- a step before "Prepare the pinned kernel": log in to ghcr.io with
  `GITHUB_TOKEN`, then `oras pull` a pinned SDK tag into
  `build/ci-kernel`. `scripts/ci/kernel-prepare.sh` skips every file that is
  already there, so the hand-made release is no longer downloaded; it
  still checks `SHA256SUMS` (now the SDK's), fetches the v6.18 source from
  kernel.org and runs `modules_prepare`.
- The SDK tag is pinned in the workflow (a run-numbered tag, not the moving
  `linux-6.18` one), so a new SDK build changes KDAL's CI only through a
  reviewed pull request.

Prerequisite (owner): in the `elmos-sdk` package settings, "Manage Actions
access", add the `kdal` repository with the Read role.

## Expectations (before the change)

| Check | Expected |
| - | - |
| K1: KUnit job on the pull request | passes: 40 of 40 cases in 6 suites, as before |
| K2: kernel used | the console log's `Linux version 6.18.0 (haven@ci)`; `kernel.release` is `6.18.0` (no `-dirty`, which the hand-made kernel had) |
| K3: no public download | the job log has no `releases/download` URL |
| K4: without access | if the package cannot be read (access not granted), the job fails at the pull step, not later |
| K5: other jobs | unchanged, pass |

## Results (2026-11-09, pull request #26, merge `78e9d62`)

| Check | Observed | Verdict |
| - | - | - |
| K1 | kdal-accel 5, kdal-core 10, kdal-demand 2, kdal-driver 8, kdal-event 6, kdal-registry 9: 40 of 40 pass | as expected |
| K2 | the pulled manifest names `Linux version 6.18.0 (haven@ci)`, ELMOS commit `4eb897c`, run 4; `kernel-prepare.sh` passed its check that the prepared tree's release equals `kernel.release` (it stops otherwise). The console log is printed only on failure, so its banner was not read | as expected for the release; banner not observed |
| K3 | the only `releases/download` URL in the job log is the ORAS CLI download by `setup-oras` (oras-project); no KDAL release is downloaded | as expected in substance; the check's wording did not foresee the tool download |
| K4 | not tested: access was granted before the first run | not judged |
| K5 | the other jobs passed unchanged | as expected |
