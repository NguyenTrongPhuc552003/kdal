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
./scripts/env/dependencies.sh

# Prepare QEMU workspace
./scripts/env/prepare.sh

# Build kernel with KDAL support
./scripts/env/kernel.sh

# Build everything (compiler + CLI + website + VS Code extension)
./scripts/dev/build.sh --variant release all

# Launch QEMU guest
./scripts/env/run.sh

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
editor/vim/         Vim/Neovim plugin for .kdh/.kdc
cmake/              8 CMake modules for build integration
tests/kunit/        In-kernel tests (38 test cases)
tests/userspace/    Userspace smoke test + benchmark
tests/integration/  Multi-device integration suite
scripts/dev/        Development scripts (build, run, test)
scripts/e2e/        End-to-end test suites
scripts/ci/         CI/CD helper scripts
scripts/installer/  SDK installer (kdalup)
docs/               Architecture, API, language guide, porting, thesis
website/            Documentation site (Astro Starlight)
examples/           Driver examples (.kdc, plain C, accel demos)
build/              Variant-specific build trees and generated outputs
```

## Building

The project uses **CMake 3.20+** as its build system, wrapped by convenience
scripts under `scripts/dev/`.

### Development Scripts

```sh
./scripts/dev/build.sh --variant <name> <target>   # Build targets
./scripts/dev/run.sh --variant <name> <target>     # Run targets (auto-builds if needed)
./scripts/dev/test.sh --variant <name> <target>    # Test targets with PASS/FAIL reporting
```

### Build Variants

- `release`: optimized local SDK build in `build/release/`; use for packaging, install checks, and normal local use.
- `debug`: symbols + assertions in `build/debug/`; use for compiler or CLI debugging.
- `asan`: AddressSanitizer-enabled build in `build/asan/`; use for memory-safety investigations.

Use `--variant` for normal local development. Use `--preset` only for automation-oriented trees such as `ci-release`, `nightly`, and `release-matrix`.

### Build Targets

```sh
./scripts/dev/build.sh --variant release kdalc   # Compiler (kdalc + libkdalc.a)
./scripts/dev/build.sh --variant release kdality # CLI toolchain
./scripts/dev/build.sh --variant release website # Documentation site → build/release/website/
./scripts/dev/build.sh --variant release vscode  # VS Code extension → build/release/editor/vscode/
./scripts/dev/build.sh --variant debug module    # Kernel module iteration build
./scripts/dev/build.sh --variant release all     # Everything above
./scripts/dev/build.sh --variant asan clean      # Remove the asan build tree
./scripts/dev/build.sh --variant release install # Install to system (cmake --install)
```

### Kernel Module (cross-compile for aarch64)

```sh
export KDAL_KERNEL_DIR=/path/to/linux-6.6
export CROSS_COMPILE=aarch64-linux-gnu-
export ARCH=arm64
./scripts/dev/build.sh --variant debug module
```

### Tests

```sh
# Run all tests (mirrors CI stages locally)
./scripts/dev/test.sh --variant release all

# Test specific targets
./scripts/dev/test.sh --variant release kdalc       # Compiler binary + sample compilation
./scripts/dev/test.sh --variant release kdality     # CLI subcommands
./scripts/dev/test.sh --variant release website     # Documentation site build
./scripts/dev/test.sh --variant release vscode      # VS Code extension packaging
./scripts/dev/test.sh --variant release cppcheck    # C static analysis (mirrors CI)
./scripts/dev/test.sh --variant release shellcheck  # Shell script linting (mirrors CI)
./scripts/dev/test.sh --variant release structure   # Project structure validation
./scripts/dev/test.sh --variant release e2e         # End-to-end test suites
./scripts/dev/test.sh help        # List all testable targets
```

Test reports are saved under the selected variant tree, for example `build/debug/test/<target>_<date>.log`.

### Build Output Layout

```
build/
├── release/           Optimized local build and install validation
│   ├── compiler/      kdalc, libkdalc.a
│   ├── website/       Documentation site dist
│   ├── editor/vscode/ .vsix package
│   └── test/          Test reports
├── debug/             Debuggable local build with symbols
├── asan/              AddressSanitizer build
├── ci/release/        CI-only verification build
└── nightly/<os-arch>/ Nightly matrix builds per target host/arch
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

## Testing Without Hardware

KDAL v0.1.0 is **QEMU-validated only** - no real hardware is required.

### QEMU workflow

```sh
./scripts/env/dependencies.sh   # Install host tools
./scripts/env/prepare.sh        # Download Ubuntu cloud image + create QEMU workspace
./scripts/env/kernel.sh         # Cross-compile Linux 6.6 with KUnit + virtio
./scripts/env/run.sh            # Boot QEMU aarch64 virt machine
# Inside guest: insmod /mnt/shared/kdal.ko
```

### Docker (zero-setup)

```sh
docker build -t kdal-dev -f Dockerfile.dev .
docker run -it -v $(pwd):/workspace kdal-dev
# Inside container: cmake --preset ci-release && cmake --build --preset ci-release
```

The container includes cmake, gcc, aarch64 cross-compiler, QEMU, cppcheck, shellcheck, and Node.js.

## License

GPLv3 - see [LICENSE](LICENSE).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
