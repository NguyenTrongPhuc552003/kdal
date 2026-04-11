# Minimal KDAL driver - out-of-tree kernel module build
#
# Usage:
#   make KDIR=/path/to/linux KDAL_DIR=/path/to/kdal
#
# For cross-compilation:
#   make KDIR=/path/to/linux KDAL_DIR=/path/to/kdal \
#        ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

KDIR      ?= /lib/modules/$(shell uname -r)/build
KDAL_DIR  ?= ../.
obj-m     := mydriver.o
mydriver-y := driver.o

EXTRA_CFLAGS += -I$(KDAL_DIR)/include

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: all clean
