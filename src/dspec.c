#include "sccs.h"

typedef enum {
	T_RPAREN = 1,
	T_EQTWID,
	T_EQUALS,
	T_NOTEQ,
	T_EQ,
	T_NE,
	T_GT,
	T_LT,
	T_GE,
	T_LE,
	T_AND,
	T_OR,
	T_EOF,
} dspec_op_t;

typedef	struct
{
	int	i;
	char	*ret;
	symbol	*sym;
} dspec_getnext_t;

/* Globals for dspec code below. */
private struct {
	char	*c;		/* current location in dspec */
	char	*start;		/* start of dspec buffer */
	MDBM	*eachvals;	/* hash of $each variables' current vals */
	FILE	*out;		/* FILE* to receive output */
	char	***buf;		/* lines array to receive output */
	datum	each;		/* current value of $each variable */
	int	line;		/* current line in $each iteration */
	int	tcl;		/* whether we are inside a $tcl construct */
	sccs	*s;
	delta	*d;
} g;

private void	dspec_err(char *msg);
private void	dspec_evalId(FILE *out, char *** buf);
private void	dspec_evalParenid(FILE *out, char ***buf, datum id);
private int	dspec_expr(void);
private int	dspec_expr2(dspec_op_t *next_tok);
private dspec_getnext_t *dspec_getnext(datum kw, dspec_getnext_t *state);
private int	dspec_getParenid(datum *id);
private void	dspec_stmtList(int output);
private char	**dspec_str(dspec_op_t *next_tok);

void
dspec_eval(FILE * out, char ***buf, sccs *s, delta *d, char *start)
{
	bzero(&g, sizeof(g));
	g.out	= out;
	g.buf	= buf;
	g.s	= s;
	g.d	= d;
	g.c	= start;
	g.start = start;

	dspec_stmtList(1);
}

/*
 * This is a recursive-descent parser that implements the following
 * grammar for dspecs (where [[...]] indicates an optional clause
 * and {{...}} indicates 0 or more repetitions of):
 *
 * <stmt_list> -> {{ <stmt> }}
 * <stmt>      -> $if(<expr>){<stmt_list>}[[$else{<stmt_list>}]]
 *	       -> $unless(<expr>){<stmt_list>}[[$else{<stmt_list>}]]
 *	       -> $each(:ID:){<stmt_list>}
 *	       -> char
 *	       -> escaped_char
 *	       -> :ID:
 *	       -> (:ID:)
 * <expr>      -> <expr2> {{ <logop> <expr2> }}
 * <expr2>     -> <str> <relop> <str>
 *	       -> <str>
 *	       -> (<expr>)
 * <str>       -> {{ <atom> }}
 * <atom>      -> char
 *	       -> escaped_char
 *	       -> :ID:
 *	       -> (:ID:)
 * <logop>     -> " && " | " || "
 * <relop>     -> "=" | "!=" | "=~" | "!~"
 *	       -> " -eq " | " -ne " | " -gt " | " -ge " | " -lt " | " -le "
 *
 * This grammar is ambiguous due to (:ID:) loooking like a
 * parenthesized sub-expression.  The code tries to parse (:ID:) first
 * as an $each variable, then as a regular :ID:, then as regular text.
 *
 * The following procedures can be thought of as implementing an
 * attribute grammar where the output parameters are synthesized
 * attributes which hold the expression values and the next token
 * of lookahead in some cases.	It has been written for speed.
 */

