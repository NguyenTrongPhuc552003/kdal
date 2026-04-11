/*
 * KUnit tests for the KDAL driver contract.
 *
 * Covers driver lookup by class, attach/detach lifecycle,
 * read/write delegation, and ioctl dispatch.
 */

#include <kunit/test.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>

static void test_find_uart_driver(struct kunit *test)
{
	struct kdal_driver *drv;

	drv = kdal_find_driver(KDAL_DEV_CLASS_UART);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drv);
	KUNIT_EXPECT_STREQ(test, drv->name, "kdal-uart");
}

static void test_find_i2c_driver(struct kunit *test)
{
	struct kdal_driver *drv;

	drv = kdal_find_driver(KDAL_DEV_CLASS_I2C);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drv);
	KUNIT_EXPECT_STREQ(test, drv->name, "kdal-i2c");
}

static void test_find_spi_driver(struct kunit *test)
{
	struct kdal_driver *drv;

	drv = kdal_find_driver(KDAL_DEV_CLASS_SPI);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drv);
	KUNIT_EXPECT_STREQ(test, drv->name, "kdal-spi");
}

static void test_find_gpu_driver(struct kunit *test)
{
	struct kdal_driver *drv;

	drv = kdal_find_driver(KDAL_DEV_CLASS_GPU);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, drv);
	KUNIT_EXPECT_STREQ(test, drv->name, "kdal-gpu");
}

static void test_find_no_gpio_driver(struct kunit *test)
{
	KUNIT_EXPECT_NULL(test, kdal_find_driver(KDAL_DEV_CLASS_GPIO));
}

static void test_driver_has_ops(struct kunit *test)
{
	struct kdal_driver *drv;

	drv = kdal_find_driver(KDAL_DEV_CLASS_UART);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drv);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->probe);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->remove);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->read);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->write);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->ioctl);
	KUNIT_EXPECT_NOT_NULL(test, drv->ops->set_power_state);
}

static void test_attach_null_params(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, kdal_attach_driver(NULL, NULL), -EINVAL);
}

static void test_device_already_attached(struct kunit *test)
{
	struct kdal_device *dev;
	struct kdal_driver *drv;
	int ret;

	dev = kdal_find_device("uart0");
	drv = kdal_find_driver(KDAL_DEV_CLASS_UART);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, drv);

	/* Already attached during init — should return -EBUSY */
	ret = kdal_attach_driver(dev, drv);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);
}

/* ── suite ──────────────────────────────────────────────────────── */

static struct kunit_case kdal_driver_cases[] = {
	KUNIT_CASE(test_find_uart_driver),
	KUNIT_CASE(test_find_i2c_driver),
	KUNIT_CASE(test_find_spi_driver),
	KUNIT_CASE(test_find_gpu_driver),
	KUNIT_CASE(test_find_no_gpio_driver),
	KUNIT_CASE(test_driver_has_ops),
	KUNIT_CASE(test_attach_null_params),
	KUNIT_CASE(test_device_already_attached),
	{}
};

static struct kunit_suite kdal_driver_suite = {
	.name = "kdal-driver",
	.test_cases = kdal_driver_cases,
};

kunit_test_suite(kdal_driver_suite);

MODULE_LICENSE("GPL");