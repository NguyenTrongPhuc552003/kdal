// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler — lexer.c
 * Hand-written tokeniser for .kdh and .kdc source files.
 *
 * Produces a flat array of kdal_token_t from a source buffer.
 * The lexer is single-pass, O(n) in source length.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/token.h"
#include "include/ast.h"
#include "include/codegen.h"

/* ── Keyword table ───────────────────────────────────────────────── */

typedef struct {
	const char *word;
	kdal_tok_t tok;
} kw_entry_t;

static const kw_entry_t kw_table[] = {
	{ "kdal_version", TOK_KW_KDAL_VERSION },
	{ "import", TOK_KW_IMPORT },
	{ "as", TOK_KW_AS },
	{ "device", TOK_KW_DEVICE },
	{ "class", TOK_KW_CLASS },
	{ "registers", TOK_KW_REGISTERS },
	{ "signals", TOK_KW_SIGNALS },
	{ "capabilities", TOK_KW_CAPABILITIES },
	{ "power", TOK_KW_POWER },
	{ "config", TOK_KW_CONFIG },
	{ "bitfield", TOK_KW_BITFIELD },
	{ "ro", TOK_KW_RO },
	{ "wo", TOK_KW_WO },
	{ "rw", TOK_KW_RW },
	{ "rc", TOK_KW_RC },
	{ "in", TOK_KW_IN },
	{ "edge", TOK_KW_EDGE },
	{ "level", TOK_KW_LEVEL },
	{ "rising", TOK_KW_RISING },
	{ "falling", TOK_KW_FALLING },
	{ "any", TOK_KW_ANY },
	{ "high", TOK_KW_HIGH },
	{ "low", TOK_KW_LOW },
	{ "completion", TOK_KW_COMPLETION },
	{ "allowed", TOK_KW_ALLOWED },
	{ "forbidden", TOK_KW_FORBIDDEN },
	{ "default", TOK_KW_DEFAULT },
	{ "on", TOK_KW_ON },
	{ "off", TOK_KW_OFF },
	{ "suspend", TOK_KW_SUSPEND },
	{ "backend", TOK_KW_BACKEND },
	{ "driver", TOK_KW_DRIVER },
	{ "for", TOK_KW_FOR },
	{ "probe", TOK_KW_PROBE },
	{ "remove", TOK_KW_REMOVE },
	{ "read", TOK_KW_READ },
	{ "write", TOK_KW_WRITE },
	{ "signal", TOK_KW_SIGNAL },
	{ "timeout", TOK_KW_TIMEOUT },
	{ "emit", TOK_KW_EMIT },
	{ "wait", TOK_KW_WAIT },
	{ "arm", TOK_KW_ARM },
	{ "cancel", TOK_KW_CANCEL },
	{ "let", TOK_KW_LET },
	{ "return", TOK_KW_RETURN },
	{ "log", TOK_KW_LOG },
	{ "if", TOK_KW_IF },
	{ "elif", TOK_KW_ELIF },
	{ "else", TOK_KW_ELSE },
	{ "true", TOK_BOOL_TRUE },
	{ "false", TOK_BOOL_FALSE },
	{ "u8", TOK_KW_U8 },
	{ "u16", TOK_KW_U16 },
	{ "u32", TOK_KW_U32 },
	{ "u64", TOK_KW_U64 },
	{ "i8", TOK_KW_I8 },
	{ "i16", TOK_KW_I16 },
	{ "i32", TOK_KW_I32 },
	{ "i64", TOK_KW_I64 },
	{ "bool", TOK_KW_BOOL },
	{ "str", TOK_KW_STR },
	{ "buf", TOK_KW_BUF },
	{ NULL, TOK_EOF },
};

/* ── Lexer state ─────────────────────────────────────────────────── */

typedef struct {
	const char *src;
	size_t len;
	size_t pos;
	int line;
	int col;
	const char *filename;

	kdal_token_t *tokens;
	int ntokens;
	int cap;
	kdal_arena_t *arena;
	int had_error;
} lexer_t;

