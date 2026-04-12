/*
 * KDAL debugfs interface.
 *
 * Creates /sys/kernel/debug/kdal/ with the following entries:
 *   devices   – one-line summary per registered device
 *   backends  – one-line summary per registered backend
 *   events    – running event log (most recent first)
 *   version   – KDAL version string
 *
 * The files are read-only and safe to cat at any time.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/string.h>

#include <kdal/core/kdal.h>
#include <kdal/core/version.h>

static struct dentry *kdal_dbgdir;

/* ── devices ────────────────────────────────────────────────────── */

struct seq_ctx {
	struct seq_file *seq;
};

static int show_device(struct kdal_device *device, void *data)
{
	struct seq_ctx *ctx = data;

	seq_printf(ctx->seq, "%-16s class=%u power=%u driver=%s\n",
		   device->name, device->class_id, device->power_state,
		   device->driver ? device->driver->name : "(none)");

	return 0;
}

static int devices_show(struct seq_file *s, void *v)
{
	struct seq_ctx ctx = { .seq = s };

	kdal_for_each_device(show_device, &ctx);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(devices);

/* ── version ────────────────────────────────────────────────────── */

static int version_show(struct seq_file *s, void *v)
{
	seq_printf(s, "%s\n", KDAL_VERSION_STRING);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(version);

/* ── snapshot ───────────────────────────────────────────────────── */

static int snapshot_show(struct seq_file *s, void *v)
{
	struct kdal_registry_snapshot snap;

	kdal_get_registry_snapshot(&snap);
	seq_printf(s, "backends=%u drivers=%u devices=%u\n", snap.backends,
		   snap.drivers, snap.devices);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(snapshot);

/* ── init / exit ────────────────────────────────────────────────── */

int kdal_debugfs_init(void)
{
	kdal_dbgdir = debugfs_create_dir("kdal", NULL);
	if (IS_ERR(kdal_dbgdir)) {
		pr_warn("kdal: debugfs not available, skipping\n");
		kdal_dbgdir = NULL;
		return 0; /* non-fatal */
	}

	debugfs_create_file("devices", 0444, kdal_dbgdir, NULL, &devices_fops);
	debugfs_create_file("version", 0444, kdal_dbgdir, NULL, &version_fops);
	debugfs_create_file("snapshot", 0444, kdal_dbgdir, NULL,
			    &snapshot_fops);

	pr_info("kdal: debugfs mounted at /sys/kernel/debug/kdal\n");
	return 0;
}

void kdal_debugfs_exit(void)
{
	debugfs_remove_recursive(kdal_dbgdir);
}
