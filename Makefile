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

kdal-y := \
	src/core/module.o \
	src/core/kdal.o \
	src/core/chardev.o \
	src/core/debugfs.o \
	src/core/registry.o \
	src/core/event.o \
	src/core/power.o \
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

ccflags-y += -I$(PWD)/include
