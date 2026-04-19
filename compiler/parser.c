// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler - parser.c
 * Recursive-descent parser: token stream → AST.
 *
 * The grammar is LL(1) for most constructs with up to 2-token lookahead.
 * All nodes are allocated from a kdal_arena_t.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "include/token.h"
#include "include/ast.h"
#include "include/codegen.h"

/* ── Parser state ────────────────────────────────────────────────── */

typedef struct {
	const kdal_token_t *tokens;
	int ntokens;
	int pos;
	kdal_arena_t *arena;
	const char *filename;
	int had_error;
} parser_t;

/* ── Arena implementation ────────────────────────────────────────── */

static kdal_arena_block_t *arena_new_block(size_t min_cap)
{
	kdal_arena_block_t *b = malloc(sizeof(*b) + min_cap);
	if (!b)
		return NULL;
	b->next = NULL;
	b->cap = min_cap;
	b->used = 0;
	return b;
}

kdal_arena_t *kdal_arena_new(size_t initial_cap)
{
	kdal_arena_t *a = malloc(sizeof(*a));
	if (!a)
		return NULL;
	a->block_size = initial_cap;
	a->head = arena_new_block(initial_cap);
	if (!a->head) {
		free(a);
		return NULL;
	}
	return a;
}

void *kdal_arena_alloc(kdal_arena_t *a, size_t sz)
{
	/* align to 8 bytes */
	sz = (sz + 7u) & ~7u;
	if (a->head->used + sz > a->head->cap) {
		size_t newcap = (a->block_size > sz) ? a->block_size : sz;
		kdal_arena_block_t *b = arena_new_block(newcap);
		if (!b)
			return NULL;
		b->next = a->head;
		a->head = b;
	}
	void *ptr = a->head->buf + a->head->used;
	a->head->used += sz;
	memset(ptr, 0, sz);
	return ptr;
}

void kdal_arena_free(kdal_arena_t *a)
{
	if (!a)
		return;
	kdal_arena_block_t *b = a->head;
	while (b) {
		kdal_arena_block_t *next = b->next;
		free(b);
		b = next;
	}
	free(a);
}

/* ── AST helpers ─────────────────────────────────────────────────── */

kdal_ast_t *kdal_ast_new(kdal_arena_t *a, kdal_node_type_t type, int line,
			 int col)
{
	kdal_ast_t *n = kdal_arena_alloc(a, sizeof(kdal_ast_t));
	if (n) {
		n->type = type;
		n->line = line;
		n->col = col;
	}
	return n;
}

kdal_ast_t *kdal_ast_append(kdal_ast_t *list, kdal_ast_t *node)
{
	if (!list)
		return node;
	kdal_ast_t *cur = list;
	while (cur->next)
		cur = cur->next;
	cur->next = node;
	return list;
}

void kdal_ast_add_child(kdal_ast_t *parent, kdal_ast_t *child)
{
	if (!parent->child) {
		parent->child = child;
		return;
	}
	kdal_ast_t *cur = parent->child;
	while (cur->next)
		cur = cur->next;
	cur->next = child;
}

/* ── Parser helpers ──────────────────────────────────────────────── */

static inline const kdal_token_t *p_cur(const parser_t *p)
{
	return &p->tokens[p->pos < p->ntokens ? p->pos : p->ntokens - 1];
}

static inline const kdal_token_t *p_peek(const parser_t *p, int ahead)
{
	int idx = p->pos + ahead;
	return &p->tokens[idx < p->ntokens ? idx : p->ntokens - 1];
}

static inline void p_advance(parser_t *p)
{
	if (p->pos < p->ntokens - 1)
		p->pos++;
}

static int p_expect(parser_t *p, kdal_tok_t t)
{
	if (p_cur(p)->type != t) {
		const kdal_token_t *tok = p_cur(p);
		kdal_error(p->filename, tok->line, tok->col,
			   "expected '%s', got '%s'", kdal_tok_name(t),
			   kdal_tok_name(tok->type));
		p->had_error = 1;
		return 0;
	}
	p_advance(p);
	return 1;
}

/* Intern a token's text into the arena as a NUL-terminated string */
static const char *p_intern(parser_t *p, const kdal_token_t *t)
{
	char *s = kdal_arena_alloc(p->arena, t->len + 1);
	if (s) {
		memcpy(s, t->src, t->len);
		s[t->len] = '\0';
	}
	return s;
}

/* ── Forward declarations ────────────────────────────────────────── */

static kdal_ast_t *parse_expr(parser_t *p);
static kdal_ast_t *parse_statement(parser_t *p);
static kdal_ast_t *parse_statement_list(parser_t *p);

/* ── Expression parser ───────────────────────────────────────────── */

