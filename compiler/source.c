// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL compiler: reading a source file into memory.
 *
 * One reader for kdalc and the kdality tools (lint, simulate, dt-gen),
 * which had three copies of it. errno is kept from the failing call, so
 * callers can report it with strerror(errno). kdal_parse_file() is the
 * read, lex and parse the tools each repeated.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "include/ast.h"
#include "include/codegen.h"

char *kdal_read_file(const char *path, size_t *out_len)
{
	FILE *fp = fopen(path, "r");
	char *buf = NULL;
	long size;
	size_t n;
	int err;

	if (!fp)
		return NULL;
	if (fseek(fp, 0, SEEK_END) != 0)
		goto fail;
	size = ftell(fp);
	/* ftell fails with -1; a size that cannot take the NUL is refused */
	if (size < 0 || (unsigned long)size >= SIZE_MAX ||
	    fseek(fp, 0, SEEK_SET) != 0)
		goto fail;
	/* zero-filled, so the text ends in NUL whatever fread returns */
	buf = calloc((size_t)size + 1, 1);
	if (!buf)
		goto fail;
	n = fread(buf, 1, (size_t)size, fp);
	(void)fclose(fp);
	if (out_len)
		*out_len = n;
	return buf;
fail:
	err = errno;
	free(buf);
	(void)fclose(fp);
	errno = err ? err : EIO;
	return NULL;
}

char *kdal_read_file_arena(kdal_arena_t *arena, const char *path,
			   size_t *out_len)
{
	size_t len;
	char *src = kdal_read_file(path, &len);
	char *kept;

	if (!src)
		return NULL;
	kept = arena ? kdal_arena_alloc(arena, len + 1) : NULL;
	if (kept)
		memcpy(kept, src, len + 1);
	free(src);
	if (!kept) {
		errno = ENOMEM;
		return NULL;
	}
	if (out_len)
		*out_len = len;
	return kept;
}

kdal_file_node_t *kdal_parse_file(const char *who, const char *path,
				  kdal_arena_t **out_arena)
{
	size_t len;
	kdal_arena_t *arena = kdal_arena_new(65536);
	/* in the arena: the tokens point into the text */
	char *src = arena ? kdal_read_file_arena(arena, path, &len) : NULL;
	kdal_token_t *tokens = NULL;
	kdal_file_node_t *file = NULL;
	int ntokens;

	if (!src) {
		(void)fprintf(stderr, "%s: cannot open '%s': %s\n", who, path,
			      strerror(arena ? errno : ENOMEM));
		kdal_arena_free(arena);
		return NULL;
	}
	/* the lexer and the parser report their own errors */
	ntokens = kdal_lex(arena, src, len, &tokens, path);
	if (ntokens >= 0)
		file = kdal_parse(arena, tokens, ntokens, path);
	if (!file) {
		kdal_arena_free(arena);
		return NULL;
	}
	*out_arena = arena;
	return file;
}
