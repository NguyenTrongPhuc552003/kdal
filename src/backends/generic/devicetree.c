/*
 * Device Tree parsing helpers for the generic platform backend.
 *
 * On real hardware kdal_devicetree_scan() walks the flattened device
 * tree looking for nodes whose compatible string matches "kdal,*".
 * For each matching node it extracts the register base, IRQ number,
 * and device class, then calls the platform backend to create a
 * kdal_device.
 *
 * On QEMU this is a no-op; the QEMU backend creates its devices
 * directly during enumerate().
 */

#include <linux/errno.h>
#include <linux/of.h>

/*
 * kdal_devicetree_scan() — scan the DT for KDAL-compatible nodes.
 *
 * Returns 0 on success (including the case where no nodes are found)
 * or a negative errno if the DT walk fails.
 */
int kdal_devicetree_scan(void)
{
#ifdef CONFIG_OF
	struct device_node *np;

	for_each_compatible_node(np, NULL, "kdal,device") {
		/*
		 * Future: parse reg property, map MMIO, create
		 * kdal_device, and register with the generic backend.
		 *
		 *   const char *name = of_node_full_name(np);
		 *   struct resource res;
		 *   of_address_to_resource(np, 0, &res);
		 *   ...
		 */
	}
#endif
	return 0;
}

void kdal_devicetree_cleanup(void)
{
	/* Future: release OF references */
}