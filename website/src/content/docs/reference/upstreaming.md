---
title: Upstreaming Strategy
description: Engineering practices that keep the Linux kernel upstreaming path viable.
---

## Current Status

KDAL is developed **out-of-tree** under GPL-3.0-or-later. Upstreaming to the
Linux kernel (GPLv2-only) would require a license change and community review.
This document outlines the engineering practices that keep that path viable.

## Design Principles

### Minimal public API

The user-kernel ABI consists of **6 ioctl commands**. No new system calls, no
new filesystem types, no new socket families. This minimises the surface that
kernel maintainers must evaluate.

### Linux kernel coding style

All code follows `checkpatch.pl` guidelines:

- Tabs for indentation, 80-column soft limit
- `/* C89 comments */` in kernel code
- `kdal_*` prefix for all exported symbols
- Proper `goto` error unwinding

### Standard kernel APIs only

| KDAL usage       | Kernel API                                         |
| ---------------- | -------------------------------------------------- |
| Character device | `misc_register()` / `misc_deregister()`            |
| Debug interface  | `debugfs_create_dir()` / `debugfs_create_file()`   |
| Synchronisation  | `struct mutex` / `mutex_lock()` / `mutex_unlock()` |
| Linked lists     | `struct list_head` / `list_for_each_entry_safe()`  |
| Memory           | `kmalloc()` / `kfree()` / `kzalloc()`              |
| User copy        | `copy_to_user()` / `copy_from_user()`              |
| Events           | `wait_queue_head_t` / `wake_up_interruptible()`    |

No custom allocators, no out-of-tree dependencies.

### Backend isolation

Platform-specific code lives exclusively under `src/backends/`. The core and
driver layers never reference SoC registers, DMA engines, or bus controllers
directly. The core can be reviewed independently of any hardware.

### Versioned ABI

`KDAL_IOCTL_GET_VERSION` returns major/minor/patch. Any ABI change increments
the version. Userspace tools check compatibility:

```c
struct kdal_ioctl_version ver;
ioctl(fd, KDAL_IOCTL_GET_VERSION, &ver);
if (ver.major != EXPECTED_MAJOR) { /* incompatible */ }
```

## Upstreaming Checklist

- [ ] Switch license to GPLv2-only (`MODULE_LICENSE("GPL")` is already set)
- [ ] Pass `checkpatch.pl --strict` on all files
- [ ] Pass `sparse` with zero warnings
- [ ] Achieve `cppcheck` clean run
- [ ] Write `Documentation/driver-api/kdal.rst` in kernel docs format
- [ ] Submit to `linux-kernel@vger.kernel.org` as RFC patch series
- [ ] Address reviewer feedback (expect 3–5 revision cycles)
- [ ] Find a subsystem maintainer willing to sponsor

## Patch Series Structure

Recommended submission order:

1. **Headers** - `include/kdal/`: types, API contracts, ioctl definitions
2. **Core** - `src/core/`: lifecycle, registry, events, chardev, debugfs
3. **QEMU backend** - `src/backends/qemu/`: reference implementation
4. **Example drivers** - `src/drivers/example/`: UART, I2C, SPI, GPU
5. **Tests** - `tests/kunit/`: in-kernel test suite
6. **Documentation** - kernel `Documentation/` format

## Timeline

| Phase                  | Gate                                      |
| ---------------------- | ----------------------------------------- |
| Out-of-tree validation | Thesis defense                            |
| Community RFC          | Post-defense, after real hardware results |
| Formal submission      | After 1–2 RFC revision cycles             |
| Acceptance             | Depends on maintainer bandwidth           |
