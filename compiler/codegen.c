// SPDX-License-Identifier: GPL-2.0-only
/*
 * KDAL Compiler — codegen.c
 * Annotated AST → kernel C translation unit + Kbuild fragment.
 *
 * Emits:
 *   <output_dir>/<driver_name>.c         — kernel module C source
 *   <output_dir>/Makefile.kbuild         — obj-m fragment for kbuild
 *
 * The generated C file uses the KDAL runtime API (kdal_driver_register,
 * kdal_driver_ops, etc.) from <linux/kdal.h>. It follows the platform_device
 * driver model by default; the backend annotation may change the template.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "include/token.h"
#include "include/ast.h"
#include "include/codegen.h"

/* ── Diagnostic implementation ───────────────────────────────────── */

void kdal_error(const char *filename, int line, int col, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:%d: error: ", filename, line, col);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void kdal_warn(const char *filename, int line, int col, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:%d: warning: ", filename, line, col);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

void kdal_note(const char *filename, int line, int col, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:%d: note: ", filename, line, col);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

/* ── Code writer ─────────────────────────────────────────────────── */

typedef struct {
	FILE *fp;
	int indent;
} writer_t;

static void w_indent(writer_t *w)
{
	for (int i = 0; i < w->indent; i++)
		fprintf(w->fp, "\t");
}

#define W(w, ...)                              \
	do {                                   \
		w_indent(w);                   \
		fprintf((w)->fp, __VA_ARGS__); \
	} while (0)
#define WR(w, ...)                             \
	do {                                   \
		fprintf((w)->fp, __VA_ARGS__); \
	} while (0) /* raw */

/* ── Expression emitter ──────────────────────────────────────────── */

static void emit_expr(writer_t *w, const kdal_ast_t *expr)
{
	if (!expr) {
		WR(w, "0");
		return;
	}

	switch (expr->type) {
	case KDAL_NODE_EXPR_LITERAL_INT:
	case KDAL_NODE_EXPR_LITERAL_BOOL: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "%lld", e->u.ival);
		break;
	}
	case KDAL_NODE_EXPR_LITERAL_STR: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "%s", e->u.sval ? e->u.sval : "\"\"");
		break;
	}
	case KDAL_NODE_EXPR_IDENT: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "%s", e->u.sval);
		break;
	}
	case KDAL_NODE_EXPR_REG_PATH: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		/* Flatten path to C: UART.DATA.enable → ctx->regs.DATA.enable */
		WR(w, "ctx->regs");
		for (int i = 1; i < e->u.path.nparts; i++)
			WR(w, ".%s", e->u.path.parts[i]);
		break;
	}
	case KDAL_NODE_EXPR_READ: {
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "kdal_reg_read(&ctx->kdal, ");
		emit_expr(w, e->u.read_path);
		WR(w, ")");
		break;
	}
	case KDAL_NODE_EXPR_BINOP: {
		static const char *op_str[] = { "+",  "-",  "*", "/",  "%",
						"<<", ">>", "&", "|",  "^",
						"==", "!=", "<", "<=", ">",
						">=", "&&", "||" };
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "(");
		emit_expr(w, e->u.binop.lhs);
		WR(w, " %s ", op_str[e->u.binop.op]);
		emit_expr(w, e->u.binop.rhs);
		WR(w, ")");
		break;
	}
	case KDAL_NODE_EXPR_UNOP: {
		static const char *uop_str[] = { "-", "!", "~" };
		const kdal_expr_node_t *e = (const kdal_expr_node_t *)expr;
		WR(w, "%s(", uop_str[e->u.unop.op]);
		emit_expr(w, e->u.unop.operand);
		WR(w, ")");
		break;
	}
	default:
		WR(w, "/* unhandled expr */0");
		break;
	}
}

/* ── Statement emitter ───────────────────────────────────────────── */

static void emit_stmts(writer_t *w, const kdal_ast_t *stmts);

