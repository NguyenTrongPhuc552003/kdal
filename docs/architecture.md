# Architecture

## Overview

KDAL (Kernel Device Abstraction Layer) is a six-layer kernel framework plus
a domain-specific language (DSL) toolchain that decouples device drivers from
hardware specifics, enabling portable driver development across embedded
platforms.

```
┌────────────────────────────────────────────┐
│          Language Toolchain (kdalc)         │
│  .kdh ──→ .kdc ──→ lexer ──→ parser ──→   │
│  sema ──→ codegen ──→ .c + Makefile.kbuild │
├────────────────────────────────────────────┤
│            Userspace Applications          │
│      (kdality, testapp, custom programs)   │
├────────────────────────────────────────────┤
│          /dev/kdal (char device)           │
│     ioctl: version, list, info, power,     │
│            select, read, write             │
├────────────────────────────────────────────┤
│              KDAL Core Runtime             │
│  ┌─────────┬──────────┬────────┬────────┐  │
│  │chardev.c│debugfs.c │event.c │power.c │  │
│  ├─────────┼──────────┼────────┼────────┤  │
│  │      registry.c    │   kdal.c        │  │
│  └─────────────────────┴────────────────┘  │
├────────────────────────────────────────────┤
│             Backend Adapters               │
│  ┌──────────────┐  ┌───────────────────┐   │
│  │  qemu-virt   │  │ generic-platdev   │   │
│  │ (ring buffer)│  │ (MMIO/DT/SoC)     │   │
│  └──────────────┘  └───────────────────┘   │
├────────────────────────────────────────────┤
│              Device Drivers                │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐               │
│  │UART│ │I2C │ │SPI │ │GPU │               │
│  └────┘ └────┘ └────┘ └────┘               │
└────────────────────────────────────────────┘
```

## Layer Responsibilities

### Layer 0: Language Toolchain (`compiler/`, `lang/`)

The KDAL DSL separates device description from driver behavior:

- **`.kdh` device headers** — declarative register maps, signals, capabilities, power states
- **`.kdc` driver files** — imperative event handlers (probe, read, write, signal, power)
- **Compiler pipeline** — lexer → parser → AST → semantic analysis → C code generation
- **Standard library** — 7 pre-built `.kdh` files covering UART, I2C, SPI, GPIO, GPU, NPU
- **Output** — idiomatic kernel C (`platform_driver`, `devm_*`, OF match table) + `Makefile.kbuild`

```
.kdc → Lexer → Tokens → Parser → AST → Sema → Codegen → .c + Makefile.kbuild → kbuild → .ko
```

### Layer 1: Public Headers (`include/kdal/`)

Stable API contracts that drivers and backends program against:

- **`types.h`** — device classes, power states, event types, core structs
- **`api/driver.h`** — conventional driver ops (probe/remove/read/write/ioctl)
- **`api/accel.h`** — accelerator ops (queue/buffer/submit)
- **`backend.h`** — backend ops (init/exit/enumerate/read/write/ioctl)
- **`ioctl.h`** — userspace ABI with versioned command numbers
- **`core/kdal.h`** — lifecycle, registration, and lookup functions

### Layer 2: Core Runtime (`src/core/`)

Subsystem orchestration and infrastructure:

- **`module.c`** — kernel module entry/exit
- **`kdal.c`** — init cascade (chardev → debugfs → backends → drivers)
- **`chardev.c`** — `/dev/kdal` misc device with per-fd state
- **`debugfs.c`** — `/sys/kernel/debug/kdal/` diagnostic files
- **`registry.c`** — thread-safe registration of backends/drivers/devices
- **`event.c`** — circular buffer event log with waitqueue
- **`power.c`** — power state transitions with event emission

### Layer 3: Backend Adapters (`src/backends/`)

Transport-specific implementations:

- **`qemu/`** — QEMU virt machine backend with software ring buffers
- **`generic/`** — platform device, Device Tree, and SoC glue for real hardware

### Layer 4: Device Drivers (`src/drivers/`)

Self-registering drivers that discover backends via the registry:

- **UART** — byte-stream I/O with configurable baud rate
- **I2C** — bus-speed-aware peripheral driver
- **SPI** — full-duplex SPI emulation
- **GPU** — accelerator driver with queue/buffer semantics

### Layer 5: Verification (`tests/`, `tools/`)

- KUnit test suites for kernel-side validation
- Userspace integration tests and benchmarks
- `kdality` CLI for device management and driver compilation

## Data Flow

```
write(fd, "hello", 5)
  → kdal_cdev_write()           [chardev.c]
  → file_ctx->selected->driver->ops->write()  [uartdriver.c]
  → copy_from_user() into kernel buffer
  → backend->ops->write()       [qemubackend.c]
  → qemu_ring_write()           [ring buffer]

read(fd, buf, 5)
  → kdal_cdev_read()            [chardev.c]
  → file_ctx->selected->driver->ops->read()   [uartdriver.c]
  → backend->ops->read()        [qemubackend.c]
  → qemu_ring_read()            [ring buffer]
  → copy_to_user() back to userspace
```

## Init Cascade

Module load triggers `kdal_core_init()` which brings up subsystems in order:

1. `kdal_chardev_init()` — registers `/dev/kdal`
2. `kdal_debugfs_init()` — creates debugfs tree (non-fatal if unavailable)
3. `kdal_qemu_backend_init()` — registers + inits QEMU backend
4. `kdal_uart_driver_init()` — registers driver + device + attaches
5. `kdal_i2c_driver_init()` — same pattern
6. `kdal_spi_driver_init()` — same pattern
7. `kdal_gpu_driver_init()` — same pattern

Teardown in `kdal_core_exit()` reverses this order.

## Design Decisions

| Decision                              | Rationale                                |
| ------------------------------------- | ---------------------------------------- |
| `misc_register` over raw char dev     | Simpler, automatic minor allocation      |
| Per-fd state via `file->private_data` | Independent device selection per open    |
| Ring buffer over DMA                  | No real hardware on QEMU first milestone |
| Mutex over spinlock in registry       | Sleep-safe for module init path          |
| Circular event log                    | Bounded memory, no allocation per event  |
| Forward-declared driver inits         | Avoids header coupling between drivers   |
