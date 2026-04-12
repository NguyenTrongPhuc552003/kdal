# Installation

## Quick Install (Recommended)

Install the KDAL SDK with one command:

```sh
curl -fsSL https://raw.githubusercontent.com/NguyenTrongPhuc552003/kdal/main/scripts/installer/install.sh | sh
```

This downloads and runs `kdalup`, the KDAL version manager. It installs:

| Component          | Path                         |
| ------------------ | ---------------------------- |
| `kdalc` compiler   | `~/.kdal/bin/kdalc`          |
| `kdality` CLI tool | `~/.kdal/bin/kdality`        |
| `kdalup` manager   | `~/.kdal/bin/kdalup`         |
| Kernel headers     | `~/.kdal/include/kdal/`      |
| Compiler headers   | `~/.kdal/include/kdalc/`     |
| `libkdalc.a`       | `~/.kdal/lib/libkdalc.a`     |
| Standard library   | `~/.kdal/share/kdal/stdlib/` |
| CMake package      | `~/.kdal/lib/cmake/KDAL/`    |

After install, restart your shell or run:

```sh
export PATH="$HOME/.kdal/bin:$PATH"
```

### Managing the SDK

```sh
kdalup version                       # Show installed version
kdalup list                          # List all components
kdalup update                        # Update to latest release
kdalup self-update                   # Update kdalup itself
kdalup install v0.2.0                # Install a specific version
kdalup install --profile full        # Install everything (headers, cmake, vim)
kdalup install --profile minimal     # Install just compiler + CLI + stdlib
kdalup doctor                        # Verify installation health
kdalup uninstall                     # Remove everything
```

### Custom install location

```sh
KDAL_HOME=/opt/kdal curl -fsSL .../install.sh | sh
```

## Prerequisites (Development from Source)

- **macOS** (M1/M2/M3/M4) or **Linux** (x86_64 or aarch64)
- QEMU with aarch64 system emulation
- Cross-compiler for aarch64 (if host is not aarch64)
- Linux 6.6 LTS kernel source (for module build)

## Build from Source

```sh
# 1. Install host dependencies
./scripts/env/dependencies.sh

# 2. Prepare QEMU environment
./scripts/env/prepare.sh

# 3. Build guest kernel with KUnit and module support
./scripts/env/kernel.sh

# 4. Build everything (compiler + CLI + website + VS Code extension)
./scripts/dev/build.sh --variant release all

# 5. Run tests
./scripts/dev/test.sh --variant release all

# 6. Launch QEMU and load module
cp kdal.ko $HOME/kdal-qemu/shared/modules/
./scripts/env/run.sh

# Inside QEMU guest:
mount -t 9p -o trans=virtio kdal-share /mnt
insmod /mnt/modules/kdal.ko
```

## macOS (Apple Silicon)

```sh
# Install via Homebrew
brew install qemu aarch64-elf-gcc dtc cppcheck

# Verify
qemu-system-aarch64 --version
```

## Linux (Debian/Ubuntu)

```sh
sudo apt install build-essential bc flex bison libelf-dev libssl-dev \
    gcc-aarch64-linux-gnu qemu-system-arm device-tree-compiler \
    cppcheck sparse
```

## Build System

The project uses **CMake 3.20+**, wrapped by convenience scripts:

| Command                                            | Description                                          |
| -------------------------------------------------- | ---------------------------------------------------- |
| `./scripts/dev/build.sh --variant release all`     | Build compiler + CLI + website + vscode              |
| `./scripts/dev/build.sh --variant debug kdalc`     | Build `kdalc` compiler and `libkdalc.a` with symbols |
| `./scripts/dev/build.sh --variant release kdality` | Build `kdality` CLI (links compiler)                 |
| `./scripts/dev/build.sh --variant release website` | Build documentation site                             |
| `./scripts/dev/build.sh --variant release vscode`  | Build VS Code extension (.vsix)                      |
| `./scripts/dev/build.sh --variant debug module`    | Build kernel module (requires `KDAL_KERNEL_DIR`)     |
| `./scripts/dev/build.sh --variant asan clean`      | Remove AddressSanitizer build artifacts              |
| `./scripts/dev/build.sh --variant release install` | Install to system (cmake --install)                  |
| `./scripts/dev/test.sh --variant release all`      | Run full test suite with reports                     |
| `./scripts/dev/test.sh help`                       | List all testable targets                            |

Build outputs are collected under variant-specific subtrees in `build/`:

```
build/
├── release/           Optimized local build
├── debug/             Symbols + assertions for debugging
├── asan/              AddressSanitizer build
├── ci/release/        CI-only verification tree
└── nightly/<os-arch>/ Nightly matrix tree
```

## Compiling `.kdc` Files

Once the toolchain is built, compile a KDAL driver source into a loadable
kernel module:

```sh
# Compile a .kdc file into C + Makefile.kbuild
./build/release/kdality compile examples/kdc_hello/uart_hello.kdc -o output/

# Or use the standalone compiler directly
./build/release/compiler/kdalc examples/kdc_hello/uart_hello.kdc -o output/

# Then build the generated kernel module
make -C /path/to/kernel M=$(pwd)/output modules
```

The compiler reads `.kdh` device headers from `lang/stdlib/` and generates
idiomatic kernel C code with a matching `Makefile.kbuild`.

## Verifying the Module

Once QEMU is running with the module loaded:

```sh
# Check module loaded
lsmod | grep kdal

# Check char device created
ls -la /dev/kdal

# Check debugfs
cat /sys/kernel/debug/kdal/version
cat /sys/kernel/debug/kdal/devices
cat /sys/kernel/debug/kdal/snapshot

# Use kdality runtime
./build/release/kdality version
./build/release/kdality list
./build/release/kdality info uart0
./build/release/kdality write uart0 "hello from KDAL"
./build/release/kdality read uart0 16

# Use kdality compiler
./build/release/kdality compile examples/kdc_hello/uart_hello.kdc -o output/
```

## Troubleshooting

| Problem                | Solution                                     |
| ---------------------- | -------------------------------------------- |
| `/dev/kdal` not found  | Check `dmesg \| grep kdal` for init errors   |
| Module won't load      | Verify kernel version matches build headers  |
| `ENODEV` on device ops | Ensure backend initialized before drivers    |
| Permission denied      | Use `sudo` or set udev rules for `/dev/kdal` |
