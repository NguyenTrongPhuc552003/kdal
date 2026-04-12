// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler - sema.c
 * Semantic analysis pass: type checking + register access verification.
 *
 * Runs after parsing on the annotated AST produced by parser.c.
 * Emits errors/warnings to stderr and returns 0 on success.
 */

#include <stdio.h>
#include <string.h>

#include "include/token.h"
#include "include/ast.h"
#include "include/codegen.h"

/* ── Symbol table ────────────────────────────────────────────────── */

#define SYMTAB_MAX 256

typedef enum {
	SYM_REGISTER,
	SYM_SIGNAL,
	SYM_CAPABILITY,
	SYM_CONFIG,
	SYM_LOCAL_VAR,
} sym_kind_t;

typedef struct {
	const char *name;
	sym_kind_t kind;
	int width; /* bit width for SYM_REGISTER */
	kdal_access_t access; /* for SYM_REGISTER */
} sym_t;

typedef struct {
	sym_t entries[SYMTAB_MAX];
	int count;
} symtab_t;

static void symtab_push(symtab_t *st, const char *name, sym_kind_t kind,
			int width, kdal_access_t access)
{
	if (st->count >= SYMTAB_MAX)
		return;
	sym_t *s = &st->entries[st->count++];
	s->name = name;
	s->kind = kind;
	s->width = width;
	s->access = access;
}

static const sym_t *symtab_find(const symtab_t *st, const char *name)
{
	for (int i = 0; i < st->count; i++)
		if (strcmp(st->entries[i].name, name) == 0)
			return &st->entries[i];
	return NULL;
}

/* ── Sema context ────────────────────────────────────────────────── */

typedef struct {
	const char *filename;
	symtab_t globals; /* registers, signals, caps from .kdh */
	symtab_t locals; /* let bindings in current handler */
	int errors;
	int in_write_handler; /* detect blocking wait() in on_write */
} sema_t;

/* ── Helpers ─────────────────────────────────────────────────────── */

static void sema_check_expr(sema_t *sema, const kdal_ast_t *expr,
			    int expect_writable)
{
	if (!expr)
		return;

	switch (expr->type) {
	case KDAL_NODE_EXPR_REG_PATH: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		/* Find the base name in globals */
		const sym_t *sym = symtab_find(
			&sema->globals, e->u.path.parts[e->u.path.nparts - 1]);
		if (!sym) {
			/* Try the first component (namespace.register) */
			if (e->u.path.nparts >= 2)
				sym = symtab_find(&sema->globals,
						  e->u.path.parts[1]);
		}
		if (sym && sym->kind == SYM_REGISTER && expect_writable) {
			if (sym->access == KDAL_ACCESS_RO) {
				kdal_error(
					sema->filename, expr->line, expr->col,
					"register '%s' is declared 'ro' - cannot write",
					e->u.path.parts[e->u.path.nparts - 1]);
				sema->errors++;
			}
		}
		if (sym && sym->kind == SYM_REGISTER && !expect_writable) {
			if (sym->access == KDAL_ACCESS_WO) {
				kdal_error(
					sema->filename, expr->line, expr->col,
					"register '%s' is declared 'wo' - cannot read",
					e->u.path.parts[e->u.path.nparts - 1]);
				sema->errors++;
			}
		}
		break;
	}
	case KDAL_NODE_EXPR_BINOP: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		sema_check_expr(sema, e->u.binop.lhs, 0);
		sema_check_expr(sema, e->u.binop.rhs, 0);
		break;
	}
	case KDAL_NODE_EXPR_UNOP: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		sema_check_expr(sema, e->u.unop.operand, 0);
		break;
	}
	case KDAL_NODE_EXPR_READ: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		sema_check_expr(sema, e->u.read_path, 0 /* reading */);
		break;
	}
	default:
		break;
	}
}

