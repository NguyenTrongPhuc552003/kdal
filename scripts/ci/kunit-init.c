// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal /init for the KUnit QEMU run (scripts/ci/kunit-qemu.sh).
 *
 * Freestanding arm64 Linux program: no libc, raw system calls. It loads
 * /kdal.ko (built with KDAL_KUNIT=y, so the suites run while the module
 * loads and print KTAP on the kernel log), reports the result, prints
 * KDAL's resource demand (debugfs "demand") between markers, waits for the
 * console to send it all, and powers the machine off so QEMU exits.
 *
 * The report goes to /dev/kmsg, one record per line with the tag
 * "KDAL-DEMAND| ": the kernel prints its own lines later, from the
 * console's printing thread, so lines written to the console directly can
 * land between KUnit's (KDAL docs/open-findings-20261121.md, F3); kernel
 * log records keep their order and are never split.
 */

#define AT_FDCWD		-100
#define O_RDONLY		0
#define O_WRONLY		1
#define NR_IOCTL		29
#define NR_READ			63
#define NR_WRITE		64
#define NR_CLOSE		57
#define NR_MOUNT		40
#define NR_OPENAT		56
#define NR_REBOOT		142
#define NR_FINIT_MODULE		273
#define TCSBRK			0x5409
#define REBOOT_MAGIC1		0xfee1dead
#define REBOOT_MAGIC2		672274793
#define REBOOT_POWER_OFF	0x4321fedc

static long sys3(long nr, long a, long b, long c)
{
	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8), "r"(x1), "r"(x2)
			 : "memory");
	return x0;
}

static long sys4(long nr, long a, long b, long c, long d)
{
	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8), "r"(x1), "r"(x2), "r"(x3)
			 : "memory");
	return x0;
}

static long sys5(long nr, long a, long b, long c, long d, long e)
{
	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
			 : "memory");
	return x0;
}

static void say(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	sys3(NR_WRITE, 1, (long)s, n);
}

/* One kernel log record: the tag, then n bytes of s (one line). */
static void report(long kmsg, const char *s, long n)
{
	static char rec[1024];
	static const char tag[] = "KDAL-DEMAND| ";
	long k = 0;

	for (long i = 0; tag[i]; i++)
		rec[k++] = tag[i];
	for (long i = 0; i < n && k < (long)sizeof(rec) - 1; i++)
		rec[k++] = s[i];
	rec[k++] = '\n';
	sys3(NR_WRITE, kmsg, (long)rec, k);
}

static void print_demand(void)
{
	static char buf[8192];
	long fd, kmsg, n, len, start, i;

	if (sys5(NR_MOUNT, (long)"debugfs", (long)"/d", (long)"debugfs", 0, 0) != 0) {
		say("KDAL-CI: cannot mount debugfs\n");
		return;
	}
	fd = sys4(NR_OPENAT, AT_FDCWD, (long)"/d/kdal/demand", O_RDONLY, 0);
	if (fd < 0) {
		say("KDAL-CI: no /d/kdal/demand\n");
		return;
	}
	kmsg = sys4(NR_OPENAT, AT_FDCWD, (long)"/dev/kmsg", O_WRONLY, 0);
	if (kmsg < 0) {
		say("KDAL-CI: no /dev/kmsg\n");
		sys3(NR_CLOSE, fd, 0, 0);
		return;
	}
	report(kmsg, "KDAL-DEMAND-BEGIN", 17);
	len = 0;
	while (len < (long)sizeof(buf) &&
	       (n = sys3(NR_READ, fd, (long)(buf + len), sizeof(buf) - len)) > 0)
		len += n;
	for (start = 0, i = 0; i < len; i++) {
		if (buf[i] == '\n') {
			report(kmsg, buf + start, i - start);
			start = i + 1;
		}
	}
	if (start < len)
		report(kmsg, buf + start, len - start);
	report(kmsg, "KDAL-DEMAND-END", 15);
	sys3(NR_CLOSE, kmsg, 0, 0);
	sys3(NR_CLOSE, fd, 0, 0);
}

void _start(void)
{
	long fd, ret;

	/* The kernel opens /dev/console (in the initramfs) as fds 0 to 2. */
	fd = sys4(NR_OPENAT, AT_FDCWD, (long)"/kdal.ko", O_RDONLY, 0);
	if (fd < 0) {
		say("KDAL-CI: cannot open /kdal.ko\n");
	} else {
		ret = sys3(NR_FINIT_MODULE, fd, (long)"", 0);
		say(ret == 0 ? "KDAL-CI: insmod ok\n" : "KDAL-CI: insmod failed\n");
		if (ret == 0)
			print_demand();
	}
	/*
	 * write() returns once the bytes are queued in the console tty, and
	 * the kernel's power-off message is written synchronously, so it can
	 * overtake them: wait until the tty and the UART are empty (tcdrain(),
	 * which is TCSBRK with a non-zero argument: no break is sent). HAVEN's
	 * docs/methodology/stage2/20261114-console-drain.
	 */
	sys3(NR_IOCTL, 1, TCSBRK, 1);
	sys4(NR_REBOOT, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_POWER_OFF, 0);
	for (;;)
		;
}
