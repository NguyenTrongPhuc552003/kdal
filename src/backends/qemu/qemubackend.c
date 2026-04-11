/*
 * QEMU virt-machine backend for KDAL.
 *
 * This backend simulates a hardware transport layer on the QEMU aarch64
 * virt machine.  For the first milestone (no actual virtio device on the
 * QEMU side) it uses a simple in-kernel ring buffer per device to emulate
 * byte-stream I/O.  Data written to a device can be read back, which is
 * enough to validate the full KDAL data path end to end.
 *
 * The ring buffer is stored in the backend's per-device private data.
 * When a real virtio or MMIO device is eventually wired into the QEMU
 * board model the read/write paths will be replaced with true transport
 * calls; the KDAL architecture guarantees that nothing above the backend
 * interface needs to change.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <kdal/api/common.h>
#include <kdal/backend.h>
#include <kdal/core/kdal.h>

/* ── per-device emulated I/O ring ───────────────────────────────── */

#define QEMU_RING_SIZE	4096

struct qemu_dev_ring {
	u8 buf[QEMU_RING_SIZE];
	unsigned int head;	/* next write position */
	unsigned int tail;	/* next read position  */
	unsigned int count;	/* bytes available     */
};

static struct qemu_dev_ring *qemu_ring_create(void)
{
	struct qemu_dev_ring *ring;

	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	return ring;
}

static void qemu_ring_destroy(struct qemu_dev_ring *ring)
{
	kfree(ring);
}

static ssize_t qemu_ring_write(struct qemu_dev_ring *ring,
			       const char *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (ring->count >= QEMU_RING_SIZE)
			break;

		ring->buf[ring->head % QEMU_RING_SIZE] = data[i];
		ring->head++;
		ring->count++;
	}

	return (ssize_t)i;
}

static ssize_t qemu_ring_read(struct qemu_dev_ring *ring,
			      char *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (ring->count == 0)
			break;

		data[i] = ring->buf[ring->tail % QEMU_RING_SIZE];
		ring->tail++;
		ring->count--;
	}

	return (ssize_t)i;
}

/* ── backend ops ────────────────────────────────────────────────── */

static int qemu_backend_init(struct kdal_backend *backend)
{
	if (!backend)
		return -EINVAL;

	pr_info("kdal: qemu backend initialised\n");
	return 0;
}

static void qemu_backend_exit(struct kdal_backend *backend)
{
	pr_info("kdal: qemu backend exiting\n");
}

static int qemu_backend_enumerate(struct kdal_backend *backend)
{
	if (!backend)
		return -EINVAL;

	pr_info("kdal: qemu backend enumeration complete\n");
	return 0;
}

static ssize_t qemu_backend_read(struct kdal_device *device, char *buf,
				 size_t count)
{
	struct qemu_dev_ring *ring;

	if (!device || !buf)
		return -EINVAL;

	ring = device->priv;
	if (!ring)
		return -EIO;

	return qemu_ring_read(ring, buf, count);
}

static ssize_t qemu_backend_write(struct kdal_device *device, const char *buf,
				  size_t count)
{
	struct qemu_dev_ring *ring;

	if (!device || !buf)
		return -EINVAL;

	ring = device->priv;
	if (!ring)
		return -EIO;

	return qemu_ring_write(ring, buf, count);
}

static long qemu_backend_ioctl(struct kdal_device *device, unsigned int cmd,
			       unsigned long arg)
{
	if (!device)
		return -EINVAL;

	return -ENOTTY;
}

static const struct kdal_backend_ops qemu_backend_ops = {
	.init = qemu_backend_init,
	.exit = qemu_backend_exit,
	.enumerate = qemu_backend_enumerate,
	.read = qemu_backend_read,
	.write = qemu_backend_write,
	.ioctl = qemu_backend_ioctl,
};

static struct kdal_backend qemu_backend = {
	.name = "qemu-virt",
	.feature_flags = KDAL_FEAT_EVENT_PUSH | KDAL_FEAT_POWER_MGMT,
	.ops = &qemu_backend_ops,
};

/* ── public helpers for drivers that need a ring on this backend ── */

int kdal_qemu_alloc_ring(struct kdal_device *device)
{
	struct qemu_dev_ring *ring;

	if (!device)
		return -EINVAL;

	ring = qemu_ring_create();
	if (!ring)
		return -ENOMEM;

	device->priv = ring;
	return 0;
}

void kdal_qemu_free_ring(struct kdal_device *device)
{
	if (!device || !device->priv)
		return;

	qemu_ring_destroy(device->priv);
	device->priv = NULL;
}

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_qemu_backend_init(void)
{
	int ret;

	ret = kdal_register_backend(&qemu_backend);
	if (ret)
		return ret;

	ret = qemu_backend.ops->init(&qemu_backend);
	if (ret) {
		kdal_unregister_backend(&qemu_backend);
		return ret;
	}

	return qemu_backend.ops->enumerate(&qemu_backend);
}

void kdal_qemu_backend_exit(void)
{
	if (qemu_backend.ops && qemu_backend.ops->exit)
		qemu_backend.ops->exit(&qemu_backend);

	kdal_unregister_backend(&qemu_backend);
}