static void sema_check_stmts(sema_t *sema, const kdal_ast_t *stmt)
{
	for (const kdal_ast_t *s = stmt; s; s = s->next) {
		switch (s->type) {
		case KDAL_NODE_REG_WRITE_STMT: {
			/* child[0] = reg path, child[1] = value */
			sema_check_expr(sema, s->child, 1 /* writable */);
			if (s->child)
				sema_check_expr(sema, s->child->next, 0);
			break;
		}
		case KDAL_NODE_WAIT_STMT: {
			if (sema->in_write_handler) {
				kdal_warn(
					sema->filename, s->line, s->col,
					"wait() inside on_write handler may block in IRQ context");
			}
			break;
		}
		case KDAL_NODE_FOR_STMT: {
			/* child[0]=var, child[1]=lo, child[2]=hi, child[3]=body */
			/* check lo and hi are compile-time integers */
			const kdal_ast_t *lo = s->child ? s->child->next : NULL;
			const kdal_ast_t *hi = lo ? lo->next : NULL;
			if (lo && lo->type != KDAL_NODE_EXPR_LITERAL_INT &&
			    lo->type != KDAL_NODE_EXPR_LITERAL_BOOL) {
				kdal_warn(
					sema->filename, s->line, s->col,
					"for-loop lower bound may not be a compile-time constant");
			}
			if (hi && hi->type != KDAL_NODE_EXPR_LITERAL_INT &&
			    hi->type != KDAL_NODE_EXPR_LITERAL_BOOL) {
				kdal_warn(
					sema->filename, s->line, s->col,
					"for-loop upper bound may not be a compile-time constant");
			}
			const kdal_ast_t *body = hi ? hi->next : NULL;
			if (body)
				sema_check_stmts(sema, body);
			break;
		}
		case KDAL_NODE_IF_STMT: {
			/* alternate cond/body pairs as children */
			for (const kdal_ast_t *c = s->child; c;
			     c = c->next ? c->next->next : NULL) {
				sema_check_expr(sema, c, 0);
				if (c->next)
					sema_check_stmts(sema, c->next);
			}
			break;
		}
		case KDAL_NODE_ASSIGN_STMT: {
			sema_check_expr(sema, s->child, 1);
			if (s->child)
				sema_check_expr(sema, s->child->next, 0);
			break;
		}
		default:
			if (s->child)
				sema_check_stmts(sema, s->child);
			break;
		}
	}
}

/* ── Device class validation ─────────────────────────────────────── */

