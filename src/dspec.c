/*
 * Copyright 2007-2008,2011-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sccs.h"
#define	octal(c)	((c >= '0') && (c <= '7'))

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
} op;

typedef	struct
{
	int	i;		/* index into a lines array typically */
	char	*freeme;	/* last value if it needs to be freed */
} nextln;

/* Globals for dspec code below. */
private struct {
	char	*p;		/* current location in dspec */
	char	*start;		/* start of dspec buffer */
	datum	eachkey;	/* key of current $each */
	char	*eachval;	/* val of eachkey in current iteration */
	FILE	*out;		/* FILE* to receive output */
	int	line;		/* current line in $each iteration */
	int	tcl;		/* whether we are inside a $tcl construct */
	int	json;		/* whether we are inside a $json construct */
	sccs	*s;
	ser_t	d;
	FILE	*flhs, *frhs;	/* tmp fmem handles to hold expressions */
} _g[2];
private	int	gindex;
#define	g	(_g[gindex])

/* Flags for lookFor(). */
enum flags {
	NEED_SPACES	= 0,
	SPACES_OPTIONAL	= 1,
};

/* Globals for $0-$9 variables (using fmem). */
private FILE	*fvars[10];

private void	dollar(FILE *out);
private void	err(char *msg);
private char	escape(void);
private void	evalId(FILE *out);
private void	evalParenid(FILE *out, datum id);
private int	expr(void);
private int	expr2(op *next_tok);
private char	*getnext(datum kw, nextln *state);
private int	getParenid(datum *id);
private int	lookFor(char *s, int len, enum flags flags);
private int	match(char *s, char *pattern);
private void	stmtList(FILE *out);
private char	*string(FILE *out, op *next_tok);

