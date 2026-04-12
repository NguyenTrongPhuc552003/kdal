/*
 * dtgen.c - kdality dt-gen subcommand.
 *
 * Generates a Device Tree Source overlay (.dtso) from a .kdh device header.
 *
 *   kdality dt-gen <file.kdh> [-o <outdir>] [--base-addr <addr>] [--irq <num>]
 *
 * This replaces manual DT overlay writing - newcomers can describe their
 * device in .kdh and auto-generate the DT binding.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Simple .kdh scanner ─────────────────────────────────────────
 *
 * We parse just enough of the .kdh format to extract:
 *   - device name
 *   - compatible string
 *   - register map (names + offsets)
 *   - signals (for interrupt bindings)
 *
 * This is a lightweight approach - the full compiler AST could be
 * used instead if libkdalc grows a .kdh-only parse mode.
 */

#define MAX_REGS 32
#define MAX_SIGNALS 16
#define NAME_LEN 64

struct kdh_reg {
	char name[NAME_LEN];
	unsigned long offset;
};

struct kdh_info {
	char device_name[NAME_LEN];
	char compatible[128];
	char class_name[NAME_LEN];
	struct kdh_reg regs[MAX_REGS];
	int nregs;
	char signals[MAX_SIGNALS][NAME_LEN];
	int nsignals;
};

static int parse_kdh(const char *path, struct kdh_info *info)
{
	FILE *fp;
	char line[512];
	int in_regmap = 0;
	int in_signals = 0;

	memset(info, 0, sizeof(*info));

	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "dt-gen: cannot open '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *p = line;

		/* skip whitespace */
		while (*p == ' ' || *p == '\t')
			p++;

		/* skip comments and empty lines */
		if (*p == '/' || *p == '\n' || *p == '\0')
			continue;

		/* detect sections */
		if (strstr(p, "register_map")) {
			in_regmap = 1;
			in_signals = 0;
			continue;
		}
		if (strstr(p, "signals")) {
			in_signals = 1;
			in_regmap = 0;
			continue;
		}
		if (strstr(p, "capabilities") || strstr(p, "power_states")) {
			in_regmap = 0;
			in_signals = 0;
			continue;
		}

		/* closing brace resets */
		if (*p == '}') {
			in_regmap = 0;
			in_signals = 0;
			continue;
		}

		/* device name */
		if (strncmp(p, "device ", 7) == 0) {
			char *name = p + 7;
			char *end = name;
			while (*end && *end != ' ' && *end != '{' &&
			       *end != '\n')
				end++;
			size_t len = (size_t)(end - name);
			if (len >= NAME_LEN)
				len = NAME_LEN - 1;
			memcpy(info->device_name, name, len);
			info->device_name[len] = '\0';
			continue;
		}

		/* class */
		if (strncmp(p, "class ", 6) == 0) {
			char *cls = p + 6;
			char *end = cls;
			while (*end && *end != ';' && *end != '\n')
				end++;
			size_t len = (size_t)(end - cls);
			if (len >= NAME_LEN)
				len = NAME_LEN - 1;
			memcpy(info->class_name, cls, len);
			info->class_name[len] = '\0';
			continue;
		}

		/* compatible string */
		if (strncmp(p, "compatible ", 11) == 0) {
			char *q = strchr(p, '"');
			if (q) {
				q++;
				const char *end = strchr(q, '"');
				if (end) {
					size_t len = (size_t)(end - q);
					if (len >= sizeof(info->compatible))
						len = sizeof(info->compatible) -
						      1;
					memcpy(info->compatible, q, len);
					info->compatible[len] = '\0';
				}
			}
			continue;
		}

		/* register entries: NAME 0xOFFSET mode; */
		if (in_regmap && info->nregs < MAX_REGS) {
			char rname[NAME_LEN];
			unsigned long off;
			if (sscanf(p, "%63s 0x%lx", rname, &off) == 2) {
				strncpy(info->regs[info->nregs].name, rname,
					NAME_LEN - 1);
				info->regs[info->nregs].offset = off;
				info->nregs++;
			}
			continue;
		}

		/* signal entries: name; */
		if (in_signals && info->nsignals < MAX_SIGNALS) {
			char sname[NAME_LEN];
			if (sscanf(p, "%63[^; \t\n]", sname) == 1) {
				strncpy(info->signals[info->nsignals], sname,
					NAME_LEN - 1);
				info->nsignals++;
			}
			continue;
		}
	}

	fclose(fp);

	if (!info->device_name[0]) {
		fprintf(stderr, "dt-gen: no 'device' found in '%s'\n", path);
		return -1;
	}
	if (!info->compatible[0]) {
		snprintf(info->compatible, sizeof(info->compatible), "kdal,%s",
			 info->device_name);
	}

	return 0;
}

/* ── DT overlay generation ───────────────────────────────────────── */

