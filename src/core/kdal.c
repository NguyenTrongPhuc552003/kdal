/*
 * KDAL core lifecycle, driver attachment, and subsystem orchestration.
 *
 * This file is the bootstrapping centre of KDAL.  It initialises the
 * char-device control surface, the debugfs diagnostic tree, then
 * brings up backends and their dependent drivers in the correct order.
 */

#include <linux/errno.h>

#include <kdal/core/kdal.h>

/* ── backend init / exit forward declarations ───────────────────── */

int kdal_qemu_backend_init(void);
void kdal_qemu_backend_exit(void);

/* ── driver init / exit forward declarations ────────────────────── */

int kdal_uart_driver_init(void);
void kdal_uart_driver_exit(void);
int kdal_i2c_driver_init(void);
void kdal_i2c_driver_exit(void);
int kdal_spi_driver_init(void);
void kdal_spi_driver_exit(void);
int kdal_gpu_driver_init(void);
void kdal_gpu_driver_exit(void);

/* ── driver attachment ──────────────────────────────────────────── */

int kdal_attach_driver(struct kdal_device *device, struct kdal_driver *driver)
{
	int ret;

	if (!device || !driver || !driver->ops || !driver->ops->probe)
		return -EINVAL;

	if (device->driver)
		return -EBUSY;

	ret = driver->ops->probe(device);
	if (ret)
		return ret;

	device->driver = driver;

	return 0;
}

void kdal_detach_driver(struct kdal_device *device)
{
	if (!device || !device->driver)
		return;

	if (device->driver->ops && device->driver->ops->remove)
		device->driver->ops->remove(device);

	device->driver = NULL;
}

/* ── subsystem lifecycle ────────────────────────────────────────── */

int kdal_core_init(void)
{
	int ret;

	/* 1. chardev (/dev/kdal) */
	ret = kdal_chardev_init();
	if (ret)
		return ret;

	/* 2. debugfs (/sys/kernel/debug/kdal) */
	ret = kdal_debugfs_init();
	if (ret)
		goto err_chardev;

	/* 3. backends */
	ret = kdal_qemu_backend_init();
	if (ret)
		goto err_debugfs;

	/* 4. drivers – order does not matter; each driver discovers its
	 *    backend via kdal_find_backend(). */
	ret = kdal_uart_driver_init();
	if (ret)
		goto err_backend;

	ret = kdal_i2c_driver_init();
	if (ret)
		goto err_uart;

	ret = kdal_spi_driver_init();
	if (ret)
		goto err_i2c;

	ret = kdal_gpu_driver_init();
	if (ret)
		goto err_spi;

	return 0;

err_spi:
	kdal_spi_driver_exit();
err_i2c:
	kdal_i2c_driver_exit();
err_uart:
	kdal_uart_driver_exit();
err_backend:
	kdal_qemu_backend_exit();
err_debugfs:
	kdal_debugfs_exit();
err_chardev:
	kdal_chardev_exit();
	return ret;
}

void kdal_core_exit(void)
{
	kdal_gpu_driver_exit();
	kdal_spi_driver_exit();
	kdal_i2c_driver_exit();
	kdal_uart_driver_exit();
	kdal_qemu_backend_exit();
	kdal_debugfs_exit();
	kdal_chardev_exit();
	kdal_event_shutdown();
}
