/*
 * KDAL character device interface.
 *
 * Exposes /dev/kdal to userspace.  Through this control surface the
 * kdality tool (or any other process) can enumerate devices, query per-device
 * information, change power states, and read/write data through the
 * selected device's driver.
 *
 * Design:
 *   - A single misc device (/dev/kdal) is created at init.
 *   - Each open file descriptor carries its own "selected device" pointer
 *     which is set via KDAL_IOCTL_SELECT_DEV before I/O.
 *   - ioctl dispatches global operations (version, list, event poll) and
 *     per-device operations (info, power, driver-specific ioctl).
 *   - read/write are forwarded to the selected device's driver ops.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include <kdal/core/kdal.h>
#include <kdal/core/version.h>
#include <kdal/ioctl.h>

/* ── per-fd state ───────────────────────────────────────────────── */

struct kdal_file_ctx {
	struct kdal_device *selected;
};

/* ── helpers ────────────────────────────────────────────────────── */

static int kdal_ioctl_get_version(unsigned long arg)
{
	struct kdal_ioctl_version ver = {
		.major = KDAL_VERSION_MAJOR,
		.minor = KDAL_VERSION_MINOR,
		.patch = KDAL_VERSION_PATCH,
	};

	if (copy_to_user((void __user *)arg, &ver, sizeof(ver)))
		return -EFAULT;

	return 0;
}

struct list_ctx {
	struct kdal_ioctl_list *list;
};

static int list_device_cb(struct kdal_device *dev, void *data)
{
	struct list_ctx *ctx = data;

	if (ctx->list->count >= KDAL_MAX_DEVICES)
		return 0;

	strscpy(ctx->list->names[ctx->list->count], dev->name, KDAL_NAME_MAX);
	ctx->list->count++;

	return 0;
}

static int kdal_ioctl_list_devices(unsigned long arg)
{
	struct kdal_ioctl_list *list;
	struct list_ctx ctx;
	int ret;

	list = kzalloc(sizeof(*list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	ctx.list = list;
	kdal_for_each_device(list_device_cb, &ctx);

	if (copy_to_user((void __user *)arg, list, sizeof(*list)))
		ret = -EFAULT;
	else
		ret = 0;

	kfree(list);
	return ret;
}

static int kdal_ioctl_get_info(struct kdal_file_ctx *fctx, unsigned long arg)
{
	struct kdal_ioctl_info info;
	struct kdal_device *dev;

	/* The caller may pass a name in the structure to query */
	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;

	if (info.name[0] != '\0') {
		info.name[sizeof(info.name) - 1] = '\0';
		dev = kdal_find_device(info.name);
	} else {
		dev = fctx->selected;
	}

	if (!dev)
		return -ENODEV;

	memset(&info, 0, sizeof(info));
	strscpy(info.name, dev->name, sizeof(info.name));
	info.class_id = dev->class_id;
	info.power_state = dev->power_state;
	info.nr_caps = dev->nr_caps;
	if (dev->driver)
		info.feature_flags = dev->driver->feature_flags;

	if (copy_to_user((void __user *)arg, &info, sizeof(info)))
		return -EFAULT;

	return 0;
}

static int kdal_ioctl_select_dev(struct kdal_file_ctx *fctx, unsigned long arg)
{
	struct kdal_ioctl_devname dn;
	struct kdal_device *dev;

	if (copy_from_user(&dn, (void __user *)arg, sizeof(dn)))
		return -EFAULT;

	dn.name[sizeof(dn.name) - 1] = '\0';

	dev = kdal_find_device(dn.name);
	if (!dev)
		return -ENODEV;

	fctx->selected = dev;
	return 0;
}

static int kdal_ioctl_set_power(struct kdal_file_ctx *fctx, unsigned long arg)
{
	__u32 state;

	if (!fctx->selected)
		return -ENODEV;

	if (copy_from_user(&state, (void __user *)arg, sizeof(state)))
		return -EFAULT;

	if (state > KDAL_POWER_SUSPEND)
		return -EINVAL;

	return kdal_set_device_power(fctx->selected, state);
}

/* ── file operations ────────────────────────────────────────────── */

static int kdal_cdev_open(struct inode *inode, struct file *filp)
{
	struct kdal_file_ctx *fctx;

	fctx = kzalloc(sizeof(*fctx), GFP_KERNEL);
	if (!fctx)
		return -ENOMEM;

	filp->private_data = fctx;
	return 0;
}

static int kdal_cdev_release(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

static ssize_t kdal_cdev_read(const struct file *filp, char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct kdal_file_ctx *fctx = filp->private_data;
	struct kdal_device *dev;

	if (!fctx || !fctx->selected)
		return -ENODEV;

	dev = fctx->selected;

	if (!dev->driver || !dev->driver->ops || !dev->driver->ops->read)
		return -EOPNOTSUPP;

	return dev->driver->ops->read(dev, buf, count, ppos);
}

static ssize_t kdal_cdev_write(const struct file *filp, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	struct kdal_file_ctx *fctx = filp->private_data;
	struct kdal_device *dev;

	if (!fctx || !fctx->selected)
		return -ENODEV;

	dev = fctx->selected;

	if (!dev->driver || !dev->driver->ops || !dev->driver->ops->write)
		return -EOPNOTSUPP;

	return dev->driver->ops->write(dev, buf, count, ppos);
}

static long kdal_cdev_ioctl(const struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	struct kdal_file_ctx *fctx = filp->private_data;

	switch (cmd) {
	case KDAL_IOCTL_GET_VERSION:
		return kdal_ioctl_get_version(arg);
	case KDAL_IOCTL_LIST_DEVICES:
		return kdal_ioctl_list_devices(arg);
	case KDAL_IOCTL_GET_INFO:
		return kdal_ioctl_get_info(fctx, arg);
	case KDAL_IOCTL_SELECT_DEV:
		return kdal_ioctl_select_dev(fctx, arg);
	case KDAL_IOCTL_SET_POWER:
		return kdal_ioctl_set_power(fctx, arg);
	default:
		/* Forward unknown commands to the selected device driver */
		if (fctx->selected && fctx->selected->driver &&
		    fctx->selected->driver->ops &&
		    fctx->selected->driver->ops->ioctl)
			return fctx->selected->driver->ops->ioctl(
				fctx->selected, cmd, arg);

		return -ENOTTY;
	}
}

static const struct file_operations kdal_cdev_fops = {
	.owner = THIS_MODULE,
	.open = kdal_cdev_open,
	.release = kdal_cdev_release,
	.read = kdal_cdev_read,
	.write = kdal_cdev_write,
	.unlocked_ioctl = kdal_cdev_ioctl,
};

static struct miscdevice kdal_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "kdal",
	.fops = &kdal_cdev_fops,
	.mode = 0666,
};

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_chardev_init(void)
{
	int ret;

	ret = misc_register(&kdal_miscdev);
	if (ret)
		pr_err("kdal: failed to register misc device (%d)\n", ret);
	else
		pr_info("kdal: /dev/kdal registered\n");

	return ret;
}

void kdal_chardev_exit(void)
{
	misc_deregister(&kdal_miscdev);
	pr_info("kdal: /dev/kdal unregistered\n");
}
