/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KDAL module-internal declarations: functions and objects defined in one
 * source file of kdal.ko and used from another (the core's start-up, the
 * QEMU backend's helpers, the example drivers). One declaration here
 * replaces hand-written prototypes in each user, so a changed signature
 * is a compile error instead of a silent mismatch. Not part of the API
 * userspace or other modules see.
 */
#ifndef KDAL_CORE_INTERNAL_H
#define KDAL_CORE_INTERNAL_H

#include <linux/compiler_types.h>
#include <linux/types.h>

struct kdal_accel_ops;
struct kdal_device;
struct virtqueue;

/* ── backends: start-up and shut-down (called by src/core/kdal.c) ── */

int kdal_qemu_backend_init(void);
void kdal_qemu_backend_exit(void);
int kdal_soc_glue_init(void);
void kdal_soc_glue_exit(void);
int kdal_generic_platdev_init(void);
void kdal_generic_platdev_exit(void);
int kdal_devicetree_scan(void);
void kdal_devicetree_cleanup(void);

/* ── QEMU backend helpers ────────────────────────────────────────── */

int kdal_qemu_alloc_ring(struct kdal_device *device);
void kdal_qemu_free_ring(struct kdal_device *device);

u32 kdal_mmio_read32(void __iomem *base, u32 offset);
void kdal_mmio_write32(void __iomem *base, u32 offset, u32 value);
u16 kdal_mmio_read16(void __iomem *base, u32 offset);
void kdal_mmio_write16(void __iomem *base, u32 offset, u16 value);
u8 kdal_mmio_read8(void __iomem *base, u32 offset);
void kdal_mmio_write8(void __iomem *base, u32 offset, u8 value);

int kdal_qemu_virtio_io_ready(struct kdal_device *device);
void kdal_qemu_virtio_io_set_ready(int ready);
void kdal_qemu_virtio_io_set_vq(struct virtqueue *vq);
ssize_t kdal_qemu_virtio_io_xfer(struct kdal_device *device, void *buf,
				 size_t len, int direction);

int kdal_qemu_virtio_accel_ready(struct kdal_device *device);
void kdal_qemu_virtio_accel_set_ready(int ready);
void kdal_qemu_virtio_accel_set_vq(struct virtqueue *vq);
int kdal_qemu_virtio_accel_submit(struct kdal_device *device, const void *cmd,
				  size_t cmd_len);

/* ── example drivers ─────────────────────────────────────────────── */

int kdal_uart_driver_init(void);
void kdal_uart_driver_exit(void);
int kdal_i2c_driver_init(void);
void kdal_i2c_driver_exit(void);
int kdal_spi_driver_init(void);
void kdal_spi_driver_exit(void);
int kdal_gpu_driver_init(void);
void kdal_gpu_driver_exit(void);

/* the GPU example's accelerator operations (KUnit's test_accel.c) */
extern const struct kdal_accel_ops *kdal_gpu_accel_ops;

#endif /* KDAL_CORE_INTERNAL_H */
