// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler — main.c
 * Standalone `kdalc` entry point (used for testing the compiler directly).
 *
 * Usage: kdalc [options] <file.kdc>
 *
 * Options:
 *   -o <dir>       output directory (default: .)
 *   -K <dir>       kernel build directory (KDIR)
 *   -x <prefix>    CROSS_COMPILE prefix
 *   -v             verbose
 *   -h             print help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/token.h"
#include "include/ast.h"
#include "include/codegen.h"

#ifndef KDAL_VERSION_STRING
#define KDAL_VERSION_STRING "0.1.0-dev"
#endif

static void usage(const char *prog)
{
	fprintf(stderr,
		"KDAL compiler %s\n"
		"Usage: %s [options] <file.kdc>\n\n"
		"Options:\n"
		"  -o <dir>    output directory (default: .)\n"
		"  -K <dir>    kernel build directory\n"
		"  -x <pfx>    CROSS_COMPILE prefix (e.g. aarch64-linux-gnu-)\n"
		"  -v          verbose output\n"
		"  -h          show this help\n",
		KDAL_VERSION_STRING, prog);
}

int main(int argc, char *argv[])
{
	kdal_codegen_opts_t opts = {
		.output_dir = ".",
		.kernel_dir = NULL,
		.cross_compile = NULL,
		.verbose = 0,
	};

	int opt;
	while ((opt = getopt(argc, argv, "o:K:x:vh")) != -1) {
		switch (opt) {
		case 'o':
			opts.output_dir = optarg;
			break;
		case 'K':
			opts.kernel_dir = optarg;
			break;
		case 'x':
			opts.cross_compile = optarg;
			break;
		case 'v':
			opts.verbose = 1;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "%s: no input file\n", argv[0]);
		usage(argv[0]);
		return 1;
	}

	const char *src_path = argv[optind];

	/* Basic extension check */
	const char *ext = strrchr(src_path, '.');
	if (!ext || (strcmp(ext, ".kdc") != 0 && strcmp(ext, ".kdh") != 0)) {
		fprintf(stderr,
			"%s: warning: expected .kdc or .kdh extension\n",
			argv[0]);
	}

	int rc = kdal_compile_file(src_path, &opts);
	if (rc != 0) {
		fprintf(stderr, "%s: compilation failed\n", argv[0]);
		return 1;
	}

	if (opts.verbose)
		fprintf(stderr, "kdalc: done.\n");

	return 0;
}
