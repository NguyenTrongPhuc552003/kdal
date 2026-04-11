# KDAL Language Design Document

**Version:** 0.1  
**Status:** Draft — Internal Design Notes

---

## 1. Motivation

Every major embedded framework eventually invents its own file format:

| Framework    | File format                       | Compile target            |
| ------------ | --------------------------------- | ------------------------- |
| Arduino      | `.ino`                            | AVR / ARM bare-metal      |
| PlatformIO   | `.ino` / `.cpp` with platform env | MCU bare-metal            |
| Zephyr RTOS  | `.c` + `.overlay` (DTS)           | RTOS kernel module        |
| Linux kernel | `.c` + Kbuild `Makefile`          | Kernel module `.ko`       |
| **KDAL**     | `.kdc` + `.kdh`                   | Linux kernel module `.ko` |

The Linux kernel has no domain-specific abstraction layer for writing device drivers that:
- separates **what a device looks like** (register map, signals) from **how the driver behaves** (event handlers)
- enforces **safe register-access rules** at compile time
- allows a **single source file** to target multiple backends (MMIO, platform_device, virtio, DT)
- is **auditable** by a reviewer who does not know C

KDAL fills this gap.

---

## 2. Language Philosophy

### 2.1 Declarative at the top, imperative at the edges

A `.kdh` file is purely declarative: it describes hardware registers, signal lines, capabilities,
and power states. It contains no executable logic.

A `.kdc` driver is *mostly* declarative (its `config` and annotations) but contains imperative
event handlers (`probe`, `on write`, `on signal`, etc.) where real work happens. This mirrors how
hardware engineers think: "the device has these registers; when this event occurs, do this sequence."

### 2.2 Intentionally not Turing-complete

KDAL rejects:
- **Recursion** — call depth is bounded at 1 (event handler calls built-ins only)
- **Heap allocation** — all driver state lives in a C struct allocated once by `probe`
- **Unbounded loops** — only `for i in 0..N` is allowed (N must be a compile-time constant)
- **Pointer arithmetic** — register access goes through typed paths; the compiler generates `ioread32` / `iowrite32` calls
- **Inline assembly** — no escape hatch to machine code

This keeps the generated code auditable and eliminates entire classes of kernel bugs
(memory corruption, double-free, stack overflow, IRQ reentrancy).

### 2.3 Self-hosting philosophy

The KDAL compiler (`kdalc`) is written in C. This is intentional:
- The compiler must run in the kernel build environment, which guarantees a C compiler
- C is the language of the kernel; keeping the compiler in C avoids a bootstrap dependency
- The generated output is idiomatic C that a Linux kernel reviewer can read and audit without knowing KDAL

---

## 3. Comparison with Alternatives

| Feature             | C + Kbuild            | Arduino           | Zephyr RTOS     | **KDAL**                 |
| ------------------- | --------------------- | ----------------- | --------------- | ------------------------ |
| Target              | Linux `.ko`           | Bare-metal        | RTOS `.ko`      | Linux `.ko`              |
| Register safety     | Manual                | Manual            | Manual          | **Compile-time**         |
| Backend abstraction | Manual                | None              | DTS + Kconfig   | **`backend` annotation** |
| Signal/IRQ model    | Manual                | `attachInterrupt` | `gpio_callback` | **`on signal` handler**  |
| Power management    | PM framework (manual) | None              | `pm_device`     | **`on power` handler**   |
| Learning curve      | High (C + kernel API) | Low               | Medium          | **Low–Medium**           |
| Auditability        | Moderate              | Low               | Moderate        | **High**                 |
| Turing-complete     | Yes                   | Yes               | Yes             | **No (by design)**       |

---

## 4. File Format Design Decisions

### 4.1 Why `.kdh` and `.kdc` (not `.kdal`)?

The two-file split separates concerns:
- `.kdh` is shareable — once written, it describes a hardware class (e.g., `uart_pl011`) and can be reused by many drivers
- `.kdc` is the implementation — one per driver, imports one or more `.kdh` files
- This mirrors the C `.h` / `.c` split but makes the semantic difference explicit in the extension

The `.kdal` extension (used in earlier drafts) was ambiguous about which type of file it was.

### 4.2 Why no `while` loop?

`while` loops can spin indefinitely if the exit condition is never met. In kernel context, that means
the CPU is stuck in IRQ or softirq context, causing a system hang. `for i in 0..N` guarantees
termination with a single glance.

The `wait(signal, ms)` built-in replaces `while (!ready) {}` with a proper kernel wait queue.

### 4.3 Why is `log` a built-in?

`pr_debug` and `dev_dbg` require a `struct device *` pointer that the driver does not have at
the KDAL abstraction level. The compiler injects the correct pointer from the driver context struct.
This prevents the common kernel bug of calling `pr_err` without a device context.

### 4.4 Why not use Rust?

