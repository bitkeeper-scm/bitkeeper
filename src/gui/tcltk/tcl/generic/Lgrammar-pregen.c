/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 1

/* Substitute the variable and function names.  */
#define yyparse L_parse
#define yylex   L_lex
#define yyerror L_error
#define yylval  L_lval
#define yychar  L_char
#define yydebug L_debug
#define yynerrs L_nerrs
#define yylloc L_lloc

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




/* Copy the first part of user declarations.  */
#line 1 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"

/*
 * Copyright (c) 2006-2008 BitMover, Inc.
 */
#include <stdio.h>
#include "Lcompile.h"

/* L_lex is generated by flex. */
extern int	L_lex (void);

#define YYERROR_VERBOSE
#define L_error L_synerr


/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

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
/* Line 193 of yacc.c.  */
#line 387 "Lgrammar.c"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

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


/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 412 "Lgrammar.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
	     && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
    YYLTYPE yyls;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   4953

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  123
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  70
/* YYNRULES -- Number of rules.  */
#define YYNRULES  261
/* YYNRULES -- Number of states.  */
#define YYNSTATES  514

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   377

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     8,    11,    17,    20,    23,    24,
      25,    31,    32,    38,    42,    46,    49,    56,    62,    65,
      71,    74,    78,    82,    85,    86,    88,    89,    92,    96,
      99,   102,   108,   114,   118,   121,   123,   125,   127,   131,
     133,   137,   141,   145,   151,   157,   160,   165,   166,   168,
     170,   172,   174,   176,   178,   181,   184,   187,   190,   194,
     198,   206,   211,   213,   220,   226,   233,   239,   247,   250,
     251,   257,   261,   263,   265,   268,   271,   272,   278,   286,
     293,   301,   311,   319,   321,   324,   326,   327,   329,   332,
     334,   335,   337,   341,   345,   349,   352,   355,   358,   359,
     361,   363,   366,   370,   374,   379,   382,   385,   389,   394,
     399,   402,   405,   408,   411,   414,   417,   420,   423,   426,
     430,   434,   440,   444,   448,   452,   456,   460,   464,   468,
     472,   476,   480,   484,   488,   495,   499,   503,   507,   511,
     515,   519,   523,   527,   531,   535,   539,   543,   545,   547,
     549,   551,   553,   558,   562,   567,   575,   581,   586,   590,
     594,   598,   602,   606,   610,   614,   618,   622,   626,   630,
     634,   638,   643,   648,   653,   657,   661,   665,   669,   673,
     677,   684,   689,   692,   698,   702,   705,   706,   707,   709,
     711,   715,   719,   724,   729,   735,   736,   738,   741,   743,
     746,   748,   750,   752,   756,   759,   761,   765,   767,   771,
     773,   777,   779,   780,   783,   786,   790,   796,   797,   802,
     806,   811,   814,   817,   819,   821,   823,   825,   827,   829,
     831,   837,   842,   845,   847,   850,   853,   856,   858,   862,
     865,   867,   871,   873,   876,   879,   882,   886,   888,   891,
     893,   896,   899,   901,   904,   908,   913,   917,   922,   924,
     926,   929
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     124,     0,    -1,   125,    -1,   125,   126,    -1,   125,   132,
      -1,   125,   105,   175,   173,    93,    -1,   125,   166,    -1,
     125,   135,    -1,    -1,    -1,    13,   161,    59,   127,   129,
      -1,    -1,    13,   104,    59,   128,   129,    -1,    13,   161,
      93,    -1,    13,   104,    93,    -1,   130,    83,    -1,   130,
      54,    59,   165,    83,   131,    -1,   130,    54,    59,    83,
     131,    -1,   130,   166,    -1,   130,   105,   175,   173,    93,
      -1,   130,   132,    -1,   130,    16,   133,    -1,   130,    19,
     133,    -1,   130,   137,    -1,    -1,    93,    -1,    -1,   175,
     133,    -1,   167,   175,   133,    -1,   161,   134,    -1,    74,
     134,    -1,    66,   152,    90,   138,   163,    -1,    66,   152,
      90,   138,    93,    -1,    52,    14,   135,    -1,    52,    14,
      -1,   139,    -1,   137,    -1,    51,    -1,    57,   158,    58,
      -1,   161,    -1,   161,    38,   161,    -1,   161,    38,    56,
      -1,   136,    15,   161,    -1,   136,    15,   161,    38,   161,
      -1,   136,    15,   161,    38,    56,    -1,   113,   136,    -1,
       5,    66,   156,    90,    -1,    -1,   140,    -1,   163,    -1,
     141,    -1,   147,    -1,   142,    -1,   148,    -1,   158,    93,
      -1,    12,    93,    -1,    17,    93,    -1,    87,    93,    -1,
      87,   158,    93,    -1,    46,    52,    93,    -1,    92,   163,
      52,    66,   158,    90,   163,    -1,    92,   163,    52,   163,
      -1,    93,    -1,    53,    66,   158,    90,   163,   146,    -1,
      53,    66,   158,    90,   140,    -1,   106,    66,   158,    90,
     163,   146,    -1,   106,    66,   158,    90,   140,    -1,   114,
      66,   158,    90,    59,   143,    83,    -1,   143,   144,    -1,
      -1,   115,   160,   145,    14,   150,    -1,   116,    14,   150,
      -1,   187,    -1,   158,    -1,    24,   163,    -1,    24,   141,
      -1,    -1,   112,    66,   158,    90,   135,    -1,    20,   135,
     112,    66,   158,    90,    93,    -1,    44,    66,   149,   149,
      90,   135,    -1,    44,    66,   149,   149,   158,    90,   135,
      -1,    45,    66,   161,     4,   161,   161,   158,    90,   135,
      -1,    45,    66,   162,   161,   158,    90,   135,    -1,    93,
      -1,   158,    93,    -1,   151,    -1,    -1,   135,    -1,   151,
     135,    -1,   153,    -1,    -1,   154,    -1,   153,    15,   154,
      -1,   155,   175,   172,    -1,   155,    23,   161,    -1,   155,
     107,    -1,   155,   108,    -1,   155,   109,    -1,    -1,   158,
      -1,   157,    -1,   157,   158,    -1,   156,    15,   158,    -1,
     156,    15,   157,    -1,   156,    15,   157,   158,    -1,    52,
      14,    -1,   116,    14,    -1,    66,   158,    90,    -1,    66,
     175,    90,   158,    -1,    66,    40,    90,   158,    -1,     6,
     158,    -1,    10,   158,    -1,     8,   158,    -1,    69,   158,
      -1,    76,   158,    -1,    77,   158,    -1,    70,   158,    -1,
     158,    77,    -1,   158,    70,    -1,   158,    37,   187,    -1,
     158,     7,   187,    -1,   158,    37,   186,   188,    86,    -1,
     158,    96,   158,    -1,   158,    94,   158,    -1,   158,    75,
     158,    -1,   158,    76,   158,    -1,   158,    69,   158,    -1,
     158,    25,   158,    -1,   158,    71,   158,    -1,   158,    68,
     158,    -1,   158,    61,   158,    -1,   158,    50,   158,    -1,
     158,    47,   158,    -1,   158,    39,   158,    -1,    25,    66,
     158,    15,   158,    90,    -1,   158,    72,   158,    -1,   158,
      48,   158,    -1,   158,    49,   158,    -1,   158,    64,   158,
      -1,   158,    65,   158,    -1,   158,     3,   158,    -1,   158,
      73,   158,    -1,   158,    67,   158,    -1,   158,    91,   158,
      -1,   158,     9,   158,    -1,   158,     8,   158,    -1,   158,
      11,   158,    -1,   161,    -1,   183,    -1,   185,    -1,    56,
      -1,    43,    -1,   161,    66,   156,    90,    -1,   161,    66,
      90,    -1,   101,    66,   156,    90,    -1,    95,    66,   159,
     187,    15,   156,    90,    -1,    95,    66,   159,   156,    90,
      -1,   191,    66,   156,    90,    -1,   191,    66,    90,    -1,
     158,    38,   158,    -1,   158,    33,   158,    -1,   158,    31,
     158,    -1,   158,    35,   158,    -1,   158,    36,   158,    -1,
     158,    32,   158,    -1,   158,    26,   158,    -1,   158,    27,
     158,    -1,   158,    28,   158,    -1,   158,    30,   158,    -1,
     158,    34,   158,    -1,   158,    29,   158,    -1,    18,    66,
     158,    90,    -1,   158,    60,   158,    84,    -1,   158,    59,
     158,    83,    -1,   158,   100,   158,    -1,   158,    21,    52,
      -1,   158,    78,    52,    -1,   104,    21,    52,    -1,   104,
      78,    52,    -1,   158,    15,   158,    -1,   158,    60,   158,
      22,   158,    84,    -1,    59,   164,   181,    83,    -1,    59,
      83,    -1,   158,    82,   158,    14,   158,    -1,    64,   158,
      48,    -1,    64,    48,    -1,    -1,    -1,    52,    -1,   161,
      -1,   161,    15,   162,    -1,    59,   164,    83,    -1,    59,
     164,   151,    83,    -1,    59,   164,   165,    83,    -1,    59,
     164,   165,   151,    83,    -1,    -1,   166,    -1,   165,   166,
      -1,   168,    -1,   167,   168,    -1,    80,    -1,    81,    -1,
      41,    -1,   175,   169,    93,    -1,   175,    93,    -1,   171,
      -1,   169,    15,   171,    -1,   173,    -1,   170,    15,   173,
      -1,   173,    -1,   173,    38,   158,    -1,   173,    -1,    -1,
     161,   174,    -1,   104,   174,    -1,     8,   161,   174,    -1,
       8,   161,    66,   152,    90,    -1,    -1,    60,   158,    84,
     174,    -1,    60,    84,   174,    -1,    59,   176,    83,   174,
      -1,   176,   174,    -1,   177,   174,    -1,   101,    -1,    55,
      -1,    42,    -1,    79,    -1,   111,    -1,   110,    -1,   104,
      -1,   102,    52,    59,   178,    83,    -1,   102,    59,   178,
      83,    -1,   102,    52,    -1,   179,    -1,   178,   179,    -1,
     180,    93,    -1,   175,   170,    -1,   182,    -1,   181,    15,
     182,    -1,   181,    15,    -1,   158,    -1,   158,     4,   158,
      -1,    99,    -1,   189,    99,    -1,   184,    99,    -1,    97,
      98,    -1,   184,    97,    98,    -1,    98,    -1,   189,    98,
      -1,    85,    -1,   190,    85,    -1,   186,    86,    -1,   103,
      -1,   190,   103,    -1,    62,   158,    88,    -1,   189,    62,
     158,    88,    -1,    63,   158,    89,    -1,   190,    63,   158,
      89,    -1,    21,    -1,   192,    -1,    21,    52,    -1,   192,
      21,    52,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   214,   214,   222,   232,   243,   249,   264,   270,   275,
     274,   296,   295,   315,   327,   338,   354,   371,   372,   389,
     394,   408,   417,   426,   437,   441,   442,   446,   452,   462,
     468,   486,   493,   503,   509,   514,   515,   520,   529,   541,
     542,   546,   551,   557,   563,   573,   582,   589,   593,   594,
     598,   603,   608,   613,   618,   623,   627,   631,   635,   640,
     645,   657,   662,   666,   671,   675,   679,   686,   703,   712,
     716,   721,   730,   736,   741,   746,   751,   755,   759,   763,
     767,   774,   782,   793,   794,   798,   799,   803,   808,   822,
     839,   843,   844,   853,   865,   874,   875,   876,   877,   881,
     882,   883,   889,   895,   901,   918,   924,   932,   938,   943,
     947,   951,   955,   959,   963,   967,   971,   975,   979,   983,
     987,   991,   998,  1002,  1006,  1010,  1014,  1018,  1022,  1026,
    1030,  1034,  1038,  1042,  1046,  1050,  1054,  1058,  1062,  1066,
    1070,  1074,  1078,  1082,  1086,  1090,  1094,  1098,  1099,  1100,
    1101,  1105,  1109,  1114,  1118,  1124,  1138,  1145,  1150,  1154,
    1158,  1162,  1166,  1170,  1174,  1178,  1182,  1186,  1190,  1194,
    1198,  1202,  1206,  1210,  1214,  1218,  1223,  1228,  1234,  1240,
    1244,  1252,  1259,  1263,  1267,  1271,  1278,  1282,  1286,  1294,
    1295,  1304,  1310,  1317,  1328,  1343,  1347,  1348,  1360,  1361,
    1373,  1374,  1375,  1379,  1388,  1392,  1393,  1401,  1402,  1410,
    1411,  1420,  1421,  1425,  1429,  1436,  1442,  1454,  1457,  1461,
    1465,  1472,  1481,  1493,  1494,  1495,  1496,  1497,  1498,  1499,
    1503,  1509,  1515,  1523,  1524,  1533,  1537,  1549,  1550,  1555,
    1559,  1563,  1571,  1575,  1580,  1588,  1595,  1606,  1611,  1619,
    1623,  1631,  1644,  1648,  1656,  1661,  1669,  1674,  1682,  1686,
    1694,  1702
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "$undefined", "\"&&\"", "\"=>\"",
  "\"_attribute\"", "\"!\"", "\"!~\"", "\"&\"", "\"|\"", "\"~\"", "\"^\"",
  "\"break\"", "\"class\"", "\":\"", "\",\"", "\"constructor\"",
  "\"continue\"", "\"defined\"", "\"destructor\"", "\"do\"", "\".\"",
  "\"..\"", "\"...\"", "\"else\"", "\"eq\"", "\"&=\"", "\"|=\"", "\"^=\"",
  "\".=\"", "\"<<=\"", "\"-=\"", "\"%=\"", "\"+=\"", "\">>=\"", "\"*=\"",
  "\"/=\"", "\"=~\"", "\"=\"", "\"==\"", "\"(expand)\"", "\"extern\"",
  "\"float\"", "\"float constant\"", "\"for\"", "\"foreach\"", "\"goto\"",
  "\"ge\"", "\">\"", "\">=\"", "\"gt\"", "T_HTML", "\"id\"", "\"if\"",
  "\"instance\"", "\"int\"", "\"integer constant\"", "\"<?=\"", "\"?>\"",
  "\"{\"", "\"[\"", "\"le\"", "\"${\"", "\"${ (in re)\"", "\"<\"",
  "\"<=\"", "\"(\"", "\"<<\"", "\"lt\"", "\"-\"", "\"--\"", "\"ne\"",
  "\"!=\"", "\"||\"", "\"pattern function\"", "\"%\"", "\"+\"", "\"++\"",
  "\"->\"", "\"poly\"", "\"private\"", "\"public\"", "\"?\"", "\"}\"",
  "\"]\"", "\"regular expression\"", "\"regexp modifier\"", "\"return\"",
  "\"} (end of interpolation)\"", "\"} (end of interpolation in re)\"",
  "\")\"", "\">>\"", "\"try\"", "\";\"", "\"/\"", "\"split\"", "\"*\"",
  "\"backtick\"", "\"`\"", "\"string constant\"", "\" . \"", "\"string\"",
  "\"struct\"", "\"=~ s/a/b/\"", "\"type name\"", "\"typedef\"",
  "\"unless\"", "\"_argused\"", "\"_optional\"", "\"_mustbetype\"",
  "\"void\"", "\"widget\"", "\"while\"", "\"#pragma\"", "\"switch\"",
  "\"case\"", "\"default\"", "LOWEST", "ADDRESS", "UMINUS", "UPLUS",
  "PREFIX_INCDEC", "HIGHEST", "$accept", "start", "toplevel_code",
  "class_decl", "@1", "@2", "class_decl_tail", "class_code", "opt_semi",
  "function_decl", "fundecl_tail", "fundecl_tail1", "stmt",
  "pragma_expr_list", "pragma", "opt_attribute", "unlabeled_stmt",
  "single_stmt", "selection_stmt", "switch_stmt", "switch_cases",
  "switch_case", "case_expr", "optional_else", "iteration_stmt",
  "foreach_stmt", "expression_stmt", "opt_stmt_list", "stmt_list",
  "parameter_list", "parameter_decl_list", "parameter_decl",
  "parameter_attrs", "argument_expr_list", "option_arg", "expr",
  "re_start_split", "re_start_case", "id", "id_list", "compound_stmt",
  "enter_scope", "declaration_list", "declaration", "decl_qualifier",
  "declaration2", "init_declarator_list", "declarator_list",
  "init_declarator", "opt_declarator", "declarator", "array_or_hash_type",
  "type_specifier", "scalar_type_specifier", "struct_specifier",
  "struct_decl_list", "struct_decl", "struct_declarator_list", "list",
  "list_element", "string_literal", "here_doc_backtick",
  "cmdsubst_literal", "regexp_literal", "regexp_literal_mod",
  "subst_literal", "interpolated_expr", "interpolated_expr_re",
  "dotted_id", "dotted_id_1", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,   123,   124,   125,   125,   125,   125,   125,   125,   127,
     126,   128,   126,   126,   126,   129,   130,   130,   130,   130,
     130,   130,   130,   130,   130,   131,   131,   132,   132,   133,
     133,   134,   134,   135,   135,   135,   135,   135,   135,   136,
     136,   136,   136,   136,   136,   137,   138,   138,   139,   139,
     140,   140,   140,   140,   140,   140,   140,   140,   140,   140,
     140,   140,   140,   141,   141,   141,   141,   142,   143,   143,
     144,   144,   145,   145,   146,   146,   146,   147,   147,   147,
     147,   148,   148,   149,   149,   150,   150,   151,   151,   152,
     152,   153,   153,   154,   154,   155,   155,   155,   155,   156,
     156,   156,   156,   156,   156,   157,   157,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   159,   160,   161,   162,
     162,   163,   163,   163,   163,   164,   165,   165,   166,   166,
     167,   167,   167,   168,   168,   169,   169,   170,   170,   171,
     171,   172,   172,   173,   173,   173,   173,   174,   174,   174,
     174,   175,   175,   176,   176,   176,   176,   176,   176,   176,
     177,   177,   177,   178,   178,   179,   180,   181,   181,   181,
     182,   182,   183,   183,   183,   184,   184,   185,   185,   186,
     186,   187,   188,   188,   189,   189,   190,   190,   191,   191,
     192,   192
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     2,     5,     2,     2,     0,     0,
       5,     0,     5,     3,     3,     2,     6,     5,     2,     5,
       2,     3,     3,     2,     0,     1,     0,     2,     3,     2,
       2,     5,     5,     3,     2,     1,     1,     1,     3,     1,
       3,     3,     3,     5,     5,     2,     4,     0,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     3,     3,
       7,     4,     1,     6,     5,     6,     5,     7,     2,     0,
       5,     3,     1,     1,     2,     2,     0,     5,     7,     6,
       7,     9,     7,     1,     2,     1,     0,     1,     2,     1,
       0,     1,     3,     3,     3,     2,     2,     2,     0,     1,
       1,     2,     3,     3,     4,     2,     2,     3,     4,     4,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     3,
       3,     5,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     6,     3,     3,     3,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     1,     1,     1,
       1,     1,     4,     3,     4,     7,     5,     4,     3,     3,
       3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
       3,     4,     4,     4,     3,     3,     3,     3,     3,     3,
       6,     4,     2,     5,     3,     2,     0,     0,     1,     1,
       3,     3,     4,     4,     5,     0,     1,     2,     1,     2,
       1,     1,     1,     3,     2,     1,     3,     1,     3,     1,
       3,     1,     0,     2,     2,     3,     5,     0,     4,     3,
       4,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       5,     4,     2,     1,     2,     2,     2,     1,     3,     2,
       1,     3,     1,     2,     2,     2,     3,     1,     2,     1,
       2,     2,     1,     2,     3,     4,     3,     4,     1,     1,
       2,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       8,     0,     2,     1,     0,     0,     0,     0,     0,     0,
       0,     0,   258,     0,   202,   225,   151,     0,     0,     0,
      37,   188,     0,   224,   150,     0,   195,     0,     0,     0,
       0,     0,     0,     0,   226,   200,   201,     0,     0,    62,
       0,     0,   247,   242,   223,     0,   229,     0,     0,   228,
     227,     0,     0,     0,     3,     4,     7,    36,    35,    48,
      50,    52,    51,    53,     0,   147,    49,     6,     0,   198,
       0,   217,   217,   148,     0,   149,     0,     0,   259,   188,
     195,     0,     0,   110,   112,   111,    55,     0,     0,    56,
       0,     0,   260,     0,     0,     0,     0,    34,     0,     0,
     182,     0,     0,   185,     0,     0,     0,     0,   113,   116,
     114,   115,    57,     0,   195,     0,   186,   245,     0,   232,
       0,     0,     0,   223,   229,     0,     0,     0,    45,    39,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   118,     0,     0,     0,     0,
       0,   117,     0,     0,     0,    54,     0,     0,     0,     0,
     199,     0,     0,     0,   204,   217,    27,   217,     0,   205,
     209,     0,     0,   221,   222,     0,   244,     0,   248,   243,
       0,     0,     0,    11,    14,     9,    13,     0,     0,     0,
      83,     0,     0,   189,     0,    59,    33,     0,    38,   191,
      87,     0,   240,     0,   196,     0,     0,     0,   237,   254,
     184,     0,   107,     0,    58,     0,     0,     0,   188,     0,
       0,   100,    99,     0,     0,     0,   233,     0,   177,   178,
     217,     0,     0,     0,     0,     0,     0,   140,     0,   249,
       0,   120,     0,   145,   144,   146,   179,   175,   127,   165,
     166,   167,   170,   168,   161,   164,   160,   169,   162,   163,
       0,   119,   159,   133,   132,   136,   137,   131,     0,     0,
     130,   138,   139,   142,   129,   126,   128,   135,   141,   124,
     125,   176,     0,   143,   123,   122,   174,   153,     0,    28,
     217,    98,    30,   214,    29,   213,     0,   203,     0,     0,
     217,     0,   246,     0,   158,     0,   261,   240,    24,    24,
     171,     0,     0,     0,    84,     0,     0,     0,     0,   192,
      88,     0,   193,     0,   197,   239,   181,   109,   108,     0,
      61,     0,     0,   105,   106,     0,   154,   101,     0,   236,
     207,   231,   234,   235,     5,     0,     0,    42,    41,    40,
       0,     0,   251,     0,   250,   252,     0,     0,   173,     0,
     172,     0,   152,    98,   215,     0,    89,    91,     0,   206,
     210,   217,   219,   217,   255,   157,    12,     0,    10,     0,
     179,     0,     0,     0,   189,   190,     0,    64,    76,   241,
     194,   238,     0,   156,     0,   103,   102,   230,     0,    66,
      76,    77,     0,    69,   256,     0,   121,   253,     0,   183,
       0,    47,    98,     0,    95,    96,    97,   212,   220,   218,
       0,     0,     0,    15,     0,    20,    23,    18,     0,   134,
      79,     0,     0,     0,     0,    63,     0,     0,   104,   208,
      65,    44,    43,     0,   257,   180,   216,     0,     0,    92,
      94,    93,   211,    21,     0,    22,     0,     0,    78,    80,
       0,    82,    75,    74,    60,   155,    67,   187,     0,    68,
       0,    32,    31,    26,     0,     0,     0,     0,    86,     0,
      25,    17,    26,    19,    81,     0,    73,    72,    71,    85,
      46,    16,    86,    70
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,    54,   329,   328,   396,   397,   501,    55,
     186,   314,   220,   128,    57,   468,    58,    59,    60,    61,
     463,   489,   505,   455,    62,    63,   211,   508,   221,   385,
     386,   387,   388,   240,   241,    64,   237,   497,    65,   214,
      66,   101,   223,   224,   225,    69,   188,   359,   189,   471,
     190,   315,   226,    71,    72,   245,   246,   247,   227,   228,
      73,    74,    75,   260,   261,   376,    76,   262,    77,    78
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -260
static const yytype_int16 yypact[] =
{
    -260,    77,   814,  -260,  1910,  1910,  1910,   -68,   -28,   -64,
     -20,  1339,    39,    12,  -260,  -260,  -260,   122,   125,    91,
    -260,    44,   128,  -260,  -260,  1910,  -260,  1910,  1625,  1503,
    1910,  1910,  1910,  1910,  -260,  -260,  -260,  1682,   151,  -260,
     146,   -17,  -260,  -260,   154,   145,     2,   204,   155,  -260,
    -260,   157,   172,   159,  -260,  -260,  -260,  -260,  -260,  -260,
    -260,  -260,  -260,  -260,  2126,   160,  -260,  -260,   204,  -260,
       8,    75,    75,  -260,    21,  -260,    30,   161,   209,  -260,
     148,   154,     2,    -3,    -3,    -3,  -260,   -27,   -10,  -260,
    1910,   120,  -260,  1910,  1739,   172,   140,  1339,  1910,  2203,
    -260,   923,  2280,  -260,  3978,   144,  2358,   150,    -3,    -3,
      -3,    -3,  -260,  2435,  -260,   183,  -260,  -260,   717,   179,
     204,   190,   193,  -260,  -260,     7,  1910,  1910,   233,   211,
    1910,  1910,    46,  1910,  1910,  1910,  1910,   198,  1910,  1910,
    1910,  1910,  1910,  1910,  1910,  1910,  1910,  1910,  1910,  1910,
      46,  1910,  1910,  1910,  1910,  1910,  1910,  1910,  1910,  1910,
    1910,  1910,  1910,  1910,  1910,  -260,  1910,  1910,  1910,  1910,
    1910,  -260,   206,  1910,  1910,  -260,  1910,  1910,  1910,   567,
    -260,     8,   172,   194,  -260,    75,  -260,   136,     4,  -260,
     218,   248,  1796,  -260,  -260,   167,  -260,  1910,  -260,  -260,
     642,   214,  1910,  -260,  -260,  -260,  -260,  2512,   202,  4055,
    -260,  1739,  2589,   177,   172,  -260,  -260,  2666,  -260,  -260,
    -260,  1141,  2742,  1032,  -260,   204,     9,    11,  -260,  -260,
    1967,  1910,  -260,  1910,  -260,   923,   152,   502,   257,   260,
      13,  1910,  4360,   204,     7,   374,  -260,   184,  -260,  -260,
      75,   188,  2819,  2896,   172,    58,  2973,  4584,  1910,  -260,
     196,  -260,   114,  4795,  4656,  4728,  4360,  -260,  4853,  4360,
    4360,  4360,  4360,  4360,  4360,  4360,  4360,  4360,  4360,  4360,
      22,  -260,  4360,  4853,  1639,  1639,  1639,  1639,  3050,  2048,
    1639,  1639,  1639,    -7,  1639,   192,  4853,  4853,  4512,    -3,
     192,  -260,  3128,    -7,    -3,    -3,   192,  -260,    15,  -260,
     141,   199,  -260,  -260,  -260,  -260,     7,  -260,  1910,   208,
      75,  3205,  -260,  3283,  -260,    25,  -260,  4131,  -260,  -260,
    -260,  1910,  1910,  1853,  -260,   172,   172,  1910,  1438,  -260,
    -260,  1910,  -260,  1240,  -260,  1910,  -260,    -3,    -3,  1910,
    -260,    26,   277,  -260,  -260,   717,  -260,  4360,   432,   278,
    -260,  -260,  -260,  -260,  -260,  1438,  1339,   258,  -260,  -260,
     236,  3361,  -260,  1910,  -260,  -260,   212,   -15,  -260,  1910,
    -260,  1910,  -260,   199,  -260,   219,   284,  -260,   359,  -260,
    4360,    75,  -260,    75,  -260,  -260,  -260,   174,  -260,  3438,
    4207,  1339,  3515,   172,   287,  -260,  3592,  -260,   280,  4360,
    -260,  -260,  3669,  -260,   717,  1910,  4360,  -260,     7,  -260,
     280,  -260,    74,  -260,  -260,  3746,  -260,  -260,  3823,  4436,
     220,   307,  -260,   172,  -260,  -260,  -260,     7,  -260,  -260,
     135,   135,   254,  -260,   204,  -260,  -260,  -260,   225,  -260,
    -260,  1339,  1910,  1339,    -8,  -260,   151,    27,  4360,  -260,
    -260,  -260,  -260,   -61,  -260,  -260,  -260,   253,    -9,  -260,
    -260,  -260,  -260,  -260,   194,  -260,   329,     7,  -260,  -260,
    3901,  -260,  -260,  -260,  -260,  -260,  -260,  -260,   306,  -260,
     717,  -260,  -260,   229,  4658,   230,  1339,  1560,  1339,    29,
    -260,  -260,   229,  -260,  -260,   311,  4284,  -260,  -260,  1339,
    -260,  -260,  1339,  -260
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -260,  -260,  -260,  -260,  -260,  -260,     3,  -260,  -176,   -69,
    -168,   147,    -2,  -260,   -66,  -260,  -260,  -259,  -125,  -260,
    -260,  -260,  -260,   -85,  -260,  -260,   129,  -170,  -218,   -39,
    -260,   -96,  -260,  -173,     5,     6,  -260,  -260,   119,    10,
     -31,    42,  -128,    -1,     0,   -47,  -260,  -260,    37,  -260,
    -121,     1,    18,   163,  -260,   123,  -237,  -260,  -260,    17,
    -260,  -260,  -260,   207,  -147,  -260,  -260,    85,  -260,  -260
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -91
static const yytype_int16 yytable[] =
{
      56,    67,    68,   281,   251,   343,   308,   115,   362,    91,
      83,    84,    85,   309,   137,   182,   182,   182,   137,   316,
      70,   180,   486,   121,    79,    86,   345,   325,   355,    89,
     355,    99,   203,   102,   104,   106,   108,   109,   110,   111,
     355,   355,   355,   113,   355,    22,    90,   107,   373,   205,
     114,   114,   157,   158,   487,   488,   157,   158,    97,    79,
      79,    79,   164,   165,   351,   125,   204,   165,   169,   170,
     171,   172,   193,   194,   171,   172,    87,     3,    93,   407,
     122,   117,   183,   206,   491,   258,   181,   176,   427,   177,
     352,    92,   197,   178,   346,   216,   207,   317,    48,   209,
     212,   184,   184,   356,   217,   382,   419,   222,   372,   258,
      79,   185,   185,   185,   368,   395,   413,   485,   195,   510,
     196,   362,   202,   360,   242,   375,    79,    88,   198,   199,
     461,   259,   252,   253,   191,   192,   256,   257,   244,   263,
     264,   265,   266,    96,   268,   269,   270,   271,   272,   273,
     274,   275,   276,   277,   278,   279,   235,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   129,   296,   297,   298,   299,   300,   373,   180,   302,
     303,   335,   304,   305,   306,   242,   313,    79,    94,   187,
     440,    95,   336,   441,    98,   191,   192,   119,   321,   374,
     191,   192,   311,   323,   120,   350,   242,   383,   327,   183,
     114,   114,   116,   137,   213,    14,    15,   212,   349,   340,
     118,   126,   344,   127,    79,   130,   179,   200,   442,    23,
     201,   100,   208,   215,   231,   236,   285,   347,   243,   348,
     233,   457,   248,   242,   250,   249,    15,   357,   254,   255,
     267,   157,   158,    34,    35,    36,   318,   443,   301,    23,
     311,   244,   165,   244,   371,   322,   326,   169,   331,   171,
     172,   353,   473,   475,   354,   123,    45,   363,   124,   444,
     509,   364,   372,    34,    49,    50,   176,    52,   177,   -90,
      15,   391,   414,   418,   509,   423,   422,   459,   426,   432,
     187,   310,   336,    23,   454,   123,    45,   408,   124,   431,
     466,   384,   467,   476,    49,    50,   472,   499,   478,   490,
     498,   392,   500,   503,   390,   512,   511,    34,   445,   482,
     312,   446,   398,   337,   420,   460,   469,   399,   400,   402,
     333,   340,   513,   406,   430,   250,   405,   409,   494,   123,
     507,   327,   124,   389,   319,   412,   495,   280,    49,    50,
     415,   416,   411,   250,   421,   377,   358,     0,     0,     0,
      14,    15,     0,   367,   369,     0,   244,     0,     0,   425,
       0,     0,   433,     0,    23,   428,     0,   429,     0,     0,
       0,     0,   438,     0,   439,     0,   447,    68,     0,   450,
       0,    15,     0,     0,     0,     0,   437,     0,    34,    35,
      36,     0,   493,     0,    23,    70,    15,     0,     0,     0,
     242,   458,     0,   483,     0,   484,     0,     0,     0,    23,
     123,    45,     0,   124,     0,   250,     0,   492,    34,    49,
      50,     0,     0,     0,     0,     0,     0,     0,     0,   479,
       0,   481,     0,    34,   403,   404,     0,   361,   480,     0,
     123,    45,   477,   124,     0,     0,   434,   435,   436,    49,
      50,     0,     0,     0,    15,   123,    45,     0,   124,     0,
       0,     0,     0,     0,    49,    50,     0,    23,     0,     0,
       0,     0,     0,   344,   504,     0,   242,     0,     0,     0,
       0,     0,     0,   506,     0,     0,     0,   340,     4,     0,
       5,    34,     6,     0,     0,   417,     0,     0,     0,     0,
      10,     0,   452,    12,     0,     0,     0,    13,     0,     0,
       0,     0,     0,   123,    45,     0,   124,   250,     0,     0,
       0,   462,    49,    50,     0,    16,     0,     0,     0,     0,
       0,     0,   470,     0,   238,     0,   250,     0,    24,   474,
     474,    80,     0,     0,    27,   258,    28,     0,    29,     0,
       0,    30,    31,     4,     0,     5,     0,     6,    32,    33,
       0,     0,     0,     0,     0,    10,     0,   259,    12,     0,
       0,     0,    13,     0,     0,     0,   250,    40,     0,    41,
      42,    43,     0,    81,     0,     0,    82,     0,     0,     0,
      16,     0,     0,     0,     0,     0,     0,     0,   239,   238,
       0,     0,     0,    24,     0,     0,    80,     0,     0,    27,
       0,    28,     0,    29,     0,     0,    30,    31,     0,     0,
       0,     0,     0,    32,    33,     0,     0,     0,     4,     0,
       5,     0,     6,     0,     0,     0,     0,   307,     0,     0,
      10,     0,    40,    12,    41,    42,    43,    13,    81,     0,
       0,    82,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   239,     0,    16,     0,     0,     0,     0,
       0,     0,     0,     0,   238,     0,     0,     0,    24,     0,
       0,    80,     0,     0,    27,     0,    28,     0,    29,     0,
       0,    30,    31,     0,     0,     0,     0,     0,    32,    33,
       0,     0,     0,     4,     0,     5,     0,     6,     0,     0,
       0,     0,   324,     0,     0,    10,     0,    40,    12,    41,
      42,    43,    13,    81,     0,     0,    82,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   239,     0,
      16,     0,     0,     0,     0,     0,     0,     0,     0,   238,
       0,     0,     0,    24,     0,     0,    80,     0,     0,    27,
       0,    28,     0,    29,     0,     0,    30,    31,     0,     0,
       0,     0,     0,    32,    33,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    40,     0,    41,    42,    43,     0,    81,     0,
       4,    82,     5,     0,     6,     0,     7,     8,     0,     0,
       0,     9,    10,   239,    11,    12,     0,     0,     0,    13,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    14,    15,    16,    17,    18,
      19,     0,     0,     0,     0,    20,    21,    22,     0,    23,
      24,    25,     0,    26,     0,     0,    27,     0,    28,     0,
      29,     0,     0,    30,    31,     0,     0,     0,     0,     0,
      32,    33,     0,    34,    35,    36,     0,     0,     0,     0,
       0,    37,     0,     0,     0,     0,    38,    39,     0,    40,
       0,    41,    42,    43,     0,    44,    45,     0,    46,    47,
      48,     0,     0,     0,    49,    50,    51,    52,    53,     4,
       0,     5,     0,     6,     0,     7,     0,     0,     0,     0,
       9,    10,     0,    11,    12,     0,     0,     0,    13,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    14,    15,    16,    17,    18,    19,
       0,     0,     0,     0,    20,    21,    22,     0,    23,    24,
      25,     0,    26,     0,     0,    27,     0,    28,     0,    29,
       0,     0,    30,    31,     0,     0,     0,     0,     0,    32,
      33,     0,    34,    35,    36,     0,   219,     0,     0,     0,
      37,     0,     0,     0,     0,    38,    39,     0,    40,     0,
      41,    42,    43,     0,    44,    45,     0,    46,     0,    48,
       0,     0,     0,    49,    50,    51,    52,    53,     4,     0,
       5,     0,     6,     0,     7,     0,     0,     0,     0,     9,
      10,     0,    11,    12,     0,     0,     0,    13,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    14,    15,    16,    17,    18,    19,     0,
       0,     0,     0,    20,    21,    22,     0,    23,    24,    25,
       0,    26,     0,     0,    27,     0,    28,     0,    29,     0,
       0,    30,    31,     0,     0,     0,     0,     0,    32,    33,
       0,    34,    35,    36,     0,   342,     0,     0,     0,    37,
       0,     0,     0,     0,    38,    39,     0,    40,     0,    41,
      42,    43,     0,    44,    45,     0,    46,     0,    48,     0,
       0,     0,    49,    50,    51,    52,    53,     4,     0,     5,
       0,     6,     0,     7,     0,     0,     0,     0,     9,    10,
       0,    11,    12,     0,     0,     0,    13,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    16,    17,    18,    19,     0,     0,
       0,     0,    20,    21,    22,     0,     0,    24,    25,     0,
      26,     0,     0,    27,     0,    28,     0,    29,     0,     0,
      30,    31,     0,     0,     0,     0,     0,    32,    33,     0,
       0,     0,     0,     0,   339,     0,     0,     0,    37,     0,
       0,     0,     0,    38,    39,     0,    40,     0,    41,    42,
      43,     0,    81,     0,     0,    82,     4,    48,     5,     0,
       6,     0,     7,    51,    52,    53,     0,     9,    10,     0,
      11,    12,     0,     0,     0,    13,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    16,    17,    18,    19,     0,     0,     0,
       0,    20,    21,    22,     0,     0,    24,    25,     0,    26,
       0,     0,    27,     0,    28,     0,    29,     0,     0,    30,
      31,     0,     0,     0,     0,     0,    32,    33,     0,     0,
       0,     0,     0,   410,     0,     0,     0,    37,     0,     0,
       0,     0,    38,    39,     0,    40,     0,    41,    42,    43,
       0,    81,     0,     0,    82,     4,    48,     5,     0,     6,
       0,     7,    51,    52,    53,     0,     9,    10,     0,    11,
      12,     0,     0,     0,    13,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    16,    17,    18,    19,     0,     0,     0,     0,
      20,    21,    22,     0,     0,    24,    25,     0,    26,     0,
       0,    27,     0,    28,     0,    29,     0,     0,    30,    31,
       0,     0,     0,     0,     0,    32,    33,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    37,     0,     0,     0,
       0,    38,    39,     0,    40,     0,    41,    42,    43,     0,
      81,     0,     0,    82,     4,    48,     5,     0,     6,     0,
       7,    51,    52,    53,     0,     9,    10,     0,    11,    12,
       0,     0,     0,    13,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    16,    17,    18,    19,     0,     0,     0,     0,     0,
      79,    22,     0,     0,    24,     0,     0,    26,     0,     0,
      27,     0,    28,     0,    29,     0,     0,    30,    31,     4,
       0,     5,     0,     6,    32,    33,     0,     0,     0,     0,
       0,    10,     0,     0,    12,    37,     0,     0,    13,     0,
      38,    39,     0,    40,     0,    41,    42,    43,     0,    81,
       0,     0,    82,   105,    48,    15,    16,     0,     0,     0,
      51,     0,    53,     0,     0,    79,     0,     0,    23,    24,
       0,     0,    80,     0,     0,    27,     4,    28,     5,    29,
       6,     0,    30,    31,     0,     0,     0,     0,    10,    32,
      33,    12,    34,     0,     0,    13,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    40,     0,
      41,    42,    43,    16,    44,    45,     0,    46,     0,     0,
       0,     0,    79,    49,    50,     0,    24,     0,     0,    80,
       0,     0,    27,   258,    28,     0,    29,     0,     0,    30,
      31,     4,     0,     5,     0,     6,    32,    33,     0,     0,
       0,     0,     0,    10,     0,   259,    12,     0,     0,     0,
      13,     0,     0,     0,     0,    40,     0,    41,    42,    43,
     137,    81,     0,     0,    82,     0,     0,     0,    16,     0,
       0,     0,     0,   103,     0,     0,     0,    79,     0,     0,
       0,    24,     0,     0,    80,     0,     0,    27,     4,    28,
       5,    29,     6,     0,    30,    31,     0,     0,   157,   158,
      10,    32,    33,    12,     0,     0,   162,    13,   164,   165,
       0,     0,     0,     0,   169,   170,   171,   172,     0,     0,
      40,     0,    41,    42,    43,    16,    81,     0,     0,    82,
     174,     0,     0,   176,    79,   177,     0,     0,    24,   178,
       0,    80,     0,     0,    27,     4,    28,     5,    29,     6,
       0,    30,    31,     0,     0,     0,     0,    10,    32,    33,
      12,     0,     0,     0,    13,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   112,     0,    40,     0,    41,
      42,    43,    16,    81,     0,     0,    82,     0,     0,     0,
       0,    79,     0,     0,     0,    24,     0,     0,    80,     0,
       0,    27,     4,    28,     5,    29,     6,     0,    30,    31,
       0,     0,     0,     0,    10,    32,    33,    12,     0,     0,
       0,    13,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   210,     0,    40,     0,    41,    42,    43,    16,
      81,     0,     0,    82,     0,     0,     0,     0,    79,     0,
       0,     0,    24,     0,     0,    80,     0,     0,    27,     4,
      28,     5,    29,     6,     0,    30,    31,     0,     0,     0,
       0,    10,    32,    33,    12,     0,     0,     0,    13,     0,
     320,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    40,     0,    41,    42,    43,    16,    81,     0,     0,
      82,     0,     0,     0,     0,    79,     0,     0,     0,    24,
       0,     0,    80,     0,     0,    27,     4,    28,     5,    29,
       6,     0,    30,    31,     0,     0,     0,     0,    10,    32,
      33,    12,     0,     0,     0,    13,     0,     0,     0,     0,
       0,     0,     0,   401,     0,     0,     0,     0,    40,     0,
      41,    42,    43,    16,    81,     0,     0,    82,     0,     0,
       0,     0,    79,     0,     0,     0,    24,     0,     0,    80,
       0,     0,    27,     4,    28,     0,    29,     6,     0,    30,
      31,     0,     0,     0,     0,    10,    32,    33,    12,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    40,     0,    41,    42,    43,
      16,    81,     0,     0,    82,     0,     0,     0,     0,    79,
       0,     0,     0,    24,     0,     0,    80,     0,     0,    27,
       0,     0,     0,    29,     0,     0,    30,    31,     0,     0,
       0,     0,     0,    32,    33,     0,     0,     0,     0,     0,
       0,   131,     0,     0,     0,   132,   133,   134,     0,   135,
       0,     0,    40,   136,    41,    42,    43,     0,    81,   137,
     379,    82,     0,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,     0,     0,
       0,     0,     0,     0,     0,   153,   154,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,   168,     0,   169,   170,   171,   172,     0,     0,   131,
     173,     0,   380,   132,   133,   134,     0,   135,     0,   174,
       0,   136,   176,     0,   177,     0,     0,   137,   178,     0,
       0,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,     0,     0,     0,     0,
       0,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,     0,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,   168,
       0,   169,   170,   171,   172,     0,   131,     0,   173,     0,
     132,   133,   134,     0,   135,     0,     0,   174,   136,   175,
     176,     0,   177,     0,   137,     0,   178,     0,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,     0,     0,     0,     0,     0,     0,     0,
     153,   154,   155,   156,     0,     0,     0,     0,     0,     0,
       0,   218,   157,   158,   159,     0,     0,   160,   161,     0,
     162,   163,   164,   165,   166,   167,   168,     0,   169,   170,
     171,   172,     0,   131,     0,   173,     0,   132,   133,   134,
       0,   135,     0,     0,   174,   136,     0,   176,     0,   177,
       0,   137,     0,   178,     0,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
       0,     0,     0,     0,     0,     0,     0,   153,   154,   155,
     156,     0,     0,     0,     0,     0,     0,     0,     0,   157,
     158,   159,     0,     0,   160,   161,     0,   162,   163,   164,
     165,   166,   167,   168,     0,   169,   170,   171,   172,     0,
       0,   131,   173,     0,     0,   132,   133,   134,   229,   135,
       0,   174,     0,   136,   176,     0,   177,     0,     0,   137,
     178,     0,     0,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,     0,     0,
       0,     0,     0,     0,     0,   153,   154,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,   168,     0,   169,   170,   171,   172,     0,   131,     0,
     173,     0,   132,   133,   134,     0,   135,     0,   232,   174,
     136,     0,   176,     0,   177,     0,   137,     0,   178,     0,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,     0,     0,     0,     0,     0,
       0,     0,   153,   154,   155,   156,     0,     0,     0,     0,
       0,     0,     0,     0,   157,   158,   159,     0,     0,   160,
     161,     0,   162,   163,   164,   165,   166,   167,   168,     0,
     169,   170,   171,   172,     0,   131,     0,   173,     0,   132,
     133,   134,     0,   135,     0,     0,   174,   136,   234,   176,
       0,   177,     0,   137,     0,   178,     0,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,     0,     0,     0,     0,     0,     0,     0,   153,
     154,   155,   156,     0,     0,     0,     0,     0,     0,     0,
       0,   157,   158,   159,     0,     0,   160,   161,     0,   162,
     163,   164,   165,   166,   167,   168,     0,   169,   170,   171,
     172,     0,   131,     0,   173,     0,   132,   133,   134,     0,
     135,     0,   330,   174,   136,     0,   176,     0,   177,     0,
     137,     0,   178,     0,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,     0,
       0,     0,     0,     0,     0,     0,   153,   154,   155,   156,
       0,     0,     0,     0,     0,     0,     0,     0,   157,   158,
     159,     0,     0,   160,   161,     0,   162,   163,   164,   165,
     166,   167,   168,     0,   169,   170,   171,   172,     0,   131,
       0,   173,     0,   132,   133,   134,     0,   135,     0,     0,
     174,   136,   334,   176,     0,   177,     0,   137,     0,   178,
       0,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,     0,     0,     0,     0,
       0,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,     0,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,   168,
       0,   169,   170,   171,   172,   131,   341,     0,   173,   132,
     133,   134,     0,   135,     0,     0,   338,   174,     0,     0,
     176,     0,   177,   137,     0,     0,   178,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,     0,     0,     0,     0,     0,     0,     0,   153,
     154,   155,   156,     0,     0,     0,     0,     0,     0,     0,
       0,   157,   158,   159,     0,     0,   160,   161,     0,   162,
     163,   164,   165,   166,   167,   168,     0,   169,   170,   171,
     172,     0,   131,     0,   173,     0,   132,   133,   134,     0,
     135,     0,     0,   174,   136,   175,   176,     0,   177,     0,
     137,     0,   178,     0,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,     0,
       0,     0,     0,     0,     0,     0,   153,   154,   155,   156,
       0,     0,     0,     0,     0,     0,     0,     0,   157,   158,
     159,     0,     0,   160,   161,     0,   162,   163,   164,   165,
     166,   167,   168,     0,   169,   170,   171,   172,     0,   131,
       0,   173,     0,   132,   133,   134,     0,   135,     0,   365,
     174,   136,     0,   176,     0,   177,     0,   137,     0,   178,
       0,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,     0,     0,     0,     0,
       0,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,     0,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,   168,
       0,   169,   170,   171,   172,     0,   131,     0,   173,     0,
     132,   133,   134,     0,   135,     0,   366,   174,   136,     0,
     176,     0,   177,     0,   137,     0,   178,     0,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,     0,     0,     0,     0,     0,     0,     0,
     153,   154,   155,   156,     0,     0,     0,     0,     0,     0,
       0,     0,   157,   158,   159,     0,     0,   160,   161,     0,
     162,   163,   164,   165,   166,   167,   168,     0,   169,   170,
     171,   172,     0,   131,     0,   173,     0,   132,   133,   134,
       0,   135,     0,   370,   174,   136,     0,   176,     0,   177,
       0,   137,     0,   178,     0,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
       0,     0,     0,     0,     0,     0,     0,   153,   154,   155,
     156,     0,     0,     0,     0,     0,     0,     0,     0,   157,
     158,   159,     0,     0,   160,   161,     0,   162,   163,   164,
     165,   166,   167,   168,     0,   169,   170,   171,   172,     0,
       0,   131,   173,   378,     0,   132,   133,   134,     0,   135,
       0,   174,   381,   136,   176,     0,   177,     0,     0,   137,
     178,     0,     0,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,     0,     0,
       0,     0,     0,     0,     0,   153,   154,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,   168,     0,   169,   170,   171,   172,     0,   131,     0,
     173,     0,   132,   133,   134,     0,   135,     0,     0,   174,
     136,     0,   176,     0,   177,     0,   137,     0,   178,     0,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,     0,     0,     0,     0,     0,
       0,     0,   153,   154,   155,   156,     0,     0,     0,     0,
       0,     0,     0,     0,   157,   158,   159,     0,     0,   160,
     161,     0,   162,   163,   164,   165,   166,   167,   168,     0,
     169,   170,   171,   172,     0,     0,   131,   173,     0,   393,
     132,   133,   134,     0,   135,     0,   174,     0,   136,   176,
       0,   177,     0,     0,   137,   178,     0,     0,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,     0,     0,     0,     0,     0,     0,     0,
     153,   154,   155,   156,     0,     0,     0,     0,     0,     0,
       0,     0,   157,   158,   159,     0,     0,   160,   161,     0,
     162,   163,   164,   165,   166,   167,   168,     0,   169,   170,
     171,   172,     0,     0,   131,   173,     0,     0,   132,   133,
     134,   394,   135,     0,   174,     0,   136,   176,     0,   177,
       0,     0,   137,   178,     0,     0,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,     0,     0,     0,     0,     0,     0,     0,   153,   154,
     155,   156,     0,     0,     0,     0,     0,     0,     0,     0,
     157,   158,   159,     0,     0,   160,   161,     0,   162,   163,
     164,   165,   166,   167,   168,     0,   169,   170,   171,   172,
       0,   131,     0,   173,     0,   132,   133,   134,     0,   135,
     424,     0,   174,   136,     0,   176,     0,   177,     0,   137,
       0,   178,     0,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,     0,     0,
       0,     0,     0,     0,     0,   153,   154,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,   168,     0,   169,   170,   171,   172,     0,   131,     0,
     173,     0,   132,   133,   134,     0,   135,     0,   448,   174,
     136,     0,   176,     0,   177,     0,   137,     0,   178,     0,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,     0,     0,     0,     0,     0,
       0,     0,   153,   154,   155,   156,     0,     0,     0,     0,
       0,     0,     0,     0,   157,   158,   159,     0,     0,   160,
     161,     0,   162,   163,   164,   165,   166,   167,   168,     0,
     169,   170,   171,   172,     0,   131,     0,   173,     0,   132,
     133,   134,     0,   135,     0,   451,   174,   136,     0,   176,
       0,   177,     0,   137,     0,   178,     0,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,     0,     0,     0,     0,     0,     0,     0,   153,
     154,   155,   156,     0,     0,     0,     0,     0,     0,     0,
       0,   157,   158,   159,     0,     0,   160,   161,     0,   162,
     163,   164,   165,   166,   167,   168,     0,   169,   170,   171,
     172,     0,   131,     0,   173,     0,   132,   133,   134,     0,
     135,     0,   453,   174,   136,     0,   176,     0,   177,     0,
     137,     0,   178,     0,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,     0,
       0,     0,     0,     0,     0,     0,   153,   154,   155,   156,
       0,     0,     0,     0,     0,     0,     0,     0,   157,   158,
     159,     0,     0,   160,   161,     0,   162,   163,   164,   165,
     166,   167,   168,     0,   169,   170,   171,   172,     0,   131,
       0,   173,     0,   132,   133,   134,     0,   135,     0,   456,
     174,   136,     0,   176,     0,   177,     0,   137,     0,   178,
       0,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,     0,     0,     0,     0,
       0,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,     0,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,   168,
       0,   169,   170,   171,   172,     0,   131,     0,   173,     0,
     132,   133,   134,     0,   135,   464,     0,   174,   136,     0,
     176,     0,   177,     0,   137,     0,   178,     0,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,     0,     0,     0,     0,     0,     0,     0,
     153,   154,   155,   156,     0,     0,     0,     0,     0,     0,
       0,     0,   157,   158,   159,     0,     0,   160,   161,     0,
     162,   163,   164,   165,   166,   167,   168,     0,   169,   170,
     171,   172,     0,     0,   131,   173,     0,   465,   132,   133,
     134,     0,   135,     0,   174,     0,   136,   176,     0,   177,
       0,     0,   137,   178,     0,     0,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,     0,     0,     0,     0,     0,     0,     0,   153,   154,
     155,   156,     0,     0,     0,     0,     0,     0,     0,     0,
     157,   158,   159,     0,     0,   160,   161,     0,   162,   163,
     164,   165,   166,   167,   168,     0,   169,   170,   171,   172,
       0,   131,     0,   173,     0,   132,   133,   134,     0,   135,
       0,   496,   174,   136,     0,   176,     0,   177,     0,   137,
       0,   178,     0,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,     0,     0,
       0,     0,     0,     0,     0,   153,   230,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,   168,     0,   169,   170,   171,   172,     0,   131,     0,
     173,     0,   132,   133,   134,     0,   135,     0,     0,   174,
     332,     0,   176,     0,   177,     0,   137,     0,   178,     0,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,     0,     0,     0,     0,     0,
       0,     0,   153,   154,   155,   156,     0,     0,     0,     0,
       0,     0,     0,     0,   157,   158,   159,     0,     0,   160,
     161,     0,   162,   163,   164,   165,   166,   167,   168,     0,
     169,   170,   171,   172,   131,   341,     0,   173,   132,   133,
     134,     0,   135,     0,     0,     0,   174,     0,     0,   176,
       0,   177,   137,     0,     0,   178,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,     0,     0,     0,     0,     0,     0,     0,   153,   154,
     155,   156,     0,     0,     0,     0,     0,     0,     0,     0,
     157,   158,   159,     0,     0,   160,   161,     0,   162,   163,
     164,   165,   166,   167,   168,     0,   169,   170,   171,   172,
     131,     0,     0,   173,   132,   133,   134,     0,   135,     0,
       0,     0,   174,     0,     0,   176,     0,   177,   137,     0,
       0,   178,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,     0,     0,     0,
       0,     0,     0,     0,   153,   154,   155,   156,     0,     0,
       0,     0,     0,     0,     0,     0,   157,   158,   159,     0,
       0,   160,   161,     0,   162,   163,   164,   165,   166,   167,
     168,     0,   169,   170,   171,   172,     0,   131,     0,   173,
       0,   132,   133,   134,     0,   135,     0,   449,   174,   136,
       0,   176,     0,   177,     0,   137,     0,   178,     0,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,     0,     0,     0,     0,     0,     0,
       0,   153,   154,   155,   156,     0,     0,     0,     0,     0,
       0,     0,     0,   157,   158,   159,     0,     0,   160,   161,
       0,   162,   163,   164,   165,   166,   167,   168,     0,   169,
     170,   171,   172,   131,     0,     0,   173,   132,   133,   134,
       0,   135,     0,     0,     0,   174,     0,     0,   176,     0,
     177,   137,     0,     0,   178,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
       0,     0,     0,     0,     0,     0,     0,   153,   154,   155,
     156,     0,     0,     0,     0,     0,     0,     0,     0,   157,
     158,   159,     0,     0,   160,   161,     0,   162,   163,   164,
     165,   166,   167,   168,     0,   169,   170,   171,   172,   131,
       0,     0,   173,   132,   133,   134,     0,   135,     0,     0,
       0,   174,     0,     0,   176,     0,   177,   137,     0,     0,
     178,   138,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   150,     0,   152,     0,     0,     0,     0,
       0,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,     0,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,   168,
       0,   169,   170,   171,   172,   131,     0,     0,   173,   132,
     133,   134,     0,   135,     0,     0,     0,   174,     0,     0,
     176,     0,   177,   137,     0,     0,   178,   138,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   150,
       0,   152,     0,     0,     0,     0,     0,     0,     0,   153,
     154,   155,   156,     0,     0,     0,     0,     0,     0,     0,
       0,   157,   158,   159,     0,     0,   160,   161,     0,   162,
     163,   164,   165,   166,   167,     0,     0,   169,   170,   171,
     172,   132,   133,   134,     0,   135,     0,     0,     0,     0,
       0,     0,     0,   174,     0,   137,   176,     0,   177,   138,
       0,     0,   178,     0,     0,     0,     0,     0,     0,     0,
       0,   150,     0,   152,     0,     0,     0,     0,     0,     0,
       0,   153,   154,   155,   156,     0,     0,     0,     0,     0,
       0,     0,     0,   157,   158,   159,     0,     0,   160,   161,
       0,   162,   163,   164,   165,   166,   167,     0,     0,   169,
     170,   171,   172,   132,   133,     0,     0,   135,     0,     0,
       0,     0,     0,     0,     0,   174,     0,   137,   176,     0,
     177,   138,     0,     0,   178,     0,     0,     0,     0,     0,
       0,     0,     0,   150,     0,   152,     0,     0,     0,    14,
      15,     0,     0,   153,   154,   155,   156,     0,     0,     0,
       0,     0,     0,    23,     0,   157,   158,   159,     0,     0,
     160,   161,     0,   162,   163,   164,   165,   166,   167,     0,
       0,   169,   170,   171,   172,   132,   133,    34,    35,    36,
       0,   502,     0,     0,     0,     0,     0,   174,     0,   137,
     176,     0,   177,   138,     0,     0,   178,     0,     0,   123,
      45,     0,   124,     0,     0,   150,     0,   152,    49,    50,
       0,     0,     0,     0,     0,   153,   154,   155,   156,     0,
       0,     0,     0,     0,     0,     0,     0,   157,   158,   159,
       0,     0,   160,   161,     0,   162,   163,   164,   165,   166,
     167,     0,   132,   169,   170,   171,   172,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   137,     0,     0,   174,
     138,     0,   176,     0,   177,     0,     0,     0,   178,     0,
       0,     0,   150,     0,   152,     0,     0,     0,     0,     0,
       0,     0,   153,   154,   155,   156,     0,     0,     0,     0,
       0,     0,     0,     0,   157,   158,   159,     0,     0,   160,
     161,     0,   162,   163,   164,   165,   166,   167,     0,     0,
     169,   170,   171,   172,   137,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   174,     0,     0,   176,
       0,   177,     0,     0,     0,   178,     0,     0,     0,     0,
     153,   154,   155,   156,     0,     0,     0,     0,     0,     0,
       0,     0,   157,   158,   159,     0,     0,   160,   161,     0,
     162,   163,   164,   165,     0,     0,     0,     0,   169,   170,
     171,   172,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   174,     0,     0,   176,     0,   177,
       0,     0,     0,   178
};

static const yytype_int16 yycheck[] =
{
       2,     2,     2,   150,   125,   223,   179,    38,   245,    11,
       4,     5,     6,   181,    21,     8,     8,     8,    21,    15,
       2,    68,    83,    21,    52,    93,    15,   200,    15,    93,
      15,    25,    59,    27,    28,    29,    30,    31,    32,    33,
      15,    15,    15,    37,    15,    53,    66,    29,    63,    59,
      59,    59,    59,    60,   115,   116,    59,    60,    14,    52,
      52,    52,    69,    70,   237,    47,    93,    70,    75,    76,
      77,    78,    71,    72,    77,    78,   104,     0,    66,   338,
      78,    98,    74,    93,    93,    63,    68,    94,   103,    96,
     237,    52,    62,   100,    83,    97,    90,    93,   106,    93,
      94,    93,    93,    90,    98,    90,   365,   101,    86,    63,
      52,   104,   104,   104,    56,    90,    90,    90,    97,    90,
      99,   358,    80,   244,   118,   103,    52,     8,    98,    99,
      56,    85,   126,   127,    59,    60,   130,   131,   120,   133,
     134,   135,   136,    52,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   114,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,    52,   166,   167,   168,   169,   170,    63,   225,   173,
     174,     4,   176,   177,   178,   179,   185,    52,    66,    70,
      16,    66,    15,    19,    66,    59,    60,    52,   192,    85,
      59,    60,    66,   197,    59,   236,   200,    66,   202,    74,
      59,    59,    66,    21,    95,    41,    42,   211,    66,   221,
      66,    66,   223,    66,    52,    66,    66,    66,    54,    55,
      21,    83,   112,    93,    90,    52,   230,   231,    59,   233,
      90,   414,    52,   237,   125,    52,    42,   241,    15,    38,
      52,    59,    60,    79,    80,    81,    38,    83,    52,    55,
      66,   243,    70,   245,   258,    98,    52,    75,    66,    77,
      78,    14,   440,   441,    14,   101,   102,    93,   104,   105,
     498,    93,    86,    79,   110,   111,    94,   113,    96,    90,
      42,    83,    15,    15,   512,    59,    38,   418,    86,    15,
     181,   182,    15,    55,    24,   101,   102,   338,   104,    90,
      90,   310,     5,    59,   110,   111,   437,   490,    93,    66,
      14,   320,    93,    93,   318,    14,   502,    79,   397,   454,
     183,   397,   329,   214,   365,   420,   432,   331,   332,   333,
     211,   343,   512,   337,   383,   226,   336,   341,   476,   101,
     497,   345,   104,   316,   191,   349,   477,   150,   110,   111,
     355,   355,   345,   244,   366,   280,   243,    -1,    -1,    -1,
      41,    42,    -1,   254,   255,    -1,   358,    -1,    -1,   373,
      -1,    -1,    23,    -1,    55,   379,    -1,   381,    -1,    -1,
      -1,    -1,   391,    -1,   393,    -1,   397,   397,    -1,   401,
      -1,    42,    -1,    -1,    -1,    -1,   388,    -1,    79,    80,
      81,    -1,    83,    -1,    55,   397,    42,    -1,    -1,    -1,
     414,   415,    -1,   454,    -1,   456,    -1,    -1,    -1,    55,
     101,   102,    -1,   104,    -1,   316,    -1,   468,    79,   110,
     111,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   451,
      -1,   453,    -1,    79,   335,   336,    -1,    83,   452,    -1,
     101,   102,   444,   104,    -1,    -1,   107,   108,   109,   110,
     111,    -1,    -1,    -1,    42,   101,   102,    -1,   104,    -1,
      -1,    -1,    -1,    -1,   110,   111,    -1,    55,    -1,    -1,
      -1,    -1,    -1,   494,   496,    -1,   490,    -1,    -1,    -1,
      -1,    -1,    -1,   497,    -1,    -1,    -1,   509,     6,    -1,
       8,    79,    10,    -1,    -1,    83,    -1,    -1,    -1,    -1,
      18,    -1,   403,    21,    -1,    -1,    -1,    25,    -1,    -1,
      -1,    -1,    -1,   101,   102,    -1,   104,   418,    -1,    -1,
      -1,   422,   110,   111,    -1,    43,    -1,    -1,    -1,    -1,
      -1,    -1,   433,    -1,    52,    -1,   437,    -1,    56,   440,
     441,    59,    -1,    -1,    62,    63,    64,    -1,    66,    -1,
      -1,    69,    70,     6,    -1,     8,    -1,    10,    76,    77,
      -1,    -1,    -1,    -1,    -1,    18,    -1,    85,    21,    -1,
      -1,    -1,    25,    -1,    -1,    -1,   477,    95,    -1,    97,
      98,    99,    -1,   101,    -1,    -1,   104,    -1,    -1,    -1,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,    52,
      -1,    -1,    -1,    56,    -1,    -1,    59,    -1,    -1,    62,
      -1,    64,    -1,    66,    -1,    -1,    69,    70,    -1,    -1,
      -1,    -1,    -1,    76,    77,    -1,    -1,    -1,     6,    -1,
       8,    -1,    10,    -1,    -1,    -1,    -1,    90,    -1,    -1,
      18,    -1,    95,    21,    97,    98,    99,    25,   101,    -1,
      -1,   104,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   116,    -1,    43,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    52,    -1,    -1,    -1,    56,    -1,
      -1,    59,    -1,    -1,    62,    -1,    64,    -1,    66,    -1,
      -1,    69,    70,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    -1,    -1,     6,    -1,     8,    -1,    10,    -1,    -1,
      -1,    -1,    90,    -1,    -1,    18,    -1,    95,    21,    97,
      98,    99,    25,   101,    -1,    -1,   104,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   116,    -1,
      43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,
      -1,    -1,    -1,    56,    -1,    -1,    59,    -1,    -1,    62,
      -1,    64,    -1,    66,    -1,    -1,    69,    70,    -1,    -1,
      -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    95,    -1,    97,    98,    99,    -1,   101,    -1,
       6,   104,     8,    -1,    10,    -1,    12,    13,    -1,    -1,
      -1,    17,    18,   116,    20,    21,    -1,    -1,    -1,    25,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    41,    42,    43,    44,    45,
      46,    -1,    -1,    -1,    -1,    51,    52,    53,    -1,    55,
      56,    57,    -1,    59,    -1,    -1,    62,    -1,    64,    -1,
      66,    -1,    -1,    69,    70,    -1,    -1,    -1,    -1,    -1,
      76,    77,    -1,    79,    80,    81,    -1,    -1,    -1,    -1,
      -1,    87,    -1,    -1,    -1,    -1,    92,    93,    -1,    95,
      -1,    97,    98,    99,    -1,   101,   102,    -1,   104,   105,
     106,    -1,    -1,    -1,   110,   111,   112,   113,   114,     6,
      -1,     8,    -1,    10,    -1,    12,    -1,    -1,    -1,    -1,
      17,    18,    -1,    20,    21,    -1,    -1,    -1,    25,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    41,    42,    43,    44,    45,    46,
      -1,    -1,    -1,    -1,    51,    52,    53,    -1,    55,    56,
      57,    -1,    59,    -1,    -1,    62,    -1,    64,    -1,    66,
      -1,    -1,    69,    70,    -1,    -1,    -1,    -1,    -1,    76,
      77,    -1,    79,    80,    81,    -1,    83,    -1,    -1,    -1,
      87,    -1,    -1,    -1,    -1,    92,    93,    -1,    95,    -1,
      97,    98,    99,    -1,   101,   102,    -1,   104,    -1,   106,
      -1,    -1,    -1,   110,   111,   112,   113,   114,     6,    -1,
       8,    -1,    10,    -1,    12,    -1,    -1,    -1,    -1,    17,
      18,    -1,    20,    21,    -1,    -1,    -1,    25,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    41,    42,    43,    44,    45,    46,    -1,
      -1,    -1,    -1,    51,    52,    53,    -1,    55,    56,    57,
      -1,    59,    -1,    -1,    62,    -1,    64,    -1,    66,    -1,
      -1,    69,    70,    -1,    -1,    -1,    -1,    -1,    76,    77,
      -1,    79,    80,    81,    -1,    83,    -1,    -1,    -1,    87,
      -1,    -1,    -1,    -1,    92,    93,    -1,    95,    -1,    97,
      98,    99,    -1,   101,   102,    -1,   104,    -1,   106,    -1,
      -1,    -1,   110,   111,   112,   113,   114,     6,    -1,     8,
      -1,    10,    -1,    12,    -1,    -1,    -1,    -1,    17,    18,
      -1,    20,    21,    -1,    -1,    -1,    25,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    43,    44,    45,    46,    -1,    -1,
      -1,    -1,    51,    52,    53,    -1,    -1,    56,    57,    -1,
      59,    -1,    -1,    62,    -1,    64,    -1,    66,    -1,    -1,
      69,    70,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,
      -1,    -1,    -1,    -1,    83,    -1,    -1,    -1,    87,    -1,
      -1,    -1,    -1,    92,    93,    -1,    95,    -1,    97,    98,
      99,    -1,   101,    -1,    -1,   104,     6,   106,     8,    -1,
      10,    -1,    12,   112,   113,   114,    -1,    17,    18,    -1,
      20,    21,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    43,    44,    45,    46,    -1,    -1,    -1,
      -1,    51,    52,    53,    -1,    -1,    56,    57,    -1,    59,
      -1,    -1,    62,    -1,    64,    -1,    66,    -1,    -1,    69,
      70,    -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,
      -1,    -1,    -1,    83,    -1,    -1,    -1,    87,    -1,    -1,
      -1,    -1,    92,    93,    -1,    95,    -1,    97,    98,    99,
      -1,   101,    -1,    -1,   104,     6,   106,     8,    -1,    10,
      -1,    12,   112,   113,   114,    -1,    17,    18,    -1,    20,
      21,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    43,    44,    45,    46,    -1,    -1,    -1,    -1,
      51,    52,    53,    -1,    -1,    56,    57,    -1,    59,    -1,
      -1,    62,    -1,    64,    -1,    66,    -1,    -1,    69,    70,
      -1,    -1,    -1,    -1,    -1,    76,    77,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    87,    -1,    -1,    -1,
      -1,    92,    93,    -1,    95,    -1,    97,    98,    99,    -1,
     101,    -1,    -1,   104,     6,   106,     8,    -1,    10,    -1,
      12,   112,   113,   114,    -1,    17,    18,    -1,    20,    21,
      -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    43,    44,    45,    46,    -1,    -1,    -1,    -1,    -1,
      52,    53,    -1,    -1,    56,    -1,    -1,    59,    -1,    -1,
      62,    -1,    64,    -1,    66,    -1,    -1,    69,    70,     6,
      -1,     8,    -1,    10,    76,    77,    -1,    -1,    -1,    -1,
      -1,    18,    -1,    -1,    21,    87,    -1,    -1,    25,    -1,
      92,    93,    -1,    95,    -1,    97,    98,    99,    -1,   101,
      -1,    -1,   104,    40,   106,    42,    43,    -1,    -1,    -1,
     112,    -1,   114,    -1,    -1,    52,    -1,    -1,    55,    56,
      -1,    -1,    59,    -1,    -1,    62,     6,    64,     8,    66,
      10,    -1,    69,    70,    -1,    -1,    -1,    -1,    18,    76,
      77,    21,    79,    -1,    -1,    25,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    95,    -1,
      97,    98,    99,    43,   101,   102,    -1,   104,    -1,    -1,
      -1,    -1,    52,   110,   111,    -1,    56,    -1,    -1,    59,
      -1,    -1,    62,    63,    64,    -1,    66,    -1,    -1,    69,
      70,     6,    -1,     8,    -1,    10,    76,    77,    -1,    -1,
      -1,    -1,    -1,    18,    -1,    85,    21,    -1,    -1,    -1,
      25,    -1,    -1,    -1,    -1,    95,    -1,    97,    98,    99,
      21,   101,    -1,    -1,   104,    -1,    -1,    -1,    43,    -1,
      -1,    -1,    -1,    48,    -1,    -1,    -1,    52,    -1,    -1,
      -1,    56,    -1,    -1,    59,    -1,    -1,    62,     6,    64,
       8,    66,    10,    -1,    69,    70,    -1,    -1,    59,    60,
      18,    76,    77,    21,    -1,    -1,    67,    25,    69,    70,
      -1,    -1,    -1,    -1,    75,    76,    77,    78,    -1,    -1,
      95,    -1,    97,    98,    99,    43,   101,    -1,    -1,   104,
      91,    -1,    -1,    94,    52,    96,    -1,    -1,    56,   100,
      -1,    59,    -1,    -1,    62,     6,    64,     8,    66,    10,
      -1,    69,    70,    -1,    -1,    -1,    -1,    18,    76,    77,
      21,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    93,    -1,    95,    -1,    97,
      98,    99,    43,   101,    -1,    -1,   104,    -1,    -1,    -1,
      -1,    52,    -1,    -1,    -1,    56,    -1,    -1,    59,    -1,
      -1,    62,     6,    64,     8,    66,    10,    -1,    69,    70,
      -1,    -1,    -1,    -1,    18,    76,    77,    21,    -1,    -1,
      -1,    25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    93,    -1,    95,    -1,    97,    98,    99,    43,
     101,    -1,    -1,   104,    -1,    -1,    -1,    -1,    52,    -1,
      -1,    -1,    56,    -1,    -1,    59,    -1,    -1,    62,     6,
      64,     8,    66,    10,    -1,    69,    70,    -1,    -1,    -1,
      -1,    18,    76,    77,    21,    -1,    -1,    -1,    25,    -1,
      84,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    95,    -1,    97,    98,    99,    43,   101,    -1,    -1,
     104,    -1,    -1,    -1,    -1,    52,    -1,    -1,    -1,    56,
      -1,    -1,    59,    -1,    -1,    62,     6,    64,     8,    66,
      10,    -1,    69,    70,    -1,    -1,    -1,    -1,    18,    76,
      77,    21,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    90,    -1,    -1,    -1,    -1,    95,    -1,
      97,    98,    99,    43,   101,    -1,    -1,   104,    -1,    -1,
      -1,    -1,    52,    -1,    -1,    -1,    56,    -1,    -1,    59,
      -1,    -1,    62,     6,    64,    -1,    66,    10,    -1,    69,
      70,    -1,    -1,    -1,    -1,    18,    76,    77,    21,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    95,    -1,    97,    98,    99,
      43,   101,    -1,    -1,   104,    -1,    -1,    -1,    -1,    52,
      -1,    -1,    -1,    56,    -1,    -1,    59,    -1,    -1,    62,
      -1,    -1,    -1,    66,    -1,    -1,    69,    70,    -1,    -1,
      -1,    -1,    -1,    76,    77,    -1,    -1,    -1,    -1,    -1,
      -1,     3,    -1,    -1,    -1,     7,     8,     9,    -1,    11,
      -1,    -1,    95,    15,    97,    98,    99,    -1,   101,    21,
      22,   104,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    73,    -1,    75,    76,    77,    78,    -1,    -1,     3,
      82,    -1,    84,     7,     8,     9,    -1,    11,    -1,    91,
      -1,    15,    94,    -1,    96,    -1,    -1,    21,   100,    -1,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    73,
      -1,    75,    76,    77,    78,    -1,     3,    -1,    82,    -1,
       7,     8,     9,    -1,    11,    -1,    -1,    91,    15,    93,
      94,    -1,    96,    -1,    21,    -1,   100,    -1,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    58,    59,    60,    61,    -1,    -1,    64,    65,    -1,
      67,    68,    69,    70,    71,    72,    73,    -1,    75,    76,
      77,    78,    -1,     3,    -1,    82,    -1,     7,     8,     9,
      -1,    11,    -1,    -1,    91,    15,    -1,    94,    -1,    96,
      -1,    21,    -1,   100,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,
      60,    61,    -1,    -1,    64,    65,    -1,    67,    68,    69,
      70,    71,    72,    73,    -1,    75,    76,    77,    78,    -1,
      -1,     3,    82,    -1,    -1,     7,     8,     9,    88,    11,
      -1,    91,    -1,    15,    94,    -1,    96,    -1,    -1,    21,
     100,    -1,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    73,    -1,    75,    76,    77,    78,    -1,     3,    -1,
      82,    -1,     7,     8,     9,    -1,    11,    -1,    90,    91,
      15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,
      65,    -1,    67,    68,    69,    70,    71,    72,    73,    -1,
      75,    76,    77,    78,    -1,     3,    -1,    82,    -1,     7,
       8,     9,    -1,    11,    -1,    -1,    91,    15,    93,    94,
      -1,    96,    -1,    21,    -1,   100,    -1,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,
      48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,    67,
      68,    69,    70,    71,    72,    73,    -1,    75,    76,    77,
      78,    -1,     3,    -1,    82,    -1,     7,     8,     9,    -1,
      11,    -1,    90,    91,    15,    -1,    94,    -1,    96,    -1,
      21,    -1,   100,    -1,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,
      61,    -1,    -1,    64,    65,    -1,    67,    68,    69,    70,
      71,    72,    73,    -1,    75,    76,    77,    78,    -1,     3,
      -1,    82,    -1,     7,     8,     9,    -1,    11,    -1,    -1,
      91,    15,    93,    94,    -1,    96,    -1,    21,    -1,   100,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    73,
      -1,    75,    76,    77,    78,     3,     4,    -1,    82,     7,
       8,     9,    -1,    11,    -1,    -1,    90,    91,    -1,    -1,
      94,    -1,    96,    21,    -1,    -1,   100,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,
      48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,    67,
      68,    69,    70,    71,    72,    73,    -1,    75,    76,    77,
      78,    -1,     3,    -1,    82,    -1,     7,     8,     9,    -1,
      11,    -1,    -1,    91,    15,    93,    94,    -1,    96,    -1,
      21,    -1,   100,    -1,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,
      61,    -1,    -1,    64,    65,    -1,    67,    68,    69,    70,
      71,    72,    73,    -1,    75,    76,    77,    78,    -1,     3,
      -1,    82,    -1,     7,     8,     9,    -1,    11,    -1,    90,
      91,    15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    73,
      -1,    75,    76,    77,    78,    -1,     3,    -1,    82,    -1,
       7,     8,     9,    -1,    11,    -1,    90,    91,    15,    -1,
      94,    -1,    96,    -1,    21,    -1,   100,    -1,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,
      67,    68,    69,    70,    71,    72,    73,    -1,    75,    76,
      77,    78,    -1,     3,    -1,    82,    -1,     7,     8,     9,
      -1,    11,    -1,    90,    91,    15,    -1,    94,    -1,    96,
      -1,    21,    -1,   100,    -1,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,
      60,    61,    -1,    -1,    64,    65,    -1,    67,    68,    69,
      70,    71,    72,    73,    -1,    75,    76,    77,    78,    -1,
      -1,     3,    82,    83,    -1,     7,     8,     9,    -1,    11,
      -1,    91,    14,    15,    94,    -1,    96,    -1,    -1,    21,
     100,    -1,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    73,    -1,    75,    76,    77,    78,    -1,     3,    -1,
      82,    -1,     7,     8,     9,    -1,    11,    -1,    -1,    91,
      15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,
      65,    -1,    67,    68,    69,    70,    71,    72,    73,    -1,
      75,    76,    77,    78,    -1,    -1,     3,    82,    -1,    84,
       7,     8,     9,    -1,    11,    -1,    91,    -1,    15,    94,
      -1,    96,    -1,    -1,    21,   100,    -1,    -1,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,
      67,    68,    69,    70,    71,    72,    73,    -1,    75,    76,
      77,    78,    -1,    -1,     3,    82,    -1,    -1,     7,     8,
       9,    88,    11,    -1,    91,    -1,    15,    94,    -1,    96,
      -1,    -1,    21,   100,    -1,    -1,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,
      49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      59,    60,    61,    -1,    -1,    64,    65,    -1,    67,    68,
      69,    70,    71,    72,    73,    -1,    75,    76,    77,    78,
      -1,     3,    -1,    82,    -1,     7,     8,     9,    -1,    11,
      89,    -1,    91,    15,    -1,    94,    -1,    96,    -1,    21,
      -1,   100,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    73,    -1,    75,    76,    77,    78,    -1,     3,    -1,
      82,    -1,     7,     8,     9,    -1,    11,    -1,    90,    91,
      15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,
      65,    -1,    67,    68,    69,    70,    71,    72,    73,    -1,
      75,    76,    77,    78,    -1,     3,    -1,    82,    -1,     7,
       8,     9,    -1,    11,    -1,    90,    91,    15,    -1,    94,
      -1,    96,    -1,    21,    -1,   100,    -1,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,
      48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,    67,
      68,    69,    70,    71,    72,    73,    -1,    75,    76,    77,
      78,    -1,     3,    -1,    82,    -1,     7,     8,     9,    -1,
      11,    -1,    90,    91,    15,    -1,    94,    -1,    96,    -1,
      21,    -1,   100,    -1,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,
      61,    -1,    -1,    64,    65,    -1,    67,    68,    69,    70,
      71,    72,    73,    -1,    75,    76,    77,    78,    -1,     3,
      -1,    82,    -1,     7,     8,     9,    -1,    11,    -1,    90,
      91,    15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,
      -1,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    73,
      -1,    75,    76,    77,    78,    -1,     3,    -1,    82,    -1,
       7,     8,     9,    -1,    11,    89,    -1,    91,    15,    -1,
      94,    -1,    96,    -1,    21,    -1,   100,    -1,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,
      67,    68,    69,    70,    71,    72,    73,    -1,    75,    76,
      77,    78,    -1,    -1,     3,    82,    -1,    84,     7,     8,
       9,    -1,    11,    -1,    91,    -1,    15,    94,    -1,    96,
      -1,    -1,    21,   100,    -1,    -1,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,
      49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      59,    60,    61,    -1,    -1,    64,    65,    -1,    67,    68,
      69,    70,    71,    72,    73,    -1,    75,    76,    77,    78,
      -1,     3,    -1,    82,    -1,     7,     8,     9,    -1,    11,
      -1,    90,    91,    15,    -1,    94,    -1,    96,    -1,    21,
      -1,   100,    -1,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    73,    -1,    75,    76,    77,    78,    -1,     3,    -1,
      82,    -1,     7,     8,     9,    -1,    11,    -1,    -1,    91,
      15,    -1,    94,    -1,    96,    -1,    21,    -1,   100,    -1,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,
      65,    -1,    67,    68,    69,    70,    71,    72,    73,    -1,
      75,    76,    77,    78,     3,     4,    -1,    82,     7,     8,
       9,    -1,    11,    -1,    -1,    -1,    91,    -1,    -1,    94,
      -1,    96,    21,    -1,    -1,   100,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,
      49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      59,    60,    61,    -1,    -1,    64,    65,    -1,    67,    68,
      69,    70,    71,    72,    73,    -1,    75,    76,    77,    78,
       3,    -1,    -1,    82,     7,     8,     9,    -1,    11,    -1,
      -1,    -1,    91,    -1,    -1,    94,    -1,    96,    21,    -1,
      -1,   100,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,
      -1,    64,    65,    -1,    67,    68,    69,    70,    71,    72,
      73,    -1,    75,    76,    77,    78,    -1,     3,    -1,    82,
      -1,     7,     8,     9,    -1,    11,    -1,    90,    91,    15,
      -1,    94,    -1,    96,    -1,    21,    -1,   100,    -1,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,
      -1,    67,    68,    69,    70,    71,    72,    73,    -1,    75,
      76,    77,    78,     3,    -1,    -1,    82,     7,     8,     9,
      -1,    11,    -1,    -1,    -1,    91,    -1,    -1,    94,    -1,
      96,    21,    -1,    -1,   100,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,    48,    49,
      50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,
      60,    61,    -1,    -1,    64,    65,    -1,    67,    68,    69,
      70,    71,    72,    73,    -1,    75,    76,    77,    78,     3,
      -1,    -1,    82,     7,     8,     9,    -1,    11,    -1,    -1,
      -1,    91,    -1,    -1,    94,    -1,    96,    21,    -1,    -1,
     100,    25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    -1,    39,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    73,
      -1,    75,    76,    77,    78,     3,    -1,    -1,    82,     7,
       8,     9,    -1,    11,    -1,    -1,    -1,    91,    -1,    -1,
      94,    -1,    96,    21,    -1,    -1,   100,    25,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    37,
      -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    47,
      48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,    67,
      68,    69,    70,    71,    72,    -1,    -1,    75,    76,    77,
      78,     7,     8,     9,    -1,    11,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    91,    -1,    21,    94,    -1,    96,    25,
      -1,    -1,   100,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    37,    -1,    39,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,
      -1,    67,    68,    69,    70,    71,    72,    -1,    -1,    75,
      76,    77,    78,     7,     8,    -1,    -1,    11,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    91,    -1,    21,    94,    -1,
      96,    25,    -1,    -1,   100,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    37,    -1,    39,    -1,    -1,    -1,    41,
      42,    -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    -1,    59,    60,    61,    -1,    -1,
      64,    65,    -1,    67,    68,    69,    70,    71,    72,    -1,
      -1,    75,    76,    77,    78,     7,     8,    79,    80,    81,
      -1,    83,    -1,    -1,    -1,    -1,    -1,    91,    -1,    21,
      94,    -1,    96,    25,    -1,    -1,   100,    -1,    -1,   101,
     102,    -1,   104,    -1,    -1,    37,    -1,    39,   110,   111,
      -1,    -1,    -1,    -1,    -1,    47,    48,    49,    50,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    59,    60,    61,
      -1,    -1,    64,    65,    -1,    67,    68,    69,    70,    71,
      72,    -1,     7,    75,    76,    77,    78,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    21,    -1,    -1,    91,
      25,    -1,    94,    -1,    96,    -1,    -1,    -1,   100,    -1,
      -1,    -1,    37,    -1,    39,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    47,    48,    49,    50,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    59,    60,    61,    -1,    -1,    64,
      65,    -1,    67,    68,    69,    70,    71,    72,    -1,    -1,
      75,    76,    77,    78,    21,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    91,    -1,    -1,    94,
      -1,    96,    -1,    -1,    -1,   100,    -1,    -1,    -1,    -1,
      47,    48,    49,    50,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    59,    60,    61,    -1,    -1,    64,    65,    -1,
      67,    68,    69,    70,    -1,    -1,    -1,    -1,    75,    76,
      77,    78,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    91,    -1,    -1,    94,    -1,    96,
      -1,    -1,    -1,   100
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,   124,   125,     0,     6,     8,    10,    12,    13,    17,
      18,    20,    21,    25,    41,    42,    43,    44,    45,    46,
      51,    52,    53,    55,    56,    57,    59,    62,    64,    66,
      69,    70,    76,    77,    79,    80,    81,    87,    92,    93,
      95,    97,    98,    99,   101,   102,   104,   105,   106,   110,
     111,   112,   113,   114,   126,   132,   135,   137,   139,   140,
     141,   142,   147,   148,   158,   161,   163,   166,   167,   168,
     175,   176,   177,   183,   184,   185,   189,   191,   192,    52,
      59,   101,   104,   158,   158,   158,    93,   104,   161,    93,
      66,   135,    52,    66,    66,    66,    52,    14,    66,   158,
      83,   164,   158,    48,   158,    40,   158,   175,   158,   158,
     158,   158,    93,   158,    59,   163,    66,    98,    66,    52,
      59,    21,    78,   101,   104,   175,    66,    66,   136,   161,
      66,     3,     7,     8,     9,    11,    15,    21,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    47,    48,    49,    50,    59,    60,    61,
      64,    65,    67,    68,    69,    70,    71,    72,    73,    75,
      76,    77,    78,    82,    91,    93,    94,    96,   100,    66,
     168,   175,     8,    74,    93,   104,   133,   161,   169,   171,
     173,    59,    60,   174,   174,    97,    99,    62,    98,    99,
      66,    21,   164,    59,    93,    59,    93,   158,   112,   158,
      93,   149,   158,   161,   162,    93,   135,   158,    58,    83,
     135,   151,   158,   165,   166,   167,   175,   181,   182,    88,
      48,    90,    90,    90,    93,   164,    52,   159,    52,   116,
     156,   157,   158,    59,   175,   178,   179,   180,    52,    52,
     161,   173,   158,   158,    15,    38,   158,   158,    63,    85,
     186,   187,   190,   158,   158,   158,   158,    52,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     186,   187,   158,   158,   158,   158,   158,   158,   158,   158,
     158,   158,   158,   158,   158,   158,   158,   158,   158,   158,
     158,    52,   158,   158,   158,   158,   158,    90,   156,   133,
     161,    66,   134,   174,   134,   174,    15,    93,    38,   176,
      84,   158,    98,   158,    90,   156,    52,   158,   128,   127,
      90,    66,    15,   149,    93,     4,    15,   161,    90,    83,
     135,     4,    83,   151,   166,    15,    83,   158,   158,    66,
     163,   156,   187,    14,    14,    15,    90,   158,   178,   170,
     173,    83,   179,    93,    93,    90,    90,   161,    56,   161,
      90,   158,    86,    63,    85,   103,   188,   190,    83,    22,
      84,    14,    90,    66,   174,   152,   153,   154,   155,   171,
     158,    83,   174,    84,    88,    90,   129,   130,   129,   158,
     158,    90,   158,   161,   161,   162,   158,   140,   163,   158,
      83,   182,   158,    90,    15,   157,   158,    83,    15,   140,
     163,   135,    38,    59,    89,   158,    86,   103,   158,   158,
     152,    90,    15,    23,   107,   108,   109,   175,   174,   174,
      16,    19,    54,    83,   105,   132,   137,   166,    90,    90,
     135,    90,   161,    90,    24,   146,    90,   156,   158,   173,
     146,    56,   161,   143,    89,    84,    90,     5,   138,   154,
     161,   172,   173,   133,   161,   133,    59,   175,    93,   135,
     158,   135,   141,   163,   163,    90,    83,   115,   116,   144,
      66,    93,   163,    83,   165,   173,    90,   160,    14,   156,
      93,   131,    83,    93,   135,   145,   158,   187,   150,   151,
      90,   131,    14,   150
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, Location); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (yylocationp);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, yylocationp)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    YYLTYPE const * const yylocationp;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yylsp, yyrule)
    YYSTYPE *yyvsp;
    YYLTYPE *yylsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       , &(yylsp[(yyi + 1) - (yynrhs)])		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, yylsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, yylocationp)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    YYLTYPE *yylocationp;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;
