# KDAL - kbuild Makefile for the out-of-tree kernel module.
#
# Usage:
#   make                 - build the kdal.ko kernel module
#   make clean           - remove kbuild artifacts
#
# For all userspace builds (kdalc, kdality, website, tests, etc.),
# use CMake instead:
#   cmake --preset release
#   cmake --build --preset release --target <name>
#   ./scripts/dev/build.sh --variant release <target>
#
# See: cmake --build --preset release --target help

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# ── kbuild object list ────────────────────────────────────────────────

obj-m += kdal.o

# Out of tree, the kernel's Kconfig never sees ./Kconfig, so these options
# are unset; default them as Kconfig does. kdal.c calls both parts
# unconditionally, so building without them fails at modpost.
CONFIG_KDAL_QEMU_BACKEND ?= y
CONFIG_KDAL_EXAMPLE_DRIVERS ?= y

kdal-y := \
	src/core/module.o \
	src/core/kdal.o \
	src/core/chardev.o \
	src/core/debugfs.o \
	src/core/registry.o \
	src/core/event.o \
	src/core/power.o \
	src/core/demand.o \
	src/backends/generic/platdev.o \
	src/backends/generic/socglue.o \
	src/backends/generic/devicetree.o

kdal-$(CONFIG_KDAL_QEMU_BACKEND) += \
	src/backends/qemu/qemubackend.o \
	src/backends/qemu/virtioio.o \
	src/backends/qemu/virtioaccel.o \
	src/backends/qemu/mmio.o

kdal-$(CONFIG_KDAL_EXAMPLE_DRIVERS) += \
	src/drivers/example/uartdriver.o \
	src/drivers/example/i2cdriver.o \
	src/drivers/example/spidriver.o \
	src/drivers/example/gpudriver.o

# KUnit suites, built into kdal.ko so they can call its internal API (KDAL
# exports no symbols). Needs a kernel with CONFIG_KUNIT=y; the suites run
# when the module loads and report in KTAP on the kernel log:
#   make KDAL_KUNIT=y
kdal-$(KDAL_KUNIT) += \
	tests/kunit/test_registry.o \
	tests/kunit/test_event.o \
	tests/kunit/test_core.o \
	tests/kunit/test_driver.o \
	tests/kunit/test_accel.o \
	tests/kunit/test_demand.o

ccflags-y += -I$(PWD)/include
