/*
 * simulate.c - kdality simulate subcommand.
 *
 * Dry-run interpreter that traces .kdc driver execution without a kernel.
 * Simulates register reads/writes to a virtual register file and prints
 * a step-by-step execution trace.
 *
 *   kdality simulate <file.kdc> [--trace] [--event <name>]
 *
 * This lets newcomers see what their driver does before compiling and
 * loading a kernel module.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/include/codegen.h"

/* ── Virtual register file ───────────────────────────────────────── */

#define MAX_VREGS 64
#define NAME_LEN 64

struct vreg {
	char name[NAME_LEN];
	unsigned long value;
};

struct sim_state {
	struct vreg regs[MAX_VREGS];
	int nregs;
	int trace;
	int step;
};

static unsigned long sim_reg_read(struct sim_state *s, const char *name)
{
	for (int i = 0; i < s->nregs; i++) {
		if (strcmp(s->regs[i].name, name) == 0) {
			if (s->trace)
				printf("  [%d] reg_read(%s) → 0x%lx\n",
				       s->step++, name, s->regs[i].value);
			return s->regs[i].value;
		}
	}
	/* Unknown register - create with zero */
	if (s->nregs < MAX_VREGS) {
		strncpy(s->regs[s->nregs].name, name, NAME_LEN - 1);
		s->regs[s->nregs].value = 0;
		s->nregs++;
	}
	if (s->trace)
		printf("  [%d] reg_read(%s) → 0x0 (new)\n", s->step++, name);
	return 0;
}

static void sim_reg_write(struct sim_state *s, const char *name,
			  unsigned long val)
{
	for (int i = 0; i < s->nregs; i++) {
		if (strcmp(s->regs[i].name, name) == 0) {
			if (s->trace)
				printf("  [%d] reg_write(%s, 0x%lx) "
				       "[was 0x%lx]\n",
				       s->step++, name, val, s->regs[i].value);
			s->regs[i].value = val;
			return;
		}
	}
	if (s->nregs < MAX_VREGS) {
		strncpy(s->regs[s->nregs].name, name, NAME_LEN - 1);
		s->regs[s->nregs].value = val;
		s->nregs++;
		if (s->trace)
			printf("  [%d] reg_write(%s, 0x%lx) [new]\n", s->step++,
			       name, val);
	}
}

static void sim_log(struct sim_state *s, const char *msg)
{
	if (s->trace)
		printf("  [%d] log(\"%s\")\n", s->step++, msg);
	else
		printf("  LOG: %s\n", msg);
}

/* ── File reader ─────────────────────────────────────────────────── */

static char *read_source(const char *path, size_t *out_len)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return NULL;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (sz < 0) {
		fclose(fp);
		return NULL;
	}
	char *buf = malloc((size_t)sz + 1);
	if (!buf) {
		fclose(fp);
		return NULL;
	}
	size_t n = fread(buf, 1, (size_t)sz, fp);
	fclose(fp);
	buf[n] = '\0';
	if (out_len)
		*out_len = n;
	return buf;
}

/* ── Expression evaluator ────────────────────────────────────────── */

static const char *expr_reg_name(const kdal_ast_t *expr)
{
	if (!expr)
		return "?";
	const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
	if (expr->type == KDAL_NODE_EXPR_REG_PATH)
		return e->u.path.parts[e->u.path.nparts - 1];
	if (expr->type == KDAL_NODE_EXPR_IDENT)
		return e->u.sval;
	return "?";
}

static unsigned long sim_eval(struct sim_state *s, const kdal_ast_t *expr)
{
	if (!expr)
		return 0;
	const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;

	switch (expr->type) {
	case KDAL_NODE_EXPR_LITERAL_INT:
		return (unsigned long)e->u.ival;
	case KDAL_NODE_EXPR_LITERAL_BOOL:
		return e->u.bval ? 1 : 0;
	case KDAL_NODE_EXPR_LITERAL_STR:
		return 0;
	case KDAL_NODE_EXPR_REG_PATH:
		return sim_reg_read(s, e->u.path.parts[e->u.path.nparts - 1]);
	case KDAL_NODE_EXPR_IDENT:
		return sim_reg_read(s, e->u.sval);
	case KDAL_NODE_EXPR_READ:
		return sim_reg_read(s, expr_reg_name(e->u.read_path));
	case KDAL_NODE_EXPR_BINOP: {
		unsigned long l = sim_eval(s, e->u.binop.lhs);
		unsigned long r = sim_eval(s, e->u.binop.rhs);
		switch (e->u.binop.op) {
		case KDAL_BINOP_ADD:
			return l + r;
		case KDAL_BINOP_SUB:
			return l - r;
		case KDAL_BINOP_MUL:
			return l * r;
		case KDAL_BINOP_DIV:
			return r ? l / r : 0;
		case KDAL_BINOP_MOD:
			return r ? l % r : 0;
		case KDAL_BINOP_SHL:
			return l << r;
		case KDAL_BINOP_SHR:
			return l >> r;
		case KDAL_BINOP_AND:
			return l & r;
		case KDAL_BINOP_OR:
			return l | r;
		case KDAL_BINOP_XOR:
			return l ^ r;
		case KDAL_BINOP_EQ:
			return l == r;
		case KDAL_BINOP_NEQ:
			return l != r;
		case KDAL_BINOP_LT:
			return l < r;
		case KDAL_BINOP_LE:
			return l <= r;
		case KDAL_BINOP_GT:
			return l > r;
		case KDAL_BINOP_GE:
			return l >= r;
		case KDAL_BINOP_LAND:
			return l && r;
		case KDAL_BINOP_LOR:
			return l || r;
		}
		return 0;
	}
	case KDAL_NODE_EXPR_UNOP: {
		unsigned long v = sim_eval(s, e->u.unop.operand);
		switch (e->u.unop.op) {
		case KDAL_UNOP_NEG:
			return (unsigned long)(-(long)v);
		case KDAL_UNOP_NOT:
			return !v;
		case KDAL_UNOP_INV:
			return ~v;
		}
		return 0;
	}
	default:
		return 0;
	}
}

