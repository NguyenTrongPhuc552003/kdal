/*
 * NPU accelerator demo — userspace program that demonstrates how an NPU
 * device would be accessed through the KDAL abstraction.
 *
 * Since the NPU driver is not yet implemented (planned for Radxa Orion O6),
 * this demo shows the expected userspace workflow and gracefully handles
 * the "device not found" case.
 *
 * Build:
 *   cc -std=c99 -Wall -o npudemo npudemo.c
 *
 * Run (with kdal.ko loaded):
 *   ./npudemo
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

struct kdal_ioctl_list {
	unsigned int count;
	char names[64][KDAL_NAME_MAX];
};

#define KDAL_IOCTL_GET_VERSION   _IOR(KDAL_IOC_MAGIC, 1, struct kdal_ioctl_version)
#define KDAL_IOCTL_LIST_DEVICES  _IOR(KDAL_IOC_MAGIC, 4, struct kdal_ioctl_list)
#define KDAL_IOCTL_SELECT_DEV    _IOW(KDAL_IOC_MAGIC, 6, struct kdal_ioctl_devname)

#define DEV_PATH  "/dev/kdal"
#define NPU_DEV   "npu0"

static int find_npu_device(int fd)
{
	struct kdal_ioctl_list list;
	int rc;
	unsigned int i;

	memset(&list, 0, sizeof(list));
	rc = ioctl(fd, KDAL_IOCTL_LIST_DEVICES, &list);
	if (rc < 0) {
		perror("LIST_DEVICES");
		return -1;
	}

	for (i = 0; i < list.count; i++) {
		if (strncmp(list.names[i], "npu", 3) == 0)
			return 1;
	}
	return 0;
}

int main(void)
{
	int fd;
	int rc;
	struct kdal_ioctl_version ver;
	struct kdal_ioctl_devname sel;

	printf("=== KDAL NPU Accelerator Demo ===\n\n");

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

	/* Check if NPU device is available */
	rc = find_npu_device(fd);
	if (rc < 0) {
		close(fd);
		return 1;
	}
	if (rc == 0) {
		printf("\nNo NPU device found.\n");
		printf("The NPU driver is planned for the Radxa Orion O6 target:\n");
		printf("  - CIX P1 SoC\n");
		printf("  - 30 TOPS NPU\n");
		printf("  - Will implement kdal_accel_ops for inference workloads\n");
		printf("\nExpected workflow when available:\n");
		printf("  1. Select NPU device: ioctl(fd, SELECT_DEV, \"npu0\")\n");
		printf("  2. Write model/data:  write(fd, model_buf, model_size)\n");
		printf("  3. Trigger inference: ioctl(fd, custom_cmd)\n");
		printf("  4. Read results:      read(fd, output_buf, output_size)\n");
		close(fd);
		return 0;
	}

	/* NPU device found — attempt to use it */
	memset(&sel, 0, sizeof(sel));
	snprintf(sel.name, sizeof(sel.name), "%s", NPU_DEV);
	rc = ioctl(fd, KDAL_IOCTL_SELECT_DEV, &sel);
	if (rc < 0) {
		perror("SELECT_DEV npu0");
		close(fd);
		return 1;
	}
	printf("Selected device: %s\n", NPU_DEV);

	/* Placeholder: submit inference workload */
	printf("Submitting inference workload...\n");
	/* write(fd, model_data, model_size); */
	/* read(fd, result_data, result_size); */
	printf("(Not implemented yet — awaiting NPU backend)\n");

	printf("\nNPU demo complete.\n");
	close(fd);
	return 0;
}