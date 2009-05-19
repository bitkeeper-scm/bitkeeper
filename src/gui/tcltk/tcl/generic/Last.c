/*
 * used to be: tclsh gen-l-ast2.tcl to regenerate
 * As of Feb 2008 it is maintained by hand.
 */
#include "tclInt.h"
#include "Lcompile.h"

private void
ast_init(void *node, Node_k type, int beg, int end)
{
	Ast	*ast = (Ast *)node;

	ast->file   = L->file;
	ast->line   = L->line;
	ast->type   = type;
	ast->beg    = beg;
	ast->end    = end;
	ast->next   = L->ast_list;
	L->ast_list = (void *)ast;
}

Block *
ast_mkBlock(VarDecl *decls, Stmt *body, int beg, int end)
{
	Block	*block = (Block *)ckalloc(sizeof(Block));
	memset(block, 0, sizeof(Block));
	block->body  = body;
	block->decls = decls;
	ast_init(block, L_NODE_BLOCK, beg, end);
	return (block);
}

Expr *
ast_mkExpr(Expr_k kind, Op_k op, Expr *a, Expr *b, Expr *c, int beg, int end)
{
	Expr	*expr = (Expr *)ckalloc(sizeof(Expr));
	memset(expr, 0, sizeof(Expr));
	expr->a    = a;
	expr->b    = b;
	expr->c    = c;
	expr->kind = kind;
	expr->op   = op;
	ast_init(expr, L_NODE_EXPR, beg, end);
	return (expr);
}

ForEach *
ast_mkForeach(Expr *expr, Expr *key, Expr *value, Stmt *body,
	      int beg, int end)
{
	ForEach	*foreach = (ForEach *)ckalloc(sizeof(ForEach));
	memset(foreach, 0, sizeof(ForEach));
	foreach->expr  = expr;
	foreach->key   = key;
	foreach->value = value;
	foreach->body  = body;
	ast_init(foreach, L_NODE_FOREACH_LOOP, beg, end);
	return (foreach);
}

FnDecl *
ast_mkFnDecl(VarDecl *decl, Block *body, int beg, int end)
{
	FnDecl *fndecl = (FnDecl *)ckalloc(sizeof(FnDecl));
	memset(fndecl, 0, sizeof(FnDecl));
	fndecl->body = body;
	fndecl->decl = decl;
	ast_init(fndecl, L_NODE_FUNCTION_DECL, beg, end);
	return (fndecl);
}

Cond *
ast_mkIfUnless(Expr *expr, Stmt *if_body, Stmt *else_body, int beg, int end)
{
	Cond *cond = (Cond *)ckalloc(sizeof(Cond));
	memset(cond, 0, sizeof(Cond));
	cond->cond      = expr;
	cond->else_body = else_body;
	cond->if_body   = if_body;
	ast_init(cond, L_NODE_IF_UNLESS, beg, end);
	return (cond);
}

Loop *
ast_mkLoop(Loop_k kind, Expr *pre, Expr *cond, Expr *post, Stmt *body,
	   int beg, int end)
{
	Loop *loop = (Loop *)ckalloc(sizeof(Loop));
	memset(loop, 0, sizeof(Loop));
	loop->cond = cond;
	loop->post = post;
	loop->pre  = pre;
	loop->kind = kind;
	loop->body = body;
	ast_init(loop, L_NODE_LOOP, beg, end);
	return (loop);
}

Stmt *
ast_mkStmt(Stmt_k kind, Stmt *next, int beg, int end)
{
	Stmt *stmt = (Stmt *)ckalloc(sizeof(Stmt));
	memset(stmt, 0, sizeof(Stmt));
	stmt->next = next;
	stmt->kind = kind;
	ast_init(stmt, L_NODE_STMT, beg, end);
	return (stmt);
}

TopLev *
ast_mkTopLevel(Toplv_k kind, TopLev *next, int beg, int end)
{
	TopLev *toplev = (TopLev *)ckalloc(sizeof(TopLev));
	memset(toplev, 0, sizeof(TopLev));
	toplev->next = next;
	toplev->kind = kind;
	ast_init(toplev, L_NODE_TOPLEVEL, beg, end);
	return (toplev);
}

VarDecl *
ast_mkVarDecl(Type *type, Expr *id, int beg, int end)
{
	VarDecl *vardecl = (VarDecl *)ckalloc(sizeof(VarDecl));
	memset(vardecl, 0, sizeof(VarDecl));
	vardecl->id   = id;
	vardecl->type = type;
	ast_init(vardecl, L_NODE_VAR_DECL, beg, end);
	return (vardecl);
}

ClsDecl *
ast_mkClsDecl(VarDecl *decl, int beg, int end)
{
	ClsDecl *clsdecl = (ClsDecl *)ckalloc(sizeof(ClsDecl));
	memset(clsdecl, 0, sizeof(ClsDecl));
	clsdecl->decl   = decl;
	ast_init(clsdecl, L_NODE_CLASS_DECL, beg, end);
	return (clsdecl);
}

