# KDAL - Kernel Device Abstraction Layer

A hardware-agnostic, pluggable abstraction framework inside the Linux kernel
that lets developers write portable device drivers and accelerator backends
**once** and run them on **any** embedded platform.

> Master thesis project - Computer Engineering

## Features

- **Single control device** (`/dev/kdal`) with per-fd device selection
- **4 device drivers**: UART, I2C, SPI, GPU (with accelerator ops)
- **Pluggable backends**: QEMU ring-buffer, generic platform device, Device Tree
- **6 ioctl commands**: GET_VERSION, GET_INFO, SET_POWER, LIST_DEVICES, POLL_EVENT, SELECT_DEV
- **debugfs interface**: `/sys/kernel/debug/kdal/` for introspection
- **Event system**: circular buffer with waitqueue-based notification
- **Power management**: per-device power state transitions
- **KUnit tests**: 38 in-kernel test cases
- **Userspace tools**: CLI tool, smoke test, integration suite, throughput benchmark
- **QEMU-first workflow**: fully reproducible on Mac M4 Pro → aarch64 guest

### Language Ecosystem

- **KDAL DSL**: `.kdh` device headers + `.kdc` driver implementations
- **Compiler** (`kdalc`): hand-written lexer → parser → sema → codegen; produces kernel C + Kbuild
- **Standard library**: 7 device headers (UART, I2C, SPI, GPIO, GPU, NPU, common)
- **Unified toolchain** (`kdality`): compile, init, lint, simulate, dt-gen, test-gen
- **VS Code extension**: syntax highlighting + snippets for `.kdh`/`.kdc`
- **8 CMake modules**: FindKDAL, Config, Targets, CompileKDC, Toolchain, Testing, Packaging

## Quick Start

```sh
# Install dependencies (macOS or Linux)
./scripts/devsetup/install_deps.sh

# Prepare QEMU workspace
./scripts/setupqemu/prepare.sh

# Build kernel with KDAL support
./scripts/setupqemu/buildkernel.sh

# Build the userspace tool + compiler
make all

# Launch QEMU guest
./scripts/setupqemu/run.sh

# Inside guest: load module and test
insmod /mnt/shared/kdal.ko
/mnt/shared/kdality version
/mnt/shared/kdality list
```

## Architecture

```
 ┌───────────────────────────────────────────────┐
 │                  Userspace                    │
 │  kdality · testapp · benchmark · kdaltest     │
 ├──────────────────── /dev/kdal ────────────────┤
 │               Control Plane                   │
 │         chardev (ioctl dispatch)              │
 │         debugfs (introspection)               │
 ├───────────────────────────────────────────────┤
 │                Core Runtime                   │
 │    registry · events · power · lifecycle      │
 ├───────────────────────────────────────────────┤
 │              Driver Layer                     │
 │    uart · i2c · spi · gpu (+ accel ops)       │
 ├───────────────────────────────────────────────┤
 │             Backend Layer                     │
 │   QEMU ring · virtio · MMIO · platdev · DT    │
 └───────────────────────────────────────────────┘
```

See [docs/architecture.md](docs/architecture.md) for full details.

## Repository Layout

```
include/kdal/       Public API headers (types, ioctl, driver, accel, backend)
src/core/           Core runtime (lifecycle, registry, events, chardev, debugfs)
src/drivers/        Device drivers (UART, I2C, SPI, GPU)
src/backends/       Hardware backends (QEMU, generic)
compiler/           KDAL compiler (kdalc): lexer, parser, sema, codegen
lang/grammar/       EBNF grammars for .kdh and .kdc
lang/stdlib/        Standard device headers (7 .kdh files)
lang/               Language specification + design docs
tools/kdality/      Unified CLI + compiler frontend (kdality)
editor/vscode/      VS Code extension for .kdh/.kdc
cmake/              8 CMake modules for build integration
tests/kunit/        In-kernel tests (38 test cases)
tests/userspace/    Userspace smoke test + benchmark
tests/integration/  Multi-device integration suite
scripts/            Build, QEMU, CI, and release scripts
docs/               Architecture, API, language guide, porting, thesis
examples/           Driver examples (.kdc, plain C, accel demos)
```

## Building

### Kernel Module (cross-compile for aarch64)

```sh
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
make KDIR=/path/to/linux-6.6 modules
```

### Userspace Tool + Compiler

```sh
make all      # builds compiler + kdality + module
make tool     # builds kdality only
make compiler # builds kdalc + libkdalc.a only
```

### Tests

```sh
# Run offline CI suite (builds + compiles + dry-run tests)
./scripts/ci/test.sh

# Static analysis
./scripts/ci/static_analysis.sh

# Style check
./scripts/ci/checkpatch.sh
```

## Documentation

| Document                                 | Description                             |
| ---------------------------------------- | --------------------------------------- |
| [Architecture](docs/architecture.md)     | 6-layer design, data flow, init cascade |
| [API Reference](docs/api_reference.md)   | All functions, ioctl commands, types    |
| [Language Guide](docs/language_guide.md) | Tutorial: .kdh/.kdc from scratch        |
| [Language Spec](lang/spec.md)            | Formal language specification           |
| [Language Design](lang/DESIGN.md)        | Design rationale and comparison table   |
| [Installation](docs/installation.md)     | Setup guide for macOS and Linux         |
| [Porting Guide](docs/portingguide.md)    | How to add a new backend                |
| [Performance](docs/performance.md)       | Benchmarking methodology and metrics    |
| [FAQ](docs/faq.md)                       | Common questions answered               |
| [Upstreaming](docs/upstreaming.md)       | Path to Linux kernel mainline           |
| [Thesis](docs/thesis.md)                 | Chapter mapping to repository artifacts |

## Target Platforms

| Platform            | Status          | Backend               |
| ------------------- | --------------- | --------------------- |
| QEMU aarch64 `virt` | **Implemented** | `qemu` (ring buffer)  |
| Radxa Orion O6      | Planned         | `generic` (MMIO + DT) |

## License

GPLv3 — see [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
