/*
 * SPI example driver for the second KDAL peripheral milestone.
 *
 * Registers an "spi0" device backed by the QEMU virt backend.
 * Uses the same ring-buffered I/O emulation as the UART and I2C
 * drivers, demonstrating KDAL's device-class-agnostic backend model.
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

#define SPI_KBUF_SIZE 4096

struct spi_config {
	u32 clock_hz; /* bus clock speed */
	u32 mode; /* SPI mode 0–3 */
	u32 bits_per_word;
	u64 tx_bytes;
	u64 rx_bytes;
};

/* ── driver ops ─────────────────────────────────────────────────── */

static int spi_probe(struct kdal_device *device)
{
	struct spi_config *cfg;
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

	cfg->clock_hz = 1000000; /* 1 MHz default */
	cfg->mode = 0;
	cfg->bits_per_word = 8;
	device->driver->priv = cfg;
	device->power_state = KDAL_POWER_ON;

	pr_info("kdal: spi0 probed (clock=%u Hz, mode=%u)\n", cfg->clock_hz,
		cfg->mode);
	return 0;
}

static void spi_remove(struct kdal_device *device)
{
	if (!device)
		return;

	kfree(device->driver->priv);
	device->driver->priv = NULL;
	kdal_qemu_free_ring(device);
	device->power_state = KDAL_POWER_OFF;

	pr_info("kdal: spi0 removed\n");
}

static ssize_t spi_read(struct kdal_device *device, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct spi_config *cfg;
	char *kbuf;
	ssize_t got;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->read)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > SPI_KBUF_SIZE)
		count = SPI_KBUF_SIZE;

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

static ssize_t spi_write(struct kdal_device *device, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	char *kbuf;
	ssize_t written;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->write)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > SPI_KBUF_SIZE)
		count = SPI_KBUF_SIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	written = device->backend->ops->write(device, kbuf, count);

	if (written > 0) {
		struct spi_config *cfg;

		cfg = device->driver->priv;
		if (cfg)
			cfg->tx_bytes += written;
	}

	kfree(kbuf);
	return written;
}

static long spi_ioctl(struct kdal_device *device, unsigned int cmd,
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

static int spi_set_power_state(struct kdal_device *device,
			       enum kdal_power_state state)
{
	if (!device)
		return -EINVAL;

	device->power_state = state;
	return 0;
}

static const struct kdal_driver_ops spi_driver_ops = {
	.probe = spi_probe,
	.remove = spi_remove,
	.read = spi_read,
	.write = spi_write,
	.ioctl = spi_ioctl,
	.set_power_state = spi_set_power_state,
};

static struct kdal_driver spi_driver = {
	.name = "kdal-spi",
	.class_id = KDAL_DEV_CLASS_SPI,
	.feature_flags = KDAL_FEAT_POWER_MGMT,
	.ops = &spi_driver_ops,
};

static struct kdal_device spi_device = {
	.name = "spi0",
	.class_id = KDAL_DEV_CLASS_SPI,
	.power_state = KDAL_POWER_OFF,
};

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_spi_driver_init(void)
{
	int ret;

	spi_device.backend = kdal_find_backend("qemu-virt");
	if (!spi_device.backend)
		return -ENODEV;

	ret = kdal_register_driver(&spi_driver);
	if (ret)
		return ret;

	ret = kdal_register_device(&spi_device);
	if (ret) {
		kdal_unregister_driver(&spi_driver);
		return ret;
	}

	ret = kdal_attach_driver(&spi_device, &spi_driver);
	if (ret) {
		kdal_unregister_device(&spi_device);
		kdal_unregister_driver(&spi_driver);
		return ret;
	}

	pr_info("kdal: spi driver registered\n");
	return kdal_emit_event(KDAL_EVENT_DEVICE_ADDED, spi_device.name, 0);
}

void kdal_spi_driver_exit(void)
{
	kdal_emit_event(KDAL_EVENT_DEVICE_REMOVED, spi_device.name, 0);
	kdal_detach_driver(&spi_device);
	kdal_unregister_device(&spi_device);
	kdal_unregister_driver(&spi_driver);
}