/* ── AST statement interpreter ───────────────────────────────────── */

static void sim_exec(struct sim_state *s, const kdal_ast_t *stmt)
{
	for (const kdal_ast_t *st = stmt; st; st = st->next) {
		switch (st->type) {
		case KDAL_NODE_REG_WRITE_STMT: {
			const kdal_ast_t *target = st->child;
			const kdal_ast_t *value = target ? target->next : NULL;
			sim_reg_write(s, expr_reg_name(target),
				      sim_eval(s, value));
			break;
		}
		case KDAL_NODE_ASSIGN_STMT: {
			const kdal_ast_t *lhs = st->child;
			const kdal_ast_t *rhs = lhs ? lhs->next : NULL;
			sim_reg_write(s, expr_reg_name(lhs), sim_eval(s, rhs));
			break;
		}
		case KDAL_NODE_LET_STMT: {
			const kdal_ast_t *var = st->child;
			const kdal_ast_t *init = var ? var->next : NULL;
			unsigned long val = sim_eval(s, init);
			if (s->trace && var)
				printf("  [%d] let %s = 0x%lx\n", s->step++,
				       expr_reg_name(var), val);
			break;
		}
		case KDAL_NODE_LOG_STMT:
			if (st->child &&
			    st->child->type == KDAL_NODE_EXPR_LITERAL_STR) {
				const kdal_expr_node_t *e =
					(const kdal_expr_node_t *)st->child;
				sim_log(s, e->u.sval);
			} else {
				sim_log(s, "(dynamic)");
			}
			break;
		case KDAL_NODE_EMIT_STMT:
			if (s->trace)
				printf("  [%d] emit %s\n", s->step++,
				       expr_reg_name(st->child));
			break;
		case KDAL_NODE_WAIT_STMT:
			if (s->trace)
				printf("  [%d] wait()\n", s->step++);
			break;
		case KDAL_NODE_ARM_STMT:
			if (s->trace)
				printf("  [%d] arm()\n", s->step++);
			break;
		case KDAL_NODE_CANCEL_STMT:
			if (s->trace)
				printf("  [%d] cancel()\n", s->step++);
			break;
		case KDAL_NODE_RETURN_STMT:
			if (s->trace)
				printf("  [%d] return\n", s->step++);
			return;
		case KDAL_NODE_IF_STMT: {
			const kdal_ast_t *c = st->child;
			while (c) {
				unsigned long cond = sim_eval(s, c);
				const kdal_ast_t *body = c->next;
				if (cond && body) {
					sim_exec(s, body);
					break;
				}
				c = body ? body->next : NULL;
			}
			break;
		}
		case KDAL_NODE_FOR_STMT: {
			const kdal_ast_t *var = st->child;
			const kdal_ast_t *lo = var ? var->next : NULL;
			const kdal_ast_t *hi = lo ? lo->next : NULL;
			const kdal_ast_t *body = hi ? hi->next : NULL;
			unsigned long lo_val = sim_eval(s, lo);
			unsigned long hi_val = sim_eval(s, hi);
			for (unsigned long i = lo_val;
			     i <= hi_val && i <= lo_val + 10000; i++) {
				if (s->trace)
					printf("  [%d] for iteration %lu\n",
					       s->step++, i);
				sim_exec(s, body);
			}
			break;
		}
		default:
			if (st->child)
				sim_exec(s, st->child);
			break;
		}
	}
}

/* ── Handler name ────────────────────────────────────────────────── */

