---
name: kdal
description: 'KDAL project knowledge — architecture, conventions, build commands, compiler pipeline, file formats (.kdh/.kdc), key types, and common workflows for the Kernel Device Abstraction Layer ecosystem.'
---

# KDAL Project Skill

## Project Overview

KDAL (Kernel Device Abstraction Layer) is an out-of-tree Linux kernel framework + domain-specific language (DSL) for writing portable embedded device drivers. It targets Linux 6.6 LTS on QEMU aarch64 virt (Mac M4 Pro host).

## Repository Structure

```
include/kdal/          Kernel-space public API headers
src/core/              Core runtime (module, chardev, debugfs, registry, events, power)
src/backends/qemu/     QEMU virt backend (ring buffer, MMIO, virtio stubs)
src/backends/generic/  Generic platform backend (platdev, DT, SoC glue)
src/drivers/example/   Built-in drivers (UART, I2C, SPI, GPU)
compiler/              KDAL compiler (kdalc): lexer, parser, sema, codegen → libkdalc.a
compiler/include/      Compiler headers: token.h, ast.h, codegen.h
lang/grammar/          EBNF grammars for .kdh and .kdc
lang/stdlib/           Standard device headers (uart.kdh, i2c.kdh, spi.kdh, gpio.kdh, gpu.kdh, npu.kdh, common.kdh)
lang/spec.md           Language specification
lang/DESIGN.md         Design rationale and comparison table
tools/kdality/         Unified CLI binary (runtime control + compiler frontend)
tests/kunit/           5 KUnit test suites (core, registry, event, driver, accel)
tests/userspace/       Smoke test (testapp) + throughput benchmark
tests/integration/     Multi-device integration test (kdaltest)
examples/kdc_hello/    Example .kdh/.kdc drivers
examples/minimaldriver/ Out-of-tree kernel module example (plain C)
examples/accel_demo/   GPU/NPU userspace demo programs
cmake/                 8 CMake modules (FindKDAL, Config, Targets, CompileKDC, Toolchain, Testing, Packaging, ConfigVersion)
scripts/ci/            CI scripts (test.sh, checkpatch.sh, static_analysis.sh, coverage.sh)
scripts/setupqemu/     QEMU setup (prepare, buildkernel, run, debug)
scripts/release/       Version bump + changelog generation
packaging/             Debian + RPM packaging stubs
editor/vscode/         VS Code extension for .kdh/.kdc syntax highlighting
docs/                  8 documentation files
```

## File Formats

- **`.kdh`** — Device header: declares register map, signals, capabilities, power states, config. Analogous to a C header. Read-only contract.
- **`.kdc`** — Driver implementation: imports a `.kdh`, declares backend, implements probe/remove/event handlers. Compiled to kernel C.
- **`.c` + `Makefile.kbuild`** — Generated output from `.kdc` compilation. Fed to kbuild to produce `.ko`.

## Compiler Pipeline

```
.kdc → Lexer (lexer.c) → Tokens → Parser (parser.c) → AST → Sema (sema.c) → Codegen (codegen.c) → .c + Makefile.kbuild
```

- **Lexer**: Hand-written, single-pass, ~60 keywords. Entry: `kdal_lex()`
- **Parser**: Recursive descent, LL(1). Arena-allocated AST. Entry: `kdal_parse()`
- **Sema**: Symbol table, register access checks, bounded-loop validation. Entry: `kdal_sema()`
- **Codegen**: Emits `platform_driver` struct, probe/remove, handler dispatch, OF table. Entry: `kdal_generate()`
- **Full pipeline**: `kdal_compile_file()` in codegen.h

## Key Types

### Kernel-space (include/kdal/)
- `struct kdal_device` — registered device instance (types.h)
- `struct kdal_driver` / `struct kdal_driver_ops` — driver contract (api/driver.h)
- `struct kdal_backend` / `struct kdal_backend_ops` — backend contract (backend.h)
- `struct kdal_accel_ops` — accelerator contract (api/accel.h)
- `enum kdal_device_class` — UNSPEC, UART, I2C, SPI, GPIO, GPU, NPU (types.h)
- `enum kdal_power_state` — OFF, ON, SUSPEND (types.h)

### Compiler (compiler/include/)
- `kdal_token_t` — token with type, line/col, value union (token.h)
- `kdal_ast_t` — base AST node with type, line/col, sibling/child (ast.h)
- `kdal_file_node_t`, `kdal_driver_node_t`, `kdal_handler_node_t` — typed AST nodes (ast.h)
- `kdal_arena_t` — linear arena allocator for AST (ast.h)
- `kdal_codegen_opts_t` — output_dir, kernel_dir, cross_compile, verbose (codegen.h)