/* Location data for the look-ahead symbol.  */
YYLTYPE yylloc;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;

  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[2];

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;
#if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  /* Initialize the default location before parsing starts.  */
  yylloc.first_line   = yylloc.last_line   = 1;
  yylloc.first_column = yylloc.last_column = 0;
#endif

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;
	YYLTYPE *yyls1 = yyls;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
	YYSTACK_RELOCATE (yyls);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;
  *++yylsp = yylloc;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 215 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(TopLev, next, (yyvsp[(1) - (1)].TopLev));
		L->ast = (yyvsp[(1) - (1)].TopLev);
	;}
    break;

  case 3:
#line 223 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(2) - (2)].ClsDecl)) {
			(yyval.TopLev) = ast_mkTopLevel(L_TOPLEVEL_CLASS, (yyvsp[(1) - (2)].TopLev), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
			(yyval.TopLev)->u.class = (yyvsp[(2) - (2)].ClsDecl);
		} else {
			// Don't create a node for a forward class declaration.
			(yyval.TopLev) = (yyvsp[(1) - (2)].TopLev);
		}
	;}
    break;

  case 4:
#line 233 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.TopLev) = ast_mkTopLevel(L_TOPLEVEL_FUN, (yyvsp[(1) - (2)].TopLev), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyvsp[(2) - (2)].FnDecl)->decl->flags |= DECL_FN;
		if ((yyvsp[(2) - (2)].FnDecl)->decl->flags & DECL_PRIVATE) {
			(yyvsp[(2) - (2)].FnDecl)->decl->flags |= SCOPE_SCRIPT;
		} else {
			(yyvsp[(2) - (2)].FnDecl)->decl->flags |= SCOPE_GLOBAL;
		}
		(yyval.TopLev)->u.fun = (yyvsp[(2) - (2)].FnDecl);
	;}
    break;

  case 5:
