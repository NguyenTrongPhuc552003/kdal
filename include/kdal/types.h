/*
 * KDAL foundational types and shared object definitions.
 */

#ifndef KDAL_TYPES_H
#define KDAL_TYPES_H

#include <linux/list.h>
#include <linux/types.h>

enum kdal_device_class {
	KDAL_DEV_CLASS_UNSPEC = 0,
	KDAL_DEV_CLASS_UART,
	KDAL_DEV_CLASS_I2C,
	KDAL_DEV_CLASS_SPI,
	KDAL_DEV_CLASS_GPIO,
	KDAL_DEV_CLASS_GPU,
	KDAL_DEV_CLASS_NPU,
};

enum kdal_power_state {
	KDAL_POWER_OFF = 0,
	KDAL_POWER_ON,
	KDAL_POWER_SUSPEND,
};

enum kdal_event_type {
	KDAL_EVENT_NONE = 0,
	KDAL_EVENT_DEVICE_ADDED,
	KDAL_EVENT_DEVICE_REMOVED,
	KDAL_EVENT_IO_READY,
	KDAL_EVENT_POWER_CHANGE,
	KDAL_EVENT_ACCEL_COMPLETE,
	KDAL_EVENT_FAULT,
};

struct kdal_capability {
	u32 id;
	u32 value;
};

struct kdal_event {
	enum kdal_event_type type;
	const char *device_name;
	u32 data;
	struct list_head node;
};

struct kdal_backend;
struct kdal_driver;

struct kdal_device {
	const char *name;
	enum kdal_device_class class_id;
	enum kdal_power_state power_state;
	const struct kdal_capability *caps;
	u32 nr_caps;
	void *priv;
	struct kdal_backend *backend;
	struct kdal_driver *driver;
	struct list_head node;
};

struct kdal_accel_buffer {
	u64 iova;
	u32 size;
	u32 flags;
	void *cpu_addr;
};

struct kdal_accel_queue {
	u32 id;
	u32 depth;
	void *priv;
};

struct kdal_registry_snapshot {
	u32 backends;
	u32 drivers;
	u32 devices;
};

#endif /* KDAL_TYPES_H */
