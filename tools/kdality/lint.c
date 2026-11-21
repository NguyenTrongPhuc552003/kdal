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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../compiler/include/codegen.h"
#include "paths.h"

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
	(void)vsnprintf(w->message, sizeof(w->message), fmt, ap);
	va_end(ap);

	ctx->nwarns++;
}

/* ── KDH lint pass (AST-based) ───────────────────────────────────── */

static int lint_kdh(struct lint_ctx *ctx, const char *path)
{
	kdal_arena_t *arena = NULL;
	kdal_file_node_t *file = kdal_parse_file("lint", path, &arena);
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

/* The register a path names: DEV.CTRL, CTRL or a bitfield DEV.CTRL.enable.
 * The last part that is a register name wins; -1 when none is. */
static int reg_of_path(const struct lint_ctx *ctx, const kdal_expr_node_t *e)
{
	for (int p = (int)e->u.path.nparts - 1; p >= 0; p--) {
		for (int i = 0; i < ctx->nregs; i++) {
			if (strcmp(ctx->regs[i], e->u.path.parts[p]) == 0)
				return i;
		}
	}
	return -1;
}

static void mark_reg(struct lint_ctx *ctx, const kdal_ast_t *target, int line,
		     bool writing)
{
	int i;

	if (!target || target->type != KDAL_NODE_EXPR_REG_PATH)
		return;
	i = reg_of_path(ctx, (const kdal_expr_node_t *)target);
	if (i < 0)
		return;
	ctx->reg_used[i] = 1;
	if (writing && strcmp(ctx->reg_modes[i], "ro") == 0)
		emit_warning(ctx, line, "W003",
			     "write to read-only register '%s'", ctx->regs[i]);
	if (!writing && strcmp(ctx->reg_modes[i], "wo") == 0)
		emit_warning(ctx, line, "W004",
			     "read from write-only register '%s'",
			     ctx->regs[i]);
}

/* An expression's operands sit in its union, not in child lists. */
static void lint_walk_expr(struct lint_ctx *ctx, const kdal_ast_t *n)
{
	const kdal_expr_node_t *e = (const kdal_expr_node_t *)n;

	if (!n)
		return;
	switch (n->type) {
	case KDAL_NODE_EXPR_READ:
		mark_reg(ctx, e->u.read_path, n->line, false);
		lint_walk_expr(ctx, e->u.read_path);
		break;
	case KDAL_NODE_EXPR_BINOP:
		lint_walk_expr(ctx, e->u.binop.lhs);
		lint_walk_expr(ctx, e->u.binop.rhs);
		break;
	case KDAL_NODE_EXPR_UNOP:
		lint_walk_expr(ctx, e->u.unop.operand);
		break;
	default:
		break;
	}
}

/* Statements carry their operands in child lists, expressions in their
 * union: every read marks its register (W004), wherever it sits (a let,
 * a log argument, a condition); writes mark theirs (W003, W009). */
static void lint_walk_stmts(struct lint_ctx *ctx, const kdal_ast_t *n)
{
	for (const kdal_ast_t *s = n; s; s = s->next) {
		const kdal_ast_t *target = s->child;

		if (s->type >= KDAL_NODE_EXPR_LITERAL_INT &&
		    s->type < KDAL_NODE__MAX) {
			lint_walk_expr(ctx, s);
			continue;
		}
		if (s->type == KDAL_NODE_REG_WRITE_STMT) {
			mark_reg(ctx, target, s->line, true);
			/* W009: magic number in a register write */
			if (ctx->strict && target && target->next &&
			    target->next->type == KDAL_NODE_EXPR_LITERAL_INT) {
				const kdal_expr_node_t *ve =
					(const kdal_expr_node_t *)target->next;
				if (ve->u.ival > 0xFF)
					emit_warning(
						ctx, s->line, "W009",
						"large magic number 0x%llx in reg_write - "
						"consider a named constant",
						(unsigned long long)ve->u.ival);
			}
		}
		if (s->child)
			lint_walk_stmts(ctx, s->child);
	}
}

/* ── KDC lint pass (AST-based) ───────────────────────────────────── */

static int file_exists(const char *p)
{
	FILE *f = fopen(p, "r");

	if (!f)
		return 0;
	(void)fclose(f);
	return 1;
}

/* An import as the compiler sees it: next to the .kdc; for
 * "kdal/stdlib/x.kdh", $KDAL_STDLIB_DIR, the installed standard library,
 * then the source tree's lang/stdlib. 0 when found next to the .kdc, 1 in
 * the standard library, -1 when not found. */
static int resolve_import(const char *kdc, const char *imp, char *out,
			  size_t outsz)
{
	static const char prefix[] = "kdal/stdlib/";
	const char *slash = strrchr(kdc, '/');
	const char *dirs[3];
	int n = 0;

	if (imp[0] == '/') {
		(void)snprintf(out, outsz, "%s", imp);
		return file_exists(out) ? 0 : -1;
	}
	if (slash)
		(void)snprintf(out, outsz, "%.*s/%s", (int)(slash - kdc), kdc,
			       imp);
	else
		(void)snprintf(out, outsz, "%s", imp);
	if (file_exists(out))
		return 0;
	if (strncmp(imp, prefix, sizeof(prefix) - 1) != 0)
		return -1;
	if (getenv("KDAL_STDLIB_DIR"))
		dirs[n++] = getenv("KDAL_STDLIB_DIR");
#ifdef KDALITY_STDLIB_INSTALL
	dirs[n++] = KDALITY_STDLIB_INSTALL;
#endif
#ifdef KDALITY_STDLIB_SOURCE
	dirs[n++] = KDALITY_STDLIB_SOURCE;
#endif
	for (int i = 0; i < n; i++) {
		(void)snprintf(out, outsz, "%s/%s", dirs[i],
			       imp + sizeof(prefix) - 1);
		if (file_exists(out))
			return 1;
	}
	return -1;
}

/* Each imported .kdh, for register info (W003, W004, W007). */
static void lint_imports(struct lint_ctx *ctx, const char *path,
			 const kdal_file_node_t *file)
{
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
			char found[PATH_MAX];

			int where = resolve_import(path, clean, found,
						   sizeof(found));
			int first = ctx->nregs;

			if (where >= 0)
				lint_kdh(ctx, found);
			/* a standard-library device is shared: a driver
			 * uses some of its registers, so no W007 for it */
			if (where == 1)
				for (int i = first; i < ctx->nregs; i++)
					ctx->reg_used[i] = 1;
			if (where < 0)
				emit_warning(
					ctx, im->line, "E002",
					"cannot find imported header '%s' (next to "
					"the .kdc, or the standard library for "
					"kdal/stdlib/; KDAL_STDLIB_DIR overrides)",
					clean);
		}
	}
}

