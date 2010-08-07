%option	noyywrap
%option noyy_top_state
%option stack
%x re_modifier
%s re_arg
%x glob_re
%x subst_re
%x comment
%x str_double
%x str_single
%x str_backtick
%x interpol
%x include
%x here_doc_interp
%x here_doc_nointerp
%x hash
%x hash2
ID	([a-zA-Z_]|::)([0-9a-zA-Z_]|::)*
%{
/*
 * Copyright (c) 2006-2008 BitMover, Inc.
 */
#include <string.h>
#define	_PWD_H			// Some solaris9 conflict, we don't need pwd.h
#include "tclInt.h"
#include "Lcompile.h"
#include "Lgrammar.h"

private int	include_pop();
private int	include_push(FILE *f, char *name);
private FILE *	include_search(char *file);
private void	put_back(char c);
private void	tally_newlines(char *s, int len, int tally);

// Stack for tracking include() statements.
#define INCLUDE_STACK_SZ	10
typedef struct {
	FILE	*file;
	char	*name;
	int	line;
	int	line_adj;
	YY_BUFFER_STATE	buf;
} Include;

private char	re_quote_char;	// delim char that began the current regexp
private Tcl_Obj	*str;		// string collection buffer
private int	str_beg;	// source offset of string
private char	*here_delim = NULL;
private int	include_top;
private Include	include_stk[INCLUDE_STACK_SZ+1];
private Tcl_HashTable *include_table = NULL;

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

/*
 * Keep track of the current offset in the input string.
 * YY_USER_ACTION is run before each action.  Note that some actions
 * further modify L_lloc.
 */

#define YY_USER_ACTION	yy_user_action();

private void
yy_user_action()
{
	L->prev_token_off = L->token_off;
	L->token_off     += L->prev_token_len;
	L->prev_token_len = yyleng;

	L_lloc.beg = L->token_off;
	L_lloc.end = L->token_off + yyleng;

	tally_newlines(yytext, yyleng, 1);

	/*
	 * Build up in L->script the text that the scanner scans.
	 * The compiler later passes this on to tcl as the script
	 * source.  This allows include() stmts to be handled properly.
	 */
	Tcl_AppendToObj(L->script, yytext, yyleng);
	L->script_len += yyleng;
}

/*
 * Un-do the effects of the YY_USER_ACTION on the token offset
 * tracking.  This is useful in include() processing where the
 * characters in the 'include("file");' must be ignored.
 */
private void
undo_yy_user_action()
{
	L->prev_token_len = L->token_off - L->prev_token_off;
	L->token_off      = L->prev_token_off;

	L_lloc.beg = L->prev_token_off;
	L_lloc.end = L->prev_token_off + L->prev_token_len;

	tally_newlines(yytext, yyleng, -1);

	L->script_len -= yyleng;
	Tcl_SetObjLength(L->script, L->script_len);
}

/*
 * In case of errors, the start condition stack will be left in an
 * unknown state.  This function gives us a way to reset it.
 */
void L_lex_start()
{
	include_top = -1;
	BEGIN(0);
}

/*
 * Count the newlines in a string and add the number to L->line.  Pass
 * in tally == 1 to count them and tally == -1 to undo it.
 */
private void
tally_newlines(char *s, int len, int tally)
{
	char	*end, *p;

	for (p = s, end = p + len; p < end; p++) {
		if (*p == '\n') {
			L->line += tally;
		} else if ((*p == '\r') && ((p+1) < end) && (*(p+1) != '\n')) {
			/* Mac line endings. */
			L->line += tally;
		}
	}
}

/*
 * Search for file in the include path and open it.  If the passed-in
 * name is not an absolute path, we currently look only in the
 * directory of the current script (i.e., dirname of [info script]).
 */
private FILE *
include_search(char *file)
{
	int	new;
	FILE	*f;
	char	*path, *resolvedFile;
	Interp	*iPtr = (Interp *)L->interp;
	Tcl_Obj *fileObj, *pathObj;
	Tcl_HashEntry *hPtr;

	unless (include_table) {
		include_table = (Tcl_HashTable *)ckalloc(sizeof(Tcl_HashTable));
		Tcl_InitHashTable(include_table, TCL_STRING_KEYS);
	}

	/* If the path is relative, make it absolute. */
	fileObj = Tcl_NewStringObj(file, -1);
	Tcl_IncrRefCount(fileObj);
	if ((Tcl_FSGetPathType(fileObj) == TCL_PATH_ABSOLUTE) ||
	    !iPtr->scriptFile) {
		resolvedFile = ckstrdup(file);
	} else {
		Tcl_DecrRefCount(fileObj);
		fileObj = TclPathPart(L->interp, iPtr->scriptFile,
				      TCL_PATH_DIRNAME);
		Tcl_AppendPrintfToObj(fileObj, "/%s", file);
		resolvedFile = ckstrdup(Tcl_GetString(fileObj));
	}

	/* See if the normalized path has been included before. */
	if ((pathObj = Tcl_FSGetNormalizedPath(NULL, fileObj)) == NULL) {
		L_err("unable to normalize include file %s\n", file);
		return (NULL);
	}
	path = Tcl_GetString(pathObj);
	hPtr = Tcl_CreateHashEntry(include_table, path, &new);

	if (new) {
		f = fopen(resolvedFile, "r");
		unless (f) {
			L_err("cannot find include file %s", file);
		}
	} else {
		f = NULL;
	}
	Tcl_DecrRefCount(fileObj);
	ckfree(resolvedFile);
	return (f);
}

private int
include_push(FILE *f, char *name)
{
	YY_BUFFER_STATE buf;

	if (include_top >= INCLUDE_STACK_SZ) {
		L_err("include file nesting too deep -- aborting");
		while (include_pop()) ;
		return (0);
	} else {
		buf = yy_create_buffer(f, YY_BUF_SIZE);
		++include_top;
		include_stk[include_top].file = f;
		include_stk[include_top].name = L->file;
		include_stk[include_top].line = L->line;
		include_stk[include_top].line_adj = L->line_adj;
		include_stk[include_top].buf = YY_CURRENT_BUFFER;
		yy_switch_to_buffer(buf);
		L->file = name;
		L->line_adj = L_offset_to_lineno(L_lloc.beg);
		L->line = 1;
		return (1);
	}
}

private int
include_pop()
{
	if (include_top >= 0) {
		L->file = include_stk[include_top].name;
		L->line = include_stk[include_top].line;
		L->line_adj = L_offset_to_lineno(L_lloc.beg) - (L->line - 1);
		fclose(include_stk[include_top].file);
		yy_delete_buffer(YY_CURRENT_BUFFER);
		yy_switch_to_buffer(include_stk[include_top].buf);
		--include_top;
		return (1);
	} else {
		return (0);
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
	".="		return T_EQDOT;
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
	[ \t\n\r]+"."[ \t\n\r]+		return T_STRCAT;
	".."		return T_DOTDOT;
	"..."		return T_ELLIPSIS;
	"class"		return T_CLASS;
	"extern"	return T_EXTERN;
	"return"	return T_RETURN;
	"void"		return T_VOID;
	"string"	return T_STRING;
	"widget"	return T_WIDGET;
	"int"		return T_INT;
	"float"		return T_FLOAT;
	"poly"		return T_POLY;
	"split"		return T_SPLIT;
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
	"instance"	return T_INSTANCE;
	"private"	return T_PRIVATE;
	"public"	return T_PUBLIC;
	"constructor"	return T_CONSTRUCTOR;
	"destructor"	return T_DESTRUCTOR;
	"expand"	return T_EXPAND;
	"_unused"	return T_UNUSED;
	"goto"		return T_GOTO;
	"switch"	return T_SWITCH;
	"case"		return T_CASE;
	"default"	return T_DEFAULT;
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
	"->"		return T_POINTS;
	":"		return T_COLON;
	"?"		return T_QUESTION;
	{ID}		{
				Type *t = L_typedef_lookup(yytext);
				if (t) {
					L_lval.Typename.s = ckstrdup(yytext);
					L_lval.Typename.t = t;
					return T_TYPE;
				} else {
					L_lval.s = ckstrdup(yytext);
					return T_ID;
				}
			}
	{ID}:		{
				/*
				 * Push back the : and return a T_ID
				 * unless it's "default".  The grammar relies
				 * on this to avoid a nasty conflict.
				 */
				put_back(':');
				if (!strncmp(yytext, "default", 7)) {
					return T_DEFAULT;
				}
				L_lval.s = ckstrdup(yytext);
				L_lval.s[yyleng-1] = 0;
				return T_ID;
			}
	([A-Z]|::)([0-9a-zA-Z]|::)*_\*	{
				L_lval.s = ckstrdup(yytext);
				return T_PATTERN;
			}
	$[0-9]+		{
				/* Regular expression submatches */
				L_lval.s = ckstrdup(yytext);
				return T_ID;
			}
	[0-9]+		{
				// XXX - we don't handle constants > 32 bits
				L_lval.i = atoi(yytext);
				return T_INT_LITERAL;
			}
	0o[0-7]+	{
				L_lval.i = strtoul(yytext+2, 0, 8);
				return T_INT_LITERAL;
			}
	0x[0-9a-fA-F]+	{
				// XXX - we don't handle constants > 32 bits
				L_lval.i = strtoul(yytext+2, 0, 16);
				return T_INT_LITERAL;
			}
	[0-9]*\.[0-9]+	{
				L_lval.f = atof(yytext);
				return T_FLOAT_LITERAL;
			}
	^include[ \t]*\([ \t]*\" {
				undo_yy_user_action();
				BEGIN(include);
			}
	^#		BEGIN(hash);
	[ \t]+#.*("\r"|"\n"|"\r\n") {
				--L->line;  // since we already scanned the \n
				L_err("# comment must start at first column");
				++L->line;
			}
	"//".*("\r"|"\n"|"\r\n")
	[ \t]+
	\n|\r|\f
	\"		yy_push_state(str_double); STRBUF_START(L->token_off);
	\'		yy_push_state(str_single); STRBUF_START(L->token_off);
	\`		yy_push_state(str_backtick); STRBUF_START(L->token_off);
	"/*"		yy_push_state(comment);
	"=~"[ \t\r\n]*"m".	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);	// next token starts at the "m"
		re_quote_char = *(yytext + yyleng - 1);
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return T_EQTWID;
	}
	/* if / is used to delimit the regexp, the m can be omitted */
	"=~"[ \t\r\n]*"/"	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
		re_quote_char = '/';
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return T_EQTWID;
	}
	/* a substitution pattern */
	"=~"[ \t\r\n]*"s".	{
		yy_push_state(re_modifier);
		yy_push_state(subst_re);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 2);	// next token starts at the "s"
		re_quote_char = *(yytext + yyleng - 1);
		L_lloc.end = L_lloc.beg + 2;	// this token spans the "=~"
		return T_EQTWID;
	}
	/* here document (interpolated), valid only on rhs of an assignment */
	"="[ \t\r\n]*"<<"[a-zA-Z_][a-zA-Z_0-9]*"\n"		{
		int	len;
		char	*p;
		if (here_delim) {
			L_err("nested here documents illegal");
		}
		/* point p at the here-document delimeter */
		for (p = yytext+1; *p != '<'; ++p) ;
		p += 2;  // now points one past the end of the <<
		len = yyleng - (p - yytext) - 1;
		here_delim = ckalloc(len+1);
		strncpy(here_delim, p, len);
		here_delim[len] = 0;
		STRBUF_START(L->token_off);
		L_lloc.end = L_lloc.beg + 1;
		yy_push_state(here_doc_interp);
		return T_EQUALS;
	}
	/* here document (uninterpolated), valid only on rhs of an assignment */
	"="[ \t\r\n]*"<<\'"[a-zA-Z_][a-zA-Z_0-9]*"\'\n"		{
		int	len;
		char	*p;
		if (here_delim) {
			L_err("nested here documents illegal");
		}
		/* point p at the here-document delimeter */
		for (p = yytext+1; *p != '<'; ++p) ;
		p += 3;  // now points one past the end of the <<'
		len = yyleng - (p - yytext) - 2;
		here_delim = ckalloc(len+1);
		strncpy(here_delim, p, len);
		here_delim[len] = 0;
		STRBUF_START(L->token_off);
		L_lloc.end = L_lloc.beg + 1;
		yy_push_state(here_doc_nointerp);
		return T_EQUALS;
	}
	/* illegal here documents (white space after the end delim) */
	"="[ \t\r\n]*"<<"[a-zA-Z_][a-zA-Z_0-9]*[^\n]		{
		L_synerr("illegal here-document delimeter");
	}
	"="[ \t\r\n]*"<<\'"[a-zA-Z_][a-zA-Z_0-9]*\'[^\n]		{
		L_synerr("illegal here-document delimeter");
	}
}

<include>{
	[^\"]*\"[ \t]*\)[ \t]*;[ \t;]*	{
		int	len = strrchr(yytext, '"') - yytext;
		char	*name = ckstrndup(yytext, len);
		FILE	*f = include_search(name);

		undo_yy_user_action();
		if (f) {
			/* This bails if includes nest too deeply. */
			unless (include_push(f, name)) yyterminate();
		}
		BEGIN(INITIAL);
	}
	.	{
		L_err("malformed include");
		BEGIN(INITIAL);
	}
}

<re_arg>{
	/*
	 * A regexp used as a function argument (e.g., in the "split"
	 * built-in).
	 */
	[ \t\r\n]*"/"	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
		re_quote_char = '/';
	}
	[ \t\r\n]*"m"[^a-zA-Z \t\r\n]	{
		yy_push_state(re_modifier);
		yy_push_state(glob_re);
		STRBUF_START(L_lloc.end - 1);	// next token starts at the "/"
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
	\n		{
				L_err("missing string terminator \"");
				STRBUF_ADD("\n", 1);
			}
	[^\\\"$\n]+	STRBUF_ADD(yytext, yyleng);
	"${"		{
				yy_push_state(interpol);
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	\"[ \t\r\n]*\"
	\"		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<str_single>{
	\\\\		STRBUF_ADD("\\", 1);
	\\\'		STRBUF_ADD("'", 1);
	\\\n		STRBUF_ADD("\n", 1);
	\n		{
				L_err("missing string terminator \'");
				STRBUF_ADD("\n", 1);
			}
	\\.		|
	[^\\\'\n]+	STRBUF_ADD(yytext, yyleng);
	\'[ \t\r\n]*\'
	\'		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				return T_STR_LITERAL;
			}
}

<str_backtick>{
	\\r		STRBUF_ADD("\r", 1);
	\\n		STRBUF_ADD("\n", 1);
	\\t		STRBUF_ADD("\t", 1);
	\\(.|\n)	STRBUF_ADD(yytext+1, 1);
	"$"		STRBUF_ADD("$", 1);
	\n		{
				L_err("missing string terminator `");
				STRBUF_ADD("\n", 1);
			}
	[^\\`$\n]+	STRBUF_ADD(yytext, yyleng);
	"${"		{
				yy_push_state(interpol);
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	`		{
				yy_pop_state();
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.end);
				return T_STR_BACKTICK;
			}
}

