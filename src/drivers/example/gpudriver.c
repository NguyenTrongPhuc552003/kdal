/*
 * GPU accelerator driver for the KDAL accelerator phase.
 *
 * Registers a "gpu0" device with accelerator queue / buffer ops.
 * This driver demonstrates the KDAL accel contract:
 *   - queue_create / queue_destroy   – command submission queues
 *   - buffer_map / buffer_unmap      – DMA-style buffer management
 *   - submit                         – command dispatch
 *
 * The QEMU ring buffer provides an emulated data path; the accel
 * ops track queue and buffer lifecycle for validation and testing.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <kdal/api/accel.h>
#include <kdal/api/common.h>
#include <kdal/core/kdal.h>
#include <kdal/ioctl.h>

/* Forward declarations from qemubackend.c */
int kdal_qemu_alloc_ring(struct kdal_device *device);
void kdal_qemu_free_ring(struct kdal_device *device);

/* ── GPU state ──────────────────────────────────────────────────── */

#define GPU_MAX_QUEUES		4
#define GPU_MAX_BUFFERS		16
#define GPU_KBUF_SIZE		4096

struct gpu_state {
	u32 active_queues;
	u32 mapped_buffers;
	u64 submit_count;
	u64 tx_bytes;
	u64 rx_bytes;
};

/* ── accel ops ──────────────────────────────────────────────────── */

static int gpu_queue_create(struct kdal_device *device,
			    struct kdal_accel_queue *queue)
{
	struct gpu_state *gs;

	if (!device || !queue)
		return -EINVAL;

	gs = device->driver->priv;
	if (!gs)
		return -EIO;

	if (gs->active_queues >= GPU_MAX_QUEUES)
		return -ENOSPC;

	queue->id = gs->active_queues;
	queue->depth = 64;
	gs->active_queues++;

	pr_info("kdal: gpu0 queue %u created (depth=%u)\n",
		queue->id, queue->depth);
	return 0;
}

static void gpu_queue_destroy(struct kdal_device *device,
			      struct kdal_accel_queue *queue)
{
	struct gpu_state *gs;

	if (!device || !queue)
		return;

	gs = device->driver->priv;
	if (gs && gs->active_queues > 0)
		gs->active_queues--;

	pr_info("kdal: gpu0 queue %u destroyed\n", queue->id);
}

static int gpu_buffer_map(struct kdal_device *device,
			  struct kdal_accel_buffer *buffer)
{
	struct gpu_state *gs;

	if (!device || !buffer)
		return -EINVAL;

	gs = device->driver->priv;
	if (!gs)
		return -EIO;

	if (gs->mapped_buffers >= GPU_MAX_BUFFERS)
		return -ENOSPC;

	/* Simulate IOVA assignment */
	buffer->iova = (u64)(gs->mapped_buffers + 1) * 0x10000;
	buffer->cpu_addr = NULL;	/* no real mapping on QEMU */
	gs->mapped_buffers++;

	pr_debug("kdal: gpu0 buffer mapped at iova=0x%llx size=%u\n",
		 buffer->iova, buffer->size);
	return 0;
}

static void gpu_buffer_unmap(struct kdal_device *device,
			     struct kdal_accel_buffer *buffer)
{
	struct gpu_state *gs;

	if (!device || !buffer)
		return;

	gs = device->driver->priv;
	if (gs && gs->mapped_buffers > 0)
		gs->mapped_buffers--;

	buffer->iova = 0;
	buffer->cpu_addr = NULL;
}

static int gpu_submit(struct kdal_device *device,
		      struct kdal_accel_queue *queue,
		      const void *cmd, size_t cmd_len)
{
	struct gpu_state *gs;

	if (!device || !queue || !cmd || cmd_len == 0)
		return -EINVAL;

	gs = device->driver->priv;
	if (!gs)
		return -EIO;

	gs->submit_count++;

	/* Write command payload to the backend ring for tracing */
	if (device->backend && device->backend->ops &&
	    device->backend->ops->write)
		device->backend->ops->write(device, cmd, cmd_len);

	kdal_emit_event(KDAL_EVENT_ACCEL_COMPLETE, device->name,
			(u32)gs->submit_count);

	pr_debug("kdal: gpu0 submit #%llu (%zu bytes)\n",
		 gs->submit_count, cmd_len);
	return 0;
}

static const struct kdal_accel_ops gpu_accel_ops = {
	.queue_create = gpu_queue_create,
	.queue_destroy = gpu_queue_destroy,
	.buffer_map = gpu_buffer_map,
	.buffer_unmap = gpu_buffer_unmap,
	.submit = gpu_submit,
};

/* ── conventional driver ops ────────────────────────────────────── */

