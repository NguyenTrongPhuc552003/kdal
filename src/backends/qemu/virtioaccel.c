/*
 * Virtio accelerator transport hooks for the QEMU backend.
 *
 * This file carries the virtio-gpu / virtio-accel style command
 * submission path.  When a custom QEMU virtio-accel device is present
 * and the virtqueue has been handed over via set_vq(), commands are
 * submitted through the virtqueue scatter-gather interface.  When no
 * device is available (vq_accel == NULL) the submit function returns
 * -EOPNOTSUPP so the GPU driver falls back to the ring-buffer
 * emulation path.
 */

#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/types.h>
#include <linux/virtio.h>

#include <kdal/types.h>

static int virtio_accel_ready_flag;

/* Virtqueue for accelerator command submission */
static struct virtqueue *vq_accel;

/*
 * kdal_qemu_virtio_accel_ready() - check whether the virtio accel
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
 * kdal_qemu_virtio_accel_set_vq() - called by the virtio probe path
 * to hand over the negotiated virtqueue for accel command submission.
 */
void kdal_qemu_virtio_accel_set_vq(struct virtqueue *vq)
{
	vq_accel = vq;
}

/*
 * kdal_qemu_virtio_accel_submit() - submit a command buffer through
 * the virtio accel transport.
 *
 * Returns 0 on success, -ENODEV if the transport is not negotiated,
 * -EOPNOTSUPP if no virtqueue is available (fallback to ring buffer),
 * or a negative errno on failure.
 */
int kdal_qemu_virtio_accel_submit(struct kdal_device *device, const void *cmd,
				  size_t cmd_len)
{
	struct scatterlist sg;
	unsigned int resp_len;
	const void *completed;
	int ret;

	if (!device || !cmd || cmd_len == 0)
		return -EINVAL;

	if (!virtio_accel_ready_flag)
		return -ENODEV;

	if (!vq_accel)
		return -EOPNOTSUPP;

	sg_init_one(&sg, cmd, cmd_len);

	ret = virtqueue_add_outbuf(vq_accel, &sg, 1, (void *)cmd, GFP_KERNEL);
	if (ret < 0)
		return ret;

	virtqueue_kick(vq_accel);

	/* Wait for the host to consume the command */
	while (!(completed = virtqueue_get_buf(vq_accel, &resp_len)))
		cpu_relax();

	return 0;
}
