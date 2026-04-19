/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * KDAL Compiler - token.h
 * Token definitions for the KDAL language lexer.
 *
 * A flat enum of all token types recognised by kdalc.
 * The enum is ordered: literals first, keywords second,
 * punctuation last, so range checks (e.g., tok_is_keyword()) are cheap.
 */

#ifndef KDALC_TOKEN_H
#define KDALC_TOKEN_H

#include <stddef.h>

/* ── Token types ─────────────────────────────────────────────────── */

typedef enum {
	/* Literals */
	TOK_IDENT, /* identifier or keyword */
	TOK_INT, /* decimal integer literal */
	TOK_HEX, /* hexadecimal integer literal */
	TOK_BINLIT, /* binary integer literal */
	TOK_STRING, /* string literal */
	TOK_BOOL_TRUE, /* boolean true literal */
	TOK_BOOL_FALSE, /* boolean false literal */

	/* Keywords (.kdh and .kdc shared) */
	TOK_KW_KDAL_VERSION,
	TOK_KW_IMPORT,
	TOK_KW_AS,
	TOK_KW_DEVICE,
	TOK_KW_CLASS,
	TOK_KW_REGISTERS,
	TOK_KW_SIGNALS,
	TOK_KW_CAPABILITIES,
	TOK_KW_POWER,
	TOK_KW_CONFIG,
	TOK_KW_BITFIELD,
	TOK_KW_RO,
	TOK_KW_WO,
	TOK_KW_RW,
	TOK_KW_RC,
	TOK_KW_IN, /* range qualifier or for-in */
	TOK_KW_EDGE,
	TOK_KW_LEVEL,
	TOK_KW_RISING,
	TOK_KW_FALLING,
	TOK_KW_ANY,
	TOK_KW_HIGH,
	TOK_KW_LOW,
	TOK_KW_COMPLETION,
	TOK_KW_ALLOWED,
	TOK_KW_FORBIDDEN,
	TOK_KW_DEFAULT,
	TOK_KW_ON,
	TOK_KW_OFF,
	TOK_KW_SUSPEND,

	/* Keywords (.kdc only) */
	TOK_KW_BACKEND,
	TOK_KW_DRIVER,
	TOK_KW_FOR,
	TOK_KW_PROBE,
	TOK_KW_REMOVE,
	TOK_KW_READ,
	TOK_KW_WRITE,
	TOK_KW_SIGNAL,
	TOK_KW_TIMEOUT,
	TOK_KW_EMIT,
	TOK_KW_WAIT,
	TOK_KW_ARM,
	TOK_KW_CANCEL,
	TOK_KW_LET,
	TOK_KW_RETURN,
	TOK_KW_LOG,
	TOK_KW_IF,
	TOK_KW_ELIF,
	TOK_KW_ELSE,

	/* Type keywords */
	TOK_KW_U8,
	TOK_KW_U16,
	TOK_KW_U32,
	TOK_KW_U64,
	TOK_KW_I8,
	TOK_KW_I16,
	TOK_KW_I32,
	TOK_KW_I64,
	TOK_KW_BOOL,
	TOK_KW_STR,
	TOK_KW_BUF,

	/* Punctuation and operators */
	TOK_LBRACE, /* left brace */
	TOK_RBRACE, /* right brace */
	TOK_LPAREN, /* left parenthesis */
	TOK_RPAREN, /* right parenthesis */
	TOK_LBRACKET, /* left bracket */
	TOK_RBRACKET, /* right bracket */
	TOK_SEMICOLON, /* semicolon */
	TOK_COLON, /* colon */
	TOK_COMMA, /* comma */
	TOK_DOT, /* dot */
	TOK_AT, /* at sign */
	TOK_EQ, /* assignment operator */
	TOK_EQEQ, /* equality operator */
	TOK_NEQ, /* inequality operator */
	TOK_LT, /* less-than operator */
	TOK_LE, /* less-than-or-equal operator */
	TOK_GT, /* greater-than operator */
	TOK_GE, /* greater-than-or-equal operator */
	TOK_PLUS, /* addition operator */
	TOK_MINUS, /* subtraction operator */
	TOK_STAR, /* multiplication operator */
	TOK_SLASH, /* division operator */
	TOK_PERCENT, /* modulo operator */
	TOK_AMP, /* bitwise and operator */
	TOK_PIPE, /* bitwise or operator */
	TOK_CARET, /* bitwise xor operator */
	TOK_TILDE, /* bitwise not operator */
	TOK_BANG, /* logical not operator */
	TOK_AMPAMP, /* logical and operator */
	TOK_PIPEPIPE, /* logical or operator */
	TOK_SHL, /* left shift operator */
	TOK_SHR, /* right shift operator */
	TOK_DOTDOT, /* range operator */
	TOK_ARROW, /* arrow operator */

	/* Meta */
	TOK_EOF,
	TOK_ERROR,
} kdal_tok_t;

/* ── Token structure ─────────────────────────────────────────────── */

typedef struct {
	kdal_tok_t type;
	const char *src; /* pointer into the source buffer */
	size_t len; /* length in bytes */
	int line;
	int col;
	union {
		long long ival; /* TOK_INT, TOK_HEX, TOK_BINLIT */
		int bval; /* TOK_BOOL_TRUE / FALSE */
	} v;
} kdal_token_t;

/* ── Helper predicates ───────────────────────────────────────────── */

static inline int tok_is_keyword(kdal_tok_t t)
{
	return t >= TOK_KW_KDAL_VERSION && t < TOK_LBRACE;
}

static inline int tok_is_type(kdal_tok_t t)
{
	return t >= TOK_KW_U8 && t <= TOK_KW_BUF;
}

static inline int tok_is_literal(kdal_tok_t t)
{
	return t >= TOK_IDENT && t <= TOK_BOOL_FALSE;
}

/* Map a keyword token back to its source spelling */
const char *kdal_tok_keyword_str(kdal_tok_t t);

/* Return a printable name for any token type (for diagnostics) */
const char *kdal_tok_name(kdal_tok_t t);

#endif /* KDALC_TOKEN_H */
