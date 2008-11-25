/* A typechecker for the L programming language.  In case not enough type
 * information is available, typechecks are queued up to try again at the end
 * of compilation.  That way it doesn't matter which order functions appear
 * in.
 *
 * Copyright (c) 2006-2007 BitMover, Inc.
 */
#include <stdio.h>
#include "tclInt.h"
#include "Lcompile.h"
#include "Lgrammar.h"
#include "Last.h"


#define CHECK_ARG_COUNT	1	/* mark a queued check as an arg count
				 * check */

/* A delayed type check */
typedef struct queued_check {
	int	flags;		// Set for special cases, like
				// argument count checks
	L_type	*want;		// The required type
	L_type	*have;		// The actual type
	/* In case :want is unknown, these three will be set: */
	char	*name;		// The function name
	int	pos;		// The argument position
	/* In case :have is unknown, the expression to get it from */
	L_expr	*expr;
	/* A pointer so we can link the checks together. */
	struct queued_check *next;
} queued_check;

/* The type of a function */
typedef struct function_type {
	L_type	*return_type;	// The return type
	int	param_count;	// How many parameters
	int	var_arity_p;	// True if # of args can be >= param_count - 1
	L_type	**param_types;	// An array of types, one for each parameter. 
} function_type;

/* A list of type checks to be performed later. */
private queued_check *queued_checks = NULL;

/* A table mapping function names to function types */
private Tcl_HashTable *function_types = NULL;

extern L_compile_frame *lframe;

/* Return codes from subtype */
typedef enum { NOT_SUBTYPE, UNKNOWN, IS_SUBTYPE } type_relation;

private type_relation subtype(L_type *have, L_type *want);
private L_type	*parameter_type(char *name, int pos);
private L_type	*return_type(char *name);
private function_type *get_function_type(char *name);
private void	L_type_error(L_expr *expr, L_type *have, L_type *want);
private L_type	*binop_expr_type(L_expr *expr);
private L_type	*unop_expr_type(L_expr *expr);
private void	free_type_info();
private L_expr	*copy_index_expr(L_expr *expr);

/* A convenience wrapper around L_check_expr_type() that instantiates
 * the type for you. */
void
L_check_expr_kind(
	L_type_kind want,	/* The kind of the expected type */
	L_expr	*expr)		/* The expression from which we derive
				 * the actual type */
{
	L_type *type;

	if (lframe->options & L_OPT_POLY) return;
	type = mk_type(want, NULL, NULL, NULL, NULL, FALSE, 0, 0);
	L_check_expr_type(type, expr);
}

/* Ensure that the type of :expr is compatible with the :want type.
 * If not, emit a type error.  In certain cases, such as when :expr is
 * a function call, L_check_expr_type() queues up the check to be performed
 * by L_finish_typechecks(). */
void
L_check_expr_type(
	L_type	*want,		/* The expected type */
	L_expr	*expr)		/* The expression from which we derive
				 * the actual type */
{
	L_type	*have;
	queued_check *new;

	if (lframe->options & L_OPT_POLY) return;
	unless (want) L_bomb("typecheck: Missing want type");
	have = L_expr_type(expr);
	switch (subtype(have, want)) {
	    case NOT_SUBTYPE:
		L_type_error(expr, have, want);
		break;
	    case IS_SUBTYPE:
		break;
	    case UNKNOWN:
		new = (queued_check *)ckalloc(sizeof(queued_check));
		memset(new, 0, sizeof(queued_check));
		new->want = want;
		new->expr = expr;
		new->next = queued_checks;
		queued_checks = new;
		break;
	    default:
		L_bomb("typecheck: bad retval from subtype()");
	}
}

/* Ensure that the type of :expr is compatible with the :want type.
 * If not, emit a type error. This form is useful when the "have" type
 * is available only as a type structure, such as with arrays or
 * hashes. */
