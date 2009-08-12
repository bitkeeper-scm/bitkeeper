/*
 * Type-checking helpers for the L programming language.
 *
 * Copyright (c) 2006-2008 BitMover, Inc.
 */
#include <stdio.h>
#include "tclInt.h"
#include "Lcompile.h"
#include "Lgrammar.h"

private int	typeck_decls(VarDecl *a, VarDecl *b);
private int	typeck_list(Type *a, Type *b);
private int	typeck_declType(Type *type, VarDecl *decl, int nameof_ok);

/*
 * Create the pre-defined types.  Init only once so all L scripts see
 * the same fundamental type pointers.  We persist these across all
 * time so we don't have to make them part of the L global state and
 * reference them as L->L_string instead of just L_string.
 */
void
L_typeck_init()
{
	static int initted = 0;

	unless (initted++) {
		L_int    = type_mkScalar(L_INT, PERSIST);
		L_float  = type_mkScalar(L_FLOAT, PERSIST);
		L_string = type_mkScalar(L_STRING, PERSIST);
		L_widget = type_mkScalar(L_WIDGET, PERSIST);
		L_void   = type_mkScalar(L_VOID, PERSIST);
		L_poly   = type_mkScalar(L_POLY, PERSIST);
	}
}

private	char	type_buf[80];
private int	notfirst;

private void
str_add(char *s)
{
	if (notfirst++) strncat(type_buf, " or ", sizeof(type_buf));
	strncat(type_buf, s, sizeof(type_buf));
}

private char *
type_str(Type_k kind)
{
	notfirst = type_buf[0] = 0;
	if (kind & L_INT)      str_add("int");
	if (kind & L_FLOAT)    str_add("float");
	if (kind & L_STRING)   str_add("string");
	if (kind & L_WIDGET)   str_add("widget");
	if (kind & L_VOID)     str_add("void");
	if (kind & L_POLY)     str_add("poly");
	if (kind & L_HASH)     str_add("hash");
	if (kind & L_STRUCT)   str_add("struct");
	if (kind & L_ARRAY)    str_add("array");
	if (kind & L_LIST)     str_add("list");
	if (kind & L_FUNCTION) str_add("function");
	if (kind & L_NAMEOF)   str_add("nameof");
	return (type_buf);
}

private void
pr_err(Type_k got, Type_k want, char *bef, char *aft, void *node)
{
	char buf[128];

	buf[0] = '\0';
	if (bef) snprintf(buf, sizeof(buf), "%s, ", bef);
	strncat(buf, "expected type ", sizeof(buf));
	strncat(buf, type_str(want), sizeof(buf));
	strncat(buf, " but got ", sizeof(buf));
	strncat(buf, type_str(got), sizeof(buf));
	if (aft) {
		strncat(buf, " ", sizeof(buf));
		strncat(buf, aft, sizeof(buf));
	}
	L_errf(node, buf);
}

void
L_typeck_deny(Type_k deny, Expr *expr)
{
	ASSERT(expr->type);

	if (L->options & L_OPT_POLY) return;

	if (expr->type->kind & deny) {
		L_errf(expr, "type %s illegal", type_str(expr->type->kind));
		expr->type = L_poly;  // minimize cascading errors
	}
}

void
L_typeck_expect(Type_k want, Expr *expr, char *msg)
{
	ASSERT(expr->type);

	if ((L->options & L_OPT_POLY) ||
	    ((expr->type->kind | want) & L_POLY)) return;

	unless (expr->type->kind & want) {
		pr_err(expr->type->kind, want, NULL, msg, expr);
		expr->type = L_poly;  // minimize cascading errors
	}
}

int
L_typeck_compat(Type *lhs, Type *rhs)
{
	if ((lhs->kind == L_POLY) || (rhs->kind == L_POLY)) {
		return (TRUE);
	}
	if (lhs->kind == L_FLOAT) {
		return (rhs->kind & (L_INT|L_FLOAT));
	} else {
		return (L_typeck_same(lhs, rhs));
	}
}

void
L_typeck_assign(Expr *lhs, Expr *rhs)
{
	if (L->options & L_OPT_POLY) return;
	unless (lhs && rhs) return;

	L_typeck_deny(L_VOID, lhs);
	L_typeck_deny(L_VOID, rhs);

	unless (L_typeck_compat(lhs->type, rhs->type)) {
		L_errf(lhs, "assignment of incompatible types");
	}
}

void
L_typeck_fncall(VarDecl *formals, Expr *call)
{
	int	i, rest_arg = 0;
	Expr	*actuals = call->b;

	if (L->options & L_OPT_POLY) return;

	for (i = 1; actuals && formals; ++i) {
		if (isexpand(actuals)) return;
		rest_arg = formals->flags & DECL_REST_ARG;  // is it "...id"?
		unless (L_typeck_compat(formals->type, actuals->type) ||
			rest_arg) {
			L_errf(call, "parameter %d has incompatible type", i);
		}
		actuals = actuals->next;
		formals = formals->next;
	}
	if (actuals && !rest_arg) {
		L_errf(call, "too many arguments for function %s",
		       call->a->u.string);
	}
	if (formals && !(formals->flags & DECL_REST_ARG)) {
		L_errf(call, "not enough arguments for function %s",
		       call->a->u.string);
	}
}

/*
 * Check that a declaration uses legal types.  This basically checks
 * for voids and name-of anywhere in the type where they aren't allowed.
 */
