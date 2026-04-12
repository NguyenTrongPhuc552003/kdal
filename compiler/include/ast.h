/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * KDAL Compiler - ast.h
 * Abstract Syntax Tree node definitions.
 *
 * The AST is a tree of tagged-union nodes allocated from a linear
 * arena (kdal_arena_t). No heap allocation after parse is complete.
 *
 * Node type hierarchy:
 *
 *   kdal_ast_t (base - every node starts with this)
 *   ├── KDAL_NODE_FILE
 *   ├── KDAL_NODE_IMPORT
 *   ├── KDAL_NODE_VERSION
 *   ├── KDAL_NODE_BACKEND
 *   ├── KDAL_NODE_DEVICE_CLASS
 *   │   ├── KDAL_NODE_REGISTERS_BLOCK
 *   │   │   └── KDAL_NODE_REGISTER_DECL
 *   │   │       └── KDAL_NODE_BITFIELD_MEMBER
 *   │   ├── KDAL_NODE_SIGNALS_BLOCK
 *   │   │   └── KDAL_NODE_SIGNAL_DECL
 *   │   ├── KDAL_NODE_CAPABILITIES_BLOCK
 *   │   │   └── KDAL_NODE_CAPABILITY_DECL
 *   │   ├── KDAL_NODE_POWER_BLOCK
 *   │   │   └── KDAL_NODE_POWER_STATE
 *   │   └── KDAL_NODE_CONFIG_BLOCK
 *   │       └── KDAL_NODE_CONFIG_FIELD
 *   └── KDAL_NODE_DRIVER
 *       ├── KDAL_NODE_CONFIG_BIND
 *       ├── KDAL_NODE_PROBE_HANDLER
 *       ├── KDAL_NODE_REMOVE_HANDLER
 *       └── KDAL_NODE_EVENT_HANDLER
 *           └── (statement nodes)
 */

#ifndef KDALC_AST_H
#define KDALC_AST_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
struct kdal_ast;
typedef struct kdal_ast kdal_ast_t;

/* ── Node type enum ──────────────────────────────────────────────── */

typedef enum {
	KDAL_NODE_INVALID = 0,

	/* Top-level */
	KDAL_NODE_FILE,
	KDAL_NODE_VERSION,
	KDAL_NODE_IMPORT,
	KDAL_NODE_BACKEND,
	KDAL_NODE_BACKEND_OPTION,

	/* .kdh - device class */
	KDAL_NODE_DEVICE_CLASS,
	KDAL_NODE_REGISTERS_BLOCK,
	KDAL_NODE_REGISTER_DECL,
	KDAL_NODE_BITFIELD_MEMBER,
	KDAL_NODE_SIGNALS_BLOCK,
	KDAL_NODE_SIGNAL_DECL,
	KDAL_NODE_CAPABILITIES_BLOCK,
	KDAL_NODE_CAPABILITY_DECL,
	KDAL_NODE_POWER_BLOCK,
	KDAL_NODE_POWER_STATE,
	KDAL_NODE_CONFIG_BLOCK,
	KDAL_NODE_CONFIG_FIELD,

	/* .kdc - driver */
	KDAL_NODE_DRIVER,
	KDAL_NODE_CONFIG_BIND,
	KDAL_NODE_CONFIG_BIND_STMT,
	KDAL_NODE_PROBE_HANDLER,
	KDAL_NODE_REMOVE_HANDLER,
	KDAL_NODE_EVENT_HANDLER,

	/* Statements */
	KDAL_NODE_LET_STMT,
	KDAL_NODE_ASSIGN_STMT,
	KDAL_NODE_REG_WRITE_STMT,
	KDAL_NODE_EMIT_STMT,
	KDAL_NODE_WAIT_STMT,
	KDAL_NODE_ARM_STMT,
	KDAL_NODE_CANCEL_STMT,
	KDAL_NODE_RETURN_STMT,
	KDAL_NODE_LOG_STMT,
	KDAL_NODE_IF_STMT,
	KDAL_NODE_FOR_STMT,

	/* Expressions */
	KDAL_NODE_EXPR_LITERAL_INT,
	KDAL_NODE_EXPR_LITERAL_STR,
	KDAL_NODE_EXPR_LITERAL_BOOL,
	KDAL_NODE_EXPR_IDENT,
	KDAL_NODE_EXPR_REG_PATH,
	KDAL_NODE_EXPR_READ,
	KDAL_NODE_EXPR_BINOP,
	KDAL_NODE_EXPR_UNOP,

	KDAL_NODE__MAX
} kdal_node_type_t;

