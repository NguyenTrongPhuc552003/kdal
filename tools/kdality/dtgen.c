/*
 * dtgen.c - kdality dt-gen subcommand.
 *
 * Generates a Device Tree Source overlay (.dtso) from a .kdh device header.
 *
 *   kdality dt-gen <file.kdh> [-o <outdir>] [--base-addr <addr>] [--irq <num>]
 *
 * This replaces manual DT overlay writing - newcomers can describe their
 * device in .kdh and auto-generate the DT binding. The .kdh is read by the
 * compiler's parser, so the full syntax (device class NAME, registers) and
 * the simplified one (device NAME, register_map) both work
 * (HAVEN's docs/methodology/stage2/20261114-kdal-dtgen).
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../compiler/include/codegen.h"

/*
 * A whole string as an unsigned number in base (0: C prefixes), within
 * max; 0 on success. strtoul's errors are checked, which sscanf and atoi
 * cannot report.
 */
static int parse_number(const char *s, int base, unsigned long *out,
			unsigned long max)
{
	char *end;
	unsigned long v;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-' || *s == '\0')
		return -1;
	errno = 0;
	v = strtoul(s, &end, base);
	if (errno || end == s || v > max)
		return -1;
	while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
		end++;
	if (*end != '\0')
		return -1;
	*out = v;
	return 0;
}

/* A device name usable as a file name: letters, digits, '_' and '-'. */
static int is_safe_name(const char *s)
{
	if (!s || !*s)
		return 0;
	for (; *s; s++) {
		if (!(('a' <= *s && *s <= 'z') || ('A' <= *s && *s <= 'Z') ||
		      ('0' <= *s && *s <= '9') || *s == '_' || *s == '-'))
			return 0;
	}
	return 1;
}

/*
 * Create outdir/filename (one path component, so nothing outside outdir)
 * and open it for writing; full_path is set first, so a caller can name
 * the file whatever fails.
 */
