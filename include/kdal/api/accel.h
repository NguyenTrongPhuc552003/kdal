/*
 * KDAL accelerator contract based on queues, buffers, and completion events.
 */

#ifndef KDAL_API_ACCEL_H
#define KDAL_API_ACCEL_H

#include <linux/types.h>

#include <kdal/types.h>

struct kdal_accel_ops {
	int (*queue_create)(struct kdal_device *device,
	                    struct kdal_accel_queue *queue);
	void (*queue_destroy)(struct kdal_device *device,
	                      struct kdal_accel_queue *queue);
	int (*buffer_map)(struct kdal_device *device,
	                  struct kdal_accel_buffer *buffer);
	void (*buffer_unmap)(struct kdal_device *device,
	                     struct kdal_accel_buffer *buffer);
	int (*submit)(struct kdal_device *device,
	              struct kdal_accel_queue *queue, const void *cmd,
	              size_t cmd_len);
};

#endif /* KDAL_API_ACCEL_H */