private void
dspec_stmtList(int output)
{
	int		c;
	FILE		*out;
	char		***buf;
	datum		id;
	static int	depth = 0;
	static int	in_each = 0;

	/*
	 * output==1 means evaluate and output.	 output==0 means
	 * evaluate but throw away.
	 */
	if (output) {
		out = g.out;
		buf = g.buf;
	} else {
		out = 0;
		buf = 0;
	}

	++depth;
	while (*g.c) {
		if (g.c[0] == '$') {
			++g.c;
			if ((g.c[0] == 'i') && (g.c[1] == 'f') &&
			    (g.c[2] == '(')) {
				g.c += 3;
				c = dspec_expr() && output;
				if (*g.c++ != ')') dspec_err("missing )");
				if (*g.c++ != '{') dspec_err("missing {");
				dspec_stmtList(c && output);
				if (*g.c++ != '}') dspec_err("missing }");
				if ((g.c[0] == '$') && (g.c[1] == 'e') &&
				    (g.c[2] == 'l') && (g.c[3] == 's') &&
				    (g.c[4] == 'e') && (g.c[5] == '{')) {
					g.c += 6;
					dspec_stmtList(!c && output);
					if (*g.c++ != '}') dspec_err("missing }");
				}
			} else if ((g.c[0] == 'e') && (g.c[1] == 'a') &&
				   (g.c[2] == 'c') && (g.c[3] == 'h') &&
				   (g.c[4] == '(')) {
				dspec_getnext_t	state;
				char		*bufptr;
				datum		k;

				if (in_each++) dspec_err("nested each illegal");

				g.c += 5;
				if (*g.c++ != ':') dspec_err("missing id");

				/* Extract the id in $each(:id:) */
				k.dptr = g.c;
				while (*g.c && (*g.c != ':')) ++g.c;
				unless (*g.c) dspec_err("premature eof");
				k.dsize = g.c - k.dptr;
				++g.c;

				if (*g.c++ != ')') dspec_err("missing )");
				if (*g.c++ != '{') dspec_err("missing {");

				unless (g.eachvals) {
					g.eachvals = mdbm_open(0, 0, 0,
							       GOOD_PSIZE);
					unless (g.eachvals) return;
				}
				/*
				 * Re-evaluate the $each body for each
				 * line of the $each variable.
				 */
				state.i	  = 1;
				state.ret = 0;
				state.sym = 0;
				bufptr	  = g.c;
				g.line	  = 1;
				while (dspec_getnext(k, &state)) {
					unless (state.ret) continue;
					g.each.dptr  = state.ret;
					g.each.dsize = strlen(g.each.dptr);
					mdbm_store(g.eachvals, k, g.each,
						   MDBM_REPLACE);
					g.c = bufptr;
					dspec_stmtList(output);
					++g.line;
				}
				mdbm_delete(g.eachvals, k);
				--in_each;
				/* Eat the body if we never parsed it above. */
				if (g.line == 1) dspec_stmtList(0);
				if (*g.c++ != '}') dspec_err("missing }");
			} else if ((g.c[0] == 'u') && (g.c[1] == 'n') &&
				   (g.c[2] == 'l') && (g.c[3] == 'e') &&
				   (g.c[4] == 's') && (g.c[5] == 's') &&
				   (g.c[6] == '(')) {
				g.c += 7;
				c = !dspec_expr() && output;
				if (*g.c++ != ')') dspec_err("missing )");
				if (*g.c++ != '{') dspec_err("missing {");
				dspec_stmtList(c && output);
				if (*g.c++ != '}') dspec_err("missing }");
				if ((g.c[0] == '$') && (g.c[1] == 'e') &&
				    (g.c[2] == 'l') && (g.c[3] == 's') &&
				    (g.c[4] == 'e') && (g.c[5] == '{')) {
					g.c += 6;
					dspec_stmtList(!c && output);
					if (*g.c++ != '}') dspec_err("missing }");
				}
			} else if ((g.c[0] == 't') && (g.c[1] == 'c') &&
				   (g.c[2] == 'l') && (g.c[3] == '{')) {
				g.c += 4;
				c = g.tcl;
				g.tcl = 0;
				show_s(g.s, out, buf, "{", 1);
				g.tcl = c;
				++g.tcl;
				dspec_stmtList(output);
				--g.tcl;
				g.tcl = 0;
				show_s(g.s, out, buf, "}", 1);
				g.tcl = c;
				if (*g.c++ != '}') dspec_err("missing }");
			} else {
				show_s(g.s, out, buf, "$", 1);
			}
		} else if (g.c[0] == ':') {
			dspec_evalId(out, buf);
		} else if (dspec_getParenid(&id)) {
			dspec_evalParenid(out, buf, id);
		} else if (g.c[0] == '}') {
			if (depth == 1) {
				show_s(g.s, g.out, g.buf, "}", 1);
				++g.c;
			}
			else {
				--depth;
				return;
			}
		} else if (g.c[0] == '\\') {
			char c;
			switch (*++g.c) {
			    case 'n': c = '\n'; break;
			    case 't': c = 9; break;
			    default:  c = *g.c; break;
			}
			show_s(g.s, out, buf, &c, 1);
			++g.c;
		} else {
			show_s(g.s, out, buf, g.c, 1);
			++g.c;
		}
	}
	--depth;
}

