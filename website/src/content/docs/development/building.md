---
title: Building from Source
description: How to build the KDAL kernel module, compiler, and CLI tool from source.
---

## Prerequisites

| Dependency                      | Minimum Version            | Purpose                        |
| ------------------------------- | -------------------------- | ------------------------------ |
| GCC                             | 12+                        | Kernel module & compiler build |
| GNU Make                        | 4.0+                       | Build orchestration            |
| Linux kernel headers            | 6.1+ (6.6 LTS recommended) | Module compilation             |
| CMake                           | 3.20+                      | Compiler and tool build        |
| QEMU (optional)                 | 8.0+                       | Testing without real hardware  |
| `aarch64-linux-gnu-` (optional) | -                          | Cross-compilation for ARM64    |

### Install dependencies

**macOS:**

```bash
brew install cmake qemu aarch64-elf-gcc
```

**Debian / Ubuntu:**

```bash
sudo apt install build-essential cmake linux-headers-$(uname -r) \
     qemu-system-arm gcc-aarch64-linux-gnu
```

**Fedora:**

```bash
sudo dnf install cmake kernel-devel qemu-system-aarch64 \
     gcc-aarch64-linux-gnu
```

## Build the Compiler

```bash
./scripts/dev/build.sh --variant release kdalc
```

This builds both the library (`libkdalc.a`) and the standalone compiler
binary (`kdalc`) under `build/release/compiler/`.

## Build the CLI Tool

```bash
./scripts/dev/build.sh --variant release kdality
```

This places `kdality` under `build/release/`.

## Build the Kernel Module

```bash
# Cross-compile for aarch64
KDAL_KERNEL_DIR=/path/to/linux-6.6 ./scripts/dev/build.sh --variant debug module
```

## Build Everything

The project uses **CMake 3.20+**, wrapped by convenience scripts under
`scripts/dev/`:

```bash
./scripts/dev/build.sh --variant release all      # compiler + tool + website + vscode
./scripts/dev/build.sh --variant debug kdalc      # debug compiler build
./scripts/dev/build.sh --variant release kdality  # optimized kdality
./scripts/dev/build.sh --variant release website  # documentation site → build/release/website/
./scripts/dev/build.sh --variant release vscode   # VS Code extension → build/release/editor/vscode/
./scripts/dev/build.sh --variant debug module     # kernel module (requires KDAL_KERNEL_DIR)
./scripts/dev/build.sh --variant release install  # install SDK (cmake --install)
./scripts/dev/build.sh --variant asan clean       # remove asan artifacts
```

### Build tree roles

- `build/release/`: optimized local SDK/toolchain build; use for install, packaging, and normal local runs.
- `build/debug/`: symbols + assertions; use when stepping through `kdalc`, `kdality`, or generated CMake targets.
- `build/asan/`: AddressSanitizer-enabled debug build; use for heap, stack, and use-after-free investigations.
- `build/ci/release/`: CI-only verification tree; keeps automation output separate from developer builds.
- `build/nightly/<os-arch>/`: nightly matrix tree per host/architecture; avoids stale cache reuse across targets.
- `build/release/<os-arch>/`: release packaging tree per artifact target in the release matrix.

### Install targets

```bash
sudo ./scripts/dev/build.sh --variant release install   # installs to /usr/local/
```

To uninstall:

```bash
cmake --build --preset release --target uninstall
```

Installed layout:

```
<PREFIX>/
├── bin/         kdalc, kdality
├── include/     kdal/ headers
├── lib/         libkdalc.a
├── share/kdal/
│   ├── stdlib/  .kdh standard library
│   └── vim/     Vim plugin files
└── lib/cmake/   CMake package files
```

## Verify the Build

```bash
# Check compiler
./build/release/compiler/kdalc -h

# Check tool
./build/release/kdality -h

# Lint a sample file
./build/release/kdality lint lang/stdlib/uart.kdh

# Run all tests
./scripts/dev/test.sh --variant release all
```

## QEMU Testing

Set up the QEMU aarch64 environment:

```bash
# Create guest rootfs and download the cloud image
./scripts/env/prepare.sh

# Build the guest kernel image
./scripts/env/kernel.sh

# Boot QEMU with shared directory
./scripts/env/run.sh

# Inside QEMU guest:
insmod /mnt/shared/kdal.ko
dmesg | grep kdal
./kdality list
```

## CMake Modules

The KDAL SDK includes CMake integration for projects that use CMake:

```cmake
find_package(KDAL REQUIRED)

kdal_compile(my_driver.kdc OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated)
kdal_lint(my_driver.kdc STRICT)
kdal_test_gen(my_driver.kdc OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests)
```

CMake modules are installed to `<PREFIX>/lib/cmake/KDAL/`.
