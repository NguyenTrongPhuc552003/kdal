// SPDX-License-Identifier: GPL-2.0-only
/*
 * kdality - compile subcommand
 * Translates a .kdc driver file into a kernel C module.
 *
 * Invoked as: kdality compile [options] <file.kdc>
 *
 * This file links against libkdalc.a (compiler/libkdalc.a).
 * It provides the same interface as the standalone `kdalc` binary,
 * but integrated into the unified kdality toolchain.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* libkdalc public API */
#include "../../compiler/include/codegen.h"

static bool is_safe_output_dir(const char *dir)
{
	const char *p;

	if (!dir || dir[0] == '\0')
		return false;

	/* Disallow absolute paths */
	if (dir[0] == '/' || dir[0] == '\\')
		return false;

	/* Disallow any ".." path segment */
	p = dir;
	while (*p) {
		if (p[0] == '.' && p[1] == '.' &&
		    (p == dir || p[-1] == '/' || p[-1] == '\\') &&
		    (p[2] == '\0' || p[2] == '/' || p[2] == '\\'))
			return false;
		p++;
	}

	return true;
}

static void compile_help(void)
{
	fprintf(stderr,
		"Usage: kdality compile [options] <file.kdc>\n\n"
		"Options:\n"
		"  -o <dir>    output directory (default: .)\n"
		"  -K <dir>    kernel build directory (KDIR)\n"
		"  -x <pfx>    CROSS_COMPILE prefix\n"
		"  -v          verbose\n"
		"  -h          this help\n\n"
		"The compiler translates a KDAL driver (.kdc) into:\n"
		"  <driver>.c            kernel C module source\n"
		"  Makefile.kbuild       kbuild fragment (obj-m)\n"
		"Then build with: make -f Makefile.kbuild KDIR=<kernel-dir>\n");
}

int kdality_compile(int argc, char *const argv[])
{
	kdal_codegen_opts_t opts = {
		.output_dir = ".",
		.kernel_dir = NULL,
		.cross_compile = NULL,
		.verbose = 0,
	};

	const char *input = NULL;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			compile_help();
			return 0;
		} else if (strcmp(argv[i], "-v") == 0) {
			opts.verbose = 1;
		} else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			const char *out_dir = argv[++i];
			if (!is_safe_output_dir(out_dir)) {
				fprintf(stderr,
					"kdality compile: invalid output directory '%s'\n",
					out_dir);
				return 1;
			}
			opts.output_dir = out_dir;
		} else if (strcmp(argv[i], "-K") == 0 && i + 1 < argc) {
			opts.kernel_dir = argv[++i];
		} else if (strcmp(argv[i], "-x") == 0 && i + 1 < argc) {
			opts.cross_compile = argv[++i];
		} else if (argv[i][0] != '-') {
			input = argv[i];
		} else {
			fprintf(stderr,
				"kdality compile: unknown option '%s'\n",
				argv[i]);
			return 1;
		}
	}

	if (!input) {
		fprintf(stderr, "kdality compile: no input file\n");
		compile_help();
		return 1;
	}

	int rc = kdal_compile_file(input, &opts);
	if (rc != 0) {
		fprintf(stderr, "kdality compile: failed to compile '%s'\n",
			input);
		return 1;
	}

	if (opts.verbose)
		fprintf(stderr, "kdality compile: done\n");

	return 0;
}