private int
dspec_expr(void)
{
	dspec_op_t	op;
	int		ret;

	ret = dspec_expr2(&op);
	while (*g.c) {
		switch (op) {
		case T_AND:
			ret = dspec_expr2(&op) && ret;
			break;
		case T_OR:
			ret = dspec_expr2(&op) || ret;
			break;
		case T_RPAREN:
			return (ret);
		default:
			dspec_err("expected &&,||, or )");
		}
	}
	return (ret);
}

private int
dspec_expr2(dspec_op_t *next_tok)
{
	dspec_op_t	op;
	datum		id;
	int		ret;
	char		*lhs, *rhs;

	while (g.c[0] == ' ') ++g.c; /* skip whitespace */
	if ((g.c[0] == '(') && !dspec_getParenid(&id)) {
		/* Parenthesized sub-expression. */
		++g.c;	/* eat ( */
		ret = dspec_expr();
		++g.c;	/* eat ) */
		while (g.c[0] == ' ') ++g.c; /* skip whitespace */
		if (g.c[0] == ')') {
			*next_tok = T_RPAREN;
		} else if ((g.c[0] == '&') && (g.c[1] == '&')) {
			*next_tok = T_AND;
			g.c += 2;
		} else if ((g.c[0] == '|') && (g.c[1] == '|')) {
			*next_tok = T_OR;
			g.c += 2;
		} else {
			dspec_err("expected &&, ||, or )");
		}
		return (ret);
	}
	else {
		lhs = str_pullup(0, dspec_str(&op));
		switch (op) {
		    case T_RPAREN:
		    case T_AND:
		    case T_OR:
			ret = *lhs;
			free(lhs);
			*next_tok = op;
			return (ret);
		    case T_EOF:
			dspec_err("expected operator or )");
		    default:
			break;
		}
		rhs = str_pullup(0, dspec_str(next_tok));
		switch (op) {
		    case T_EQUALS:	ret =  streq(lhs, rhs); break;
		    case T_NOTEQ:	ret = !streq(lhs, rhs); break;
		    case T_EQ:		ret = atof(lhs) == atof(rhs); break;
		    case T_NE:		ret = atof(lhs) != atof(rhs); break;
		    case T_GT:		ret = atof(lhs) >  atof(rhs); break;
		    case T_GE:		ret = atof(lhs) >= atof(rhs); break;
		    case T_LT:		ret = atof(lhs) <  atof(rhs); break;
		    case T_LE:		ret = atof(lhs) <= atof(rhs); break;
		    case T_EQTWID:	ret =  match_one(lhs, rhs, 1); break;
		    default: assert(0); ret = 0; break;
		}
		free(lhs);
		free(rhs);
		return (ret);
	}
}