int
L_typeck_declType(VarDecl *decl)
{
	return (typeck_declType(decl->type, decl, FALSE));
}
private int
typeck_declType(Type *type, VarDecl *decl, int nameof_ok)
{
	int	ret = 1;
	char	*s = NULL;
	VarDecl	*v;

	switch (type->kind) {
	    case L_VOID:
		s = "void";
		ret = 0;
		break;
	    case L_FUNCTION:
		/* First check the return type.  Void is legal here. */
		unless (isvoidtype(type->base_type)) {
			ret = typeck_declType(type->base_type, decl, FALSE);
		}
		/* Now look at the formals. */
		v = type->u.func.formals;
		for (v = type->u.func.formals; v; v = v->next) {
			/* To type-check all formals, don't short-circuit. */
			ret = typeck_declType(v->type, v, TRUE) && ret;
		}
		break;
	    case L_NAMEOF:
		if (nameof_ok) {
			/* Pass FALSE since name-of of a name-of is illegal. */
			ret = typeck_declType(type->base_type, decl, FALSE);
		} else {
			s = "name-of";
			ret = 0;
		}
		break;
	    case L_ARRAY:
		ret = typeck_declType(type->base_type, decl, FALSE);
		break;
	    case L_HASH:
		ret = typeck_declType(type->base_type, decl, FALSE) &&
		      typeck_declType(type->u.hash.idx_type, decl, FALSE);
		break;
	    case L_STRUCT:
		for (v = type->u.struc.members; v; v = v->next) {
			/* To type-check all members, don't short-circuit. */
			ret = typeck_declType(v->type, v, FALSE) && ret;
		}
		break;
	    default:
		break;
	}
	if (s) {
		if (decl->id) {
			L_errf(decl, "type %s illegal in declaration of '%s'",
			       s, decl->id->u.string);
		} else {
			L_errf(decl, "type %s illegal", s);
		}
	}
	return (ret);
}

/*
 * Determine if two declaration lists have structurally equivalent
 * type declarations.
 */
private int
typeck_decls(VarDecl *a, VarDecl *b)
{
	for (; a && b; a = a->next, b = b->next) {
		unless (L_typeck_same(a->type, b->type)) return (0);
	}
	/* Not the same if one has more declarations. */
	return !(a || b);
}

/*
 * Determine if something is structurally compatible with a list type.
 */
private int
typeck_list(Type *a, Type *b)
{
	Type	*l, *t;
	VarDecl	*m;

	ASSERT((a->kind == L_LIST) || (b->kind == L_LIST));

	/* If only one of a,b is a list, put that in "l". */
	if (a->kind == L_LIST) {
		l = a;
		t = b;
	} else {
		l = b;
		t = a;
	}

	switch (t->kind) {
	    case L_ARRAY:
		/*
		 * A list type is compatible with an array type iff all the
		 * list elements have the same type as the array base type.
		 */
		for (; l; l = l->next) {
			ASSERT(l->kind == L_LIST);
			unless (L_typeck_compat(l->base_type, t->base_type)) {
				return (0);
			}
		}
		return (1);
	    case L_STRUCT:
		/*
		 * A list type is compatible with a struct type iff the list
		 * element types match up with the struct member types.
		 */
		m = t->u.struc.members;
		while (m && l) {
			ASSERT(l->kind == L_LIST);
			unless (L_typeck_compat(l->base_type, m->type)) {
				return (0);
			}
			m = m->next;
			l = l->next;
		}
		return !(l || m);  // not the same if one has more elements
	    case L_LIST:
		/*
		 * Two list types are compatible iff element types
		 * match up, although one can have more.
		 */
		for (; t && l; t = t->next, l = l->next) {
			unless (L_typeck_same(l->base_type, t->base_type)) {
				return (0);
			}
		}
		return (1);
	    default:
		return (0);
	}
}

/*
 * Determine if two types are structurally equivalent.  Note that
 * polys match anything and strings and widgets are compatible.
 */
int
L_typeck_same(Type *a, Type *b)
{
	unless (a && b) return (0);

	/* Polys match anything. */
	if ((a->kind == L_POLY) || (b->kind == L_POLY)) return (1);

	/* Strings and widgets are compatible. */
	if ((a->kind & (L_STRING|L_WIDGET)) && (b->kind & (L_STRING|L_WIDGET))){
		return (1);
	}

	if ((a->kind == L_LIST) || (b->kind == L_LIST)) {
		return (typeck_list(a, b));
	}

	unless (a->kind == b->kind) return (0);

	switch (a->kind) {
	    case L_INT:
	    case L_FLOAT:
	    case L_STRING:
	    case L_WIDGET:
	    case L_VOID:
		return (1);
	    case L_ARRAY:
		/* Element types must match (array sizes are ignored). */
		return (L_typeck_same(a->base_type, b->base_type));
	    case L_HASH:
		/* Element types must match and index types must match. */
		return (L_typeck_same(a->base_type, b->base_type) &&
			L_typeck_same(a->u.hash.idx_type, b->u.hash.idx_type));
	    case L_STRUCT:
		/* Struct members must match in type and number
		 * but member names can be different. */
		return (typeck_decls(a->u.struc.members, b->u.struc.members));
	    case L_NAMEOF:
		return (L_typeck_same(a->base_type, b->base_type));
	    case L_FUNCTION:
		/* Return types must match and all arg types must match. */
		return (L_typeck_same(a->base_type, b->base_type) &&
			typeck_decls(a->u.func.formals, b->u.func.formals));
	    case L_CLASS:
		/* Must be the same class. */
		return (a == b);
	    default:
		L_bomb("bad type kind in L_typeck_same");
		return (0);
	}
}
