/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * KDAL Compiler — codegen.h
 * Public API consumed by kdality's compile subcommand.
 */

#ifndef KDALC_CODEGEN_H
#define KDALC_CODEGEN_H

#include "token.h"
#include "ast.h"

/* ── Codegen context ─────────────────────────────────────────────── */

typedef struct {
	const char *output_dir; /* directory to write .c and Makefile.kbuild */
	const char *kernel_dir; /* KERNEL_DIR for generated Makefile.kbuild */
	const char *cross_compile; /* CROSS_COMPILE prefix, may be NULL */
	int verbose;
} kdal_codegen_opts_t;

/* ── Entry points ────────────────────────────────────────────────── */

/*
 * kdal_compile_file   — full pipeline: lex → parse → sema → codegen
 *
 * @src_path   path to .kdc file
 * @opts       codegen options
 * @returns    0 on success, negative on error (diagnostics to stderr)
 */
int kdal_compile_file(const char *src_path, const kdal_codegen_opts_t *opts);

/*
 * kdal_generate        — codegen stage only, from a validated annotated AST
 *
 * @file_node  result of kdal_parse + kdal_sema
 * @src_path   original file path (for error messages)
 * @opts       codegen options
 * @returns    0 on success
 */
int kdal_generate(const kdal_file_node_t *file_node, const char *src_path,
		  const kdal_codegen_opts_t *opts);

/* ── Lower-level stage entry points (exposed for testing) ─────────── */

/*
 * kdal_lex     — tokenise src_len bytes of src into *out_tokens.
 *               Caller owns the returned array (arena-allocated).
 * @returns     number of tokens, or negative on error
 */
int kdal_lex(kdal_arena_t *arena, const char *src, size_t src_len,
	     kdal_token_t **out_tokens, const char *filename);

/*
 * kdal_parse   — produce an AST from a token stream.
 * @returns     root file node, or NULL on parse error
 */
kdal_file_node_t *kdal_parse(kdal_arena_t *arena, const kdal_token_t *tokens,
			     int ntokens, const char *filename);

/*
 * kdal_sema    — semantic analysis pass; fills in type annotations.
 * @returns     0 on success, negative on first error
 */
int kdal_sema(kdal_file_node_t *file, const char *filename);

/* ── Diagnostic helpers ──────────────────────────────────────────── */

void kdal_error(const char *filename, int line, int col, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

void kdal_warn(const char *filename, int line, int col, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

void kdal_note(const char *filename, int line, int col, const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

#endif /* KDALC_CODEGEN_H */