/* ── Internal helpers ────────────────────────────────────────────── */

static inline char lx_cur(const lexer_t *lx)
{
	return lx->pos < lx->len ? lx->src[lx->pos] : '\0';
}

static inline char lx_peek(const lexer_t *lx, int ahead)
{
	size_t off = lx->pos + (size_t)ahead;
	return off < lx->len ? lx->src[off] : '\0';
}

static inline void lx_advance(lexer_t *lx)
{
	if (lx->pos < lx->len) {
		if (lx->src[lx->pos] == '\n') {
			lx->line++;
			lx->col = 1;
		} else {
			lx->col++;
		}
		lx->pos++;
	}
}

static void lx_emit(lexer_t *lx, kdal_tok_t type, const char *start,
		    size_t tok_len, int line, int col)
{
	if (lx->ntokens >= lx->cap) {
		int newcap = lx->cap ? lx->cap * 2 : 64;
		kdal_token_t *newarr = realloc(
			lx->tokens, (size_t)newcap * sizeof(kdal_token_t));
		if (!newarr) {
			fprintf(stderr, "%s: out of memory\n", lx->filename);
			lx->had_error = 1;
			return;
		}
		lx->tokens = newarr;
		lx->cap = newcap;
	}
	kdal_token_t *t = &lx->tokens[lx->ntokens++];
	t->type = type;
	t->src = start;
	t->len = tok_len;
	t->line = line;
	t->col = col;
	t->v.ival = 0;
}

static void lx_skip_block_comment(lexer_t *lx)
{
	/* already consumed '/' and '*' */
	while (lx->pos < lx->len) {
		if (lx_cur(lx) == '*' && lx_peek(lx, 1) == '/') {
			lx_advance(lx); /* * */
			lx_advance(lx); /* / */
			return;
		}
		lx_advance(lx);
	}
	kdal_error(lx->filename, lx->line, lx->col,
		   "unterminated block comment");
	lx->had_error = 1;
}

static void lx_skip_line_comment(lexer_t *lx)
{
	while (lx->pos < lx->len && lx_cur(lx) != '\n')
		lx_advance(lx);
}

static kdal_tok_t lx_classify_ident(const char *start, size_t len)
{
	for (int i = 0; kw_table[i].word; i++) {
		if (strlen(kw_table[i].word) == len &&
		    memcmp(kw_table[i].word, start, len) == 0)
			return kw_table[i].tok;
	}
	return TOK_IDENT;
}

static void lx_lex_ident(lexer_t *lx)
{
	int sline = lx->line, scol = lx->col;
	size_t start = lx->pos;
	while (isalnum((unsigned char)lx_cur(lx)) || lx_cur(lx) == '_')
		lx_advance(lx);
	size_t len = lx->pos - start;
	kdal_tok_t tt = lx_classify_ident(lx->src + start, len);
	kdal_token_t *t;
	lx_emit(lx, tt, lx->src + start, len, sline, scol);
	if (tt == TOK_BOOL_TRUE || tt == TOK_BOOL_FALSE) {
		t = &lx->tokens[lx->ntokens - 1];
		t->v.bval = (tt == TOK_BOOL_TRUE) ? 1 : 0;
	}
}

