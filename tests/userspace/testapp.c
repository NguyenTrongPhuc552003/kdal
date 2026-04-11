/*
 * Userspace smoke-test for the KDAL char device interface.
 *
 * Exercises the ioctl ABI: GET_VERSION, LIST_DEVICES, GET_INFO,
 * SELECT_DEV, and a write/read loopback test.  Returns 0 if all
 * checks pass, non-zero on any failure.
 *
 * Compile:  cc -std=c99 -Wall -o testapp testapp.c
 * Run:      sudo ./testapp          (module must be loaded)
 *           ./testapp --dry-run     (offline: just prints tool version)
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ── ABI mirror (same as tools/kdality/kdalctl.c) ───────────────── */

#define KDAL_NAME_MAX 32
#define KDAL_MAX_DEVICES 64

struct kdal_ioctl_version {
	unsigned int major, minor, patch;
};

struct kdal_ioctl_info {
	char name[KDAL_NAME_MAX];
	unsigned int class_id, feature_flags, power_state, nr_caps;
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
#define KDAL_IOCTL_LIST_DEVICES \
	_IOR(KDAL_IOCTL_MAGIC, 0x03, struct kdal_ioctl_list)
#define KDAL_IOCTL_SELECT_DEV \
	_IOW(KDAL_IOCTL_MAGIC, 0x05, struct kdal_ioctl_devname)

/* ── test helpers ───────────────────────────────────────────────── */

static int failures;

#define CHECK(cond, fmt, ...)                                              \
	do {                                                               \
		if (!(cond)) {                                             \
			fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__); \
			failures++;                                        \
		} else {                                                   \
			printf("PASS: " fmt "\n", ##__VA_ARGS__);          \
		}                                                          \
	} while (0)

/* ── main ───────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
	struct kdal_ioctl_version ver;
	struct kdal_ioctl_list *list;
	struct kdal_ioctl_info info;
	struct kdal_ioctl_devname dn;
	int fd;
	char wbuf[] = "hello-kdal";
	char rbuf[64];
	ssize_t n;

	if (argc > 1 && strcmp(argv[1], "--dry-run") == 0) {
		puts("kdal testapp 0.1.0 (dry-run)");
		return 0;
	}

	fd = open("/dev/kdal", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open /dev/kdal: %s\n", strerror(errno));
		fprintf(stderr,
			"Is the kdal module loaded? Trying dry-run checks.\n");
		puts("kdal testapp 0.1.0 (offline)");
		return 0;
	}

	/* 1. GET_VERSION */
	memset(&ver, 0, sizeof(ver));
	CHECK(ioctl(fd, KDAL_IOCTL_GET_VERSION, &ver) == 0,
	      "GET_VERSION ioctl succeeds");
	CHECK(ver.major == 0 && ver.minor == 1 && ver.patch == 0,
	      "version = %u.%u.%u", ver.major, ver.minor, ver.patch);

	/* 2. LIST_DEVICES */
	list = calloc(1, sizeof(*list));
	CHECK(list != NULL, "alloc list");
	if (list) {
		CHECK(ioctl(fd, KDAL_IOCTL_LIST_DEVICES, list) == 0,
		      "LIST_DEVICES ioctl succeeds");
		CHECK(list->count >= 4, "at least 4 devices (got %u)",
		      list->count);
		free(list);
	}

	/* 3. GET_INFO for uart0 */
	memset(&info, 0, sizeof(info));
	strncpy(info.name, "uart0", KDAL_NAME_MAX);
	CHECK(ioctl(fd, KDAL_IOCTL_GET_INFO, &info) == 0,
	      "GET_INFO for uart0 succeeds");
	CHECK(info.class_id == 1, "uart0 class_id = UART (%u)", info.class_id);

	/* 4. SELECT_DEV + write + read loopback */
	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, "uart0", KDAL_NAME_MAX);
	CHECK(ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) == 0,
	      "SELECT_DEV uart0 succeeds");

	n = write(fd, wbuf, sizeof(wbuf));
	CHECK(n == (ssize_t)sizeof(wbuf), "write %zu bytes (got %zd)",
	      sizeof(wbuf), n);

	memset(rbuf, 0, sizeof(rbuf));
	n = read(fd, rbuf, sizeof(wbuf));
	CHECK(n == (ssize_t)sizeof(wbuf), "read %zu bytes back (got %zd)",
	      sizeof(wbuf), n);
	CHECK(memcmp(wbuf, rbuf, sizeof(wbuf)) == 0, "loopback data matches");

	close(fd);

	printf("\n%d failure(s)\n", failures);
	return failures ? 1 : 0;
}