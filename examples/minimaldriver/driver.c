// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal third-party KDAL driver example.
 *
 * This demonstrates how to implement a KDAL driver outside the main
 * source tree. Build as an out-of-tree kernel module against the
 * KDAL headers.
 *
 * Usage:
 *   make KDIR=/path/to/linux KDAL_DIR=/path/to/kdal
 *   insmod kdal.ko          # load KDAL core first
 *   insmod mydriver.ko      # load this driver
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>

/*
 * In a real build, these would be:
 *   #include <kdal/core/kdal.h>
 *   #include <kdal/api/driver.h>
 *   #include <kdal/types.h>
 * For this standalone example, we provide minimal forward declarations.
 */

#define MY_DRIVER_NAME  "mydevice"
#define MY_BUF_SIZE     256

struct my_priv {
	char buf[MY_BUF_SIZE];
	size_t len;
};

/*
 * probe - called when KDAL attaches this driver to a device.
 * Allocate per-device private data here.
 */
static int my_probe(void *device)
{
	struct my_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	pr_info("mydriver: probed device\n");
	/* In real code: ((struct kdal_device *)device)->priv = priv; */
	return 0;
}

/*
 * remove - called when KDAL detaches this driver.
 * Free per-device private data here.
 */
static void my_remove(void *device)
{
	pr_info("mydriver: removed device\n");
	/* In real code: kfree(((struct kdal_device *)device)->priv); */
}

/*
 * read - copy data from device buffer to userspace.
 */
static ssize_t my_read(void *device, char __user *ubuf, size_t count)
{
	/* Simplified: in real code, access priv via device->priv */
	pr_debug("mydriver: read %zu bytes\n", count);
	return 0;
}

/*
 * write - copy data from userspace to device buffer.
 */
static ssize_t my_write(void *device, const char __user *ubuf, size_t count)
{
	pr_debug("mydriver: write %zu bytes\n", count);
	return (ssize_t)count;
}

/*
 * Driver ops structure - this is the KDAL contract.
 * Fill in the function pointers that your device supports.
 */
/*
static const struct kdal_driver_ops my_ops = {
	.probe           = my_probe,
	.remove          = my_remove,
	.read            = my_read,
	.write           = my_write,
	.ioctl           = NULL,
	.set_power_state = NULL,
};

static struct kdal_driver my_driver = {
	.name  = MY_DRIVER_NAME,
	.class = KDAL_DEV_UART,
	.ops   = &my_ops,
};
*/

static int __init mydriver_init(void)
{
	pr_info("mydriver: init\n");
	/*
	 * In real code:
	 *   return kdal_register_driver(&my_driver);
	 */
	return 0;
}

static void __exit mydriver_exit(void)
{
	pr_info("mydriver: exit\n");
	/*
	 * In real code:
	 *   kdal_unregister_driver(&my_driver);
	 */
}

module_init(mydriver_init);
module_exit(mydriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KDAL Contributors");
MODULE_DESCRIPTION("Minimal KDAL third-party driver example");