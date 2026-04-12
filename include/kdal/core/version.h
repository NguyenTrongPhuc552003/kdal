/*
 * KDAL versioning information for source and ABI coordination.
 */

#ifndef KDAL_CORE_VERSION_H
#define KDAL_CORE_VERSION_H

#define KDAL_VERSION_MAJOR 0
#define KDAL_VERSION_MINOR 1
#define KDAL_VERSION_PATCH 0

#define KDAL_VERSION_CODE                                         \
	((KDAL_VERSION_MAJOR << 16) | (KDAL_VERSION_MINOR << 8) | \
	 KDAL_VERSION_PATCH)

#define KDAL_VERSION_STRING "0.1.0"

#endif /* KDAL_CORE_VERSION_H */
