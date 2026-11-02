// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal /init for the KUnit QEMU run (scripts/ci/kunit-qemu.sh).
 *
 * Freestanding arm64 Linux program: no libc, raw system calls. It loads
 * /kdal.ko (built with KDAL_KUNIT=y, so the suites run while the module
 * loads and print KTAP on the kernel log), reports the result and powers
 * the machine off so QEMU exits.
 */

#define AT_FDCWD		-100
#define O_RDONLY		0
#define NR_WRITE		64
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

static void say(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	sys3(NR_WRITE, 1, (long)s, n);
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
	}
	sys4(NR_REBOOT, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_POWER_OFF, 0);
	for (;;)
		;
}