void
dspec_eval(FILE * out, sccs *s, ser_t d, char *dspec)
{
	gindex = gindex ? 0 : 1;	// flip flop
	g.out = out;
	g.s = s;
	g.d = d;
	g.p = dspec;
	g.start = dspec;
	bzero(&g.eachkey, sizeof(g.eachkey));
	g.eachval = 0;
	g.line = 0;
	g.tcl = 0;
	g.json = 0;
	/* we preserve any existing g.f[rl]hs */

	stmtList(g.out);
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
 *	       -> ${<num>=<stmt_list>}
 *	       -> <atom>
 * <expr>      -> <expr2> {{ <logop> <expr2> }}
 * <expr2>     -> <str> <relop> <str>
 *	       -> <str>
 *	       -> (<expr>)
 *             -> !<expr2>
 * <str>       -> {{ <atom> }}
 * <atom>      -> char
 *	       -> escaped_char
 *	       -> :ID:
 *	       -> (:ID:)
 *	       -> $<num>
 * <logop>     -> " && " | " || "
 * <relop>     -> "=" | "!=" | "=~"
 *	       -> " -eq " | " -ne " | " -gt " | " -ge " | " -lt " | " -le "
 *
 * This grammar is ambiguous due to (:ID:) loooking like a
 * parenthesized sub-expression.  The code tries to parse (:ID:) first
 * as an $each variable, then as a regular :ID:, then as regular text.
 *
 * Note that this is broken: $if((:MERGE:)){:REV:}
 *
 * The following procedures can be thought of as implementing an
 * attribute grammar where the output parameters are synthesized
 * attributes which hold the expression values and the next token
 * of lookahead in some cases.	It has been written for speed.
 *
 * NOTE: out==0 means evaluate but throw away.
 *
 * Written by Rob Netzer <rob@bolabs.com> with some hacking
 * by wscott & lm.
 */

private void
stmtList(FILE *out)
{
	char	c;
	datum	id;
	static int	depth = 0;

	++depth;
	while (*g.p) {
		switch (*g.p) {
		    case '$':
			dollar(out);
			break;
		    case ':':
			evalId(out);
			break;
		    case '}':
			if (depth == 1) {
				show_s(g.s, out, "}", 1);
				++g.p;
			} else {
				--depth;
				return;
			}
			break;
		    case '\\':
			c = escape();
			show_s(g.s, out, &c, 1);
			break;
		    default:
			if (getParenid(&id)) {
				evalParenid(out, id);
			} else {
				show_s(g.s, out, g.p, 1);
				++g.p;
			}
			break;
		}
	}
	--depth;
}

private char
escape(void)
{
	char	c;
	int	i;

	if (octal(g.p[1]) && octal(g.p[2]) && octal(g.p[3])) {
		sscanf(g.p, "\\%03o", &i);
		c = i;
		g.p += 4;
	} else {
		switch (g.p[1]) {
		    case 'b': c = '\b'; break;
		    case 'f': c = '\f'; break;
		    case 'n': c = '\n'; break;
		    case 'r': c = '\r'; break;
		    case 't': c = '\t'; break;
		    default:  c = g.p[1]; break;
		}
		g.p += 2;
	}
	return (c);
}

/*
 * See if the string in s appears next, ignoring any leading and
 * trailing whitespace.  If flags == SPACES_REQUIRED, at least one
 * space before and after is required.  If there's a match, consume it
 * and return 1.
 */
private int
lookFor(char *s, int len, enum flags flags)
{
	int	sp = 0;
	char	*p = g.p;

	while (isspace(*p)) {
		++p;
		sp = 0x01;
	}
	unless (strneq(p, s, len)) return (0);
	p += len;
	while (isspace(*p)) {
		++p;
		sp |= 0x02;
	}
	if ((flags == NEED_SPACES) && (sp != 0x03)) return (0);
	g.p = p;
	return (1);
}

private void
dollar(FILE *out)
{
	int	num, rc;
	size_t	len;
	char	*p;
	static int	in_each = 0;

	if (strneq("$if(", g.p, 4)) {
		g.p += 4;
		rc = expr();
		if (*g.p++ != ')') err("missing )");
		if (*g.p++ != '{') err("missing {");
		stmtList(rc ? out : 0);
		if (*g.p++ != '}') err("missing }");
		if (strneq("$else{", g.p, 6)) {
			g.p += 6;
			stmtList(rc ? 0 : out);
			if (*g.p++ != '}') err("missing }");
		}
	} else if (strneq("$each(", g.p, 6) ||
	    strneq("$first(", g.p, 7)) {
		nextln	state;
		char	*bufptr;
		int	savejoin;
		int	first = strneq("$first(", g.p, 7);

		if (in_each++) {
			first
			    ? err("nested first illegal")
			    : err("nested each illegal");
		}

		g.p += first ? 7 : 6;
		if (*g.p++ != ':') err("missing id");

		/* Extract the id in $each(:id:) */
		g.eachkey.dptr = g.p;
		while (*g.p && (*g.p != ':')) ++g.p;
		unless (*g.p) err("premature eof");
		g.eachkey.dsize = g.p - g.eachkey.dptr;
		++g.p;

		if (*g.p++ != ')') err("missing )");
		if (*g.p++ != '{') err("missing {");

		/*
		 * Re-evaluate the $each body for each
		 * line of the $each variable.
		 */
		bzero(&state, sizeof(state));
		bufptr	  = g.p;
		g.line	  = 1;
		savejoin  = g.s->prs_join;
		g.s->prs_join = 0;
		while (g.eachval = getnext(g.eachkey, &state)) {
			g.p = bufptr;
			stmtList(out);
			++g.line;
			if (first) break;
		}
		g.eachkey.dptr = 0;
		g.eachkey.dsize = 0;
		g.s->prs_join = savejoin;
		--in_each;
		/* Eat the body if we never parsed it above. */
		if (g.line == 1) stmtList(0);
		if (*g.p++ != '}') err("missing }");
	} else if (strneq("$unless(", g.p, 8)) {
		g.p += 8;
		rc = !expr();
		if (*g.p++ != ')') err("missing )");
		if (*g.p++ != '{') err("missing {");
		stmtList(rc ? out : 0);
		if (*g.p++ != '}') err("missing }");
		if (strneq("$else{", g.p, 6)) {
			g.p += 6;
			stmtList(rc ? 0 : out);
			if (*g.p++ != '}') err("missing }");
		}
	} else if (strneq("$tcl{", g.p, 5)) {
		g.p += 5;
		rc = g.tcl;
		g.tcl = 0;
		show_s(g.s, out, "{", 1);
		g.tcl = rc + 1;
		stmtList(out);
		g.tcl = 0;
		show_s(g.s, out, "}", 1);
		g.tcl = rc;
		if (*g.p++ != '}') err("missing }");
	} else if (strneq("$json{", g.p, 6)) {
		g.p += 6;
		g.json = 1;
		stmtList(out);
		g.json = 0;
		if (*g.p++ != '}') err("missing }");
	} else if ((g.p[0] == '$') && (g.p[1] == '{')) {
		g.p += 2;
		unless (isdigit(*g.p)) err("expected digit");
		num = *g.p++ - '0';
		unless (*g.p++ == '=') err("missing =");
		/*
		 * Recurse to parse the right-hand side of the ${n=<stmt_list>}
		 * and put the output into the $n fmem.
		 */
		if (out) {
			FILE	*memfile = fmem();

			stmtList(memfile);
			if (fvars[num]) fclose(fvars[num]);
			fvars[num] = memfile;
		} else {
			stmtList(0);	/* ignore */
		}
		unless (*g.p++ == '}') err("missing }");
	} else if ((g.p[0] == '$') && isdigit(g.p[1])) {
		num = g.p[1] - '0';
		g.p += 2;
		if (out && fvars[num]) {
			p = fmem_peek(fvars[num], &len);
			show_s(g.s, out, p, len);
		}
	} else {
		show_s(g.s, out, "$", 1);
		g.p++;
	}
}

private int
expr(void)
{
	op	op;
	int	ret;

	ret = expr2(&op);
	while (*g.p) {
		switch (op) {
		    case T_AND:
			/*
			 * You might be tempted to write this as
			 *	(ret && expr2(&op))
			 * since that reads more like what the user
			 * wrote.  But C will short circuit that and
			 * you won't run expr2() in some cases.  Oops.
			 *
			 * You might also think you can do a return here
			 * and you can't, this handles a || b && c.
			 */
			ret = expr2(&op) && ret;
			break;
		    case T_OR:
			ret = expr2(&op) || ret;
			break;
		    case T_RPAREN:
			return (ret);
		    default:
			err("expected &&, ||, or )");
		}
	}
	return (ret);
}

private int
expr2(op *next_tok)
{
	op	op;
	datum	id;
	int	ret;
	char	*lhs, *rhs;

	if (lookFor("!", 1, SPACES_OPTIONAL)) {
		return (!expr2(next_tok));
	}
	if ((g.p[0] == '(') && !getParenid(&id)) {
		/* Parenthesized sub-expression. */
		++g.p;	/* eat ( */
		ret = expr();
		++g.p;	/* eat ) */
		if (g.p[0] == ')') {
			*next_tok = T_RPAREN;
		} else if (lookFor("&&", 2, SPACES_OPTIONAL)) {
			*next_tok = T_AND;
		} else if (lookFor("||", 2, SPACES_OPTIONAL)) {
			*next_tok = T_OR;
		} else {
			err("expected &&, ||, or )");
		}
		return (ret);
	} else {
		unless (g.flhs) g.flhs = fmem();
		lhs = string(g.flhs, &op);
		switch (op) {
		    case T_RPAREN:
		    case T_AND:
		    case T_OR:
			ret = *lhs;
			*next_tok = op;
			return (ret);
		    case T_EOF:
			err("expected operator or )");
		    default:
			break;
		}
		unless (g.frhs) g.frhs = fmem();
		rhs = string(g.frhs, next_tok);
		switch (op) {
		    case T_EQUALS:	ret =  streq(lhs, rhs); break;
		    case T_NOTEQ:	ret = !streq(lhs, rhs); break;
		    case T_EQ:		ret = atof(lhs) == atof(rhs); break;
		    case T_NE:		ret = atof(lhs) != atof(rhs); break;
		    case T_GT:		ret = atof(lhs) >  atof(rhs); break;
		    case T_GE:		ret = atof(lhs) >= atof(rhs); break;
		    case T_LT:		ret = atof(lhs) <  atof(rhs); break;
		    case T_LE:		ret = atof(lhs) <= atof(rhs); break;
		    case T_EQTWID:	ret = match(lhs, rhs); break;
		    default: assert(0); ret = 0; break;
		}
		return (ret);
	}
}

/*
 * evaluate a string a part of a conditional expression.
 *
 * f is a fmem FILE* that will hold the temporary result.  We are
 * reusing this memory so we start with ftrunc() to throw away any
 * existing data.
 *
 * A pointer into f's memory is returned.  This doesn't get free'd
 * by the caller and remains vaild until the next time this function
 * is called.
 */
private char *
string(FILE *f, op *next_tok)
{
	char	c;
	int	num;
	size_t	len;
	char	*p;
	datum	id;

	ftrunc(f, 0);
	while (*g.p) {
		if (g.p[0] == ':') {
			evalId(f);
			continue;
		} else if (getParenid(&id)) {
			evalParenid(f, id);
			continue;
		} else if (g.p[0] == ')') {
			*next_tok = T_RPAREN;
			goto out;
		} else if (lookFor("=~", 2, SPACES_OPTIONAL)) {
			*next_tok = T_EQTWID;
			goto out;
		} else if (lookFor("=", 1, SPACES_OPTIONAL)) {
			*next_tok = T_EQUALS;
			goto out;
		} else if (lookFor("!=", 2, SPACES_OPTIONAL)) {
			*next_tok = T_NOTEQ;
			goto out;
		} else if (lookFor("&&", 2, SPACES_OPTIONAL)) {
			*next_tok = T_AND;
			goto out;
		} else if (lookFor("||", 2, SPACES_OPTIONAL)) {
			*next_tok = T_OR;
			goto out;
		} else if (lookFor("-eq", 3, NEED_SPACES)) {
			*next_tok = T_EQ;
			goto out;
		} else if (lookFor("-ne", 3, NEED_SPACES)) {
			*next_tok = T_NE;
			goto out;
		} else if (lookFor("-gt", 3, NEED_SPACES)) {
			*next_tok = T_GT;
			goto out;
		} else if (lookFor("-ge", 3, NEED_SPACES)) {
			*next_tok = T_GE;
			goto out;
		} else if (lookFor("-lt", 3, NEED_SPACES)) {
			*next_tok = T_LT;
			goto out;
		} else if (lookFor("-le", 3, NEED_SPACES)) {
			*next_tok = T_LE;
			goto out;
		} else if ((g.p[0] == '$') && isdigit(g.p[1])) {
			num = g.p[1] - '0';
			g.p += 2;
			if (fvars[num]) {
				p = fmem_peek(fvars[num], &len);
				show_s(g.s, f, p, len);
			}
			continue;
		} else if (g.p[0] == '\\') {
			c = escape();
			show_s(g.s, f, &c, 1);
			continue;
		}
		show_s(g.s, f, g.p, 1);
		++g.p;
	}
	*next_tok = T_EOF;
out:	return (fmem_peek(f, 0));
}

private int
getParenid(datum *id)
{
	/*
	 * Find out whether g.p points to a (:ID:) construct.  If so,
	 * return 1 and set *id.
	 */
	char *c;

	unless ((g.p[0] == '(') && (g.p[1] == ':')) return (0);
	id->dptr = c = g.p + 2;
	while (*c && ((c[0] != ':') || (c[1] != ')'))) {
	       if ((*c != '%') && (*c != '_') &&
		   (*c != '-') && (*c != '#') &&
		   !isalpha(*c)) {
		       return (0);
	       }
	       ++c;
	}
	unless (*c) return (0);
	id->dsize = c - id->dptr;
	return (1);
}

private void
evalParenid(FILE *out, datum id)
{
	/*
	 * Expand a (:ID:).  If the eachkey has a value for if id,
	 * use that.  Otherwise output the parentheses and try
	 * expanding ID as a regular keyword.  If it's not a keyword,
	 * treat it as a regular string.
	 */
	if ((id.dsize == g.eachkey.dsize) &&
	    strneq(g.eachkey.dptr, id.dptr, id.dsize)) {
		show_s(g.s, out, g.eachval, strlen(g.eachval));
	} else {
		show_s(g.s, out, "(", 1);
		if (kw2val(out, id.dptr, id.dsize, g.s, g.d) < 0) {
			show_s(g.s, out, ":", 1);
			show_s(g.s, out, id.dptr, id.dsize);
			show_s(g.s, out, ":", 1);
		}
		show_s(g.s, out, ")", 1);
	}
	g.p += id.dsize + 4;  /* move past ending ':)' */
}

private void
evalId(FILE *out)
{
	/*
	 * Call with g.p pointing to a ':'.  If what comes after is
	 * ":ID:" and a keyword, expand it into out/buf.  If it's not
	 * a keyword or no ending colon is there, output just the ':'.
	 */
	char	*c, *id;

	id = c = g.p + 1;
	while (*c && (*c != ':')) ++c;

	if (*c) {
		if (kw2val(out, id, c - id, g.s, g.d) >= 0) {
			g.p = c + 1;  /* move past ending ':' */
			return;
		}
	}
	show_s(g.s, out, ":", 1);
	++g.p;
}

private int
match(char *s, char *pattern)
{
	int	ret;

	/*
	 * If pattern is like /xyzzy/ do a regexp match otherwise do a
	 * glob match.
	 */
	if (pattern[0] == '/') {
		search	search = search_parse(pattern+1);
		ret = search_either(s, search);
		search_free(search);
	} else {
		ret = match_one(s, pattern, 1);
	}
	return (ret);
}

private void
err(char *msg)
{
	int	i, n;

	fprintf(stderr, "syntax error: %s\n", msg);
	fprintf(stderr, "%s\n", g.start);
	n = g.p - g.start - 1;
	for (i = 0; i < n; ++i) fputc(' ', stderr);
	fprintf(stderr, "^\n");
	exit(1);
}

/*
 * Given a keyword with a multi-line value, return each line successively.
 * A nextln * is passed in to store the state
 * of where we are in the list of lines to return.
 * Call this function with all of state cleared the first time.
 * In particular, state->i=0 to get the first line, then pass the
 * return value back in to get subsquent lines.
 * Return a pointer to the data or 0 meaning EOF.
 * If data is malloced, save the pointer in freeme, which will get freed
 * on next call.  Nothing outside this routine knows internals of state.
 */
private char *
getnext(datum kw, nextln *state)
{
	if (state->freeme) {
		free(state->freeme);
		state->freeme = 0;
	}
again:
	if (strneq(kw.dptr, "C", kw.dsize)) {
		char	*p, *t;
		int	len;

		unless (g.d && HAS_COMMENTS(g.s, g.d)) return (0);

		t = COMMENTS(g.s, g.d);
		t += state->i;
		if (*t && (p = strchr(t, '\n'))) {
			len = p-t;
			state->freeme = strndup(t, len);
			state->i += len+1;
		}
		return(state->freeme);
	}
	++state->i;	/* first call has it set to 0, so now 1 */

	/* XXX FD depracated */
	if (strneq(kw.dptr, "FD", kw.dsize)) {
		unless (g.s && g.s->text &&
		    (state->i <= nLines(g.s->text))) {
			return (0);
		}
		/* XXX Is this needed for title, or only comments? */
		if (g.s->text[state->i][0] == '\001') goto again;
		return (g.s->text[state->i]);
	}

	if (((kw.dsize == 6) && strneq(kw.dptr, "SYMBOL", kw.dsize)) ||
	    ((kw.dsize == 3) && strneq(kw.dptr, "TAG", kw.dsize))) {
		symbol	*sym;
		int	i;

		i = 0;
		sym = 0;
		while (sym = sccs_walkTags(sym, g.s, g.d, 0, g.s->prs_all)) {
			if (++i == state->i) break;
		}
		unless (sym) return (0);
		return (SYMNAME(g.s, sym));
	}

	/* Handle all single-line keywords. */
	if (state->i == 1) {
		FILE	*f = fmem();

		/* First time in, get the keyword value. */
		kw2val(f, kw.dptr, kw.dsize, g.s, g.d);
		state->freeme = fmem_close(f, 0);
		return (state->freeme);
	} else {
		/* Second time in, bail out. */
		return (0);
	}
	/* not reached */
}


void
dspec_printeach(sccs *s, FILE *out)
{
	show_s(s, out, g.eachval, strlen(g.eachval));
}

void
dspec_printline(sccs *s, FILE *out)
{
	show_d(s, out, "%d", g.line);
}

private void
tclQuote(char *s, int len, FILE *f)
{
	unless (s && s[0] && len) return;
	for (; len; --len, ++s) {
		switch (*s) {
		    case '\b':	fputs("\\b", f); continue;
		    case '\f':	fputs("\\f", f); continue;
		    case '\n':	fputs("\\n", f); continue;
		    case '\r':  fputs("\\r", f); continue;
		    case '\t':  fputs("\\t", f); continue;

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

/*
 * From http://www.json.org/ (string section)
 * Except the \u0000 section.
 */
private void
jsonQuote(char *s, int len, FILE *f)
{
	unless (s && s[0] && len) return;
	for (; len; --len, ++s) {
		switch (*s) {
		    case '\b': fputs("\\b", f); continue;
		    case '\f': fputs("\\f", f); continue;
		    case '\n': fputs("\\n", f); continue;
		    case '\r': fputs("\\r", f); continue;
		    case '\t': fputs("\\t", f); continue;

		    case '"':
		    case '\\':fputc('\\', f); /* Fallthrough */
		    default: fputc(*s, f);
		}
	}
}

void
show_d(sccs *s, FILE *out, char *format, int num)
{
	fprintf(out, format, num);
	/*
	 * The out==g.out test means that we are printing directly to
	 * the prs output stream.  When it is not true we are probably
	 * evaluating dspecs for a conditional expression.
	 */
	if (out == g.out) s->prs_output = 1;
}

void
show_s(sccs *s, FILE *out, char *data, int len)
{
	if (out) {
		if (len == -1) len = strlen(data); /* special interface */
		unless (len) return;
		if (g.tcl) {
			tclQuote(data, len, out);
		} else if (g.json) {
			jsonQuote(data, len, out);
		} else {
			fwrite(data, 1, len, out);
		}
		/* see comment in show_d() about out==g.out */
		if (out == g.out) s->prs_output = 1;
	}
}

private char *
str_closebracket(char *p)
{
	int	cnt = 0;

	assert(p && *p == '{');
	for (; *p; p++) {
		switch (*p) {
		    case '{': ++cnt; break;
		    case '}': --cnt; break;
		    case '\\': ++p; break;
		    default: break;
		}
		if (cnt == 0) break;
	}
	if (cnt) return (0);
	return (p);
}

/*
 * handle the "extended" dspec syntax.
 * strip out all unescaped whitespace and comments.
 */
void
dspec_collapse(char **dspec, char **begin, char **end)
{
	FILE	*f;
	char	*p, *t;
	char	*who, ***where;

	/*
	 * The first line has to be
	 * #[ ]+dv2
	 * or
	 * #[ ]+dv2\n<the-rest-of-the-dspec>
	 * or
	 * #[ ]+dspec-v2
	 * or we don't expand, where the "\n" above means the two-character
	 * string "\" followed by "n" and is how we get a one-line dv2 dspec.
	 * The first is for tired fingers, but we should use the second in
	 * the files we ship, self documenting.
	 */
	p = *dspec;
	unless (*p == '#') return;
	for (p++; *p && isspace(*p); p++);
	if (strneq(p, "dv2\\n", 5)) {
		p += 5;
	} else if (strneq(p, "dv2\n", 4)) {
		p += 4;
	} else if (strneq(p, "dspec-v2\n", 9)) {
		p += 9;
	} else {
		return;
	}
	f = fmem();
	for (; *p; p++) {
		switch (*p) {
		    case '#':	/* comment to end of line */
			for (++p; *p; ++p) {
				if (*p == '\n') break;
			}
			unless (*p) --p; /* outer for loop needs to see end */
			break;
		    case '\\':
			fputc(*p, f);
			if (p[1]) fputc(*++p, f);
			break;
		    case ' ':
			if ((p[1] == '-') &&
			    (strneq(p, " -eq ", 5) ||
			     strneq(p, " -ne ", 5) ||
			     strneq(p, " -gt ", 5) ||
			     strneq(p, " -ge ", 5) ||
			     strneq(p, " -lt ", 5) ||
			     strneq(p, " -le ", 5))) {
				/* don't trim number compares */
				fwrite(p, 1, 5, f);
				p += 4;
				break;
			}
			/* fallthough */
		    case '\t': case '\n':	/* skip whitespace */
			break;
		    case '"':
			for (++p; *p && (*p != '"'); p++) {
				switch (*p) {
				    case '\n':
					fprintf(stderr, "error in dspec, "
					    "no multi-line strings\n");
					exit(1);
					break;
				    case '\\':
					fputc(*p, f);
					if (p[1]) fputc(*++p, f);
					break;
				    case '{': case '}':
				    case '$':
					unless (isdigit(p[1])) fputc('\\', f);
					fputc(*p, f);
					break;
				    default:
					fputc(*p, f);
					break;
				}
			}
			break;
		    case '$':
			if (strneq(p+1, "begin", 5)) {
				t = p+6;
				where = &begin;
				who = "$begin";
			} else if (strneq(p+1, "end", 3)) {
				t = p+4;
				where = &end;
				who = "$end";
			} else {
				fputc(*p, f);
				break;
			}
			while (*t == ' ') t++;
			if (*t != '{') { /* no match */
				fputc(*p, f);
				break;
			}
			unless (*where) {
				fprintf(stderr, "%s not supported\n", who);
				exit(1);
			}
			if (**where) {
				fprintf(stderr, "only one %s\n", who);
				exit(1);
			}
			p = t;
			unless (t = str_closebracket(p)) {
				fprintf(stderr, "unmatched } in %s\n", who);
				exit(1);
			}
			*t = 0;
			**where = aprintf("#dv2\n%s", p+1);
			dspec_collapse(*where, 0, 0);
			p = t;
			break;
		    default:
			fputc(*p, f);
			break;
		}
	}
	p = fmem_close(f, 0);
	free(*dspec);
	*dspec = p;
}
