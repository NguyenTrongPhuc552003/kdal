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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/* libkdalc public API */
#include "../../compiler/include/codegen.h"
#include "paths.h"

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
	char resolved_input[PATH_MAX];
	int argi = 1; /* argv[0] is the subcommand's name */

	while (argi < argc) {
		const char *arg = argv[argi++];

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			compile_help();
			return 0;
		} else if (strcmp(arg, "-v") == 0) {
			opts.verbose = 1;
		} else if (strcmp(arg, "-o") == 0) {
			const char *out_dir;
			if (argi >= argc) {
				fprintf(stderr,
					"kdality compile: missing value for '-o'\n");
				return 1;
			}
			out_dir = argv[argi++];
			if (!kdality_path_ok(out_dir)) {
				fprintf(stderr,
					"kdality compile: invalid output directory '%s'\n",
					out_dir);
				return 1;
			}
			opts.output_dir = out_dir;
		} else if (strcmp(arg, "-K") == 0) {
			if (argi >= argc) {
				fprintf(stderr,
					"kdality compile: missing value for '-K'\n");
				return 1;
			}
			opts.kernel_dir = argv[argi++];
		} else if (strcmp(arg, "-x") == 0) {
			if (argi >= argc) {
				fprintf(stderr,
					"kdality compile: missing value for '-x'\n");
				return 1;
			}
			opts.cross_compile = argv[argi++];
		} else if (arg[0] != '-') {
			if (input) {
				(void)fprintf(
					stderr,
					"kdality compile: more than one input ('%s', '%s')\n",
					input, arg);
				compile_help();
				return 1;
			}
			input = arg;
		} else {
			fprintf(stderr,
				"kdality compile: unknown option '%s'\n", arg);
			compile_help();
			return 1;
		}
	}

	if (!input) {
		fprintf(stderr, "kdality compile: no input file\n");
		compile_help();
		return 1;
	}

	if (!kdality_path_ok(input) || !realpath(input, resolved_input)) {
		fprintf(stderr, "kdality compile: cannot resolve '%s'\n",
			input);
		return 1;
	}

	if (kdality_ensure_dir("kdality compile", opts.output_dir) != 0)
		return 1;

	int rc = kdal_compile_file(resolved_input, &opts);
	if (rc != 0) {
		fprintf(stderr, "kdality compile: failed to compile '%s'\n",
			resolved_input);
		return 1;
	}

	if (opts.verbose)
		fprintf(stderr, "kdality compile: done\n");

	return 0;
}
