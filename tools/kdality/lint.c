/*
 * lint.c — kdality lint subcommand.
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

#define NAME_LEN 64
#define LINE_LEN 512
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

	/* KDH state */
	char regs[MAX_REGS][NAME_LEN];
	char reg_modes[MAX_REGS][4]; /* ro, wo, rw */
	int nregs;
	int reg_used[MAX_REGS];

	/* KDC state */
	int has_use;
	int has_probe;
	int has_remove;
	int has_power;
	int handler_lines; /* lines in current handler (detect empty) */
	char current_handler[NAME_LEN];
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

/* ── KDH lint pass ───────────────────────────────────────────────── */

static int lint_kdh(struct lint_ctx *ctx, const char *path)
{
	FILE *fp;
	char line[LINE_LEN];
	int in_regmap = 0;
	int has_device = 0;
	int has_compatible = 0;

	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "lint: cannot open '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '/' || *p == '\n' || *p == '\0')
			continue;

		if (strncmp(p, "device ", 7) == 0)
			has_device = 1;

		if (strncmp(p, "compatible ", 11) == 0)
			has_compatible = 1;

		if (strstr(p, "register_map")) {
			in_regmap = 1;
			continue;
		}
		if (*p == '}')
			in_regmap = 0;

		if (in_regmap && ctx->nregs < MAX_REGS) {
			char rname[NAME_LEN];
			unsigned long off;
			char mode[4] = "";
			if (sscanf(p, "%63s 0x%lx %3s", rname, &off, mode) >=
			    2) {
				strncpy(ctx->regs[ctx->nregs], rname,
					NAME_LEN - 1);
				strncpy(ctx->reg_modes[ctx->nregs], mode,
					sizeof(ctx->reg_modes[0]) - 1);
				ctx->nregs++;
			}
		}
	}

	fclose(fp);

	if (!has_device)
		emit_warning(ctx, 1, "E001", "no 'device' declaration found");

	if (!has_compatible && ctx->strict)
		emit_warning(ctx, 1, "W010",
			     "no 'compatible' string (needed for Device Tree)");

	return 0;
}

/* ── KDC lint pass ───────────────────────────────────────────────── */

static void check_reg_access(struct lint_ctx *ctx, int lineno, const char *line,
			     int is_write)
{
	/* Extract register name from reg_read/reg_write call */
	const char *fn = is_write ? "reg_write(" : "reg_read(";
	const char *pos = strstr(line, fn);
	if (!pos)
		return;

	pos += strlen(fn);
	char rname[NAME_LEN] = "";
	int j = 0;
	while (*pos && *pos != ',' && *pos != ')' && j < NAME_LEN - 1) {
		if (*pos != ' ')
			rname[j++] = *pos;
		pos++;
	}
	rname[j] = '\0';

	if (!rname[0])
		return;

	/* Find in register map */
	for (int i = 0; i < ctx->nregs; i++) {
		if (strcmp(ctx->regs[i], rname) == 0) {
			ctx->reg_used[i] = 1;

			if (is_write && strcmp(ctx->reg_modes[i], "ro") == 0) {
				emit_warning(ctx, lineno, "W003",
					     "write to read-only register '%s'",
					     rname);
			}
			if (!is_write && strcmp(ctx->reg_modes[i], "wo") == 0) {
				emit_warning(
					ctx, lineno, "W004",
					"read from write-only register '%s'",
					rname);
			}
			return;
		}
	}
}

static int lint_kdc(struct lint_ctx *ctx, const char *path)
{
	FILE *fp;
	char line[LINE_LEN];
	int lineno = 0;
	int in_handler = 0;
	int handler_start = 0;

	fp = fopen(path, "r");
	if (!fp) {
		fprintf(stderr, "lint: cannot open '%s': %s\n", path,
			strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), fp)) {
		lineno++;
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\n' || *p == '\0' || *p == '/')
			continue;

		/* use statement */
		if (strncmp(p, "use ", 4) == 0) {
			ctx->has_use = 1;

			/* Try to find and lint the .kdh file */
			char *q = strchr(p, '"');
			if (q) {
				q++;
				char *end = strchr(q, '"');
				if (end) {
					char kdh_path[512];
					size_t len = (size_t)(end - q);
					if (len < sizeof(kdh_path)) {
						memcpy(kdh_path, q, len);
						kdh_path[len] = '\0';
						/* Try relative to kdc dir */
						lint_kdh(ctx, kdh_path);
					}
				}
			}
			continue;
		}

		/* on <event> */
		if (strncmp(p, "on ", 3) == 0) {
			/* Finish previous handler */
			if (in_handler && ctx->handler_lines == 0) {
				emit_warning(ctx, handler_start, "W005",
					     "empty handler body: on %s",
					     ctx->current_handler);
			}

			in_handler = 1;
			handler_start = lineno;
			ctx->handler_lines = 0;

			char event[NAME_LEN] = "";
			sscanf(p + 3, "%63[^{ \n]", event);
			strncpy(ctx->current_handler, event, NAME_LEN - 1);

			if (strcmp(event, "probe") == 0)
				ctx->has_probe = 1;
			if (strcmp(event, "remove") == 0)
				ctx->has_remove = 1;
			if (strcmp(event, "power") == 0)
				ctx->has_power = 1;
			continue;
		}

		/* Closing brace */
		if (*p == '}') {
			if (in_handler && ctx->handler_lines == 0) {
				emit_warning(ctx, handler_start, "W005",
					     "empty handler body: on %s",
					     ctx->current_handler);
			}
			in_handler = 0;
			continue;
		}

		/* Count handler content lines */
		if (in_handler)
			ctx->handler_lines++;

		/* Check register accesses */
		if (strstr(p, "reg_write"))
			check_reg_access(ctx, lineno, p, 1);
		if (strstr(p, "reg_read"))
			check_reg_access(ctx, lineno, p, 0);

		/* W009: magic numbers in reg_write value */
		if (ctx->strict && strstr(p, "reg_write")) {
			const char *comma = strchr(p, ',');
			if (comma) {
				comma++;
				while (*comma == ' ')
					comma++;
				if (*comma == '0' && *(comma + 1) == 'x') {
					/* hex literal — could be magic */
					unsigned long val =
						strtoul(comma, NULL, 0);
					if (val > 0xFF) {
						emit_warning(
							ctx, lineno, "W009",
							"large magic number 0x%lx "
							"in reg_write — consider "
							"a named constant",
							val);
					}
				}
			}
		}
	}

	fclose(fp);

	/* Post-scan checks */
	if (!ctx->has_use)
		emit_warning(ctx, 1, "W006",
			     "no 'use' statement — no device header imported");

	if (!ctx->has_probe)
		emit_warning(ctx, 1, "W001", "missing 'on probe' handler");

	if (!ctx->has_remove)
		emit_warning(ctx, 1, "W002",
			     "missing 'on remove' handler "
			     "(potential resource leak)");

	if (!ctx->has_power && ctx->strict)
		emit_warning(ctx, 1, "W008", "no power management handlers");

	/* W007: unused registers */
	for (int i = 0; i < ctx->nregs; i++) {
		if (!ctx->reg_used[i]) {
			emit_warning(ctx, 0, "W007",
				     "register '%s' declared but never used",
				     ctx->regs[i]);
		}
	}

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

int kdality_lint(int argc, char *argv[])
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