static kdal_ast_t *parse_primary(parser_t *p)
{
	const kdal_token_t *t = p_cur(p);
	kdal_expr_node_t *e;

	switch (t->type) {
	case TOK_INT:
	case TOK_HEX:
	case TOK_BINLIT:
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.type = KDAL_NODE_EXPR_LITERAL_INT;
		e->base.line = t->line;
		e->base.col = t->col;
		e->u.ival = t->v.ival;
		p_advance(p);
		return &e->base;

	case TOK_BOOL_TRUE:
	case TOK_BOOL_FALSE:
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.type = KDAL_NODE_EXPR_LITERAL_BOOL;
		e->base.line = t->line;
		e->base.col = t->col;
		e->u.bval = t->v.bval;
		p_advance(p);
		return &e->base;

	case TOK_STRING:
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.type = KDAL_NODE_EXPR_LITERAL_STR;
		e->base.line = t->line;
		e->base.col = t->col;
		e->u.sval = p_intern(p, t);
		p_advance(p);
		return &e->base;

	case TOK_KW_READ: {
		int line = t->line, col = t->col;
		p_advance(p); /* read */
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *path = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.type = KDAL_NODE_EXPR_READ;
		e->base.line = line;
		e->base.col = col;
		e->u.read_path = path;
		return &e->base;
	}

	case TOK_IDENT: {
		/* Could be a plain ident or a reg_path (ident.ident.…) */
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.line = t->line;
		e->base.col = t->col;
		e->u.path.parts[0] = p_intern(p, t);
		e->u.path.nparts = 1;
		p_advance(p);

		while (p_cur(p)->type == TOK_DOT &&
		       p_peek(p, 1)->type == TOK_IDENT &&
		       e->u.path.nparts < 4) {
			p_advance(p); /* . */
			e->u.path.parts[e->u.path.nparts++] =
				p_intern(p, p_cur(p));
			p_advance(p);
		}

		if (e->u.path.nparts > 1) {
			e->base.type = KDAL_NODE_EXPR_REG_PATH;
		} else {
			/* single ident */
			e->base.type = KDAL_NODE_EXPR_IDENT;
			e->u.sval = e->u.path.parts[0];
		}
		return &e->base;
	}

	case TOK_LPAREN: {
		p_advance(p);
		kdal_ast_t *inner = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		return inner;
	}

	case TOK_MINUS:
	case TOK_BANG:
	case TOK_TILDE: {
		kdal_unop_t op = (t->type == TOK_MINUS) ? KDAL_UNOP_NEG :
				 (t->type == TOK_BANG)	? KDAL_UNOP_NOT :
							  KDAL_UNOP_INV;
		int line = t->line, col = t->col;
		p_advance(p);
		e = kdal_arena_alloc(p->arena, sizeof(*e));
		e->base.type = KDAL_NODE_EXPR_UNOP;
		e->base.line = line;
		e->base.col = col;
		e->u.unop.op = op;
		e->u.unop.operand = parse_primary(p);
		return &e->base;
	}

	default:
		kdal_error(p->filename, t->line, t->col,
			   "unexpected token '%s' in expression",
			   kdal_tok_name(t->type));
		p->had_error = 1;
		p_advance(p);
		return NULL;
	}
}

static int p_binop_prec(kdal_tok_t t)
{
	switch (t) {
	case TOK_PIPEPIPE:
		return 1;
	case TOK_AMPAMP:
		return 2;
	case TOK_PIPE:
		return 3;
	case TOK_CARET:
		return 4;
	case TOK_AMP:
		return 5;
	case TOK_EQEQ:
	case TOK_NEQ:
		return 6;
	case TOK_LT:
	case TOK_LE:
	case TOK_GT:
	case TOK_GE:
		return 7;
	case TOK_SHL:
	case TOK_SHR:
		return 8;
	case TOK_PLUS:
	case TOK_MINUS:
		return 9;
	case TOK_STAR:
	case TOK_SLASH:
	case TOK_PERCENT:
		return 10;
	default:
		return 0;
	}
}

static kdal_binop_t p_tok_to_binop(kdal_tok_t t)
{
	switch (t) {
	case TOK_PLUS:
		return KDAL_BINOP_ADD;
	case TOK_MINUS:
		return KDAL_BINOP_SUB;
	case TOK_STAR:
		return KDAL_BINOP_MUL;
	case TOK_SLASH:
		return KDAL_BINOP_DIV;
	case TOK_PERCENT:
		return KDAL_BINOP_MOD;
	case TOK_SHL:
		return KDAL_BINOP_SHL;
	case TOK_SHR:
		return KDAL_BINOP_SHR;
	case TOK_AMP:
		return KDAL_BINOP_AND;
	case TOK_PIPE:
		return KDAL_BINOP_OR;
	case TOK_CARET:
		return KDAL_BINOP_XOR;
	case TOK_EQEQ:
		return KDAL_BINOP_EQ;
	case TOK_NEQ:
		return KDAL_BINOP_NEQ;
	case TOK_LT:
		return KDAL_BINOP_LT;
	case TOK_LE:
		return KDAL_BINOP_LE;
	case TOK_GT:
		return KDAL_BINOP_GT;
	case TOK_GE:
		return KDAL_BINOP_GE;
	case TOK_AMPAMP:
		return KDAL_BINOP_LAND;
	case TOK_PIPEPIPE:
		return KDAL_BINOP_LOR;
	default:
		return KDAL_BINOP_ADD;
	}
}

static kdal_ast_t *parse_expr_prec(parser_t *p, int min_prec)
{
	kdal_ast_t *lhs = parse_primary(p);
	if (!lhs)
		return NULL;

	while (1) {
		int prec = p_binop_prec(p_cur(p)->type);
		if (prec < min_prec)
			break;

		kdal_tok_t op_tok = p_cur(p)->type;
		int op_line = p_cur(p)->line;
		int op_col = p_cur(p)->col;
		p_advance(p);

		kdal_ast_t *rhs = parse_expr_prec(p, prec + 1);
		if (!rhs)
			return lhs;

		kdal_expr_node_t *b = kdal_arena_alloc(p->arena, sizeof(*b));
		b->base.type = KDAL_NODE_EXPR_BINOP;
		b->base.line = op_line;
		b->base.col = op_col;
		b->u.binop.op = p_tok_to_binop(op_tok);
		b->u.binop.lhs = lhs;
		b->u.binop.rhs = rhs;
		lhs = &b->base;
	}
	return lhs;
}