static int generate_dtso(const struct kdh_info *info, const char *outdir,
			 unsigned long base_addr, int irq_num)
{
	char path[512];
	FILE *fp;
	unsigned long reg_size;

	/* Compute reg size from highest register offset + 4 */
	reg_size = 0x100; /* default 256 bytes */
	if (info->nregs > 0) {
		unsigned long max_off = 0;
		for (int i = 0; i < info->nregs; i++) {
			if (info->regs[i].offset > max_off)
				max_off = info->regs[i].offset;
		}
		/* Round up to next power of 2 boundary, min 0x100 */
		reg_size = max_off + 4;
		if (reg_size < 0x100)
			reg_size = 0x100;
	}

	snprintf(path, sizeof(path), "%s/%s.dtso", outdir, info->device_name);
	fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "dt-gen: cannot create '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	/* DT overlay header */
	fprintf(fp, "/*\n");
	fprintf(fp, " * %s.dtso - Device Tree overlay for %s\n",
		info->device_name, info->device_name);
	fprintf(fp, " * Auto-generated by: kdality dt-gen\n");
	fprintf(fp, " * Source: %s.kdh\n", info->device_name);
	fprintf(fp, " */\n\n");
	fprintf(fp, "/dts-v1/;\n");
	fprintf(fp, "/plugin/;\n\n");

	fprintf(fp, "/ {\n");
	fprintf(fp, "    compatible = \"simple-bus\";\n");
	fprintf(fp, "    #address-cells = <2>;\n");
	fprintf(fp, "    #size-cells = <2>;\n\n");

	/* Fragment */
	fprintf(fp, "    fragment@0 {\n");
	fprintf(fp, "        target-path = \"/\";\n");
	fprintf(fp, "        __overlay__ {\n");
	fprintf(fp, "            %s@%lx {\n", info->device_name, base_addr);
	fprintf(fp, "                compatible = \"%s\";\n", info->compatible);
	fprintf(fp, "                reg = <0x0 0x%lx 0x0 0x%lx>;\n", base_addr,
		reg_size);

	/* Status */
	fprintf(fp, "                status = \"okay\";\n");

	/* Interrupts if signals contain irq */
	if (irq_num >= 0) {
		fprintf(fp,
			"                interrupts = <0x0 %d 0x4>; "
			"/* SPI, level high */\n",
			irq_num);
	}

	/* Register comments */
	if (info->nregs > 0) {
		fprintf(fp, "\n                /* Register map from %s.kdh:\n",
			info->device_name);
		for (int i = 0; i < info->nregs; i++) {
			fprintf(fp, "                 *   %s @ 0x%lx\n",
				info->regs[i].name, info->regs[i].offset);
		}
		fprintf(fp, "                 */\n");
	}

	fprintf(fp, "            };\n");
	fprintf(fp, "        };\n");
	fprintf(fp, "    };\n");
	fprintf(fp, "};\n");

	fclose(fp);
	return 0;
}

/* ── help ────────────────────────────────────────────────────────── */

static void dtgen_help(void)
{
	fprintf(stderr,
		"Usage: kdality dt-gen <file.kdh> [options]\n\n"
		"Generate a Device Tree Source overlay from a .kdh device header.\n\n"
		"Options:\n"
		"  -o <dir>           output directory (default: .)\n"
		"  --base-addr <hex>  base MMIO address (default: 0x10000000)\n"
		"  --irq <num>        interrupt number (default: none)\n"
		"  -h, --help         show this help\n\n"
		"Example:\n"
		"  kdality dt-gen myled.kdh -o dtbs/ --base-addr 0x40010000 --irq 32\n\n"
		"Output:\n"
		"  dtbs/myled.dtso    Device Tree Source overlay\n\n"
		"Compile the overlay:\n"
		"  dtc -I dts -O dtb -o myled.dtbo dtbs/myled.dtso\n");
}

/* ── main entry ──────────────────────────────────────────────────── */

int kdality_dtgen(int argc, char *const argv[])
{
	const char *input = NULL;
	const char *outdir = ".";
	unsigned long base_addr = 0x10000000;
	int irq_num = -1;
	struct kdh_info info;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			dtgen_help();
			return 0;
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			outdir = argv[++i];
		} else if (strcmp(argv[i], "--base-addr") == 0 &&
			   i + 1 < argc) {
			base_addr = strtoul(argv[++i], NULL, 0);
		} else if (strcmp(argv[i], "--irq") == 0 && i + 1 < argc) {
			irq_num = atoi(argv[++i]);
		} else if (argv[i][0] != '-') {
			input = argv[i];
		}
	}

	if (!input) {
		fprintf(stderr, "dt-gen: no input .kdh file\n\n");
		dtgen_help();
		return 1;
	}

	if (parse_kdh(input, &info) != 0)
		return 1;

	if (generate_dtso(&info, outdir, base_addr, irq_num) != 0)
		return 1;

	printf("Generated %s/%s.dtso\n", outdir, info.device_name);
	printf("  compatible: %s\n", info.compatible);
	printf("  base addr:  0x%lx\n", base_addr);
	printf("  reg size:   0x%lx\n",
	       info.nregs > 0 ? (info.regs[info.nregs - 1].offset + 4 < 0x100 ?
					 0x100UL :
					 info.regs[info.nregs - 1].offset + 4) :
				0x100UL);
	if (irq_num >= 0)
		printf("  irq:        %d\n", irq_num);
	printf("\nCompile with: dtc -I dts -O dtb -o %s.dtbo %s/%s.dtso\n",
	       info.device_name, outdir, info.device_name);

	return 0;
}
