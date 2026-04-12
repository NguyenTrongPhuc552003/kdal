# Makefile for init KDAL project
#
# Usage:
#   make compile   - compile .kdc to C + Kbuild Makefile
#   make module    - build the kernel module (requires KDIR)
#   make clean     - remove build artifacts

KDALITY ?= kdality
KDIR   ?= /lib/modules/$$(shell uname -r)/build
OUT    := output

.PHONY: all compile module clean

all: compile

compile:
	$(KDALITY) compile init.kdc -o $(OUT)

module: compile
	$(MAKE) -C $(KDIR) M=$$(pwd)/$(OUT) modules

clean:
	rm -rf $(OUT)
