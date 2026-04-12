/*
 * init.c - kdality init subcommand.
 *
 * Scaffolds a new KDAL driver project:
 *   kdality init <name> [--class <class>] [--vendor <vendor>]
 *
 * Creates a directory with:
 *   <name>.kdh          - device header
 *   <name>_driver.kdc   - driver stub
 *   Makefile             - build orchestration
 *   README.md            - project documentation
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "templates.h"

/* ── helpers ─────────────────────────────────────────────────────── */

static int write_file(const char *dir, const char *filename,
		      const char *content)
{
	char path[512];
	FILE *fp;

	snprintf(path, sizeof(path), "%s/%s", dir, filename);
	fp = fopen(path, "w");
	if (!fp) {
		fprintf(stderr, "cannot create %s: %s\n", path,
			strerror(errno));
		return -1;
	}
	fputs(content, fp);
	fclose(fp);
	return 0;
}

static const char *valid_classes[] = { "uart", "i2c", "spi", "gpio",
				       "gpu",  "npu", NULL };

static int is_valid_class(const char *cls)
{
	for (int i = 0; valid_classes[i]; i++) {
		if (strcmp(cls, valid_classes[i]) == 0)
			return 1;
	}
	return 0;
}

static int is_valid_name(const char *name)
{
	if (!name || !name[0])
		return 0;
	/* Must start with letter, contain only alnum and underscore */
	if (!((name[0] >= 'a' && name[0] <= 'z') ||
	      (name[0] >= 'A' && name[0] <= 'Z')))
		return 0;
	for (int i = 1; name[i]; i++) {
		char c = name[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '_'))
			return 0;
	}
	return 1;
}

/* ── init_help ───────────────────────────────────────────────────── */

static void init_help(void)
{
	fprintf(stderr,
		"Usage: kdality init <name> [options]\n\n"
		"Scaffold a new KDAL driver project.\n\n"
		"Options:\n"
		"  --class <class>    device class (uart, i2c, spi, gpio, gpu, npu)\n"
		"                     default: gpio\n"
		"  --vendor <vendor>  vendor prefix for DT compatible string\n"
		"                     default: example\n"
		"  -h, --help         show this help\n\n"
		"Example:\n"
		"  kdality init myled --class gpio --vendor acme\n\n"
		"Creates myled/ with:\n"
		"  myled.kdh          device header\n"
		"  myled_driver.kdc   driver stub\n"
		"  Makefile           build rules\n"
		"  README.md          documentation\n");
}

/* ── main entry ──────────────────────────────────────────────────── */

int kdality_init(int argc, char *const argv[])
{
	const char *name = NULL;
	const char *class = "gpio";
	const char *vendor = "example";
	char buf[4096];
	char filename[256];

	/* Parse args */
	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			init_help();
			return 0;
		} else if (strcmp(argv[i], "--class") == 0 && i + 1 < argc) {
			class = argv[++i];
		} else if (strcmp(argv[i], "--vendor") == 0 && i + 1 < argc) {
			vendor = argv[++i];
		} else if (argv[i][0] != '-') {
			name = argv[i];
		}
	}

	if (!name) {
		fprintf(stderr, "kdality init: project name required\n\n");
		init_help();
		return 1;
	}

	if (!is_valid_name(name)) {
		fprintf(stderr,
			"kdality init: invalid name '%s' "
			"(must start with a letter, contain only [a-zA-Z0-9_])\n",
			name);
		return 1;
	}

	if (!is_valid_class(class)) {
		fprintf(stderr,
			"kdality init: unknown class '%s'\n"
			"Valid classes: uart, i2c, spi, gpio, gpu, npu\n",
			class);
		return 1;
	}

	/* Create project directory */
	if (mkdir(name, 0755) != 0 && errno != EEXIST) {
		fprintf(stderr,
			"kdality init: cannot create directory '%s': %s\n",
			name, strerror(errno));
		return 1;
	}

	/* Write .kdh */
	snprintf(buf, sizeof(buf), tmpl_kdh, name, name, name, name, class,
		 vendor, name);
	snprintf(filename, sizeof(filename), "%s.kdh", name);
	if (write_file(name, filename, buf) != 0)
		return 1;

	/* Write .kdc */
	snprintf(buf, sizeof(buf), tmpl_kdc, name, name, name, name, name, name,
		 name, name);
	snprintf(filename, sizeof(filename), "%s_driver.kdc", name);
	if (write_file(name, filename, buf) != 0)
		return 1;

	/* Write Makefile */
	snprintf(buf, sizeof(buf), tmpl_makefile, name, name);
	if (write_file(name, "Makefile", buf) != 0)
		return 1;

	/* Write README.md */
	snprintf(buf, sizeof(buf), tmpl_readme, name, name, name, name, name);
	if (write_file(name, "README.md", buf) != 0)
		return 1;

	printf("Created KDAL project '%s/' with:\n", name);
	printf("  %s/%s.kdh             device header\n", name, name);
	printf("  %s/%s_driver.kdc      driver stub\n", name, name);
	printf("  %s/Makefile            build rules\n", name);
	printf("  %s/README.md           documentation\n", name);
	printf("\nNext steps:\n");
	printf("  cd %s\n", name);
	printf("  # Edit %s.kdh to describe your hardware\n", name);
	printf("  # Edit %s_driver.kdc to add driver logic\n", name);
	printf("  make compile           # generate kernel C code\n");
	printf("  make module KDIR=...   # build the .ko module\n");

	return 0;
}
