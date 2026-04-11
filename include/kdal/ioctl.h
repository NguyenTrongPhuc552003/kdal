/*
 * KDAL userspace control ABI.
 *
 * All structures in this header use fixed-width types so the ABI
 * remains stable between 32-bit and 64-bit user / kernel pairs.
 */

#ifndef KDAL_IOCTL_H
#define KDAL_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define KDAL_NAME_MAX 32
#define KDAL_MAX_DEVICES 64

/* ── ioctl payloads ─────────────────────────────────────────────── */

struct kdal_ioctl_info {
	char name[KDAL_NAME_MAX];
	__u32 class_id;
	__u32 feature_flags;
	__u32 power_state;
	__u32 nr_caps;
};

struct kdal_ioctl_version {
	__u32 major;
	__u32 minor;
	__u32 patch;
};

struct kdal_ioctl_list {
	__u32 count;
	char names[KDAL_MAX_DEVICES][KDAL_NAME_MAX];
};

struct kdal_ioctl_event {
	__u32 type;
	char device_name[KDAL_NAME_MAX];
	__u32 data;
};

struct kdal_ioctl_devname {
	char name[KDAL_NAME_MAX];
};

/* ── command numbers ────────────────────────────────────────────── */

#define KDAL_IOCTL_MAGIC 'K'

#define KDAL_IOCTL_GET_VERSION                                                 \
	_IOR(KDAL_IOCTL_MAGIC, 0x00, struct kdal_ioctl_version)
#define KDAL_IOCTL_GET_INFO                                                    \
	_IOWR(KDAL_IOCTL_MAGIC, 0x01, struct kdal_ioctl_info)
#define KDAL_IOCTL_SET_POWER _IOW(KDAL_IOCTL_MAGIC, 0x02, __u32)
#define KDAL_IOCTL_LIST_DEVICES                                                \
	_IOR(KDAL_IOCTL_MAGIC, 0x03, struct kdal_ioctl_list)
#define KDAL_IOCTL_POLL_EVENT                                                  \
	_IOR(KDAL_IOCTL_MAGIC, 0x04, struct kdal_ioctl_event)
#define KDAL_IOCTL_SELECT_DEV                                                  \
	_IOW(KDAL_IOCTL_MAGIC, 0x05, struct kdal_ioctl_devname)

#endif /* KDAL_IOCTL_H */