#line 244 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		L_set_declBaseType((yyvsp[(4) - (5)].VarDecl), (yyvsp[(3) - (5)].Type));
		L_typedef_store((yyvsp[(4) - (5)].VarDecl));
		(yyval.TopLev) = (yyvsp[(1) - (5)].TopLev);  // nothing more to do
	;}
    break;

  case 6:
#line 250 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// Global variable declaration.
		VarDecl *v;
		(yyval.TopLev) = ast_mkTopLevel(L_TOPLEVEL_GLOBAL, (yyvsp[(1) - (2)].TopLev), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		for (v = (yyvsp[(2) - (2)].VarDecl); v; v = v->next) {
			v->flags |= DECL_GLOBAL_VAR;
			if ((yyvsp[(2) - (2)].VarDecl)->flags & DECL_PRIVATE) {
				v->flags |= SCOPE_SCRIPT;
			} else {
				v->flags |= SCOPE_GLOBAL;
			}
		}
		(yyval.TopLev)->u.global = (yyvsp[(2) - (2)].VarDecl);
	;}
    break;

  case 7:
#line 265 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// Top-level statement.
		(yyval.TopLev) = ast_mkTopLevel(L_TOPLEVEL_STMT, (yyvsp[(1) - (2)].TopLev), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyval.TopLev)->u.stmt = (yyvsp[(2) - (2)].Stmt);
	;}
    break;

  case 8:
