// SPDX-License-Identifier: GPL-2.0-only
/*
 * KUnit tests for the resource demand report (FF-A vocabulary JSON).
 */

#include <kunit/test.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>

static const struct kdal_demand gpio_demand = {
	.base = 0x09030000, .size = 0x1000, .attributes = 0x3,
	.nr_irqs = 1, .irqs = { 39 },
};

static const struct kdal_demand dma_demand = {
	.base = 0x0a003e00, .size = 0x200, .attributes = 0x3,
	.nr_irqs = 1, .irqs = { 79 },
	.smmu_id = 0, .nr_streams = 2, .streams = { 8, 9 },
};

static struct kdal_device dev_gpio = {
	.name = "demand-gpio", .class_id = KDAL_DEV_CLASS_GPIO, .demand = &gpio_demand,
};
static struct kdal_device dev_dma = {
	.name = "demand-dma", .class_id = KDAL_DEV_CLASS_UNSPEC, .demand = &dma_demand,
};
static struct kdal_device dev_none = {
	.name = "demand-none", .class_id = KDAL_DEV_CLASS_UNSPEC,
};

static char *format(struct kunit *test)
{
	char *buf = kunit_kzalloc(test, 4096, GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, buf);
	kdal_demand_format(buf, 4096);
	return buf;
}

static void test_demand_regions(struct kunit *test)
{
	char *out;

	KUNIT_ASSERT_EQ(test, kdal_register_device(&dev_gpio), 0);
	KUNIT_ASSERT_EQ(test, kdal_register_device(&dev_dma), 0);
	KUNIT_ASSERT_EQ(test, kdal_register_device(&dev_none), 0);
	out = format(test);

	KUNIT_EXPECT_NOT_NULL(test, strstr(out, "\"format\": \"kdal-demand/1\""));
	KUNIT_EXPECT_NOT_NULL(test, strstr(out,
		"{\"description\": \"demand-gpio\", \"base-address\": \"0x9030000\", "
		"\"pages-count\": 1, \"attributes\": \"0x3\", \"interrupts\": [{\"id\": 39}]}"));
	/* 0xa003e00 + 0x200 lies in one 4 KiB page, reported from its start */
	KUNIT_EXPECT_NOT_NULL(test, strstr(out,
		"\"base-address\": \"0xa003000\", \"pages-count\": 1"));
	KUNIT_EXPECT_NOT_NULL(test, strstr(out, "\"smmu-id\": 0, \"stream-ids\": [8, 9]"));
	KUNIT_EXPECT_NULL(test, strstr(out, "demand-none"));

	kdal_unregister_device(&dev_none);
	kdal_unregister_device(&dev_dma);
	kdal_unregister_device(&dev_gpio);
}

static void test_demand_small_buffer(struct kunit *test)
{
	char buf[16];
	size_t n;

	n = kdal_demand_format(buf, sizeof(buf));
	KUNIT_EXPECT_LT(test, n, sizeof(buf));
	KUNIT_EXPECT_EQ(test, buf[n], '\0');
	KUNIT_EXPECT_EQ(test, kdal_demand_format(NULL, 10), (size_t)0);
}

static struct kunit_case kdal_demand_cases[] = {
	KUNIT_CASE(test_demand_regions),
	KUNIT_CASE(test_demand_small_buffer),
	{}
};

static struct kunit_suite kdal_demand_suite = {
	.name = "kdal-demand",
	.test_cases = kdal_demand_cases,
};

kunit_test_suite(kdal_demand_suite);

MODULE_LICENSE("GPL");
