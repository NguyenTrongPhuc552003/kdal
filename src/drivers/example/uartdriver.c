/*
 * UART example driver — the first end-to-end proof for KDAL.
 *
 * This driver registers a "uart0" device backed by the QEMU virt
 * backend.  Data written by userspace goes into the QEMU emulation
 * ring buffer and can be read back, completing the full KDAL data
 * path: userspace → chardev → driver → backend → ring → backend →
 * driver → chardev → userspace.
 *
 * Proper copy_to_user / copy_from_user boundaries are enforced here
 * so that the backend ring buffer only ever sees kernel memory.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>
#include <kdal/ioctl.h>

/* Forward declaration from qemubackend.c */
int kdal_qemu_alloc_ring(struct kdal_device *device);
void kdal_qemu_free_ring(struct kdal_device *device);

/* ── UART-specific configuration (stored in driver priv) ──────── */

#define UART_DEFAULT_BAUD	115200
#define UART_KBUF_SIZE		4096

struct uart_config {
	u32 baud_rate;
	u32 data_bits;	/* 5–8 */
	u32 stop_bits;	/* 1–2 */
	u32 parity;	/* 0=none, 1=odd, 2=even */
	u64 tx_bytes;
	u64 rx_bytes;
};

/* ── driver ops ─────────────────────────────────────────────────── */

static int uart_probe(struct kdal_device *device)
{
	struct uart_config *cfg;
	int ret;

	if (!device)
		return -EINVAL;

	/* Allocate emulated ring buffer on the backend */
	ret = kdal_qemu_alloc_ring(device);
	if (ret)
		return ret;

	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		kdal_qemu_free_ring(device);
		return -ENOMEM;
	}

	cfg->baud_rate = UART_DEFAULT_BAUD;
	cfg->data_bits = 8;
	cfg->stop_bits = 1;
	cfg->parity = 0;
	device->driver->priv = cfg;
	device->power_state = KDAL_POWER_ON;

	pr_info("kdal: uart0 probed (baud=%u)\n", cfg->baud_rate);
	return 0;
}

static void uart_remove(struct kdal_device *device)
{
	if (!device)
		return;

	kfree(device->driver->priv);
	device->driver->priv = NULL;
	kdal_qemu_free_ring(device);
	device->power_state = KDAL_POWER_OFF;

	pr_info("kdal: uart0 removed\n");
}

static ssize_t uart_read(struct kdal_device *device, char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct uart_config *cfg;
	char *kbuf;
	ssize_t got;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->read)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > UART_KBUF_SIZE)
		count = UART_KBUF_SIZE;

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

static ssize_t uart_write(struct kdal_device *device, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct uart_config *cfg;
	char *kbuf;
	ssize_t written;

	if (!device || !device->backend || !device->backend->ops ||
	    !device->backend->ops->write)
		return -EOPNOTSUPP;

	if (device->power_state != KDAL_POWER_ON)
		return -EIO;

	if (count > UART_KBUF_SIZE)
		count = UART_KBUF_SIZE;

	kbuf = kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	written = device->backend->ops->write(device, kbuf, count);

	if (written > 0) {
		cfg = device->driver->priv;
		if (cfg)
			cfg->tx_bytes += written;
	}

	kfree(kbuf);
	return written;
}

static long uart_ioctl(struct kdal_device *device, unsigned int cmd,
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

static int uart_set_power_state(struct kdal_device *device,
				enum kdal_power_state state)
{
	if (!device)
		return -EINVAL;

	switch (state) {
	case KDAL_POWER_ON:
		pr_info("kdal: uart0 powered on\n");
		break;
	case KDAL_POWER_SUSPEND:
		pr_info("kdal: uart0 suspended\n");
		break;
	case KDAL_POWER_OFF:
		pr_info("kdal: uart0 powered off\n");
		break;
	}

	device->power_state = state;
	return 0;
}

static const struct kdal_driver_ops uart_driver_ops = {
	.probe = uart_probe,
	.remove = uart_remove,
	.read = uart_read,
	.write = uart_write,
	.ioctl = uart_ioctl,
	.set_power_state = uart_set_power_state,
};

static struct kdal_driver uart_driver = {
	.name = "kdal-uart",
	.class_id = KDAL_DEV_CLASS_UART,
	.feature_flags = KDAL_FEAT_EVENT_PUSH | KDAL_FEAT_POWER_MGMT,
	.ops = &uart_driver_ops,
};

static struct kdal_device uart_device = {
	.name = "uart0",
	.class_id = KDAL_DEV_CLASS_UART,
	.power_state = KDAL_POWER_OFF,
};

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_uart_driver_init(void)
{
	int ret;

	uart_device.backend = kdal_find_backend("qemu-virt");
	if (!uart_device.backend)
		return -ENODEV;

	ret = kdal_register_driver(&uart_driver);
	if (ret)
		return ret;

	ret = kdal_register_device(&uart_device);
	if (ret) {
		kdal_unregister_driver(&uart_driver);
		return ret;
	}

	ret = kdal_attach_driver(&uart_device, &uart_driver);
	if (ret) {
		kdal_unregister_device(&uart_device);
		kdal_unregister_driver(&uart_driver);
		return ret;
	}

	pr_info("kdal: uart driver registered\n");
	return kdal_emit_event(KDAL_EVENT_DEVICE_ADDED, uart_device.name, 0);
}

void kdal_uart_driver_exit(void)
{
	kdal_emit_event(KDAL_EVENT_DEVICE_REMOVED, uart_device.name, 0);
	kdal_detach_driver(&uart_device);
	kdal_unregister_device(&uart_device);
	kdal_unregister_driver(&uart_driver);
}