<here_doc_nointerp>{
	\\\\		STRBUF_ADD("\\", 1);
	\\\'		STRBUF_ADD("'", 1);
	^[a-zA-Z_][a-zA-Z_0-9]*$	{
				if (strcmp(yytext, here_delim)) {
					STRBUF_ADD(yytext, yyleng);
				} else {
					yy_pop_state();
					unput(';');  // inject ; for the parser
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					here_delim = NULL;
					return T_STR_LITERAL;
				}
			}
	^[a-zA-Z_][a-zA-Z_0-9]*[ \t\n]*;[ \t]*$	{
				/*
				 * This is an error check -- look for a
				 * trailing ; after end delimeter.
				 */
				int	len;
				char	*p = &yytext[yyleng-1];
				/* walk back over the ; to the delim end */
				while (*p == ' ') --p;
				--p;
				while (*p == ' ') --p;
				len = p - yytext;
				if (strncmp(yytext, here_delim, len)) {
					/* not the delim -- add to here doc */
					STRBUF_ADD(yytext, yyleng);
				} else {
					/* delim w/trailing ; found -- error */
					L_err("illegal ; after here document");
					yy_pop_state();
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					here_delim = NULL;
					return T_STR_LITERAL;
				}
			}
	.|\n		STRBUF_ADD(yytext, 1);
}