static void emit_stmt(writer_t *w, const kdal_ast_t *s)
{
	switch (s->type) {
	case KDAL_NODE_LET_STMT: {
		/* child[0] = ident, child[1] = expr */
		const kdal_ast_t *ident = s->child;
		const kdal_ast_t *val = ident ? ident->next : NULL;
		const char *name =
			ident ? ((const kdal_expr_node_t *)ident)->u.sval : "v";
		W(w, "unsigned long long %s = ", name);
		emit_expr(w, val);
		WR(w, ";\n");
		break;
	}
	case KDAL_NODE_ASSIGN_STMT: {
		const kdal_ast_t *lv = s->child;
		const kdal_ast_t *rv = lv ? lv->next : NULL;
		W(w, "");
		emit_expr(w, lv);
		WR(w, " = ");
		emit_expr(w, rv);
		WR(w, ";\n");
		break;
	}
	case KDAL_NODE_REG_WRITE_STMT: {
		const kdal_ast_t *reg = s->child;
		const kdal_ast_t *val = reg ? reg->next : NULL;
		W(w, "kdal_reg_write(&ctx->kdal, ");
		emit_expr(w, reg);
		WR(w, ", ");
		emit_expr(w, val);
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_EMIT_STMT: {
		const kdal_ast_t *sig = s->child;
		const kdal_ast_t *val = sig ? sig->next : NULL;
		W(w, "kdal_emit_signal(&ctx->kdal, ");
		emit_expr(w, sig);
		if (val) {
			WR(w, ", ");
			emit_expr(w, val);
		}
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_WAIT_STMT: {
		const kdal_ast_t *sig = s->child;
		const kdal_ast_t *ms = sig ? sig->next : NULL;
		W(w, "kdal_wait_signal(&ctx->kdal, ");
		emit_expr(w, sig);
		WR(w, ", ");
		emit_expr(w, ms);
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_ARM_STMT: {
		const kdal_ast_t *t = s->child;
		const kdal_ast_t *ms = t ? t->next : NULL;
		W(w, "kdal_arm_timeout(&ctx->kdal, ");
		emit_expr(w, t);
		WR(w, ", ");
		emit_expr(w, ms);
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_CANCEL_STMT: {
		W(w, "kdal_cancel_timeout(&ctx->kdal, ");
		emit_expr(w, s->child);
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_RETURN_STMT: {
		W(w, "return");
		if (s->child) {
			WR(w, " ");
			emit_expr(w, s->child);
		}
		WR(w, ";\n");
		break;
	}
	case KDAL_NODE_LOG_STMT: {
		W(w, "dev_dbg(ctx->kdal.dev");
		for (const kdal_ast_t *a = s->child; a; a = a->next) {
			WR(w, ", ");
			emit_expr(w, a);
		}
		WR(w, ");\n");
		break;
	}
	case KDAL_NODE_IF_STMT: {
		const kdal_ast_t *c = s->child;
		const char *kw = "if";
		while (c && c->next) {
			W(w, "%s (", kw);
			emit_expr(w, c);
			WR(w, ") {\n");
			w->indent++;
			emit_stmts(w, c->next);
			w->indent--;
			W(w, "}\n");
			c = c->next->next;
			kw = "else if";
		}
		if (c) {
			/* orphan child = else body */
			W(w, "else {\n");
			w->indent++;
			emit_stmts(w, c);
			w->indent--;
			W(w, "}\n");
		}
		break;
	}
	case KDAL_NODE_FOR_STMT: {
		/* child[0]=var, child[1]=lo, child[2]=hi, child[3]=body */
		const kdal_ast_t *var = s->child;
		const kdal_ast_t *lo = var ? var->next : NULL;
		const kdal_ast_t *hi = lo ? lo->next : NULL;
		const kdal_ast_t *body = hi ? hi->next : NULL;
		const char *vname =
			var ? ((const kdal_expr_node_t *)var)->u.sval : "i";
		W(w, "for (unsigned int %s = ", vname);
		emit_expr(w, lo);
		WR(w, "; %s < ", vname);
		emit_expr(w, hi);
		WR(w, "; %s++) {\n", vname);
		w->indent++;
		if (body)
			emit_stmts(w, body);
		w->indent--;
		W(w, "}\n");
		break;
	}
	default:
		W(w, "/* unknown stmt %d */\n", s->type);
		break;
	}
}

static void emit_stmts(writer_t *w, const kdal_ast_t *stmts)
{
	for (const kdal_ast_t *s = stmts; s; s = s->next)
		emit_stmt(w, s);
}

/* ── Top-level codegen ───────────────────────────────────────────── */

static int emit_c_file(const kdal_file_node_t *file, const char *out_path)
{
	FILE *fp = fopen(out_path, "w");
	if (!fp) {
		fprintf(stderr, "kdalc: cannot open output '%s': %s\n",
			out_path, strerror(errno));
		return -1;
	}

	writer_t w = { .fp = fp, .indent = 0 };
	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;
	const char *name = drv ? drv->name : "unknown";

	/* ── File header ── */
	WR(&w, "// SPDX-License-Identifier: GPL-2.0-only\n");
	WR(&w, "/*\n");
	WR(&w, " * Generated by kdalc %s — DO NOT EDIT\n", KDAL_VERSION_STRING);
	WR(&w, " * Driver: %s\n", name);
	if (drv && drv->device_class)
		WR(&w, " * Device class: %s\n", drv->device_class);
	WR(&w, " */\n\n");

	WR(&w, "#include <linux/module.h>\n");
	WR(&w, "#include <linux/platform_device.h>\n");
	WR(&w, "#include <linux/io.h>\n");
	WR(&w, "#include <linux/of.h>\n");
	WR(&w, "#include <linux/kdal.h>\n\n");

	/* ── Driver context struct ── */
	WR(&w, "struct %s_ctx {\n", name);
	WR(&w, "\tstruct kdal_driver kdal;\n");
	WR(&w, "\tvoid __iomem *base;\n");
	WR(&w, "};\n\n");

	if (!drv) {
		fclose(fp);
		return 0;
	}

	/* ── Event handlers ── */
	for (const kdal_ast_t *h = drv->handlers; h; h = h->next) {
		const kdal_handler_node_t *ev = (const kdal_handler_node_t *)h;
		switch (ev->evt_type) {
		case KDAL_EVT_READ:
			WR(&w,
			   "static ssize_t %s_read(struct kdal_driver *kdal,\n"
			   "\t\t\t\t\tchar __user *buf, size_t len, loff_t *off)\n{\n",
			   name);
			break;
		case KDAL_EVT_WRITE:
			WR(&w,
			   "static ssize_t %s_write(struct kdal_driver *kdal,\n"
			   "\t\t\t\t\tconst char __user *buf, size_t len, loff_t *off)\n{\n",
			   name);
			break;
		case KDAL_EVT_SIGNAL:
			WR(&w,
			   "static irqreturn_t %s_irq(int irq, void *dev_id)\n{\n",
			   name);
			break;
		case KDAL_EVT_POWER:
			WR(&w,
			   "static int %s_pm_notify(struct kdal_driver *kdal, int state)\n{\n",
			   name);
			break;
		case KDAL_EVT_TIMEOUT:
			WR(&w,
			   "static void %s_timeout(struct kdal_driver *kdal)\n{\n",
			   name);
			break;
		}
		w.indent = 1;
		WR(&w,
		   "\tstruct %s_ctx *ctx =\n"
		   "\t\tcontainer_of(kdal, struct %s_ctx, kdal);\n\n",
		   name, name);
		if (ev->body)
			emit_stmts(&w, ev->body);
		w.indent = 0;
		if (ev->evt_type == KDAL_EVT_READ ||
		    ev->evt_type == KDAL_EVT_WRITE)
			WR(&w, "\treturn (ssize_t)len;\n");
		else if (ev->evt_type == KDAL_EVT_SIGNAL)
			WR(&w, "\treturn IRQ_HANDLED;\n");
		else if (ev->evt_type == KDAL_EVT_POWER)
			WR(&w, "\treturn 0;\n");
		WR(&w, "}\n\n");
	}

	/* ── kdal_driver_ops ── */
	WR(&w, "static const struct kdal_driver_ops %s_ops = {\n", name);
	WR(&w, "\t.read   = %s_read,\n", name);
	WR(&w, "\t.write  = %s_write,\n", name);
	WR(&w, "};\n\n");

	/* ── probe ── */
	WR(&w, "static int %s_probe(struct platform_device *pdev)\n{\n", name);
	WR(&w, "\tstruct %s_ctx *ctx;\n", name);
	WR(&w, "\tstruct resource *res;\n\n");
	WR(&w, "\tctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);\n");
	WR(&w, "\tif (!ctx) return -ENOMEM;\n\n");
	WR(&w, "\tres = platform_get_resource(pdev, IORESOURCE_MEM, 0);\n");
	WR(&w, "\tctx->base = devm_ioremap_resource(&pdev->dev, res);\n");
	WR(&w, "\tif (IS_ERR(ctx->base)) return PTR_ERR(ctx->base);\n\n");
	WR(&w, "\tctx->kdal.dev = &pdev->dev;\n");
	WR(&w, "\tctx->kdal.ops = &%s_ops;\n", name);
	WR(&w, "\tplatform_set_drvdata(pdev, ctx);\n\n");
	if (drv->probe) {
		w.indent = 1;
		emit_stmts(&w, drv->probe->child);
		w.indent = 0;
	}
	WR(&w, "\treturn kdal_driver_register(&ctx->kdal);\n");
	WR(&w, "}\n\n");

	/* ── remove ── */
	WR(&w, "static int %s_remove(struct platform_device *pdev)\n{\n", name);
	WR(&w, "\tstruct %s_ctx *ctx = platform_get_drvdata(pdev);\n\n", name);
	if (drv->remove) {
		w.indent = 1;
		emit_stmts(&w, drv->remove->child);
		w.indent = 0;
	}
	WR(&w, "\treturn kdal_driver_unregister(&ctx->kdal);\n");
	WR(&w, "}\n\n");

	/* ── OF match table ── */
	WR(&w, "static const struct of_device_id %s_of_match[] = {\n", name);
	WR(&w, "\t{ .compatible = \"kdal,%s\" },\n", name);
	WR(&w, "\t{ }\n};\n");
	WR(&w, "MODULE_DEVICE_TABLE(of, %s_of_match);\n\n", name);

	/* ── platform_driver ── */
	WR(&w, "static struct platform_driver %s_driver = {\n", name);
	WR(&w, "\t.probe  = %s_probe,\n", name);
	WR(&w, "\t.remove = %s_remove,\n", name);
	WR(&w, "\t.driver = {\n");
	WR(&w, "\t\t.name           = \"%s\",\n", name);
	WR(&w, "\t\t.of_match_table = %s_of_match,\n", name);
	WR(&w, "\t},\n};\n");
	WR(&w, "module_platform_driver(%s_driver);\n\n", name);

	/* ── Module metadata ── */
	WR(&w, "MODULE_LICENSE(\"GPL v2\");\n");
	WR(&w, "MODULE_AUTHOR(\"KDAL compiler %s\");\n", KDAL_VERSION_STRING);
	WR(&w, "MODULE_DESCRIPTION(\"KDAL generated driver: %s\");\n", name);

	fclose(fp);
	return 0;
}

static int emit_kbuild(const kdal_file_node_t *file, const char *out_path,
		       const char *kernel_dir, const char *cross_compile)
{
	FILE *fp = fopen(out_path, "w");
	if (!fp)
		return -1;
	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;
	const char *name = drv ? drv->name : "unknown";

	fprintf(fp, "# Generated by kdalc %s — DO NOT EDIT\n",
		KDAL_VERSION_STRING);
	fprintf(fp, "obj-m := %s.o\n\n", name);
	if (kernel_dir)
		fprintf(fp, "KDIR ?= %s\n", kernel_dir);
	else
		fprintf(fp, "KDIR ?= /lib/modules/$(shell uname -r)/build\n");
	if (cross_compile)
		fprintf(fp, "CROSS_COMPILE ?= %s\n", cross_compile);
	fprintf(fp, "\nall:\n\t$(MAKE) -C $(KDIR) M=$(PWD) modules\n\n");
	fprintf(fp, "clean:\n\t$(MAKE) -C $(KDIR) M=$(PWD) clean\n");

	fclose(fp);
	return 0;
}

/* ── Public kdal_generate ────────────────────────────────────────── */

int kdal_generate(const kdal_file_node_t *file, const char *src_path,
		  const kdal_codegen_opts_t *opts)
{
	const kdal_driver_node_t *drv =
		(const kdal_driver_node_t *)file->driver;
	const char *name = drv ? drv->name : "unknown";

	char c_path[512], mk_path[512];
	const char *dir = opts->output_dir ? opts->output_dir : ".";
	snprintf(c_path, sizeof(c_path), "%s/%s.c", dir, name);
	snprintf(mk_path, sizeof(mk_path), "%s/Makefile.kbuild", dir);

	if (opts->verbose)
		fprintf(stderr, "kdalc: generating %s and %s\n", c_path,
			mk_path);

	if (emit_c_file(file, c_path) < 0)
		return -1;
	if (emit_kbuild(file, mk_path, opts->kernel_dir, opts->cross_compile) <
	    0)
		return -1;
	return 0;
}

/* ── Full pipeline ───────────────────────────────────────────────── */

int kdal_compile_file(const char *src_path, const kdal_codegen_opts_t *opts)
{
	/* Read source */
	FILE *fp = fopen(src_path, "r");
	if (!fp) {
		fprintf(stderr, "kdalc: cannot open '%s': %s\n", src_path,
			strerror(errno));
		return -1;
	}
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	rewind(fp);
	char *src = malloc((size_t)fsize + 1);
	if (!src) {
		fclose(fp);
		return -1;
	}
	size_t nread = fread(src, 1, (size_t)fsize, fp);
	src[nread] = '\0';
	fclose(fp);

	kdal_arena_t *arena = kdal_arena_new(65536);
	if (!arena) {
		free(src);
		return -1;
	}

	int rc = 0;

	/* Lex */
	kdal_token_t *tokens = NULL;
	int ntokens = kdal_lex(arena, src, nread, &tokens, src_path);
	if (ntokens < 0) {
		rc = -1;
		goto out;
	}

	/* Parse */
	kdal_file_node_t *file = kdal_parse(arena, tokens, ntokens, src_path);
	if (!file) {
		rc = -1;
		goto out;
	}

	/* Sema */
	if (kdal_sema(file, src_path) < 0) {
		rc = -1;
		goto out;
	}

	/* Codegen */
	rc = kdal_generate(file, src_path, opts);

out:
	kdal_arena_free(arena);
	free(src);
	return rc;
}
