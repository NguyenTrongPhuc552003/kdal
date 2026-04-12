/*
 * KDAL event queue and dispatch subsystem.
 *
 * Events are produced by drivers, backends, and the core itself.
 * A bounded circular log keeps the most recent KDAL_EVENT_LOG_SIZE
 * entries for debugfs / poll-based consumption.  A waitqueue wakes
 * any userspace readers blocked on the char device.
 */

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <kdal/core/kdal.h>

#define KDAL_EVENT_LOG_SIZE 256

static struct kdal_event kdal_event_log[KDAL_EVENT_LOG_SIZE];
static unsigned int kdal_event_head;
static unsigned int kdal_event_count;
static DEFINE_MUTEX(kdal_event_lock);
static DECLARE_WAIT_QUEUE_HEAD(kdal_event_wq);

int kdal_emit_event(enum kdal_event_type type, const char *device_name,
		    u32 data)
{
	struct kdal_event *slot;

	mutex_lock(&kdal_event_lock);

	slot = &kdal_event_log[kdal_event_head % KDAL_EVENT_LOG_SIZE];
	slot->type = type;
	slot->device_name = device_name;
	slot->data = data;

	kdal_event_head++;
	if (kdal_event_count < KDAL_EVENT_LOG_SIZE)
		kdal_event_count++;

	mutex_unlock(&kdal_event_lock);

	wake_up_interruptible(&kdal_event_wq);

	pr_debug("kdal: event type=%u dev=%s data=%u\n", type,
		 device_name ? device_name : "(null)", data);

	return 0;
}

/*
 * Pop the oldest unconsumed event into @out.
 * Returns 0 on success, -EAGAIN if the log is empty.
 */
int kdal_poll_event(struct kdal_event *out)
{
	unsigned int tail;

	if (!out)
		return -EINVAL;

	mutex_lock(&kdal_event_lock);

	if (kdal_event_count == 0) {
		mutex_unlock(&kdal_event_lock);
		return -EAGAIN;
	}

	tail = (kdal_event_head - kdal_event_count) % KDAL_EVENT_LOG_SIZE;
	*out = kdal_event_log[tail];
	kdal_event_count--;

	mutex_unlock(&kdal_event_lock);
	return 0;
}

void kdal_event_shutdown(void)
{
	mutex_lock(&kdal_event_lock);
	kdal_event_head = 0;
	kdal_event_count = 0;
	mutex_unlock(&kdal_event_lock);
}
