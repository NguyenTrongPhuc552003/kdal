// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler — parser.c
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

kdal_arena_t *kdal_arena_new(size_t initial_cap)
{
	kdal_arena_t *a = malloc(sizeof(*a));
	if (!a)
		return NULL;
	a->buf = malloc(initial_cap);
	a->cap = a->buf ? initial_cap : 0;
	a->used = 0;
	return a;
}

void *kdal_arena_alloc(kdal_arena_t *a, size_t sz)
{
	/* align to 8 bytes */
	sz = (sz + 7u) & ~7u;
	if (a->used + sz > a->cap) {
		size_t newcap = (a->cap * 2 > a->used + sz) ?
					a->cap * 2 :
					a->used + sz + 4096;
		char *newbuf = realloc(a->buf, newcap);
		if (!newbuf)
			return NULL;
		a->buf = newbuf;
		a->cap = newcap;
	}
	void *ptr = a->buf + a->used;
	a->used += sz;
	memset(ptr, 0, sz);
	return ptr;
}

void kdal_arena_free(kdal_arena_t *a)
{
	if (a) {
		free(a->buf);
		free(a);
	}
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

static kdal_ast_t *parse_statement(parser_t *p)
{
	const kdal_token_t *t = p_cur(p);
	int line = t->line, col = t->col;
	kdal_ast_t *node;

	switch (t->type) {
	case TOK_KW_LET: {
		p_advance(p);
		const kdal_token_t *name_tok = p_cur(p);
		if (name_tok->type != TOK_IDENT) {
			kdal_error(p->filename, line, col,
				   "expected identifier after 'let'");
			p->had_error = 1;
			return NULL;
		}
		const char *name = p_intern(p, name_tok);
		p_advance(p);
		/* optional : type */
		if (p_cur(p)->type == TOK_COLON)
			p_advance(p), p_advance(p); /* skip type for now */
		p_expect(p, TOK_EQ);
		kdal_ast_t *val = parse_expr(p);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_LET_STMT, line, col);
		/* store name as first child (as a fake IDENT expr), val as second */
		kdal_ast_t *nm =
			kdal_ast_new(p->arena, KDAL_NODE_EXPR_IDENT, line, col);
		((kdal_expr_node_t *)nm)->u.sval = name;
		kdal_ast_add_child(node, nm);
		kdal_ast_add_child(node, val);
		return node;
	}

	case TOK_KW_WRITE: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *reg = parse_expr(p);
		p_expect(p, TOK_COMMA);
		kdal_ast_t *val = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_REG_WRITE_STMT, line,
				    col);
		kdal_ast_add_child(node, reg);
		kdal_ast_add_child(node, val);
		return node;
	}

	case TOK_KW_EMIT: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *sig = parse_expr(p);
		kdal_ast_t *val = NULL;
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

	case TOK_KW_WAIT: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *sig = parse_expr(p);
		p_expect(p, TOK_COMMA);
		kdal_ast_t *ms = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_WAIT_STMT, line, col);
		kdal_ast_add_child(node, sig);
		kdal_ast_add_child(node, ms);
		return node;
	}

	case TOK_KW_ARM: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *timer = parse_expr(p);
		p_expect(p, TOK_COMMA);
		kdal_ast_t *ms = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_ARM_STMT, line, col);
		kdal_ast_add_child(node, timer);
		kdal_ast_add_child(node, ms);
		return node;
	}

	case TOK_KW_CANCEL: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		kdal_ast_t *timer = parse_expr(p);
		p_expect(p, TOK_RPAREN);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_CANCEL_STMT, line, col);
		kdal_ast_add_child(node, timer);
		return node;
	}

	case TOK_KW_RETURN: {
		p_advance(p);
		kdal_ast_t *val = NULL;
		if (p_cur(p)->type != TOK_SEMICOLON)
			val = parse_expr(p);
		p_expect(p, TOK_SEMICOLON);
		node = kdal_ast_new(p->arena, KDAL_NODE_RETURN_STMT, line, col);
		if (val)
			kdal_ast_add_child(node, val);
		return node;
	}

	case TOK_KW_LOG: {
		p_advance(p);
		p_expect(p, TOK_LPAREN);
		node = kdal_ast_new(p->arena, KDAL_NODE_LOG_STMT, line, col);
		kdal_ast_t *arg;
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

	case TOK_KW_IF: {
		node = kdal_ast_new(p->arena, KDAL_NODE_IF_STMT, line, col);
		while (p_cur(p)->type == TOK_KW_IF ||
		       p_cur(p)->type == TOK_KW_ELIF) {
			p_advance(p);
			p_expect(p, TOK_LPAREN);
			kdal_ast_t *cond = parse_expr(p);
			p_expect(p, TOK_RPAREN);
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *body = parse_statement_list(p);
			p_expect(p, TOK_RBRACE);
			kdal_ast_add_child(node, cond);
			kdal_ast_add_child(
				node,
				body ? body :
				       kdal_ast_new(p->arena, KDAL_NODE_INVALID,
						    line, col));
			if (p_cur(p)->type != TOK_KW_ELIF)
				break;
		}
		if (p_cur(p)->type == TOK_KW_ELSE) {
			p_advance(p);
			p_expect(p, TOK_LBRACE);
			kdal_ast_t *body = parse_statement_list(p);
			p_expect(p, TOK_RBRACE);
			kdal_ast_add_child(
				node,
				body ? body :
				       kdal_ast_new(p->arena, KDAL_NODE_INVALID,
						    line, col));
		}
		return node;
	}

	case TOK_KW_FOR: {
		p_advance(p);
		const char *var = p_intern(p, p_cur(p));
		p_advance(p); /* ident */
		p_expect(p, TOK_KW_IN);
		kdal_ast_t *lo = parse_expr(p);
		p_expect(p, TOK_DOTDOT);
		kdal_ast_t *hi = parse_expr(p);
		p_expect(p, TOK_LBRACE);
		kdal_ast_t *body = parse_statement_list(p);
		p_expect(p, TOK_RBRACE);
		node = kdal_ast_new(p->arena, KDAL_NODE_FOR_STMT, line, col);
		kdal_ast_t *vn =
			kdal_ast_new(p->arena, KDAL_NODE_EXPR_IDENT, line, col);
		((kdal_expr_node_t *)vn)->u.sval = var;
		kdal_ast_add_child(node, vn);
		kdal_ast_add_child(node, lo);
		kdal_ast_add_child(node, hi);
		if (body)
			kdal_ast_add_child(node, body);
		return node;
	}

	default:
		/* Fallback: treat as assignment: lvalue = expr ; */
		{
			kdal_ast_t *lval = parse_expr(p);
			if (p_cur(p)->type == TOK_EQ) {
				p_advance(p);
				kdal_ast_t *rval = parse_expr(p);
				p_expect(p, TOK_SEMICOLON);
				node = kdal_ast_new(p->arena,
						    KDAL_NODE_ASSIGN_STMT, line,
						    col);
				kdal_ast_add_child(node, lval);
				kdal_ast_add_child(node, rval);
				return node;
			}
			/* Not a recognised statement */
			kdal_error(p->filename, line, col,
				   "unexpected token '%s' in statement",
				   kdal_tok_name(t->type));
			p->had_error = 1;
			return NULL;
		}
	}
}

