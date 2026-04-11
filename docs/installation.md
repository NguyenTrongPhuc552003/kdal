# Installation

## Prerequisites

- **macOS** (M1/M2/M3/M4) or **Linux** (x86_64 or aarch64)
- QEMU with aarch64 system emulation
- Cross-compiler for aarch64 (if host is not aarch64)
- Linux 6.6 LTS kernel source (for module build)

## Quick Start

```sh
# 1. Install host dependencies
./scripts/devsetup/install_deps.sh

# 2. Prepare QEMU environment
./scripts/setupqemu/prepare.sh

# 3. Build guest kernel with KUnit and module support
./scripts/setupqemu/buildkernel.sh

# 4. Build KDAL module
make KDIR=$HOME/kdal-qemu/kernel/linux-6.6.80 \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- module

# 5. Build userspace tool
make tool

# 6. Run smoke tests
make test

# 7. Launch QEMU and load module
cp kdal.ko $HOME/kdal-qemu/shared/modules/
./scripts/setupqemu/run.sh

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

| Command         | Description                             |
| --------------- | --------------------------------------- |
| `make all`      | Build compiler + CLI tool               |
| `make module`   | Build kernel module (requires `KDIR`)   |
| `make compiler` | Build `kdalc` compiler and `libkdalc.a` |
| `make tool`     | Build `kdality` CLI (links compiler)    |
| `make test`     | Run smoke tests                         |
| `make lint`     | Run style and static analysis           |
| `make clean`    | Remove all build artifacts              |
| `make docs`     | Show documentation location             |

## Compiling `.kdc` Files

Once the toolchain is built, compile a KDAL driver source into a loadable
kernel module:

```sh
# Compile a .kdc file into C + Makefile.kbuild
./kdality compile examples/kdc_hello/uart_hello.kdc -o output/

# Or use the standalone compiler directly
./kdalc examples/kdc_hello/uart_hello.kdc -o output/

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
./kdality version
./kdality list
./kdality info uart0
./kdality write uart0 "hello from KDAL"
./kdality read uart0 16

# Use kdality compiler
./kdality compile examples/kdc_hello/uart_hello.kdc -o output/
```

## Troubleshooting

| Problem                | Solution                                     |
| ---------------------- | -------------------------------------------- |
| `/dev/kdal` not found  | Check `dmesg \| grep kdal` for init errors   |
| Module won't load      | Verify kernel version matches build headers  |
| `ENODEV` on device ops | Ensure backend initialized before drivers    |
| Permission denied      | Use `sudo` or set udev rules for `/dev/kdal` |
