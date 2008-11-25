/*
 * used to be: tclsh gen-l-ast2.tcl to regenerate
 * As of Feb 2008 it is maintained by hand.
 */
#ifndef L_AST_H
#define L_AST_H

#define	unless(p)	if (!(p))
#define	private		static

/* Defines */
#define	L_WALK_CONTINUE		0x00000
#define	L_WALK_PRE		0x00001
#define	L_WALK_POST		0x00002
#define	L_WALK_SKIP_CHILDREN	0x00004
#define	L_WALK_SKIP_POST	0x00008
#define	L_WALK_BREAK		0x0000C
#define	L_WALK_ERROR		0x8000C

/* Typedefs */
typedef struct Ast Ast;
typedef struct L_block L_block;
typedef struct L_var_decl L_var_decl;
typedef struct L_function_decl L_function_decl;
typedef struct L_stmt L_stmt;
typedef struct L_toplevel L_toplevel;
typedef struct L_if_unless L_if_unless;
typedef struct L_loop L_loop;
typedef struct L_foreach_loop L_foreach_loop;
typedef struct L_expr L_expr;
typedef struct L_type L_type;
typedef struct L_initializer L_initializer;

/* Enums */
typedef enum L_expr_kind {
	L_EXPR_ARRAY_INDEX,
	L_EXPR_BINARY,
	L_EXPR_FLOTE,
	L_EXPR_FUNCALL,
	L_EXPR_HASH_INDEX,
	L_EXPR_INTEGER,
	L_EXPR_INTERPOLATED_STRING,
	L_EXPR_POST,
	L_EXPR_PRE,
	L_EXPR_REGEXP,
	L_EXPR_STRING,
	L_EXPR_STRUCT_INDEX,
	L_EXPR_TERTIARY,
	L_EXPR_UNARY,
	L_EXPR_VAR,
	L_EXPR_PUSH
} L_expr_kind;

extern char *L_expr_tostr[15];
typedef enum L_loop_kind {
	L_LOOP_DO,
	L_LOOP_FOR,
	L_LOOP_WHILE
} L_loop_kind;

extern char *L_loop_tostr[3];
typedef enum L_stmt_kind {
	L_STMT_BLOCK,
	L_STMT_BREAK,
	L_STMT_COND,
	L_STMT_CONTINUE,
	L_STMT_DECL,
	L_STMT_EXPR,
	L_STMT_FOREACH,
	L_STMT_LOOP,
	L_STMT_RETURN,
	L_STMT_PUSH
} L_stmt_kind;

extern char *L_stmt_tostr[9];
typedef enum L_toplevel_kind {
	L_TOPLEVEL_FUN,
	L_TOPLEVEL_GLOBAL,
	L_TOPLEVEL_INC,
	L_TOPLEVEL_STMT,
	L_TOPLEVEL_TYPE,
	L_TOPLEVEL_TYPEDEF
} L_toplevel_kind;

extern char *L_toplevel_tostr[6];
typedef enum L_type_kind {
	L_TYPE_ARRAY,
	L_TYPE_FLOAT,
	L_TYPE_HASH,
	L_TYPE_INT,
	L_TYPE_NUMBER,
	L_TYPE_POLY,
	L_TYPE_STRING,
	L_TYPE_STRUCT,
	L_TYPE_VAR,
	L_TYPE_VOID,
	L_TYPE_WIDGET
} L_type_kind;

extern char *L_type_tostr[10];
typedef enum L_node_type {
	L_NODE_BLOCK,
	L_NODE_EXPR,
	L_NODE_FOREACH_LOOP,
	L_NODE_FUNCTION_DECL,
	L_NODE_IF_UNLESS,
	L_NODE_INITIALIZER,
	L_NODE_LOOP,
	L_NODE_STMT,
	L_NODE_TOPLEVEL,
	L_NODE_TYPE,
	L_NODE_VAR_DECL
} L_node_type;
extern char *L_node_type_tostr[11];