static kdal_ast_t *parse_expr(parser_t *p)
{
	return parse_expr_prec(p, 1);
}

/* ── Statement parsers ───────────────────────────────────────────── */

static kdal_ast_t *parse_statement_list(parser_t *p)
{
	kdal_ast_t *head = NULL;
	while (p_cur(p)->type != TOK_RBRACE && p_cur(p)->type != TOK_EOF) {
		kdal_ast_t *s = parse_statement(p);
		if (s)
			head = kdal_ast_append(head, s);
		else
			p_advance(p); /* error recovery: skip token */
	}
	return head;
}

static kdal_ast_t *parse_let_statement(parser_t *p, int line, int col)
{
	const kdal_token_t *name_tok;
	kdal_ast_t *node, *val;
	kdal_expr_node_t *nm;
	const char *name;

	p_advance(p);
	name_tok = p_cur(p);
	if (name_tok->type != TOK_IDENT) {
		kdal_error(p->filename, line, col,
			   "expected identifier after 'let'");
		p->had_error = 1;
		return NULL;
	}

	name = p_intern(p, name_tok);
	p_advance(p);
	if (p_cur(p)->type == TOK_COLON) {
		p_advance(p);
		p_advance(p);
	}
	p_expect(p, TOK_EQ);
	val = parse_expr(p);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_LET_STMT, line, col);
	nm = kdal_arena_alloc(p->arena, sizeof(*nm));
	nm->base.type = KDAL_NODE_EXPR_IDENT;
	nm->base.line = line;
	nm->base.col = col;
	nm->u.sval = name;
	kdal_ast_add_child(node, &nm->base);
	kdal_ast_add_child(node, val);
	return node;
}

static kdal_ast_t *parse_write_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *reg, *val;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	reg = parse_expr(p);
	p_expect(p, TOK_COMMA);
	val = parse_expr(p);
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_REG_WRITE_STMT, line, col);
	kdal_ast_add_child(node, reg);
	kdal_ast_add_child(node, val);
	return node;
}

static kdal_ast_t *parse_emit_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *sig, *val = NULL;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	sig = parse_expr(p);
	if (p_cur(p)->type == TOK_COMMA) {
		p_advance(p);
		val = parse_expr(p);
	}
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_EMIT_STMT, line, col);
	kdal_ast_add_child(node, sig);
	if (val)
		kdal_ast_add_child(node, val);
	return node;
}

static kdal_ast_t *parse_wait_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *sig, *ms;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	sig = parse_expr(p);
	p_expect(p, TOK_COMMA);
	ms = parse_expr(p);
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_WAIT_STMT, line, col);
	kdal_ast_add_child(node, sig);
	kdal_ast_add_child(node, ms);
	return node;
}

static kdal_ast_t *parse_arm_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *timer, *ms;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	timer = parse_expr(p);
	p_expect(p, TOK_COMMA);
	ms = parse_expr(p);
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_ARM_STMT, line, col);
	kdal_ast_add_child(node, timer);
	kdal_ast_add_child(node, ms);
	return node;
}

static kdal_ast_t *parse_cancel_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *timer;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	timer = parse_expr(p);
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_CANCEL_STMT, line, col);
	kdal_ast_add_child(node, timer);
	return node;
}

static kdal_ast_t *parse_return_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *val = NULL;

	p_advance(p);
	if (p_cur(p)->type != TOK_SEMICOLON)
		val = parse_expr(p);
	p_expect(p, TOK_SEMICOLON);
	node = kdal_ast_new(p->arena, KDAL_NODE_RETURN_STMT, line, col);
	if (val)
		kdal_ast_add_child(node, val);
	return node;
}

static kdal_ast_t *parse_log_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *arg;

	p_advance(p);
	p_expect(p, TOK_LPAREN);
	node = kdal_ast_new(p->arena, KDAL_NODE_LOG_STMT, line, col);
	while ((arg = parse_expr(p)) != NULL) {
		kdal_ast_add_child(node, arg);
		if (p_cur(p)->type != TOK_COMMA)
			break;
		p_advance(p);
	}
	p_expect(p, TOK_RPAREN);
	p_expect(p, TOK_SEMICOLON);
	return node;
}

static kdal_ast_t *parse_if_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node = kdal_ast_new(p->arena, KDAL_NODE_IF_STMT, line, col);

	while (p_cur(p)->type == TOK_KW_IF || p_cur(p)->type == TOK_KW_ELIF) {
		kdal_ast_t *cond, *body;

		p_advance(p);
		p_expect(p, TOK_LPAREN);
		cond = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		p_expect(p, TOK_LBRACE);
		body = parse_statement_list(p);
		p_expect(p, TOK_RBRACE);
		kdal_ast_add_child(node, cond);
		kdal_ast_add_child(node, body ? body :
						kdal_ast_new(p->arena,
							     KDAL_NODE_INVALID,
							     line, col));
		if (p_cur(p)->type != TOK_KW_ELIF)
			break;
	}

	if (p_cur(p)->type == TOK_KW_ELSE) {
		kdal_ast_t *body;

		p_advance(p);
		p_expect(p, TOK_LBRACE);
		body = parse_statement_list(p);
		p_expect(p, TOK_RBRACE);
		kdal_ast_add_child(node, body ? body :
						kdal_ast_new(p->arena,
							     KDAL_NODE_INVALID,
							     line, col));
	}

	return node;
}

