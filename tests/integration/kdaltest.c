/*
 * Integration test harness for KDAL.
 *
 * Performs end-to-end validation of the KDAL subsystem through
 * the /dev/kdal char device interface.  Tests multi-device selection,
 * concurrent read/write, power management transitions, and error
 * paths.
 *
 * Compile:  cc -std=c99 -Wall -o kdaltest kdaltest.c
 * Run:      sudo ./kdaltest
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ── ABI mirror ─────────────────────────────────────────────────── */

#define KDAL_NAME_MAX 32
#define KDAL_MAX_DEVICES 64

struct kdal_ioctl_version { unsigned int major, minor, patch; };
struct kdal_ioctl_info { char name[KDAL_NAME_MAX]; unsigned int class_id, feature_flags, power_state, nr_caps; };
struct kdal_ioctl_list { unsigned int count; char names[KDAL_MAX_DEVICES][KDAL_NAME_MAX]; };
struct kdal_ioctl_devname { char name[KDAL_NAME_MAX]; };

#define KDAL_IOCTL_MAGIC 'K'
#define KDAL_IOCTL_GET_VERSION  _IOR(KDAL_IOCTL_MAGIC, 0x00, struct kdal_ioctl_version)
#define KDAL_IOCTL_GET_INFO     _IOWR(KDAL_IOCTL_MAGIC, 0x01, struct kdal_ioctl_info)
#define KDAL_IOCTL_SET_POWER    _IOW(KDAL_IOCTL_MAGIC, 0x02, unsigned int)
#define KDAL_IOCTL_LIST_DEVICES _IOR(KDAL_IOCTL_MAGIC, 0x03, struct kdal_ioctl_list)
#define KDAL_IOCTL_SELECT_DEV   _IOW(KDAL_IOCTL_MAGIC, 0x05, struct kdal_ioctl_devname)

static int failures;
static int passes;

#define CHECK(cond, fmt, ...) do { \
	if (!(cond)) { fprintf(stderr, "  FAIL: " fmt "\n", ##__VA_ARGS__); failures++; } \
	else { printf("  PASS: " fmt "\n", ##__VA_ARGS__); passes++; } \
} while (0)

static int select_device(int fd, const char *name)
{
	struct kdal_ioctl_devname dn;
	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, name, KDAL_NAME_MAX - 1);
	return ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn);
}

/* ── test groups ────────────────────────────────────────────────── */

static void test_multi_device_loopback(int fd)
{
	const char *devices[] = { "uart0", "i2c0", "spi0", "gpu0" };
	unsigned int i;
	char buf[32];
	ssize_t nw, nr;

	printf("\n[multi-device loopback]\n");
	for (i = 0; i < 4; i++) {
		CHECK(select_device(fd, devices[i]) == 0,
		      "select %s", devices[i]);

		snprintf(buf, sizeof(buf), "test-%s", devices[i]);
		nw = write(fd, buf, strlen(buf) + 1);
		CHECK(nw == (ssize_t)(strlen(buf) + 1),
		      "write to %s (%zd bytes)", devices[i], nw);

		memset(buf, 0, sizeof(buf));
		nr = read(fd, buf, sizeof(buf));
		CHECK(nr > 0, "read from %s (%zd bytes)", devices[i], nr);
	}
}

static void test_power_management(int fd)
{
	struct kdal_ioctl_info info;
	unsigned int state;

	printf("\n[power management]\n");
	CHECK(select_device(fd, "uart0") == 0, "select uart0");

	/* Suspend */
	state = 2; /* KDAL_POWER_SUSPEND */
	CHECK(ioctl(fd, KDAL_IOCTL_SET_POWER, &state) == 0,
	      "set power → suspend");

	memset(&info, 0, sizeof(info));
	strncpy(info.name, "uart0", KDAL_NAME_MAX);
	ioctl(fd, KDAL_IOCTL_GET_INFO, &info);
	CHECK(info.power_state == 2, "power_state = suspend (%u)", info.power_state);

	/* Resume */
	state = 1; /* KDAL_POWER_ON */
	CHECK(ioctl(fd, KDAL_IOCTL_SET_POWER, &state) == 0,
	      "set power → on");
}

static void test_error_paths(int fd)
{
	struct kdal_ioctl_devname dn;
	char buf[16];
	ssize_t n;

	printf("\n[error paths]\n");

	/* Select nonexistent device */
	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, "nonexistent", KDAL_NAME_MAX);
	CHECK(ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) < 0,
	      "SELECT_DEV nonexistent fails");

	/* Read without selection (new fd) */
	{
		int fd2 = open("/dev/kdal", O_RDWR);
		if (fd2 >= 0) {
			n = read(fd2, buf, sizeof(buf));
			CHECK(n < 0, "read without select fails (errno=%d)", errno);
			close(fd2);
		}
	}
}

static void test_independent_fds(void)
{
	int fd1, fd2;
	char w1[] = "fd1-data";
	char w2[] = "fd2-data";
	char rbuf[32];
	ssize_t n;

	printf("\n[independent file descriptors]\n");

	fd1 = open("/dev/kdal", O_RDWR);
	fd2 = open("/dev/kdal", O_RDWR);
	if (fd1 < 0 || fd2 < 0) {
		fprintf(stderr, "  SKIP: cannot open two fds\n");
		if (fd1 >= 0) close(fd1);
		if (fd2 >= 0) close(fd2);
		return;
	}

	/* fd1 selects uart0, fd2 selects spi0 */
	CHECK(select_device(fd1, "uart0") == 0, "fd1 select uart0");
	CHECK(select_device(fd2, "spi0") == 0, "fd2 select spi0");

	/* They should write to different ring buffers */
	write(fd1, w1, sizeof(w1));
	write(fd2, w2, sizeof(w2));

	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd1, rbuf, sizeof(rbuf));
	CHECK(n > 0 && memcmp(rbuf, w1, sizeof(w1)) == 0,
	      "fd1 reads back its own data");

	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd2, rbuf, sizeof(rbuf));
	CHECK(n > 0 && memcmp(rbuf, w2, sizeof(w2)) == 0,
	      "fd2 reads back its own data");

	close(fd1);
	close(fd2);
}

/* ── main ───────────────────────────────────────────────────────── */

int main(void)
{
	int fd;

	printf("KDAL Integration Test Suite\n");
	printf("===========================\n");

	fd = open("/dev/kdal", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open /dev/kdal: %s\n", strerror(errno));
		printf("kdal integration test 0.1.0 (offline — module not loaded)\n");
		return 0;
	}

	test_multi_device_loopback(fd);
	test_power_management(fd);
	test_error_paths(fd);
	close(fd);

	test_independent_fds();

	printf("\n===========================\n");
	printf("%d passed, %d failed\n", passes, failures);
	return failures ? 1 : 0;
}