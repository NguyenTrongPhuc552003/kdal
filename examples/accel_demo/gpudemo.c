/*
 * GPU accelerator demo — userspace program that exercises the KDAL
 * accelerator path through /dev/kdal ioctls.
 *
 * Demonstrates:
 *   1. Open /dev/kdal
 *   2. Select GPU device
 *   3. Write command buffer
 *   4. Read result
 *   5. Close
 *
 * Build:
 *   cc -std=c99 -Wall -o gpudemo gpudemo.c
 *
 * Run (with kdal.ko loaded):
 *   ./gpudemo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/* Mirror KDAL ioctl ABI */
#define KDAL_IOC_MAGIC  'K'
#define KDAL_NAME_MAX   32

struct kdal_ioctl_version {
	unsigned int major;
	unsigned int minor;
	unsigned int patch;
};

struct kdal_ioctl_devname {
	char name[KDAL_NAME_MAX];
};

struct kdal_ioctl_info {
	char name[KDAL_NAME_MAX];
	unsigned int dev_class;
	unsigned int power_state;
	unsigned int feature_flags;
};

#define KDAL_IOCTL_GET_VERSION  _IOR(KDAL_IOC_MAGIC, 1, struct kdal_ioctl_version)
#define KDAL_IOCTL_SELECT_DEV   _IOW(KDAL_IOC_MAGIC, 6, struct kdal_ioctl_devname)
#define KDAL_IOCTL_GET_INFO     _IOR(KDAL_IOC_MAGIC, 2, struct kdal_ioctl_info)

#define DEV_PATH  "/dev/kdal"
#define GPU_DEV   "gpu0"

int main(void)
{
	int fd;
	int rc;
	struct kdal_ioctl_version ver;
	struct kdal_ioctl_devname sel;
	struct kdal_ioctl_info info;
	const char *cmd = "DISPATCH 128 THREADS";
	char result[256];
	ssize_t n;

	printf("=== KDAL GPU Accelerator Demo ===\n\n");

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open " DEV_PATH);
		fprintf(stderr, "Is the kdal module loaded?\n");
		return 1;
	}

	/* Check version */
	rc = ioctl(fd, KDAL_IOCTL_GET_VERSION, &ver);
	if (rc < 0) {
		perror("GET_VERSION");
		close(fd);
		return 1;
	}
	printf("KDAL version: %u.%u.%u\n", ver.major, ver.minor, ver.patch);

	/* Select GPU device */
	memset(&sel, 0, sizeof(sel));
	snprintf(sel.name, sizeof(sel.name), "%s", GPU_DEV);
	rc = ioctl(fd, KDAL_IOCTL_SELECT_DEV, &sel);
	if (rc < 0) {
		perror("SELECT_DEV gpu0");
		close(fd);
		return 1;
	}
	printf("Selected device: %s\n", GPU_DEV);

	/* Get device info */
	rc = ioctl(fd, KDAL_IOCTL_GET_INFO, &info);
	if (rc < 0) {
		perror("GET_INFO");
		close(fd);
		return 1;
	}
	printf("Device class: %u, power state: %u, flags: 0x%x\n",
	       info.dev_class, info.power_state, info.feature_flags);

	/* Write command buffer to GPU */
	printf("\nSubmitting command: \"%s\"\n", cmd);
	n = write(fd, cmd, strlen(cmd));
	if (n < 0) {
		perror("write");
		close(fd);
		return 1;
	}
	printf("Wrote %zd bytes\n", n);

	/* Read result back */
	memset(result, 0, sizeof(result));
	n = read(fd, result, sizeof(result) - 1);
	if (n < 0) {
		if (errno == EAGAIN) {
			printf("No data available (GPU processing pending)\n");
		} else {
			perror("read");
		}
	} else if (n > 0) {
		printf("Read back %zd bytes: \"%s\"\n", n, result);
	} else {
		printf("Read returned 0 bytes\n");
	}

	printf("\nGPU demo complete.\n");
	close(fd);
	return 0;
}