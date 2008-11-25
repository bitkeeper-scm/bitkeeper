/*
 * used to be: tclsh gen-l-ast2.tcl to regenerate
 * As of Feb 2008 it is maintained by hand.
 */
#include "tclInt.h"
#include "Last.h"

extern void *ast_trace_root;
extern int L_line_number;
extern int L_token_offset;

char *L_expr_tostr[15] = {
	"L_EXPR_ARRAY_INDEX",
	"L_EXPR_BINARY",
	"L_EXPR_FLOTE",
	"L_EXPR_FUNCALL",
	"L_EXPR_HASH_INDEX",
	"L_EXPR_INTEGER",
	"L_EXPR_INTERPOLATED_STRING",
	"L_EXPR_POST",
	"L_EXPR_PRE",
	"L_EXPR_REGEXP",
	"L_EXPR_STRING",
	"L_EXPR_STRUCT_INDEX",
	"L_EXPR_TERTIARY",
	"L_EXPR_UNARY",
	"L_EXPR_VAR"
};

char *L_loop_tostr[3] = {
	"L_LOOP_DO",
	"L_LOOP_FOR",
	"L_LOOP_WHILE"
};

char *L_stmt_tostr[9] = {
	"L_STMT_BLOCK",
	"L_STMT_BREAK",
	"L_STMT_COND",
	"L_STMT_CONTINUE",
	"L_STMT_DECL",
	"L_STMT_EXPR",
	"L_STMT_FOREACH",
	"L_STMT_LOOP",
	"L_STMT_RETURN"
};

char *L_toplevel_tostr[6] = {
	"L_TOPLEVEL_FUN",
	"L_TOPLEVEL_GLOBAL",
	"L_TOPLEVEL_INC",
	"L_TOPLEVEL_STMT",
	"L_TOPLEVEL_TYPE",
	"L_TOPLEVEL_TYPEDEF"
};

char *L_type_tostr[10] = {
	"L_TYPE_ARRAY",
	"L_TYPE_FLOAT",
	"L_TYPE_HASH",
	"L_TYPE_INT",
	"L_TYPE_NUMBER",
	"L_TYPE_POLY",
	"L_TYPE_STRING",
	"L_TYPE_STRUCT",
	"L_TYPE_VAR",
	"L_TYPE_VOID"
};

char *L_node_type_tostr[11] = {
	"L_NODE_BLOCK",
	"L_NODE_EXPR",
	"L_NODE_FOREACH_LOOP",
	"L_NODE_FUNCTION_DECL",
	"L_NODE_IF_UNLESS",
	"L_NODE_INITIALIZER",
	"L_NODE_LOOP",
	"L_NODE_STMT",
	"L_NODE_TOPLEVEL",
	"L_NODE_TYPE",
	"L_NODE_VAR_DECL"
};


/* constructors for the L language */
L_block *
mk_block(L_var_decl *decls,L_stmt *body, int beg, int end)
{
	L_block *block;

	block = (L_block *)ckalloc(sizeof(L_block));
	memset(block, 0, sizeof(L_block));
	block->body = body;
	block->decls = decls;
	((Ast *)block)->_trace = ast_trace_root;
	ast_trace_root = (void *)block;
	((Ast *)block)->line_no = L_line_number;
	((Ast *)block)->type = L_NODE_BLOCK;
	((Ast *)block)->beg = beg;
	((Ast *)block)->end = end;
	return (block);
}

L_expr *
mk_expr(L_expr_kind kind, int op, L_expr *a, L_expr *b,
    L_expr *c, L_expr *indices, L_expr *next, int beg, int end)
{
	L_expr *expression;

	expression = (L_expr *)ckalloc(sizeof(L_expr));
	memset(expression, 0, sizeof(L_expr));
	expression->a = a;
	expression->b = b;
	expression->c = c;
	expression->indices = indices;
	expression->next = next;
	expression->kind = kind;
	expression->op = op;
	((Ast *)expression)->_trace = ast_trace_root;
	ast_trace_root = (void *)expression;
	((Ast *)expression)->line_no = L_line_number;
	((Ast *)expression)->type = L_NODE_EXPR;
	((Ast *)expression)->beg = beg;
	((Ast *)expression)->end = end;
	return (expression);
}