static kdal_ast_t *parse_for_statement(parser_t *p, int line, int col)
{
	kdal_ast_t *node, *lo, *hi, *body;
	kdal_expr_node_t *vn;
	const char *var;

	p_advance(p);
	var = p_intern(p, p_cur(p));
	p_advance(p);
	p_expect(p, TOK_KW_IN);
	lo = parse_expr(p);
	p_expect(p, TOK_DOTDOT);
	hi = parse_expr(p);
	p_expect(p, TOK_LBRACE);
	body = parse_statement_list(p);
	p_expect(p, TOK_RBRACE);
	node = kdal_ast_new(p->arena, KDAL_NODE_FOR_STMT, line, col);
	vn = kdal_arena_alloc(p->arena, sizeof(*vn));
	vn->base.type = KDAL_NODE_EXPR_IDENT;
	vn->base.line = line;
	vn->base.col = col;
	vn->u.sval = var;
	kdal_ast_add_child(node, &vn->base);
	kdal_ast_add_child(node, lo);
	kdal_ast_add_child(node, hi);
	if (body)
		kdal_ast_add_child(node, body);
	return node;
}

static kdal_ast_t *parse_assignment_statement(parser_t *p, int line, int col,
					      const kdal_token_t *t)
{
	kdal_ast_t *node, *lval, *rval;

	lval = parse_expr(p);
	if (p_cur(p)->type == TOK_EQ) {
		p_advance(p);
		rval = parse_expr(p);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_ASSIGN_STMT, line, col);
		kdal_ast_add_child(node, lval);
		kdal_ast_add_child(node, rval);
		return node;
	}

	kdal_error(p->filename, line, col, "unexpected token '%s' in statement",
		   kdal_tok_name(t->type));
	p->had_error = 1;
	return NULL;
}

static kdal_ast_t *parse_statement(parser_t *p)
{
	const kdal_token_t *t = p_cur(p);
	int line = t->line, col = t->col;

	if (t->type == TOK_KW_LET)
		return parse_let_statement(p, line, col);
	if (t->type == TOK_KW_WRITE)
		return parse_write_statement(p, line, col);
	if (t->type == TOK_KW_EMIT)
		return parse_emit_statement(p, line, col);
	if (t->type == TOK_KW_WAIT)
		return parse_wait_statement(p, line, col);
	if (t->type == TOK_KW_ARM)
		return parse_arm_statement(p, line, col);
	if (t->type == TOK_KW_CANCEL)
		return parse_cancel_statement(p, line, col);
	if (t->type == TOK_KW_RETURN)
		return parse_return_statement(p, line, col);
	if (t->type == TOK_KW_LOG)
		return parse_log_statement(p, line, col);
	if (t->type == TOK_KW_IF)
		return parse_if_statement(p, line, col);
	if (t->type == TOK_KW_FOR)
		return parse_for_statement(p, line, col);
	return parse_assignment_statement(p, line, col, t);
}

/* ── Device class body parsers ────────────────────────────────────── */

/* Check if current token is an identifier matching a specific string */
static int p_is_ident(const parser_t *p, const char *name)
{
	const kdal_token_t *t = p_cur(p);
	return t->type == TOK_IDENT && (size_t)t->len == strlen(name) &&
	       memcmp(t->src, name, t->len) == 0;
}

/* Parse an access qualifier (ro/wo/rw/rc); returns default if absent */
static kdal_access_t parse_access(parser_t *p)
{
	switch (p_cur(p)->type) {
	case TOK_KW_RO:
		p_advance(p);
		return KDAL_ACCESS_RO;
	case TOK_KW_WO:
		p_advance(p);
		return KDAL_ACCESS_WO;
	case TOK_KW_RW:
		p_advance(p);
		return KDAL_ACCESS_RW;
	case TOK_KW_RC:
		p_advance(p);
		return KDAL_ACCESS_RC;
	default:
		return KDAL_ACCESS_RW;
	}
}

/* Parse bitfield members using name, bit range, and optional reset. */
static kdal_ast_t *parse_bitfield_members(parser_t *p)
{
	kdal_ast_t *members = NULL;
	while (p_cur(p)->type == TOK_IDENT) {
		int line = p_cur(p)->line, col = p_cur(p)->col;
		kdal_bf_member_node_t *m =
			kdal_arena_alloc(p->arena, sizeof(*m));
		m->base.type = KDAL_NODE_BITFIELD_MEMBER;
		m->base.line = line;
		m->base.col = col;
		m->name = p_intern(p, p_cur(p));
		p_advance(p);
		p_expect(p, TOK_COLON);
		/* Parse a single bit or a low/high bit range. */
		if (p_cur(p)->type == TOK_INT || p_cur(p)->type == TOK_HEX) {
			m->bit_lo = (int)p_cur(p)->v.ival;
			m->bit_hi = m->bit_lo;
			p_advance(p);
			if (p_cur(p)->type == TOK_DOTDOT) {
				p_advance(p);
				if (p_cur(p)->type == TOK_INT ||
				    p_cur(p)->type == TOK_HEX) {
					m->bit_hi = (int)p_cur(p)->v.ival;
					p_advance(p);
				}
			}
		}
		/* Parse an optional reset value. */
		if (p_cur(p)->type == TOK_EQ) {
			p_advance(p);
			m->reset_val = parse_expr(p);
		}
		p_expect(p, TOK_SEMICOLON);
		members = kdal_ast_append(members, &m->base);
	}
	return members;
}

