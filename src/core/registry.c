/*
 * KDAL registry for backends, drivers, and devices.
 */

#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/string.h>

#include <kdal/core/kdal.h>

static LIST_HEAD(kdal_backend_list);
static LIST_HEAD(kdal_driver_list);
static LIST_HEAD(kdal_device_list);
static DEFINE_MUTEX(kdal_registry_lock);

static bool kdal_name_match(const char *lhs, const char *rhs)
{
	if (!lhs || !rhs)
		return false;

	return strcmp(lhs, rhs) == 0;
}

int kdal_register_backend(struct kdal_backend *backend)
{
	struct kdal_backend *iter;

	if (!backend || !backend->name || !backend->ops)
		return -EINVAL;

	INIT_LIST_HEAD(&backend->node);

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(iter, &kdal_backend_list, node) {
		if (kdal_name_match(iter->name, backend->name)) {
			mutex_unlock(&kdal_registry_lock);
			return -EEXIST;
		}
	}
	list_add_tail(&backend->node, &kdal_backend_list);
	mutex_unlock(&kdal_registry_lock);

	return 0;
}

void kdal_unregister_backend(struct kdal_backend *backend)
{
	if (!backend)
		return;

	mutex_lock(&kdal_registry_lock);
	if (!list_empty(&backend->node))
		list_del_init(&backend->node);
	mutex_unlock(&kdal_registry_lock);
}

int kdal_register_driver(struct kdal_driver *driver)
{
	struct kdal_driver *iter;

	if (!driver || !driver->name || !driver->ops)
		return -EINVAL;

	INIT_LIST_HEAD(&driver->node);

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(iter, &kdal_driver_list, node) {
		if (kdal_name_match(iter->name, driver->name)) {
			mutex_unlock(&kdal_registry_lock);
			return -EEXIST;
		}
	}
	list_add_tail(&driver->node, &kdal_driver_list);
	mutex_unlock(&kdal_registry_lock);

	return 0;
}

void kdal_unregister_driver(struct kdal_driver *driver)
{
	if (!driver)
		return;

	mutex_lock(&kdal_registry_lock);
	if (!list_empty(&driver->node))
		list_del_init(&driver->node);
	mutex_unlock(&kdal_registry_lock);
}

int kdal_register_device(struct kdal_device *device)
{
	struct kdal_device *iter;

	if (!device || !device->name)
		return -EINVAL;

	INIT_LIST_HEAD(&device->node);

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(iter, &kdal_device_list, node) {
		if (kdal_name_match(iter->name, device->name)) {
			mutex_unlock(&kdal_registry_lock);
			return -EEXIST;
		}
	}
	list_add_tail(&device->node, &kdal_device_list);
	mutex_unlock(&kdal_registry_lock);

	return 0;
}

void kdal_unregister_device(struct kdal_device *device)
{
	if (!device)
		return;

	mutex_lock(&kdal_registry_lock);
	if (!list_empty(&device->node))
		list_del_init(&device->node);
	mutex_unlock(&kdal_registry_lock);
}

struct kdal_backend *kdal_find_backend(const char *name)
{
	struct kdal_backend *backend;

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(backend, &kdal_backend_list, node) {
		if (kdal_name_match(backend->name, name)) {
			mutex_unlock(&kdal_registry_lock);
			return backend;
		}
	}
	mutex_unlock(&kdal_registry_lock);

	return NULL;
}

struct kdal_driver *kdal_find_driver(enum kdal_device_class class_id)
{
	struct kdal_driver *driver;

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(driver, &kdal_driver_list, node) {
		if (driver->class_id == class_id) {
			mutex_unlock(&kdal_registry_lock);
			return driver;
		}
	}
	mutex_unlock(&kdal_registry_lock);

	return NULL;
}

struct kdal_device *kdal_find_device(const char *name)
{
	struct kdal_device *device;

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(device, &kdal_device_list, node) {
		if (kdal_name_match(device->name, name)) {
			mutex_unlock(&kdal_registry_lock);
			return device;
		}
	}
	mutex_unlock(&kdal_registry_lock);

	return NULL;
}

void kdal_get_registry_snapshot(struct kdal_registry_snapshot *snapshot)
{
	struct kdal_backend *backend;
	struct kdal_driver *driver;
	struct kdal_device *device;

	if (!snapshot)
		return;

	memset(snapshot, 0, sizeof(*snapshot));

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(backend, &kdal_backend_list, node)
		snapshot->backends++;
	list_for_each_entry(driver, &kdal_driver_list, node)
		snapshot->drivers++;
	list_for_each_entry(device, &kdal_device_list, node)
		snapshot->devices++;
	mutex_unlock(&kdal_registry_lock);
}

int kdal_for_each_device(kdal_device_iter_fn fn, void *data)
{
	struct kdal_device *device;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	mutex_lock(&kdal_registry_lock);
	list_for_each_entry(device, &kdal_device_list, node) {
		ret = fn(device, data);
		if (ret)
			break;
	}
	mutex_unlock(&kdal_registry_lock);

	return ret;
}