static int lint_kdc(struct lint_ctx *ctx, const char *path)
{
	kdal_arena_t *arena = NULL;
	kdal_file_node_t *file = kdal_parse_file("lint", path, &arena);
	if (!file) {
		emit_warning(ctx, 1, "E001", "failed to parse '%s'", path);
		return -1;
	}

	lint_imports(ctx, path, file);

	if (!file->driver) {
		emit_warning(ctx, 1, "E001", "no 'driver' declaration found");
		kdal_arena_free(arena);
		return 0;
	}

	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;

	/* W006: no import statement */
	if (!file->imports)
		emit_warning(
			ctx, 1, "W006",
			"no 'import' statement - no device header imported");

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
	char resolved_input[PATH_MAX];
	const char *safe_input;

	/* argv[0] is the subcommand's name */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 ||
		    strcmp(argv[i], "--help") == 0) {
			lint_help();
			return 0;
		} else if (strcmp(argv[i], "--strict") == 0) {
			strict = 1;
		} else if (argv[i][0] == '-') {
			(void)fprintf(stderr, "lint: unknown option '%s'\n\n",
				      argv[i]);
			lint_help();
			return 1;
		} else if (input) {
			(void)fprintf(
				stderr,
				"lint: more than one input ('%s', '%s')\n\n",
				input, argv[i]);
			lint_help();
			return 1;
		} else {
			input = argv[i];
		}
	}

	if (!input) {
		fprintf(stderr, "lint: no input file\n\n");
		lint_help();
		return 1;
	}

	if (!kdality_path_ok(input) ||
	    realpath(input, resolved_input) == NULL) {
		(void)fprintf(stderr, "lint: cannot resolve '%s': %s\n", input,
			      strerror(errno));
		return 1;
	}

	safe_input = resolved_input;

	struct lint_ctx ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.filename = safe_input;
	ctx.strict = strict;

	/* Determine file type */
	size_t len = strlen(safe_input);
	int is_kdh = (len > 4 && strcmp(safe_input + len - 4, ".kdh") == 0);
	int is_kdc = (len > 4 && strcmp(safe_input + len - 4, ".kdc") == 0);

	if (!is_kdh && !is_kdc) {
		fprintf(stderr,
			"lint: unknown file type (expected .kdh or .kdc)\n");
		return 1;
	}

	int rc;
	if (is_kdh)
		rc = lint_kdh(&ctx, safe_input);
	else
		rc = lint_kdc(&ctx, safe_input);

	print_warnings(&ctx);

	if (rc != 0)
		return 1;

	if (ctx.nwarns == 0)
		printf("%s: OK (0 warnings)\n", safe_input);
	else
		printf("\n%d warning(s)\n", ctx.nwarns);

	return (ctx.nwarns > 0) ? 1 : 0;
}
