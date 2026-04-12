/*
 * Userspace control-plane tool for KDAL.
 *
 * Opens /dev/kdal and performs ioctl operations to manage devices:
 *   version    - query kernel module version
 *   list       - list all registered devices
 *   info NAME  - show detailed info for a device
 *   power NAME STATE - set power state (on/off/suspend)
 *   select NAME      - select a device for I/O
 *   read NAME COUNT  - read data from a device
 *   write NAME DATA  - write data to a device
 *
 * Compiles as a standalone C99 userspace program, no kernel headers
 * required (the ioctl definitions are duplicated below so the tool
 * can be compiled on any host without deploying kernel headers).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ── ABI definitions (mirror include/kdal/ioctl.h) ──────────────── */

#define KDAL_NAME_MAX 32
#define KDAL_MAX_DEVICES 64

struct kdal_ioctl_version {
	unsigned int major;
	unsigned int minor;
	unsigned int patch;
};

struct kdal_ioctl_info {
	char name[KDAL_NAME_MAX];
	unsigned int class_id;
	unsigned int feature_flags;
	unsigned int power_state;
	unsigned int nr_caps;
};

struct kdal_ioctl_list {
	unsigned int count;
	char names[KDAL_MAX_DEVICES][KDAL_NAME_MAX];
};

struct kdal_ioctl_devname {
	char name[KDAL_NAME_MAX];
};

#define KDAL_IOCTL_MAGIC 'K'

#define KDAL_IOCTL_GET_VERSION \
	_IOR(KDAL_IOCTL_MAGIC, 0x00, struct kdal_ioctl_version)
#define KDAL_IOCTL_GET_INFO \
	_IOWR(KDAL_IOCTL_MAGIC, 0x01, struct kdal_ioctl_info)
#define KDAL_IOCTL_SET_POWER _IOW(KDAL_IOCTL_MAGIC, 0x02, unsigned int)
#define KDAL_IOCTL_LIST_DEVICES \
	_IOR(KDAL_IOCTL_MAGIC, 0x03, struct kdal_ioctl_list)
#define KDAL_IOCTL_SELECT_DEV \
	_IOW(KDAL_IOCTL_MAGIC, 0x05, struct kdal_ioctl_devname)

/* ── device path ────────────────────────────────────────────────── */

#define KDAL_DEV_PATH "/dev/kdal"

static const char *class_name(unsigned int id)
{
	static const char *names[] = { "unspec", "uart", "i2c", "spi",
				       "gpio",	 "gpu",	 "npu" };

	if (id < sizeof(names) / sizeof(names[0]))
		return names[id];
	return "unknown";
}

static const char *power_name(unsigned int state)
{
	switch (state) {
	case 0:
		return "off";
	case 1:
		return "on";
	case 2:
		return "suspend";
	default:
		return "unknown";
	}
}

/* ── commands ───────────────────────────────────────────────────── */

static int cmd_version(int fd)
{
	struct kdal_ioctl_version ver;

	memset(&ver, 0, sizeof(ver));
	if (ioctl(fd, KDAL_IOCTL_GET_VERSION, &ver) < 0) {
		perror("ioctl GET_VERSION");
		return 1;
	}

	printf("KDAL version %u.%u.%u\n", ver.major, ver.minor, ver.patch);
	return 0;
}

static int cmd_list(int fd)
{
	struct kdal_ioctl_list *list;
	unsigned int i;

	list = calloc(1, sizeof(*list));
	if (!list) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	if (ioctl(fd, KDAL_IOCTL_LIST_DEVICES, list) < 0) {
		perror("ioctl LIST_DEVICES");
		free(list);
		return 1;
	}

	printf("Registered devices (%u):\n", list->count);
	for (i = 0; i < list->count; i++)
		printf("  %s\n", list->names[i]);

	free(list);
	return 0;
}

static int cmd_info(int fd, const char *name)
{
	struct kdal_ioctl_info info;

	memset(&info, 0, sizeof(info));
	strncpy(info.name, name, KDAL_NAME_MAX - 1);

	if (ioctl(fd, KDAL_IOCTL_GET_INFO, &info) < 0) {
		perror("ioctl GET_INFO");
		return 1;
	}

	printf("Device: %s\n", info.name);
	printf("  class:    %s (%u)\n", class_name(info.class_id),
	       info.class_id);
	printf("  power:    %s (%u)\n", power_name(info.power_state),
	       info.power_state);
	printf("  features: 0x%08x\n", info.feature_flags);
	printf("  caps:     %u\n", info.nr_caps);
	return 0;
}