static int gpu_probe(struct kdal_device *device)
{
	struct gpu_state *gs;
	int ret;

	if (!device)
		return -EINVAL;

	ret = kdal_qemu_alloc_ring(device);
	if (ret)
		return ret;

	gs = kzalloc(sizeof(*gs), GFP_KERNEL);
	if (!gs) {
		kdal_qemu_free_ring(device);
		return -ENOMEM;
	}

	device->driver->priv = gs;
	device->power_state = KDAL_POWER_ON;

	pr_info("kdal: gpu0 probed\n");
	return 0;
}

static void gpu_remove(struct kdal_device *device)
{
	if (!device)
		return;

	kfree(device->driver->priv);
	device->driver->priv = NULL;
	kdal_qemu_free_ring(device);
	device->power_state = KDAL_POWER_OFF;

	pr_info("kdal: gpu0 removed\n");
}

static ssize_t gpu_read(struct kdal_device *device, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct gpu_state *gs;
	char *kbuf;
	ssize_t got;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->read)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > GPU_KBUF_SIZE)
		count = GPU_KBUF_SIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	got = device->backend->ops->read(device, kbuf, count);
	if (got < 0) {
		kfree(kbuf);
		return got;
	}

	if (got > 0 && copy_to_user(buf, kbuf, got)) {
		kfree(kbuf);
		return -EFAULT;
	}

	gs = device->driver->priv;
	if (gs)
		gs->rx_bytes += got;

	kfree(kbuf);
	return got;
}

static ssize_t gpu_write(struct kdal_device *device, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct gpu_state *gs;
	char *kbuf;
	ssize_t written;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->write)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > GPU_KBUF_SIZE)
		count = GPU_KBUF_SIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	written = device->backend->ops->write(device, kbuf, count);

	if (written > 0) {
		gs = device->driver->priv;
		if (gs)
			gs->tx_bytes += written;
	}

	kfree(kbuf);
	return written;
}

static long gpu_ioctl(struct kdal_device *device, unsigned int cmd,
		      unsigned long arg)
{
	struct kdal_ioctl_info info;

	if (!device)
		return -EINVAL;

	switch (cmd) {
	case KDAL_IOCTL_GET_INFO:
		memset(&info, 0, sizeof(info));
		strscpy(info.name, device->name, sizeof(info.name));
		info.class_id = device->class_id;
		info.power_state = device->power_state;
		info.nr_caps = device->nr_caps;
		if (device->driver)
			info.feature_flags = device->driver->feature_flags;
		if (copy_to_user((void __user *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	default:
		return -ENOTTY;
	}
}

static int gpu_set_power_state(struct kdal_device *device,
			       enum kdal_power_state state)
{
	if (!device)
		return -EINVAL;

	device->power_state = state;
	return 0;
}

static const struct kdal_driver_ops gpu_driver_ops = {
	.probe = gpu_probe,
	.remove = gpu_remove,
	.read = gpu_read,
	.write = gpu_write,
	.ioctl = gpu_ioctl,
	.set_power_state = gpu_set_power_state,
};

static struct kdal_driver gpu_driver = {
	.name = "kdal-gpu",
	.class_id = KDAL_DEV_CLASS_GPU,
	.feature_flags = KDAL_FEAT_ACCEL_QUEUE | KDAL_FEAT_SHARED_MEM |
			 KDAL_FEAT_POWER_MGMT,
	.ops = &gpu_driver_ops,
};

static struct kdal_device gpu_device = {
	.name = "gpu0",
	.class_id = KDAL_DEV_CLASS_GPU,
	.power_state = KDAL_POWER_OFF,
};

/* Keep a module-level reference so tests can get at the accel ops */
const struct kdal_accel_ops *kdal_gpu_accel_ops = &gpu_accel_ops;

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_gpu_driver_init(void)
{
	int ret;

	gpu_device.backend = kdal_find_backend("qemu-virt");
	if (!gpu_device.backend)
		return -ENODEV;

	ret = kdal_register_driver(&gpu_driver);
	if (ret)
		return ret;

	ret = kdal_register_device(&gpu_device);
	if (ret) {
		kdal_unregister_driver(&gpu_driver);
		return ret;
	}

	ret = kdal_attach_driver(&gpu_device, &gpu_driver);
	if (ret) {
		kdal_unregister_device(&gpu_device);
		kdal_unregister_driver(&gpu_driver);
		return ret;
	}

	pr_info("kdal: gpu driver registered\n");
	return kdal_emit_event(KDAL_EVENT_DEVICE_ADDED, gpu_device.name, 0);
}

void kdal_gpu_driver_exit(void)
{
	kdal_emit_event(KDAL_EVENT_DEVICE_REMOVED, gpu_device.name, 0);
	kdal_detach_driver(&gpu_device);
	kdal_unregister_device(&gpu_device);
	kdal_unregister_driver(&gpu_driver);
}