static void lx_lex_number(lexer_t *lx)
{
	int sline = lx->line, scol = lx->col;
	char c0 = lx_cur(lx);
	char c1 = lx_peek(lx, 1);

	if (c0 == '0' && (c1 == 'x' || c1 == 'X')) {
		/* Hex */
		size_t start = lx->pos;
		lx_advance(lx);
		lx_advance(lx); /* 0x */
		while (isxdigit((unsigned char)lx_cur(lx)))
			lx_advance(lx);
		size_t len = lx->pos - start;
		lx_emit(lx, TOK_HEX, lx->src + start, len, sline, scol);
		lx->tokens[lx->ntokens - 1].v.ival =
			(long long)strtoll(lx->src + start + 2, NULL, 16);
	} else if (c0 == '0' && (c1 == 'b' || c1 == 'B')) {
		/* Binary */
		size_t start = lx->pos;
		lx_advance(lx);
		lx_advance(lx); /* 0b */
		while (lx_cur(lx) == '0' || lx_cur(lx) == '1')
			lx_advance(lx);
		size_t len = lx->pos - start;
		lx_emit(lx, TOK_BINLIT, lx->src + start, len, sline, scol);
		lx->tokens[lx->ntokens - 1].v.ival =
			(long long)strtoll(lx->src + start + 2, NULL, 2);
	} else {
		/* Decimal */
		size_t start = lx->pos;
		while (isdigit((unsigned char)lx_cur(lx)))
			lx_advance(lx);
		size_t len = lx->pos - start;
		lx_emit(lx, TOK_INT, lx->src + start, len, sline, scol);
		lx->tokens[lx->ntokens - 1].v.ival =
			(long long)strtoll(lx->src + start, NULL, 10);
	}
}

static void lx_lex_string(lexer_t *lx)
{
	int sline = lx->line, scol = lx->col;
	size_t start = lx->pos; /* points at opening '"' */
	lx_advance(lx); /* consume '"' */
	while (lx->pos < lx->len) {
		char c = lx_cur(lx);
		if (c == '"') {
			lx_advance(lx); /* consume closing '"' */
			break;
		}
		if (c == '\n') {
			kdal_error(lx->filename, sline, scol,
				   "unterminated string literal");
			lx->had_error = 1;
			return;
		}
		if (c == '\\')
			lx_advance(lx); /* skip escape char */
		lx_advance(lx);
	}
	lx_emit(lx, TOK_STRING, lx->src + start, lx->pos - start, sline, scol);
}

/* ── Public API ──────────────────────────────────────────────────── */

int kdal_lex(kdal_arena_t *arena, const char *src, size_t src_len,
	     kdal_token_t **out_tokens, const char *filename)
{
	lexer_t lx = {
		.src = src,
		.len = src_len,
		.pos = 0,
		.line = 1,
		.col = 1,
		.filename = filename,
		.arena = arena,
	};

	while (lx.pos < lx.len) {
		char c = lx_cur(&lx);
		char c1 = lx_peek(&lx, 1);

		/* Whitespace */
		if (isspace((unsigned char)c)) {
			lx_advance(&lx);
			continue;
		}

		/* Line comment */
		if (c == '/' && c1 == '/') {
			lx_advance(&lx);
			lx_advance(&lx);
			lx_skip_line_comment(&lx);
			continue;
		}

		/* Block comment */
		if (c == '/' && c1 == '*') {
			lx_advance(&lx);
			lx_advance(&lx);
			lx_skip_block_comment(&lx);
			continue;
		}

		/* Identifiers / keywords */
		if (isalpha((unsigned char)c) || c == '_') {
			lx_lex_ident(&lx);
			continue;
		}

		/* Numbers */
		if (isdigit((unsigned char)c)) {
			lx_lex_number(&lx);
			continue;
		}

		/* String literals */
		if (c == '"') {
			lx_lex_string(&lx);
			continue;
		}

		/* Multi-char punctuation */
		int sline = lx.line, scol = lx.col;
#define EMIT2(a, b, T)                                                      \
	do {                                                                \
		if (c == (a) && c1 == (b)) {                                \
			lx_emit(&lx, (T), lx.src + lx.pos, 2, sline, scol); \
			lx_advance(&lx);                                    \
			lx_advance(&lx);                                    \
			goto next;                                          \
		}                                                           \
	} while (0)
		EMIT2('=', '=', TOK_EQEQ);
		EMIT2('!', '=', TOK_NEQ);
		EMIT2('<', '=', TOK_LE);
		EMIT2('>', '=', TOK_GE);
		EMIT2('<', '<', TOK_SHL);
		EMIT2('>', '>', TOK_SHR);
		EMIT2('&', '&', TOK_AMPAMP);
		EMIT2('|', '|', TOK_PIPEPIPE);
		EMIT2('.', '.', TOK_DOTDOT);
		EMIT2('-', '>', TOK_ARROW);
#undef EMIT2

		/* Single-char punctuation */
		{
			kdal_tok_t st = TOK_ERROR;
			switch (c) {
			case '{':
				st = TOK_LBRACE;
				break;
			case '}':
				st = TOK_RBRACE;
				break;
			case '(':
				st = TOK_LPAREN;
				break;
			case ')':
				st = TOK_RPAREN;
				break;
			case '[':
				st = TOK_LBRACKET;
				break;
			case ']':
				st = TOK_RBRACKET;
				break;
			case ';':
				st = TOK_SEMICOLON;
				break;
			case ':':
				st = TOK_COLON;
				break;
			case ',':
				st = TOK_COMMA;
				break;
			case '.':
				st = TOK_DOT;
				break;
			case '@':
				st = TOK_AT;
				break;
			case '=':
				st = TOK_EQ;
				break;
			case '<':
				st = TOK_LT;
				break;
			case '>':
				st = TOK_GT;
				break;
			case '+':
				st = TOK_PLUS;
				break;
			case '-':
				st = TOK_MINUS;
				break;
			case '*':
				st = TOK_STAR;
				break;
			case '/':
				st = TOK_SLASH;
				break;
			case '%':
				st = TOK_PERCENT;
				break;
			case '&':
				st = TOK_AMP;
				break;
			case '|':
				st = TOK_PIPE;
				break;
			case '^':
				st = TOK_CARET;
				break;
			case '~':
				st = TOK_TILDE;
				break;
			case '!':
				st = TOK_BANG;
				break;
			default:
				kdal_error(filename, lx.line, lx.col,
					   "unexpected character '%c' (0x%02x)",
					   c, (unsigned char)c);
				lx.had_error = 1;
				lx_advance(&lx);
				continue;
			}
			lx_emit(&lx, st, lx.src + lx.pos, 1, sline, scol);
			lx_advance(&lx);
		}
next:;
	}

	/* EOF sentinel */
	lx_emit(&lx, TOK_EOF, lx.src + lx.pos, 0, lx.line, lx.col);

	if (out_tokens)
		*out_tokens = lx.tokens;
	return lx.had_error ? -lx.ntokens : lx.ntokens;
}