void
L_check_type(
	L_type	*want,		/* The expected type */
	L_type	*have,		/* The actual type */
	L_expr	*expr)		/* Expression to use for err msg */
{
	if (lframe->options & L_OPT_POLY) return;
	unless (want) L_bomb("typecheck: Missing want type");
	switch (subtype(have, want)) {
	    case NOT_SUBTYPE:
		L_type_error(expr, have, want);
		break;
	    case IS_SUBTYPE:
	    case UNKNOWN:
		break;
	    default:
		L_bomb("typecheck: bad retval from subtype()");
	}
}

/* Functions don't need to be declared before they're used, so
 * L_check_arg_type() just queues up argument typechecks.
 * L_finish_typechecks() below will do the actual checks. */
void
L_check_arg_type(
	char	*funcname,	/* The name of the function being
				 * called. */
	int	pos,		/* The position of this argument in the
				 * arglist of the function being called.
				 * Starts at 1.	 0 means no arguments. */
	L_expr	*expr)		/* The argument itself. */
{
	queued_check *new;

	if (lframe->options & L_OPT_POLY) return;
	new = (queued_check *)ckalloc(sizeof(queued_check));
	memset(new, 0, sizeof(queued_check));
	new->have = L_expr_type(expr);
	new->expr = expr;
	new->name = funcname;
	new->pos = pos;
	new->next = queued_checks;
	queued_checks = new;
}

/* Check that the number of actual parameters matches the number of formal
 * parameters. */
void
L_check_arg_count(
	char	*funcname,	/* The name of the function being
				 * called. */
	int	count,		/* The number of args passed */
	L_expr	*expr)		/* An expression to grab location info from */
{
	queued_check *new;
	new = (queued_check *)ckalloc(sizeof(queued_check));
	memset(new, 0, sizeof(queued_check));
	new->flags = CHECK_ARG_COUNT;
	new->pos = count;
	new->name = funcname;
	new->expr = expr;
	new->next = queued_checks;
	queued_checks = new;
}


/* Store the return type and parameter types of a function, so that
 * they can be checked by L_finish_typechecks(). */