## Build Commands

```sh
make all                # Build compiler + kdality + kernel module
make compiler           # Build libkdalc.a + kdalc binary
make tool               # Build kdality binary
make module             # Build kdal.ko (needs KDIR, ARCH, CROSS_COMPILE)
make test               # Run ./scripts/ci/test.sh (5-stage smoke test)
make clean              # Clean all artifacts

# Compiler standalone
make -C compiler        # Produces compiler/libkdalc.a + compiler/kdalc

# kdality CLI
kdality version         # Show version
kdality list            # List registered devices (needs /dev/kdal)
kdality compile file.kdc -o output/  # Compile .kdc → .c + Makefile.kbuild
kdality init mydriver --template uart  # Scaffold a new driver project
kdality lint file.kdc   # Static analysis for .kdc/.kdh
kdality simulate file.kdc --scenario probe  # Dry-run register trace
kdality dt-gen file.kdh -o overlay.dtso     # Generate Device Tree overlay
kdality test-gen file.kdc -o tests/         # Generate KUnit test skeleton

# CI
./scripts/ci/test.sh              # 5-stage: build kdality, version check, compiler, test programs, smoke tests
./scripts/ci/checkpatch.sh        # Kernel coding style
./scripts/ci/static_analysis.sh   # cppcheck + sparse
```

## Coding Conventions

### Kernel C (src/, include/)
- Linux kernel coding style: tabs, 80-col soft limit, `/* C89 comments */`
- All exported symbols prefixed `kdal_`
- `goto` error unwinding pattern
- No `printk` spam — use `pr_debug()` for dev, `pr_info()` for lifecycle

### Userspace C (tools/, compiler/, tests/)
- C99 with `-Wall -Wextra -pedantic`
- Function naming: `kdal_` prefix for library functions, `kdality_` for CLI subcommands
- Version string injected via `-DKDAL_VERSION_STRING` from VERSION file

### KDAL Language (.kdh/.kdc)
- `kdal_version: "0.1";` at top of every file
- `import "path.kdh" as Alias;` for device headers
- `device class Name { registers { ... } signals { ... } }` in .kdh
- `driver Name for Alias.DeviceClass { probe { ... } on read(REG) { ... } }` in .kdc
- Intentionally NOT Turing-complete: no while, no recursion, no heap, bounded for-loops only

### Scripts
- POSIX sh with `set -eu` (not bash-specific)

## ioctl ABI (6 commands)

| Command              | Nr   | Direction | Payload                |
|---------------------|------|-----------|------------------------|
| GET_VERSION         | 0x00 | _IOR      | kdal_ioctl_version     |
| GET_INFO            | 0x01 | _IOWR     | kdal_ioctl_info        |
| SET_POWER           | 0x02 | _IOW      | u32                    |
| LIST_DEVICES        | 0x03 | _IOR      | kdal_ioctl_list        |
| POLL_EVENT          | 0x04 | _IOR      | kdal_ioctl_event       |
| SELECT_DEV          | 0x05 | _IOW      | kdal_ioctl_devname     |

## Testing Patterns

- **KUnit** (tests/kunit/): In-kernel, tests internal APIs. 5 suites, 38 cases.
- **Userspace** (tests/userspace/): testapp (smoke), benchmark (throughput). Run with `--dry-run` when no /dev/kdal.
- **Integration** (tests/integration/): kdaltest exercises full ioctl path.
- CI script `./scripts/ci/test.sh` validates all userspace-compilable code in 5 stages.

## Common Pitfalls

1. **Missing `#include "token.h"` in codegen.h** — `kdal_token_t` used in function signatures needs token.h before ast.h
2. **VERSION file** — Must exist at repo root; Makefiles read it for `-DKDAL_VERSION_STRING`
3. **Linker order** — `libkdalc.a` must come AFTER object files in link command (C static lib convention)
4. **Relative paths** — `tools/kdality/compile.c` includes `../../compiler/include/codegen.h`
5. **Arena allocation** — All AST nodes allocated from `kdal_arena_t`; never free individual nodes
6. **Register access** — `.kdh` declares `ro`/`wo`/`rw`/`rc`; sema.c enforces at compile time
7. **No newline at EOF** — Causes `-Wnewline-eof` warnings on macOS clang; ensure trailing newline
