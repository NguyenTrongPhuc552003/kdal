---
title: Writing Drivers
description: End-to-end guide for writing KDAL drivers - from device header to loaded module.
---

This guide walks through writing a complete KDAL driver using the two-file
model: a **device header** (`.kdh`) that declares hardware resources and a
**driver implementation** (`.kdc`) that contains event-handler logic.

## Workflow Overview

```
1. Describe hardware  →  my_device.kdh
2. Implement logic    →  my_device_driver.kdc
3. Lint               →  kdality lint
4. Compile            →  kdality compile → .c + Makefile.kbuild
5. Build              →  make -C $KDIR M=$PWD → .ko
6. Load               →  insmod my_device_driver.ko
```

## Step 1 - Device Header

Create a `.kdh` file that models your hardware's registers, signals, and
capabilities:

```
kdal_version: "0.1.0";

device class TemperatureSensor {
    registers {
        CTRL      : u8  @0x00 rw = 0x00;
        STATUS    : u8  @0x01 ro;
        TEMP_DATA : u16 @0x02 ro;
        CONFIG    : bitfield @0x04 rw {
            resolution : 1..0 = 0x02;
            mode       : 3..2 = 0x00;
            enable     : 4;
        };
    }

    signals {
        data_ready : in edge(rising);
        alert      : out level(high);
    }

    capabilities {
        max_sample_rate = 100;
        resolution_bits = 12;
    }

    power {
        on      : default;
        off     : allowed;
        suspend : allowed;
    }
}
```

### Key points

- Every register must have a **type** and an **access qualifier** (`ro`/`wo`/`rw`/`rc`)
- MMIO backends require `@offset` annotations
- Bitfield registers declare named bit ranges
- Signals define interrupt-like connections with direction and trigger

## Step 2 - Driver Implementation

Create a `.kdc` file that imports the header and handles events:

```
kdal_version: "0.1.0";
import "temperature_sensor.kdh";

backend mmio;

driver TempSensorDriver for TemperatureSensor {
    config {
        sample_rate = 50;
    }

    probe {
        // Enable the sensor
        write(CONFIG.enable, 1);
        write(CONFIG.resolution, 0x03);
        write(CTRL, 0x01);
        log("temperature sensor initialised");
    }

    remove {
        write(CTRL, 0x00);
        log("temperature sensor removed");
    }

    on read(TEMP_DATA) {
        let raw = read(TEMP_DATA);
        return raw;
    }

    on signal(data_ready) {
        let temp = read(TEMP_DATA);
        emit(alert, temp);
        log("temperature reading: %d", temp);
    }

    on power(suspend) {
        write(CTRL, 0x00);
        log("entering suspend");
    }

    on power(on) {
        write(CTRL, 0x01);
        log("resuming from suspend");
    }
}
```

### Event handler reference

| Handler                   | Trigger                        |
| ------------------------- | ------------------------------ |
| `probe {}`                | Module insertion / device bind |
| `remove {}`               | Module removal / device unbind |
| `on read(reg) {}`         | Userspace read request         |
| `on write(reg, value) {}` | Userspace write request        |
| `on signal(name) {}`      | Hardware interrupt             |
| `on power(transition) {}` | Power state change             |
| `on timeout(name) {}`     | Timer expiration               |

### Built-in operations

| Operation               | Description                     |
| ----------------------- | ------------------------------- |
| `read(reg)`             | Read a register (returns value) |
| `write(reg, value)`     | Write a value to a register     |
| `emit(signal, data)`    | Emit a signal/event             |
| `wait(signal, timeout)` | Wait for a signal               |
| `arm(timer, duration)`  | Start a timer                   |
| `cancel(timer)`         | Cancel a timer                  |
| `log(fmt, ...)`         | Kernel log message              |
| `return [value]`        | Return from handler             |

## Step 3 - Lint

Run static analysis before compiling:

```bash
kdality lint temperature_sensor.kdh
kdality lint temp_sensor_driver.kdc
kdality lint temp_sensor_driver.kdc --strict
```

Fix any warnings (W001–W010) and errors (E001) before proceeding.

## Step 4 - Compile

```bash
kdality compile -o build/ temp_sensor_driver.kdc
```

This produces:
- `build/TempSensorDriver.c` - self-contained kernel module
- `build/Makefile.kbuild` - kbuild integration

## Step 5 - Build the Kernel Module

```bash
# Native build
make -C /lib/modules/$(uname -r)/build M=$(pwd)/build modules

# Cross-compile for aarch64
make -C $KDIR M=$(pwd)/build \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules
```

## Step 6 - Load and Verify

```bash
sudo insmod build/TempSensorDriver.ko
dmesg | tail -5
# [kdal] temperature sensor initialised

# Interact via kdality
kdality list
kdality info TempSensorDriver
kdality read TempSensorDriver 2

# Unload
sudo rmmod TempSensorDriver
```

## Common Patterns

### Register access with bitfields

```
// Read a bitfield
let res = read(CONFIG.resolution);

// Modify a bitfield
write(CONFIG.mode, 0x01);
```

### Conditional logic

```
on read(TEMP_DATA) {
    let status = read(STATUS);
    if (status & 0x01) {
        return read(TEMP_DATA);
    } else {
        log("sensor not ready");
        return 0;
    }
}
```

### Bounded loops

```
probe {
    for i in 0..8 {
        write(CTRL, i);
    }
}
```

The loop bound must be a compile-time constant - KDAL guarantees termination.

### Timer patterns

```
on signal(data_ready) {
    arm(timeout_timer, 1000);
}

on timeout(timeout_timer) {
    log("no response within 1 second");
    cancel(timeout_timer);
}
```

## Generated Code

The compiler translates KDAL constructs into idiomatic kernel C. Here is a
simplified example of what `probe` generates:

```c
static int TempSensorDriver_probe(struct platform_device *pdev)
{
    struct TempSensorDriver_ctx *ctx;
    ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(ctx->base))
        return PTR_ERR(ctx->base);

    /* write(CONFIG.enable, 1) */
    iowrite8(ioread8(ctx->base + 0x04) | BIT(4), ctx->base + 0x04);

    /* write(CTRL, 0x01) */
    iowrite8(0x01, ctx->base + 0x00);

    dev_info(&pdev->dev, "temperature sensor initialised\n");
    return 0;
}
```