L_foreach_loop *
mk_foreach_loop(L_expr *expr, L_expr *key,
    L_expr *value, L_stmt *body, int beg, int end)
{
	L_foreach_loop *foreach_loop;

	foreach_loop = (L_foreach_loop *)ckalloc(sizeof(L_foreach_loop));
	memset(foreach_loop, 0, sizeof(L_foreach_loop));
	foreach_loop->expr = expr;
	foreach_loop->key = key;
	foreach_loop->value = value;
	foreach_loop->body = body;
	((Ast *)foreach_loop)->_trace = ast_trace_root;
	ast_trace_root = (void *)foreach_loop;
	((Ast *)foreach_loop)->line_no = L_line_number;
	((Ast *)foreach_loop)->type = L_NODE_FOREACH_LOOP;
	((Ast *)foreach_loop)->beg = beg;
	((Ast *)foreach_loop)->end = end;
	return (foreach_loop);
}

L_function_decl *
mk_function_decl(L_expr *name, L_var_decl *params,
    L_type *return_type, L_block *body, int pattern_p, int beg, int end)
{
	L_function_decl *function_decl;

	function_decl =
	    (L_function_decl *)ckalloc(sizeof(L_function_decl));
	memset(function_decl, 0, sizeof(L_function_decl));
	function_decl->body = body;
	function_decl->name = name;
	function_decl->return_type = return_type;
	function_decl->params = params;
	function_decl->pattern_p = pattern_p;
	((Ast *)function_decl)->_trace = ast_trace_root;
	ast_trace_root = (void *)function_decl;
	((Ast *)function_decl)->line_no = L_line_number;
	((Ast *)function_decl)->type =
	    L_NODE_FUNCTION_DECL;
	((Ast *)function_decl)->beg = beg;
	((Ast *)function_decl)->end = end;
	return (function_decl);
}

L_if_unless *
mk_if_unless(L_expr *condition, L_stmt *if_body,
    L_stmt *else_body, int beg, int end)
{
	L_if_unless *if_unless;

	if_unless = (L_if_unless *)ckalloc(sizeof(L_if_unless));
	memset(if_unless, 0, sizeof(L_if_unless));
	if_unless->condition = condition;
	if_unless->else_body = else_body;
	if_unless->if_body = if_body;
	((Ast *)if_unless)->_trace = ast_trace_root;
	ast_trace_root = (void *)if_unless;
	((Ast *)if_unless)->line_no = L_line_number;
	((Ast *)if_unless)->type = L_NODE_IF_UNLESS;
	((Ast *)if_unless)->beg = beg;
	((Ast *)if_unless)->end = end;
	return (if_unless);
}

L_initializer *
mk_initializer(L_expr *key, L_expr *value,
    L_initializer *next_dim, L_initializer *next, int beg, int end)
{
	L_initializer *initializer;

	initializer = (L_initializer *)ckalloc(sizeof(L_initializer));
	memset(initializer, 0, sizeof(L_initializer));
	initializer->key = key;
	initializer->value = value;
	initializer->next = next;
	initializer->next_dim = next_dim;
	((Ast *)initializer)->_trace = ast_trace_root;
	ast_trace_root = (void *)initializer;
	((Ast *)initializer)->line_no = L_line_number;
	((Ast *)initializer)->type = L_NODE_INITIALIZER;
	((Ast *)initializer)->beg = beg;
	((Ast *)initializer)->end = end;
	return (initializer);
}