/* Struct declarations */
struct Ast {
	Ast *_trace;
	L_node_type type;
	int line_no;
	int beg;
	int end;
};

struct L_block {
	Ast node;
	L_stmt *body;
	L_var_decl *decls;
};

struct L_expr {
	Ast node;
	L_expr *a;
	L_expr *b;
	L_expr *c;
	L_expr *indices;
	L_expr *next;
	L_expr_kind kind;
	int op;
	union {
		char *string;
		double flote;
		long integer;
	} u;
};

struct L_foreach_loop {
	Ast node;
	L_expr *expr;
	L_expr *key;
	L_expr *value;
	L_stmt *body;
};

struct L_function_decl {
	Ast node;
	L_block *body;
	L_expr *name;
	L_type *return_type;
	L_var_decl *params;
	int pattern_p;
};

struct L_if_unless {
	Ast node;
	L_expr *condition;
	L_stmt *else_body;
	L_stmt *if_body;
};

struct L_initializer {
	Ast node;
	L_expr *key;
	L_expr *value;
	L_initializer *next;
	L_initializer *next_dim;
};

struct L_loop {
	Ast node;
	L_expr *condition;
	L_expr *post;
	L_expr *pre;
	L_loop_kind kind;
	L_stmt *body;
};

struct L_stmt {
	Ast node;
	L_stmt *next;
	L_stmt_kind kind;
	union {
		L_block *block;
		L_expr *expr;
		L_foreach_loop *foreach;
		L_if_unless *cond;
		L_loop *loop;
		L_var_decl *decl;
	} u;
};

struct L_toplevel {
	Ast node;
	L_toplevel *next;
	L_toplevel_kind kind;
	union {
		L_expr *inc;
		L_function_decl *fun;
		L_stmt *stmt;
		L_type *type;
		L_var_decl *global;
	} u;
};

struct L_type {
	Ast node;
	L_expr *array_dim;
	L_expr *struct_tag;
	L_type *next_dim;
	L_type_kind kind;
	L_var_decl *members;
	int typedef_p;
};

struct L_var_decl {
	Ast node;
	L_expr *name;
	L_initializer *initial_value;
	L_type *type;
	L_var_decl *next;
	int by_name;
	int extern_p;
	int rest_p;
};


/* Prototypes */
typedef int (*LWalkFunc)(Ast *node, void *data, int order);
L_block *mk_block(L_var_decl *decls,L_stmt *body, int beg, int end);
L_expr *mk_expr(L_expr_kind kind,int op,L_expr *a,L_expr *b,L_expr *c,L_expr *indices,L_expr *next, int beg, int end);
L_foreach_loop *mk_foreach_loop(L_expr *hash,L_expr *key,L_expr *value,L_stmt *body, int beg, int end);
L_function_decl *mk_function_decl(L_expr *name,L_var_decl *params,L_type *return_type,L_block *body,int pattern_p, int beg, int end);
L_if_unless *mk_if_unless(L_expr *condition,L_stmt *if_body,L_stmt *else_body, int beg, int end);
L_initializer *mk_initializer(L_expr *key,L_expr *value,L_initializer *next_dim,L_initializer *next, int beg, int end);
L_loop *mk_loop(L_loop_kind kind,L_expr *pre,L_expr *condition,L_expr *post,L_stmt *body, int beg, int end);
L_stmt *mk_stmt(L_stmt_kind kind,L_stmt *next, int beg, int end);
L_toplevel *mk_toplevel(L_toplevel_kind kind,L_toplevel *next, int beg, int end);
L_type *mk_type(L_type_kind kind,L_expr *array_dim,L_expr *struct_tag,L_type *next_dim,L_var_decl *members,int typedef_p, int beg, int end);
L_var_decl *mk_var_decl(L_type *type,L_expr *name,L_initializer *initial_value,int by_name,int extern_p,int rest_p,L_var_decl *next, int beg, int end);
int L_walk_ast(Ast *node, int order, LWalkFunc func, void *data);

#endif /* L_AST_H */