/* ── Top-level parser ────────────────────────────────────────────── */

kdal_file_node_t *kdal_parse(kdal_arena_t *arena, const kdal_token_t *tokens,
			     int ntokens, const char *filename)
{
	parser_t p = {
		.tokens = tokens,
		.ntokens = ntokens,
		.pos = 0,
		.arena = arena,
		.filename = filename,
	};

	kdal_file_node_t *file = kdal_arena_alloc(arena, sizeof(*file));
	file->base.type = KDAL_NODE_FILE;
	file->filename = filename;

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
		p_expect(&p, TOK_SEMICOLON);
		file->backend = &be->base;
	}

	/* device class or driver */
	if (p_cur(&p)->type == TOK_KW_DEVICE) {
		int line = p_cur(&p)->line, col = p_cur(&p)->col;
		p_advance(&p);
		p_expect(&p, TOK_KW_CLASS);
		kdal_device_node_t *dev = kdal_arena_alloc(arena, sizeof(*dev));
		dev->base.type = KDAL_NODE_DEVICE_CLASS;
		dev->base.line = line;
		dev->base.col = col;
		dev->name = p_intern(&p, p_cur(&p));
		p_advance(&p);
		p_expect(&p, TOK_LBRACE);
		while (p_cur(&p)->type != TOK_RBRACE &&
		       p_cur(&p)->type != TOK_EOF) {
			/* skip blocks — reserved for sema pass in full implementation */
			p_advance(&p);
		}
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
