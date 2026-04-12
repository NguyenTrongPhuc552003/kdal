/*
 * KDAL common public constants, flags, and status helpers.
 */

#ifndef KDAL_API_COMMON_H
#define KDAL_API_COMMON_H

#include <linux/bitops.h>
#include <linux/errno.h>

#define KDAL_NAME_LEN 32

#define KDAL_FEAT_EVENT_PUSH BIT(0)
#define KDAL_FEAT_POWER_MGMT BIT(1)
#define KDAL_FEAT_SHARED_MEM BIT(2)
#define KDAL_FEAT_ACCEL_QUEUE BIT(3)

enum kdal_status {
	KDAL_STATUS_OK = 0,
	KDAL_STATUS_INVALID = -EINVAL,
	KDAL_STATUS_BUSY = -EBUSY,
	KDAL_STATUS_NOMEM = -ENOMEM,
	KDAL_STATUS_NOTSUPP = -EOPNOTSUPP,
	KDAL_STATUS_NODEV = -ENODEV,
};

#endif /* KDAL_API_COMMON_H */
