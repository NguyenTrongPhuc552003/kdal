---
title: kdality CLI
description: The KDAL multi-tool - compile, scaffold, simulate, lint, and more.
---

`kdality` is the all-in-one command-line tool for KDAL development.
It wraps the compiler and adds project scaffolding, simulation, linting,
test generation, device-tree generation, and runtime control.

## Usage

```bash
kdality <subcommand> [options] [arguments]
```

If no recognised subcommand is given, `kdality` falls through to `kdalctl`
(runtime control) mode.

---

## Subcommands

### `compile` - Compile a driver

Translates a `.kdc` file into a kernel module (links `libkdalc.a` internally).

```bash
kdality compile [options] <file.kdc>
```

| Flag | Argument | Description                     |
| ---- | -------- | ------------------------------- |
| `-o` | `<dir>`  | Output directory (default: `.`) |
| `-K` | `<dir>`  | Kernel build directory (`KDIR`) |
| `-x` | `<pfx>`  | `CROSS_COMPILE` prefix          |
| `-v` | -        | Verbose output                  |
| `-h` | -        | Show help                       |

**Output:** `<driver>.c` + `Makefile.kbuild`

---

### `init` - Scaffold a new project

Creates a ready-to-build KDAL driver project directory.

```bash
kdality init <name> [--class <class>] [--vendor <vendor>]
```

| Flag       | Argument   | Default   | Description                                              |
| ---------- | ---------- | --------- | -------------------------------------------------------- |
| `--class`  | `<class>`  | `gpio`    | Device class: `uart`, `i2c`, `spi`, `gpio`, `gpu`, `npu` |
| `--vendor` | `<vendor>` | `example` | Device-tree compatible prefix                            |
| `-h`       | -          | -         | Show help                                                |

**Created files:**

| File                | Description                                                   |
| ------------------- | ------------------------------------------------------------- |
| `<name>.kdh`        | Device header with registers and signals for the chosen class |
| `<name>_driver.kdc` | Driver skeleton with probe/remove handlers                    |
| `Makefile`          | Build rules (compile, lint, simulate, test, clean)            |
| `README.md`         | Getting-started instructions                                  |

The project name must be alphanumeric plus underscores, starting with a letter.

**Example:**

```bash
kdality init my_sensor --class i2c --vendor acme
cd my_sensor/
make            # compile → kernel module
kdality lint my_sensor_driver.kdc  # static analysis
make simulate   # dry-run execution
```

---

### `simulate` - Dry-run a driver

Traces driver execution without a kernel, using a virtual register file.

```bash
kdality simulate <file.kdc> [--trace] [--event <name>]
```

| Flag      | Argument | Description                        |
| --------- | -------- | ---------------------------------- |
| `--trace` | -        | Print step-by-step execution trace |
| `--event` | `<name>` | Simulate a specific event handler  |

The simulator maintains up to 64 virtual registers and prints each
read/write operation:

```
[1] reg_write  STATUS  0x00000001
[2] reg_read   DATA    => 0x000000FF
[3] log        "probe complete"
```

This is useful for newcomers to visualise driver behaviour before compiling
and loading onto real hardware.

---

### `lint` - Static analysis

Checks `.kdh` and `.kdc` files for common mistakes.

```bash
kdality lint <file.kdc|file.kdh> [--strict]
```

| Flag       | Description                                                 |
| ---------- | ----------------------------------------------------------- |
| `--strict` | Enable additional warnings (e.g. missing compatible string) |

#### Lint rules

| Code   | Severity | Description                                         |
| ------ | -------- | --------------------------------------------------- |
| `E001` | Error    | No `device` declaration found (`.kdh`)              |
| `W001` | Warning  | Missing `probe` handler                             |
| `W002` | Warning  | Missing `remove` handler (resource leak risk)       |
| `W003` | Warning  | Write to read-only register                         |
| `W004` | Warning  | Read from write-only register                       |
| `W005` | Warning  | Empty handler body                                  |
| `W006` | Warning  | Missing `use` statement (no device header imported) |
| `W007` | Warning  | Unused register (declared but never accessed)       |
| `W008` | Warning  | Missing power handler                               |
| `W009` | Warning  | Magic number in `reg_write` (use named constants)   |
| `W010` | Warning  | No `compatible` string (strict mode only)           |

---

### `test-gen` - Generate KUnit tests

Auto-generates KUnit test stubs from a `.kdc` driver.

```bash
kdality test-gen <file.kdc> [-o <dir>]
```

| Flag | Argument | Default | Description      |
| ---- | -------- | ------- | ---------------- |
| `-o` | `<dir>`  | `.`     | Output directory |
| `-h` | -        | -       | Show help        |

**Output:** `test_<driver>.c` containing:

- SPDX header and `#include <kunit/test.h>`
- Setup/teardown fixtures
- One `KUNIT_CASE` per event handler found in the driver
- Suite registration and `MODULE_LICENSE`

---

### `dt-gen` - Generate Device Tree overlay

Produces a `.dtso` (Device Tree Source Overlay) from a `.kdh` device header.

```bash
kdality dt-gen <file.kdh> [-o <dir>] [--base-addr <addr>] [--irq <num>]
```

| Flag          | Argument | Default | Description                    |
| ------------- | -------- | ------- | ------------------------------ |
| `-o`          | `<dir>`  | `.`     | Output directory               |
| `--base-addr` | `<addr>` | -       | Base MMIO address (hex)        |
| `--irq`       | `<num>`  | -       | IRQ number for signal bindings |
| `-h`          | -        | -       | Show help                      |

The generator parses the `.kdh` for device name, compatible string,
register offsets (to compute region size), and signals (for interrupt
bindings). Default region size: highest offset + 4, minimum 256 bytes.

---

### `kdalctl` - Runtime control

Userspace control plane for loaded KDAL drivers via `/dev/kdal` ioctls.

```bash
kdality kdalctl <command> [arguments]
# or simply:
kdality <command> [arguments]
```

| Command   | Arguments        | Description                                              |
| --------- | ---------------- | -------------------------------------------------------- |
| `version` | -                | Query kernel module version                              |
| `list`    | -                | List all registered KDAL devices                         |
| `info`    | `<name>`         | Show device details (class, power state, features, caps) |
| `power`   | `<name> <state>` | Set power state: `on`, `off`, `suspend`                  |
| `select`  | `<name>`         | Select a device for subsequent I/O                       |
| `read`    | `<name> <count>` | Read bytes from a device                                 |
| `write`   | `<name> <data>`  | Write data to a device                                   |

**Example:**

```bash
kdality list
# my_sensor  (i2c, power: on)

kdality info my_sensor
# Class:      i2c
# Power:      on
# Features:   0x0003
# Caps:       dma, irq

kdality power my_sensor suspend
```

## Source Files

| File                        | Purpose                                  |
| --------------------------- | ---------------------------------------- |
| `tools/kdality/main.c`      | Subcommand dispatcher                    |
| `tools/kdality/compile.c`   | `compile` subcommand                     |
| `tools/kdality/init.c`      | `init` - project scaffolding             |
| `tools/kdality/simulate.c`  | `simulate` - dry-run interpreter         |
| `tools/kdality/lint.c`      | `lint` - static analysis                 |
| `tools/kdality/testgen.c`   | `test-gen` - KUnit test generator        |
| `tools/kdality/dtgen.c`     | `dt-gen` - Device Tree overlay generator |
| `tools/kdality/kdalctl.c`   | `kdalctl` - runtime ioctl control        |
| `tools/kdality/templates.h` | Embedded templates for `init`            |
