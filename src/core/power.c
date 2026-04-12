/*
 * KDAL device power-state helpers.
 */

#include <linux/errno.h>

#include <kdal/core/kdal.h>

int kdal_set_device_power(struct kdal_device *device,
			  enum kdal_power_state state)
{
	if (!device)
		return -EINVAL;

	if (device->driver && device->driver->ops &&
	    device->driver->ops->set_power_state) {
		int ret;

		ret = device->driver->ops->set_power_state(device, state);
		if (ret)
			return ret;
	}

	device->power_state = state;

	return kdal_emit_event(KDAL_EVENT_POWER_CHANGE, device->name, state);
}
