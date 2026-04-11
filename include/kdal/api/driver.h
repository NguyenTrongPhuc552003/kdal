/*
 * KDAL conventional device driver contract.
 */

#ifndef KDAL_API_DRIVER_H
#define KDAL_API_DRIVER_H

#include <linux/fs.h>

#include <kdal/types.h>

struct kdal_driver_ops {
	int (*probe)(struct kdal_device *device);
	void (*remove)(struct kdal_device *device);
	ssize_t (*read)(struct kdal_device *device, char __user *buf,
	                size_t count, loff_t *ppos);
	ssize_t (*write)(struct kdal_device *device, const char __user *buf,
	                 size_t count, loff_t *ppos);
	long (*ioctl)(struct kdal_device *device, unsigned int cmd,
	              unsigned long arg);
	int (*set_power_state)(struct kdal_device *device,
	                       enum kdal_power_state state);
};

struct kdal_driver {
	const char *name;
	enum kdal_device_class class_id;
	u32 feature_flags;
	const struct kdal_driver_ops *ops;
	void *priv;
	struct list_head node;
};

#endif /* KDAL_API_DRIVER_H */