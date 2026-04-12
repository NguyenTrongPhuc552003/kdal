---
title: Language Specification
description: Formal specification of the KDAL DSL - lexical rules, types, and semantics.
---

**Version:** 0.1 | **Status:** Draft | **Applies to:** kdalc ≥ 0.1, kdality ≥ 0.1

## 1. Overview

KDAL defines two file formats that together describe a hardware device and its Linux kernel driver:

| Extension | Role                                                     | Analogous to                        |
| --------- | -------------------------------------------------------- | ----------------------------------- |
| `.kdh`    | Device header - register map, signals, capabilities      | C `.h` header + device tree binding |
| `.kdc`    | Driver implementation - event handlers compiled to `.ko` | C `.c` source + Kbuild Makefile     |

The compiler translates a `.kdc` file (and its imported `.kdh` files) into:
- a C translation unit (`.c`) containing a `kdal_driver_ops` struct and all handler functions
- a Kbuild fragment (`Makefile.kbuild`) ready for `make -C $KERNEL_DIR M=$PWD modules`

## 2. Design Goals

1. **Auditable surface** - not Turing-complete. No recursion, no heap, no pointers. Every program terminates.
2. **Kernel-space first** - all output targets the Linux kernel ABI. No libc, no syscalls.
3. **Type safety at register level** - access direction verified at compile time (ro/wo/rw/rc).
4. **Backend agnostic** - same `.kdc` targets MMIO, platform_device, devicetree, virtio, or QEMU.
5. **Self-describing** - `.kdh` + `.kdc` fully describe device and driver.

## 3. Lexical Rules

### Comments

```kdal
// single-line comment
/* multi-line comment */
```

### Keywords

```
kdal_version  import  as  device  class  registers  signals
capabilities  power   config  backend  driver  for  probe
remove  on  read  write  signal  timeout  power  emit
wait  arm  cancel  let  return  log  if  elif  else  in
true  false  ro  wo  rw  rc  rising  falling  any
high  low  edge  level  completion  allowed  forbidden
default  on  off  suspend  use  while  for
```

### Primitive Types

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
| `bool`    | 1-bit                     | -      |
| `str`     | NUL-terminated, read-only | -      |
| `timeout` | opaque timer handle       | -      |
| `buf[T]`  | contiguous array of `T`   | -      |

### Integer Literals

```kdal
42        // decimal
0xFF      // hexadecimal (case-insensitive)
0b1010    // binary
```

## 4. Device Header Format (.kdh)

See [Device Headers](/language/device-headers/) for detailed examples.

```
kdal_version: "0.1";
import "...";
device class <Name> {
    compatible "...";
    class <device_class>;
    registers { … }
    signals   { … }
    capabilities { … }
    power     { … }
    config    { … }
}
```

## 5. Driver Implementation Format (.kdc)

See [Driver Code](/language/driver-code/) for detailed examples.

```
kdal_version: "0.1";
import "device.kdh" as DEV;
backend <type> { … }
driver <Name> for DEV.<Device> {
    config  { … }
    probe   { … }
    remove  { … }
    on read  { … }
    on write { … }
    on signal <name> { … }
    on power <A> -> <B> { … }
    on timeout <timer> { … }
}
```

## 6. Backend Annotations

| Backend      | Description                                 |
| ------------ | ------------------------------------------- |
| `mmio`       | Direct MMIO register access via `ioremap`   |
| `platdev`    | Linux `platform_device` / `platform_driver` |
| `devicetree` | DT-compatible (generates OF match table)    |
| `virtio`     | VirtIO transport (QEMU)                     |
| `soc`        | SoC vendor glue layer                       |
| `qemu`       | QEMU ring-buffer emulation                  |

## 7. Event Handlers

| Handler              | Trigger                               |
| -------------------- | ------------------------------------- |
| `on read(reg)`       | Userspace reads `reg` via `/dev/kdal` |
| `on write(reg, val)` | Userspace writes `val` to `reg`       |
| `on signal(sig)`     | IRQ or GPIO edge/level event          |
| `on power(A -> B)`   | Power state transition                |
| `on timeout(t)`      | Armed timer fires                     |

## 8. Built-in Operations

| Operation              | Description                             |
| ---------------------- | --------------------------------------- |
| `read(reg_path)`       | Returns register value as typed integer |
| `write(reg_path, val)` | Writes typed value to register          |
| `emit(signal [, val])` | Triggers an output signal               |
| `wait(signal, ms)`     | Waits for signal with timeout           |
| `arm(timer, ms)`       | Starts a one-shot or periodic timer     |
| `cancel(timer)`        | Cancels armed timer                     |
| `log(fmt, ...)`        | Kernel `pr_debug`/`dev_dbg` print       |

## 9. Control Flow

```kdal
if (condition) { … } elif (condition) { … } else { … }
for i in 0..8 { … }   // bounded range - always terminates
```

No `while`, no `goto`, no `break`, no `continue`.

## 10. Type Rules

- Register reads return the declared type (`u8`, `u16`, `u32`, `u64`, or bitfield width)
- Write values are implicitly narrowed; out-of-range is a compile error
- `bool` is not implicitly promoted to integers
- `str` values are read-only, only as `log` format arguments

## 11. Error Messages

```
my_driver.kdc:12:5: error: register 'DATA' is declared 'ro' - cannot write
my_driver.kdc:18:3: warning: wait() inside on_write handler may block IRQ context
my_device.kdh:7:1: note: DATA declared here
```

Severity levels: `error` (abort), `warning` (continue), `note` (context), `hint` (suggestion).

## 12. Reserved Future Keywords

```
sync  async  atomic  pub  priv  use  module  pkg
test  bench  mock  sim  trace  profile
```