/* Parse a single register declaration (both forms) */
static kdal_ast_t *parse_register_decl(parser_t *p, int simplified)
{
	int line = p_cur(p)->line, col = p_cur(p)->col;
	kdal_reg_decl_node_t *reg = kdal_arena_alloc(p->arena, sizeof(*reg));
	reg->base.type = KDAL_NODE_REGISTER_DECL;
	reg->base.line = line;
	reg->base.col = col;
	reg->offset = UINT64_MAX;
	reg->access = KDAL_ACCESS_RW;
	reg->width = 32;
	reg->name = p_intern(p, p_cur(p));
	p_advance(p);

	if (simplified || p_cur(p)->type != TOK_COLON) {
		/* Compact register declaration with offset and access. */
		if (p_cur(p)->type == TOK_HEX || p_cur(p)->type == TOK_INT) {
			reg->offset = (uint64_t)p_cur(p)->v.ival;
			p_advance(p);
		}
		reg->access = parse_access(p);
		p_expect(p, TOK_SEMICOLON);
	} else {
		/* Structured register declaration with type and optional metadata. */
		p_expect(p, TOK_COLON);
		if (p_cur(p)->type == TOK_KW_BITFIELD) {
			reg->is_bitfield = 1;
			reg->width = 0;
			p_advance(p);
			if (p_cur(p)->type == TOK_AT) {
				p_advance(p);
				if (p_cur(p)->type == TOK_HEX ||
				    p_cur(p)->type == TOK_INT) {
					reg->offset =
						(uint64_t)p_cur(p)->v.ival;
					p_advance(p);
				}
			}
			reg->access = parse_access(p);
			p_expect(p, TOK_LBRACE);
			reg->members = parse_bitfield_members(p);
			p_expect(p, TOK_RBRACE);
			p_expect(p, TOK_SEMICOLON);
		} else {
			/* Scalar register width. */
			switch (p_cur(p)->type) {
			case TOK_KW_U8:
				reg->width = 8;
				p_advance(p);
				break;
			case TOK_KW_U16:
				reg->width = 16;
				p_advance(p);
				break;
			case TOK_KW_U32:
				reg->width = 32;
				p_advance(p);
				break;
			case TOK_KW_U64:
				reg->width = 64;
				p_advance(p);
				break;
			default:
				kdal_error(p->filename, p_cur(p)->line,
					   p_cur(p)->col,
					   "expected type (u8/u16/u32/u64/"
					   "bitfield)");
				p->had_error = 1;
				break;
			}
			if (p_cur(p)->type == TOK_AT) {
				p_advance(p);
				if (p_cur(p)->type == TOK_HEX ||
				    p_cur(p)->type == TOK_INT) {
					reg->offset =
						(uint64_t)p_cur(p)->v.ival;
					p_advance(p);
				}
			}
			reg->access = parse_access(p);
			if (p_cur(p)->type == TOK_EQ) {
				p_advance(p);
				reg->reset_val = parse_expr(p);
			}
			p_expect(p, TOK_SEMICOLON);
		}
	}
	return &reg->base;
}

/* Parse a single signal declaration */
static kdal_ast_t *parse_signal_decl(parser_t *p)
{
	int line = p_cur(p)->line, col = p_cur(p)->col;
	kdal_signal_node_t *sig = kdal_arena_alloc(p->arena, sizeof(*sig));
	sig->base.type = KDAL_NODE_SIGNAL_DECL;
	sig->base.line = line;
	sig->base.col = col;
	sig->dir = KDAL_SIG_IN;
	sig->trigger = KDAL_TRIG_EDGE_RISING;
	sig->name = p_intern(p, p_cur(p));
	p_advance(p);

	if (p_cur(p)->type == TOK_COLON) {
		/* Structured signal declaration with direction and trigger. */
		p_advance(p);
		/* direction */
		if (p_cur(p)->type == TOK_KW_IN) {
			sig->dir = KDAL_SIG_IN;
			p_advance(p);
		} else if (p_is_ident(p, "out")) {
			sig->dir = KDAL_SIG_OUT;
			p_advance(p);
		} else if (p_is_ident(p, "inout")) {
			sig->dir = KDAL_SIG_INOUT;
			p_advance(p);
		}
		/* trigger */
		if (p_cur(p)->type == TOK_KW_EDGE) {
			p_advance(p);
			p_expect(p, TOK_LPAREN);
			switch (p_cur(p)->type) {
			case TOK_KW_RISING:
				sig->trigger = KDAL_TRIG_EDGE_RISING;
				break;
			case TOK_KW_FALLING:
				sig->trigger = KDAL_TRIG_EDGE_FALLING;
				break;
			case TOK_KW_ANY:
				sig->trigger = KDAL_TRIG_EDGE_ANY;
				break;
			default:
				break;
			}
			p_advance(p);
			p_expect(p, TOK_RPAREN);
		} else if (p_cur(p)->type == TOK_KW_LEVEL) {
			p_advance(p);
			p_expect(p, TOK_LPAREN);
			switch (p_cur(p)->type) {
			case TOK_KW_HIGH:
				sig->trigger = KDAL_TRIG_LEVEL_HIGH;
				break;
			case TOK_KW_LOW:
				sig->trigger = KDAL_TRIG_LEVEL_LOW;
				break;
			default:
				break;
			}
			p_advance(p);
			p_expect(p, TOK_RPAREN);
		} else if (p_cur(p)->type == TOK_KW_COMPLETION) {
			sig->trigger = KDAL_TRIG_COMPLETION;
			p_advance(p);
		} else if (p_cur(p)->type == TOK_KW_TIMEOUT) {
			sig->trigger = KDAL_TRIG_TIMEOUT;
			p_advance(p);
			p_expect(p, TOK_LPAREN);
			sig->trigger_arg = parse_expr(p);
			p_expect(p, TOK_RPAREN);
		}
	}
	/* Otherwise the simplified declaration uses the default signal settings. */
	p_expect(p, TOK_SEMICOLON);
	return &sig->base;
}

