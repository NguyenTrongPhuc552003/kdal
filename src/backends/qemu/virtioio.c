/*
 * Virtio I/O transport helpers for the QEMU backend.
 *
 * In a full deployment these routines would negotiate virtio queues
 * with the QEMU device model and move data through virtqueue scatter-
 * gather lists.  For the first milestone (no custom QEMU device) this
 * file provides the status/readiness API that drivers can query and
 * that the backend uses to decide whether to fall back to the ring-
 * buffer emulation path.
 *
 * The design intentionally separates transport readiness checks from
 * the backend read/write paths so that adding real virtio support
 * later only requires filling in these functions without changing the
 * backend or driver code.
 */

#include <linux/errno.h>
#include <linux/types.h>

#include <kdal/types.h>

/* Readiness flag — set to 1 when a real virtio device is negotiated */
static int virtio_io_ready_flag;

/*
 * kdal_qemu_virtio_io_ready() — check whether the virtio I/O
 * transport is negotiated and ready for use.
 *
 * Returns 1 if ready, 0 if not ready, or a negative errno.
 */
int kdal_qemu_virtio_io_ready(struct kdal_device *device)
{
	if (!device)
		return -EINVAL;

	return virtio_io_ready_flag;
}

/*
 * kdal_qemu_virtio_io_set_ready() — called by a future virtio probe
 * path when device negotiation completes.
 */
void kdal_qemu_virtio_io_set_ready(int ready)
{
	virtio_io_ready_flag = !!ready;
}

/*
 * kdal_qemu_virtio_io_xfer() — placeholder for a future virtio
 * scatter-gather transfer.  Returns -ENODEV until the transport is
 * ready so the backend falls back to the ring buffer path.
 */
ssize_t kdal_qemu_virtio_io_xfer(struct kdal_device *device,
				 void *buf, size_t len, int direction)
{
	if (!device || !buf)
		return -EINVAL;

	if (!virtio_io_ready_flag)
		return -ENODEV;

	/* Future: submit a virtqueue buffer and wait for completion */
	return -EOPNOTSUPP;
}