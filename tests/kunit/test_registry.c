/*
 * KUnit tests for the KDAL registry subsystem.
 *
 * Covers backend/driver/device registration, duplicate detection,
 * lookup by name and class, snapshot counters, and iteration.
 */

#include <kunit/test.h>

#include <kdal/api/common.h>
#include <kdal/core/kdal.h>

/* ── helpers ────────────────────────────────────────────────────── */

static const struct kdal_backend_ops dummy_backend_ops = { 0 };

static struct kdal_backend test_backend = {
	.name = "test-backend",
	.feature_flags = KDAL_FEAT_EVENT_PUSH,
	.ops = &dummy_backend_ops,
};

static const struct kdal_driver_ops dummy_driver_ops = { 0 };

static struct kdal_driver test_driver = {
	.name = "test-driver",
	.class_id = KDAL_DEV_CLASS_UART,
	.ops = &dummy_driver_ops,
};

static struct kdal_device test_device = {
	.name = "test-dev0",
	.class_id = KDAL_DEV_CLASS_UART,
	.power_state = KDAL_POWER_OFF,
};

/* ── test cases ─────────────────────────────────────────────────── */

static void test_register_backend(struct kunit *test)
{
	int ret;

	ret = kdal_register_backend(&test_backend);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Duplicate registration should fail */
	ret = kdal_register_backend(&test_backend);
	KUNIT_EXPECT_EQ(test, ret, -EEXIST);

	kdal_unregister_backend(&test_backend);
}

static void test_find_backend(struct kunit *test)
{
	struct kdal_backend *found;

	kdal_register_backend(&test_backend);

	found = kdal_find_backend("test-backend");
	KUNIT_EXPECT_PTR_EQ(test, found, &test_backend);

	found = kdal_find_backend("nonexistent");
	KUNIT_EXPECT_NULL(test, found);

	kdal_unregister_backend(&test_backend);
}

static void test_register_driver(struct kunit *test)
{
	int ret;

	ret = kdal_register_driver(&test_driver);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kdal_register_driver(&test_driver);
	KUNIT_EXPECT_EQ(test, ret, -EEXIST);

	kdal_unregister_driver(&test_driver);
}

static void test_find_driver_by_class(struct kunit *test)
{
	struct kdal_driver *found;

	kdal_register_driver(&test_driver);

	found = kdal_find_driver(KDAL_DEV_CLASS_UART);
	KUNIT_EXPECT_PTR_EQ(test, found, &test_driver);

	found = kdal_find_driver(KDAL_DEV_CLASS_GPIO);
	KUNIT_EXPECT_NULL(test, found);

	kdal_unregister_driver(&test_driver);
}

static void test_register_device(struct kunit *test)
{
	int ret;

	ret = kdal_register_device(&test_device);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = kdal_register_device(&test_device);
	KUNIT_EXPECT_EQ(test, ret, -EEXIST);

	kdal_unregister_device(&test_device);
}

static void test_find_device(struct kunit *test)
{
	struct kdal_device *found;

	kdal_register_device(&test_device);

	found = kdal_find_device("test-dev0");
	KUNIT_EXPECT_PTR_EQ(test, found, &test_device);

	found = kdal_find_device("nonexistent");
	KUNIT_EXPECT_NULL(test, found);

	kdal_unregister_device(&test_device);
}

static void test_registry_snapshot(struct kunit *test)
{
	struct kdal_registry_snapshot snap;

	kdal_get_registry_snapshot(&snap);
	/* At minimum the QEMU backend and real drivers are registered */
	KUNIT_EXPECT_GE(test, snap.backends, 1U);
}

static int iter_count_cb(struct kdal_device *dev, void *data)
{
	int *count = data;
	(*count)++;
	return 0;
}

static void test_for_each_device(struct kunit *test)
{
	int count = 0;

	kdal_for_each_device(iter_count_cb, &count);
	/* At least uart0/i2c0/spi0/gpu0 should be registered */
	KUNIT_EXPECT_GE(test, count, 1);
}

static void test_null_params(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, kdal_register_backend(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, kdal_register_driver(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, kdal_register_device(NULL), -EINVAL);
	KUNIT_EXPECT_NULL(test, kdal_find_backend(NULL));
	KUNIT_EXPECT_NULL(test, kdal_find_device(NULL));
	KUNIT_EXPECT_EQ(test, kdal_for_each_device(NULL, NULL), -EINVAL);
}

/* ── suite ──────────────────────────────────────────────────────── */

static struct kunit_case kdal_registry_cases[] = {
	KUNIT_CASE(test_register_backend),
	KUNIT_CASE(test_find_backend),
	KUNIT_CASE(test_register_driver),
	KUNIT_CASE(test_find_driver_by_class),
	KUNIT_CASE(test_register_device),
	KUNIT_CASE(test_find_device),
	KUNIT_CASE(test_registry_snapshot),
	KUNIT_CASE(test_for_each_device),
	KUNIT_CASE(test_null_params),
	{}
};

static struct kunit_suite kdal_registry_suite = {
	.name = "kdal-registry",
	.test_cases = kdal_registry_cases,
};

kunit_test_suite(kdal_registry_suite);

MODULE_LICENSE("GPL");