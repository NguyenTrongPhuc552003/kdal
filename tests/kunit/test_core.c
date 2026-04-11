/*
 * KUnit tests for KDAL core lifecycle and power management.
 *
 * Covers registry snapshot consistency, device lookup after init,
 * power state transitions, and driver attachment semantics.
 */

#include <kunit/test.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>

static void test_snapshot_after_init(struct kunit *test)
{
	struct kdal_registry_snapshot snap;

	kdal_get_registry_snapshot(&snap);
	KUNIT_EXPECT_GE(test, snap.backends, 1U);
	KUNIT_EXPECT_GE(test, snap.drivers, 1U);
	KUNIT_EXPECT_GE(test, snap.devices, 1U);
}

static void test_find_uart_device(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("uart0");
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_EQ(test, (int)dev->class_id, (int)KDAL_DEV_CLASS_UART);
}

static void test_find_i2c_device(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("i2c0");
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_EQ(test, (int)dev->class_id, (int)KDAL_DEV_CLASS_I2C);
}

static void test_find_spi_device(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("spi0");
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_EQ(test, (int)dev->class_id, (int)KDAL_DEV_CLASS_SPI);
}

static void test_find_gpu_device(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("gpu0");
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_EQ(test, (int)dev->class_id, (int)KDAL_DEV_CLASS_GPU);
}

static void test_device_has_driver(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("uart0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_NOT_NULL(test, dev->driver);
	KUNIT_EXPECT_STREQ(test, dev->driver->name, "kdal-uart");
}

static void test_device_has_backend(struct kunit *test)
{
	struct kdal_device *dev;

	dev = kdal_find_device("uart0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_EXPECT_NOT_NULL(test, dev->backend);
	KUNIT_EXPECT_STREQ(test, dev->backend->name, "qemu-virt");
}

static void test_power_transition(struct kunit *test)
{
	struct kdal_device *dev;
	int ret;

	dev = kdal_find_device("uart0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ret = kdal_set_device_power(dev, KDAL_POWER_SUSPEND);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (int)dev->power_state, (int)KDAL_POWER_SUSPEND);

	ret = kdal_set_device_power(dev, KDAL_POWER_ON);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, (int)dev->power_state, (int)KDAL_POWER_ON);
}

static void test_power_null_device(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, kdal_set_device_power(NULL, KDAL_POWER_ON),
			-EINVAL);
}

static void test_find_nonexistent_device(struct kunit *test)
{
	KUNIT_EXPECT_NULL(test, kdal_find_device("does-not-exist"));
}

/* ── suite ──────────────────────────────────────────────────────── */

static struct kunit_case kdal_core_cases[] = {
	KUNIT_CASE(test_snapshot_after_init),
	KUNIT_CASE(test_find_uart_device),
	KUNIT_CASE(test_find_i2c_device),
	KUNIT_CASE(test_find_spi_device),
	KUNIT_CASE(test_find_gpu_device),
	KUNIT_CASE(test_device_has_driver),
	KUNIT_CASE(test_device_has_backend),
	KUNIT_CASE(test_power_transition),
	KUNIT_CASE(test_power_null_device),
	KUNIT_CASE(test_find_nonexistent_device),
	{}
};

static struct kunit_suite kdal_core_suite = {
	.name = "kdal-core",
	.test_cases = kdal_core_cases,
};

kunit_test_suite(kdal_core_suite);

MODULE_LICENSE("GPL");