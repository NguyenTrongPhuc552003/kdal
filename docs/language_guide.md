# KDAL Language Guide

This guide walks you through writing kernel drivers using the KDAL DSL
instead of raw C. No prior kernel programming experience is required.

## Concepts

KDAL splits driver development into two files:

| File type     | Extension | Purpose                                                          |
| ------------- | --------- | ---------------------------------------------------------------- |
| Device Header | `.kdh`    | Declares *what* the device is (registers, signals, capabilities) |
| Driver Code   | `.kdc`    | Implements *how* the driver behaves (event handlers)             |

The compiler (`kdalc`) reads both files and generates a complete Linux
kernel module in C, ready to build with `make modules`.

## Your First Device Header (`.kdh`)

A `.kdh` file describes a hardware device declaratively:

```kdal
// led.kdh - a simple GPIO-controlled LED

device led {
    class gpio;
    compatible "example,led-gpio";

    register_map {
        CTRL  0x00 rw;   // control register
        STATE 0x04 ro;   // current state (on/off)
    }

    signals {
        toggle;           // software signal to toggle LED
    }

    capabilities {
        dimmable false;
    }

    power_states {
        off;
        on;
    }
}
```

Key elements:
- **`device`** - names the device (used in `.kdc` and generated code)
- **`class`** - maps to `enum kdal_device_class` (uart, i2c, spi, gpio, gpu, npu)
- **`compatible`** - Device Tree compatible string for OF matching
- **`register_map`** - named registers with offset and access mode (`ro`, `wo`, `rw`)
- **`signals`** - software events the driver can react to
- **`capabilities`** - key-value pairs describing hardware features
- **`power_states`** - supported power states for power management

## Your First Driver (`.kdc`)

A `.kdc` file implements driver behavior using event handlers. It imports
the device header under a name (`DEV` here) and reaches every register,
bitfield and signal through it:

```kdal
// led_driver.kdc - a driver for the led device above

kdal_version: "0.1";

import "led.kdh" as DEV;

driver led_driver for DEV.led {

    probe {
        write(DEV.CTRL, 0x01);      // enable
        log("led: probed\n");
    }

    remove {
        write(DEV.CTRL, 0x00);      // disable
        log("led: removed\n");
    }

    on read(DEV.STATE) {
        wait(DEV.toggle, 1000);     // until the state changes
    }

    on write(DEV.CTRL, val) {
        write(DEV.CTRL, val);
    }

    on signal(DEV.toggle) {
        log("led: state=0x%x\n", read(DEV.STATE));
    }

    on power(on -> off) {
        write(DEV.CTRL, 0x00);
    }

    on power(off -> on) {
        write(DEV.CTRL, 0x01);
    }
}
```

Key constructs:
- **`import "x.kdh" as DEV;`** - imports a `.kdh` device header under a name
- **`driver ... for DEV.device`** - names the driver and binds it to a device
- **`probe { ... }`** - runs when the kernel attaches the driver to hardware
- **`remove { ... }`** - runs when the driver is unloaded
- **`on read(DEV.REG)` / `on write(DEV.REG, val)`** - handle userspace `read()` / `write()` on a register
- **`on signal(DEV.name)`** - handle a hardware signal (interrupt)
- **`on power(a -> b)`** - handle a power transition
- **`read(DEV.REG)`** / **`write(DEV.REG, val)`** - typed register access (bitfields too: `DEV.CTRL.enable`)
- **`log(fmt, ...)`** - emits `pr_info()` in the generated C

`kdality init <name>` writes a project in exactly this form; `kdality
simulate`, `lint` and `test-gen` all work on it as generated.

## Compiling

```sh
# Using the unified tool
./kdality compile examples/kdc_hello/uart_hello.kdc -o output/

# Or the standalone compiler
./kdalc examples/kdc_hello/uart_hello.kdc -o output/
```

This produces:
- `output/uart_hello.c` - complete kernel module source
- `output/Makefile.kbuild` - Kbuild Makefile for `make modules`

Then build the kernel module:

```sh
make -C /path/to/kernel M=$(pwd)/output modules
```

## What the Compiler Generates

For the `uart_hello.kdc` above, the compiler emits roughly:

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/* Register offsets from uart.kdh */
#define UART_DR    0x000
#define UART_FR    0x018

static int uart_hello_probe(struct platform_device *pdev) {
    void __iomem *base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(base)) return PTR_ERR(base);
    pr_info("UART initialized\n");
    return 0;
}