private char **
dspec_str(dspec_op_t *next_tok)
{
	char	**s = 0;
	datum	id;

	while (*g.c) {
		if (g.c[0] == ':') {
			dspec_evalId(0, &s);
			continue;
		} else if (dspec_getParenid(&id)) {
			dspec_evalParenid(0, &s, id);
			continue;
		} else if (g.c[0] == ')') {
			*next_tok = T_RPAREN;
			return (s);
		} else if ((g.c[0] == '=') && (g.c[1] == '~')) {
			*next_tok = T_EQTWID;
			g.c += 2;
			return (s);
		} else if (g.c[0] == '=') {
			*next_tok = T_EQUALS;
			++g.c;
			return (s);
		} else if ((g.c[0] == '!') && (g.c[1] == '=')) {
			*next_tok = T_NOTEQ;
			g.c += 2;
			return (s);
		} else if ((g.c[0] == ' ') && (g.c[1] == '-') &&
			   (g.c[4] == ' ')) {
			if ((g.c[2] == 'e') && (g.c[3] == 'q')) {
				*next_tok = T_EQ;
				g.c += 5;
				return (s);
			} else if ((g.c[2] == 'n') && (g.c[3] == 'e')) {
				*next_tok = T_NE;
				g.c += 5;
				return (s);
			} else if ((g.c[2] == 'g') && (g.c[3] == 't')) {
				*next_tok = T_GT;
				g.c += 5;
				return (s);
			} else if ((g.c[2] == 'g') && (g.c[3] == 'e')) {
				*next_tok = T_GE;
				g.c += 5;
				return (s);
			} else if ((g.c[2] == 'l') && (g.c[3] == 't')) {
				*next_tok = T_LT;
				g.c += 5;
				return (s);
			} else if ((g.c[2] == 'l') && (g.c[3] == 'e')) {
				*next_tok = T_LE;
				g.c += 5;
				return (s);
			}
		} else if ((g.c[0] == '&') && (g.c[1] == '&')) {
			*next_tok = T_AND;
			g.c += 2;
			return (s);
		} else if ((g.c[0] == '|') && (g.c[1] == '|')) {
			*next_tok = T_OR;
			g.c += 2;
			return (s);
		} else if (g.c[0] == ' ') {
			++g.c;
			continue;
		}
		s = data_append(s, g.c++, 1, 0);
	}
	*next_tok = T_EOF;
	return (s);
}

private int
dspec_getParenid(datum *id)
{
	/*
	 * Find out whether g.c points to a (:ID:) construct.  If so,
	 * return 1 and set *id.
	 */
	char *c;

	unless ((g.c[0] == '(') && (g.c[1] == ':')) return 0;
	id->dptr = c = g.c + 2;
	while (*c && ((c[0] != ':') || (c[1] != ')'))) {
	       if ((*c != '%') && (*c != '_') &&
		   (*c != '-') && (*c != '#') &&
		   !isalpha(*c)) {
		       return 0;
	       }
	       ++c;
	}
	if (*c) {
		id->dsize = c - id->dptr;
		return 1;
	} else {
		return 0;
	}
}

private void
dspec_evalParenid(FILE *out, char ***buf, datum id)
{
	/*
	 * Expand a (:ID:).  If the eachvals hash has a value for ID,
	 * use that.  Otherwise output the parentheses and try
	 * expanding ID as a regular keyword.  If it's not a keyword,
	 * treat it as a regular string.
	 */
	datum	v;
	v = mdbm_fetch(g.eachvals, id);
	if (v.dptr) {
		show_s(g.s, out, buf, v.dptr, v.dsize);
	} else {
		show_s(g.s, out, buf, "(", 1);
		if (kw2val(out, buf, id.dptr, id.dsize, g.s, g.d) < 0) {
			show_s(g.s, out, buf, ":", 1);
			show_s(g.s, out, buf, id.dptr, id.dsize);
			show_s(g.s, out, buf, ":", 1);
		}
		show_s(g.s, out, buf, ")", 1);
	}
	g.c += id.dsize + 4;  /* move past ending ':)' */
}

private void
dspec_evalId(FILE *out, char *** buf)
{
	/*
	 * Call with g.c pointing to a ':'.  If what comes after is
	 * ":ID:" and a keyword, expand it into out/buf.  If it's not
	 * a keyword or no ending colon is there, output just the ':'.
	 */
	char	*c, *id;

	id = c = g.c + 1;
	while (*c && (*c != ':')) ++c;

	if (*c) {
		if (kw2val(out, buf, id, c - id, g.s, g.d) >= 0) {
			g.c = c + 1;  /* move past ending ':' */
			return;
		}
	}
	show_s(g.s, out, buf, ":", 1);
	++g.c;
}

private void
dspec_err(char *msg)
{
	int	i, n;

	fprintf(stderr, "syntax error: %s\n", msg);
	fprintf(stderr, "%s\n", g.start);
	n = g.c - g.start - 1;
	for (i = 0; i < n; ++i) fputc(' ', stderr);
	fprintf(stderr, "^\n");
	exit(1);
}

