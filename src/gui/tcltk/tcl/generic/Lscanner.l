%option	noyywrap
%option stack
%x re_modifier
%s re_arg
%x glob_re
%x subst_re
%x comment
%x str_double
%x str_single
%x interpol
ID		([a-zA-Z_]|::)([0-9a-zA-Z_]|::)*
%{
/*
 * Copyright (c) 2006-2007 BitMover, Inc.
 */
#include <string.h>
#define	_PWD_H			// Some solaris9 conflict, we don't need pwd.h
#include "Lcompile.h"
#include "Lgrammar.h"

/* String collection buffer */
Tcl_Obj *str;
int str_beg;

#define STRBUF_START(beg)			\
	do {					\
		str = Tcl_NewObj();		\
		Tcl_IncrRefCount(str);		\
		str_beg = (beg);		\
	} while (0)


#define STRBUF_STRING()		Tcl_GetString(str)

#define STRBUF_ADD(s, len)	Tcl_AppendToObj(str, s, len)

#define STRBUF_STOP(e)				\
	do {					\
		Tcl_DecrRefCount(str);		\
		str = NULL;			\
		L_lloc.beg = str_beg;		\
		L_lloc.end = (e);		\
	} while (0)

extern int L_line_number;
extern char *L_source_file;
extern int L_token_offset;
extern int L_prev_token_len;

/* Omit needless routine to silence compiler warning. */
#define YY_NO_TOP_STATE

/*
 * Keep track of the current offset in the input string.
 * YY_USER_ACTION is run before each action.  Note that some actions
 * further modify L_lloc.
 */
#define YY_USER_ACTION					\
	do {						\
		L_token_offset += L_prev_token_len;	\
		L_prev_token_len = yyleng;		\
		L_lloc.beg = L_token_offset;		\
		L_lloc.end = L_token_offset + yyleng;	\
	} while (0);

/* The character that began the current regexp, so in m|foo| this is
 * '|'.
 *
 * XXX: since regexps are interpolated, they can nest, which
 * will fail unless they all use the same quote char. */
char re_quote_char;

/* In case of errors, the start condition stack will be left in an
   unknown state.  This function gives us a way to reset it. */
void L_start_lexer() { BEGIN(0); }

/**
 * Count the newlines in a string and add the number to L_line_number.
 */
private void
tally_newlines(
	char *s,	/* the string with the newlines */
	int len)	/* the length of the string */
{
	char *p, *end;

	for (p = s, end = p + len; p < end; p++) {
		if (*p == '\n') {
			L_line_number++;
		} else if ((*p == '\r') && (p+1 < end) && (*(p+1) != '\n')) {
			/* mac line endings */
			L_line_number++;
		}
	}
}

%}
%%
<INITIAL,interpol,re_arg>{
	"("		return T_LPAREN;
	")"		return T_RPAREN;
	"{"		return T_LBRACE;
	"["		return T_LBRACKET;
	"]"		return T_RBRACKET;
	","		return T_COMMA;
	"!"		return T_BANG;
	"+"		return T_PLUS;
	"-"		return T_MINUS;
	"*"		return T_STAR;
	"/"		return T_SLASH;
	"%"		return T_PERC;
	"+="		return T_EQPLUS;
	"-="		return T_EQMINUS;
	"*="		return T_EQSTAR;
	"/="		return T_EQSLASH;
	"%="		return T_EQPERC;
	"&="		return T_EQBITAND;
	"|="		return T_EQBITOR;
	"^="		return T_EQBITXOR;
	"<<="		return T_EQLSHIFT;
	">>="		return T_EQRSHIFT;
	"++"		return T_PLUSPLUS;
	"--"		return T_MINUSMINUS;
	"&&"		return T_ANDAND;
	"||"		return T_OROR;
	"&"		return T_BITAND;
	"|"		return T_BITOR;
	"^"		return T_BITXOR;
	"~"		return T_BITNOT;
	"<<"		return T_LSHIFT;
	">>"		return T_RSHIFT;
	"="		return T_EQUALS;
	";"		return T_SEMI;
	"."		return T_DOT;
	"..."		return T_ELLIPSIS;
	"class"		return T_CLASS;
	"extern"	return T_EXTERN;
	"return"	return T_RETURN;
	"include"	return T_INCLUDE;
	"void"		return T_VOID;
	"string"	return T_STRING;
	"int"		return T_INT;
	"float"		return T_FLOAT;
	"poly"		return T_POLY;
	"var"		return T_VAR;
	"widget"	return T_WIDGET;
	"split"		{
		MK_STRING_NODE(L_lval, yytext, L_lloc.beg, L_lloc.end);
		return T_SPLIT;
	}

	"("[ \t]*"string"[ \t]*")"	return T_STRING_CAST;
	"("[ \t]*"tcl"[ \t]*")"		return T_TCL_CAST;
	"("[ \t]*"float"[ \t]*")"	return T_FLOAT_CAST;
	"("[ \t]*"int"[ \t]*")"		return T_INT_CAST;
	"("[ \t]*"hash"[ \t]*")"	return T_HASH_CAST;
	"("[ \t]*"widget"[ \t]*")"	return T_WIDGET_CAST;

	"if"		return T_IF;
	"else"		return T_ELSE;
	"unless"	return T_UNLESS;
	"while"		return T_WHILE;
	"do"		return T_DO;
	"for"		return T_FOR;
	"struct"	return T_STRUCT;
	"typedef"	return T_TYPEDEF;
	"defined"	return T_DEFINED;
	"foreach"	return T_FOREACH;
	"break"		return T_BREAK;
	"continue"	return T_CONTINUE;
	"push"		return T_PUSH;
	"in"		return T_IN;
	"=>"		return T_ARROW;
	"eq"		return T_EQ;
	"ne"		return T_NE;
	"lt"		return T_LT;
	"le"		return T_LE;
	"gt"		return T_GT;
	"ge"		return T_GE;
	"=="		return T_EQUALEQUAL;
	"!="		return T_NOTEQUAL;
	">"		return T_GREATER;
	">="		return T_GREATEREQ;
	"<"		return T_LESSTHAN;
	"<="		return T_LESSTHANEQ;
	{ID}[ \t\n\r]*:	{
				int i, j;
				tally_newlines(yytext, yyleng);
				MK_STRING_NODE(L_lval, yytext, L_lloc.beg,
					       L_lloc.end);
				((L_expr *)L_lval)->u.string[0] = '-';
				/* strip the whitespace */
				for (i=0, j=1; i<yyleng-1; i++) {
					switch (yytext[i]) {
					case ' ':
					case '\t':
					case '\n':
					case '\r':
						break;
					default:
						((L_expr *)L_lval)->u.string[j++] = yytext[i];
					}
				}
				((L_expr *)L_lval)->u.string[j] = '\0';
				return T_KEYWORD;
			}
	{ID}		{
				MK_STRING_NODE(L_lval, yytext, L_lloc.beg,
					       L_lloc.end);

				if (L_lookup_typedef(L_lval, FALSE)) {
					return T_TYPE;
				} else {
					return T_ID;
				}
			}
	([A-Z]|::)([0-9a-zA-Z]|::)*_\*	{
				MK_STRING_NODE(L_lval, yytext, L_lloc.beg,
					       L_lloc.end);
				return T_PATTERN;
			}
	$[0-9]+		{
				/* Regular expression submatches */
				MK_STRING_NODE(L_lval, yytext, L_lloc.beg,
					       L_lloc.end);
				return T_ID;
			}
	[0-9]+		{
				// XXX - we don't handle constants > 32 bits
				MK_INT_NODE(L_lval, atoi(yytext), L_lloc.beg,
					    L_lloc.end);
				return T_INT_LITERAL;
			}
	0o[0-7]+	{
				MK_INT_NODE(L_lval, strtoul(yytext+2, 0, 8),
					    L_lloc.beg, L_lloc.end);
				return T_INT_LITERAL;
			}
	0x[0-9a-fA-F]+	{
				// XXX - we don't handle constants > 32 bits
				MK_INT_NODE(L_lval, strtoul(yytext+2, 0, 16),
					    L_lloc.beg, L_lloc.end);
				return T_INT_LITERAL;
			}
	[0-9]*\.[0-9]+	{
				MK_FLOAT_NODE(L_lval, atof(yytext),
					      L_lloc.beg, L_lloc.end);
				return T_FLOAT_LITERAL;
			}
	"//".*("\r"|"\n"|"\r\n") tally_newlines(yytext, yyleng);
	[ \t\n\r]+	tally_newlines(yytext, yyleng);
	\"		yy_push_state(str_double); STRBUF_START(L_token_offset);
	\'		yy_push_state(str_single); STRBUF_START(L_token_offset);
	"/*"		yy_push_state(comment);
	"=~"[ \t\r\n]*"m".	{
		tally_newlines(yytext, yyleng);
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);  // next token starts at the "m"
		re_quote_char = *(yytext + yyleng - 1);
		L_lloc.end = L_lloc.beg + 2;    // this token spans the "=~"
		return T_EQTWID;
	}
	/* if / is used to delimit the regexp, the m can be omitted */
	"=~"[ \t\r\n]*"/"	{
		tally_newlines(yytext, yyleng);
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);  // next token starts at the "/"
		re_quote_char = '/';
		L_lloc.end = L_lloc.beg + 2;    // this token spans the "=~"
		return T_EQTWID;
	}
	/* a substitution pattern */
	"=~"[ \t\r\n]*"s".	{
		tally_newlines(yytext, yyleng);
		yy_push_state(re_modifier);
		yy_push_state(subst_re);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);  // next token starts at the "s"
		re_quote_char = *(yytext + yyleng - 1);
		L_lloc.end = L_lloc.beg + 2;    // this token spans the "=~"
		return T_EQTWID;
	}
}

