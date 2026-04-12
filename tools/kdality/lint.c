/*
 * lint.c - kdality lint subcommand.
 *
 * Static analysis for .kdc and .kdh files. Checks for common mistakes
 * and anti-patterns that newcomers often make.
 *
 *   kdality lint <file.kdc|file.kdh> [--strict]
 *
 * Rules:
 *   W001  Missing probe handler (every driver should handle probe)
 *   W002  Missing remove handler (resource leak risk)
 *   W003  Write to read-only register
 *   W004  Read from write-only register
 *   W005  Empty handler body
 *   W006  Missing use statement (no device header imported)
 *   W007  Unused register (declared in .kdh but never accessed)
 *   W008  Missing power handler (no power management)
 *   W009  Magic number in reg_write (use named constants)
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/include/codegen.h"

#define NAME_LEN 64
#define MAX_REGS 32
#define MAX_WARNS 128

/* ── Warning infrastructure ──────────────────────────────────────── */

struct lint_warning {
	int line;
	char code[8];
	char message[256];
};

struct lint_ctx {
	const char *filename;
	struct lint_warning warnings[MAX_WARNS];
	int nwarns;
	int strict;

	/* Register info from device class (for cross-file checks) */
	char regs[MAX_REGS][NAME_LEN];
	char reg_modes[MAX_REGS][4]; /* ro, wo, rw */
	int nregs;
	int reg_used[MAX_REGS];
};

