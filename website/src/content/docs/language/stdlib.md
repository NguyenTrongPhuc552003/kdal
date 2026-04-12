---
title: Standard Library
description: Pre-built device headers for common peripherals and accelerators.
---

The KDAL standard library ships 7 `.kdh` files covering common device classes.
These files live in `lang/stdlib/` and are installed to
`~/.kdal/share/kdal/stdlib/` by kdalup.

## Available Devices

| File         | Device Class                  | Notable Registers                                       |
| ------------ | ----------------------------- | ------------------------------------------------------- |
| `common.kdh` | Primitive types and constants | -                                                       |
| `uart.kdh`   | UART 16550-compatible         | DR, FR, IBRD, FBRD, LCRH, CR, IFLS, IMSC, RIS, MIS, ICR |
| `i2c.kdh`    | I2C controller (DesignWare)   | CON, TAR, SAR, DATA_CMD, SS/FS_SCL, ISR, IMR, CLR_INTR  |
| `spi.kdh`    | SPI controller (PL022)        | CR0, CR1, DR, SR, CPSR, IMSC, RIS, MIS, ICR, DMACR      |
| `gpio.kdh`   | Generic GPIO bank             | DIR, DATA, IS, IBE, IEV, IE, RIS, MIS, IC, AFSEL        |
| `gpu.kdh`    | Abstract GPU command queue    | SUBMIT, STATUS, RESET, IRQ_STATUS, IRQ_ENABLE, FENCE    |
| `npu.kdh`    | Abstract NPU inference engine | MODEL_LOAD, INFER_CMD, STATUS, IRQ_STATUS, RESULT_ADDR  |

## Using Standard Library Headers

Import a standard library header in your `.kdc` file:

```kdal
use "uart.kdh";

driver MyUartDriver for uart {
    on probe {
        reg_write(CR, 0x301);
    }
}
```

The compiler searches for `.kdh` files in:
1. The current directory
2. `$KDAL_HOME/share/kdal/stdlib/`
3. Paths specified via `-I` flag

## Creating Custom Device Headers

Create a `.kdh` file following the format in the
[Language Specification](/language/spec/). A minimal device header declares:

```kdal
kdal_version: "0.1";

device class MyDevice {
    compatible "vendor,my-device";
    class gpio;

    register_map {
        CTRL   0x00 rw;
        STATUS 0x04 ro;
    }

    capabilities {
        version = 1;
    }
}
```

Then reference it from your `.kdc`:

```kdal
use "mydevice.kdh";
```

## Extending the Standard Library

To contribute a new standard library device:

1. Create `lang/stdlib/<device>.kdh`
2. Define `device class`, `register_map`, `signals`, `capabilities`, `power_states`
3. Follow existing patterns - `uart.kdh` is the reference
4. Create a matching example `.kdc` in `examples/`
5. Submit a pull request