<re_arg>{
	/* A regexp used as a function argument (e.g., in
	 * the "split" built-in). */
	[ \t\r\n]*"/"	{
		tally_newlines(yytext, yyleng);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);  // next token starts at the "/"
		re_quote_char = '/';
	}
	[ \t\r\n]*"m"[^a-zA-Z \t\r\n]	{
		tally_newlines(yytext, yyleng);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);  // next token starts at the "/"
		re_quote_char = *(yytext + yyleng - 1);
	}
}

<INITIAL>{
	"}"		return T_RBRACE;
}

<interpol>{
	"}"		{
				STRBUF_START(L_lloc.end);
				yy_pop_state();
				return T_RIGHT_INTERPOL;
			}
	"\\}"		return T_RBRACE;
}

<str_double>{
	\\r		STRBUF_ADD("\r", 1);
	\\n		STRBUF_ADD("\n", 1);
	\\t		STRBUF_ADD("\t", 1);
	\\(.|\n)	STRBUF_ADD(yytext+1, 1);
	"$"		STRBUF_ADD("$", 1);
	[^\\\"$]+	tally_newlines(yytext, yyleng); STRBUF_ADD(yytext, yyleng);
	"${"		{
				yy_push_state(interpol);
				MK_STRING_NODE(L_lval, STRBUF_STRING(),
					       str_beg, L_lloc.end);
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	\"[ \t\r\n]*\"	tally_newlines(yytext, yyleng);
	\"		{
				yy_pop_state();
				MK_STRING_NODE(L_lval, STRBUF_STRING(),
					       str_beg, L_lloc.end);
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<str_single>{
	\\n		STRBUF_ADD("\n", 1);
	\\t		STRBUF_ADD("\t", 1);
	\\(.|\n)	STRBUF_ADD(yytext+1, 1);
	[^\\\']+	tally_newlines(yytext, yyleng); STRBUF_ADD(yytext, yyleng);
	\'		{
				yy_pop_state();
				MK_STRING_NODE(L_lval, STRBUF_STRING(),
					       str_beg, L_lloc.end);
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<comment>{
	[^*]+		tally_newlines(yytext, yyleng);
	"*"
	"*/"		yy_pop_state();
}

<glob_re,subst_re>{
	"${"		{
				yy_push_state(interpol);
				MK_STRING_NODE(L_lval, STRBUF_STRING(),
					       str_beg, L_lloc.beg);
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	\\.		{
				if (yytext[1] == re_quote_char) {
					STRBUF_ADD(yytext+1, 1);
				} else {
					STRBUF_ADD(yytext, yyleng);
				}
			}
	.		{
				if (*(yytext) == re_quote_char) {
					MK_STRING_NODE(L_lval, STRBUF_STRING(),
						       str_beg, L_lloc.end);
					STRBUF_STOP(L_lloc.end);
					if (YYSTATE == subst_re) {
						yy_pop_state();
						return T_SUBST;
					} else {
						yy_pop_state();
						if (YYSTATE == subst_re) {
							STRBUF_START(L_lloc.end);
						}
						return T_RE;
					}
				} else {
					tally_newlines(yytext, yyleng);
					STRBUF_ADD(yytext, yyleng);
				}
			}

}
<re_modifier>{
	[ig]+		{
				MK_STRING_NODE(L_lval, yytext, L_lloc.beg, L_lloc.end);
				yy_pop_state();
				return T_RE_MODIFIER;
			}
	.		{
				unput(yytext[0]);
				/*
				 * This is ugly, but since we just
				 * unput a character, we have to undo
				 * the effects of the YY_USER_ACTION
				 * rule to keep the source-file
				 * offsets correct.  We rely on the
				 * fact that the previous token must
				 * have been a single character,
				 * matched by the "." rule in
				 * <glob_re,subst_re> above.
				 */
				--L_token_offset;
				--L_lloc.end;  // now, L_lloc.beg == L_lloc.end
				yy_pop_state();
				L_lval = NULL;
				return T_RE_MODIFIER;
			}
}
	\f		/* ignore page breaks */
	.		{	/* this is essentially an "else" */
				fprintf(stderr, "%s:%d: L error: illegal character '%c'",
				    L_source_file, L_line_number, *yytext);
				exit(1);
			}
