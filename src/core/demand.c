// SPDX-License-Identifier: GPL-2.0-only
/*
 * Resource demand report.
 *
 * kdal_demand_format() writes the demand of every registered device that
 * declares one, as JSON in the vocabulary of Arm's FF-A partition manifest
 * binding (device-regions with base-address, pages-count, attributes,
 * interrupts, smmu-id, stream-ids). debugfs exposes it as
 * /sys/kernel/debug/kdal/demand; a host tool compares it with the
 * hypervisor's grant written in the same vocabulary.
 */

#include <linux/kernel.h>
#include <linux/sizes.h>

#include <kdal/core/kdal.h>

struct demand_ctx {
	char *buf;
	size_t len;
	size_t pos;
	unsigned int count;
};

static void put(struct demand_ctx *c, const char *fmt, ...)
{
	va_list ap;

	if (c->pos >= c->len)
		return;
	va_start(ap, fmt);
	c->pos += vscnprintf(c->buf + c->pos, c->len - c->pos, fmt, ap);
	va_end(ap);
}

static int demand_one(struct kdal_device *dev, void *data)
{
	struct demand_ctx *c = data;
	const struct kdal_demand *d = dev->demand;
	u32 i;

	if (!d)
		return 0;

	put(c, "%s\n  {\"description\": \"%s\"", c->count ? "," : "", dev->name);
	if (d->size) {
		u64 base = round_down(d->base, SZ_4K);
		u64 end = round_up(d->base + d->size, SZ_4K);

		put(c, ", \"base-address\": \"0x%llx\", \"pages-count\": %llu",
		    base, (end - base) / SZ_4K);
	} else {
		put(c, ", \"pages-count\": 0");
	}
	put(c, ", \"attributes\": \"0x%x\"", d->attributes);
	if (d->nr_irqs) {
		put(c, ", \"interrupts\": [");
		for (i = 0; i < d->nr_irqs && i < KDAL_DEMAND_MAX_IRQS; i++)
			put(c, "%s{\"id\": %u}", i ? ", " : "", d->irqs[i]);
		put(c, "]");
	}
	if (d->nr_streams) {
		put(c, ", \"smmu-id\": %u, \"stream-ids\": [", d->smmu_id);
		for (i = 0; i < d->nr_streams && i < KDAL_DEMAND_MAX_STREAMS; i++)
			put(c, "%s%u", i ? ", " : "", d->streams[i]);
		put(c, "]");
	}
	put(c, "}");
	c->count++;
	return 0;
}

size_t kdal_demand_format(char *buf, size_t len)
{
	struct demand_ctx c = { .buf = buf, .len = len };

	if (!buf || !len)
		return 0;
	buf[0] = '\0';
	put(&c, "{\"format\": \"kdal-demand/1\", "
		"\"vocabulary\": \"FF-A partition manifest binding\", "
		"\"device-regions\": [");
	kdal_for_each_device(demand_one, &c);
	put(&c, "%s]}\n", c.count ? "\n" : "");
	return c.pos;
}
