/*
 * Throughput benchmark for the KDAL data path.
 *
 * Measures write + readback throughput through /dev/kdal for a
 * selected device.  Reports bytes/sec and latency statistics
 * suitable for thesis performance evaluation.
 *
 * Compile:  cc -std=c99 -Wall -O2 -o benchmark benchmark.c
 * Run:      sudo ./benchmark [device] [iterations] [block_size]
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

/* ── ABI mirror ─────────────────────────────────────────────────── */

#define KDAL_NAME_MAX 32

struct kdal_ioctl_devname {
	char name[KDAL_NAME_MAX];
};

#define KDAL_IOCTL_MAGIC 'K'
#define KDAL_IOCTL_SELECT_DEV \
	_IOW(KDAL_IOCTL_MAGIC, 0x05, struct kdal_ioctl_devname)

/* ── timing ─────────────────────────────────────────────────────── */

static double time_diff_ms(const struct timespec *start,
			   const struct timespec *end)
{
	double s = (double)(end->tv_sec - start->tv_sec) * 1000.0;
	double ns = (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
	return s + ns;
}

int main(int argc, char **argv)
{
	const char *devname = argc > 1 ? argv[1] : "uart0";
	int iterations = argc > 2 ? atoi(argv[2]) : 1000;
	int block_size = argc > 3 ? atoi(argv[3]) : 256;
	struct kdal_ioctl_devname dn;
	struct timespec t0, t1;
	char *wbuf, *rbuf;
	double elapsed, throughput;
	int fd, i;
	long total_bytes = 0;

	if (iterations <= 0)
		iterations = 1000;
	if (block_size <= 0 || block_size > 4096)
		block_size = 256;

	fd = open("/dev/kdal", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open /dev/kdal: %s\n", strerror(errno));
		puts("kdal benchmark 0.1.0 (offline - no module loaded)");
		return 0;
	}

	memset(&dn, 0, sizeof(dn));
	strncpy(dn.name, devname, KDAL_NAME_MAX - 1);
	if (ioctl(fd, KDAL_IOCTL_SELECT_DEV, &dn) < 0) {
		fprintf(stderr, "SELECT_DEV '%s': %s\n", devname,
			strerror(errno));
		close(fd);
		return 1;
	}

	wbuf = malloc((size_t)block_size);
	rbuf = malloc((size_t)block_size);
	if (!wbuf || !rbuf) {
		fprintf(stderr, "out of memory\n");
		close(fd);
		return 1;
	}
	memset(wbuf, 0xAA, (size_t)block_size);

	printf("KDAL throughput benchmark\n");
	printf("  device:     %s\n", devname);
	printf("  iterations: %d\n", iterations);
	printf("  block_size: %d bytes\n\n", block_size);

	clock_gettime(CLOCK_MONOTONIC, &t0);

	for (i = 0; i < iterations; i++) {
		ssize_t n = write(fd, wbuf, (size_t)block_size);
		if (n <= 0)
			break;
		total_bytes += n;

		n = read(fd, rbuf, (size_t)block_size);
		if (n <= 0)
			break;
		total_bytes += n;
	}

	clock_gettime(CLOCK_MONOTONIC, &t1);

	elapsed = time_diff_ms(&t0, &t1);
	throughput = (elapsed > 0.0) ? ((double)total_bytes / 1024.0) /
					       (elapsed / 1000.0) :
				       0.0;

	printf("Results:\n");
	printf("  completed:  %d / %d iterations\n", i, iterations);
	printf("  total data: %ld bytes\n", total_bytes);
	printf("  elapsed:    %.2f ms\n", elapsed);
	printf("  throughput: %.1f KB/s\n", throughput);
	printf("  avg iter:   %.3f ms\n", (i > 0) ? elapsed / i : 0.0);

	free(wbuf);
	free(rbuf);
	close(fd);
	return 0;
}
