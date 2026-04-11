/*
 * KUnit tests for the KDAL event subsystem.
 *
 * Covers event emission, circular buffer wrap-around, poll/consume
 * semantics, and shutdown behaviour.
 */

#include <kunit/test.h>

#include <kdal/core/kdal.h>

static void test_emit_event(struct kunit *test)
{
	int ret;

	ret = kdal_emit_event(KDAL_EVENT_IO_READY, "uart0", 42);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_emit_null_device(struct kunit *test)
{
	int ret;

	/* NULL device name should still succeed (defensive) */
	ret = kdal_emit_event(KDAL_EVENT_FAULT, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void test_poll_event(struct kunit *test)
{
	struct kdal_event ev;
	int ret;

	/* Drain any existing events */
	kdal_event_shutdown();

	/* Emit one event */
	kdal_emit_event(KDAL_EVENT_DEVICE_ADDED, "test-dev", 1);

	ret = kdal_poll_event(&ev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (int)ev.type, (int)KDAL_EVENT_DEVICE_ADDED);
	KUNIT_EXPECT_EQ(test, ev.data, 1U);

	/* Buffer should now be empty */
	ret = kdal_poll_event(&ev);
	KUNIT_EXPECT_EQ(test, ret, -EAGAIN);
}

static void test_poll_null(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, kdal_poll_event(NULL), -EINVAL);
}

static void test_event_shutdown(struct kunit *test)
{
	struct kdal_event ev;

	kdal_emit_event(KDAL_EVENT_IO_READY, "uart0", 99);
	kdal_event_shutdown();

	KUNIT_EXPECT_EQ(test, kdal_poll_event(&ev), -EAGAIN);
}

static void test_event_overflow(struct kunit *test)
{
	struct kdal_event ev;
	int i, ret;

	kdal_event_shutdown();

	/* Emit more events than the ring can hold (256) */
	for (i = 0; i < 300; i++)
		kdal_emit_event(KDAL_EVENT_IO_READY, "overflow", (u32)i);

	/* Should still be able to read 256 events (oldest was overwritten) */
	for (i = 0; i < 256; i++) {
		ret = kdal_poll_event(&ev);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* Now empty */
	ret = kdal_poll_event(&ev);
	KUNIT_EXPECT_EQ(test, ret, -EAGAIN);

	kdal_event_shutdown();
}

/* ── suite ──────────────────────────────────────────────────────── */

static struct kunit_case kdal_event_cases[] = {
	KUNIT_CASE(test_emit_event),
	KUNIT_CASE(test_emit_null_device),
	KUNIT_CASE(test_poll_event),
	KUNIT_CASE(test_poll_null),
	KUNIT_CASE(test_event_shutdown),
	KUNIT_CASE(test_event_overflow),
	{}
};

static struct kunit_suite kdal_event_suite = {
	.name = "kdal-event",
	.test_cases = kdal_event_cases,
};

kunit_test_suite(kdal_event_suite);

MODULE_LICENSE("GPL");