void
L_store_fun_type(
	L_function_decl *fun) /* The function declaration from
				  * which we get the type info. */
{
	function_type *fun_type;
	L_var_decl *p;
	int	param_count, freshp;
	Tcl_HashEntry *hPtr;

	fun_type = (function_type *)ckalloc(sizeof(function_type));
	memset(fun_type, 0, sizeof(function_type));

	fun_type->return_type = fun->return_type;

	for (param_count = 0, p = fun->params; p; param_count++, p = p->next) {
		if (p->rest_p) fun_type->var_arity_p = TRUE;
	}
	fun_type->param_count = param_count;

	fun_type->param_types =
	    (L_type **)ckalloc(sizeof(L_type *) * param_count);
	for (param_count = 0, p = fun->params; p; param_count++, p = p->next) {
		fun_type->param_types[param_count] = p->type;
	}

	if (function_types == NULL) {
		function_types =
		    (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
		Tcl_InitHashTable(function_types, TCL_STRING_KEYS);
	}
	hPtr = Tcl_CreateHashEntry(function_types,
	    fun->name->u.string, &freshp);
	unless (freshp) {
		L_bomb("typecheck: Redefinition of function type for %s",
		    fun->name->u.string);
	} else {
		Tcl_SetHashValue(hPtr, fun_type);
	}
}

/* Perform type checks that have been queued up by L_check_expr_type() and
 * L_check_arg_type().	Emit an error for each type mismatch.  Also frees up
 * the memory allocated by the typechecker. */
void
L_finish_typechecks(void)
{
	queued_check *q;

	for (q = queued_checks; q; q = q->next) {
		if (q->flags == CHECK_ARG_COUNT) {
			function_type *fun_type = get_function_type(q->name);
			if (fun_type) {
				if ((!fun_type->var_arity_p &&
					fun_type->param_count > q->pos) ||
				    (fun_type->var_arity_p &&
					!(q->pos >=
					    (fun_type->param_count - 1)))) {
					L_errorf(q->expr, "Not enough arguments"
					    " for function %s", q->name);
				} else if (fun_type->param_count < q->pos &&
				    !fun_type->var_arity_p) {
					L_errorf(q->expr, "Too many arguments"
					    " for function %s", q->name);
				}
			} else {
				L_trace("couldn't check argcount for %s",
				    q->name);
			}
		} else {
			if (q->have == NULL) q->have = L_expr_type(q->expr);
			if (q->want == NULL) {
				q->want = parameter_type(q->name, q->pos);
			}
			switch (subtype(q->have, q->want)) {
			    case NOT_SUBTYPE:
				L_type_error(q->expr, q->have, q->want);
				break;
			    case IS_SUBTYPE:
				L_trace("queued check succeeded");
				break;
			    case UNKNOWN:
				L_trace("typecheck: "
				    "Couldn't typecheck expression. "
				    "Have is %s, want is %s. Line %d\n",
				    q->have ? "set" : "NULL",
				    q->want ? "set" : "NULL",
				    q->expr ? ((Ast *)q->expr)->line_no : -1);
				break;
			    default:
				L_bomb("typecheck: bad retval from subtype()");
			}
		}
	}
	free_type_info();
}

/* Determine whether an expression is of void type. If the type is
 * unknown, don't call it a void since it could be a call to an
 * external function of unknown type. */
int
L_expr_is_void(L_expr *expr)
{
	L_type	*type;

	if (!expr || !(type = L_expr_type(expr))) return (0);
	return (type->kind == L_TYPE_VOID);
}

private type_relation
subtype(L_type *have, L_type *want)
{
	if (!have || !want) return (UNKNOWN);

	/* XXX Actually, once L_TYPE_VAR is implemented, we shouldn't
	 * see it here. */
	if (want->kind != L_TYPE_POLY && want->kind != L_TYPE_VAR) {
		if (want->kind == L_TYPE_NUMBER) {
			if (have->kind != L_TYPE_FLOAT &&
			    have->kind != L_TYPE_INT) {
				return (NOT_SUBTYPE);
			}
		} else if (want->kind == L_TYPE_FLOAT &&
		    have->kind == L_TYPE_INT) {
			/*
			 * Allow int -> float coercion.  No byte codes
			 * need to be generated because tcl
			 * automatically does the coercion.
			 */
			return (IS_SUBTYPE);
		} else if (want->kind != have->kind) {
			return (NOT_SUBTYPE);
		}
		if (want->kind == L_TYPE_STRUCT) {
			if (want->struct_tag && have->struct_tag) {
				/* Note that since we define struct
				 * subtyping by checking type tags,
				 * untagged structs are pairwise
				 * disjoint... */
				if (strcmp(want->struct_tag->u.string,
					have->struct_tag->u.string)) {
					return (NOT_SUBTYPE);
				}
			} else {
				return (NOT_SUBTYPE);
			}
		}
		/* We don't check if the dimensions of two array types
		 * are all the same.  Since the size of arrays can
		 * change anyway, it seems pointless.  We don't check
		 * the number of dimensions, either, though that would
		 * be more pointful. */
		if ((want->next_dim && !have->next_dim) ||
		    (!want->next_dim && have->next_dim)) {
			return (NOT_SUBTYPE);
		}
	}
	return (IS_SUBTYPE);
}

/* Using structure type equivalence, determine whether two types
 * are the same. */
int
L_same_type(L_type *a, L_type *b)
{
	int		na, nb;
	L_type		*ta, *tb;
	L_var_decl	*ma, *mb;

	unless (a && b) return (0);

	switch (a->kind) {
	    case L_TYPE_INT:
	    case L_TYPE_FLOAT:
	    case L_TYPE_STRING:
	    case L_TYPE_NUMBER:
	    case L_TYPE_WIDGET:
	    case L_TYPE_VOID:
	    case L_TYPE_POLY:
	    case L_TYPE_VAR:
		return (a->kind == b->kind);
	    case L_TYPE_ARRAY:
		/* Array dimensions and element type must match. */
		if (a->array_dim && b->array_dim) {
			na = a->array_dim->u.integer;
			nb = b->array_dim->u.integer;
			unless (na == nb) return (0);
		} else {
			/* If one has no dimension so must the other. */
			if (a->array_dim || b->array_dim) return (0);
		}
		return (L_same_type(a->next_dim, b->next_dim));
	    case L_TYPE_HASH:
		/* Element types and index types must match. */
		ta = (L_type *)a->array_dim;
		tb = (L_type *)b->array_dim;
		return (L_same_type(ta, tb) &&
			L_same_type(a->next_dim, b->next_dim));
	    case L_TYPE_STRUCT:
		/* Struct members must match in type and number
		 * but names can be different. */
		ma = a->members;
		mb = b->members;
		while (ma && mb) {
			unless (L_same_type(ma->type, mb->type)) return 0;
			ma = ma->next;
			mb = mb->next;
		}
		/* Not the same if one has more members. */
		return (!ma && !mb);
	    default:
		L_bomb("bad type kind in L_same_type");
		return (0);
	}
}

private void
tab(int tabs)
{
	while (tabs--) fprintf(stderr, "\t");
}

/* handy debugging type dumper thingy.	say elucidate_type(type, 0); */
private void
elucidate_type(L_type *type, int tabs)
{
	tab(tabs);
	fprintf(stderr, "type's kind is %s\n", L_type_tostr[type->kind]);
	if (type->next_dim) {
		tab(tabs);
		fprintf(stderr, "type's next dim is:\n");
		elucidate_type(type->next_dim, tabs + 1);
	}
	if (type->kind == L_TYPE_STRUCT) {
		L_var_decl *mem;
		tab(tabs);
		fprintf(stderr, "type has struct members:\n");
		for (mem = type->members;  mem; mem = mem->next) {
			tab(tabs);
			fprintf(stderr, "%s", mem->name->u.string);
			elucidate_type(mem->type, tabs + 1);
		}
	}
	tab(tabs);
	fprintf(stderr, ";\n");
}

/* Return the type of an expression. */
L_type *
L_expr_type(L_expr *expr)
{
	L_symbol *symbol;
	L_type	*type = NULL;
	L_expr	*index;

	unless (expr) return (NULL);
	switch (expr->kind) {
	    case L_EXPR_FUNCALL:
		unless (symbol = L_get_symbol(expr->a, FALSE)) {
			type = return_type(expr->a->u.string);
		}
		break;
	    case L_EXPR_PRE:
	    case L_EXPR_POST:
	    case L_EXPR_INTEGER:
		type = mk_type(L_TYPE_INT, NULL, NULL, NULL, NULL, FALSE, 0, 0);
		break;
	    case L_EXPR_STRING:
	    case L_EXPR_INTERPOLATED_STRING:
	    case L_EXPR_REGEXP:
		type = mk_type(L_TYPE_STRING, NULL, NULL, NULL, NULL, FALSE,
			       0, 0);
		break;
	    case L_EXPR_FLOTE:
		type = mk_type(L_TYPE_FLOAT, NULL, NULL, NULL, NULL, FALSE,
			       0, 0);
		break;
	    case L_EXPR_UNARY:
		type = unop_expr_type(expr);
		break;
	    case L_EXPR_BINARY:
		type = binop_expr_type(expr);
		break;
	    case L_EXPR_VAR:
		index = copy_index_expr(expr->indices);
		if ((symbol = L_get_symbol(expr->a, FALSE))) {
			type = symbol->type;
		} else {
			L_trace("unable to find definition for symbol %s, "
			    "giving up", expr->a->u.string);
			type = mk_type(L_TYPE_POLY, NULL, NULL, NULL, NULL,
			    FALSE, 0, 0);
			break;
		}
		while (index) {
			switch (index->kind) {
			    case L_EXPR_ARRAY_INDEX:
			    case L_EXPR_HASH_INDEX:
				L_trace(index->kind == L_EXPR_HASH_INDEX
				    ? "hash index" : "array index");
				if (type && type->next_dim) {
					type = type->next_dim;
				} else {
					L_trace("not enough dimensions "
					    "in type for index, giving up");
					type = mk_type(L_TYPE_POLY, NULL, NULL,
						       NULL, NULL, FALSE, 0, 0);
				}
				break;
			    case L_EXPR_STRUCT_INDEX: {
				    L_var_decl *member;
				    int memberOffset;

				    L_trace("struct index");
				    member = L_get_struct_member(type,
					index, &memberOffset);
				    if (member) {
					    type = member->type;
				    } else {
					    L_trace("not enough dimensions in "
						"type for struct index, "
						"giving up");
					    type = mk_type(L_TYPE_POLY, NULL,
							   NULL, NULL, NULL,
							   FALSE, 0, 0);
				    }
				    break;
			    }
			    default:
				L_bomb("malformed AST in L_expr_type()");
			}
			index = index->indices;
		}
		break;
	    default:
		L_bomb("typecheck: Unknown expression type %d", expr->kind);
	}
	return (type);
}

private L_expr *
copy_index_expr(L_expr *expr)
{
	L_expr	*copy, *runner;

	unless (expr) return (0);
	copy = (L_expr *)ckalloc(sizeof(L_expr));
	memcpy(copy, expr, sizeof(L_expr));
	for (runner = copy, expr = expr->indices; expr;
	     expr = expr->indices, runner = runner->indices) {
		runner->indices = (L_expr *)ckalloc(sizeof(L_expr));
		memcpy(runner->indices, expr, sizeof(L_expr));
	}
	return (copy);
}

/* Return the type of an expression with a unary operator in it. */
private L_type *
unop_expr_type(L_expr *expr)
{
	L_type	*type, *type1;

	type = mk_type(L_TYPE_POLY, NULL, NULL, NULL, NULL, FALSE, 0, 0);
	switch (expr->op) {
	    case T_TCL_CAST:
	    case T_STRING_CAST:
		type->kind = L_TYPE_STRING;
		break;
	    case T_FLOAT_CAST:
		type->kind = L_TYPE_FLOAT;
		break;
	    case T_HASH_CAST:
		type->kind = L_TYPE_HASH;
		break;
	    case T_WIDGET_CAST:
		type->kind = L_TYPE_WIDGET;
		break;
	    case T_BANG:
	    case T_BITNOT:
	    case T_INT_CAST:
	    case T_DEFINED:
		type->kind = L_TYPE_INT;
		break;
	    case T_BITAND:
		type = L_expr_type(expr->a);
		break;
	    case T_PLUS:
	    case T_MINUS:
		type1 = L_expr_type(expr->a);
		unless (type1) return NULL;
		if (type1->kind == L_TYPE_FLOAT) {
			type->kind = L_TYPE_FLOAT;
		} else {
			type->kind = L_TYPE_INT;
		}
		break;
	    default:
		L_bomb("typecheck: Unknown unary operator %d", expr->op);
	}
	return (type);
}

/* Return the type of an expression with a binary operator in it. */
private L_type *
binop_expr_type(L_expr *expr)
{
	L_type	*type, *type1, *type2;

	type = mk_type(L_TYPE_POLY, NULL, NULL, NULL, NULL, FALSE, 0, 0);
	switch (expr->op) {
	    case T_EQUALS:
		type = L_expr_type(expr->b);
		break;
	    case T_PLUS:
	    case T_PLUSPLUS:
	    case T_EQPLUS:
	    case T_MINUS:
	    case T_MINUSMINUS:
	    case T_EQMINUS:
	    case T_STAR:
	    case T_EQSTAR:
	    case T_SLASH:
	    case T_EQSLASH:
		type1 = L_expr_type(expr->a);
		type2 = L_expr_type(expr->b);
		if (!type1 || !type2) return (NULL);
		if (type1->kind == L_TYPE_FLOAT ||
		    type2->kind == L_TYPE_FLOAT) {
			type->kind = L_TYPE_FLOAT;
		} else {
			type->kind = L_TYPE_INT;
		}
		break;
	    case T_BITAND:
	    case T_EQBITAND:
	    case T_BITOR:
	    case T_EQBITOR:
	    case T_BITXOR:
	    case T_EQBITXOR:
	    case T_LSHIFT:
	    case T_EQLSHIFT:
	    case T_RSHIFT:
	    case T_EQRSHIFT:
	    case T_PERC:
	    case T_EQPERC:
	    case T_ANDAND:
	    case T_OROR:
	    case T_EQUALEQUAL:
	    case T_NOTEQUAL:
	    case T_GREATER:
	    case T_GREATEREQ:
	    case T_LESSTHAN:
	    case T_LESSTHANEQ:
		type->kind = L_TYPE_INT;
		break;
	    case T_EQ:
	    case T_NE:
	    case T_GT:
	    case T_GE:
	    case T_LT:
	    case T_LE:
	    case T_EQTWID:
		type->kind = L_TYPE_STRING;
		break;
	    default:
		L_bomb("typecheck: Unknown binary operator %d", expr->op);
	}
	return (type);
}


/* Return the want type for a function parameter.  Returns NULL if nothing is
 * known about the parameter, or it is out of bounds. */
private L_type *
parameter_type(
	char	*name,		/* The name of the function */
	int	pos)		/* The position in the argument list, starting
				 * at 1 -- 0 means no arguments. */
{
	function_type *fun_type;

	unless (fun_type = get_function_type(name)) return (0);
	if (fun_type->var_arity_p && pos >= (fun_type->param_count - 1)) {
		/* for rest parameters, return a type that always succeeds: */
		return (mk_type(L_TYPE_POLY, NULL, NULL, NULL, NULL, FALSE,
				0, 0));
	} else if (pos < fun_type->param_count) {
		return (fun_type->param_types[pos]);
	} else {
		L_trace("parameter %d out of range 0..%d for function %s",
		    pos, fun_type->param_count, name);
	}
	return (0);
}

/* Look up the return type for a function.  Returns NULL if nothing is
 * known about the function. */
private L_type *
return_type(char *name)
{
	function_type *fun_type;

	unless (fun_type = get_function_type(name)) {
		L_trace("checking return type failed");
		return (0);
	}
	return (fun_type->return_type);
}


/* Lookup the type of a function.  Return null if the type is not
 * known. */
private function_type *
get_function_type(char *name)
{
	Tcl_HashEntry *hPtr;

	unless (function_types) {
		L_trace("typecheck: No function types found.");
		return (0);
	}
	if ((hPtr = Tcl_FindHashEntry(function_types, name))) {
		return ((function_type *)Tcl_GetHashValue(hPtr));
	} else {
		L_trace("typecheck: No function type found for %s", name);
		return (0);
	}
}

/* Release memory used by the type checker */
private void
free_type_info(void)
{
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch hSearch;
	function_type *f;
	queued_check *q;

	while (queued_checks) {
		q = queued_checks->next;
		ckfree((char *)queued_checks);
		queued_checks = q;
	}
	if (function_types != NULL) {
		for (hPtr = Tcl_FirstHashEntry(function_types, &hSearch);
		     hPtr != NULL;
		     hPtr = Tcl_NextHashEntry(&hSearch)) {
			f = (function_type *)Tcl_GetHashValue(hPtr);
			ckfree((char *)f->param_types);
			ckfree((char *)f);
		}
		Tcl_DeleteHashTable(function_types);
		function_types = NULL;
	}
}

/* Generate a type error */
private void
L_type_error(
	L_expr	*expr,		/* The erroneous expression */
	L_type	*have,		/* The expected type */
	L_type	*want)		/* The erroneous type */
{
	L_errorf(expr, "type error, want %s, got %s",
	    L_type_tostr[want->kind], L_type_tostr[have->kind]);
}
