---
name: Bug report
about: Report a crash, wrong result or build failure in KDAL
title: "bug: "
labels: bug
---

## Summary

<!-- One or two sentences: what goes wrong? -->

## Component

<!-- Tick one. The language, compiler, editor plugins and tools are frozen
since 2026-11-01: reports are welcome, fixes may wait. -->

- [ ] Kernel module (`src/`, `include/`)
- [ ] Tests (`tests/kunit`, `tests/integration`, `tests/userspace`)
- [ ] CI scripts (`scripts/ci`)
- [ ] Frozen: language, compiler, editor plugins or tools (`lang/`, `compiler/`, `editor/`, `tools/`)
- [ ] Documentation or other

## Environment

- Kernel version (`uname -r`, or the kernel tree and tag you built against):
- Architecture (for example arm64 QEMU `virt`, arm64 board, x86_64):
- KDAL commit (`git rev-parse --short HEAD`):
- Compiler and toolchain (for example clang with `LLVM=1`, or `aarch64-linux-gnu-gcc`):
- Host OS (if you cross-compile):

## How the module was built and loaded

<!-- Exact commands, for example:
make KDIR=/path/to/linux ARCH=arm64 LLVM=1 [KDAL_KUNIT=y]
insmod kdal.ko
Mention the options of the target kernel that matter (CONFIG_KUNIT,
CONFIG_OF, CONFIG_DEBUG_FS) and any device tree you added. -->

```sh

```

## Steps to reproduce

1.
2.
3.

## Expected behavior

## Actual behavior

## dmesg output

<!-- Paste the relevant part of `dmesg` from module load onwards, including
any oops, warning or KUnit (KTAP) output. -->

```text

```
