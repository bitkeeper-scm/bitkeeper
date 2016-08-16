/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     END = 0,
     T_ANDAND = 258,
     T_ARROW = 259,
     T_ATTRIBUTE = 260,
     T_BANG = 261,
     T_BANGTWID = 262,
     T_BITAND = 263,
     T_BITOR = 264,
     T_BITNOT = 265,
     T_BITXOR = 266,
     T_BREAK = 267,
     T_CLASS = 268,
     T_COLON = 269,
     T_COMMA = 270,
     T_CONSTRUCTOR = 271,
     T_CONTINUE = 272,
     T_DEFINED = 273,
     T_DESTRUCTOR = 274,
     T_DO = 275,
     T_DOT = 276,
     T_DOTDOT = 277,
     T_ELLIPSIS = 278,
     T_ELSE = 279,
     T_EQ = 280,
     T_EQBITAND = 281,
     T_EQBITOR = 282,
     T_EQBITXOR = 283,
     T_EQDOT = 284,
     T_EQLSHIFT = 285,
     T_EQMINUS = 286,
     T_EQPERC = 287,
     T_EQPLUS = 288,
     T_EQRSHIFT = 289,
     T_EQSTAR = 290,
     T_EQSLASH = 291,
     T_EQTWID = 292,
     T_EQUALS = 293,
     T_EQUALEQUAL = 294,
     T_EXPAND = 295,
     T_EXTERN = 296,
     T_FLOAT = 297,
     T_FLOAT_LITERAL = 298,
     T_FOR = 299,
     T_FOREACH = 300,
     T_GOTO = 301,
     T_GE = 302,
     T_GREATER = 303,
     T_GREATEREQ = 304,
     T_GT = 305,
     T_HTML = 306,
     T_ID = 307,
     T_IF = 308,
     T_INSTANCE = 309,
     T_INT = 310,
     T_INT_LITERAL = 311,
     T_LHTML_EXPR_START = 312,
     T_LHTML_EXPR_END = 313,
     T_LBRACE = 314,
     T_LBRACKET = 315,
     T_LE = 316,
     T_LEFT_INTERPOL = 317,
     T_LEFT_INTERPOL_RE = 318,
     T_LESSTHAN = 319,
     T_LESSTHANEQ = 320,
     T_LPAREN = 321,
     T_LSHIFT = 322,
     T_LT = 323,
     T_MINUS = 324,
     T_MINUSMINUS = 325,
     T_NE = 326,
     T_NOTEQUAL = 327,
     T_OROR = 328,
     T_PATTERN = 329,
     T_PERC = 330,
     T_PLUS = 331,
     T_PLUSPLUS = 332,
     T_POINTS = 333,
     T_POLY = 334,
     T_PRIVATE = 335,
     T_PUBLIC = 336,
     T_QUESTION = 337,
     T_RBRACE = 338,
     T_RBRACKET = 339,
     T_RE = 340,
     T_RE_MODIFIER = 341,
     T_RETURN = 342,
     T_RIGHT_INTERPOL = 343,
     T_RIGHT_INTERPOL_RE = 344,
     T_RPAREN = 345,
     T_RSHIFT = 346,
     T_TRY = 347,
     T_SEMI = 348,
     T_SLASH = 349,
     T_SPLIT = 350,
     T_STAR = 351,
     T_START_BACKTICK = 352,
     T_STR_BACKTICK = 353,
     T_STR_LITERAL = 354,
     T_STRCAT = 355,
     T_STRING = 356,
     T_STRUCT = 357,
     T_SUBST = 358,
     T_TYPE = 359,
     T_TYPEDEF = 360,
     T_UNLESS = 361,
     T_ARGUSED = 362,
     T_OPTIONAL = 363,
     T_MUSTBETYPE = 364,
     T_VOID = 365,
     T_WIDGET = 366,
     T_WHILE = 367,
     T_PRAGMA = 368,
     T_SWITCH = 369,
     T_CASE = 370,
     T_DEFAULT = 371,
     LOWEST = 372,
     ADDRESS = 373,
     UMINUS = 374,
     UPLUS = 375,
     PREFIX_INCDEC = 376,
     HIGHEST = 377
   };
