/*
 * templates.h — embedded file templates for kdality init.
 *
 * Each template is a static const char[] that gets written to disk
 * by the init subcommand, with placeholder substitution.
 */

#ifndef KDALITY_TEMPLATES_H
#define KDALITY_TEMPLATES_H

/* ── .kdh device header template ─────────────────────────────────── */

static const char tmpl_kdh[] =
	"// %s.kdh — device header for %s\n"
	"//\n"
	"// This file describes the hardware registers, signals,\n"
	"// and capabilities of the %s device.\n"
	"\n"
	"device %s {\n"
	"    class %s;\n"
	"    compatible \"%s,%s\";\n"
	"\n"
	"    register_map {\n"
	"        CTRL   0x00 rw;    // control register\n"
	"        STATUS 0x04 ro;    // status register\n"
	"        DATA   0x08 rw;    // data register\n"
	"    }\n"
	"\n"
	"    signals {\n"
	"        irq;               // hardware interrupt\n"
	"    }\n"
	"\n"
	"    capabilities {\n"
	"        dma false;\n"
	"    }\n"
	"\n"
	"    power_states {\n"
	"        off;\n"
	"        on;\n"
	"        suspend;\n"
	"    }\n"
	"}\n";

/* ── .kdc driver code template ───────────────────────────────────── */

static const char tmpl_kdc[] =
	"// %s_driver.kdc — driver implementation for %s\n"
	"//\n"
	"// Compile with: kdality compile %s_driver.kdc -o output/\n"
	"\n"
	"use \"%s.kdh\";\n"
	"\n"
	"driver %s_driver for %s {\n"
	"\n"
	"    on probe {\n"
	"        reg_write(CTRL, 0x01);    // enable device\n"
	"        log(\"%s driver probed\");\n"
	"    }\n"
	"\n"
	"    on remove {\n"
	"        reg_write(CTRL, 0x00);    // disable device\n"
	"        log(\"%s driver removed\");\n"
	"    }\n"
	"\n"
	"    on read {\n"
	"        // Wait for data ready\n"
	"        if (reg_read(STATUS) & 0x01) {\n"
	"            return reg_read(DATA);\n"
	"        }\n"
	"        return -1;\n"
	"    }\n"
	"\n"
	"    on write {\n"
	"        reg_write(DATA, input);\n"
	"    }\n"
	"\n"
	"    on signal irq {\n"
	"        log(\"interrupt received\");\n"
	"    }\n"
	"\n"
	"    on power off {\n"
	"        reg_write(CTRL, 0x00);\n"
	"    }\n"
	"\n"
	"    on power on {\n"
	"        reg_write(CTRL, 0x01);\n"
	"    }\n"
	"}\n";

/* ── Makefile template ───────────────────────────────────────────── */

static const char tmpl_makefile[] =
	"# Makefile for %s KDAL project\n"
	"#\n"
	"# Usage:\n"
	"#   make compile   — compile .kdc to C + Kbuild Makefile\n"
	"#   make module    — build the kernel module (requires KDIR)\n"
	"#   make clean     — remove build artifacts\n"
	"\n"
	"KDALITY ?= kdality\n"
	"KDIR   ?= /lib/modules/$$(shell uname -r)/build\n"
	"OUT    := output\n"
	"\n"
	".PHONY: all compile module clean\n"
	"\n"
	"all: compile\n"
	"\n"
	"compile:\n"
	"\t$(KDALITY) compile %s_driver.kdc -o $(OUT)\n"
	"\n"
	"module: compile\n"
	"\t$(MAKE) -C $(KDIR) M=$$(pwd)/$(OUT) modules\n"
	"\n"
	"clean:\n"
	"\trm -rf $(OUT)\n";

/* ── README template ─────────────────────────────────────────────── */

static const char tmpl_readme[] =
	"# %s — KDAL Driver Project\n"
	"\n"
	"A kernel driver for the **%s** device, written in KDAL DSL.\n"
	"\n"
	"## Files\n"
	"\n"
	"| File | Purpose |\n"
	"| ---- | ------- |\n"
	"| `%s.kdh` | Device header (registers, signals, capabilities) |\n"
	"| `%s_driver.kdc` | Driver implementation (event handlers) |\n"
	"| `Makefile` | Build orchestration |\n"
	"\n"
	"## Quick Start\n"
	"\n"
	"```sh\n"
	"# 1. Compile the .kdc file to kernel C\n"
	"make compile\n"
	"\n"
	"# 2. Build the kernel module (requires kernel headers)\n"
	"make module KDIR=/path/to/kernel\n"
	"\n"
	"# 3. Load the module\n"
	"sudo insmod output/%s_driver.ko\n"
	"```\n"
	"\n"
	"## Learn More\n"
	"\n"
	"- [KDAL Language Guide](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/docs/language_guide.md)\n"
	"- [KDAL API Reference](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/docs/api_reference.md)\n";

#endif /* KDALITY_TEMPLATES_H */