/* ── Register access qualifier ───────────────────────────────────── */

typedef enum {
	KDAL_ACCESS_RW = 0,
	KDAL_ACCESS_RO,
	KDAL_ACCESS_WO,
	KDAL_ACCESS_RC, /* clear on read */
} kdal_access_t;

/* ── Signal direction ────────────────────────────────────────────── */

typedef enum {
	KDAL_SIG_IN = 0,
	KDAL_SIG_OUT,
	KDAL_SIG_INOUT,
} kdal_sig_dir_t;

/* ── Signal trigger ──────────────────────────────────────────────── */

typedef enum {
	KDAL_TRIG_EDGE_RISING = 0,
	KDAL_TRIG_EDGE_FALLING,
	KDAL_TRIG_EDGE_ANY,
	KDAL_TRIG_LEVEL_HIGH,
	KDAL_TRIG_LEVEL_LOW,
	KDAL_TRIG_COMPLETION,
	KDAL_TRIG_TIMEOUT,
} kdal_trigger_t;

/* ── Event handler type ──────────────────────────────────────────── */

typedef enum {
	KDAL_EVT_READ = 0,
	KDAL_EVT_WRITE,
	KDAL_EVT_SIGNAL,
	KDAL_EVT_POWER,
	KDAL_EVT_TIMEOUT,
} kdal_evt_type_t;

/* ── Binary and unary operator ───────────────────────────────────── */

typedef enum {
	KDAL_BINOP_ADD,
	KDAL_BINOP_SUB,
	KDAL_BINOP_MUL,
	KDAL_BINOP_DIV,
	KDAL_BINOP_MOD,
	KDAL_BINOP_SHL,
	KDAL_BINOP_SHR,
	KDAL_BINOP_AND,
	KDAL_BINOP_OR,
	KDAL_BINOP_XOR,
	KDAL_BINOP_EQ,
	KDAL_BINOP_NEQ,
	KDAL_BINOP_LT,
	KDAL_BINOP_LE,
	KDAL_BINOP_GT,
	KDAL_BINOP_GE,
	KDAL_BINOP_LAND,
	KDAL_BINOP_LOR,
} kdal_binop_t;

typedef enum {
	KDAL_UNOP_NEG,
	KDAL_UNOP_NOT,
	KDAL_UNOP_INV,
} kdal_unop_t;

/* ── Base AST node ───────────────────────────────────────────────── */

struct kdal_ast {
	kdal_node_type_t type;
	int line;
	int col;
	kdal_ast_t *next; /* sibling list */
	kdal_ast_t *child; /* first child */
};

/* ── Typed node accessors ────────────────────────────────────────── */

#define KDAL_CONTAINER_OF(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

/* File node */
typedef struct {
	kdal_ast_t base;
	const char *filename;
	kdal_ast_t *version; /* KDAL_NODE_VERSION (may be NULL) */
	kdal_ast_t *imports; /* list of KDAL_NODE_IMPORT */
	kdal_ast_t *device; /* KDAL_NODE_DEVICE_CLASS (kdh) or NULL */
	kdal_ast_t *driver; /* KDAL_NODE_DRIVER (kdc) or NULL */
	kdal_ast_t *backend; /* KDAL_NODE_BACKEND (kdc) or NULL */
} kdal_file_node_t;

/* Version node */
typedef struct {
	kdal_ast_t base;
	const char *version; /* e.g. "0.1" */
} kdal_version_node_t;

/* Import node */
typedef struct {
	kdal_ast_t base;
	const char *path; /* "kdal/stdlib/uart.kdh" */
	const char *alias; /* may be NULL */
} kdal_import_node_t;

/* Backend node */
typedef struct {
	kdal_ast_t base;
	const char *name; /* "mmio", "platdev", "qemu", … */
	kdal_ast_t *options; /* list of KDAL_NODE_BACKEND_OPTION */
} kdal_backend_node_t;

/* Backend option node */
typedef struct {
	kdal_ast_t base;
	const char *key;
	kdal_ast_t *value; /* KDAL_NODE_EXPR_LITERAL_* */
} kdal_backend_opt_node_t;

/* Device class node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	const char *compatible; /* e.g. "example,init" or NULL */
	const char *device_class_type; /* e.g. "gpio" from simplified form */
	kdal_ast_t *registers; /* KDAL_NODE_REGISTERS_BLOCK or NULL */
	kdal_ast_t *signals; /* KDAL_NODE_SIGNALS_BLOCK or NULL */
	kdal_ast_t *capabilities;
	kdal_ast_t *power;
	kdal_ast_t *config;
} kdal_device_node_t;