/* Build a default constructor if the user didn't provide one. */
FnDecl *
ast_mkConstructor(ClsDecl *class)
{
	char	*name;
	Type	*type;
	Expr	*id;
	VarDecl	*decl;
	Block	*block;
	FnDecl	*fn;

	type  = type_mkFunc(class->decl->type, NULL, PER_INTERP);
	name  = cksprintf("%s_new", class->decl->id->u.string);
	id    = ast_mkId(name, 0, 0);
	decl  = ast_mkVarDecl(type, id, 0, 0);
	decl->flags |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
		DECL_CLASS_CONST;
	decl->clsdecl = class;
	block = ast_mkBlock(NULL, NULL, 0, 0);
	fn    = ast_mkFnDecl(decl, block, 0, 0);

	return (fn);
}

/* Build a default destructor if the user didn't provide one. */
FnDecl *
ast_mkDestructor(ClsDecl *class)
{
	char	*name;
	Type	*type;
	Expr	*id, *self;
	VarDecl	*decl, *parm;
	Block	*block;
	FnDecl	*fn;

	self = ast_mkId("self", 0, 0);
	parm = ast_mkVarDecl(class->decl->type, self, 0, 0);
	parm->flags = SCOPE_LOCAL | DECL_LOCAL_VAR;
	type = type_mkFunc(L_void, parm, PER_INTERP);
	name = cksprintf("%s_delete", class->decl->id->u.string);
	id   = ast_mkId(name, 0, 0);
	decl = ast_mkVarDecl(type, id, 0, 0);
	decl->flags |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
		DECL_CLASS_DESTR;
	decl->clsdecl = class;
	block = ast_mkBlock(NULL, NULL, 0, 0);
	fn    = ast_mkFnDecl(decl, block, 0, 0);

	return (fn);
}

Expr *
ast_mkUnOp(Op_k op, Expr *e1, int beg, int end)
{
	return (ast_mkExpr(L_EXPR_UNOP, op, e1, NULL, NULL, beg, end));
}

Expr *
ast_mkBinOp(Op_k op, Expr *e1, Expr *e2, int beg, int end)
{
	return (ast_mkExpr(L_EXPR_BINOP, op, e1, e2, NULL, beg, end));
}

Expr *
ast_mkTrinOp(Op_k op, Expr *e1, Expr *e2, Expr *e3, int beg, int end)
{
	return (ast_mkExpr(L_EXPR_TRINOP, op, e1, e2, e3, beg, end));
}

Expr *
ast_mkConst(Type *type, int beg, int end)
{
	Expr *e = ast_mkExpr(L_EXPR_CONST, L_OP_NONE, NULL, NULL, NULL,
			    beg, end);
	e->type = type;
	return (e);
}

Expr *
ast_mkRegexp(char *re, int beg, int end)
{
	Expr *e = ast_mkExpr(L_EXPR_RE, L_OP_NONE, NULL, NULL, NULL, beg, end);
	e->u.string = re;
	e->type = L_string;
	return (e);
}

Expr *
ast_mkFnCall(Expr *id, Expr *arg_list, int beg, int end)
{
	Expr *e = ast_mkExpr(L_EXPR_FUNCALL, L_OP_NONE, id, arg_list, NULL,
			    beg, end);
	return (e);
}

Expr *
ast_mkId(char *name, int beg, int end)
{
	Expr *e = ast_mkExpr(L_EXPR_ID, L_OP_NONE, NULL, NULL, NULL, beg, end);
	e->u.string = ckstrdup(name);
	return (e);
}

private Type *
type_alloc(Type_k kind, enum typemk_k disposition)
{
	Type *type = (Type *)ckalloc(sizeof(Type));
	memset(type, 0, sizeof(Type));
	type->kind = kind;
	unless (disposition == PERSIST) {
		type->list   = L->type_list;
		L->type_list = type;
	}
	return (type);
}

Type *
type_mkScalar(Type_k kind, enum typemk_k disposition)
{
	Type *type = type_alloc(kind, disposition);
	return (type);
}

Type *
type_mkArray(Expr *size, Type *base_type, enum typemk_k disposition)
{
	Type *type = type_alloc(L_ARRAY, disposition);
	type->u.array.size = size;
	type->base_type    = base_type;
	return (type);
}

Type *
type_mkHash(Type *index_type, Type *base_type, enum typemk_k disposition)
{
	Type *type = type_alloc(L_HASH, disposition);
	type->u.hash.idx_type = index_type;
	type->base_type       = base_type;
	return (type);
}

Type *
type_mkStruct(char *tag, VarDecl *members, enum typemk_k disposition)
{
	Type *type = type_alloc(L_STRUCT, disposition);
	type->u.struc.tag     = ckstrdup(tag);
	type->u.struc.members = members;
	return (type);
}

Type *
type_mkNameOf(Type *base_type, enum typemk_k disposition)
{
	Type *type = type_alloc(L_NAMEOF, disposition);
	type->base_type = base_type;
	return (type);
}

Type *
type_mkFunc(Type *ret_type, VarDecl *formals, enum typemk_k disposition)
{
	Type *type = type_alloc(L_FUNCTION, disposition);
	type->base_type      = ret_type;
	type->u.func.formals = formals;
	return (type);
}

Type *
type_mkList(Type *a, enum typemk_k disposition)
{
	Type *type = type_alloc(L_LIST, disposition);
	type->base_type = a;
	return (type);
}

Type *
type_mkClass(enum typemk_k disposition)
{
	Type *type = type_alloc(L_CLASS, disposition);
	return (type);
}