<here_doc_interp>{
	\\n		STRBUF_ADD("\n", 1);
	\\t		STRBUF_ADD("\t", 1);
	\\.		STRBUF_ADD(yytext+1, 1);
	"${"		{
				yy_push_state(interpol);
				L_lval.s = ckstrdup(STRBUF_STRING());
				STRBUF_STOP(L_lloc.beg);
				return T_LEFT_INTERPOL;
			}
	^[a-zA-Z_][a-zA-Z_0-9]*$	{
				if (strcmp(yytext, here_delim)) {
					STRBUF_ADD(yytext, yyleng);
				} else {
					yy_pop_state();
					unput(';');  // inject ; for the parser
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					here_delim = NULL;
					return T_STR_LITERAL;
				}
			}
	^[a-zA-Z_][a-zA-Z_0-9]*[ \t\n]*;[ \t]*$	{
				/*
				 * This is an error check -- look for a
				 * trailing ; after ending delimeter.
				 */
				int	len;
				char	*p = &yytext[yyleng-1];
				/* walk back over the ; to the delim end */
				while (*p == ' ') --p;
				--p;
				while (*p == ' ') --p;
				len = p - yytext;
				if (strncmp(yytext, here_delim, len)) {
					/* not the delim -- add to here doc */
					STRBUF_ADD(yytext, yyleng);
				} else {
					/* delim w/trailing ; found -- error */
					L_err("illegal ; after here document");
					yy_pop_state();
					L_lval.s = ckstrdup(STRBUF_STRING());
					STRBUF_STOP(L_lloc.end);
					ckfree(here_delim);
					here_delim = NULL;
					return T_STR_LITERAL;
				}
			}
	.|\n		STRBUF_ADD(yytext, 1);
}