/* ── Token name helpers ──────────────────────────────────────────── */

const char *kdal_tok_name(kdal_tok_t t)
{
	switch (t) {
	case TOK_IDENT:
		return "<ident>";
	case TOK_INT:
		return "<int>";
	case TOK_HEX:
		return "<hex>";
	case TOK_BINLIT:
		return "<bin>";
	case TOK_STRING:
		return "<string>";
	case TOK_BOOL_TRUE:
		return "true";
	case TOK_BOOL_FALSE:
		return "false";
	case TOK_EOF:
		return "<eof>";
	case TOK_ERROR:
		return "<error>";
	case TOK_LBRACE:
		return "{";
	case TOK_RBRACE:
		return "}";
	case TOK_LPAREN:
		return "(";
	case TOK_RPAREN:
		return ")";
	case TOK_SEMICOLON:
		return ";";
	case TOK_COLON:
		return ":";
	case TOK_COMMA:
		return ",";
	case TOK_DOT:
		return ".";
	case TOK_AT:
		return "@";
	case TOK_EQ:
		return "=";
	case TOK_EQEQ:
		return "==";
	case TOK_NEQ:
		return "!=";
	case TOK_LT:
		return "<";
	case TOK_LE:
		return "<=";
	case TOK_GT:
		return ">";
	case TOK_GE:
		return ">=";
	case TOK_DOTDOT:
		return "..";
	case TOK_ARROW:
		return "->";
	default:
		if (tok_is_keyword(t))
			return kdal_tok_keyword_str(t);
		return "?";
	}
}

const char *kdal_tok_keyword_str(kdal_tok_t t)
{
	for (int i = 0; kw_table[i].word; i++)
		if (kw_table[i].tok == t)
			return kw_table[i].word;
	return "<keyword>";
}
