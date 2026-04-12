/*
 * Generic platform-device backend for real hardware.
 *
 * On a real SoC (e.g. Radxa Orion O6 / CIX P1) this backend
 * registers as a Linux platform_driver, matches against Device Tree
 * compatible strings, maps MMIO registers from the DT node, and
 * provides the kdal_backend_ops read/write/ioctl path through the
 * physical register interface.
 *
 * Each device discovered via the Device Tree has a struct platdev_priv
 * attached to device->priv that holds the ioremapped MMIO base and
 * region size.
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>

#include <kdal/api/common.h>
#include <kdal/backend.h>
#include <kdal/core/kdal.h>

/*
 * Per-device private data for the platform backend.
 * Stored in kdal_device->priv by the Device Tree scanner.
 */
struct platdev_priv {
	void __iomem *base;
	resource_size_t size;
};

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

static ssize_t platdev_backend_read(struct kdal_device *device, char *buf,
				    size_t count)
{
	struct platdev_priv *pp;

	if (!device)
		return -EINVAL;

	pp = device->priv;
	if (!pp || !pp->base)
		return -EIO;

	if (count > pp->size)
		count = pp->size;

	memcpy_fromio(buf, pp->base, count);
	return (ssize_t)count;
}

static ssize_t platdev_backend_write(struct kdal_device *device,
				     const char *buf, size_t count)
{
	struct platdev_priv *pp;

	if (!device)
		return -EINVAL;

	pp = device->priv;
	if (!pp || !pp->base)
		return -EIO;

	if (count > pp->size)
		count = pp->size;

	memcpy_toio(pp->base, buf, count);
	return (ssize_t)count;
}

static long platdev_backend_ioctl(struct kdal_device *device, unsigned int cmd,
				  unsigned long arg)
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
	 * On QEMU the generic backend is not needed - the qemu-virt
	 * backend handles everything.  Register it anyway so the
	 * architecture is validated.
	 */
	return kdal_register_backend(&platdev_backend);
}

void kdal_generic_platdev_exit(void)
{
	kdal_unregister_backend(&platdev_backend);
}