/* Parse a single capability declaration */
static kdal_ast_t *parse_capability_decl(parser_t *p)
{
	int line = p_cur(p)->line, col = p_cur(p)->col;
	kdal_cap_node_t *cap = kdal_arena_alloc(p->arena, sizeof(*cap));
	cap->base.type = KDAL_NODE_CAPABILITY_DECL;
	cap->base.line = line;
	cap->base.col = col;
	cap->name = p_intern(p, p_cur(p));
	p_advance(p);

	if (p_cur(p)->type == TOK_EQ) {
		/* Explicit capability value. */
		p_advance(p);
		cap->value = parse_expr(p);
	} else if (p_cur(p)->type != TOK_SEMICOLON) {
		/* Compact capability declaration without an equals token. */
		cap->value = parse_expr(p);
	}
	p_expect(p, TOK_SEMICOLON);
	return &cap->base;
}

/* Check if current token is a power state keyword or identifier */
static int p_is_power_state_name(const parser_t *p)
{
	kdal_tok_t t = p_cur(p)->type;
	return t == TOK_KW_ON || t == TOK_KW_OFF || t == TOK_KW_SUSPEND ||
	       t == TOK_KW_DEFAULT || t == TOK_IDENT;
}

/* Parse a single power state declaration */
static kdal_ast_t *parse_power_state_decl(parser_t *p, int simplified)
{
	int line = p_cur(p)->line, col = p_cur(p)->col;
	kdal_power_state_node_t *ps = kdal_arena_alloc(p->arena, sizeof(*ps));
	ps->base.type = KDAL_NODE_POWER_STATE;
	ps->base.line = line;
	ps->base.col = col;
	ps->name = p_intern(p, p_cur(p));
	p_advance(p);

	if (!simplified && p_cur(p)->type == TOK_COLON) {
		/* Structured power-state declaration with a spec name. */
		p_advance(p);
		ps->spec = p_intern(p, p_cur(p));
		p_advance(p);
	}
	p_expect(p, TOK_SEMICOLON);
	return &ps->base;
}

/* Parse a config field with type plus optional default and range. */
static kdal_ast_t *parse_config_field_decl(parser_t *p)
{
	int line = p_cur(p)->line, col = p_cur(p)->col;
	kdal_config_field_node_t *cf = kdal_arena_alloc(p->arena, sizeof(*cf));
	cf->base.type = KDAL_NODE_CONFIG_FIELD;
	cf->base.line = line;
	cf->base.col = col;
	cf->name = p_intern(p, p_cur(p));
	p_advance(p);
	p_expect(p, TOK_COLON);
	/* Parse the type keyword. */
	cf->type_kw = (int)p_cur(p)->type;
	p_advance(p);
	/* Parse an optional default value. */
	if (p_cur(p)->type == TOK_EQ) {
		p_advance(p);
		cf->default_val = parse_expr(p);
	}
	/* Parse an optional inclusive range. */
	if (p_cur(p)->type == TOK_KW_IN) {
		p_advance(p);
		cf->range_lo = parse_expr(p);
		p_expect(p, TOK_DOTDOT);
		cf->range_hi = parse_expr(p);
	}
	p_expect(p, TOK_SEMICOLON);
	return &cf->base;
}

