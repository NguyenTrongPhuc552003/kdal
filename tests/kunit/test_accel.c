/*
 * KUnit tests for KDAL accelerator queue and buffer semantics.
 *
 * Verifies the GPU driver's accel_ops: queue create/destroy,
 * buffer map/unmap, and command submission.
 */

#include <kunit/test.h>

#include <kdal/api/accel.h>
#include <kdal/core/kdal.h>

/* Exported by gpudriver.c */
extern const struct kdal_accel_ops *kdal_gpu_accel_ops;

static void test_queue_create(struct kunit *test)
{
	struct kdal_device *dev;
	struct kdal_accel_queue queue = { 0 };
	int ret;

	dev = kdal_find_device("gpu0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ret = kdal_gpu_accel_ops->queue_create(dev, &queue);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_GT(test, queue.depth, 0U);

	kdal_gpu_accel_ops->queue_destroy(dev, &queue);
}

static void test_queue_null_params(struct kunit *test)
{
	struct kdal_accel_queue queue = { 0 };

	KUNIT_EXPECT_EQ(test, kdal_gpu_accel_ops->queue_create(NULL, &queue),
			-EINVAL);
}

static void test_buffer_map_unmap(struct kunit *test)
{
	struct kdal_device *dev;
	struct kdal_accel_buffer buf = { .size = 4096 };
	int ret;

	dev = kdal_find_device("gpu0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ret = kdal_gpu_accel_ops->buffer_map(dev, &buf);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_NE(test, (long long)buf.iova, 0LL);

	kdal_gpu_accel_ops->buffer_unmap(dev, &buf);
	KUNIT_EXPECT_EQ(test, (long long)buf.iova, 0LL);
}

static void test_submit(struct kunit *test)
{
	struct kdal_device *dev;
	struct kdal_accel_queue queue = { 0 };
	char cmd[] = "test-payload";
	int ret;

	dev = kdal_find_device("gpu0");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ret = kdal_gpu_accel_ops->queue_create(dev, &queue);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = kdal_gpu_accel_ops->submit(dev, &queue, cmd, sizeof(cmd));
	KUNIT_EXPECT_EQ(test, ret, 0);

	kdal_gpu_accel_ops->queue_destroy(dev, &queue);
}

static void test_submit_null(struct kunit *test)
{
	struct kdal_accel_queue queue = { 0 };

	KUNIT_EXPECT_EQ(test, kdal_gpu_accel_ops->submit(NULL, &queue, "x", 1),
			-EINVAL);
}

/* ── suite ──────────────────────────────────────────────────────── */

static struct kunit_case kdal_accel_cases[] = {
	KUNIT_CASE(test_queue_create),	   KUNIT_CASE(test_queue_null_params),
	KUNIT_CASE(test_buffer_map_unmap), KUNIT_CASE(test_submit),
	KUNIT_CASE(test_submit_null),	   {}
};

static struct kunit_suite kdal_accel_suite = {
	.name = "kdal-accel",
	.test_cases = kdal_accel_cases,
};

kunit_test_suite(kdal_accel_suite);

MODULE_LICENSE("GPL");
