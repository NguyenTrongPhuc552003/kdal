# init - KDAL Driver Project

A kernel driver for the **init** device, written in KDAL DSL.

## Files

| File              | Purpose                                          |
| ----------------- | ------------------------------------------------ |
| `init.kdh`        | Device header (registers, signals, capabilities) |
| `init_driver.kdc` | Driver implementation (event handlers)           |
| `Makefile`        | Build orchestration                              |

## Quick Start

```sh
# 1. Compile the .kdc file to kernel C
make compile

# 2. Build the kernel module (requires kernel headers)
make module KDIR=/path/to/kernel

# 3. Load the module
sudo insmod output/init_driver.ko
```

## Learn More

- [KDAL Language Guide](https://codeberg.org/NguyenTrongPhuc552003/kdal/src/branch/main/docs/language_guide.md)
- [KDAL API Reference](https://codeberg.org/NguyenTrongPhuc552003/kdal/src/branch/main/docs/api_reference.md)
