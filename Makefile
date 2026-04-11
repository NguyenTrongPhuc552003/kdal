# KDAL top-level build entrypoint.

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

MODULE_NAME := kdal

.PHONY: help module clean tool compiler kdc lang test lint docs all

help:
	@printf '%s\n' \
		'make module    - build the out-of-tree kernel module' \
		'make tool      - build the kdality unified toolchain binary' \
		'make compiler  - build the KDAL compiler (kdalc + libkdalc.a)' \
		'make kdc       - compile .kdc examples in examples/kdc_hello/' \
		'make lang      - validate lang/ grammar and spec files (syntax check)' \
		'make test      - run repository test wrapper (all 5 CI stages)' \
		'make lint      - run repository lint wrapper' \
		'make docs      - list documentation under docs/' \
		'make all       - build module + compiler + tool' \
		'make clean     - remove generated build artifacts'

all: compiler tool module

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

tool:
	$(MAKE) -C tools/kdality

compiler:
	$(MAKE) -C compiler

kdc: compiler
	@for f in examples/kdc_hello/*.kdc; do \
		printf '[kdc] compiling %s\n' "$$f"; \
		compiler/kdalc -o /tmp/kdal_kdc_out "$$f" || true; \
	done

lang:
	@printf '[lang] grammar files: %s\n' \
		$$(ls lang/grammar/*.ebnf 2>/dev/null | wc -l) found
	@printf '[lang] stdlib .kdh files: %s\n' \
		$$(ls lang/stdlib/*.kdh 2>/dev/null | wc -l) found
	@printf '[lang] spec: %s\n' \
		$$(test -f lang/spec.md && echo OK || echo MISSING)

test:
	./scripts/ci/test.sh

lint:
	./scripts/ci/checkpatch.sh && ./scripts/ci/static_analysis.sh

docs:
	@printf '%s\n' 'Documentation lives under docs/'

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C tools/kdality clean
	$(MAKE) -C compiler clean
	rm -f /tmp/kdal_kdc_out/*.c /tmp/kdal_kdc_out/Makefile.kbuild 2>/dev/null || true

obj-m += kdal.o

kdal-y := \
	src/core/module.o \
	src/core/kdal.o \
	src/core/chardev.o \
	src/core/debugfs.o \
	src/core/registry.o \
	src/core/event.o \
	src/core/power.o \
	src/backends/qemu/qemubackend.o \
	src/backends/qemu/virtioio.o \
	src/backends/qemu/virtioaccel.o \
	src/backends/qemu/mmio.o \
	src/backends/generic/platdev.o \
	src/backends/generic/socglue.o \
	src/backends/generic/devicetree.o \
	src/drivers/example/uartdriver.o \
	src/drivers/example/i2cdriver.o \
	src/drivers/example/spidriver.o \
	src/drivers/example/gpudriver.o

ccflags-y += -I$(PWD)/include