static int uart_hello_remove(struct platform_device *pdev) {
    /* ... tear down ... */
    return 0;
}

static const struct of_device_id uart_hello_of_match[] = {
    { .compatible = "arm,pl011" },
    { }
};
MODULE_DEVICE_TABLE(of, uart_hello_of_match);

static struct platform_driver uart_hello_driver = {
    .probe  = uart_hello_probe,
    .remove = uart_hello_remove,
    .driver = {
        .name = "uart_hello",
        .of_match_table = uart_hello_of_match,
    },
};
module_platform_driver(uart_hello_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Auto-generated by kdalc from uart_hello.kdc");
```

Notice: the compiler handles `platform_driver` boilerplate, OF match table,
`devm_*` resource management, and `MODULE_*` macros automatically.

## Register Maps

Register definitions in `.kdh` files map directly to `#define` constants
and typed accessor calls in the generated C:

| KDAL access mode | Generated C macro               | Notes                                   |
| ---------------- | ------------------------------- | --------------------------------------- |
| `ro`             | `ioread32(base + offset)`       | Read-only; lint warns on `write` (W003) |
| `wo`             | `iowrite32(val, base + offset)` | Write-only; lint warns on `read` (W004) |
| `rw`             | Both                            | Full access                             |

The semantic analysis pass enforces these access restrictions at compile
time, catching errors before any C code is generated.

## Event Handlers

| Handler                   | Kernel equivalent               | When it runs             |
| ------------------------- | ------------------------------- | ------------------------ |
| `probe`                   | `.probe` callback               | Driver bound to device   |
| `remove`                  | `.remove` callback              | Driver unbound           |
| `on read(DEV.REG)`        | `fops->read`                    | Userspace `read()`       |
| `on write(DEV.REG, val)`  | `fops->write`                   | Userspace `write()`      |
| `on signal(DEV.name)`     | Interrupt handler               | The signal fires         |
| `on power(a -> b)`        | PM callbacks                    | Power state transition   |

## Standard Library Devices

KDAL ships with pre-built `.kdh` files for common device classes:

| File       | Device class    | Key registers              |
| ---------- | --------------- | -------------------------- |
| `uart.kdh` | UART serial     | TX, RX, CTRL, STATUS, BAUD |
| `i2c.kdh`  | I2C bus         | DATA, ADDR, CTRL, STATUS   |
| `spi.kdh`  | SPI bus         | TX, RX, CTRL, STATUS, CLK  |
| `gpio.kdh` | GPIO pins       | DIR, OUT, IN, IRQ_EN       |
| `gpu.kdh`  | GPU accelerator | CMD, STATUS, CTRL, FENCE   |
| `npu.kdh`  | NPU accelerator | CMD, STATUS, CTRL, RESULT  |

Import them by their `kdal/stdlib/` path, as
`examples/kdc_hello/i2c_sensor.kdc` does:
`import "kdal/stdlib/i2c.kdh" as I2C;`. `kdality lint` finds them in the
installed standard library (or in `lang/stdlib/` in a source tree;
`KDAL_STDLIB_DIR` overrides).

## Common Patterns

The parser has `if`/`else`, `for i in a..b`, `let`, `return` and `wait`;
it has no `while`: a poll is a `wait` on the signal that ends it.

### Waiting for data

```kdal
on read(DEV.DATA) {
    wait(DEV.rx_ready, 1000);   // up to 1000 ms
}
```

### Conditional initialization

```kdal
probe {
    let id = read(DEV.DEVICE_ID);
    if (id != 0xCAFE) {
        log("unknown device id 0x%x\n", id);
        return -1;
    }
    write(DEV.CTRL, 0x01);
}
```

### Multi-register write sequence

```kdal
on write(DEV.DATA, val) {
    write(DEV.CTRL, 0x00);       // disable
    write(DEV.DATA, val);        // load data
    write(DEV.CTRL, 0x01);       // enable + trigger
}
```

## Error Handling

The compiler maps non-zero returns from handlers to kernel error codes.
Return `0` for success, negative values for errors:

```kdal
probe {
    if (read(DEV.STATUS) == 0xFF) {
        return -1;   // generates: return -EIO;
    }
    return 0;
}
```

## Next Steps

- Browse `examples/` for complete `.kdc` examples
- Read `lang/spec.md` for the full language specification
- Read `lang/DESIGN.md` for design rationale
- Create your own `.kdh` + `.kdc` and compile with `kdality compile`