/*
 * Given a keyword with a multi-line value, return each line successively.
 * A dspec_getnext_t * is passed in, and returned, to store the state
 * of where we are in the list of lines to return.
 * Call this function with state->i=1 to get the first line, then pass the
 * return value back in to get subsquent lines.	 state->buf must be a pointer
 * to an appropriately-sized buffer to receive the values of single-line
 * keywords.  NULL is returned when there are no more lines,
 * and state->ret=NULL is returned for a "line" that the caller should skip.
 */

private dspec_getnext_t *
dspec_getnext(datum kw, dspec_getnext_t *state)
{
	if (strneq(kw.dptr, "C", kw.dsize)) {
		if (g.d && g.d->comments &&
		    (state->i < (long)g.d->comments[0]) &&
		    g.d->comments[state->i]) {
			if (g.d->comments[state->i][0] == '\001') {
				state->ret = NULL;
			} else {
				state->ret = g.d->comments[state->i];
			}
			++state->i;
			return state;
		} else
			return NULL;
	}

	if (strneq(kw.dptr, "FD", kw.dsize)) {
		if (g.s && g.s->text && (state->i < (long)g.s->text[0]) &&
		    g.s->text[state->i]) {
			if (g.s->text[state->i][0] == '\001') {
				state->ret = NULL;
			} else {
				state->ret = g.s->text[state->i];
			}
			++state->i;
			return state;
		} else
			return NULL;
	}

	if (strneq(kw.dptr, "SYMBOL", kw.dsize) ||
	    strneq(kw.dptr, "TAG", kw.dsize)) {
		if (state->i++ == 1) {
			unless (g.d && (g.d->flags & D_SYMBOLS)) { return NULL; }
			while (g.d->type == 'R') g.d = g.d->parent;
			state->sym = g.s->symbols;
		}
		if (state->sym) {
			unless (state->sym->d == g.d) {
				state->ret = NULL;
				state->sym = state->sym->next;
				return state;
			}
			state->ret = state->sym->symname;
			state->sym = state->sym->next;
			return state;
		}
		else {
			return NULL;
		}
	}

	/* Handle all single-line keywords. */
	if (state->i == 1) {
		char	**ret = 0;
		/* First time in, get the keyword value. */
		kw2val(0, &ret, kw.dptr, kw.dsize, g.s, g.d);
		state->ret = str_pullup(0, ret);
		state->i = 2;
		return state;
	}
	else {
		/* Second time in, bail out. */
		return NULL;
	}
}


void
dspec_printeach(sccs *s, FILE *out, char ***vbuf)
{
	show_s(s, out, vbuf, g.each.dptr, g.each.dsize);
}

void
dspec_printline(sccs *s, FILE *out, char ***vbuf)
{
	show_d(s, out, vbuf, "%d", g.line);
}

private void
tclouts(char *s, int len, FILE *f)
{
	unless (s && s[0] && len) return;
	for (; len; --len, ++s) {
		switch (*s) {
		    case '\f':	fputs("\\f", f); continue;
		    case '\n':	fputs("\\n", f); continue;
		    case '\r':  fputs("\\r", f); continue;
		    case '\t':  fputs("\\t", f); continue;
		    case '\v':  fputs("\\v", f); continue;

		    case ']':
		    case '[':
		    case '$':
		    case ';':
		    case '\\':
		    case '"':
		    case ' ':
		    case '{':
		    case '}':	fputc('\\', f); /* Fallthrough */
		    default:	fputc(*s, f);
		}
	}
}

void
show_d(sccs *s, FILE *out, char ***vbuf, char *format, int num)
{
	if (out) {
		fprintf(out, format, num);
		s->prs_output = 1;
	}
	if (vbuf) *vbuf = str_append(*vbuf, aprintf(format, num), 1);
}

void
show_s(sccs *s, FILE *out, char ***vbuf, char *data, int len)
{
	if (len == -1) len = strlen(data); /* special interface */
	if (out) {
		if (g.tcl) {
			tclouts(data, len, out);
		} else {
			fwrite(data, 1, len, out);
		}
		s->prs_output = 1;
	}
	if (vbuf) *vbuf = data_append(*vbuf, data, len, 0);
}

