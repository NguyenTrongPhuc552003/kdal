# KDAL Language Specification

**Version:** 0.1  
**Status:** Draft  
**Applies to:** KDAL compiler (kdalc) ≥ 0.1, kdality ≥ 0.1

---

## 1. Overview

KDAL (Kernel Device Abstraction Layer) defines two file formats that together describe a hardware device and its Linux kernel driver:

| Extension | Role                                                       | Analogous to                        |
| --------- | ---------------------------------------------------------- | ----------------------------------- |
| `.kdh`    | Device header — register map, signals, capabilities        | C `.h` header + device tree binding |
| `.kdc`    | Driver implementation — event handlers compiled to a `.ko` | C `.c` source + Kbuild Makefile     |

The KDAL compiler (`kdalc`, also invoked via `kdality compile`) translates a `.kdc` file (and its imported `.kdh` files) into:
- a C translation unit (`.c`) containing a `kdal_driver_ops` struct and all handler functions
- a Kbuild fragment (`Makefile.kbuild`) ready for `make -C $KERNEL_DIR M=$PWD modules`

---

## 2. Design Goals

1. **Auditable surface** — the language is intentionally not Turing-complete. No recursion, no heap, no pointers, no inline assembly. Every program terminates.
2. **Kernel-space first** — all output targets the Linux kernel ABI. No libc, no syscalls in generated code.
3. **Type safety at register level** — register accesses are always typed (u8/u16/u32/u64), access direction is verified at compile time (ro/wo/rw/rc).
4. **Backend agnostic** — the same `.kdc` driver can target MMIO, platform_device, devicetree, virtio, or QEMU register stubs by changing the `backend` annotation.
5. **Self-describing** — a `.kdh` file and a `.kdc` file together fully describe the device and driver; no out-of-band documentation is required.

---

## 3. Lexical Rules

### 3.1 Comments

```
// single-line comment
/* multi-line comment */
```

### 3.2 Keywords

The following identifiers are reserved:

```
kdal_version  import  as  device  class  registers  signals
capabilities  power   config  backend  driver  for  probe
remove  on  read  write  signal  timeout  power  emit
wait  arm  cancel  let  return  log  if  elif  else  in
true  false  ro  wo  rw  rc  rising  falling  any
high  low  edge  level  completion  allowed  forbidden
default  on  off  suspend
```

### 3.3 Primitive types

| Type      | Width                     | Signed |
| --------- | ------------------------- | ------ |
| `u8`      | 8-bit                     | no     |
| `u16`     | 16-bit                    | no     |
| `u32`     | 32-bit                    | no     |
| `u64`     | 64-bit                    | no     |
| `i8`      | 8-bit                     | yes    |
| `i16`     | 16-bit                    | yes    |
| `i32`     | 32-bit                    | yes    |
| `i64`     | 64-bit                    | yes    |
| `bool`    | 1-bit                     | —      |
| `str`     | NUL-terminated, read-only | —      |
| `timeout` | opaque timer handle       | —      |
| `buf[T]`  | contiguous array of `T`   | —      |

### 3.4 Integer literals

```
42        decimal
0xFF      hexadecimal (case-insensitive)
0b1010    binary
```

---

## 4. Device Header Files (.kdh)

### 4.1 File structure

```text
kdal_version: "0.1";

import "kdal/stdlib/common.kdh";

device class <Name> {
    registers { … }
    signals   { … }
    capabilities { … }
    power     { … }
    config    { … }
}
```

### 4.2 Registers block

Each register declaration binds a name to a hardware register:

```kdh
registers {
    STATUS   : u8   @ 0x00  rw = 0x00;
    DATA     : u32  @ 0x04  rw;
    CONTROL  : bitfield @ 0x08 rw {
        enable    : 0     = false;
        irq_mask  : 1     = true;
        baud_sel  : 4..6  = 0;
    };
}
```

- `@ 0xNN` sets the MMIO offset (optional; platform drivers may ignore it)
- Access qualifiers: `ro` read-only, `wo` write-only, `rw` read-write, `rc` clear-on-read
- `= value` sets the reset/default value

### 4.3 Signals block

```kdh
signals {
    rx_ready  : in   edge(rising);
    tx_empty  : in   edge(rising);
    error     : in   level(high);
    power_ack : in   completion;
}
```

Signals map to IRQs or GPIO lines at runtime. The compiler generates the IRQ handler scaffolding.

### 4.4 Capabilities block

```kdh
capabilities {
    fifo_depth = 16;
    dma_capable;
    error_correction;
}
```

Capabilities become `#define` constants in the generated C.

### 4.5 Power block

```kdh
power {
    on      : allowed;
    off     : allowed;
    suspend : allowed;
    default : on;
}
```

### 4.6 Config block

```kdh
config {
    baud_rate : u32 = 115200 in 1200..4000000;
    data_bits : u8  = 8      in 5..9;
    parity    : u8  = 0      in 0..2;
}
```

