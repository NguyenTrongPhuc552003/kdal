# KDAL KDC Hello Examples

This directory contains the canonical introductory examples for the KDAL language
(`.kdh` device headers and `.kdc` driver implementations).

## Files

| File             | Description                                            |
| ---------------- | ------------------------------------------------------ |
| `uart_hello.kdh` | Custom UART device header (simplified PL011 subset)    |
| `uart_hello.kdc` | Full UART driver: probe/remove, read/write, IRQ, power |
| `i2c_sensor.kdc` | I2C temperature sensor driver with periodic polling    |

---

## Prerequisites

- KDAL tools installed (`kdality` binary in `$PATH`)  
- Linux kernel headers for your target (e.g. `linux-headers-6.6-arm64`)
- aarch64 cross-compiler (for QEMU virt target):
  ```sh
  sudo apt install gcc-aarch64-linux-gnu
  ```

---

## Compiling uart_hello.kdc

```sh
# 1. Compile .kdc → .c + Makefile.kbuild
kdality compile uart_hello.kdc \
    -K /path/to/kernel/build \
    -x aarch64-linux-gnu- \
    -o build/

# 2. Build the kernel module
make -C /path/to/kernel/build \
     M=$(pwd)/build \
     CROSS_COMPILE=aarch64-linux-gnu- \
     modules

# 3. The output is build/UartHelloDriver.ko
```

Or using the generated Kbuild fragment directly:
```sh
make -f build/Makefile.kbuild \
     KDIR=/path/to/kernel/build \
     CROSS_COMPILE=aarch64-linux-gnu-
```

---

## Compiling i2c_sensor.kdc

```sh
kdality compile i2c_sensor.kdc \
    -K /path/to/kernel/build \
    -x aarch64-linux-gnu- \
    -o build/

make -f build/Makefile.kbuild \
     KDIR=/path/to/kernel/build \
     CROSS_COMPILE=aarch64-linux-gnu-
```

---

## Running on QEMU

See [scripts/setupqemu/run.sh](../../scripts/setupqemu/run.sh) to start the QEMU aarch64 virt machine.

Once booted:
```sh
# Copy modules into the VM (via scp or 9p virtio-fs)
insmod UartHelloDriver.ko
insmod I2cTempSensor.ko

# Check KDAL device list
kdality list

# Read from the UART device
kdality read /dev/kdal0
```

---

## Language features demonstrated

### uart_hello.kdc
- `import` of a local `.kdh` file with alias
- `backend mmio` with explicit base address
- `config` bind block
- `probe` / `remove` handlers
- `on write(reg, val)` — value parameter
- `on read(reg)` — blocking wait
- `on signal(sig)` — IRQ handler
- `on power(A -> B)` — state transition
- `wait()`, `write()`, `read()`, `log()`

### i2c_sensor.kdc
- `import` of standard library `.kdh`
- `backend platdev` (device tree resolved)
- `arm()` / `cancel()` — periodic timer
- `on timeout(t)` — recurring callback
- Multi-step probe sequence
- `if` / `elif` / `else` control flow
- Multi-register read sequence

---

## What the compiler generates

For `uart_hello.kdc`, the compiler emits:

```
build/
├── UartHelloDriver.c       ← kernel module C source
└── Makefile.kbuild         ← obj-m := UartHelloDriver.o
```

The generated C file contains:
- `struct UartHelloDriver_ctx { struct kdal_driver kdal; void __iomem *base; }`
- `UartHelloDriver_read()` / `UartHelloDriver_write()` — file operations
- `UartHelloDriver_irq()` — IRQ handler for signals
- `UartHelloDriver_probe()` / `UartHelloDriver_remove()` — platform_driver ops
- `OF device ID table` — `compatible = "kdal,UartHelloDriver"`
- `module_platform_driver(UartHelloDriver_driver)` — module registration
