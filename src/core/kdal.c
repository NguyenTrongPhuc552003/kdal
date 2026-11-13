/*
 * KDAL core lifecycle, driver attachment, and subsystem orchestration.
 *
 * This file is the bootstrapping centre of KDAL.  It initialises the
 * char-device control surface, the debugfs diagnostic tree, then
 * brings up backends and their dependent drivers in the correct order.
 */

#include <linux/errno.h>

#include <kdal/core/kdal.h>
#include <kdal/core/internal.h>

/* ── driver attachment ──────────────────────────────────────────── */

int kdal_attach_driver(struct kdal_device *device, struct kdal_driver *driver)
{
	int ret;

	if (!device || !driver || !driver->ops || !driver->ops->probe)
		return -EINVAL;

	if (device->driver)
		return -EBUSY;

	/*
	 * Drivers reach their state through device->driver during probe
	 * (uartdriver.c sets device->driver->priv), so bind before probing
	 * and unbind if probe fails. Binding after probe made every example
	 * driver dereference NULL on load.
	 */
	device->driver = driver;
	ret = driver->ops->probe(device);
	if (ret) {
		device->driver = NULL;
		return ret;
	}

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

	/*
	 * 3b. generic platform backend: SoC glue, then devices described in
	 * the device tree ("kdal,device" nodes). Before the KDAL audit (K15)
	 * none of this was called, so DT devices never appeared.
	 */
	ret = kdal_soc_glue_init();
	if (ret)
		goto err_qemu;
	ret = kdal_generic_platdev_init();
	if (ret)
		goto err_glue;
	ret = kdal_devicetree_scan();
	if (ret)
		goto err_platdev;

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
	kdal_devicetree_cleanup();
err_platdev:
	kdal_generic_platdev_exit();
err_glue:
	kdal_soc_glue_exit();
err_qemu:
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
	kdal_devicetree_cleanup();
	kdal_generic_platdev_exit();
	kdal_soc_glue_exit();
	kdal_qemu_backend_exit();
	kdal_debugfs_exit();
	kdal_chardev_exit();
	kdal_event_shutdown();
}
