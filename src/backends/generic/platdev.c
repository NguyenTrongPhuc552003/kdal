/*
 * Generic platform-device backend for real hardware.
 *
 * On a real SoC (e.g. Radxa Orion O6 / CIX P1) this backend will
 * register as a Linux platform_driver, match against Device Tree
 * compatible strings, map MMIO registers from the DT node, and
 * provide the kdal_backend_ops read/write/ioctl path through the
 * physical register interface.
 *
 * For the first milestone this module provides the initialisation
 * entry point and the backend structure so that the build succeeds
 * and the architecture is in place for future platform bring-up.
 */

#include <linux/errno.h>
#include <linux/module.h>

#include <kdal/api/common.h>
#include <kdal/backend.h>
#include <kdal/core/kdal.h>

static int platdev_backend_init(struct kdal_backend *backend)
{
	pr_info("kdal: generic platform backend init (no-op on QEMU)\n");
	return 0;
}

static void platdev_backend_exit(struct kdal_backend *backend)
{
	pr_info("kdal: generic platform backend exit\n");
}

static int platdev_backend_enumerate(struct kdal_backend *backend)
{
	/* On real hardware: walk DT nodes, create kdal_device per node */
	return 0;
}

static ssize_t platdev_backend_read(struct kdal_device *device,
				    char *buf, size_t count)
{
	if (!device)
		return -EINVAL;

	/* Future: read from MMIO-mapped register bank */
	return -EOPNOTSUPP;
}

static ssize_t platdev_backend_write(struct kdal_device *device,
				     const char *buf, size_t count)
{
	if (!device)
		return -EINVAL;

	/* Future: write to MMIO-mapped register bank */
	return -EOPNOTSUPP;
}

static long platdev_backend_ioctl(struct kdal_device *device,
				  unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static const struct kdal_backend_ops platdev_backend_ops = {
	.init = platdev_backend_init,
	.exit = platdev_backend_exit,
	.enumerate = platdev_backend_enumerate,
	.read = platdev_backend_read,
	.write = platdev_backend_write,
	.ioctl = platdev_backend_ioctl,
};

static struct kdal_backend platdev_backend = {
	.name = "generic-platdev",
	.feature_flags = KDAL_FEAT_POWER_MGMT,
	.ops = &platdev_backend_ops,
};

int kdal_generic_platdev_init(void)
{
	/*
	 * On QEMU the generic backend is not needed — the qemu-virt
	 * backend handles everything.  Register it anyway so the
	 * architecture is validated.
	 */
	return kdal_register_backend(&platdev_backend);
}

void kdal_generic_platdev_exit(void)
{
	kdal_unregister_backend(&platdev_backend);
}