#endif
/* Tokens.  */
#define END 0
#define T_ANDAND 258
#define T_ARROW 259
#define T_ATTRIBUTE 260
#define T_BANG 261
#define T_BANGTWID 262
#define T_BITAND 263
#define T_BITOR 264
#define T_BITNOT 265
#define T_BITXOR 266
#define T_BREAK 267
#define T_CLASS 268
#define T_COLON 269
#define T_COMMA 270
#define T_CONSTRUCTOR 271
#define T_CONTINUE 272
#define T_DEFINED 273
#define T_DESTRUCTOR 274
#define T_DO 275
#define T_DOT 276
#define T_DOTDOT 277
#define T_ELLIPSIS 278
#define T_ELSE 279
#define T_EQ 280
#define T_EQBITAND 281
#define T_EQBITOR 282
#define T_EQBITXOR 283
#define T_EQDOT 284
#define T_EQLSHIFT 285
#define T_EQMINUS 286
#define T_EQPERC 287
#define T_EQPLUS 288
#define T_EQRSHIFT 289
#define T_EQSTAR 290
#define T_EQSLASH 291
#define T_EQTWID 292
#define T_EQUALS 293
#define T_EQUALEQUAL 294
#define T_EXPAND 295
#define T_EXTERN 296
#define T_FLOAT 297
#define T_FLOAT_LITERAL 298
#define T_FOR 299
#define T_FOREACH 300
#define T_GOTO 301
#define T_GE 302
#define T_GREATER 303
#define T_GREATEREQ 304
#define T_GT 305
#define T_HTML 306
#define T_ID 307
#define T_IF 308
#define T_INSTANCE 309
#define T_INT 310
#define T_INT_LITERAL 311
#define T_LHTML_EXPR_START 312
#define T_LHTML_EXPR_END 313
#define T_LBRACE 314
#define T_LBRACKET 315
#define T_LE 316
#define T_LEFT_INTERPOL 317
#define T_LEFT_INTERPOL_RE 318
#define T_LESSTHAN 319
#define T_LESSTHANEQ 320
#define T_LPAREN 321
#define T_LSHIFT 322
#define T_LT 323
#define T_MINUS 324
#define T_MINUSMINUS 325
#define T_NE 326
#define T_NOTEQUAL 327
#define T_OROR 328
#define T_PATTERN 329
#define T_PERC 330
#define T_PLUS 331
#define T_PLUSPLUS 332
#define T_POINTS 333
#define T_POLY 334
#define T_PRIVATE 335
#define T_PUBLIC 336
#define T_QUESTION 337
#define T_RBRACE 338
#define T_RBRACKET 339
#define T_RE 340
#define T_RE_MODIFIER 341
#define T_RETURN 342
#define T_RIGHT_INTERPOL 343
#define T_RIGHT_INTERPOL_RE 344
#define T_RPAREN 345
#define T_RSHIFT 346
#define T_TRY 347
#define T_SEMI 348
#define T_SLASH 349
#define T_SPLIT 350
#define T_STAR 351
#define T_START_BACKTICK 352
#define T_STR_BACKTICK 353
#define T_STR_LITERAL 354
#define T_STRCAT 355
#define T_STRING 356
#define T_STRUCT 357
#define T_SUBST 358
#define T_TYPE 359
#define T_TYPEDEF 360
#define T_UNLESS 361
#define T_ARGUSED 362
#define T_OPTIONAL 363
#define T_MUSTBETYPE 364
#define T_VOID 365
#define T_WIDGET 366
#define T_WHILE 367
#define T_PRAGMA 368
#define T_SWITCH 369
#define T_CASE 370
#define T_DEFAULT 371
#define LOWEST 372
#define ADDRESS 373
#define UMINUS 374
#define UPLUS 375
#define PREFIX_INCDEC 376
#define HIGHEST 377




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 17 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
{
	long	i;
	char	*s;
	Tcl_Obj	*obj;
	Type	*Type;
	Expr	*Expr;
	Block	*Block;
	ForEach	*ForEach;
	Switch	*Switch;
	Case	*Case;
	FnDecl	*FnDecl;
	Cond	*Cond;
	Loop	*Loop;
	Stmt	*Stmt;
	TopLev	*TopLev;
	VarDecl	*VarDecl;
	ClsDecl	*ClsDecl;
	struct {
		Type	*t;
		char	*s;
	} Typename;
}
/* Line 1529 of yacc.c.  */
#line 318 "Lgrammar.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE L_lval;

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

extern YYLTYPE L_lloc;