Rust is a valid kernel language (Linux 6.1+) and KDAL could in principle generate Rust instead of C.
The decision to target C for KDAL 0.x is pragmatic:
- Rust in-kernel toolchains are still stabilizing (bindgen, macros)
- The KDAL runtime (`libkdal.a`) is C; a C codegen avoids cross-language FFI
- Rust-as-target is listed as a future work item (see CHANGELOG)

---

## 5. Compiler Architecture

```
.kdc file
    │
    ▼
┌─────────────┐
│   Lexer     │  characters → token stream
│ (lexer.c)   │
└──────┬──────┘
       │ token stream
       ▼
┌─────────────┐
│   Parser    │  tokens → AST
│ (parser.c)  │  recursive descent, no external parser generator
└──────┬──────┘
       │ AST
       ▼
┌─────────────┐
│  Semantic   │  type checking, register access verification,
│  Analysis   │  scope resolution, backend validation
│  (sema.c)   │
└──────┬──────┘
       │ annotated AST
       ▼
┌─────────────┐
│   Codegen   │  AST → .c source + Makefile.kbuild
│ (codegen.c) │
└──────┬──────┘
       │
   .c  +  Makefile.kbuild
       │
       ▼
   kbuild  →  .ko
```

Each phase is a separate `.c` file. The phases communicate via the `kdal_ast_t` struct hierarchy
defined in `compiler/include/ast.h`.

### 5.1 Lexer

Hand-written tokenizer. No external tool dependency. Produces a flat token array.

### 5.2 Parser

Recursive descent. Grammar is LL(1) for most constructs; lookahead ≤ 2 for the few ambiguous cases
(e.g., distinguishing `ident` from `reg_path`).

### 5.3 Semantic analysis

- Symbol table: two-scope chain (global = device namespace; local = handler block)
- Register access checks: `ro` registers catch write in `on write` handler; `wo` registers catch read
- Backend validation: MMIO requires `@` offset in every register; other backends may be relaxed
- Bounded-loop check: `for` upper bound must resolve to a compile-time integer constant

### 5.4 Code generation

Output targets Linux kernel C:
- Driver context struct: `struct <Name>_ctx { void __iomem *base; … kdal_driver_t kdal; }` (for MMIO)
- `probe` function: `static int <Name>_probe(struct platform_device *pdev)`
- `remove` function: `static int <Name>_remove(struct platform_device *pdev)`
- `read` / `write` handlers map to `kdal_driver_ops.read` / `.write`
- Signal handlers: `static irqreturn_t <Name>_irq_<sig>(int irq, void *dev)` registered via `devm_request_irq`
- Power handlers: `static int <Name>_pm_<transition>(struct device *dev)`

---

## 6. Standard Library (.kdh files)

The KDAL standard library lives in `lang/stdlib/` and provides `.kdh` interfaces for common device classes:

| File         | Device class                  | Notable registers                                       |
| ------------ | ----------------------------- | ------------------------------------------------------- |
| `common.kdh` | Primitive types and constants | —                                                       |
| `uart.kdh`   | UART 16550-compatible         | DR, FR, IBRD, FBRD, LCRH, CR, IFLS, IMSC, RIS, MIS, ICR |
| `i2c.kdh`    | I2C controller (DesignWare)   | CON, TAR, SAR, DATA_CMD, SS/FS_SCL, ISR, IMR, CLR_INTR  |
| `spi.kdh`    | SPI controller (PL022)        | CR0, CR1, DR, SR, CPSR, IMSC, RIS, MIS, ICR, DMACR      |
| `gpio.kdh`   | Generic GPIO bank             | DIR, DATA, IS, IBE, IEV, IE, RIS, MIS, IC, AFSEL        |
| `gpu.kdh`    | Abstract GPU command queue    | SUBMIT, STATUS, RESET, IRQ_STATUS, IRQ_ENABLE, FENCE    |
| `npu.kdh`    | Abstract NPU inference engine | MODEL_LOAD, INFER_CMD, STATUS, IRQ_STATUS, RESULT_ADDR  |

---

## 7. Future Work

- **v0.2** — `async` event handlers (work-queue dispatch instead of IRQ context)
- **v0.3** — KDAL debugfs integration (auto-generate `/sys/kernel/debug/kdal/<driver>/` from register declarations)
- **v0.4** — KUnit test generation (generate KUnit test scaffold from `.kdh` type information)
- **v0.5** — Rust codegen target (`codegen_rust.c`)
- **v1.0** — Stable ABI, Language Conformance Test Suite

---

## 8. Non-Goals

- **General-purpose programming language** — KDAL is not Python, not Go, not a scripting language
- **Bare-metal / RTOS support** — KDAL targets Linux kernel modules only
- **Bytecode / VM** — generated output is always C, never bytecode
- **IDE language server** — planned but out of scope for v0.1