static void emit_warning(struct lint_ctx *ctx, int line, const char *code,
			 const char *fmt, ...)
{
	if (ctx->nwarns >= MAX_WARNS)
		return;

	struct lint_warning *w = &ctx->warnings[ctx->nwarns];
	w->line = line;
	strncpy(w->code, code, sizeof(w->code) - 1);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(w->message, sizeof(w->message), fmt, ap);
	va_end(ap);

	ctx->nwarns++;
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

/* ── Parse helper ────────────────────────────────────────────────── */

static kdal_file_node_t *lint_parse_file(const char *path,
					 kdal_arena_t **out_arena)
{
	size_t src_len;
	char *src = read_source(path, &src_len);
	if (!src) {
		fprintf(stderr, "lint: cannot open '%s': %s\n", path,
			strerror(errno));
		return NULL;
	}

	kdal_arena_t *arena = kdal_arena_new(65536);

	/* Copy source into arena so token src pointers stay valid */
	char *arena_src = kdal_arena_alloc(arena, src_len + 1);
	memcpy(arena_src, src, src_len + 1);
	free(src);

	kdal_token_t *tokens = NULL;
	int ntokens = kdal_lex(arena, arena_src, src_len, &tokens, path);

	if (ntokens < 0) {
		kdal_arena_free(arena);
		return NULL;
	}

	kdal_file_node_t *file = kdal_parse(arena, tokens, ntokens, path);
	if (!file) {
		kdal_arena_free(arena);
		return NULL;
	}

	*out_arena = arena;
	return file;
}

/* ── KDH lint pass (AST-based) ───────────────────────────────────── */

static int lint_kdh(struct lint_ctx *ctx, const char *path)
{
	kdal_arena_t *arena = NULL;
	kdal_file_node_t *file = lint_parse_file(path, &arena);
	if (!file) {
		emit_warning(ctx, 1, "E001", "failed to parse '%s'", path);
		return -1;
	}

	if (!file->device) {
		emit_warning(ctx, 1, "E001", "no 'device' declaration found");
		kdal_arena_free(arena);
		return 0;
	}

	const kdal_device_node_t *dev =
		(const kdal_device_node_t *)file->device;

	if (!dev->compatible && ctx->strict)
		emit_warning(ctx, file->device->line, "W010",
			     "no 'compatible' string (needed for Device Tree)");

	/* Populate register map for cross-file checking */
	if (dev->registers) {
		for (const kdal_ast_t *r = dev->registers->child; r;
		     r = r->next) {
			if (ctx->nregs >= MAX_REGS)
				break;
			const kdal_reg_decl_node_t *reg =
				(const kdal_reg_decl_node_t *)r;
			strncpy(ctx->regs[ctx->nregs], reg->name, NAME_LEN - 1);
			switch (reg->access) {
			case KDAL_ACCESS_RO:
				strncpy(ctx->reg_modes[ctx->nregs], "ro", 3);
				break;
			case KDAL_ACCESS_WO:
				strncpy(ctx->reg_modes[ctx->nregs], "wo", 3);
				break;
			default:
				strncpy(ctx->reg_modes[ctx->nregs], "rw", 3);
				break;
			}
			ctx->nregs++;
		}
	}

	/* Run semantic analysis for structural checks */
	kdal_sema(file, path);

	kdal_arena_free(arena);
	return 0;
}

/* ── Statement walker (detect reg access and magic numbers) ──────── */

static void lint_walk_stmts(struct lint_ctx *ctx, const kdal_ast_t *stmt)
{
	for (const kdal_ast_t *s = stmt; s; s = s->next) {
		if (s->type == KDAL_NODE_REG_WRITE_STMT) {
			/* Check W003: write to read-only register */
			if (s->child &&
			    s->child->type == KDAL_NODE_EXPR_REG_PATH) {
				const kdal_expr_node_t *e =
					(const kdal_expr_node_t *)s->child;
				const char *name =
					e->u.path.parts[e->u.path.nparts - 1];
				for (int i = 0; i < ctx->nregs; i++) {
					if (strcmp(ctx->regs[i], name) == 0) {
						ctx->reg_used[i] = 1;
						if (strcmp(ctx->reg_modes[i],
							   "ro") == 0)
							emit_warning(
								ctx, s->line,
								"W003",
								"write to read-only register '%s'",
								name);
					}
				}
			}
			/* Check W009: magic number in value */
			if (ctx->strict && s->child) {
				const kdal_ast_t *val = s->child->next;
				if (val &&
				    val->type == KDAL_NODE_EXPR_LITERAL_INT) {
					const kdal_expr_node_t *ve =
						(const kdal_expr_node_t *)val;
					if (ve->u.ival > 0xFF)
						emit_warning(
							ctx, s->line, "W009",
							"large magic number 0x%llx "
							"in reg_write - consider "
							"a named constant",
							(unsigned long long)
								ve->u.ival);
				}
			}
		}
		/* Check W004: read from write-only register */
		if (s->child) {
			for (const kdal_ast_t *c = s->child; c; c = c->next) {
				if (c->type == KDAL_NODE_EXPR_READ &&
				    c->child &&
				    c->child->type == KDAL_NODE_EXPR_REG_PATH) {
					const kdal_expr_node_t *re =
						(const kdal_expr_node_t *)
							c->child;
					const char *name =
						re->u.path.parts
							[re->u.path.nparts - 1];
					for (int i = 0; i < ctx->nregs; i++) {
						if (strcmp(ctx->regs[i],
							   name) == 0) {
							ctx->reg_used[i] = 1;
							if (strcmp(ctx->reg_modes
									   [i],
								   "wo") == 0)
								emit_warning(
									ctx,
									c->line,
									"W004",
									"read from write-only register '%s'",
									name);
						}
					}
				}
			}
		}
		/* Recurse into children */
		if (s->child)
			lint_walk_stmts(ctx, s->child);
	}
}

/* ── KDC lint pass (AST-based) ───────────────────────────────────── */

static int lint_kdc(struct lint_ctx *ctx, const char *path)
{
	kdal_arena_t *arena = NULL;
	kdal_file_node_t *file = lint_parse_file(path, &arena);
	if (!file) {
		emit_warning(ctx, 1, "E001", "failed to parse '%s'", path);
		return -1;
	}

	/* Try to load imported .kdh for register info */
	for (const kdal_ast_t *im = file->imports; im; im = im->next) {
		const kdal_import_node_t *imp = (const kdal_import_node_t *)im;
		if (imp->path) {
			/* Strip quotes from path if present */
			const char *p = imp->path;
			size_t plen = strlen(p);
			char clean[512];
			if (plen > 2 && p[0] == '"' && p[plen - 1] == '"') {
				memcpy(clean, p + 1, plen - 2);
				clean[plen - 2] = '\0';
			} else {
				strncpy(clean, p, sizeof(clean) - 1);
				clean[sizeof(clean) - 1] = '\0';
			}
			lint_kdh(ctx, clean);
		}
	}

	if (!file->driver) {
		emit_warning(ctx, 1, "E001", "no 'driver' declaration found");
		kdal_arena_free(arena);
		return 0;
	}

	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;

	/* W006: no import statement */
	if (!file->imports)
		emit_warning(ctx, 1, "W006",
			     "no 'use' statement - no device header imported");

	/* W001: missing probe */
	if (!drv->probe)
		emit_warning(ctx, 1, "W001", "missing 'on probe' handler");

	/* W002: missing remove */
	if (!drv->remove)
		emit_warning(ctx, 1, "W002",
			     "missing 'on remove' handler "
			     "(potential resource leak)");

	/* W005: empty probe handler */
	if (drv->probe && !drv->probe->child)
		emit_warning(ctx, drv->probe->line, "W005",
			     "empty handler body: on probe");

	/* W005: empty remove handler */
	if (drv->remove && !drv->remove->child)
		emit_warning(ctx, drv->remove->line, "W005",
			     "empty handler body: on remove");

	/* Walk event handlers */
	int has_power = 0;
	for (const kdal_ast_t *h = drv->handlers; h; h = h->next) {
		const kdal_handler_node_t *ev = (const kdal_handler_node_t *)h;
		if (ev->evt_type == KDAL_EVT_POWER)
			has_power = 1;
		/* W005: empty event handler */
		if (!ev->body)
			emit_warning(ctx, h->line, "W005",
				     "empty event handler body");
		else
			lint_walk_stmts(ctx, ev->body);
	}

	/* W008: no power handler */
	if (!has_power && ctx->strict)
		emit_warning(ctx, 1, "W008", "no power management handlers");

	/* Walk probe/remove for register access checks */
	if (drv->probe && drv->probe->child)
		lint_walk_stmts(ctx, drv->probe->child);
	if (drv->remove && drv->remove->child)
		lint_walk_stmts(ctx, drv->remove->child);

	/* W007: unused registers */
	for (int i = 0; i < ctx->nregs; i++) {
		if (!ctx->reg_used[i])
			emit_warning(ctx, 0, "W007",
				     "register '%s' declared but never used",
				     ctx->regs[i]);
	}

	kdal_arena_free(arena);
	return 0;
}

/* ── Output ──────────────────────────────────────────────────────── */

static void print_warnings(const struct lint_ctx *ctx)
{
	for (int i = 0; i < ctx->nwarns; i++) {
		const struct lint_warning *w = &ctx->warnings[i];
		if (w->line > 0)
			printf("%s:%d: %s: %s\n", ctx->filename, w->line,
			       w->code, w->message);
		else
			printf("%s: %s: %s\n", ctx->filename, w->code,
			       w->message);
	}
}

/* ── help ────────────────────────────────────────────────────────── */

static void lint_help(void)
{
	fprintf(stderr, "Usage: kdality lint <file.kdc|file.kdh> [options]\n\n"
			"Static analysis for KDAL source files.\n\n"
			"Options:\n"
			"  --strict    enable additional warnings\n"
			"  -h, --help  show this help\n\n"
			"Warnings:\n"
			"  W001  Missing probe handler\n"
			"  W002  Missing remove handler\n"
			"  W003  Write to read-only register\n"
			"  W004  Read from write-only register\n"
			"  W005  Empty handler body\n"
			"  W006  Missing use statement\n"
			"  W007  Unused register\n"
			"  W008  Missing power handler (--strict)\n"
			"  W009  Magic number in reg_write (--strict)\n");
}

int kdality_lint(int argc, char *const argv[])
{
	const char *input = NULL;
	int strict = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			lint_help();
			return 0;
		} else if (strcmp(argv[i], "--strict") == 0) {
			strict = 1;
		} else if (argv[i][0] != '-') {
			input = argv[i];
		}
	}

	if (!input) {
		fprintf(stderr, "lint: no input file\n\n");
		lint_help();
		return 1;
	}

	struct lint_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.filename = input;
	ctx.strict = strict;

	/* Determine file type */
	size_t len = strlen(input);
	int is_kdh = (len > 4 && strcmp(input + len - 4, ".kdh") == 0);
	int is_kdc = (len > 4 && strcmp(input + len - 4, ".kdc") == 0);

	if (!is_kdh && !is_kdc) {
		fprintf(stderr,
			"lint: unknown file type (expected .kdh or .kdc)\n");
		return 1;
	}

	int rc;
	if (is_kdh)
		rc = lint_kdh(&ctx, input);
	else
		rc = lint_kdc(&ctx, input);

	if (rc != 0)
		return 1;

	print_warnings(&ctx);

	if (ctx.nwarns == 0)
		printf("%s: OK (0 warnings)\n", input);
	else
		printf("\n%d warning(s)\n", ctx.nwarns);

	return (ctx.nwarns > 0) ? 1 : 0;
}