<comment>{
	[^*]+
	"*"
	"*/"		yy_pop_state();
}

<glob_re,subst_re>{
	"${"		{
				yy_push_state(interpol);
				L_lval.s = ckstrdup(STRBUF_STRING());
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
					L_lval.s = ckstrdup(STRBUF_STRING());
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
					STRBUF_ADD(yytext, yyleng);
				}
			}

}
<re_modifier>{
	[ig]+		{
				L_lval.s = ckstrdup(yytext);
				yy_pop_state();
				return T_RE_MODIFIER;
			}
	.		{
				unput(yytext[0]);
				undo_yy_user_action();
				yy_pop_state();
				L_lval.s = ckstrdup("");
				return T_RE_MODIFIER;
			}
}
	.		{
				/* This rule matches a char if no other does. */
				L_err("illegal character '%c'", *yytext);
				yyterminate();
			}
	<<EOF>>		{
				unless (include_pop()) yyterminate();
			}
<hash>{
	pragma[ \t]+	BEGIN(INITIAL); return T_PRAGMA;
	file\ \"[^\"\n]*\"\n {
				char *name = ckstrndup(yytext + 6, yyleng - 8);
				/*
				 * Make like the #file wasn't there in the
				 * input, and reset the line # to 1.
				 */
				undo_yy_user_action();
				L->file = name;
				L->line = 1;
				L->line_adj = L_offset_to_lineno(L_lloc.end) - 1;
				BEGIN(INITIAL);
			}
	.		BEGIN(hash2);
}
<hash2>{
	.*("\r"|"\n"|"\r\n")	BEGIN(INITIAL);
}
%%
void
L_lex_begReArg()
{
	yy_push_state(re_arg);
}

void
L_lex_endReArg()
{
	yy_pop_state();
}

/*
 * Unput a single character.  This function is declared down here
 * because it calls flex's unput() which is not declared before
 * the prelogue code earlier.
 */
private void
put_back(char c)
{
	unput(c);
	--L_lloc.end;
	--L->prev_token_len;
	tally_newlines(&c, 1, -1);
	--L->script_len;
	Tcl_SetObjLength(L->script, L->script_len);
}