static const char *handler_label(const kdal_handler_node_t *h)
{
	switch (h->evt_type) {
	case KDAL_EVT_READ:
		return "read";
	case KDAL_EVT_WRITE:
		return "write";
	case KDAL_EVT_SIGNAL:
		return "signal";
	case KDAL_EVT_POWER:
		return "power";
	case KDAL_EVT_TIMEOUT:
		return "timeout";
	}
	return "unknown";
}

/* ── Simulate a parsed .kdc file ─────────────────────────────────── */

static int simulate_file(const char *path, const char *target_event, int trace)
{
	size_t src_len;
	char *src = read_source(path, &src_len);
	if (!src) {
		fprintf(stderr, "simulate: cannot open '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	kdal_arena_t *arena = kdal_arena_new(65536);

	/* Copy source into arena so token src pointers stay valid */
	char *arena_src = kdal_arena_alloc(arena, src_len + 1);
	memcpy(arena_src, src, src_len + 1);
	free(src);

	kdal_token_t *tokens = NULL;
	int ntokens = kdal_lex(arena, arena_src, src_len, &tokens, path);

	if (ntokens < 0) {
		fprintf(stderr, "simulate: lexer error in '%s'\n", path);
		kdal_arena_free(arena);
		return 1;
	}

	kdal_file_node_t *file = kdal_parse(arena, tokens, ntokens, path);
	if (!file || !file->driver) {
		fprintf(stderr, "simulate: no driver found in '%s'\n", path);
		kdal_arena_free(arena);
		return 1;
	}

	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;
	struct sim_state state;
	int found = 0;

	memset(&state, 0, sizeof(state));
	state.trace = trace;

	printf("=== KDAL Simulation: %s ===\n", path);
	printf("Driver: %s (device: %s)\n", drv->name,
	       drv->device_class ? drv->device_class : "?");

	/* Simulate probe handler */
	if (drv->probe && drv->probe->child) {
		if (!target_event || strcmp(target_event, "probe") == 0) {
			printf("\n── Simulating: probe ──\n");
			state.step = 0;
			sim_exec(&state, drv->probe->child);
			found = 1;
		}
	}

	/* Simulate remove handler */
	if (drv->remove && drv->remove->child) {
		if (!target_event || strcmp(target_event, "remove") == 0) {
			printf("\n── Simulating: remove ──\n");
			state.step = 0;
			sim_exec(&state, drv->remove->child);
			found = 1;
		}
	}

	/* Simulate event handlers */
	for (const kdal_ast_t *h = drv->handlers; h; h = h->next) {
		const kdal_handler_node_t *ev = (const kdal_handler_node_t *)h;
		const char *label = handler_label(ev);
		if (target_event && strcmp(target_event, label) != 0)
			continue;
		printf("\n── Simulating: on %s ──\n", label);
		state.step = 0;
		sim_exec(&state, ev->body);
		found = 1;
	}

	if (!found && target_event) {
		fprintf(stderr, "simulate: handler 'on %s' not found in '%s'\n",
			target_event, path);
		kdal_arena_free(arena);
		return 1;
	}

	/* Print final register state */
	if (state.nregs > 0) {
		printf("\n── Final register state ──\n");
		for (int i = 0; i < state.nregs; i++) {
			printf("  %-16s = 0x%lx\n", state.regs[i].name,
			       state.regs[i].value);
		}
	}

	printf("\n=== Simulation complete ===\n");
	kdal_arena_free(arena);
	return 0;
}

/* ── help ────────────────────────────────────────────────────────── */

static void simulate_help(void)
{
	fprintf(stderr,
		"Usage: kdality simulate <file.kdc> [options]\n\n"
		"Dry-run a .kdc driver without a kernel. Shows register\n"
		"read/write traces and final register state.\n\n"
		"Options:\n"
		"  --trace            show step-by-step execution trace\n"
		"  --event <name>     simulate only this handler (e.g. probe, read)\n"
		"  -h, --help         show this help\n\n"
		"Example:\n"
		"  kdality simulate examples/kdc_hello/uart_hello.kdc --trace\n"
		"  kdality simulate examples/kdc_hello/uart_hello.kdc --event probe\n");
}

int kdality_simulate(int argc, char *const argv[])
{
	const char *input = NULL;
	const char *target_event = NULL;
	int trace = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			simulate_help();
			return 0;
		} else if (strcmp(argv[i], "--trace") == 0) {
			trace = 1;
		} else if (strcmp(argv[i], "--event") == 0 && i + 1 < argc) {
			target_event = argv[++i];
		} else if (argv[i][0] != '-') {
			input = argv[i];
		}
	}

	if (!input) {
		fprintf(stderr, "simulate: no input .kdc file\n\n");
		simulate_help();
		return 1;
	}

	return simulate_file(input, target_event, trace);
}
