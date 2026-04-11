/*
 * Virtio accelerator transport hooks for the QEMU backend.
 *
 * This file will carry the virtio-gpu / virtio-accel style command
 * submission path once a custom QEMU device is available.  Until then
 * it offers readiness checks and a submit stub so the GPU driver can
 * detect whether to fall back to the ring-buffer emulation or use a
 * real hardware-backed command queue.
 */

#include <linux/errno.h>
#include <linux/types.h>

#include <kdal/types.h>

static int virtio_accel_ready_flag;

/*
 * kdal_qemu_virtio_accel_ready() — check whether the virtio accel
 * transport has been negotiated.
 *
 * Returns 1 if ready, 0 if not, or negative errno.
 */
int kdal_qemu_virtio_accel_ready(struct kdal_device *device)
{
	if (!device)
		return -EINVAL;

	return virtio_accel_ready_flag;
}

void kdal_qemu_virtio_accel_set_ready(int ready)
{
	virtio_accel_ready_flag = !!ready;
}

/*
 * kdal_qemu_virtio_accel_submit() — submit a command buffer through
 * the virtio accel transport.  Returns -ENODEV until the transport
 * is negotiated.
 */
int kdal_qemu_virtio_accel_submit(struct kdal_device *device,
				  const void *cmd, size_t cmd_len)
{
	if (!device || !cmd || cmd_len == 0)
		return -EINVAL;

	if (!virtio_accel_ready_flag)
		return -ENODEV;

	/* Future: format a virtio-gpu/accel command descriptor and submit */
	return -EOPNOTSUPP;
}