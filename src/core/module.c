/*
 * KDAL module bootstrap for the first out-of-tree milestone.
 */

#include <linux/init.h>
#include <linux/module.h>

#include <kdal/core/kdal.h>
#include <kdal/core/version.h>

static int __init kdal_module_init(void)
{
	pr_info("kdal: initializing version %s\n", KDAL_VERSION_STRING);
	return kdal_core_init();
}

static void __exit kdal_module_exit(void)
{
	kdal_core_exit();
	pr_info("kdal: exited\n");
}

module_init(kdal_module_init);
module_exit(kdal_module_exit);

MODULE_AUTHOR("Trong Phuc Nguyen");
MODULE_DESCRIPTION("Kernel Device Abstraction Layer");
MODULE_LICENSE("GPL");
MODULE_VERSION(KDAL_VERSION_STRING);