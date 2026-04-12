---
title: Device Headers (.kdh)
description: Declaring hardware registers, signals, capabilities, and power states.
---

A `.kdh` file describes a hardware device **declaratively**. It contains no
executable logic - only structural declarations that the compiler uses to
generate typed accessors and safety checks.

## File Structure

```kdal
kdal_version: "0.1";

import "kdal/stdlib/common.kdh";

device class <Name> {
    compatible "<vendor>,<device>";
    class <device_class>;

    registers     { … }
    register_map  { … }
    signals       { … }
    capabilities  { … }
    power         { … }
    config        { … }
}
```

## Example

```kdal
// led.kdh - a simple GPIO-controlled LED

device class Led {
    class gpio;
    compatible "example,led-gpio";

    register_map {
        CTRL  0x00 rw;
        STATE 0x04 ro;
    }

    signals {
        toggle;
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

## Registers Block

Each register declaration binds a name to a hardware register with type, offset,
access mode, and optional default value:

```kdal
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

- `@ 0xNN` - MMIO offset (optional; platform drivers may ignore it)
- Access qualifiers:

| Qualifier | Meaning       | Generated C                                                   |
| --------- | ------------- | ------------------------------------------------------------- |
| `ro`      | Read-only     | `ioread32(base + offset)` - compiler rejects `reg_write`      |
| `wo`      | Write-only    | `iowrite32(val, base + offset)` - compiler rejects `reg_read` |
| `rw`      | Read-write    | Both                                                          |
| `rc`      | Clear-on-read | Read clears the register                                      |

- `= value` - reset/default value
- `bitfield` - nested bit fields with named bits or bit ranges (`4..6`)

## Signals Block

Signals map to IRQs or GPIO lines. The compiler generates IRQ handler scaffolding:

```kdal
signals {
    rx_ready  : in   edge(rising);
    tx_empty  : in   edge(rising);
    error     : in   level(high);
    power_ack : in   completion;
    done      : in   timeout;
}
```

| Direction | Meaning                         |
| --------- | ------------------------------- |
| `in`      | Input signal (device → driver)  |
| `out`     | Output signal (driver → device) |
| `inout`   | Bidirectional                   |

| Trigger         | Meaning                |
| --------------- | ---------------------- |
| `edge(rising)`  | Rising edge triggered  |
| `edge(falling)` | Falling edge triggered |
| `edge(any)`     | Any edge               |
| `level(high)`   | High level triggered   |
| `level(low)`    | Low level triggered    |
| `completion`    | Completion event       |
| `timeout`       | Timer expiry           |

## Capabilities Block

Key-value pairs describing hardware features. These become `#define` constants:

```kdal
capabilities {
    fifo_depth = 16;
    dma_capable;
    error_correction;
}
```

## Power Block

Supported power states and transition rules:

```kdal
power {
    on      : allowed;
    off     : allowed;
    suspend : forbidden;
    default : on;
}
```

## Config Block

Typed parameters with optional default values and range constraints:

```kdal
config {
    baud_rate : u32 = 115200 in 1200..4000000;
    data_bits : u8  = 8      in 5..9;
    parity    : u8  = 0      in 0..2;
}
```

Config fields may be overridden in the `.kdc` driver's `config` bind block.

## Key Elements Summary

| Element                      | Purpose                                                           |
| ---------------------------- | ----------------------------------------------------------------- |
| `device class`               | Names the device (used in `.kdc` and generated code)              |
| `class`                      | Maps to `enum kdal_device_class` (uart, i2c, spi, gpio, gpu, npu) |
| `compatible`                 | Device Tree compatible string for OF matching                     |
| `registers` / `register_map` | Named registers with offset and access mode                       |
| `signals`                    | Software/hardware events the driver reacts to                     |
| `capabilities`               | Key-value pairs describing hardware features                      |
| `power`                      | Supported power states                                            |
| `config`                     | Typed, range-constrained parameters                               |
