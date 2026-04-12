---
title: Design Rationale
description: Why KDAL exists and the reasoning behind its language and compiler design.
---

## Motivation

Every major embedded framework eventually invents its own file format:

| Framework    | File format                       | Compile target                |
| ------------ | --------------------------------- | ----------------------------- |
| Arduino      | `.ino`                            | AVR / ARM bare-metal          |
| PlatformIO   | `.ino` / `.cpp` with platform env | MCU bare-metal                |
| Zephyr RTOS  | `.c` + `.overlay` (DTS)           | RTOS kernel module            |
| Linux kernel | `.c` + Kbuild `Makefile`          | Kernel module `.ko`           |
| **KDAL**     | **`.kdc` + `.kdh`**               | **Linux kernel module `.ko`** |

The Linux kernel has no domain-specific abstraction layer for writing device
drivers that:

- Separates **what a device looks like** (register map, signals) from **how the driver behaves** (event handlers)
- Enforces **safe register-access rules** at compile time
- Allows a **single source file** to target multiple backends (MMIO, platform_device, virtio, DT)
- Is **auditable** by a reviewer who does not know C

KDAL fills this gap.

## Language Philosophy

### Declarative at the top, imperative at the edges

A `.kdh` file is purely declarative: it describes hardware registers, signal
lines, capabilities, and power states. It contains no executable logic.

A `.kdc` driver is mostly declarative (annotations and config) but contains
imperative event handlers where real work happens. This mirrors how hardware
engineers think: "the device has these registers; when this event occurs, do
this sequence."

### Intentionally not Turing-complete

KDAL rejects:

| Construct          | Reason                                                  |
| ------------------ | ------------------------------------------------------- |
| Recursion          | Call depth bounded at 1 (handler calls built-ins only)  |
| Heap allocation    | All state lives in a C struct allocated once by `probe` |
| Unbounded loops    | Only `for i in 0..N` with compile-time constant N       |
| Pointer arithmetic | Register access goes through typed paths                |
| Inline assembly    | No escape hatch to machine code                         |

This keeps generated code auditable and eliminates entire classes of kernel
bugs (memory corruption, double-free, stack overflow, IRQ reentrancy).

### Self-hosting in C

The KDAL compiler (`kdalc`) is written in C. This is intentional:

- The compiler must run in any kernel build environment (which guarantees a C compiler)
- C is the language of the kernel - no bootstrap dependency
- Generated output is idiomatic C that a Linux kernel reviewer can read without knowing KDAL

## File Format Decisions

### Why `.kdh` and `.kdc` (not `.kdal`)?

The two-file split separates concerns:

- `.kdh` is **shareable** - describes a hardware class, reused by many drivers
- `.kdc` is the **implementation** - one per driver, imports `.kdh` files
- Mirrors the C `.h` / `.c` split with semantic difference explicit in the extension

### Why no `while` loop?

`while` loops can spin indefinitely in kernel context (IRQ or softirq),
causing system hangs. `for i in 0..N` guarantees termination at a glance.
The `wait(signal, ms)` built-in replaces `while (!ready) {}` with a proper
kernel wait queue.

### Why is `log` a built-in?

`pr_debug` and `dev_dbg` require a `struct device *` pointer. The compiler
injects the correct pointer from the driver context struct, preventing the
common bug of calling `pr_err` without a device context.

### Why not Rust?

Rust is a valid kernel language (Linux 6.1+). The decision to target C for
KDAL 0.x is pragmatic:

- Rust in-kernel toolchains are still stabilising (bindgen, macros)
- The KDAL runtime is C - C codegen avoids cross-language FFI
- Rust-as-target is listed as future work

## Comparison with Alternatives

| Feature             | C + Kbuild   | Arduino           | Zephyr RTOS     | **KDAL**                 |
| ------------------- | ------------ | ----------------- | --------------- | ------------------------ |
| Target              | Linux `.ko`  | Bare-metal        | RTOS `.ko`      | **Linux `.ko`**          |
| Register safety     | Manual       | Manual            | Manual          | **Compile-time**         |
| Backend abstraction | Manual       | None              | DTS + Kconfig   | **`backend` annotation** |
| Signal/IRQ model    | Manual       | `attachInterrupt` | `gpio_callback` | **`on signal` handler**  |
| Power management    | PM framework | None              | `pm_device`     | **`on power` handler**   |
| Learning curve      | High         | Low               | Medium          | **Low–Medium**           |
| Auditability        | Moderate     | Low               | Moderate        | **High**                 |
| Turing-complete     | Yes          | Yes               | Yes             | **No (by design)**       |

## Compiler Architecture

```
.kdc → Lexer → Tokens → Parser → AST → Sema → Codegen → .c + Makefile.kbuild
```

Each phase is a separate `.c` file. Phases communicate via the `kdal_ast_t`
struct hierarchy in `compiler/include/ast.h`. See the
[Compiler](/toolchain/compiler/) page for full details.

## Standard Library

7 pre-built `.kdh` files in `lang/stdlib/`:

| File         | Device class     | Notable registers                  |
| ------------ | ---------------- | ---------------------------------- |
| `common.kdh` | Primitive types  | -                                  |
| `uart.kdh`   | UART 16550       | DR, FR, IBRD, FBRD, LCRH, CR, IMSC |
| `i2c.kdh`    | I2C (DesignWare) | CON, TAR, DATA_CMD, SS/FS_SCL      |
| `spi.kdh`    | SPI (PL022)      | CR0, CR1, DR, SR, CPSR             |
| `gpio.kdh`   | Generic GPIO     | DIR, DATA, IS, IBE, IEV, IE        |
| `gpu.kdh`    | Abstract GPU     | SUBMIT, STATUS, RESET, FENCE       |
| `npu.kdh`    | Abstract NPU     | MODEL_LOAD, INFER_CMD, STATUS      |

## Future Work

- **v0.2** - `async` event handlers (work-queue dispatch)
- **v0.3** - Auto-generate debugfs from register declarations
- **v0.4** - KUnit test generation from `.kdh` type information
- **v0.5** - Rust codegen target (`codegen_rust.c`)
