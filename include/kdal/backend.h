/*
 * KDAL backend interface used by the core to abstract transports and platforms.
 */

#ifndef KDAL_BACKEND_H
#define KDAL_BACKEND_H

#include <linux/types.h>

#include <kdal/types.h>

struct kdal_backend_ops {
	int (*init)(struct kdal_backend *backend);
	void (*exit)(struct kdal_backend *backend);
	int (*enumerate)(struct kdal_backend *backend);
	ssize_t (*read)(struct kdal_device *device, char *buf, size_t count);
	ssize_t (*write)(struct kdal_device *device, const char *buf,
	                 size_t count);
	long (*ioctl)(struct kdal_device *device, unsigned int cmd,
	              unsigned long arg);
};

struct kdal_backend {
	const char *name;
	u32 feature_flags;
	const struct kdal_backend_ops *ops;
	void *priv;
	struct list_head node;
};

#endif /* KDAL_BACKEND_H */