/* Parse the body of a device class declaration */
static void parse_device_class_body(parser_t *p, kdal_device_node_t *dev)
{
	while (p_cur(p)->type != TOK_RBRACE && p_cur(p)->type != TOK_EOF) {
		if (p_cur(p)->type == TOK_KW_CLASS) {
			/* Simplified class declaration for the device class type. */
			p_advance(p);
			if (p_cur(p)->type == TOK_IDENT)
				dev->device_class_type = p_intern(p, p_cur(p));
			p_advance(p);
			p_expect(p, TOK_SEMICOLON);

		} else if (p_is_ident(p, "compatible")) {
			/* Compatible string declaration. */
			p_advance(p);
			if (p_cur(p)->type == TOK_STRING)
				dev->compatible = p_intern(p, p_cur(p));
			p_advance(p);
			p_expect(p, TOK_SEMICOLON);

		} else if (p_cur(p)->type == TOK_KW_REGISTERS ||
			   p_is_ident(p, "register_map")) {
			int simplified = p_is_ident(p, "register_map");
			p_advance(p);
			int bl = p_cur(p)->line, bc = p_cur(p)->col;
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *block = kdal_ast_new(
				p->arena, KDAL_NODE_REGISTERS_BLOCK, bl, bc);
			while (p_cur(p)->type != TOK_RBRACE &&
			       p_cur(p)->type != TOK_EOF) {
				if (p_cur(p)->type == TOK_IDENT) {
					kdal_ast_t *r = parse_register_decl(
						p, simplified);
					kdal_ast_add_child(block, r);
				} else {
					p_advance(p);
				}
			}
			p_expect(p, TOK_RBRACE);
			dev->registers = block;

		} else if (p_cur(p)->type == TOK_KW_SIGNALS) {
			p_advance(p);
			int bl = p_cur(p)->line, bc = p_cur(p)->col;
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *block = kdal_ast_new(
				p->arena, KDAL_NODE_SIGNALS_BLOCK, bl, bc);
			while (p_cur(p)->type != TOK_RBRACE &&
			       p_cur(p)->type != TOK_EOF) {
				if (p_cur(p)->type == TOK_IDENT) {
					kdal_ast_t *s = parse_signal_decl(p);
					kdal_ast_add_child(block, s);
				} else {
					p_advance(p);
				}
			}
			p_expect(p, TOK_RBRACE);
			dev->signals = block;

		} else if (p_cur(p)->type == TOK_KW_CAPABILITIES) {
			p_advance(p);
			int bl = p_cur(p)->line, bc = p_cur(p)->col;
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *block = kdal_ast_new(
				p->arena, KDAL_NODE_CAPABILITIES_BLOCK, bl, bc);
			while (p_cur(p)->type != TOK_RBRACE &&
			       p_cur(p)->type != TOK_EOF) {
				if (p_cur(p)->type == TOK_IDENT) {
					kdal_ast_t *c =
						parse_capability_decl(p);
					kdal_ast_add_child(block, c);
				} else {
					p_advance(p);
				}
			}
			p_expect(p, TOK_RBRACE);
			dev->capabilities = block;

		} else if (p_cur(p)->type == TOK_KW_POWER ||
			   p_is_ident(p, "power_states")) {
			int simplified = p_is_ident(p, "power_states");
			p_advance(p);
			int bl = p_cur(p)->line, bc = p_cur(p)->col;
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *block = kdal_ast_new(
				p->arena, KDAL_NODE_POWER_BLOCK, bl, bc);
			while (p_cur(p)->type != TOK_RBRACE &&
			       p_cur(p)->type != TOK_EOF) {
				if (p_is_power_state_name(p)) {
					kdal_ast_t *ps = parse_power_state_decl(
						p, simplified);
					kdal_ast_add_child(block, ps);
				} else {
					p_advance(p);
				}
			}
			p_expect(p, TOK_RBRACE);
			dev->power = block;

		} else if (p_cur(p)->type == TOK_KW_CONFIG) {
			p_advance(p);
			int bl = p_cur(p)->line, bc = p_cur(p)->col;
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *block = kdal_ast_new(
				p->arena, KDAL_NODE_CONFIG_BLOCK, bl, bc);
			while (p_cur(p)->type != TOK_RBRACE &&
			       p_cur(p)->type != TOK_EOF) {
				if (p_cur(p)->type == TOK_IDENT) {
					kdal_ast_t *f =
						parse_config_field_decl(p);
					kdal_ast_add_child(block, f);
				} else {
					p_advance(p);
				}
			}
			p_expect(p, TOK_RBRACE);
			dev->config = block;

		} else {
			/* error recovery - skip unrecognised tokens */
			p_advance(p);
		}
	}
}

/* ── Top-level parser ────────────────────────────────────────────── */