/* Populate global symbol table from device class registers/signals/etc. */
static void sema_populate_device(sema_t *sema, const kdal_device_node_t *dev)
{
	/* Walk registers */
	if (dev->registers) {
		for (const kdal_ast_t *r = dev->registers->child; r;
		     r = r->next) {
			const kdal_reg_decl_node_t *reg =
				(const kdal_reg_decl_node_t *)r;
			/* Check for duplicate */
			if (symtab_find(&sema->globals, reg->name)) {
				kdal_error(sema->filename, r->line, r->col,
					   "duplicate register name '%s'",
					   reg->name);
				sema->errors++;
				continue;
			}
			symtab_push(&sema->globals, reg->name, SYM_REGISTER,
				    reg->width, reg->access);

			/* Check offset overlaps with prior registers */
			if (reg->offset != UINT64_MAX &&
			    dev->registers->child) {
				for (const kdal_ast_t *p =
					     dev->registers->child;
				     p != r; p = p->next) {
					const kdal_reg_decl_node_t *pr =
						(const kdal_reg_decl_node_t *)p;
					if (pr->offset == UINT64_MAX)
						continue;
					int pw = pr->width ? pr->width / 8 : 4;
					int rw = reg->width ? reg->width / 8 :
							      4;
					uint64_t pend =
						pr->offset + (uint64_t)pw;
					uint64_t rend =
						reg->offset + (uint64_t)rw;
					if (reg->offset < pend &&
					    pr->offset < rend) {
						kdal_warn(
							sema->filename, r->line,
							r->col,
							"register '%s' @ 0x%llx overlaps with '%s' @ 0x%llx",
							reg->name,
							(unsigned long long)
								reg->offset,
							pr->name,
							(unsigned long long)
								pr->offset);
					}
				}
			}

			/* Validate bitfield members */
			if (reg->is_bitfield && reg->members) {
				for (const kdal_ast_t *m = reg->members; m;
				     m = m->next) {
					const kdal_bf_member_node_t *bf =
						(const kdal_bf_member_node_t *)m;
					if (bf->bit_lo > bf->bit_hi) {
						kdal_error(
							sema->filename, m->line,
							m->col,
							"bitfield '%s' has invalid range %d..%d",
							bf->name, bf->bit_lo,
							bf->bit_hi);
						sema->errors++;
					}
				}
			}
		}
	}

	/* Walk signals */
	if (dev->signals) {
		for (const kdal_ast_t *s = dev->signals->child; s;
		     s = s->next) {
			const kdal_signal_node_t *sig =
				(const kdal_signal_node_t *)s;
			if (symtab_find(&sema->globals, sig->name)) {
				kdal_error(sema->filename, s->line, s->col,
					   "duplicate signal name '%s'",
					   sig->name);
				sema->errors++;
				continue;
			}
			symtab_push(&sema->globals, sig->name, SYM_SIGNAL, 0,
				    KDAL_ACCESS_RW);
		}
	}

	/* Walk capabilities */
	if (dev->capabilities) {
		for (const kdal_ast_t *c = dev->capabilities->child; c;
		     c = c->next) {
			const kdal_cap_node_t *cap = (const kdal_cap_node_t *)c;
			if (symtab_find(&sema->globals, cap->name)) {
				kdal_warn(sema->filename, c->line, c->col,
					  "duplicate capability '%s'",
					  cap->name);
			}
			symtab_push(&sema->globals, cap->name, SYM_CAPABILITY,
				    0, KDAL_ACCESS_RW);
		}
	}

	/* Walk config fields */
	if (dev->config) {
		for (const kdal_ast_t *f = dev->config->child; f; f = f->next) {
			const kdal_config_field_node_t *cf =
				(const kdal_config_field_node_t *)f;
			if (symtab_find(&sema->globals, cf->name)) {
				kdal_error(sema->filename, f->line, f->col,
					   "duplicate config field '%s'",
					   cf->name);
				sema->errors++;
				continue;
			}
			symtab_push(&sema->globals, cf->name, SYM_CONFIG, 0,
				    KDAL_ACCESS_RW);
		}
	}
}

/* ── Public entry point ──────────────────────────────────────────── */

int kdal_sema(kdal_file_node_t *file, const char *filename)
{
	sema_t sema = { .filename = filename };

	/* Build global symbol table from device class (if present) */
	if (file->device) {
		const kdal_device_node_t *dev =
			(const kdal_device_node_t *)file->device;
		sema_populate_device(&sema, dev);
	}

	if (!file->driver)
		return 0; /* .kdh only - nothing to check */

	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;

	/* Check probe handler */
	if (drv->probe) {
		sema.in_write_handler = 0;
		sema_check_stmts(&sema, drv->probe->child);
	}

	/* Check remove handler */
	if (drv->remove) {
		sema.in_write_handler = 0;
		sema_check_stmts(&sema, drv->remove->child);
	}

	/* Check event handlers */
	for (const kdal_ast_t *h = drv->handlers; h; h = h->next) {
		const kdal_handler_node_t *ev = (const kdal_handler_node_t *)h;
		sema.in_write_handler = (ev->evt_type == KDAL_EVT_WRITE);
		memset(&sema.locals, 0, sizeof(sema.locals));
		sema_check_stmts(&sema, ev->body);
	}

	return sema.errors > 0 ? -sema.errors : 0;
}
