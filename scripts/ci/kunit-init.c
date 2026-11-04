// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal /init for the KUnit QEMU run (scripts/ci/kunit-qemu.sh).
 *
 * Freestanding arm64 Linux program: no libc, raw system calls. It loads
 * /kdal.ko (built with KDAL_KUNIT=y, so the suites run while the module
 * loads and print KTAP on the kernel log), reports the result, prints
 * KDAL's resource demand (debugfs "demand") between markers, and powers
 * the machine off so QEMU exits.
 */

#define AT_FDCWD		-100
#define O_RDONLY		0
#define NR_READ			63
#define NR_WRITE		64
#define NR_CLOSE		57
#define NR_MOUNT		40
#define NR_OPENAT		56
#define NR_REBOOT		142
#define NR_FINIT_MODULE		273
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

static void print_demand(void)
{
	static char buf[8192];
	long fd, n;

	if (sys5(NR_MOUNT, (long)"debugfs", (long)"/d", (long)"debugfs", 0, 0) != 0) {
		say("KDAL-CI: cannot mount debugfs\n");
		return;
	}
	fd = sys4(NR_OPENAT, AT_FDCWD, (long)"/d/kdal/demand", O_RDONLY, 0);
	if (fd < 0) {
		say("KDAL-CI: no /d/kdal/demand\n");
		return;
	}
	say("KDAL-DEMAND-BEGIN\n");
	while ((n = sys3(NR_READ, fd, (long)buf, sizeof(buf))) > 0)
		sys3(NR_WRITE, 1, (long)buf, n);
	say("KDAL-DEMAND-END\n");
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
	sys4(NR_REBOOT, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_POWER_OFF, 0);
	for (;;)
		;
}
