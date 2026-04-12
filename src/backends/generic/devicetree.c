/*
 * Device Tree parsing helpers for the generic platform backend.
 *
 * On real hardware kdal_devicetree_scan() walks the flattened device
 * tree looking for nodes whose compatible string matches "kdal,*".
 * For each matching node it extracts the register base and region
 * size, ioremaps the MMIO range, creates a kdal_device with a
 * platdev_priv, and registers it with the KDAL core.
 *
 * On QEMU (CONFIG_OF undefined or no matching DT nodes) both
 * functions are no-ops; the QEMU backend creates its devices
 * directly during enumerate().
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>

#ifdef CONFIG_OF
#include <linux/of_address.h>
#endif

#include <kdal/core/kdal.h>

/*
 * Per-device private data mirrors the struct in platdev.c so
 * the platform backend read/write paths can use it directly.
 */
struct platdev_priv {
	void __iomem *base;
	resource_size_t size;
};

/*
 * Tracks each device created during DT scan so that cleanup can
 * unregister, iounmap, and free them.
 */
struct kdal_dt_entry {
	struct kdal_device dev;
	struct platdev_priv priv;
	char name[32];
	struct list_head node;
};

static LIST_HEAD(dt_device_list);
static DEFINE_MUTEX(dt_lock);

/*
 * kdal_devicetree_scan() - scan the DT for KDAL-compatible nodes.
 *
 * Returns 0 on success (including the case where no nodes are found)
 * or a negative errno if the DT walk fails.
 */
int kdal_devicetree_scan(void)
{
#ifdef CONFIG_OF
	struct device_node *np;

	for_each_compatible_node(np, NULL, "kdal,device") {
		struct kdal_dt_entry *entry;
		struct resource res;
		const char *node_name;
		int ret;

		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			pr_warn("kdal: DT node %pOFn - cannot read reg property (%d)\n",
				np, ret);
			continue;
		}

		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			of_node_put(np);
			return -ENOMEM;
		}

		entry->priv.base = ioremap(res.start, resource_size(&res));
		if (!entry->priv.base) {
			pr_warn("kdal: DT node %pOFn - ioremap failed\n", np);
			kfree(entry);
			continue;
		}
		entry->priv.size = resource_size(&res);

		node_name = of_node_full_name(np);
		strscpy(entry->name, node_name, sizeof(entry->name));

		entry->dev.name = entry->name;
		entry->dev.class_id = KDAL_DEV_CLASS_UNSPEC;
		entry->dev.power_state = KDAL_POWER_OFF;
		entry->dev.priv = &entry->priv;

		ret = kdal_register_device(&entry->dev);
		if (ret) {
			pr_warn("kdal: DT node %s - register failed (%d)\n",
				entry->name, ret);
			iounmap(entry->priv.base);
			kfree(entry);
			continue;
		}

		mutex_lock(&dt_lock);
		list_add_tail(&entry->node, &dt_device_list);
		mutex_unlock(&dt_lock);

		pr_info("kdal: DT device %s @ %pR\n", entry->name, &res);
	}
#endif
	return 0;
}

void kdal_devicetree_cleanup(void)
{
#ifdef CONFIG_OF
	struct kdal_dt_entry *entry, *tmp;

	mutex_lock(&dt_lock);
	list_for_each_entry_safe(entry, tmp, &dt_device_list, node) {
		kdal_unregister_device(&entry->dev);
		iounmap(entry->priv.base);
		list_del(&entry->node);
		kfree(entry);
	}
	mutex_unlock(&dt_lock);
#endif
}