/* Register declaration node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	int width; /* 8/16/32/64 or 0 for bitfield */
	int is_bitfield;
	uint64_t offset; /* @ value, UINT64_MAX = not specified */
	kdal_access_t access;
	kdal_ast_t *reset_val; /* expr or NULL */
	kdal_ast_t *members; /* list of KDAL_NODE_BITFIELD_MEMBER */
} kdal_reg_decl_node_t;

/* Bitfield member node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	int bit_lo;
	int bit_hi; /* == bit_lo for single-bit */
	kdal_ast_t *reset_val;
} kdal_bf_member_node_t;

/* Signal declaration node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	kdal_sig_dir_t dir;
	kdal_trigger_t trigger;
	kdal_ast_t *trigger_arg; /* expr for timeout duration, or NULL */
} kdal_signal_node_t;

/* Capability declaration node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	kdal_ast_t *value; /* expr or NULL */
} kdal_cap_node_t;

/* Power state node */
typedef struct {
	kdal_ast_t base;
	const char *name; /* "on", "off", "suspend", or custom */
	const char *spec; /* "allowed", "forbidden", or NULL */
} kdal_power_state_node_t;

/* Config field node (both .kdh config block and .kdc config bind) */
typedef struct {
	kdal_ast_t base;
	const char *name;
	int type_kw; /* TOK_KW_U8 … TOK_KW_BUF */
	kdal_ast_t *default_val;
	kdal_ast_t *range_lo; /* "in lo..hi" or NULL */
	kdal_ast_t *range_hi;
} kdal_config_field_node_t;

/* Driver node */
typedef struct {
	kdal_ast_t base;
	const char *name;
	const char *device_class; /* "for" target, may be NULL */
	kdal_ast_t *config_bind;
	kdal_ast_t *probe;
	kdal_ast_t *remove; /* may be NULL */
	kdal_ast_t *handlers; /* list of KDAL_NODE_EVENT_HANDLER */
} kdal_driver_node_t;

/* Event handler node */
typedef struct {
	kdal_ast_t base;
	kdal_evt_type_t evt_type;
	const char *reg_path[3]; /* up to 3 path components */
	int path_len;
	const char *param_name; /* "val" in on_write, or NULL */
	const char *power_from; /* power transition "from" state */
	const char *power_to; /* power transition "to" state */
	kdal_ast_t *body; /* list of statements */
} kdal_handler_node_t;

/* Expression node - covers all expression variants */
typedef struct {
	kdal_ast_t base;
	union {
		long long ival; /* LITERAL_INT / HEX / BIN */
		int bval; /* LITERAL_BOOL */
		const char *sval; /* LITERAL_STR / IDENT */
		struct {
			const char *parts[4];
			int nparts;
		} path; /* REG_PATH */
		struct {
			kdal_binop_t op;
			kdal_ast_t *lhs;
			kdal_ast_t *rhs;
		} binop; /* BINOP */
		struct {
			kdal_unop_t op;
			kdal_ast_t *operand;
		} unop; /* UNOP */
		kdal_ast_t *read_path; /* EXPR_READ: the register path */
	} u;
} kdal_expr_node_t;

/* Statement nodes (let, assign, reg_write, emit, wait, arm, cancel,
 * return, log, if, for) are distinguished by kdal_ast_t.type and
 * carry their operands in child/next lists. See parser.c for layout. */

/* ── Arena allocator interface ───────────────────────────────────── */

typedef struct kdal_arena_block {
	struct kdal_arena_block *next;
	size_t cap;
	size_t used;
	char buf[];
} kdal_arena_block_t;

typedef struct {
	kdal_arena_block_t *head; /* current (newest) block */
	size_t block_size; /* default block capacity */
} kdal_arena_t;

kdal_arena_t *kdal_arena_new(size_t initial_cap);
void *kdal_arena_alloc(kdal_arena_t *a, size_t sz);
void kdal_arena_free(kdal_arena_t *a);

/* ── AST factory helpers ─────────────────────────────────────────── */

kdal_ast_t *kdal_ast_new(kdal_arena_t *a, kdal_node_type_t type, int line,
			 int col);

/* Append a sibling to a list, returning the updated head */
kdal_ast_t *kdal_ast_append(kdal_ast_t *list, kdal_ast_t *node);

/* Append a child to a node's child list */
void kdal_ast_add_child(kdal_ast_t *parent, kdal_ast_t *child);

/* ── AST dump (debug) ────────────────────────────────────────────── */

void kdal_ast_dump(const kdal_ast_t *root, int indent);

#endif /* KDALC_AST_H */