Config fields may be overridden in the `.kdc` driver's `config` bind block.

---

## 5. Driver Implementation Files (.kdc)

### 5.1 File structure

```text
kdal_version: "0.1";

import "uart.kdh" as UART;

backend mmio {
    base_addr: 0x09000000;
}

driver MyUartDriver for UART.UartDevice {
    config {
        baud_rate = 115200;
    }

    probe {
        write(UART.CONTROL.enable, true);
    }

    remove {
        write(UART.CONTROL.enable, false);
    }

    on write(UART.DATA, val) {
        // val is the u32 value being written
        wait(UART.tx_empty, 1000);
    }

    on read(UART.DATA) {
        wait(UART.rx_ready, 1000);
    }

    on signal(UART.error) {
        let status = read(UART.STATUS);
        log("UART error: 0x%x", status);
    }

    on power(on -> suspend) {
        write(UART.CONTROL.enable, false);
    }
}
```

### 5.2 Backend annotation

| Backend      | Description                                 |
| ------------ | ------------------------------------------- |
| `mmio`       | Direct MMIO register access via `ioremap`   |
| `platdev`    | Linux `platform_device` / `platform_driver` |
| `devicetree` | DT-compatible (generates OF match table)    |
| `virtio`     | VirtIO transport (QEMU)                     |
| `soc`        | SoC vendor glue layer                       |
| `qemu`       | QEMU ring-buffer emulation (test/dev)       |

### 5.3 Event handlers

| Handler              | Trigger                                                                  |
| -------------------- | ------------------------------------------------------------------------ |
| `on read(reg)`       | Invoked when userspace reads `reg` via `ioctl(KDAL_READ)` or `/dev/kdal` |
| `on write(reg, val)` | Invoked when userspace writes `val` to `reg`                             |
| `on signal(sig)`     | IRQ or GPIO edge/level event                                             |
| `on power(A -> B)`   | Power state transition                                                   |
| `on timeout(t)`      | Armed timer fires                                                        |

### 5.4 Built-in operations

| Operation              | Description                             |
| ---------------------- | --------------------------------------- |
| `read(reg_path)`       | Returns register value as typed integer |
| `write(reg_path, val)` | Writes typed value to register          |
| `emit(signal [, val])` | Triggers an output signal               |
| `wait(signal, ms)`     | Waits for a signal with timeout (ms)    |
| `arm(timer, ms)`       | Starts a one-shot or periodic timer     |
| `cancel(timer)`        | Cancels armed timer                     |
| `log(fmt, ...)`        | Kernel `pr_debug`/`dev_dbg` print       |

### 5.5 Control flow

Only structured control flow is permitted:

```kdal
if (condition) { … } elif (condition) { … } else { … }

for i in 0..8 { … }   // bounded range — always terminates
```

No `while`, no `goto`, no `break`, no `continue`.

---

## 6. Scoping Rules

- `.kdh` file defines a **namespace** equal to the import alias (or the device class name if no alias is given)
- `ident.ident` — namespace member access (e.g., `UART.DATA`)
- `ident.ident.ident` — bitfield within register (e.g., `UART.CONTROL.enable`)
- `let` bindings are local to the enclosing handler block
- No global mutable variables (driver state lives in a C struct allocated by the probe handler in generated code)

---

## 7. Type Rules

- Register reads always return the declared register type (`u8`, `u16`, `u32`, `u64`, or bitfield width)
- Write values are implicitly narrowed to the register width; out-of-range assignments are a compile-time error
- `wait` and `arm` take a `timeout` handle declared in the `.kdh` signals block, plus a `u32` millisecond integer
- `bool` values are not implicitly promoted to integers; use an explicit cast annotation where required
- `str` values are read-only; they may only appear as `log` format arguments

---

## 8. Compiler Output

Given `my_driver.kdc` importing `my_device.kdh`, the compiler emits:

```
my_driver.c          — C translation unit
Makefile.kbuild      — obj-m := my_driver.o
```

The generated C registers a `kdal_driver_ops` struct and delegate all symbol definitions to the KDAL runtime library (`libkdal.a`). The developer then runs:

```sh
kdality compile my_driver.kdc --kernel-dir /lib/modules/$(uname -r)/build
# or equivalently:
add_kdc_driver(my_driver my_driver.kdc)   # CMake
```

---

## 9. Error Messages

The compiler emits structured diagnostics:

```
my_driver.kdc:12:5: error: register 'DATA' is declared 'ro' — cannot write
my_driver.kdc:18:3: warning: wait() inside on_write handler may block IRQ context
my_device.kdh:7:1: note: DATA declared here
```

Severity levels: `error` (compilation aborted), `warning` (emitted, compilation continues), `note` (supplementary context), `hint` (optional suggestion).

---

## 10. Reserved Future Keywords

The following identifiers are reserved for future language versions and must not be used as user-defined names:

```
sync  async  atomic  pub  priv  use  module  pkg
test  bench  mock  sim  trace  profile
```
