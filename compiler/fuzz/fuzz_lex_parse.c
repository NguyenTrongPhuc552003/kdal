// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler - libFuzzer target for the lexer, parser and checks.
 *
 * Any bytes go through kdal_lex, kdal_parse and kdal_sema, as kdalc does
 * with a .kdc or .kdh file. Built only with the CMake option KDAL_FUZZ
 * (clang, with AddressSanitizer and UBSan); the quality gate runs it in
 * KDAL's weekly CI workflow, seeded with examples/kdc_hello/.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../include/ast.h"
#include "../include/codegen.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	kdal_arena_t *arena = kdal_arena_new(65536);
	char *src;

	if (!arena)
		return 0;
	src = kdal_arena_alloc(arena, size + 1);
	if (src) {
		kdal_token_t *tokens = NULL;
		int ntokens;

		memcpy(src, data, size);
		src[size] = '\0';
		ntokens = kdal_lex(arena, src, size, &tokens, "fuzz.kdc");
		if (ntokens > 0) {
			kdal_file_node_t *file =
				kdal_parse(arena, tokens, ntokens, "fuzz.kdc");
			if (file)
				(void)kdal_sema(file, "fuzz.kdc");
		}
	}
	kdal_arena_free(arena);
	return 0;
}