static FILE *open_output_in_dir(const char *outdir, const char *filename,
				char *full_path, size_t full_path_size)
{
	int dirfd, fd;
	FILE *fp;

	if (snprintf(full_path, full_path_size, "%s/%s", outdir, filename) >=
	    (int)full_path_size) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	if (!*filename || filename[0] == '.' || strchr(filename, '/')) {
		errno = EINVAL;
		return NULL;
	}

	dirfd = open(outdir, O_RDONLY | O_DIRECTORY);
	if (dirfd < 0)
		return NULL;

	fd = openat(dirfd, filename, O_WRONLY | O_CREAT | O_TRUNC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	(void)close(dirfd);
	if (fd < 0)
		return NULL;

	fp = fdopen(fd, "w");
	if (!fp) {
		(void)close(fd);
		return NULL;
	}

	return fp;
}

/* ── From the AST ────────────────────────────────────────────────── */

/* The registers block's declarations that have an offset ("@" or the
 * simplified form's number), one after the other. */
static const kdal_reg_decl_node_t *next_reg(const kdal_ast_t *r)
{
	for (; r; r = r->next) {
		const kdal_reg_decl_node_t *reg =
			(const kdal_reg_decl_node_t *)r;

		if (r->type == KDAL_NODE_REGISTER_DECL &&
		    reg->offset != UINT64_MAX)
			return reg;
	}
	return NULL;
}

static const kdal_reg_decl_node_t *first_reg(const kdal_device_node_t *dev)
{
	return dev->registers ? next_reg(dev->registers->child) : NULL;
}

/* The overlay's reg size: the highest register offset plus 4, at least
 * 0x100. One function, so the file and the summary agree. */
static unsigned long long reg_size(const kdal_device_node_t *dev)
{
	unsigned long long size = 0x100;

	for (const kdal_reg_decl_node_t *reg = first_reg(dev); reg;
	     reg = next_reg(reg->base.next)) {
		if (reg->offset + 4 > size)
			size = reg->offset + 4;
	}
	return size;
}

/* The compatible string: the .kdh's, without the lexer's quotes, or
 * "kdal,<name>". */
static void compatible_of(const kdal_device_node_t *dev, char *out, size_t size)
{
	const char *c = dev->compatible;
	size_t len = c ? strlen(c) : 0;

	if (len >= 2 && c[0] == '"' && c[len - 1] == '"')
		(void)snprintf(out, size, "%.*s", (int)(len - 2), c + 1);
	else
		(void)snprintf(out, size, "kdal,%s", dev->name);
}

/* ── DT overlay generation ───────────────────────────────────────── */

struct dtgen_opts {
	const char *input; /* the .kdh */
	const char *outdir;
	unsigned long base_addr;
	int irq; /* -1: none */
};

static int generate_dtso(const kdal_device_node_t *dev,
			 const struct dtgen_opts *o, char *path,
			 size_t path_size)
{
	const unsigned long base_addr = o->base_addr;
	const int irq_num = o->irq;
	char filename[NAME_MAX + 1];
	char compatible[256];
	FILE *fp;
	int failed;

	if (!is_safe_name(dev->name)) {
		(void)fprintf(stderr, "dt-gen: invalid device name '%s'\n",
			      dev->name ? dev->name : "");
		return -1;
	}
	if (snprintf(filename, sizeof(filename), "%s.dtso", dev->name) >=
	    (int)sizeof(filename)) {
		(void)fprintf(stderr, "dt-gen: output filename too long\n");
		return -1;
	}
	fp = open_output_in_dir(o->outdir, filename, path, path_size);
	if (!fp) {
		(void)fprintf(stderr, "dt-gen: cannot create '%s': %s\n", path,
			      strerror(errno));
		return -1;
	}
	compatible_of(dev, compatible, sizeof(compatible));

	/* DT overlay header */
	(void)fprintf(fp, "/*\n");
	(void)fprintf(fp, " * %s.dtso - Device Tree overlay for %s\n",
		      dev->name, dev->name);
	(void)fprintf(fp, " * Auto-generated by: kdality dt-gen\n");
	(void)fprintf(fp, " * Source: %s.kdh\n", dev->name);
	(void)fprintf(fp, " */\n\n");
	(void)fprintf(fp, "/dts-v1/;\n");
	(void)fprintf(fp, "/plugin/;\n\n");

	(void)fprintf(fp, "/ {\n");
	(void)fprintf(fp, "    compatible = \"simple-bus\";\n");
	(void)fprintf(fp, "    #address-cells = <2>;\n");
	(void)fprintf(fp, "    #size-cells = <2>;\n\n");

	/* Fragment */
	(void)fprintf(fp, "    fragment@0 {\n");
	(void)fprintf(fp, "        target-path = \"/\";\n");
	(void)fprintf(fp, "        __overlay__ {\n");
	(void)fprintf(fp, "            %s@%lx {\n", dev->name, base_addr);
	(void)fprintf(fp, "                compatible = \"%s\";\n", compatible);
	(void)fprintf(fp, "                reg = <0x0 0x%lx 0x0 0x%llx>;\n",
		      base_addr, reg_size(dev));

	/* Status */
	(void)fprintf(fp, "                status = \"okay\";\n");

	/* Interrupts if signals contain irq */
	if (irq_num >= 0) {
		(void)fprintf(fp,
			      "                interrupts = <0x0 %d 0x4>; "
			      "/* SPI, level high */\n",
			      irq_num);
	}

	/* Register comments */
	if (first_reg(dev)) {
		(void)fprintf(
			fp, "\n                /* Register map from %s.kdh:\n",
			dev->name);
		for (const kdal_reg_decl_node_t *reg = first_reg(dev); reg;
		     reg = next_reg(reg->base.next))
			(void)fprintf(fp, "                 *   %s @ 0x%llx\n",
				      reg->name,
				      (unsigned long long)reg->offset);
		(void)fprintf(fp, "                 */\n");
	}

	(void)fprintf(fp, "            };\n");
	(void)fprintf(fp, "        };\n");
	(void)fprintf(fp, "    };\n");
	(void)fprintf(fp, "};\n");

	/* a failed write sets the stream's error flag; fclose reports the
	 * last flush */
	failed = ferror(fp);
	if (fclose(fp) != 0 || failed) {
		(void)fprintf(stderr, "dt-gen: cannot write '%s': %s\n", path,
			      strerror(errno));
		return -1;
	}
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

/* The options into *o: 0 to go on, 1 after the help, -1 on an error.
 * argv[0] is the subcommand's name ("dt-gen"), not an input. */
static int parse_args(int argc, char *const argv[], struct dtgen_opts *o)
{
	int argi = 1;

	while (argi < argc) {
		const char *arg = argv[argi++];

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			dtgen_help();
			return 1;
		} else if (strcmp(arg, "-o") == 0) {
			if (argi >= argc) {
				fprintf(stderr,
					"dt-gen: missing value for '-o'\n");
				return -1;
			}
			o->outdir = argv[argi++];
		} else if (strcmp(arg, "--base-addr") == 0) {
			if (argi >= argc) {
				fprintf(stderr,
					"dt-gen: missing value for '--base-addr'\n");
				return -1;
			}
			if (parse_number(argv[argi++], 0, &o->base_addr,
					 ULONG_MAX)) {
				(void)fprintf(
					stderr,
					"dt-gen: invalid value for '--base-addr'\n");
				return -1;
			}
		} else if (strcmp(arg, "--irq") == 0) {
			if (argi >= argc) {
				fprintf(stderr,
					"dt-gen: missing value for '--irq'\n");
				return -1;
			}
			unsigned long irq;
			if (parse_number(argv[argi++], 10, &irq, INT_MAX)) {
				(void)fprintf(
					stderr,
					"dt-gen: invalid value for '--irq'\n");
				return -1;
			}
			o->irq = (int)irq;
		} else if (arg[0] != '-') {
			o->input = arg;
		}
	}

	return 0;
}

static void print_summary(const kdal_device_node_t *dev,
			  const struct dtgen_opts *o, const char *path)
{
	char compatible[256];

	compatible_of(dev, compatible, sizeof(compatible));
	printf("Generated %s\n", path);
	printf("  compatible: %s\n", compatible);
	printf("  base addr:  0x%lx\n", o->base_addr);
	printf("  reg size:   0x%llx\n", reg_size(dev));
	if (o->irq >= 0)
		printf("  irq:        %d\n", o->irq);
	printf("\nCompile with: dtc -I dts -O dtb -o %s.dtbo %s\n", dev->name,
	       path);
}

int kdality_dtgen(int argc, char *const argv[])
{
	struct dtgen_opts o = { NULL, ".", 0x10000000, -1 };
	char resolved_input[PATH_MAX];
	char resolved_outdir[PATH_MAX];
	char path[PATH_MAX];
	kdal_arena_t *arena = NULL;
	const kdal_file_node_t *file;
	const kdal_device_node_t *dev;
	int ret = parse_args(argc, argv, &o);

	if (ret)
		return ret < 0 ? 1 : 0;
	if (!o.input) {
		fprintf(stderr, "dt-gen: no input .kdh file\n\n");
		dtgen_help();
		return 1;
	}

	if (!realpath(o.input, resolved_input)) {
		(void)fprintf(stderr, "dt-gen: cannot resolve '%s': %s\n",
			      o.input, strerror(errno));
		return 1;
	}

	if (!realpath(o.outdir, resolved_outdir)) {
		fprintf(stderr,
			"dt-gen: cannot resolve output directory '%s': %s\n",
			o.outdir, strerror(errno));
		return 1;
	}
	o.outdir = resolved_outdir;

	file = kdal_parse_file("dt-gen", resolved_input, &arena);
	if (!file)
		return 1;
	dev = (const kdal_device_node_t *)file->device;
	if (!dev)
		(void)fprintf(stderr,
			      "dt-gen: no 'device' declaration in '%s'\n",
			      resolved_input);
	ret = dev && generate_dtso(dev, &o, path, sizeof(path)) == 0 ? 0 : 1;
	if (!ret)
		print_summary(dev, &o, path);
	kdal_arena_free(arena);
	return ret;
}
