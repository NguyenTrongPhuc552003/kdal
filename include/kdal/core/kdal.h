/*
 * KDAL top-level lifecycle, registration, and lookup entrypoints.
 *
 * This header defines every public function the KDAL core exposes to
 * backend adapters, device drivers, and the char-device control path.
 */

#ifndef KDAL_CORE_KDAL_H
#define KDAL_CORE_KDAL_H

#include <kdal/api/driver.h>
#include <kdal/backend.h>
#include <kdal/types.h>

/* ── lifecycle ──────────────────────────────────────────────────── */

int kdal_core_init(void);
void kdal_core_exit(void);

/* ── char device ────────────────────────────────────────────────── */

int kdal_chardev_init(void);
void kdal_chardev_exit(void);

/* ── debugfs ────────────────────────────────────────────────────── */

int kdal_debugfs_init(void);
void kdal_debugfs_exit(void);

/* ── registry ───────────────────────────────────────────────────── */

int kdal_register_backend(struct kdal_backend *backend);
void kdal_unregister_backend(struct kdal_backend *backend);

int kdal_register_driver(struct kdal_driver *driver);
void kdal_unregister_driver(struct kdal_driver *driver);

int kdal_register_device(struct kdal_device *device);
void kdal_unregister_device(struct kdal_device *device);

int kdal_attach_driver(struct kdal_device *device, struct kdal_driver *driver);
void kdal_detach_driver(struct kdal_device *device);

struct kdal_backend *kdal_find_backend(const char *name);
struct kdal_driver *kdal_find_driver(enum kdal_device_class class_id);
struct kdal_device *kdal_find_device(const char *name);

void kdal_get_registry_snapshot(struct kdal_registry_snapshot *snapshot);

/* ── iteration (used by chardev / debugfs) ──────────────────────── */

typedef int (*kdal_device_iter_fn)(struct kdal_device *device, void *data);
int kdal_for_each_device(kdal_device_iter_fn fn, void *data);

/* ── events ─────────────────────────────────────────────────────── */

int kdal_emit_event(enum kdal_event_type type, const char *device_name,
                    u32 data);
void kdal_event_shutdown(void);

/* ── power ──────────────────────────────────────────────────────── */

int kdal_set_device_power(struct kdal_device *device,
                          enum kdal_power_state state);

#endif /* KDAL_CORE_KDAL_H */