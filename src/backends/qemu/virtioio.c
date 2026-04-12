/*
 * Virtio I/O transport helpers for the QEMU backend.
 *
 * In a full deployment these routines negotiate virtio queues with
 * the QEMU device model and move data through virtqueue scatter-
 * gather lists.  When no custom QEMU virtio device is present
 * (vq_io == NULL) the transfer function returns -EOPNOTSUPP so the
 * backend falls back to the ring-buffer emulation path.
 *
 * The design intentionally separates transport readiness checks from
 * the backend read/write paths so that adding real virtio support
 * only requires wiring the virtqueue from the probe callback into
 * kdal_qemu_virtio_io_set_vq() - nothing above the backend interface
 * needs to change.
 */

#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
#include <linux/virtio.h>

#include <kdal/types.h>

/* Readiness flag - set to 1 when a real virtio device is negotiated */
static int virtio_io_ready_flag;

/* Virtqueue for I/O transfers - set by the virtio probe path */
static struct virtqueue *vq_io;

/*
 * kdal_qemu_virtio_io_ready() - check whether the virtio I/O
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
 * kdal_qemu_virtio_io_set_ready() - called by the virtio probe
 * path when device negotiation completes.
 */
void kdal_qemu_virtio_io_set_ready(int ready)
{
	virtio_io_ready_flag = !!ready;
}

/*
 * kdal_qemu_virtio_io_set_vq() - called by the virtio probe path
 * to hand over the negotiated virtqueue for I/O transfers.
 */
void kdal_qemu_virtio_io_set_vq(struct virtqueue *vq)
{
	vq_io = vq;
}

/*
 * kdal_qemu_virtio_io_xfer() - submit a virtio scatter-gather
 * transfer.  @direction: 0 = device-to-host (read), 1 = host-to-device
 * (write).  Returns bytes transferred or a negative errno.
 *
 * When vq_io is NULL (no custom QEMU device) returns -EOPNOTSUPP so
 * the backend falls back to the ring-buffer emulation path.
 */
ssize_t kdal_qemu_virtio_io_xfer(struct kdal_device *device, void *buf,
				 size_t len, int direction)
{
	struct scatterlist sg;
	unsigned int sg_out, sg_in;
	unsigned int xfer_len;
	const void *completed;
	int ret;

	if (!device || !buf)
		return -EINVAL;

	if (!virtio_io_ready_flag)
		return -ENODEV;

	if (!vq_io)
		return -EOPNOTSUPP;

	sg_init_one(&sg, buf, len);

	if (direction) {
		/* host-to-device (write): outgoing buffer */
		sg_out = 1;
		sg_in = 0;
	} else {
		/* device-to-host (read): incoming buffer */
		sg_out = 0;
		sg_in = 1;
	}

	ret = virtqueue_add_sgs(vq_io, (struct scatterlist *[]){ &sg }, sg_out,
				sg_in, buf, GFP_KERNEL);
	if (ret < 0)
		return ret;

	virtqueue_kick(vq_io);

	/* Wait for the host to process the buffer */
	while (!(completed = virtqueue_get_buf(vq_io, &xfer_len)))
		cpu_relax();

	return (ssize_t)xfer_len;
}
