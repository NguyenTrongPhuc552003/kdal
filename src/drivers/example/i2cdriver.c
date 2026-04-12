/*
 * I2C example driver for the second KDAL peripheral milestone.
 *
 * Registers an "i2c0" device backed by the QEMU virt backend.
 * Supports the full KDAL driver contract: probe/remove, read/write
 * with proper copy_to_user / copy_from_user boundaries, ioctl for
 * info queries, and power management.
 *
 * In a real deployment the backend I/O path would route through an
 * actual I2C controller register interface; here the QEMU ring
 * buffer emulates the transport for validation purposes.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>
#include <kdal/ioctl.h>

/* Forward declarations from qemubackend.c */
int kdal_qemu_alloc_ring(struct kdal_device *device);
void kdal_qemu_free_ring(struct kdal_device *device);

#define I2C_KBUF_SIZE 256

struct i2c_config {
	u32 bus_speed_hz; /* 100000 = standard, 400000 = fast */
	u32 slave_addr;
	u64 tx_bytes;
	u64 rx_bytes;
};

/* ── driver ops ─────────────────────────────────────────────────── */

static int i2c_probe(struct kdal_device *device)
{
	struct i2c_config *cfg;
	int ret;

	if (!device)
		return -EINVAL;

	ret = kdal_qemu_alloc_ring(device);
	if (ret)
		return ret;

	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		kdal_qemu_free_ring(device);
		return -ENOMEM;
	}

	cfg->bus_speed_hz = 100000;
	cfg->slave_addr = 0x50;
	device->driver->priv = cfg;
	device->power_state = KDAL_POWER_ON;

	pr_info("kdal: i2c0 probed (speed=%u Hz, addr=0x%02x)\n",
		cfg->bus_speed_hz, cfg->slave_addr);
	return 0;
}

static void i2c_remove(struct kdal_device *device)
{
	if (!device)
		return;

	kfree(device->driver->priv);
	device->driver->priv = NULL;
	kdal_qemu_free_ring(device);
	device->power_state = KDAL_POWER_OFF;

	pr_info("kdal: i2c0 removed\n");
}

static ssize_t i2c_read(struct kdal_device *device, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct i2c_config *cfg;
	char *kbuf;
	ssize_t got;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->read)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > I2C_KBUF_SIZE)
		count = I2C_KBUF_SIZE;

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

	cfg = device->driver->priv;
	if (cfg)
		cfg->rx_bytes += got;

	kfree(kbuf);
	return got;
}

static ssize_t i2c_write(struct kdal_device *device, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t written;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->write)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > I2C_KBUF_SIZE)
		count = I2C_KBUF_SIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	written = device->backend->ops->write(device, kbuf, count);

	if (written > 0) {
		struct i2c_config *cfg;

		cfg = device->driver->priv;
		if (cfg)
			cfg->tx_bytes += written;
	}

	kfree(kbuf);
	return written;
}

static long i2c_ioctl(struct kdal_device *device, unsigned int cmd,
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

static int i2c_set_power_state(struct kdal_device *device,
			       enum kdal_power_state state)
{
	if (!device)
		return -EINVAL;

	device->power_state = state;
	return 0;
}

static const struct kdal_driver_ops i2c_driver_ops = {
	.probe = i2c_probe,
	.remove = i2c_remove,
	.read = i2c_read,
	.write = i2c_write,
	.ioctl = i2c_ioctl,
	.set_power_state = i2c_set_power_state,
};

static struct kdal_driver i2c_driver = {
	.name = "kdal-i2c",
	.class_id = KDAL_DEV_CLASS_I2C,
	.feature_flags = KDAL_FEAT_POWER_MGMT,
	.ops = &i2c_driver_ops,
};

static struct kdal_device i2c_device = {
	.name = "i2c0",
	.class_id = KDAL_DEV_CLASS_I2C,
	.power_state = KDAL_POWER_OFF,
};

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_i2c_driver_init(void)
{
	int ret;

	i2c_device.backend = kdal_find_backend("qemu-virt");
	if (!i2c_device.backend)
		return -ENODEV;

	ret = kdal_register_driver(&i2c_driver);
	if (ret)
		return ret;

	ret = kdal_register_device(&i2c_device);
	if (ret) {
		kdal_unregister_driver(&i2c_driver);
		return ret;
	}

	ret = kdal_attach_driver(&i2c_device, &i2c_driver);
	if (ret) {
		kdal_unregister_device(&i2c_device);
		kdal_unregister_driver(&i2c_driver);
		return ret;
	}

	pr_info("kdal: i2c driver registered\n");
	return kdal_emit_event(KDAL_EVENT_DEVICE_ADDED, i2c_device.name, 0);
}

void kdal_i2c_driver_exit(void)
{
	kdal_emit_event(KDAL_EVENT_DEVICE_REMOVED, i2c_device.name, 0);
	kdal_detach_driver(&i2c_device);
	kdal_unregister_device(&i2c_device);
	kdal_unregister_driver(&i2c_driver);
}