static int cmd_power(int fd, const char *name, const char *state_str)
{
	struct kdal_ioctl_devname dn;
	unsigned int state;

	if (strcmp(state_str, "on") == 0)
		state = 1;
	else if (strcmp(state_str, "off") == 0)
		state = 0;
	else if (strcmp(state_str, "suspend") == 0)
		state = 2;
	else {
		fprintf(stderr,
			"invalid power state: %s (use on/off/suspend)\n",
			state_str);
		return 1;
	}

	/* Select the device first */
	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, name, KDAL_NAME_MAX - 1);
	if (ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) < 0) {
		perror("ioctl SELECT_DEV");
		return 1;
	}

	if (ioctl(fd, KDAL_IOCTL_SET_POWER, &state) < 0) {
		perror("ioctl SET_POWER");
		return 1;
	}

	printf("%s: power → %s\n", name, state_str);
	return 0;
}

static int cmd_read(int fd, const char *name, const char *count_str)
{
	struct kdal_ioctl_devname dn;
	char *buf;
	ssize_t n;
	long count;

	count = strtol(count_str, NULL, 10);
	if (count <= 0 || count > 4096) {
		fprintf(stderr, "count must be 1–4096\n");
		return 1;
	}

	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, name, KDAL_NAME_MAX - 1);
	if (ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) < 0) {
		perror("ioctl SELECT_DEV");
		return 1;
	}

	buf = malloc((size_t)count);
	if (!buf) {
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	n = read(fd, buf, (size_t)count);
	if (n < 0) {
		perror("read");
		free(buf);
		return 1;
	}

	printf("Read %zd bytes from %s:\n", n, name);
	fwrite(buf, 1, (size_t)n, stdout);
	if (n > 0 && buf[n - 1] != '\n')
		putchar('\n');

	free(buf);
	return 0;
}

static int cmd_write(int fd, const char *name, const char *data)
{
	struct kdal_ioctl_devname dn;
	ssize_t n;
	size_t len;

	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, name, KDAL_NAME_MAX - 1);
	if (ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) < 0) {
		perror("ioctl SELECT_DEV");
		return 1;
	}

	len = strlen(data);
	n = write(fd, data, len);
	if (n < 0) {
		perror("write");
		return 1;
	}

	printf("Wrote %zd/%zu bytes to %s\n", n, len, name);
	return 0;
}

/* ── usage ──────────────────────────────────────────────────────── */

static void usage(void)
{
	fprintf(stderr,
		"Usage: kdality <command> [args...]\n"
		"\n"
		"Runtime commands:\n"
		"  version                 Show KDAL kernel module version\n"
		"  list                    List registered devices\n"
		"  info <device>           Show device info\n"
		"  power <device> <state>  Set power (on/off/suspend)\n"
		"  read <device> <count>   Read bytes from device\n"
		"  write <device> <data>   Write string to device\n"
		"\n"
		"Toolchain commands:\n"
		"  init <name> [opts]         Scaffold a new KDAL driver project\n"
		"  compile [opts] <file.kdc>  Compile a KDAL driver to a kernel module\n"
		"  dt-gen <file.kdh> [opts]   Generate Device Tree overlay from .kdh\n"
		"  simulate <file.kdc> [opts] Dry-run a driver (no kernel needed)\n"
		"  test-gen <file.kdc> [opts] Generate KUnit test stubs\n"
		"  lint <file> [--strict]     Static analysis for .kdc/.kdh files\n"
		"\n"
		"  compile options:\n"
		"    -o <dir>    output directory (default: .)\n"
		"    -K <dir>    kernel build directory (KDIR)\n"
		"    -x <pfx>    CROSS_COMPILE prefix\n"
		"    -v          verbose\n");
}

int kdalctl_run(int argc, char **argv)
{
	int fd, ret;

	if (argc < 2) {
		usage();
		return 1;
	}

	/* Non-device commands */
	if (strcmp(argv[1], "version") == 0 && argc == 2) {
		/* Try to open device; if it fails, show tool version */
		fd = open(KDAL_DEV_PATH, O_RDWR);
		if (fd < 0) {
			printf("kdality 0.1.0 (kernel module not loaded)\n");
			return 0;
		}
		ret = cmd_version(fd);
		close(fd);
		return ret;
	}

	/* All other commands need the device open */
	fd = open(KDAL_DEV_PATH, O_RDWR);
	if (fd < 0) {
		fprintf(stderr,
			"Cannot open %s: %s\n"
			"Is the kdal module loaded?\n",
			KDAL_DEV_PATH, strerror(errno));
		return 1;
	}

	if (strcmp(argv[1], "list") == 0) {
		ret = cmd_list(fd);
	} else if (strcmp(argv[1], "info") == 0 && argc >= 3) {
		ret = cmd_info(fd, argv[2]);
	} else if (strcmp(argv[1], "power") == 0 && argc >= 4) {
		ret = cmd_power(fd, argv[2], argv[3]);
	} else if (strcmp(argv[1], "read") == 0 && argc >= 4) {
		ret = cmd_read(fd, argv[2], argv[3]);
	} else if (strcmp(argv[1], "write") == 0 && argc >= 4) {
		ret = cmd_write(fd, argv[2], argv[3]);
	} else {
		usage();
		ret = 1;
	}

	close(fd);
	return ret;
}