#line 270 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.TopLev) = NULL; ;}
    break;

  case 9:
#line 275 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/*
		 * This is a new class declaration.
		 * Alloc the VarDecl now and associate it with
		 * the class name so that it is available while
		 * parsing the class body.
		 */
		Type	*t = type_mkClass();
		VarDecl	*d = ast_mkVarDecl(t, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(1) - (3)]));
		ClsDecl	*c = ast_mkClsDecl(d, (yylsp[(1) - (3)]), (yylsp[(1) - (3)]));
		t->u.class.clsdecl = c;
		ASSERT(!L_typedef_lookup((yyvsp[(2) - (3)].Expr)->str));
		L_typedef_store(d);
		(yyval.ClsDecl) = c;
	;}
    break;

  case 10:
#line 290 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.ClsDecl) = (yyvsp[(5) - (5)].ClsDecl);
		/* silence unused warning */
		(void)(yyvsp[(4) - (5)].ClsDecl);
	;}
    break;

  case 11:
#line 296 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/*
		 * This is a class declaration where the type name was
		 * previously declared.  Use the ClsDecl from the
		 * prior decl.
		 */
		ClsDecl	*c = (yyvsp[(2) - (3)].Typename).t->u.class.clsdecl;
		unless (c->decl->flags & DECL_FORWARD) {
			L_err("redeclaration of %s", (yyvsp[(2) - (3)].Typename).s);
		}
		ASSERT(isclasstype(c->decl->type));
		c->decl->flags &= ~DECL_FORWARD;
		(yyval.ClsDecl) = c;
	;}
    break;

  case 12:
#line 310 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.ClsDecl) = (yyvsp[(5) - (5)].ClsDecl);
		/* silence unused warning */
		(void)(yyvsp[(4) - (5)].ClsDecl);
	;}
    break;

  case 13:
#line 316 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/* This is a forward class declaration. */
		Type	*t = type_mkClass();
		VarDecl	*d = ast_mkVarDecl(t, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		ClsDecl	*c = ast_mkClsDecl(d, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		ASSERT(!L_typedef_lookup((yyvsp[(2) - (3)].Expr)->str));
		t->u.class.clsdecl = c;
		d->flags |= DECL_FORWARD;
		L_typedef_store(d);
		(yyval.ClsDecl) = NULL;
	;}
    break;

  case 14:
#line 328 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/* Empty declaration of an already declared type. */
		unless (isclasstype((yyvsp[(2) - (3)].Typename).t)) {
			L_err("%s not a class type", (yyvsp[(2) - (3)].Typename).s);
		}
		(yyval.ClsDecl) = NULL;
	;}
    break;

  case 15:
#line 339 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.ClsDecl) = (yyvsp[(0) - (2)].ClsDecl);
		(yyval.ClsDecl)->node.loc.end       = (yylsp[(2) - (2)]).end;
		(yyval.ClsDecl)->decl->node.loc.end = (yylsp[(2) - (2)]).end;
		/* If constructor or destructor were omitted, make defaults. */
		unless ((yyval.ClsDecl)->constructors) {
			(yyval.ClsDecl)->constructors = ast_mkConstructor((yyval.ClsDecl));
		}
		unless ((yyval.ClsDecl)->destructors) {
			(yyval.ClsDecl)->destructors = ast_mkDestructor((yyval.ClsDecl));
		}
	;}
    break;

  case 16:
#line 355 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl	*v;
		ClsDecl	*clsdecl = (yyvsp[(0) - (6)].ClsDecl);
		REVERSE(VarDecl, next, (yyvsp[(4) - (6)].VarDecl));
		for (v = (yyvsp[(4) - (6)].VarDecl); v; v = v->next) {
			v->clsdecl = clsdecl;
			v->flags  |= SCOPE_CLASS | DECL_CLASS_INST_VAR;
			unless (v->flags & (DECL_PUBLIC | DECL_PRIVATE)) {
				L_errf(v, "class instance variable %s not "
				       "declared public or private",
				       v->id->str);
				v->flags |= DECL_PUBLIC;
			}
		}
		APPEND_OR_SET(VarDecl, next, clsdecl->instvars, (yyvsp[(4) - (6)].VarDecl));
	;}
    break;

  case 18:
#line 373 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl	*v;
		ClsDecl	*clsdecl = (yyvsp[(0) - (2)].ClsDecl);
		REVERSE(VarDecl, next, (yyvsp[(2) - (2)].VarDecl));
		for (v = (yyvsp[(2) - (2)].VarDecl); v; v = v->next) {
			v->clsdecl = clsdecl;
			v->flags  |= SCOPE_CLASS | DECL_CLASS_VAR;
			unless (v->flags & (DECL_PUBLIC | DECL_PRIVATE)) {
				L_errf(v, "class variable %s not "
				       "declared public or private",
				       v->id->str);
				v->flags |= DECL_PUBLIC;
			}
		}
		APPEND_OR_SET(VarDecl, next, clsdecl->clsvars, (yyvsp[(2) - (2)].VarDecl));
	;}
    break;

  case 19:
#line 390 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		L_set_declBaseType((yyvsp[(4) - (5)].VarDecl), (yyvsp[(3) - (5)].Type));
		L_typedef_store((yyvsp[(4) - (5)].VarDecl));
	;}
    break;

  case 20:
#line 395 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		ClsDecl	*clsdecl = (yyvsp[(0) - (2)].ClsDecl);
		(yyvsp[(2) - (2)].FnDecl)->decl->clsdecl = clsdecl;
		(yyvsp[(2) - (2)].FnDecl)->decl->flags  |= DECL_CLASS_FN;
		unless ((yyvsp[(2) - (2)].FnDecl)->decl->flags & DECL_PRIVATE) {
			(yyvsp[(2) - (2)].FnDecl)->decl->flags |= SCOPE_GLOBAL | DECL_PUBLIC;
		} else {
			(yyvsp[(2) - (2)].FnDecl)->decl->flags |= SCOPE_CLASS;
			(yyvsp[(2) - (2)].FnDecl)->decl->tclprefix = cksprintf("_L_class_%s_",
						clsdecl->decl->id->str);
		}
		APPEND_OR_SET(FnDecl, next, clsdecl->fns, (yyvsp[(2) - (2)].FnDecl));
	;}
    break;

  case 21:
#line 409 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		ClsDecl	*clsdecl = (yyvsp[(0) - (3)].ClsDecl);
		(yyvsp[(3) - (3)].FnDecl)->decl->type->base_type = clsdecl->decl->type;
		(yyvsp[(3) - (3)].FnDecl)->decl->clsdecl = clsdecl;
		(yyvsp[(3) - (3)].FnDecl)->decl->flags  |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
			DECL_CLASS_CONST;
		APPEND_OR_SET(FnDecl, next, clsdecl->constructors, (yyvsp[(3) - (3)].FnDecl));
	;}
    break;

  case 22:
#line 418 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		ClsDecl	*clsdecl = (yyvsp[(0) - (3)].ClsDecl);
		(yyvsp[(3) - (3)].FnDecl)->decl->type->base_type = L_void;
		(yyvsp[(3) - (3)].FnDecl)->decl->clsdecl = clsdecl;
		(yyvsp[(3) - (3)].FnDecl)->decl->flags  |= SCOPE_GLOBAL | DECL_CLASS_FN | DECL_PUBLIC |
			DECL_CLASS_DESTR;
		APPEND_OR_SET(FnDecl, next, clsdecl->destructors, (yyvsp[(3) - (3)].FnDecl));
	;}
    break;

  case 23:
#line 427 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/*
		 * We don't store the things that make up class_code
		 * in order, so there's no place in which to
		 * interleave #pragmas.  So don't create an AST node,
		 * just update L->options now; it gets used when other
		 * AST nodes are created.
		 */
		L_compile_attributes(L->options, (yyvsp[(2) - (2)].Expr), L_attrs_pragma);
	;}
    break;

  case 27:
#line 447 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(2) - (2)].FnDecl)->decl->type->base_type = (yyvsp[(1) - (2)].Type);
		(yyval.FnDecl) = (yyvsp[(2) - (2)].FnDecl);
		(yyval.FnDecl)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 28:
#line 453 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].FnDecl)->decl->type->base_type = (yyvsp[(2) - (3)].Type);
		(yyvsp[(3) - (3)].FnDecl)->decl->flags |= (yyvsp[(1) - (3)].i);
		(yyval.FnDecl) = (yyvsp[(3) - (3)].FnDecl);
		(yyval.FnDecl)->node.loc = (yylsp[(1) - (3)]);
	;}
    break;

  case 29:
#line 463 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.FnDecl) = (yyvsp[(2) - (2)].FnDecl);
		(yyval.FnDecl)->decl->id = (yyvsp[(1) - (2)].Expr);
		(yyval.FnDecl)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 30:
#line 469 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl	*new_param;
		Expr	*dollar1 = ast_mkId("$cmd", (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));

		(yyval.FnDecl) = (yyvsp[(2) - (2)].FnDecl);
		(yyval.FnDecl)->decl->id = ast_mkId((yyvsp[(1) - (2)].s), (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
		ckfree((yyvsp[(1) - (2)].s));
		(yyval.FnDecl)->node.loc = (yylsp[(1) - (2)]);
		/* Prepend a new arg "$1" as the first formal. */
		new_param = ast_mkVarDecl(L_string, dollar1, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		new_param->flags = SCOPE_LOCAL | DECL_LOCAL_VAR;
		new_param->next = (yyvsp[(2) - (2)].FnDecl)->decl->type->u.func.formals;
		(yyval.FnDecl)->decl->type->u.func.formals = new_param;
	;}
    break;

  case 31:
#line 487 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Type	*type = type_mkFunc(NULL, (yyvsp[(2) - (5)].VarDecl));
		VarDecl	*decl = ast_mkVarDecl(type, NULL, (yylsp[(1) - (5)]), (yylsp[(3) - (5)]));
		decl->attrs = (yyvsp[(4) - (5)].Expr);
		(yyval.FnDecl) = ast_mkFnDecl(decl, (yyvsp[(5) - (5)].Stmt)->u.block, (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 32:
#line 494 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Type	*type = type_mkFunc(NULL, (yyvsp[(2) - (5)].VarDecl));
		VarDecl	*decl = ast_mkVarDecl(type, NULL, (yylsp[(1) - (5)]), (yylsp[(3) - (5)]));
		decl->attrs = (yyvsp[(4) - (5)].Expr);
		(yyval.FnDecl) = ast_mkFnDecl(decl, NULL, (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 33:
#line 504 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_LABEL, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Stmt)->u.label = (yyvsp[(1) - (3)].s);
		(yyval.Stmt)->next = (yyvsp[(3) - (3)].Stmt);
	;}
    break;

  case 34:
#line 510 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_LABEL, NULL, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Stmt)->u.label = (yyvsp[(1) - (2)].s);
	;}
    break;

  case 36:
#line 516 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		L_compile_attributes(L->options, (yyvsp[(1) - (1)].Expr), L_attrs_pragma);
		(yyval.Stmt) = NULL;
	;}
    break;

  case 37:
#line 521 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// Wrap the html in a puts(-nonewline) call.
		Expr	*fn = ast_mkId("puts", (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		Expr	*arg = ast_mkConst(L_string, "-nonewline", (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		arg->next = ast_mkConst(L_string, (yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt) = ast_mkStmt(L_STMT_EXPR, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt)->u.expr = ast_mkFnCall(fn, arg, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 38:
#line 530 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// Wrap expr in a puts(-nonewline) call.
		Expr	*fn = ast_mkId("puts", (yylsp[(2) - (3)]), (yylsp[(2) - (3)]));
		Expr	*arg = ast_mkConst(L_string, "-nonewline", (yylsp[(2) - (3)]), (yylsp[(2) - (3)]));
		arg->next = (yyvsp[(2) - (3)].Expr);
		(yyval.Stmt) = ast_mkStmt(L_STMT_EXPR, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Stmt)->u.expr = ast_mkFnCall(fn, arg, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 40:
#line 543 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 41:
#line 547 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr	*lit = ast_mkConst(L_int, (yyvsp[(3) - (3)].s), (yylsp[(3) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(1) - (3)].Expr), lit, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 42:
#line 552 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].Expr)->next = (yyvsp[(1) - (3)].Expr);
		(yyval.Expr) = (yyvsp[(3) - (3)].Expr);
		(yyval.Expr)->node.loc.beg = (yylsp[(1) - (3)]).beg;
	;}
    break;

  case 43:
#line 558 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(3) - (5)].Expr), (yyvsp[(5) - (5)].Expr), (yylsp[(3) - (5)]), (yylsp[(5) - (5)]));
		(yyval.Expr)->next = (yyvsp[(1) - (5)].Expr);
		(yyval.Expr)->node.loc.beg = (yylsp[(1) - (5)]).beg;
	;}
    break;

  case 44:
#line 564 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr	*lit = ast_mkConst(L_int, (yyvsp[(5) - (5)].s), (yylsp[(5) - (5)]), (yylsp[(5) - (5)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(3) - (5)].Expr), lit, (yylsp[(3) - (5)]), (yylsp[(5) - (5)]));
		(yyval.Expr)->next = (yyvsp[(1) - (5)].Expr);
		(yyval.Expr)->node.loc.beg = (yylsp[(1) - (5)]).beg;
	;}
    break;

  case 45:
#line 574 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Expr, next, (yyvsp[(2) - (2)].Expr));
		(yyval.Expr) = (yyvsp[(2) - (2)].Expr);
		(yyval.Expr)->node.loc.beg = (yylsp[(1) - (2)]).beg;
	;}
    break;

  case 46:
#line 583 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Expr, next, (yyvsp[(3) - (4)].Expr));
		(yyval.Expr) = (yyvsp[(3) - (4)].Expr);
		(yyval.Expr)->node.loc.beg = (yylsp[(1) - (4)]).beg;
		(yyval.Expr)->node.loc.end = (yylsp[(4) - (4)]).end;
	;}
    break;

  case 47:
#line 589 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Expr) = NULL; ;}
    break;

  case 50:
#line 599 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_COND, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt)->u.cond = (yyvsp[(1) - (1)].Cond);
	;}
    break;

  case 51:
#line 604 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_LOOP, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt)->u.loop = (yyvsp[(1) - (1)].Loop);
	;}
    break;

  case 52:
#line 609 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_SWITCH, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt)->u.swich = (yyvsp[(1) - (1)].Switch);
	;}
    break;

  case 53:
#line 614 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_FOREACH, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Stmt)->u.foreach = (yyvsp[(1) - (1)].ForEach);
	;}
    break;

  case 54:
#line 619 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_EXPR, NULL, (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
		(yyval.Stmt)->u.expr = (yyvsp[(1) - (2)].Expr);
	;}
    break;

  case 55:
#line 624 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_BREAK, NULL, (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
	;}
    break;

  case 56:
#line 628 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_CONTINUE, NULL, (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
	;}
    break;

  case 57:
#line 632 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_RETURN, NULL, (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
	;}
    break;

  case 58:
#line 636 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_RETURN, NULL, (yylsp[(1) - (3)]), (yylsp[(2) - (3)]));
		(yyval.Stmt)->u.expr = (yyvsp[(2) - (3)].Expr);
	;}
    break;

  case 59:
#line 641 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_GOTO, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Stmt)->u.label = (yyvsp[(2) - (3)].s);
	;}
    break;

  case 60:
#line 646 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/*
		 * We don't want to make "catch" a keyword since it's a Tcl
		 * function name, so allow any ID here but check it.
		 */
		unless (!strcmp((yyvsp[(3) - (7)].s), "catch")) {
			L_synerr2("syntax error -- expected 'catch'", (yylsp[(3) - (7)]).beg);
		}
		(yyval.Stmt) = ast_mkStmt(L_STMT_TRY, NULL, (yylsp[(1) - (7)]), (yylsp[(7) - (7)]));
		(yyval.Stmt)->u.try = ast_mkTry((yyvsp[(2) - (7)].Stmt), (yyvsp[(5) - (7)].Expr), (yyvsp[(7) - (7)].Stmt));
	;}
    break;

  case 61:
#line 658 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_TRY, NULL, (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
		(yyval.Stmt)->u.try = ast_mkTry((yyvsp[(2) - (4)].Stmt), NULL, (yyvsp[(4) - (4)].Stmt));
	;}
    break;

  case 62:
#line 662 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Stmt) = NULL; ;}
    break;

  case 63:
#line 667 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Cond) = ast_mkIfUnless((yyvsp[(3) - (6)].Expr), (yyvsp[(5) - (6)].Stmt), (yyvsp[(6) - (6)].Stmt), (yylsp[(1) - (6)]), (yylsp[(6) - (6)]));
	;}
    break;

  case 64:
#line 672 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Cond) = ast_mkIfUnless((yyvsp[(3) - (5)].Expr), (yyvsp[(5) - (5)].Stmt), NULL, (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 65:
#line 676 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Cond) = ast_mkIfUnless((yyvsp[(3) - (6)].Expr), (yyvsp[(6) - (6)].Stmt), (yyvsp[(5) - (6)].Stmt), (yylsp[(1) - (6)]), (yylsp[(6) - (6)]));
	;}
    break;

  case 66:
#line 680 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Cond) = ast_mkIfUnless((yyvsp[(3) - (5)].Expr), NULL, (yyvsp[(5) - (5)].Stmt), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 67:
#line 687 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Case	*c, *def;

		for (c = (yyvsp[(6) - (7)].Case), def = NULL; c; c = c->next) {
			if (c->expr) continue;
			if (def) {
				L_errf(c,
				"multiple default cases in switch statement");
			}
			def = c;
		}
		(yyval.Switch) = ast_mkSwitch((yyvsp[(3) - (7)].Expr), (yyvsp[(6) - (7)].Case), (yylsp[(1) - (7)]), (yylsp[(7) - (7)]));
	;}
    break;

  case 68:
#line 704 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(1) - (2)].Case)) {
			APPEND(Case, next, (yyvsp[(1) - (2)].Case), (yyvsp[(2) - (2)].Case));
			(yyval.Case) = (yyvsp[(1) - (2)].Case);
		} else {
			(yyval.Case) = (yyvsp[(2) - (2)].Case);
		}
	;}
    break;

  case 69:
#line 712 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Case) = NULL; ;}
    break;

  case 70:
#line 717 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Stmt, next, (yyvsp[(5) - (5)].Stmt));
		(yyval.Case) = ast_mkCase((yyvsp[(3) - (5)].Expr), (yyvsp[(5) - (5)].Stmt), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 71:
#line 722 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/* The default case is distinguished by a NULL expr. */
		REVERSE(Stmt, next, (yyvsp[(3) - (3)].Stmt));
		(yyval.Case) = ast_mkCase(NULL, (yyvsp[(3) - (3)].Stmt), (yylsp[(1) - (3)]), (yylsp[(2) - (3)]));
	;}
    break;

  case 72:
#line 731 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(1) - (1)].Expr)->flags & L_EXPR_RE_G) {
			L_errf((yyvsp[(1) - (1)].Expr), "illegal regular expression modifier");
		}
	;}
    break;

  case 74:
#line 742 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = (yyvsp[(2) - (2)].Stmt);
		(yyval.Stmt)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 75:
#line 747 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_COND, NULL, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Stmt)->u.cond = (yyvsp[(2) - (2)].Cond);
	;}
    break;

  case 76:
#line 751 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Stmt) = NULL; ;}
    break;

  case 77:
#line 756 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Loop) = ast_mkLoop(L_LOOP_WHILE, NULL, (yyvsp[(3) - (5)].Expr), NULL, (yyvsp[(5) - (5)].Stmt), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 78:
#line 760 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Loop) = ast_mkLoop(L_LOOP_DO, NULL, (yyvsp[(5) - (7)].Expr), NULL, (yyvsp[(2) - (7)].Stmt), (yylsp[(1) - (7)]), (yylsp[(6) - (7)]));
	;}
    break;

  case 79:
#line 764 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Loop) = ast_mkLoop(L_LOOP_FOR, (yyvsp[(3) - (6)].Expr), (yyvsp[(4) - (6)].Expr), NULL, (yyvsp[(6) - (6)].Stmt), (yylsp[(1) - (6)]), (yylsp[(6) - (6)]));
	;}
    break;

  case 80:
#line 768 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Loop) = ast_mkLoop(L_LOOP_FOR, (yyvsp[(3) - (7)].Expr), (yyvsp[(4) - (7)].Expr), (yyvsp[(5) - (7)].Expr), (yyvsp[(7) - (7)].Stmt), (yylsp[(1) - (7)]), (yylsp[(7) - (7)]));
	;}
    break;

  case 81:
#line 775 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.ForEach) = ast_mkForeach((yyvsp[(7) - (9)].Expr), (yyvsp[(3) - (9)].Expr), (yyvsp[(5) - (9)].Expr), (yyvsp[(9) - (9)].Stmt), (yylsp[(1) - (9)]), (yylsp[(9) - (9)]));
		unless (isid((yyvsp[(6) - (9)].Expr), "in")) {
			L_synerr2("syntax error -- expected 'in' in foreach",
				  (yylsp[(6) - (9)]).beg);
		}
	;}
    break;

  case 82:
#line 783 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.ForEach) = ast_mkForeach((yyvsp[(5) - (7)].Expr), (yyvsp[(3) - (7)].Expr), NULL, (yyvsp[(7) - (7)].Stmt), (yylsp[(1) - (7)]), (yylsp[(7) - (7)]));
		unless (isid((yyvsp[(4) - (7)].Expr), "in")) {
			L_synerr2("syntax error -- expected 'in' in foreach",
				  (yylsp[(4) - (7)]).beg);
		}
	;}
    break;

  case 83:
#line 793 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Expr) = NULL; ;}
    break;

  case 86:
#line 799 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Stmt) = NULL; ;}
    break;

  case 87:
#line 804 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Stmt, next, (yyvsp[(1) - (1)].Stmt));
		(yyval.Stmt) = (yyvsp[(1) - (1)].Stmt);
	;}
    break;

  case 88:
#line 809 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(2) - (2)].Stmt)) {
			REVERSE(Stmt, next, (yyvsp[(2) - (2)].Stmt));
			APPEND(Stmt, next, (yyvsp[(2) - (2)].Stmt), (yyvsp[(1) - (2)].Stmt));
			(yyval.Stmt) = (yyvsp[(2) - (2)].Stmt);
		} else {
			// Empty stmt.
			(yyval.Stmt) = (yyvsp[(1) - (2)].Stmt);
		}
	;}
    break;

  case 89:
#line 823 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl *v;
		REVERSE(VarDecl, next, (yyvsp[(1) - (1)].VarDecl));
		for (v = (yyvsp[(1) - (1)].VarDecl); v; v = v->next) {
			v->flags |= SCOPE_LOCAL | DECL_LOCAL_VAR;
		}
		(yyval.VarDecl) = (yyvsp[(1) - (1)].VarDecl);
		/*
		 * Special case a parameter list of "void" -- a single
		 * formal of type void with no arg name.  This really
		 * means there are no args.
		 */
		if ((yyvsp[(1) - (1)].VarDecl) && !(yyvsp[(1) - (1)].VarDecl)->next && !(yyvsp[(1) - (1)].VarDecl)->id && ((yyvsp[(1) - (1)].VarDecl)->type == L_void)) {
			(yyval.VarDecl) = NULL;
		}
	;}
    break;

  case 90:
#line 839 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.VarDecl) = NULL; ;}
    break;

  case 92:
#line 845 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].VarDecl)->next = (yyvsp[(1) - (3)].VarDecl);
		(yyval.VarDecl) = (yyvsp[(3) - (3)].VarDecl);
		(yyval.VarDecl)->node.loc = (yylsp[(1) - (3)]);
	;}
    break;

  case 93:
#line 854 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(3) - (3)].VarDecl)) {
			L_set_declBaseType((yyvsp[(3) - (3)].VarDecl), (yyvsp[(2) - (3)].Type));
			(yyval.VarDecl) = (yyvsp[(3) - (3)].VarDecl);
		} else {
			(yyval.VarDecl) = ast_mkVarDecl((yyvsp[(2) - (3)].Type), NULL, (yylsp[(2) - (3)]), (yylsp[(2) - (3)]));
			if (isnameoftype((yyvsp[(2) - (3)].Type))) (yyval.VarDecl)->flags |= DECL_REF;
		}
		(yyval.VarDecl)->flags |= (yyvsp[(1) - (3)].i);
		(yyval.VarDecl)->node.loc = (yylsp[(1) - (3)]);
	;}
    break;

  case 94:
#line 866 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Type *t = type_mkArray(NULL, L_poly);
		(yyval.VarDecl) = ast_mkVarDecl(t, (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.VarDecl)->flags |= (yyvsp[(1) - (3)].i) | DECL_REST_ARG;
	;}
    break;

  case 95:
#line 874 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = (yyvsp[(1) - (2)].i) | DECL_ARGUSED; ;}
    break;

  case 96:
#line 875 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = (yyvsp[(1) - (2)].i) | DECL_OPTIONAL; ;}
    break;

  case 97:
#line 876 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = (yyvsp[(1) - (2)].i) | DECL_NAME_EQUIV; ;}
    break;

  case 98:
#line 877 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = 0; ;}
    break;

  case 101:
#line 884 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(2) - (2)].Expr)->next = (yyvsp[(1) - (2)].Expr);
		(yyval.Expr) = (yyvsp[(2) - (2)].Expr);
		(yyval.Expr)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 102:
#line 890 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].Expr)->next = (yyvsp[(1) - (3)].Expr);
		(yyval.Expr) = (yyvsp[(3) - (3)].Expr);
		(yyval.Expr)->node.loc.end = (yylsp[(3) - (3)]).end;
	;}
    break;

  case 103:
#line 896 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].Expr)->next = (yyvsp[(1) - (3)].Expr);
		(yyval.Expr) = (yyvsp[(3) - (3)].Expr);
		(yyval.Expr)->node.loc.end = (yylsp[(3) - (3)]).end;
	;}
    break;

  case 104:
#line 902 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(4) - (4)].Expr)->next = (yyvsp[(3) - (4)].Expr);
		(yyvsp[(3) - (4)].Expr)->next = (yyvsp[(1) - (4)].Expr);
		(yyval.Expr) = (yyvsp[(4) - (4)].Expr);
		(yyval.Expr)->node.loc.end = (yylsp[(4) - (4)]).end;
	;}
    break;

  case 105:
#line 919 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		char	*s = cksprintf("-%s", (yyvsp[(1) - (2)].s));
		(yyval.Expr) = ast_mkConst(L_string, s, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		ckfree((yyvsp[(1) - (2)].s));
	;}
    break;

  case 106:
#line 925 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		char	*s = cksprintf("-default");
		(yyval.Expr) = ast_mkConst(L_string, s, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 107:
#line 933 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = (yyvsp[(2) - (3)].Expr);
		(yyval.Expr)->node.loc = (yylsp[(1) - (3)]);
		(yyval.Expr)->node.loc.end = (yylsp[(3) - (3)]).end;
	;}
    break;

  case 108:
#line 939 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// This is a binop where an arg is a Type*.
		(yyval.Expr) = ast_mkBinOp(L_OP_CAST, (Expr *)(yyvsp[(2) - (4)].Type), (yyvsp[(4) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 109:
#line 944 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_EXPAND, (yyvsp[(4) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 110:
#line 948 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_BANG, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 111:
#line 952 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_BITNOT, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 112:
#line 956 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_ADDROF, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 113:
#line 960 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_UMINUS, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 114:
#line 964 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_UPLUS, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 115:
#line 968 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_PLUSPLUS_PRE, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 116:
#line 972 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_MINUSMINUS_PRE, (yyvsp[(2) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 117:
#line 976 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_PLUSPLUS_POST, (yyvsp[(1) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 118:
#line 980 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_MINUSMINUS_POST, (yyvsp[(1) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 119:
#line 984 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQTWID, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 120:
#line 988 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_BANGTWID, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 121:
#line 992 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if (strchr((yyvsp[(5) - (5)].s), 'i')) (yyvsp[(3) - (5)].Expr)->flags |= L_EXPR_RE_I;
		if (strchr((yyvsp[(5) - (5)].s), 'g')) (yyvsp[(3) - (5)].Expr)->flags |= L_EXPR_RE_G;
		(yyval.Expr) = ast_mkTrinOp(L_OP_EQTWID, (yyvsp[(1) - (5)].Expr), (yyvsp[(3) - (5)].Expr), (yyvsp[(4) - (5)].Expr), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
		ckfree((yyvsp[(5) - (5)].s));
	;}
    break;

  case 122:
#line 999 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STAR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 123:
#line 1003 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_SLASH, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 124:
#line 1007 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_PERC, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 125:
#line 1011 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_PLUS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 126:
#line 1015 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_MINUS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 127:
#line 1019 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_EQ, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 128:
#line 1023 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_NE, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 129:
#line 1027 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_LT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 130:
#line 1031 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_LE, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 131:
#line 1035 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_GT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 132:
#line 1039 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_STR_GE, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 133:
#line 1043 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALEQUAL, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 134:
#line 1047 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALEQUAL, (yyvsp[(3) - (6)].Expr), (yyvsp[(5) - (6)].Expr), (yylsp[(1) - (6)]), (yylsp[(6) - (6)]));
	;}
    break;

  case 135:
#line 1051 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_NOTEQUAL, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 136:
#line 1055 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_GREATER, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 137:
#line 1059 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_GREATEREQ, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 138:
#line 1063 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_LESSTHAN, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 139:
#line 1067 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_LESSTHANEQ, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 140:
#line 1071 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_ANDAND, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 141:
#line 1075 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_OROR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 142:
#line 1079 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_LSHIFT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 143:
#line 1083 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_RSHIFT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 144:
#line 1087 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_BITOR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 145:
#line 1091 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_BITAND, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 146:
#line 1095 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_BITXOR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 150:
#line 1102 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkConst(L_int, (yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 151:
#line 1106 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkConst(L_float, (yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 152:
#line 1110 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Expr, next, (yyvsp[(3) - (4)].Expr));
		(yyval.Expr) = ast_mkFnCall((yyvsp[(1) - (4)].Expr), (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 153:
#line 1115 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkFnCall((yyvsp[(1) - (3)].Expr), NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 154:
#line 1119 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *id = ast_mkId("string", (yylsp[(1) - (4)]), (yylsp[(1) - (4)]));
		REVERSE(Expr, next, (yyvsp[(3) - (4)].Expr));
		(yyval.Expr) = ast_mkFnCall(id, (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 155:
#line 1125 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *id = ast_mkId("split", (yylsp[(1) - (7)]), (yylsp[(1) - (7)]));
		REVERSE(Expr, next, (yyvsp[(6) - (7)].Expr));
		(yyvsp[(4) - (7)].Expr)->next = (yyvsp[(6) - (7)].Expr);
		(yyval.Expr) = ast_mkFnCall(id, (yyvsp[(4) - (7)].Expr), (yylsp[(1) - (7)]), (yylsp[(7) - (7)]));
	;}
    break;

  case 156:
#line 1139 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *id = ast_mkId("split", (yylsp[(1) - (5)]), (yylsp[(1) - (5)]));
		REVERSE(Expr, next, (yyvsp[(4) - (5)].Expr));
		(yyval.Expr) = ast_mkFnCall(id, (yyvsp[(4) - (5)].Expr), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 157:
#line 1146 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Expr, next, (yyvsp[(3) - (4)].Expr));
		(yyval.Expr) = ast_mkFnCall((yyvsp[(1) - (4)].Expr), (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 158:
#line 1151 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkFnCall((yyvsp[(1) - (3)].Expr), NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 159:
#line 1155 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 160:
#line 1159 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQPLUS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 161:
#line 1163 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQMINUS, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 162:
#line 1167 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQSTAR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 163:
#line 1171 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQSLASH, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 164:
#line 1175 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQPERC, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 165:
#line 1179 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQBITAND, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 166:
#line 1183 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQBITOR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 167:
#line 1187 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQBITXOR, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 168:
#line 1191 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQLSHIFT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 169:
#line 1195 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQRSHIFT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 170:
#line 1199 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_EQDOT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 171:
#line 1203 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_DEFINED, (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 172:
#line 1207 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_ARRAY_INDEX, (yyvsp[(1) - (4)].Expr), (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 173:
#line 1211 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_HASH_INDEX, (yyvsp[(1) - (4)].Expr), (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 174:
#line 1215 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_CONCAT, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 175:
#line 1219 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_DOT, (yyvsp[(1) - (3)].Expr), NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr)->str = (yyvsp[(3) - (3)].s);
	;}
    break;

  case 176:
#line 1224 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_POINTS, (yyvsp[(1) - (3)].Expr), NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr)->str = (yyvsp[(3) - (3)].s);
	;}
    break;

  case 177:
#line 1229 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// This is a binop where an arg is a Type*.
		(yyval.Expr) = ast_mkBinOp(L_OP_CLASS_INDEX, (Expr *)(yyvsp[(1) - (3)].Typename).t, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr)->str = (yyvsp[(3) - (3)].s);
	;}
    break;

  case 178:
#line 1235 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		// This is a binop where an arg is a Type*.
		(yyval.Expr) = ast_mkBinOp(L_OP_CLASS_INDEX, (Expr *)(yyvsp[(1) - (3)].Typename).t, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr)->str = (yyvsp[(3) - (3)].s);
	;}
    break;

  case 179:
#line 1241 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_COMMA, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 180:
#line 1245 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkTrinOp(L_OP_ARRAY_SLICE, (yyvsp[(1) - (6)].Expr), (yyvsp[(3) - (6)].Expr), (yyvsp[(5) - (6)].Expr), (yylsp[(1) - (6)]), (yylsp[(3) - (6)]));
	;}
    break;

  case 181:
#line 1253 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = (yyvsp[(3) - (4)].Expr);
		(yyval.Expr)->node.loc = (yylsp[(1) - (4)]);
		(yyval.Expr)->node.loc.end = (yylsp[(4) - (4)]).end;
		L_scope_leave();
	;}
    break;

  case 182:
#line 1260 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_LIST, NULL, NULL, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 183:
#line 1264 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkTrinOp(L_OP_TERNARY_COND, (yyvsp[(1) - (5)].Expr), (yyvsp[(3) - (5)].Expr), (yyvsp[(5) - (5)].Expr), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
	;}
    break;

  case 184:
#line 1268 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_FILE, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 185:
#line 1272 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_FILE, NULL, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 186:
#line 1278 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { L_lex_begReArg(0); ;}
    break;

  case 187:
#line 1282 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { L_lex_begReArg(1); ;}
    break;

  case 188:
#line 1287 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkId((yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		ckfree((yyvsp[(1) - (1)].s));
	;}
    break;

  case 190:
