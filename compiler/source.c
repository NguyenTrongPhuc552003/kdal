// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL compiler: reading a source file into memory.
 *
 * One reader for kdalc and the kdality tools (lint, simulate), which had
 * three copies of it. errno is kept from the failing call, so callers can
 * report it with strerror(errno).
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
	buf = malloc((size_t)size + 1);
	if (!buf)
		goto fail;
	n = fread(buf, 1, (size_t)size, fp);
	if (n >
	    (size_t)size) /* fread returns at most size; stated, not assumed */
		n = (size_t)size;
	buf[n] = '\0';
	fclose(fp);
	if (out_len)
		*out_len = n;
	return buf;
fail:
	err = errno;
	free(buf);
	fclose(fp);
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
