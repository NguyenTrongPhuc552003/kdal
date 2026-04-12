# FAQ

## Architecture

### Why QEMU first instead of real hardware?

QEMU provides a **deterministic, reproducible** environment that eliminates
hardware variance during architecture validation. Every developer gets the
exact same execution environment regardless of whether they have physical
hardware. This is critical for a thesis project where results must be
reproducible.

The QEMU aarch64 `virt` machine also matches the target ISA (ARM64), so
drivers developed on QEMU require zero code changes on real hardware - only
a backend swap.

### Why not support every device class immediately?

The first milestone validates the **abstraction boundary**, not the device
coverage. Starting with four classes (UART, I2C, SPI, GPU) proves the
framework can handle both simple serial peripherals and complex accelerators.
Adding GPIO, NPU, or others later exercises only the porting guide - the
core remains unchanged.

### Why a thin core?

Portability depends on **stable interfaces**, not on absorbing device details
into a monolithic framework. The core is ~600 lines managing lifecycle,
registry, and events. All hardware logic lives in drivers and backends.
This keeps the trusted surface small and auditable.

### Why out-of-tree instead of in-tree?

Out-of-tree development allows:
- Faster iteration without kernel patch review cycles
- GPLv3 licensing (in-tree requires GPLv2-only)
- Easy deployment via `insmod` without kernel rebuilds
- Clear separation for thesis evaluation

The architecture is designed for future upstreaming - see
[upstreaming.md](upstreaming.md).

## Implementation

### How does device selection work for userspace?

KDAL exposes a single `/dev/kdal` character device. Userspace selects a
device via the `KDAL_IOCTL_SELECT_DEV` ioctl, which stores the selection
in per-fd state (`file->private_data`). Subsequent `read()`/`write()` calls
are routed to the selected device's driver. Multiple processes can open
`/dev/kdal` independently, each with their own device selection.

### Why `misc_register` instead of a custom major number?

Automatic minor number allocation via `misc_register()` avoids conflicts
with other drivers and simplifies deployment. A custom major number offers
no benefit for a single control device.

### How is the ring buffer sized?

The QEMU backend allocates a 4096-byte ring buffer per device. This balances
memory usage with throughput for the emulated I/O path. Real hardware backends
would use DMA buffers or MMIO regions instead.

### Why are KUnit tests separate from integration tests?

KUnit tests run inside the kernel at boot (or module load) and test internal
APIs without userspace. Integration tests run from userspace and exercise the
full ioctl path through `/dev/kdal`. Both are needed: KUnit catches internal
regressions, integration tests validate the end-to-end contract.

## Development

### How do I build the userspace tool without kernel headers?

The tool (`tools/kdality/kdalctl.c`) mirrors the ioctl ABI definitions
locally and does not depend on kernel headers. Build with:

```sh
./scripts/dev/build.sh --variant release kdality
```

### How do I add a new device class?

1. Add the class enum to `include/kdal/types.h`
2. Create a driver under `src/drivers/example/`
3. Wire init/exit into `src/core/kdal.c`
4. Add KUnit tests
5. Update `kdality` (kdalctl.c) to display the new class name

### What kernel versions are supported?

KDAL targets **Linux 6.6 LTS**. The API surface (`misc_register`, `debugfs`,
`copy_to_user/copy_from_user`, `mutex`, `list_head`) has been stable since
Linux 4.x, so backporting is straightforward.

## Language Ecosystem

### What are `.kdh` and `.kdc` files?

`.kdh` (KDAL Device Header) files are **declarative** descriptions of a
hardware device - register maps, signal definitions, capabilities, and power
states. `.kdc` (KDAL Driver Code) files contain **imperative** event
handlers that implement driver logic (probe, read, write, signal, power).
Together they separate *what a device is* from *how it behaves*.

### How does the KDAL compiler work?

The compiler (`kdalc`) reads a `.kdc` file plus its referenced `.kdh`
header, then runs four pipeline stages:

1. **Lexer** - tokenizes source into 80+ token types
2. **Parser** - builds an arena-allocated AST via recursive descent
3. **Semantic analysis** - resolves symbols, validates register accesses
4. **Code generation** - emits idiomatic kernel C + `Makefile.kbuild`

The output is ready to compile with any kernel build tree.

### Why not just write C directly?

KDAL DSL reduces a typical driver from 200+ lines of boilerplate C to
~30 lines of intent. The compiler handles `platform_driver` registration,
OF match table generation, `devm_*` resource management, and error unwinding.
New programmers can focus on *driver logic* rather than kernel API minutiae.

### Can I write a driver without knowing C?

Yes - that is the goal. A `.kdc` file uses high-level constructs like
`on probe`, `on read`, and `reg_write(CTRL, 0x01)`. The compiler generates
all the C code needed for a loadable kernel module. You can inspect the
generated C to learn kernel programming incrementally.

### How do I create a custom device header?

Create a `.kdh` file in `lang/stdlib/` (or anywhere the compiler can find
it) following the format in `lang/spec.md`. A minimal `.kdh` file declares
the device name, a register map, and at least one capability. Then reference
it from your `.kdc` with `use "mydevice.kdh";`.