#line 1296 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = (yyvsp[(1) - (3)].Expr);
		(yyval.Expr)->next = (yyvsp[(3) - (3)].Expr);
		(yyval.Expr)->node.loc.end = (yylsp[(3) - (3)]).end;
	;}
    break;

  case 191:
#line 1305 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Stmt) = ast_mkStmt(L_STMT_BLOCK, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Stmt)->u.block = ast_mkBlock(NULL, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		L_scope_leave();
	;}
    break;

  case 192:
#line 1311 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(Stmt, next, (yyvsp[(3) - (4)].Stmt));
		(yyval.Stmt) = ast_mkStmt(L_STMT_BLOCK, NULL, (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
		(yyval.Stmt)->u.block = ast_mkBlock(NULL, (yyvsp[(3) - (4)].Stmt), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
		L_scope_leave();
	;}
    break;

  case 193:
#line 1318 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl	*v;
		REVERSE(VarDecl, next, (yyvsp[(3) - (4)].VarDecl));
		for (v = (yyvsp[(3) - (4)].VarDecl); v; v = v->next) {
			v->flags |= SCOPE_LOCAL | DECL_LOCAL_VAR;
		}
		(yyval.Stmt) = ast_mkStmt(L_STMT_BLOCK, NULL, (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
		(yyval.Stmt)->u.block = ast_mkBlock((yyvsp[(3) - (4)].VarDecl), NULL, (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
		L_scope_leave();
	;}
    break;

  case 194:
#line 1329 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl	*v;
		REVERSE(VarDecl, next, (yyvsp[(3) - (5)].VarDecl));
		for (v = (yyvsp[(3) - (5)].VarDecl); v; v = v->next) {
			v->flags |= SCOPE_LOCAL | DECL_LOCAL_VAR;
		}
		REVERSE(Stmt, next, (yyvsp[(4) - (5)].Stmt));
		(yyval.Stmt) = ast_mkStmt(L_STMT_BLOCK, NULL, (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
		(yyval.Stmt)->u.block = ast_mkBlock((yyvsp[(3) - (5)].VarDecl), (yyvsp[(4) - (5)].Stmt), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
		L_scope_leave();
	;}
    break;

  case 195:
#line 1343 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { L_scope_enter(); ;}
    break;

  case 197:
#line 1349 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/*
		 * Each declaration is a list of declarators.  Here we
		 * append the lists.
		 */
		APPEND(VarDecl, next, (yyvsp[(2) - (2)].VarDecl), (yyvsp[(1) - (2)].VarDecl));
		(yyval.VarDecl) = (yyvsp[(2) - (2)].VarDecl);
	;}
    break;

  case 199:
#line 1362 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl *v;
		for (v = (yyvsp[(2) - (2)].VarDecl); v; v = v->next) {
			v->flags |= (yyvsp[(1) - (2)].i);
		}
		(yyval.VarDecl) = (yyvsp[(2) - (2)].VarDecl);
		(yyval.VarDecl)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 200:
#line 1373 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = DECL_PRIVATE; ;}
    break;

  case 201:
#line 1374 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = DECL_PUBLIC; ;}
    break;

  case 202:
#line 1375 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.i) = DECL_EXTERN; ;}
    break;

  case 203:
#line 1380 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/* Don't REVERSE $2; it's done as part of declaration_list. */
		VarDecl *v;
		for (v = (yyvsp[(2) - (3)].VarDecl); v; v = v->next) {
			L_set_declBaseType(v, (yyvsp[(1) - (3)].Type));
		}
		(yyval.VarDecl) = (yyvsp[(2) - (3)].VarDecl);
	;}
    break;

  case 204:
#line 1388 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.VarDecl) = NULL; ;}
    break;

  case 206:
#line 1394 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].VarDecl)->next = (yyvsp[(1) - (3)].VarDecl);
		(yyval.VarDecl) = (yyvsp[(3) - (3)].VarDecl);
	;}
    break;

  case 208:
#line 1403 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(3) - (3)].VarDecl)->next = (yyvsp[(1) - (3)].VarDecl);
		(yyval.VarDecl) = (yyvsp[(3) - (3)].VarDecl);
	;}
    break;

  case 210:
#line 1412 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyvsp[(1) - (3)].VarDecl)->initializer = ast_mkBinOp(L_OP_EQUALS, (yyvsp[(1) - (3)].VarDecl)->id, (yyvsp[(3) - (3)].Expr), (yylsp[(3) - (3)]), (yylsp[(3) - (3)]));
		(yyval.VarDecl) = (yyvsp[(1) - (3)].VarDecl);
		(yyval.VarDecl)->node.loc.end = (yylsp[(3) - (3)]).end;
	;}
    break;

  case 212:
#line 1421 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.VarDecl) = NULL; ;}
    break;

  case 213:
#line 1426 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.VarDecl) = ast_mkVarDecl((yyvsp[(2) - (2)].Type), (yyvsp[(1) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 214:
#line 1430 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *id = ast_mkId((yyvsp[(1) - (2)].Typename).s, (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
		(yyval.VarDecl) = ast_mkVarDecl((yyvsp[(2) - (2)].Type), id, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		if (isnameoftype((yyvsp[(1) - (2)].Typename).t)) (yyval.VarDecl)->flags |= DECL_REF;
		ckfree((yyvsp[(1) - (2)].Typename).s);
	;}
    break;

  case 215:
#line 1437 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Type *t = type_mkNameOf((yyvsp[(3) - (3)].Type));
		(yyval.VarDecl) = ast_mkVarDecl(t, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.VarDecl)->flags |= DECL_REF;
	;}
    break;

  case 216:
#line 1443 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Type *tf = type_mkFunc(NULL, (yyvsp[(4) - (5)].VarDecl));
		Type *tn = type_mkNameOf(tf);
		(yyval.VarDecl) = ast_mkVarDecl(tn, (yyvsp[(2) - (5)].Expr), (yylsp[(1) - (5)]), (yylsp[(5) - (5)]));
		(yyval.VarDecl)->flags |= DECL_REF;
	;}
    break;

  case 217:
#line 1454 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Type) = NULL;
	;}
    break;

  case 218:
#line 1458 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Type) = type_mkArray((yyvsp[(2) - (4)].Expr), (yyvsp[(4) - (4)].Type));
	;}
    break;

  case 219:
#line 1462 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Type) = type_mkArray(NULL, (yyvsp[(3) - (3)].Type));
	;}
    break;

  case 220:
#line 1466 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Type) = type_mkHash((yyvsp[(2) - (4)].Type), (yyvsp[(4) - (4)].Type));
	;}
    break;

  case 221:
#line 1473 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(2) - (2)].Type)) {
			L_set_baseType((yyvsp[(2) - (2)].Type), (yyvsp[(1) - (2)].Type));
			(yyval.Type) = (yyvsp[(2) - (2)].Type);
		} else {
			(yyval.Type) = (yyvsp[(1) - (2)].Type);
		}
	;}
    break;

  case 222:
#line 1482 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		if ((yyvsp[(2) - (2)].Type)) {
			L_set_baseType((yyvsp[(2) - (2)].Type), (yyvsp[(1) - (2)].Type));
			(yyval.Type) = (yyvsp[(2) - (2)].Type);
		} else {
			(yyval.Type) = (yyvsp[(1) - (2)].Type);
		}
	;}
    break;

  case 223:
#line 1493 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_string; ;}
    break;

  case 224:
#line 1494 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_int; ;}
    break;

  case 225:
#line 1495 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_float; ;}
    break;

  case 226:
#line 1496 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_poly; ;}
    break;

  case 227:
#line 1497 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_widget; ;}
    break;

  case 228:
#line 1498 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = L_void; ;}
    break;

  case 229:
#line 1499 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.Type) = (yyvsp[(1) - (1)].Typename).t; ckfree((yyvsp[(1) - (1)].Typename).s); ;}
    break;

  case 230:
#line 1504 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(VarDecl, next, (yyvsp[(4) - (5)].VarDecl));
		(yyval.Type) = L_struct_store((yyvsp[(2) - (5)].s), (yyvsp[(4) - (5)].VarDecl));
		ckfree((yyvsp[(2) - (5)].s));
	;}
    break;

  case 231:
#line 1510 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		REVERSE(VarDecl, next, (yyvsp[(3) - (4)].VarDecl));
		(void)L_struct_store(NULL, (yyvsp[(3) - (4)].VarDecl));  // to sanity check member types
		(yyval.Type) = type_mkStruct(NULL, (yyvsp[(3) - (4)].VarDecl));
	;}
    break;

  case 232:
#line 1516 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Type) = L_struct_lookup((yyvsp[(2) - (2)].s), FALSE);
		ckfree((yyvsp[(2) - (2)].s));
	;}
    break;

  case 234:
#line 1525 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		APPEND(VarDecl, next, (yyvsp[(2) - (2)].VarDecl), (yyvsp[(1) - (2)].VarDecl));
		(yyval.VarDecl) = (yyvsp[(2) - (2)].VarDecl);
		(yyval.VarDecl)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 235:
#line 1533 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    { (yyval.VarDecl)->node.loc.end = (yylsp[(2) - (2)]).end; ;}
    break;

  case 236:
#line 1538 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		VarDecl *v;
		for (v = (yyvsp[(2) - (2)].VarDecl); v; v = v->next) {
			L_set_declBaseType(v, (yyvsp[(1) - (2)].Type));
		}
		(yyval.VarDecl) = (yyvsp[(2) - (2)].VarDecl);
		(yyval.VarDecl)->node.loc = (yylsp[(1) - (2)]);
	;}
    break;

  case 238:
#line 1551 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		APPEND(Expr, b, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr));
		(yyval.Expr) = (yyvsp[(1) - (3)].Expr);
	;}
    break;

  case 240:
#line 1560 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkBinOp(L_OP_LIST, (yyvsp[(1) - (1)].Expr), NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 241:
#line 1564 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *kv = ast_mkBinOp(L_OP_KV, (yyvsp[(1) - (3)].Expr), (yyvsp[(3) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_LIST, kv, NULL, (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 242:
#line 1572 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkConst(L_string, (yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 243:
#line 1576 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *right = ast_mkConst(L_string, (yyvsp[(2) - (2)].s), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_STRING, (yyvsp[(1) - (2)].Expr), right, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 244:
#line 1581 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *right = ast_mkConst(L_string, (yyvsp[(2) - (2)].s), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_STRING, (yyvsp[(1) - (2)].Expr), right, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 245:
#line 1589 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *left  = ast_mkConst(L_string, (yyvsp[(1) - (2)].s), (yylsp[(1) - (2)]), (yylsp[(1) - (2)]));
		Expr *right = ast_mkUnOp(L_OP_CMDSUBST, NULL, (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		right->str = (yyvsp[(2) - (2)].s);
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_STRING, left, right, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 246:
#line 1596 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *middle = ast_mkConst(L_string, (yyvsp[(2) - (3)].s), (yylsp[(2) - (3)]), (yylsp[(2) - (3)]));
		Expr *right  = ast_mkUnOp(L_OP_CMDSUBST, NULL, (yylsp[(3) - (3)]), (yylsp[(3) - (3)]));
		right->str = (yyvsp[(3) - (3)].s);
		(yyval.Expr) = ast_mkTrinOp(L_OP_INTERP_STRING, (yyvsp[(1) - (3)].Expr), middle, right,
				  (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 247:
#line 1607 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_CMDSUBST, NULL, (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		(yyval.Expr)->str = (yyvsp[(1) - (1)].s);
	;}
    break;

  case 248:
#line 1612 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkUnOp(L_OP_CMDSUBST, (yyvsp[(1) - (2)].Expr), (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Expr)->str = (yyvsp[(2) - (2)].s);
	;}
    break;

  case 249:
#line 1620 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkRegexp((yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 250:
#line 1624 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *right = ast_mkConst(L_string, (yyvsp[(2) - (2)].s), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_RE, (yyvsp[(1) - (2)].Expr), right, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 251:
#line 1632 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		/* Note: the scanner catches illegal modifiers. */
		if (strchr((yyvsp[(2) - (2)].s), 'i')) (yyvsp[(1) - (2)].Expr)->flags |= L_EXPR_RE_I;
		if (strchr((yyvsp[(2) - (2)].s), 'g')) (yyvsp[(1) - (2)].Expr)->flags |= L_EXPR_RE_G;
		if (strchr((yyvsp[(2) - (2)].s), 'l')) (yyvsp[(1) - (2)].Expr)->flags |= L_EXPR_RE_L;
		if (strchr((yyvsp[(2) - (2)].s), 't')) (yyvsp[(1) - (2)].Expr)->flags |= L_EXPR_RE_T;
		ckfree((yyvsp[(2) - (2)].s));
		(yyval.Expr) = (yyvsp[(1) - (2)].Expr);
	;}
    break;

  case 252:
#line 1645 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkConst(L_string, (yyvsp[(1) - (1)].s), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 253:
#line 1649 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *right = ast_mkConst(L_string, (yyvsp[(2) - (2)].s), (yylsp[(2) - (2)]), (yylsp[(2) - (2)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_RE, (yyvsp[(1) - (2)].Expr), right, (yylsp[(1) - (2)]), (yylsp[(2) - (2)]));
	;}
    break;

  case 254:
#line 1657 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *left = ast_mkConst(L_string, (yyvsp[(1) - (3)].s), (yylsp[(1) - (3)]), (yylsp[(1) - (3)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_STRING, left, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 255:
#line 1662 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *middle = ast_mkConst(L_string, (yyvsp[(2) - (4)].s), (yylsp[(2) - (4)]), (yylsp[(2) - (4)]));
		(yyval.Expr) = ast_mkTrinOp(L_OP_INTERP_STRING, (yyvsp[(1) - (4)].Expr), middle, (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 256:
#line 1670 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *left = ast_mkConst(L_string, (yyvsp[(1) - (3)].s), (yylsp[(1) - (3)]), (yylsp[(1) - (3)]));
		(yyval.Expr) = ast_mkBinOp(L_OP_INTERP_STRING, left, (yyvsp[(2) - (3)].Expr), (yylsp[(1) - (3)]), (yylsp[(3) - (3)]));
	;}
    break;

  case 257:
#line 1675 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Expr *middle = ast_mkConst(L_string, (yyvsp[(2) - (4)].s), (yylsp[(2) - (4)]), (yylsp[(2) - (4)]));
		(yyval.Expr) = ast_mkTrinOp(L_OP_INTERP_STRING, (yyvsp[(1) - (4)].Expr), middle, (yyvsp[(3) - (4)].Expr), (yylsp[(1) - (4)]), (yylsp[(4) - (4)]));
	;}
    break;

  case 258:
#line 1683 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkId(".", (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
	;}
    break;

  case 259:
#line 1687 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.Expr) = ast_mkId(Tcl_GetString((yyvsp[(1) - (1)].obj)), (yylsp[(1) - (1)]), (yylsp[(1) - (1)]));
		Tcl_DecrRefCount((yyvsp[(1) - (1)].obj));
	;}
    break;

  case 260:
#line 1695 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		(yyval.obj) = Tcl_NewObj();
		Tcl_IncrRefCount((yyval.obj));
		Tcl_AppendToObj((yyval.obj), ".", 1);
		Tcl_AppendToObj((yyval.obj), (yyvsp[(2) - (2)].s), -1);
		ckfree((yyvsp[(2) - (2)].s));
	;}
    break;

  case 261:
#line 1703 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"
    {
		Tcl_AppendToObj((yyvsp[(1) - (3)].obj), ".", 1);
		Tcl_AppendToObj((yyvsp[(1) - (3)].obj), (yyvsp[(3) - (3)].s), -1);
		(yyval.obj) = (yyvsp[(1) - (3)].obj);
		ckfree((yyvsp[(3) - (3)].s));
	;}
    break;


/* Line 1267 of yacc.c.  */
#line 5007 "Lgrammar.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }

  yyerror_range[0] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, &yylloc);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  yyerror_range[0] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      yyerror_range[0] = *yylsp;
      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, yylsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;

  yyerror_range[1] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the look-ahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, (yyerror_range - 1), 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, &yylloc);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, yylsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 1710 "/Users/rob/bk/dev-oss-L-structs-in-funcs/src/gui/tcltk/tcl/generic/Lgrammar.y"