kdal_file_node_t *kdal_parse(kdal_arena_t *arena, const kdal_token_t *tokens,
			     int ntokens, const char *filename)
{
	char *filename_copy;
	size_t filename_len;

	if (!arena || !tokens || ntokens < 0 || !filename)
		return NULL;

	filename_len = strlen(filename) + 1;
	filename_copy = kdal_arena_alloc(arena, filename_len);
	if (!filename_copy)
		return NULL;
	memcpy(filename_copy, filename, filename_len);

	parser_t p = {
		.tokens = tokens,
		.ntokens = ntokens,
		.pos = 0,
		.arena = arena,
		.filename = filename_copy,
	};

	kdal_file_node_t *file = kdal_arena_alloc(arena, sizeof(*file));
	if (!file)
		return NULL;
	file->base.type = KDAL_NODE_FILE;
	file->filename = filename_copy;

	/* version_decl? */
	if (p_cur(&p)->type == TOK_KW_KDAL_VERSION) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		p_expect(&p, TOK_COLON);
		const kdal_token_t *v = p_cur(&p);
		if (v->type == TOK_STRING) {
			kdal_version_node_t *vn =
				kdal_arena_alloc(arena, sizeof(*vn));
			vn->base.type = KDAL_NODE_VERSION;
			vn->base.line = line;
			vn->base.col = col;
			vn->version = p_intern(&p, v);
			file->version = &vn->base;
			p_advance(&p);
		}
		p_expect(&p, TOK_SEMICOLON);
	}

	/* import_decl* */
	while (p_cur(&p)->type == TOK_KW_IMPORT) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		const kdal_token_t *path_tok = p_cur(&p);
		kdal_import_node_t *im = kdal_arena_alloc(arena, sizeof(*im));
		im->base.type = KDAL_NODE_IMPORT;
		im->base.line = line;
		im->base.col = col;
		if (path_tok->type == TOK_STRING) {
			im->path = p_intern(&p, path_tok);
			p_advance(&p);
		}
		if (p_cur(&p)->type == TOK_KW_AS) {
			p_advance(&p);
			im->alias = p_intern(&p, p_cur(&p));
			if (p_cur(&p)->type == TOK_IDENT)
				p_advance(&p);
		}
		p_expect(&p, TOK_SEMICOLON);
		file->imports = kdal_ast_append(file->imports, &im->base);
	}

	/* backend_decl? */
	if (p_cur(&p)->type == TOK_KW_BACKEND) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		kdal_backend_node_t *be = kdal_arena_alloc(arena, sizeof(*be));
		be->base.type = KDAL_NODE_BACKEND;
		be->base.line = line;
		be->base.col = col;
		be->name = p_intern(&p, p_cur(&p));
		p_advance(&p);
		if (p_cur(&p)->type == TOK_LBRACE) {
			p_advance(&p);
			while (p_cur(&p)->type == TOK_IDENT) {
				kdal_backend_opt_node_t *opt =
					kdal_arena_alloc(arena, sizeof(*opt));
				opt->base.type = KDAL_NODE_BACKEND_OPTION;
				opt->key = p_intern(&p, p_cur(&p));
				p_advance(&p);
				p_expect(&p, TOK_COLON);
				opt->value = parse_expr(&p);
				p_expect(&p, TOK_SEMICOLON);
				be->options = kdal_ast_append(be->options,
							      &opt->base);
			}
			p_expect(&p, TOK_RBRACE);
		}
		file->backend = &be->base;
	}

	/* device class or driver */
	if (p_cur(&p)->type == TOK_KW_DEVICE) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		/* 'class' keyword is optional (simplified form omits it) */
		if (p_cur(&p)->type == TOK_KW_CLASS)
			p_advance(&p);
		kdal_device_node_t *dev = kdal_arena_alloc(arena, sizeof(*dev));
		dev->base.type = KDAL_NODE_DEVICE_CLASS;
		dev->base.line = line;
		dev->base.col = col;
		dev->name = p_intern(&p, p_cur(&p));
		p_advance(&p);
		p_expect(&p, TOK_LBRACE);
		parse_device_class_body(&p, dev);
		p_expect(&p, TOK_RBRACE);
		file->device = &dev->base;
	} else if (p_cur(&p)->type == TOK_KW_DRIVER) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		kdal_driver_node_t *drv = kdal_arena_alloc(arena, sizeof(*drv));
		drv->base.type = KDAL_NODE_DRIVER;
		drv->base.line = line;
		drv->base.col = col;
		drv->name = p_intern(&p, p_cur(&p));
		p_advance(&p);
		if (p_cur(&p)->type == TOK_KW_FOR) {
			p_advance(&p);
			drv->device_class = p_intern(&p, p_cur(&p));
			p_advance(&p);
			/* consume qualified name: IDENT.IDENT */
			while (p_cur(&p)->type == TOK_DOT) {
				p_advance(&p); /* skip '.' */
				if (p_cur(&p)->type == TOK_IDENT) {
					drv->device_class =
						p_intern(&p, p_cur(&p));
					p_advance(&p);
				}
			}
		}
		p_expect(&p, TOK_LBRACE);
		/* Parse probe / remove / on handlers */
		while (p_cur(&p)->type != TOK_RBRACE &&
		       p_cur(&p)->type != TOK_EOF) {
			if (p_cur(&p)->type == TOK_KW_PROBE) {
				p_advance(&p);
				p_expect(&p, TOK_LBRACE);
				kdal_ast_t *body = parse_statement_list(&p);
				p_expect(&p, TOK_RBRACE);
				kdal_ast_t *ph = kdal_ast_new(
					arena, KDAL_NODE_PROBE_HANDLER, line,
					col);
				if (body)
					kdal_ast_add_child(ph, body);
				drv->probe = ph;
			} else if (p_cur(&p)->type == TOK_KW_REMOVE) {
				p_advance(&p);
				p_expect(&p, TOK_LBRACE);
				kdal_ast_t *body = parse_statement_list(&p);
				p_expect(&p, TOK_RBRACE);
				kdal_ast_t *rh = kdal_ast_new(
					arena, KDAL_NODE_REMOVE_HANDLER, line,
					col);
				if (body)
					kdal_ast_add_child(rh, body);
				drv->remove = rh;
			} else if (p_cur(&p)->type == TOK_KW_ON) {
				int hl = p_cur(&p)->line, hc = p_cur(&p)->col;
				p_advance(&p); /* on */
				kdal_handler_node_t *h =
					kdal_arena_alloc(arena, sizeof(*h));
				h->base.type = KDAL_NODE_EVENT_HANDLER;
				h->base.line = hl;
				h->base.col = hc;
				kdal_tok_t et = p_cur(&p)->type;
				p_advance(
					&p); /* read/write/signal/power/timeout */
				switch (et) {
				case TOK_KW_READ:
					h->evt_type = KDAL_EVT_READ;
					break;
				case TOK_KW_WRITE:
					h->evt_type = KDAL_EVT_WRITE;
					break;
				case TOK_KW_SIGNAL:
					h->evt_type = KDAL_EVT_SIGNAL;
					break;
				case TOK_KW_POWER:
					h->evt_type = KDAL_EVT_POWER;
					break;
				case TOK_KW_TIMEOUT:
					h->evt_type = KDAL_EVT_TIMEOUT;
					break;
				default:
					break;
				}
				p_expect(&p, TOK_LPAREN);
				/* consume handler args until ')' */
				while (p_cur(&p)->type != TOK_RPAREN &&
				       p_cur(&p)->type != TOK_EOF)
					p_advance(&p);
				p_expect(&p, TOK_RPAREN);
				p_expect(&p, TOK_LBRACE);
				h->body = parse_statement_list(&p);
				p_expect(&p, TOK_RBRACE);
				drv->handlers = kdal_ast_append(drv->handlers,
								&h->base);
			} else if (p_cur(&p)->type == TOK_KW_CONFIG) {
				p_advance(&p);
				p_expect(&p, TOK_LBRACE);
				while (p_cur(&p)->type != TOK_RBRACE &&
				       p_cur(&p)->type != TOK_EOF)
					p_advance(&p);
				p_expect(&p, TOK_RBRACE);
			} else {
				p_advance(&p); /* error recovery */
			}
		}
		p_expect(&p, TOK_RBRACE);
		file->driver = &drv->base;
	}

	return p.had_error ? NULL : file;
}
