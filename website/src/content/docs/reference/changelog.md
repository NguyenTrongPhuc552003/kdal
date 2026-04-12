---
title: Changelog
description: Release history for the KDAL project.
---

All notable changes are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/).

---

## [0.1.0] - 2025-06-09

### Added

#### Toolchain Subcommands

- `kdality init <name>` - scaffold new driver projects (.kdh + .kdc + Makefile + README)
- `kdality dt-gen <file.kdh>` - generate Device Tree Source overlays from .kdh headers
- `kdality simulate <file.kdc>` - dry-run interpreter with register trace (no kernel needed)
- `kdality test-gen <file.kdc>` - auto-generate KUnit test stubs from driver handlers
- `kdality lint <file>` - static analysis with 9 rules (W001–W009) and `--strict` mode

#### VS Code Extension

- TextMate grammars for `.kdh` and `.kdc` syntax highlighting
- Code snippets: device skeleton, driver skeleton, handler stubs, register access
- Language configuration with comment toggling and bracket matching

#### Core Runtime

- Module init/exit with full cascade (chardev → debugfs → backends → drivers)
- Registry with linked-list storage for backends, drivers, and devices
- Duplicate name prevention and find-by-name/class queries
- Device iterator (`kdal_for_each_device()`) and registry snapshot counters
- Circular event buffer (256 entries) with mutex and waitqueue
- Per-device power state management with event emission

#### Control Plane

- Character device `/dev/kdal` via `misc_register()`
- Per-fd state with `kdal_file_ctx` in `file->private_data`
- 6 ioctl commands: GET_VERSION, GET_INFO, SET_POWER, LIST_DEVICES, POLL_EVENT, SELECT_DEV
- Read/write dispatch to selected device's driver
- debugfs interface at `/sys/kernel/debug/kdal/`

#### Drivers

- UART driver with ring buffer allocation, baud/parity/stop config
- I2C driver with bus speed and slave address config
- SPI driver with clock/mode/bits-per-word config
- GPU driver with accelerator ops (queue/buffer/submit)

#### Backends

- QEMU backend with 4096-byte per-device ring buffer
- Generic platform device backend
- SoC glue layer stub for CIX P1
- Device Tree scanner (`CONFIG_OF` guarded)
- MMIO helper with 8/16/32-bit read/write

#### Userspace

- `kdality` CLI: version, list, info, power, read, write commands
- Smoke test, integration test, and throughput benchmark

#### Testing

- 38 KUnit test cases across 5 suites (core, registry, event, driver, accel)
- CI test script with 4-stage pipeline

#### Infrastructure

- QEMU workspace preparation (cloud image, cloud-init, 9p shared dir)
- Kernel cross-compilation script for Linux 6.6.80 aarch64
- QEMU launch and debug scripts
- `checkpatch.pl` wrapper, static analysis, coverage collection
- Version bump script and changelog generator
- GitHub Actions CI workflows (build, test, lint, CodeQL, release)

#### Documentation

- Architecture, API Reference, Installation, Porting Guide
- Performance methodology, FAQ, Upstreaming strategy, Thesis mapping

#### Language Ecosystem

- KDAL DSL with `.kdh` device headers and `.kdc` driver files
- Compiler pipeline: lexer → parser → AST → sema → C codegen
- 7 standard library `.kdh` files (UART, I2C, SPI, GPIO, GPU, NPU, generic)
- 8 CMake modules for IDE integration and cross-compilation
- Language specification and design rationale
- Example drivers (UART, I2C sensor)