L_loop *
mk_loop(L_loop_kind kind, L_expr *pre, L_expr *condition,
    L_expr *post, L_stmt *body, int beg, int end)
{
	L_loop *loop;

	loop = (L_loop *)ckalloc(sizeof(L_loop));
	memset(loop, 0, sizeof(L_loop));
	loop->condition = condition;
	loop->post = post;
	loop->pre = pre;
	loop->kind = kind;
	loop->body = body;
	((Ast *)loop)->_trace = ast_trace_root;
	ast_trace_root = (void *)loop;
	((Ast *)loop)->line_no = L_line_number;
	((Ast *)loop)->type = L_NODE_LOOP;
	((Ast *)loop)->beg = beg;
	((Ast *)loop)->end = end;
	return (loop);
}

L_stmt *
mk_stmt(L_stmt_kind kind, L_stmt *next, int beg, int end)
{
	L_stmt *statement;

	statement = (L_stmt *)ckalloc(sizeof(L_stmt));
	memset(statement, 0, sizeof(L_stmt));
	statement->next = next;
	statement->kind = kind;
	((Ast *)statement)->_trace = ast_trace_root;
	ast_trace_root = (void *)statement;
	((Ast *)statement)->line_no = L_line_number;
	((Ast *)statement)->type = L_NODE_STMT;
	((Ast *)statement)->beg = beg;
	((Ast *)statement)->end = end;
	return (statement);
}

L_toplevel *
mk_toplevel(L_toplevel_kind kind,
    L_toplevel *next, int beg, int end)
{
	L_toplevel *toplevel;

	toplevel =
	    (L_toplevel *)ckalloc(sizeof(L_toplevel));
	memset(toplevel, 0, sizeof(L_toplevel));
	toplevel->next = next;
	toplevel->kind = kind;
	((Ast *)toplevel)->_trace = ast_trace_root;
	ast_trace_root = (void *)toplevel;
	((Ast *)toplevel)->line_no = L_line_number;
	((Ast *)toplevel)->type = L_NODE_TOPLEVEL;
	((Ast *)toplevel)->beg = beg;
	((Ast *)toplevel)->end = end;
	return (toplevel);
}

L_type *
mk_type(L_type_kind kind, L_expr *array_dim, L_expr *struct_tag,
    L_type *next_dim, L_var_decl *members, int typedef_p, int beg, int end)
{
	L_type *type;

	type = (L_type *)ckalloc(sizeof(L_type));
	memset(type, 0, sizeof(L_type));
	type->array_dim = array_dim;
	type->struct_tag = struct_tag;
	type->next_dim = next_dim;
	type->kind = kind;
	type->members = members;
	type->typedef_p = typedef_p;
	((Ast *)type)->_trace = ast_trace_root;
	ast_trace_root = (void *)type;
	((Ast *)type)->line_no = L_line_number;
	((Ast *)type)->type = L_NODE_TYPE;
	((Ast *)type)->beg = beg;
	((Ast *)type)->end = end;
	return (type);
}

L_var_decl *
mk_var_decl(L_type *type, L_expr *name,
    L_initializer *initial_value, int by_name, int extern_p, int rest_p,
    L_var_decl *next, int beg, int end)
{
	L_var_decl *variable_decl;

	variable_decl =
	    (L_var_decl *)ckalloc(sizeof(L_var_decl));
	memset(variable_decl, 0, sizeof(L_var_decl));
	variable_decl->name = name;
	variable_decl->initial_value = initial_value;
	variable_decl->type = type;
	variable_decl->next = next;
	variable_decl->by_name = by_name;
	variable_decl->extern_p = extern_p;
	variable_decl->rest_p = rest_p;
	((Ast *)variable_decl)->_trace = ast_trace_root;
	ast_trace_root = (void *)variable_decl;
	((Ast *)variable_decl)->line_no = L_line_number;
	((Ast *)variable_decl)->type =
	    L_NODE_VAR_DECL;
	((Ast *)variable_decl)->beg = beg;
	((Ast *)variable_decl)->end = end;
	return (variable_decl);
}

