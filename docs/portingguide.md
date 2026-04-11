# Porting Guide

## Overview

Porting KDAL to a new platform involves implementing a backend adapter. The
driver layer and userspace interface remain unchanged — this is the core value
proposition of the abstraction.

## Step-by-Step

### 1. Create a Backend

Create a new directory under `src/backends/<platform>/` with a backend source
file implementing `struct kdal_backend_ops`:

```c
static const struct kdal_backend_ops myplatform_ops = {
    .init      = myplatform_init,
    .exit      = myplatform_exit,
    .enumerate = myplatform_enumerate,
    .read      = myplatform_read,
    .write     = myplatform_write,
    .ioctl     = myplatform_ioctl,
};

static struct kdal_backend myplatform_backend = {
    .name          = "my-platform",
    .feature_flags = KDAL_FEAT_POWER_MGMT,
    .ops           = &myplatform_ops,
};
```

### 2. Implement Transport

The `read`/`write` ops provide the data transport path. On real hardware these
would access MMIO registers, DMA buffers, or a bus controller:

```c
static ssize_t myplatform_read(struct kdal_device *device, char *buf, size_t count)
{
    void __iomem *base = device->priv;
    /* Read from hardware register bank */
    return kdal_mmio_read_bulk(base, buf, count);
}
```

### 3. Device Enumeration

During `enumerate()`, scan platform resources (Device Tree, ACPI, PCI) and
create `kdal_device` instances:

```c
static int myplatform_enumerate(struct kdal_backend *backend)
{
    /* Parse DT nodes matching "kdal,device" compatible */
    kdal_devicetree_scan();
    return 0;
}
```

### 4. Register the Backend

Add init/exit functions and wire them into `src/core/kdal.c`:

```c
int kdal_myplatform_backend_init(void);
void kdal_myplatform_backend_exit(void);
```

### 5. Add to Build

Add the new `.o` files to the `kdal-y` list in `Makefile`.

### 6. Test

1. Build with the new backend
2. Load module on target hardware
3. Verify `cat /sys/kernel/debug/kdal/devices` shows enumerated devices
4. Test with `kdality list` and `kdality info <device>`
5. Run write/read loopback verification

## Porting Checklist

- [ ] Backend ops fully implemented (no NULL function pointers)
- [ ] Backend registered with unique name
- [ ] `enumerate()` creates correctly typed `kdal_device` instances
- [ ] Devices have `backend` pointer set before driver attachment
- [ ] `priv` pointer holds per-device hardware state
- [ ] Read/write ops handle partial transfers correctly
- [ ] Error codes follow Linux kernel conventions (negative errno)
- [ ] No platform-specific types in public headers
- [ ] KUnit tests added for new backend
- [ ] Documentation updated with platform notes

## Reference: QEMU Backend

The QEMU backend (`src/backends/qemu/qemubackend.c`) serves as the reference
implementation. It uses a software ring buffer to emulate hardware I/O and
demonstrates the full lifecycle:

1. `init()` — log initialization
2. `enumerate()` — (devices created by drivers in first milestone)
3. `read()`/`write()` — route through `qemu_dev_ring`
4. `exit()` — cleanup

## Future: Radxa Orion O6

The target platform uses the CIX P1 SoC with:
- Arm Immortalis G720 GPU
- 30 TOPS NPU
- Standard ARM peripherals (UART, I2C, SPI, GPIO)

The porting path will use the generic backend with Device Tree enumeration
and MMIO register access through the kernel's `platform_driver` framework.
