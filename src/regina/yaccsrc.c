/* A Bison parser, made by GNU Bison 3.7.5.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30705

/* Bison version string.  */
#define YYBISON_VERSION "3.7.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         __reginaparse
#define yylex           __reginalex
#define yyerror         __reginaerror
#define yydebug         __reginadebug
#define yynerrs         __reginanerrs
#define yylval          __reginalval
#define yychar          __reginachar

/* First part of user prologue.  */
#line 1 "./yaccsrc.y"


#ifndef lint
static char *RCSid = "$Id: yaccsrc.c,v 1.48 2023/03/11 01:30:15 mark Exp $";
#endif

/*
 *  The Regina Rexx Interpreter
 *  Copyright (C) 1992-1994  Anders Christensen <anders@pvv.unit.no>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "rexx.h"
#include <time.h>

#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif

#if defined(HAVE_ALLOCA_H)
# include <alloca.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if defined(_MSC_VER) || defined(MAC)
# define __STDC__ 1 /* Hack to allow const since it is not defined */
#endif

#define YYSTYPE nodeptr

/* locals, they are protected by regina_parser (see lexsrc.l) */
static int tmplno,           /* lineno of current instruction             */
           tmpchr,           /* character position of current instruction */
           level,            /* nested do/if/select depth                 */
           start_parendepth; /* see below at parendepth                   */

/*
 * parendepth regulates the action which happens detecting a comma or an
 * empty expression. A negative values indicates an error; both a comma
 * and an empty expression raise an error.
 * We regulate the enumeration of arguments with this semantical flag.
 * Look at "call subroutine args" and "function args". Function itself
 * contains a parentheses pair, so starting with a depth of just allows
 * the enumeration. subroutine starts with either 0 or 1. The latter one
 * is allowed for the support request of "call subroutine(a,b,c)" which
 * isn't allowed by ANSI but can be enabled for backward compatibility.
 */
static int parendepth;

static nodeptr current, with = NULL;

static char *nullptr = NULL; /* for C++ compilation */

typedef enum { IS_UNKNOWN,
               IS_A_NUMBER,
               IS_NO_NUMBER,
               IS_SIM_SYMBOL,
               IS_COMP_SYMBOL } node_type;

typedef enum { REDUCE_CALL,
               REDUCE_EXPR,
               REDUCE_RIGHT,
               REDUCE_SUBEXPR } reduce_mode;

static node_type gettypeof( nodeptr thisptr ) ;
static void checkconst( nodeptr thisptr ) ;
static nodeptr reduce_expr_list( nodeptr thisptr, reduce_mode mode );
static void transform( nodeptr thisptr ) ;
static nodeptr create_head( const char *name ) ;
static nodeptr makenode( int type, int numb, ... ) ;
static void checkdosyntax( cnodeptr thisptr ) ;
void newlabel( const tsd_t *TSD, internal_parser_type *ipt, nodeptr thisptr ) ;
static nodeptr optgluelast( nodeptr p1, nodeptr p2 );
static void move_labels( nodeptr front, nodeptr end, int level );

#define IS_EXPRLIST(x) ( ( (x) != NULL )                   \
                      && ( ( (x)->type == X_CEXPRLIST )    \
                        || ( (x)->type == X_EXPRLIST ) ) )

#define IS_FUNCTION(x) ( ( (x) != NULL )                  \
                      && ( ( (x)->type == X_EX_FUNC )     \
                        || ( (x)->type == X_IN_FUNC ) ) )

#define AUTO_REDUCE(x,y) { if ( parendepth == 1 )                             \
                           {                                                  \
                             x = reduce_expr_list( x, REDUCE_EXPR );          \
                             /* detect "call s (a,b)<op>" and      */         \
                             /* "call s ()<op>"                    */         \
                             if ( IS_EXPRLIST( x ) )                          \
                             {                                                \
                                if ( (y) != NULL )                            \
                                   exiterror( ERR_INVALID_EXPRESSION, 1, y ); \
                                else if ( (x)->p[0] == NULL )                 \
                                   exiterror( ERR_UNEXPECTED_PARAN, 0 );      \
                                else                                          \
                                   exiterror( ERR_UNEXPECTED_PARAN, 1 );      \
                             }                                                \
                           }                                                  \
                         }
#line 150 "./yaccsrc.y"

#ifdef NDEBUG
# define YYDEBUG 0
#else
# define YYDEBUG 1
#endif

#line 203 "./yaccsrc.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "yaccsrc.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ADDRESS = 3,                    /* ADDRESS  */
  YYSYMBOL_ARG = 4,                        /* ARG  */
  YYSYMBOL_CALL = 5,                       /* CALL  */
  YYSYMBOL_DO = 6,                         /* DO  */
  YYSYMBOL_TO = 7,                         /* TO  */
  YYSYMBOL_BY = 8,                         /* BY  */
  YYSYMBOL_FOR = 9,                        /* FOR  */
  YYSYMBOL_WHILE = 10,                     /* WHILE  */
  YYSYMBOL_UNTIL = 11,                     /* UNTIL  */
  YYSYMBOL_EXIT = 12,                      /* EXIT  */
  YYSYMBOL_IF = 13,                        /* IF  */
  YYSYMBOL_THEN = 14,                      /* THEN  */
  YYSYMBOL_ELSE = 15,                      /* ELSE  */
  YYSYMBOL_ITERATE = 16,                   /* ITERATE  */
  YYSYMBOL_INTERPRET = 17,                 /* INTERPRET  */
  YYSYMBOL_LEAVE = 18,                     /* LEAVE  */
  YYSYMBOL_NOP = 19,                       /* NOP  */
  YYSYMBOL_NUMERIC = 20,                   /* NUMERIC  */
  YYSYMBOL_PARSE = 21,                     /* PARSE  */
  YYSYMBOL_EXTERNAL = 22,                  /* EXTERNAL  */
  YYSYMBOL_SOURCE = 23,                    /* SOURCE  */
  YYSYMBOL_VAR = 24,                       /* VAR  */
  YYSYMBOL_VALUE = 25,                     /* VALUE  */
  YYSYMBOL_WITH = 26,                      /* WITH  */
  YYSYMBOL_PROCEDURE = 27,                 /* PROCEDURE  */
  YYSYMBOL_EXPOSE = 28,                    /* EXPOSE  */
  YYSYMBOL_PULL = 29,                      /* PULL  */
  YYSYMBOL_PUSH = 30,                      /* PUSH  */
  YYSYMBOL_QUEUE = 31,                     /* QUEUE  */
  YYSYMBOL_SAY = 32,                       /* SAY  */
  YYSYMBOL_RETURN = 33,                    /* RETURN  */
  YYSYMBOL_SELECT = 34,                    /* SELECT  */
  YYSYMBOL_WHEN = 35,                      /* WHEN  */
  YYSYMBOL_DROP = 36,                      /* DROP  */
  YYSYMBOL_OTHERWISE = 37,                 /* OTHERWISE  */
  YYSYMBOL_SIGNAL = 38,                    /* SIGNAL  */
  YYSYMBOL_ON = 39,                        /* ON  */
  YYSYMBOL_OFF = 40,                       /* OFF  */
  YYSYMBOL_SYNTAX = 41,                    /* SYNTAX  */
  YYSYMBOL_HALT = 42,                      /* HALT  */
  YYSYMBOL_NOVALUE = 43,                   /* NOVALUE  */
  YYSYMBOL_TRACE = 44,                     /* TRACE  */
  YYSYMBOL_END = 45,                       /* END  */
  YYSYMBOL_UPPER = 46,                     /* UPPER  */
  YYSYMBOL_ASSIGNMENTVARIABLE = 47,        /* ASSIGNMENTVARIABLE  */
  YYSYMBOL_STATSEP = 48,                   /* STATSEP  */
  YYSYMBOL_FOREVER = 49,                   /* FOREVER  */
  YYSYMBOL_DIGITS = 50,                    /* DIGITS  */
  YYSYMBOL_FORM = 51,                      /* FORM  */
  YYSYMBOL_FUZZ = 52,                      /* FUZZ  */
  YYSYMBOL_SCIENTIFIC = 53,                /* SCIENTIFIC  */
  YYSYMBOL_ENGINEERING = 54,               /* ENGINEERING  */
  YYSYMBOL_NOT = 55,                       /* NOT  */
  YYSYMBOL_CONCATENATE = 56,               /* CONCATENATE  */
  YYSYMBOL_MODULUS = 57,                   /* MODULUS  */
  YYSYMBOL_GTE = 58,                       /* GTE  */
  YYSYMBOL_GT = 59,                        /* GT  */
  YYSYMBOL_LTE = 60,                       /* LTE  */
  YYSYMBOL_LT = 61,                        /* LT  */
  YYSYMBOL_DIFFERENT = 62,                 /* DIFFERENT  */
  YYSYMBOL_EQUALEQUAL = 63,                /* EQUALEQUAL  */
  YYSYMBOL_NOTEQUALEQUAL = 64,             /* NOTEQUALEQUAL  */
  YYSYMBOL_OFFSET = 65,                    /* OFFSET  */
  YYSYMBOL_SPACE = 66,                     /* SPACE  */
  YYSYMBOL_EXP = 67,                       /* EXP  */
  YYSYMBOL_XOR = 68,                       /* XOR  */
  YYSYMBOL_PLACEHOLDER = 69,               /* PLACEHOLDER  */
  YYSYMBOL_NOTREADY = 70,                  /* NOTREADY  */
  YYSYMBOL_CONSYMBOL = 71,                 /* CONSYMBOL  */
  YYSYMBOL_SIMSYMBOL = 72,                 /* SIMSYMBOL  */
  YYSYMBOL_EXFUNCNAME = 73,                /* EXFUNCNAME  */
  YYSYMBOL_INFUNCNAME = 74,                /* INFUNCNAME  */
  YYSYMBOL_LABEL = 75,                     /* LABEL  */
  YYSYMBOL_DOVARIABLE = 76,                /* DOVARIABLE  */
  YYSYMBOL_HEXSTRING = 77,                 /* HEXSTRING  */
  YYSYMBOL_STRING = 78,                    /* STRING  */
  YYSYMBOL_VERSION = 79,                   /* VERSION  */
  YYSYMBOL_LINEIN = 80,                    /* LINEIN  */
  YYSYMBOL_WHATEVER = 81,                  /* WHATEVER  */
  YYSYMBOL_NAME = 82,                      /* NAME  */
  YYSYMBOL_FAILURE = 83,                   /* FAILURE  */
  YYSYMBOL_BINSTRING = 84,                 /* BINSTRING  */
  YYSYMBOL_OPTIONS = 85,                   /* OPTIONS  */
  YYSYMBOL_ENVIRONMENT = 86,               /* ENVIRONMENT  */
  YYSYMBOL_LOSTDIGITS = 87,                /* LOSTDIGITS  */
  YYSYMBOL_GTGT = 88,                      /* GTGT  */
  YYSYMBOL_LTLT = 89,                      /* LTLT  */
  YYSYMBOL_NOTGTGT = 90,                   /* NOTGTGT  */
  YYSYMBOL_NOTLTLT = 91,                   /* NOTLTLT  */
  YYSYMBOL_GTGTE = 92,                     /* GTGTE  */
  YYSYMBOL_LTLTE = 93,                     /* LTLTE  */
  YYSYMBOL_INPUT = 94,                     /* INPUT  */
  YYSYMBOL_OUTPUT = 95,                    /* OUTPUT  */
  YYSYMBOL_ERROR = 96,                     /* ERROR  */
  YYSYMBOL_NORMAL = 97,                    /* NORMAL  */
  YYSYMBOL_APPEND = 98,                    /* APPEND  */
  YYSYMBOL_REPLACE = 99,                   /* REPLACE  */
  YYSYMBOL_STREAM = 100,                   /* STREAM  */
  YYSYMBOL_STEM = 101,                     /* STEM  */
  YYSYMBOL_LIFO = 102,                     /* LIFO  */
  YYSYMBOL_FIFO = 103,                     /* FIFO  */
  YYSYMBOL_NOEOL = 104,                    /* NOEOL  */
  YYSYMBOL_LOWER = 105,                    /* LOWER  */
  YYSYMBOL_CASELESS = 106,                 /* CASELESS  */
  YYSYMBOL_PLUSASSIGNMENTVARIABLE = 107,   /* PLUSASSIGNMENTVARIABLE  */
  YYSYMBOL_MINUSASSIGNMENTVARIABLE = 108,  /* MINUSASSIGNMENTVARIABLE  */
  YYSYMBOL_MULTASSIGNMENTVARIABLE = 109,   /* MULTASSIGNMENTVARIABLE  */
  YYSYMBOL_DIVASSIGNMENTVARIABLE = 110,    /* DIVASSIGNMENTVARIABLE  */
  YYSYMBOL_MODULUSASSIGNMENTVARIABLE = 111, /* MODULUSASSIGNMENTVARIABLE  */
  YYSYMBOL_INTDIVASSIGNMENTVARIABLE = 112, /* INTDIVASSIGNMENTVARIABLE  */
  YYSYMBOL_ORASSIGNMENTVARIABLE = 113,     /* ORASSIGNMENTVARIABLE  */
  YYSYMBOL_XORASSIGNMENTVARIABLE = 114,    /* XORASSIGNMENTVARIABLE  */
  YYSYMBOL_ANDASSIGNMENTVARIABLE = 115,    /* ANDASSIGNMENTVARIABLE  */
  YYSYMBOL_CONCATASSIGNMENTVARIABLE = 116, /* CONCATASSIGNMENTVARIABLE  */
  YYSYMBOL_117_ = 117,                     /* '|'  */
  YYSYMBOL_118_ = 118,                     /* '&'  */
  YYSYMBOL_119_ = 119,                     /* '='  */
  YYSYMBOL_CCAT = 120,                     /* CCAT  */
  YYSYMBOL_121_ = 121,                     /* '+'  */
  YYSYMBOL_122_ = 122,                     /* '-'  */
  YYSYMBOL_123_ = 123,                     /* '*'  */
  YYSYMBOL_124_ = 124,                     /* '/'  */
  YYSYMBOL_125_ = 125,                     /* '%'  */
  YYSYMBOL_UMINUS = 126,                   /* UMINUS  */
  YYSYMBOL_UPLUS = 127,                    /* UPLUS  */
  YYSYMBOL_128_ = 128,                     /* '('  */
  YYSYMBOL_129_ = 129,                     /* ')'  */
  YYSYMBOL_130_ = 130,                     /* ','  */
  YYSYMBOL_YYACCEPT = 131,                 /* $accept  */
  YYSYMBOL_start = 132,                    /* start  */
  YYSYMBOL_133_1 = 133,                    /* $@1  */
  YYSYMBOL_prog = 134,                     /* prog  */
  YYSYMBOL_stats = 135,                    /* stats  */
  YYSYMBOL_xstats = 136,                   /* xstats  */
  YYSYMBOL_ystatement = 137,               /* ystatement  */
  YYSYMBOL_lonely_end = 138,               /* lonely_end  */
  YYSYMBOL_nxstats = 139,                  /* nxstats  */
  YYSYMBOL_ncl = 140,                      /* ncl  */
  YYSYMBOL_nlncl = 141,                    /* nlncl  */
  YYSYMBOL_optLabels = 142,                /* optLabels  */
  YYSYMBOL_statement = 143,                /* statement  */
  YYSYMBOL_gruff = 144,                    /* gruff  */
  YYSYMBOL_mttstatement = 145,             /* mttstatement  */
  YYSYMBOL_mtstatement = 146,              /* mtstatement  */
  YYSYMBOL_nclstatement = 147,             /* nclstatement  */
  YYSYMBOL_call = 148,                     /* call  */
  YYSYMBOL_do = 149,                       /* do  */
  YYSYMBOL_exit = 150,                     /* exit  */
  YYSYMBOL_if = 151,                       /* if  */
  YYSYMBOL_iterate = 152,                  /* iterate  */
  YYSYMBOL_leave = 153,                    /* leave  */
  YYSYMBOL_say = 154,                      /* say  */
  YYSYMBOL_return = 155,                   /* return  */
  YYSYMBOL_address = 156,                  /* address  */
  YYSYMBOL_arg = 157,                      /* arg  */
  YYSYMBOL_drop = 158,                     /* drop  */
  YYSYMBOL_interpret = 159,                /* interpret  */
  YYSYMBOL_label = 160,                    /* label  */
  YYSYMBOL_nop = 161,                      /* nop  */
  YYSYMBOL_numeric = 162,                  /* numeric  */
  YYSYMBOL_options = 163,                  /* options  */
  YYSYMBOL_parse = 164,                    /* parse  */
  YYSYMBOL_proc = 165,                     /* proc  */
  YYSYMBOL_pull = 166,                     /* pull  */
  YYSYMBOL_push = 167,                     /* push  */
  YYSYMBOL_queue = 168,                    /* queue  */
  YYSYMBOL_select = 169,                   /* select  */
  YYSYMBOL_signal = 170,                   /* signal  */
  YYSYMBOL_when = 171,                     /* when  */
  YYSYMBOL_otherwise = 172,                /* otherwise  */
  YYSYMBOL_trace = 173,                    /* trace  */
  YYSYMBOL_upper = 174,                    /* upper  */
  YYSYMBOL_address_stat = 175,             /* address_stat  */
  YYSYMBOL_176_2 = 176,                    /* @2  */
  YYSYMBOL_address_stat2 = 177,            /* address_stat2  */
  YYSYMBOL_178_3 = 178,                    /* $@3  */
  YYSYMBOL_arg_stat = 179,                 /* arg_stat  */
  YYSYMBOL_call_stat = 180,                /* call_stat  */
  YYSYMBOL_181_4 = 181,                    /* $@4  */
  YYSYMBOL_182_5 = 182,                    /* $@5  */
  YYSYMBOL_call_name = 183,                /* call_name  */
  YYSYMBOL_call_args = 184,                /* call_args  */
  YYSYMBOL_expr_stat = 185,                /* expr_stat  */
  YYSYMBOL_end_stat = 186,                 /* end_stat  */
  YYSYMBOL_end = 187,                      /* end  */
  YYSYMBOL_do_stat = 188,                  /* do_stat  */
  YYSYMBOL_repetitor = 189,                /* repetitor  */
  YYSYMBOL_nvir = 190,                     /* nvir  */
  YYSYMBOL_naddr_with = 191,               /* naddr_with  */
  YYSYMBOL_192_6 = 192,                    /* @6  */
  YYSYMBOL_addr_with = 193,                /* addr_with  */
  YYSYMBOL_connection = 194,               /* connection  */
  YYSYMBOL_inputstmts = 195,               /* inputstmts  */
  YYSYMBOL_outputstmts = 196,              /* outputstmts  */
  YYSYMBOL_errorstmts = 197,               /* errorstmts  */
  YYSYMBOL_adeo = 198,                     /* adeo  */
  YYSYMBOL_adei = 199,                     /* adei  */
  YYSYMBOL_adio = 200,                     /* adio  */
  YYSYMBOL_inputstmt = 201,                /* inputstmt  */
  YYSYMBOL_outputstmt = 202,               /* outputstmt  */
  YYSYMBOL_errorstmt = 203,                /* errorstmt  */
  YYSYMBOL_resourcei = 204,                /* resourcei  */
  YYSYMBOL_resourceo = 205,                /* resourceo  */
  YYSYMBOL_resources = 206,                /* resources  */
  YYSYMBOL_nsimsymb = 207,                 /* nsimsymb  */
  YYSYMBOL_208_7 = 208,                    /* $@7  */
  YYSYMBOL_nnvir = 209,                    /* nnvir  */
  YYSYMBOL_210_8 = 210,                    /* $@8  */
  YYSYMBOL_nspace = 211,                   /* nspace  */
  YYSYMBOL_addrAll = 212,                  /* addrAll  */
  YYSYMBOL_addrSim = 213,                  /* addrSim  */
  YYSYMBOL_addrString = 214,               /* addrString  */
  YYSYMBOL_dovar = 215,                    /* dovar  */
  YYSYMBOL_tobyfor = 216,                  /* tobyfor  */
  YYSYMBOL_conditional = 217,              /* conditional  */
  YYSYMBOL_drop_stat = 218,                /* drop_stat  */
  YYSYMBOL_upper_stat = 219,               /* upper_stat  */
  YYSYMBOL_exit_stat = 220,                /* exit_stat  */
  YYSYMBOL_if_stat = 221,                  /* if_stat  */
  YYSYMBOL_unexp_then = 222,               /* unexp_then  */
  YYSYMBOL_unexp_else = 223,               /* unexp_else  */
  YYSYMBOL_ipret_stat = 224,               /* ipret_stat  */
  YYSYMBOL_iterate_stat = 225,             /* iterate_stat  */
  YYSYMBOL_label_stat = 226,               /* label_stat  */
  YYSYMBOL_labelname = 227,                /* labelname  */
  YYSYMBOL_leave_stat = 228,               /* leave_stat  */
  YYSYMBOL_nop_stat = 229,                 /* nop_stat  */
  YYSYMBOL_numeric_stat = 230,             /* numeric_stat  */
  YYSYMBOL_form_expr = 231,                /* form_expr  */
  YYSYMBOL_options_stat = 232,             /* options_stat  */
  YYSYMBOL_parse_stat = 233,               /* parse_stat  */
  YYSYMBOL_parse_flags = 234,              /* parse_flags  */
  YYSYMBOL_templs = 235,                   /* templs  */
  YYSYMBOL_parse_param = 236,              /* parse_param  */
  YYSYMBOL_proc_stat = 237,                /* proc_stat  */
  YYSYMBOL_pull_stat = 238,                /* pull_stat  */
  YYSYMBOL_push_stat = 239,                /* push_stat  */
  YYSYMBOL_queue_stat = 240,               /* queue_stat  */
  YYSYMBOL_say_stat = 241,                 /* say_stat  */
  YYSYMBOL_return_stat = 242,              /* return_stat  */
  YYSYMBOL_sel_end = 243,                  /* sel_end  */
  YYSYMBOL_select_stat = 244,              /* select_stat  */
  YYSYMBOL_when_stats = 245,               /* when_stats  */
  YYSYMBOL_when_stat = 246,                /* when_stat  */
  YYSYMBOL_ex_when_stat = 247,             /* ex_when_stat  */
  YYSYMBOL_otherwise_stat = 248,           /* otherwise_stat  */
  YYSYMBOL_signal_stat = 249,              /* signal_stat  */
  YYSYMBOL_signal_name = 250,              /* signal_name  */
  YYSYMBOL_namespec = 251,                 /* namespec  */
  YYSYMBOL_asymbol = 252,                  /* asymbol  */
  YYSYMBOL_on = 253,                       /* on  */
  YYSYMBOL_off = 254,                      /* off  */
  YYSYMBOL_c_action = 255,                 /* c_action  */
  YYSYMBOL_s_action = 256,                 /* s_action  */
  YYSYMBOL_trace_stat = 257,               /* trace_stat  */
  YYSYMBOL_whatever = 258,                 /* whatever  */
  YYSYMBOL_assignment = 259,               /* assignment  */
  YYSYMBOL_ass_part = 260,                 /* ass_part  */
  YYSYMBOL_plusassignment = 261,           /* plusassignment  */
  YYSYMBOL_plus_ass_part = 262,            /* plus_ass_part  */
  YYSYMBOL_minusassignment = 263,          /* minusassignment  */
  YYSYMBOL_minus_ass_part = 264,           /* minus_ass_part  */
  YYSYMBOL_multassignment = 265,           /* multassignment  */
  YYSYMBOL_mult_ass_part = 266,            /* mult_ass_part  */
  YYSYMBOL_divassignment = 267,            /* divassignment  */
  YYSYMBOL_div_ass_part = 268,             /* div_ass_part  */
  YYSYMBOL_intdivassignment = 269,         /* intdivassignment  */
  YYSYMBOL_intdiv_ass_part = 270,          /* intdiv_ass_part  */
  YYSYMBOL_modulusassignment = 271,        /* modulusassignment  */
  YYSYMBOL_modulus_ass_part = 272,         /* modulus_ass_part  */
  YYSYMBOL_andassignment = 273,            /* andassignment  */
  YYSYMBOL_and_ass_part = 274,             /* and_ass_part  */
  YYSYMBOL_xorassignment = 275,            /* xorassignment  */
  YYSYMBOL_xor_ass_part = 276,             /* xor_ass_part  */
  YYSYMBOL_orassignment = 277,             /* orassignment  */
  YYSYMBOL_or_ass_part = 278,              /* or_ass_part  */
  YYSYMBOL_concatassignment = 279,         /* concatassignment  */
  YYSYMBOL_concat_ass_part = 280,          /* concat_ass_part  */
  YYSYMBOL_expr = 281,                     /* expr  */
  YYSYMBOL_282_9 = 282,                    /* $@9  */
  YYSYMBOL_283_10 = 283,                   /* $@10  */
  YYSYMBOL_284_11 = 284,                   /* $@11  */
  YYSYMBOL_285_12 = 285,                   /* $@12  */
  YYSYMBOL_286_13 = 286,                   /* $@13  */
  YYSYMBOL_287_14 = 287,                   /* $@14  */
  YYSYMBOL_288_15 = 288,                   /* $@15  */
  YYSYMBOL_289_16 = 289,                   /* $@16  */
  YYSYMBOL_290_17 = 290,                   /* $@17  */
  YYSYMBOL_291_18 = 291,                   /* $@18  */
  YYSYMBOL_292_19 = 292,                   /* $@19  */
  YYSYMBOL_293_20 = 293,                   /* $@20  */
  YYSYMBOL_294_21 = 294,                   /* $@21  */
  YYSYMBOL_295_22 = 295,                   /* $@22  */
  YYSYMBOL_296_23 = 296,                   /* $@23  */
  YYSYMBOL_297_24 = 297,                   /* $@24  */
  YYSYMBOL_298_25 = 298,                   /* $@25  */
  YYSYMBOL_299_26 = 299,                   /* $@26  */
  YYSYMBOL_300_27 = 300,                   /* $@27  */
  YYSYMBOL_301_28 = 301,                   /* $@28  */
  YYSYMBOL_302_29 = 302,                   /* $@29  */
  YYSYMBOL_303_30 = 303,                   /* $@30  */
  YYSYMBOL_304_31 = 304,                   /* $@31  */
  YYSYMBOL_305_32 = 305,                   /* $@32  */
  YYSYMBOL_306_33 = 306,                   /* $@33  */
  YYSYMBOL_307_34 = 307,                   /* $@34  */
  YYSYMBOL_308_35 = 308,                   /* $@35  */
  YYSYMBOL_exprs_sub = 309,                /* exprs_sub  */
  YYSYMBOL_symbtree = 310,                 /* symbtree  */
  YYSYMBOL_function = 311,                 /* function  */
  YYSYMBOL_func_args = 312,                /* func_args  */
  YYSYMBOL_313_36 = 313,                   /* @36  */
  YYSYMBOL_intfunc = 314,                  /* intfunc  */
  YYSYMBOL_extfunc = 315,                  /* extfunc  */
  YYSYMBOL_template = 316,                 /* template  */
  YYSYMBOL_solid = 317,                    /* solid  */
  YYSYMBOL_offset = 318,                   /* offset  */
  YYSYMBOL_string = 319,                   /* string  */
  YYSYMBOL_pv = 320,                       /* pv  */
  YYSYMBOL_exprs = 321,                    /* exprs  */
  YYSYMBOL_322_37 = 322,                   /* $@37  */
  YYSYMBOL_nexpr = 323,                    /* nexpr  */
  YYSYMBOL_anyvars = 324,                  /* anyvars  */
  YYSYMBOL_xsimsymb = 325,                 /* xsimsymb  */
  YYSYMBOL_simsymb = 326                   /* simsymb  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2307

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  131
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  196
/* YYNRULES -- Number of rules.  */
#define YYNRULES  479
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  654

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   374


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,   125,   118,     2,
     128,   129,   123,   121,   130,   122,     2,   124,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   119,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,   117,     2,     2,     2,     2,     2,
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
     115,   116,   120,   126,   127
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   160,   160,   160,   170,   173,   179,   181,   184,   186,
     189,   190,   193,   198,   199,   202,   203,   206,   207,   210,
     211,   214,   215,   218,   222,   225,   226,   227,   228,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   246,   247,   248,   249,   250,   251,
     252,   253,   254,   255,   256,   257,   258,   259,   260,   261,
     262,   263,   264,   265,   268,   272,   277,   281,   286,   290,
     294,   298,   302,   306,   310,   314,   318,   322,   326,   330,
     334,   338,   342,   346,   350,   354,   359,   363,   367,   371,
     375,   379,   379,   383,   386,   387,   388,   388,   390,   394,
     400,   406,   406,   411,   411,   417,   418,   419,   420,   421,
     427,   432,   433,   439,   440,   443,   451,   454,   460,   466,
     468,   469,   472,   493,   497,   501,   505,   508,   509,   510,
     512,   515,   516,   517,   520,   520,   526,   529,   530,   531,
     534,   535,   536,   537,   540,   544,   548,   552,   553,   554,
     555,   556,   559,   560,   561,   562,   563,   566,   567,   568,
     569,   570,   573,   574,   577,   578,   581,   582,   585,   586,
     588,   589,   594,   595,   597,   598,   599,   600,   605,   613,
     614,   627,   628,   632,   633,   637,   640,   640,   644,   644,
     648,   649,   652,   653,   656,   664,   672,   675,   676,   677,
     680,   681,   682,   685,   686,   690,   691,   695,   699,   707,
     717,   719,   721,   722,   725,   728,   731,   736,   738,   739,
     742,   750,   754,   756,   757,   760,   761,   764,   767,   768,
     769,   772,   774,   776,   777,   778,   781,   784,   785,   788,
     791,   797,   803,   804,   807,   809,   811,   813,   815,   817,
     819,   823,   825,   828,   829,   830,   831,   832,   833,   834,
     836,   837,   840,   841,   842,   843,   844,   848,   852,   856,
     860,   864,   868,   869,   870,   873,   878,   879,   881,   882,
     883,   887,   888,   889,   892,   898,   900,   901,   911,   912,
     915,   919,   925,   928,   929,   931,   932,   933,   934,   935,
     941,   945,   946,   952,   953,   954,   957,   958,   959,   962,
     963,   966,   969,   972,   973,   974,   975,   978,   979,   980,
     981,   984,   986,   988,   989,   993,   997,  1013,  1019,  1026,
    1032,  1039,  1045,  1052,  1058,  1065,  1071,  1078,  1084,  1091,
    1097,  1104,  1110,  1117,  1123,  1130,  1136,  1141,  1148,  1148,
    1181,  1181,  1184,  1184,  1187,  1187,  1190,  1191,  1191,  1194,
    1195,  1195,  1198,  1199,  1199,  1202,  1203,  1203,  1206,  1207,
    1207,  1210,  1211,  1211,  1214,  1215,  1215,  1218,  1219,  1219,
    1222,  1223,  1223,  1226,  1227,  1229,  1230,  1230,  1234,  1235,
    1235,  1239,  1240,  1240,  1244,  1245,  1245,  1249,  1250,  1250,
    1254,  1255,  1255,  1259,  1260,  1260,  1263,  1264,  1264,  1267,
    1268,  1268,  1271,  1272,  1272,  1275,  1276,  1276,  1279,  1280,
    1280,  1283,  1284,  1284,  1287,  1288,  1288,  1291,  1292,  1293,
    1295,  1299,  1303,  1305,  1306,  1308,  1310,  1311,  1314,  1315,
    1316,  1319,  1323,  1325,  1329,  1329,  1336,  1339,  1342,  1343,
    1344,  1347,  1349,  1351,  1353,  1355,  1357,  1359,  1361,  1363,
    1367,  1368,  1377,  1378,  1382,  1388,  1389,  1390,  1400,  1400,
    1480,  1524,  1525,  1528,  1530,  1532,  1534,  1538,  1541,  1542
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ADDRESS", "ARG",
  "CALL", "DO", "TO", "BY", "FOR", "WHILE", "UNTIL", "EXIT", "IF", "THEN",
  "ELSE", "ITERATE", "INTERPRET", "LEAVE", "NOP", "NUMERIC", "PARSE",
  "EXTERNAL", "SOURCE", "VAR", "VALUE", "WITH", "PROCEDURE", "EXPOSE",
  "PULL", "PUSH", "QUEUE", "SAY", "RETURN", "SELECT", "WHEN", "DROP",
  "OTHERWISE", "SIGNAL", "ON", "OFF", "SYNTAX", "HALT", "NOVALUE", "TRACE",
  "END", "UPPER", "ASSIGNMENTVARIABLE", "STATSEP", "FOREVER", "DIGITS",
  "FORM", "FUZZ", "SCIENTIFIC", "ENGINEERING", "NOT", "CONCATENATE",
  "MODULUS", "GTE", "GT", "LTE", "LT", "DIFFERENT", "EQUALEQUAL",
  "NOTEQUALEQUAL", "OFFSET", "SPACE", "EXP", "XOR", "PLACEHOLDER",
  "NOTREADY", "CONSYMBOL", "SIMSYMBOL", "EXFUNCNAME", "INFUNCNAME",
  "LABEL", "DOVARIABLE", "HEXSTRING", "STRING", "VERSION", "LINEIN",
  "WHATEVER", "NAME", "FAILURE", "BINSTRING", "OPTIONS", "ENVIRONMENT",
  "LOSTDIGITS", "GTGT", "LTLT", "NOTGTGT", "NOTLTLT", "GTGTE", "LTLTE",
  "INPUT", "OUTPUT", "ERROR", "NORMAL", "APPEND", "REPLACE", "STREAM",
  "STEM", "LIFO", "FIFO", "NOEOL", "LOWER", "CASELESS",
  "PLUSASSIGNMENTVARIABLE", "MINUSASSIGNMENTVARIABLE",
  "MULTASSIGNMENTVARIABLE", "DIVASSIGNMENTVARIABLE",
  "MODULUSASSIGNMENTVARIABLE", "INTDIVASSIGNMENTVARIABLE",
  "ORASSIGNMENTVARIABLE", "XORASSIGNMENTVARIABLE", "ANDASSIGNMENTVARIABLE",
  "CONCATASSIGNMENTVARIABLE", "'|'", "'&'", "'='", "CCAT", "'+'", "'-'",
  "'*'", "'/'", "'%'", "UMINUS", "UPLUS", "'('", "')'", "','", "$accept",
  "start", "$@1", "prog", "stats", "xstats", "ystatement", "lonely_end",
  "nxstats", "ncl", "nlncl", "optLabels", "statement", "gruff",
  "mttstatement", "mtstatement", "nclstatement", "call", "do", "exit",
  "if", "iterate", "leave", "say", "return", "address", "arg", "drop",
  "interpret", "label", "nop", "numeric", "options", "parse", "proc",
  "pull", "push", "queue", "select", "signal", "when", "otherwise",
  "trace", "upper", "address_stat", "@2", "address_stat2", "$@3",
  "arg_stat", "call_stat", "$@4", "$@5", "call_name", "call_args",
  "expr_stat", "end_stat", "end", "do_stat", "repetitor", "nvir",
  "naddr_with", "@6", "addr_with", "connection", "inputstmts",
  "outputstmts", "errorstmts", "adeo", "adei", "adio", "inputstmt",
  "outputstmt", "errorstmt", "resourcei", "resourceo", "resources",
  "nsimsymb", "$@7", "nnvir", "$@8", "nspace", "addrAll", "addrSim",
  "addrString", "dovar", "tobyfor", "conditional", "drop_stat",
  "upper_stat", "exit_stat", "if_stat", "unexp_then", "unexp_else",
  "ipret_stat", "iterate_stat", "label_stat", "labelname", "leave_stat",
  "nop_stat", "numeric_stat", "form_expr", "options_stat", "parse_stat",
  "parse_flags", "templs", "parse_param", "proc_stat", "pull_stat",
  "push_stat", "queue_stat", "say_stat", "return_stat", "sel_end",
  "select_stat", "when_stats", "when_stat", "ex_when_stat",
  "otherwise_stat", "signal_stat", "signal_name", "namespec", "asymbol",
  "on", "off", "c_action", "s_action", "trace_stat", "whatever",
  "assignment", "ass_part", "plusassignment", "plus_ass_part",
  "minusassignment", "minus_ass_part", "multassignment", "mult_ass_part",
  "divassignment", "div_ass_part", "intdivassignment", "intdiv_ass_part",
  "modulusassignment", "modulus_ass_part", "andassignment", "and_ass_part",
  "xorassignment", "xor_ass_part", "orassignment", "or_ass_part",
  "concatassignment", "concat_ass_part", "expr", "$@9", "$@10", "$@11",
  "$@12", "$@13", "$@14", "$@15", "$@16", "$@17", "$@18", "$@19", "$@20",
  "$@21", "$@22", "$@23", "$@24", "$@25", "$@26", "$@27", "$@28", "$@29",
  "$@30", "$@31", "$@32", "$@33", "$@34", "$@35", "exprs_sub", "symbtree",
  "function", "func_args", "@36", "intfunc", "extfunc", "template",
  "solid", "offset", "string", "pv", "exprs", "$@37", "nexpr", "anyvars",
  "xsimsymb", "simsymb", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
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
     365,   366,   367,   368,   369,   370,   371,   124,    38,    61,
     372,    43,    45,    42,    47,    37,   373,   374,    40,    41,
      44
};
#endif

#define YYPACT_NINF (-532)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-473)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -532,    38,  -532,  -532,  -532,    60,    91,    86,  -532,  -532,
    -532,  1240,  -532,  -532,  -532,  -532,    -4,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,   462,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,   966,  1040,
    -532,  -532,  -532,  -532,   207,  -532,    92,   267,  1868,  1985,
    1713,    25,    84,  1985,  1985,  -532,    83,   -38,  1985,    70,
     328,  1985,   195,    28,   109,  1985,  1985,   136,   420,  -532,
    -532,  1566,   -38,  -532,  -532,  -532,  -532,   247,    92,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  1985,  -532,  1985,  -532,  1985,  -532,  1985,
    -532,  1985,  -532,  1985,  -532,  1985,  -532,  1985,  -532,  1985,
    -532,  1985,  -532,  1985,  2068,  -532,  -532,  -532,  -532,    90,
    -532,  -532,  -532,  -532,  -532,  -532,  1791,  -532,  -532,    -4,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
      94,   105,  -532,   376,  -532,   379,    49,  2106,  2068,  -532,
      -4,  2068,  -532,  -532,   194,   206,  -532,  -532,   137,  -532,
     -11,    29,   -11,  -532,  1595,  -532,   101,   213,   -38,  2068,
    -532,  -532,  1985,   209,  1985,  -532,  -532,  -532,  -532,  -532,
      32,   892,  -532,    77,  -532,  -532,   110,   -24,   471,    83,
    -532,    30,  -532,  -532,  -532,  -532,  1034,  -532,  1985,  -532,
     225,  -532,   285,   436,  1985,  -532,   228,  -532,   231,   270,
      -4,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  1791,
    -532,    90,  -532,  -532,    24,    46,  1985,  -532,  -532,  -532,
    -532,  -532,    26,  -532,   321,  1985,  -532,  -532,  -532,  1985,
    1985,    92,  1985,   474,  -532,    41,  -532,  -532,  -532,  1985,
      48,  -532,  -532,  -532,  1985,  -532,  1985,  -532,  -532,    83,
    -532,  -532,  -532,   -41,   -30,    -3,   120,    83,  -532,  -532,
      98,  -532,  -532,  2068,  -532,  1985,  -532,  -532,   323,  2068,
    -532,  -532,   214,  -532,  -532,  -532,  -532,  -532,    83,    29,
    -532,   340,  -532,  -532,   251,  1114,   259,    40,  -532,  2068,
    -532,  -532,  -532,  -532,  -532,  -532,    44,  -532,   391,  2068,
    -532,  -532,  -532,  1985,  1985,  1985,  1985,  1985,  1985,  1985,
    1985,  1985,  1985,  1985,  1985,  1985,  1985,  1985,  1985,  1985,
    1985,  1985,  1985,  1985,  1985,  1985,  1985,  1985,  1985,  -532,
    -532,  -532,  -532,  -532,   135,  -532,    56,   442,  -532,  -532,
    2106,  2106,    -4,  2106,  -532,  -532,   219,  1914,  -532,   454,
    -532,  -532,  -532,   266,   266,   266,   -29,  2026,   219,  -532,
     120,  -532,   120,  -532,   120,  -532,   221,  -532,   -38,  2068,
    -532,  -532,    29,  -532,  -532,  -532,  1641,  -532,   566,  -532,
     280,  -532,   461,  -532,   440,   286,   939,   939,   939,   939,
     939,   939,   939,   440,  -532,  2144,   939,   939,   939,   939,
     939,   939,  2144,  2182,   939,   390,   390,   286,   286,   286,
    1985,  -532,  -532,  -532,  -532,  -532,  -532,  -532,   318,   320,
    -532,  1366,   496,   692,  -532,   341,  -532,  -532,  -532,   266,
     266,   140,  -532,   266,   266,   104,  -532,   266,   266,   288,
     181,   190,   211,   266,  -532,   241,   245,   256,  -532,  -532,
     406,  -532,  -532,   268,  -532,  -532,  -532,  -532,  1492,  -532,
    -532,  1985,  1985,  1985,   496,  -532,   378,  -532,   266,   345,
     266,   353,   266,   345,   266,   359,   266,   353,   266,   359,
    -532,   257,  -532,   428,  -532,   428,   219,  -532,  -532,  -532,
    -532,  -532,   472,  2106,  2106,  2106,   496,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,   265,    22,   372,   374,    69,
    -532,  -532,  -532,   196,   202,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,   818,  -532,  -532,   266,  -532,
    -532,   266,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,   450,  -532,  -532,   166,   101,  -532,  -532,  -532,
    -532,  -532,  -532,  -532
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       2,     0,    20,     1,     3,    23,    18,    23,     7,    11,
      10,    23,    21,    22,    20,    76,    17,   221,    19,   220,
       6,    72,    73,    64,    65,    66,    67,    68,    75,    69,
      77,    78,    80,    81,    82,    83,    84,    70,    71,    85,
      87,    74,    88,    86,    89,   118,    90,   327,   385,   383,
     362,   391,   397,   394,   400,   403,   406,   409,   380,   377,
     374,   429,   441,   447,   446,   430,   432,   431,    79,   412,
     415,   418,   421,   424,   427,   329,   331,   333,   335,   339,
     337,   345,   343,   341,   347,   368,   371,   388,     0,     0,
     356,   359,   365,   348,     0,    24,     0,     0,   130,   472,
       0,     0,     0,   472,   472,    91,     0,     0,     0,     0,
       0,   472,     0,     0,     0,   472,   472,     0,     0,   288,
     289,   472,     0,    29,    31,    32,    30,     0,     0,    33,
      34,    52,    35,    26,    27,    28,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    48,    47,    49,
      50,    51,    63,   472,    53,   472,    54,   472,    55,   472,
      56,   472,    57,   472,    58,   472,    61,   472,    59,   472,
      60,   472,    62,   472,   117,   428,   433,   444,   444,    16,
      20,   384,   436,   434,   437,   435,   472,   214,   215,    25,
     114,   311,   312,   309,   310,   463,   462,   464,   101,   113,
       0,     0,   103,     0,   196,   202,     0,   191,   471,   207,
     212,    20,   479,   478,     0,     0,   270,   271,     0,   450,
     467,   100,   467,   252,   449,   477,     0,     0,   474,   216,
     226,   236,   228,     0,   234,   239,   243,   253,   255,   258,
       0,     0,   257,   244,   256,   254,   247,   250,     0,     0,
     263,     0,   267,   268,   269,   278,     0,   305,     0,   304,
       0,   303,     0,     0,     0,   325,     0,   322,     0,     0,
      12,   326,   328,   330,   332,   334,   336,   338,   340,   342,
     344,   346,   381,   360,   389,   395,   392,   398,   401,   404,
     407,   378,   375,   372,   410,   413,   416,   419,   422,   425,
     366,   369,   386,   350,   352,   354,   357,   363,   443,   472,
     442,    15,   440,   349,     0,   470,   472,   105,   314,   315,
     316,   313,     0,   106,     0,   472,   128,   190,   127,     0,
       0,     0,     0,   378,   129,     0,   218,   223,    96,     0,
       0,   131,   132,   133,     0,    92,   472,    94,   465,     0,
     466,   460,   461,     0,     0,     0,     0,     0,   453,   459,
       0,   203,   473,   227,   233,     0,   237,   238,     0,   235,
     259,   261,     0,   245,   248,   246,   249,   242,     0,   241,
     264,     0,   283,   279,     0,     0,     0,   291,   282,   292,
     293,   295,   319,   318,   320,   317,     0,   296,     0,   321,
     323,   205,   121,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   445,
     439,   438,   468,   102,   115,   107,     0,     0,   111,   104,
     191,   191,    23,   191,   213,    20,   134,   134,   143,     0,
     140,   141,   142,   191,   191,   191,   139,     0,   134,   251,
       0,   454,     0,   452,     0,   451,     0,   448,   476,   232,
     229,   260,   240,   265,   276,   287,    20,   277,     0,   281,
       0,   297,     0,   301,   382,   361,   390,   396,   393,   399,
     402,   405,   408,   379,   376,   373,   411,   414,   417,   420,
     423,   426,   367,   370,   387,   351,   353,   355,   358,   364,
     472,   116,   308,   306,   307,   108,   200,   201,    23,     0,
      23,    23,   126,     0,    97,     0,    93,   138,   144,   191,
     191,   151,   145,   191,   191,   156,   146,   191,   191,   161,
       0,     0,     0,   191,    98,     0,     0,     0,   455,   475,
       0,   280,    23,     0,   275,   298,   469,     8,    23,   122,
       9,     0,     0,     0,   125,   211,   208,   135,   191,   147,
     191,   149,   191,   152,   191,   154,   191,   157,   191,   159,
     163,     0,   165,     0,   167,     0,   134,   458,   457,   456,
      20,   290,     0,   191,   191,   191,   124,    20,   148,   150,
     153,   155,   158,   160,   171,     0,     0,     0,     0,     0,
     162,   168,   177,     0,     0,   164,   172,   166,    99,    23,
     273,   197,   199,   198,   123,     0,   179,   178,   191,   181,
     180,   191,   183,   182,   185,   184,   170,   169,   174,   173,
     176,   175,   284,   210,   209,     0,     0,   285,   189,   192,
     193,   195,   194,   187
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -532,  -532,  -532,  -532,  -532,  -532,    -7,  -532,   -86,   -85,
    -206,    -1,  -425,   -10,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -242,  -237,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,   143,  -532,  -532,   -49,  -532,  -532,  -532,
    -440,  -532,   -51,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -418,  -407,  -400,  -532,  -109,  -531,  -532,  -532,   -70,  -532,
    -107,  -532,  -166,  -532,  -532,  -524,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -229,   253,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,   102,  -532,  -532,  -532,  -532,
     103,   400,   423,   427,   343,   284,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
      -9,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,
    -532,  -532,  -532,  -532,  -532,  -532,  -532,  -532,   246,   -98,
    -532,   381,  -532,  -532,  -532,  -104,  -532,   203,  -220,    95,
    -304,  -532,   -52,  -116,  -217,   -99
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     1,     2,     4,     7,   518,     8,     9,   519,    16,
       5,     6,    10,    11,    12,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,    17,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   218,   345,   446,   124,   125,
     316,   325,   198,   433,   126,   127,   128,   129,   205,   346,
     524,   525,   347,   449,   450,   451,   452,   528,   532,   536,
     453,   454,   455,   610,   615,   616,   630,   631,   627,   628,
     328,   648,   649,   650,   206,   564,   331,   130,   131,   132,
     133,   134,   135,   136,   137,    18,    19,   138,   139,   140,
     368,   141,   142,   248,   221,   249,   143,   144,   145,   146,
     147,   148,   554,   149,   387,   388,    13,   480,   150,   260,
     437,   199,   200,   201,   395,   396,   151,   266,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     208,   186,   424,   425,   426,   427,   404,   428,   421,   422,
     414,   413,   412,   403,   423,   405,   407,   406,   408,   409,
     410,   411,   415,   416,   417,   418,   419,   420,   313,   175,
     176,   308,   309,   177,   178,   223,   357,   358,   202,   224,
     314,   510,   315,   227,   228,   214
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      20,    94,   174,   215,   359,   335,   268,   526,   222,   360,
     252,   189,   434,   179,   385,   210,   222,   520,   544,   386,
     379,   434,   375,   629,   351,   430,   212,   435,   269,   250,
     352,   380,   256,   212,   225,   351,   533,   537,     3,   181,
     596,   352,   444,   270,   180,   481,   529,   209,   538,   448,
     611,   216,   217,   530,   534,   445,   251,   512,   220,   235,
      -5,    62,   351,   253,   254,   540,   541,   542,   352,   267,
     636,   230,   624,  -219,  -110,    40,  -262,    42,   637,   183,
     185,   376,   639,   641,   219,   212,    -4,   460,  -186,   207,
     226,   211,  -300,   557,  -186,   317,  -191,   213,   462,   229,
     334,   271,   225,   272,   213,   273,   323,   274,   436,   275,
     219,   276,   362,   277,   327,   278,   574,   279,  -225,   280,
     578,   281,   222,   570,   222,   464,   436,   520,   513,   568,
     576,  -467,  -224,   572,   514,   381,   318,   255,   338,    14,
      14,   370,  -191,  -191,  -191,   385,   618,   318,  -467,   472,
     478,   222,   220,   431,  -467,    62,   213,  -467,   226,   349,
    -467,  -467,   339,   340,   319,    15,    15,  -467,   332,   605,
     606,   607,   608,   225,  -467,   319,   432,   320,   220,   311,
    -467,    62,   580,   373,    14,   -95,  -467,  -467,   320,   372,
     321,   582,    62,  -467,   642,   336,   236,   638,   540,   237,
     542,   321,  -467,   640,  -467,  -467,   556,   337,   341,   342,
     364,  -467,   584,  -467,   361,   343,   374,   238,   239,   240,
     241,   187,   188,   363,   242,   369,   390,   468,  -467,   400,
    -467,  -467,   401,   456,   365,   541,   542,  -467,   225,   523,
     471,   243,  -217,   195,   196,   459,   442,   327,   212,   389,
     197,   222,   474,   467,  -222,   399,   327,  -231,   466,   222,
     477,  -204,   366,   367,   511,   344,   626,  -136,   190,   212,
     550,   402,   552,  -294,   244,   245,  -324,   327,  -191,  -206,
     222,  -191,  -191,  -191,  -191,  -191,   391,  -191,  -191,  -191,
    -191,  -191,  -191,  -191,   458,  -120,   605,   606,   607,   608,
     246,   247,   605,   606,   607,   608,   191,   192,  -191,  -191,
    -191,  -191,  -191,  -191,  -191,   348,  -274,   350,  -119,   213,
     440,   441,   438,   443,   470,   553,   392,   318,   393,   231,
     447,  -188,   327,   516,   517,   457,   522,  -188,   193,   194,
     213,   473,  -188,  -188,   195,   196,   531,   535,   539,  -188,
     548,   197,   549,   292,   604,   319,   469,   605,   606,   607,
     608,   609,   545,   -13,   546,    45,   547,   340,   320,  -112,
     587,  -230,   394,   632,   588,   634,   476,   326,   232,   233,
     234,   321,   540,   541,   619,   589,  -191,  -191,  -266,   329,
     330,   625,   483,   597,   484,   485,   486,   487,   488,   489,
     490,   491,   492,   493,   494,   495,   496,   497,   498,   499,
     500,   501,   502,   503,   504,   505,   506,   507,   508,   509,
     590,   257,   569,   571,  -191,   651,   573,   575,   652,   652,
     577,   579,   521,   581,   583,   585,   586,   397,  -188,  -302,
    -188,   542,   327,   515,  -188,   258,  -188,   283,   541,  -188,
    -188,  -188,  -188,   540,   592,   527,  -188,   292,  -188,   191,
     192,   598,   555,   599,   647,   600,   591,   601,   439,   602,
     559,   603,   377,   620,   567,   237,   617,   392,   318,   393,
     653,  -190,  -190,  -190,  -190,  -190,   621,   622,   623,   479,
    -109,   193,   194,   238,   239,   240,   241,   283,   259,   482,
     242,   378,  -137,   561,   562,   563,   319,   292,   558,  -299,
     560,    94,   174,   305,   306,   307,   566,    48,   261,   320,
    -272,   645,  -190,   394,   646,   612,   613,   614,   605,   606,
     607,   608,   321,    61,    62,    63,    64,   633,   635,    65,
      66,   262,   521,   322,   324,   263,    67,   398,    94,   174,
     244,   245,   593,   594,   595,   429,   461,   463,   465,   310,
       0,   303,   304,   305,   306,   307,     0,   551,     0,   -20,
     -20,   -20,   -20,     0,     0,     0,     0,     0,   -20,   -20,
     -20,   -20,   -20,   -20,   -20,   -20,   -20,   -20,     0,     0,
      93,     0,     0,   -20,     0,   -20,   -20,   -20,   -20,   -20,
     -20,   -20,   -20,   -20,   -20,     0,     0,     0,     0,   558,
     -20,   -20,   -20,   -20,   -20,     0,     0,     0,   644,     0,
       0,   -20,   -20,   -20,   -20,   -20,   -20,   -20,   -20,   -20,
     -20,     0,   -20,   -20,   -20,     0,     0,   -20,   -20,   -20,
     -20,   -20,     0,   -20,   -20,     0,     0,     0,     0,     0,
     -20,   -20,     0,     0,   -20,   -20,   -20,   -20,   -20,   -20,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   -20,   -20,   -20,   -20,   -20,   -20,   -20,
     -20,   -20,   -20,   -20,   -20,   -20,     0,   -20,   -20,   -20,
     -20,   -20,     0,   565,   -20,   -23,   -23,   -23,   -23,     0,
       0,     0,     0,     0,   -23,   -23,   -23,   -23,   -23,   -23,
     -23,   -23,   -23,   -23,     0,     0,     0,     0,     0,   -23,
       0,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,
     -23,     0,     0,     0,     0,     0,   -23,   -23,   -23,   -23,
       0,     0,     0,     0,     0,     0,     0,   -23,   -23,   -23,
     -23,   -23,   -23,   -23,   -23,   -23,   -23,     0,   -23,   -23,
     -23,     0,     0,   -23,   -23,   -23,   -23,     0,     0,   -23,
     -23,     0,     0,     0,     0,     0,   -23,   -23,     0,     0,
     -23,   -23,   -23,   -23,   -23,   -23,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   -23,
     -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,
     -23,   -23,     0,   -23,   -23,   -23,   -23,   -23,     0,   643,
     -23,   -23,   -23,   -23,   -23,     0,     0,     0,     0,     0,
     -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,
       0,     0,     0,     0,     0,   -23,     0,   -23,   -23,   -23,
     -23,   -23,   -23,   -23,   -23,   -23,   -23,     0,     0,     0,
       0,     0,   -23,   -23,   -23,   -23,     0,     0,     0,     0,
       0,     0,     0,   -23,   -23,   -23,   -23,   -23,   -23,   -23,
     -23,   -23,   -23,     0,   -23,   -23,   -23,     0,     0,   -23,
     -23,   -23,   -23,   371,     0,   -23,   -23,     0,     0,     0,
       0,     0,   -23,   -23,     0,     0,   -23,   -23,   -23,   -23,
     -23,   -23,     0,     0,     0,     0,     0,     0,  -472,     0,
       0,     0,     0,     0,     0,   -23,   -23,   -23,   -23,   -23,
     -23,   -23,   -23,   -23,   -23,   -23,   -23,   -23,     0,   -23,
     -23,   -23,   -23,   -23,     0,     0,   -23,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,     0,    58,    59,
      60,     0,     0,    61,    62,    63,    64,   182,     0,    65,
      66,     0,     0,     0,     0,     0,    67,     0,     0,     0,
      69,    70,    71,    72,    73,    74,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   282,   283,     0,     0,     0,
       0,     0,     0,     0,     0,   291,   292,     0,     0,    85,
      86,    87,     0,    88,    89,    90,    91,    92,     0,     0,
      93,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,     0,    58,    59,    60,   382,     0,    61,    62,    63,
      64,   184,     0,    65,    66,     0,     0,     0,   383,     0,
      67,     0,     0,     0,    69,    70,    71,    72,    73,    74,
     303,   304,   305,   306,   307,     0,     0,     0,     0,    40,
       0,    42,     0,     0,     0,     0,     0,     0,     0,   384,
       0,     0,   180,    85,    86,    87,     0,    88,    89,    90,
      91,    92,     0,     0,    93,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,     0,    58,    59,    60,     0,
       0,    61,    62,    63,    64,   475,     0,    65,    66,     0,
       0,     0,     0,     0,    67,     0,     0,     0,    69,    70,
      71,    72,    73,    74,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    85,    86,    87,
       0,    88,    89,    90,    91,    92,     0,     0,    93,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,     0,
      58,    59,    60,     0,     0,    61,    62,    63,    64,     0,
       0,    65,    66,     0,     0,     0,     0,     0,    67,     0,
       0,     0,    69,    70,    71,    72,    73,    74,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    85,    86,    87,     0,    88,    89,    90,    91,    92,
       0,     0,    93,    21,    22,    23,    24,     0,     0,     0,
       0,     0,    25,    26,     0,     0,    27,    28,    29,    30,
      31,    32,     0,     0,     0,     0,     0,    33,     0,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,     0,
       0,     0,     0,     0,    44,    45,    46,    47,     0,     0,
       0,     0,     0,     0,     0,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,     0,    58,    59,    60,     0,
       0,    61,    62,    63,    64,     0,     0,    65,    66,     0,
       0,     0,     0,     0,    67,    68,     0,     0,    69,    70,
      71,    72,    73,    74,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
       0,    88,    89,    90,    91,    92,     0,     0,    93,    21,
      22,    23,    24,     0,     0,     0,     0,     0,    25,    26,
       0,     0,    27,    28,    29,    30,    31,    32,     0,     0,
       0,     0,     0,    33,     0,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,     0,     0,     0,     0,     0,
      44,   -14,    46,    47,     0,     0,     0,     0,     0,     0,
       0,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,     0,    58,    59,    60,     0,     0,    61,    62,    63,
      64,     0,     0,    65,    66,     0,     0,     0,     0,     0,
      67,    68,     0,     0,    69,    70,    71,    72,    73,    74,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,     0,    88,    89,    90,
      91,    92,     0,     0,    93,    21,    22,    23,    24,     0,
       0,     0,     0,     0,    25,    26,     0,     0,    27,    28,
      29,    30,    31,    32,     0,     0,     0,     0,     0,    33,
       0,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,     0,     0,     0,     0,     0,    44,     0,    46,    47,
       0,     0,     0,     0,     0,     0,     0,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,     0,    58,    59,
      60,     0,     0,    61,    62,    63,    64,     0,     0,    65,
      66,     0,     0,     0,     0,     0,    67,    68,     0,     0,
      69,    70,    71,    72,    73,    74,     0,     0,     0,     0,
       0,   264,     0,     0,     0,     0,     0,     0,     0,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,     0,    88,    89,    90,    91,    92,     0,     0,
      93,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,     0,    58,    59,    60,     0,     0,    61,    62,    63,
      64,     0,     0,    65,    66,     0,     0,   265,     0,     0,
      67,     0,     0,     0,    69,    70,    71,    72,    73,    74,
     351,     0,     0,     0,     0,     0,   352,     0,     0,     0,
       0,     0,   195,   196,     0,     0,  -286,     0,  -286,   197,
       0,     0,     0,    85,    86,    87,  -286,    88,    89,    90,
      91,    92,     0,     0,    93,     0,     0,   282,   283,   284,
     285,   286,   287,   288,   289,   290,     0,   291,   292,   293,
       0,     0,     0,     0,   353,     0,   354,   355,     0,     0,
       0,     0,     0,   356,     0,     0,     0,     0,     0,   294,
     295,   296,   297,   298,   299,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   300,   301,
     302,    14,   303,   304,   305,   306,   307,     0,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,     0,    58,
      59,    60,     0,     0,    61,    62,    63,    64,     0,     0,
      65,    66,     0,     0,     0,     0,     0,    67,     0,     0,
       0,    69,    70,    71,    72,    73,    74,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      85,    86,    87,     0,    88,    89,    90,    91,    92,   312,
       0,    93,     0,     0,     0,     0,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,     0,    58,    59,    60,
       0,     0,    61,    62,    63,    64,     0,     0,    65,    66,
       0,     0,     0,     0,     0,    67,     0,     0,     0,    69,
      70,    71,    72,    73,    74,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    85,    86,
      87,     0,    88,    89,    90,    91,    92,   203,     0,    93,
       0,     0,     0,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,     0,    58,    59,    60,     0,     0,    61,
      62,    63,    64,     0,   204,    65,    66,     0,     0,     0,
       0,     0,    67,     0,     0,     0,    69,    70,    71,    72,
      73,    74,  -136,     0,     0,     0,     0,     0,     0,     0,
     282,   283,   284,   285,   286,   287,   288,   289,   290,     0,
     291,   292,   293,     0,     0,    85,    86,    87,     0,    88,
      89,    90,    91,    92,     0,     0,    93,     0,     0,     0,
       0,     0,   294,   295,   296,   297,   298,   299,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   300,   301,   302,     0,   303,   304,   305,   306,   307,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
       0,    58,    59,    60,     0,     0,    61,    62,    63,    64,
       0,     0,    65,    66,     0,     0,     0,     0,     0,    67,
       0,     0,     0,    69,    70,    71,    72,    73,    74,     0,
       0,     0,   282,   283,   284,   285,   286,   287,   288,   289,
     290,     0,   291,   292,   293,     0,     0,     0,     0,     0,
       0,     0,    85,    86,    87,     0,    88,    89,    90,    91,
      92,     0,     0,    93,   294,   295,   296,   297,   298,   299,
       0,     0,     0,     0,   282,   283,   284,   285,   286,   287,
     288,   289,   290,     0,   291,   292,   293,     0,     0,     0,
       0,     0,     0,   300,   301,   302,     0,   303,   304,   305,
     306,   307,     0,     0,     0,   543,   294,   295,   296,   297,
     298,   299,   282,   283,   284,   285,   286,   287,   288,   289,
     290,     0,   333,   292,   293,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   300,   301,   302,     0,   303,
     304,   305,   306,   307,   294,   295,   296,   297,   298,   299,
     282,   283,   284,   285,   286,   287,   288,   289,   290,     0,
     291,   292,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   300,   301,   302,     0,   303,   304,   305,
     306,   307,   294,   295,   296,   297,   298,   299,   282,   283,
     284,   285,   286,   287,   288,   289,   290,     0,   291,   292,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   301,   302,     0,   303,   304,   305,   306,   307,
     294,   295,   296,   297,   298,   299,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   302,     0,   303,   304,   305,   306,   307
};

static const yytype_int16 yycheck[] =
{
       7,    11,    11,   102,   224,   211,   122,   447,   106,   226,
     114,    96,   316,    14,   256,   100,   114,   442,   458,   256,
     249,   325,    46,     1,    65,     1,     1,     1,   127,     1,
      71,     1,   117,     1,    72,    65,   454,   455,     0,    48,
     564,    71,     1,   128,    48,     1,   453,    99,   455,     1,
     581,   103,   104,   453,   454,    14,    28,     1,    69,   111,
       0,    72,    65,   115,   116,    94,    95,    96,    71,   121,
       1,     1,   596,    48,    48,    35,    48,    37,   609,    88,
      89,   105,   613,   614,     1,     1,     0,   128,    66,    98,
     128,   100,    48,   518,    72,     1,    48,    72,   128,   108,
     207,   153,    72,   155,    72,   157,     1,   159,    82,   161,
       1,   163,   228,   165,    66,   167,   534,   169,    48,   171,
     538,   173,   220,   530,   222,   128,    82,   552,    72,   529,
     537,    48,    48,   533,    78,   251,    42,     1,     1,    48,
      48,   240,    94,    95,    96,   387,   586,    42,    65,   378,
     387,   249,    69,   129,    71,    72,    72,    48,   128,   130,
      77,    78,    25,    26,    70,    75,    75,    84,   119,   100,
     101,   102,   103,    72,    65,    70,   130,    83,    69,   180,
      71,    72,     1,   106,    48,    48,    77,    78,    83,   241,
      96,     1,    72,    84,   619,     1,     1,     1,    94,     4,
      96,    96,   119,     1,   121,   122,   510,     1,    71,    72,
       1,   128,     1,   130,     1,    78,   106,    22,    23,    24,
      25,    14,    15,   232,    29,   234,     1,   129,   119,     1,
     121,   122,     1,   340,    25,    95,    96,   128,    72,   445,
      26,    46,    48,    77,    78,   349,   331,    66,     1,   258,
      84,   349,     1,   357,    48,   264,    66,    48,   356,   357,
       1,    48,    53,    54,   129,   128,     1,    48,     1,     1,
     476,     1,   478,    48,    79,    80,    48,    66,    97,    48,
     378,   100,   101,   102,   103,   104,     1,    97,    98,    99,
     100,   101,   102,   103,   346,    48,   100,   101,   102,   103,
     105,   106,   100,   101,   102,   103,    39,    40,    97,    98,
      99,   100,   101,   102,   103,   220,    48,   222,    48,    72,
     329,   330,     1,   332,     1,    45,    41,    42,    43,     1,
     339,    66,    66,   440,   441,   344,   443,    72,    71,    72,
      72,     1,    77,    78,    77,    78,   453,   454,   455,    84,
     129,    84,   468,    67,    97,    70,   365,   100,   101,   102,
     103,   104,   460,    45,   462,    45,   464,    26,    83,    48,
     129,    48,    87,     1,   129,     1,   385,     1,    50,    51,
      52,    96,    94,    95,   590,   129,    10,    11,    48,    10,
      11,   597,     1,    15,   403,   404,   405,   406,   407,   408,
     409,   410,   411,   412,   413,   414,   415,   416,   417,   418,
     419,   420,   421,   422,   423,   424,   425,   426,   427,   428,
      14,     1,   529,   530,    48,   645,   533,   534,   645,   646,
     537,   538,   442,   540,   541,   542,   543,     1,    66,    48,
      66,    96,    66,     1,    72,    25,    72,    57,    95,    77,
      78,    77,    78,    94,   553,     1,    84,    67,    84,    39,
      40,   568,     1,   570,    14,   572,   552,   574,   325,   576,
     519,   578,     1,     1,   525,     4,   585,    41,    42,    43,
     646,     7,     8,     9,    10,    11,   593,   594,   595,   387,
      48,    71,    72,    22,    23,    24,    25,    57,    78,   396,
      29,   248,    48,     7,     8,     9,    70,    67,   518,    48,
     520,   521,   521,   123,   124,   125,   523,    55,   118,    83,
      48,   628,    48,    87,   631,    97,    98,    99,   100,   101,
     102,   103,    96,    71,    72,    73,    74,   607,   608,    77,
      78,   118,   552,   200,   201,   118,    84,   263,   558,   558,
      79,    80,   561,   562,   563,   309,   353,   354,   355,   178,
      -1,   121,   122,   123,   124,   125,    -1,     1,    -1,     3,
       4,     5,     6,    -1,    -1,    -1,    -1,    -1,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    -1,    -1,
     128,    -1,    -1,    27,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,   619,
      44,    45,    46,    47,    48,    -1,    -1,    -1,   625,    -1,
      -1,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    71,    72,    73,
      74,    75,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    85,    -1,    -1,    88,    89,    90,    91,    92,    93,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,   121,   122,   123,
     124,   125,    -1,     1,   128,     3,     4,     5,     6,    -1,
      -1,    -1,    -1,    -1,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    27,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    -1,    -1,    -1,    -1,    -1,    44,    45,    46,    47,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    66,    67,
      68,    -1,    -1,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    -1,    -1,    -1,    84,    85,    -1,    -1,
      88,    89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,   123,   124,   125,    -1,     1,
     128,     3,     4,     5,     6,    -1,    -1,    -1,    -1,    -1,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      -1,    -1,    -1,    -1,    -1,    27,    -1,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    -1,    -1,    -1,
      -1,    -1,    44,    45,    46,    47,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    66,    67,    68,    -1,    -1,    71,
      72,    73,    74,     1,    -1,    77,    78,    -1,    -1,    -1,
      -1,    -1,    84,    85,    -1,    -1,    88,    89,    90,    91,
      92,    93,    -1,    -1,    -1,    -1,    -1,    -1,    26,    -1,
      -1,    -1,    -1,    -1,    -1,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,    -1,   121,
     122,   123,   124,   125,    -1,    -1,   128,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    66,    67,
      68,    -1,    -1,    71,    72,    73,    74,     1,    -1,    77,
      78,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,
      88,    89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    56,    57,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    66,    67,    -1,    -1,   117,
     118,   119,    -1,   121,   122,   123,   124,   125,    -1,    -1,
     128,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,     1,    -1,    71,    72,    73,
      74,     1,    -1,    77,    78,    -1,    -1,    -1,    14,    -1,
      84,    -1,    -1,    -1,    88,    89,    90,    91,    92,    93,
     121,   122,   123,   124,   125,    -1,    -1,    -1,    -1,    35,
      -1,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    45,
      -1,    -1,    48,   117,   118,   119,    -1,   121,   122,   123,
     124,   125,    -1,    -1,   128,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    66,    67,    68,    -1,
      -1,    71,    72,    73,    74,     1,    -1,    77,    78,    -1,
      -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,    88,    89,
      90,    91,    92,    93,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   117,   118,   119,
      -1,   121,   122,   123,   124,   125,    -1,    -1,   128,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      66,    67,    68,    -1,    -1,    71,    72,    73,    74,    -1,
      -1,    77,    78,    -1,    -1,    -1,    -1,    -1,    84,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   117,   118,   119,    -1,   121,   122,   123,   124,   125,
      -1,    -1,   128,     3,     4,     5,     6,    -1,    -1,    -1,
      -1,    -1,    12,    13,    -1,    -1,    16,    17,    18,    19,
      20,    21,    -1,    -1,    -1,    -1,    -1,    27,    -1,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    -1,
      -1,    -1,    -1,    -1,    44,    45,    46,    47,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    -1,    66,    67,    68,    -1,
      -1,    71,    72,    73,    74,    -1,    -1,    77,    78,    -1,
      -1,    -1,    -1,    -1,    84,    85,    -1,    -1,    88,    89,
      90,    91,    92,    93,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      -1,   121,   122,   123,   124,   125,    -1,    -1,   128,     3,
       4,     5,     6,    -1,    -1,    -1,    -1,    -1,    12,    13,
      -1,    -1,    16,    17,    18,    19,    20,    21,    -1,    -1,
      -1,    -1,    -1,    27,    -1,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    -1,    -1,    -1,    -1,    -1,
      44,    45,    46,    47,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,
      84,    85,    -1,    -1,    88,    89,    90,    91,    92,    93,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,    -1,   121,   122,   123,
     124,   125,    -1,    -1,   128,     3,     4,     5,     6,    -1,
      -1,    -1,    -1,    -1,    12,    13,    -1,    -1,    16,    17,
      18,    19,    20,    21,    -1,    -1,    -1,    -1,    -1,    27,
      -1,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    -1,    -1,    -1,    -1,    -1,    44,    -1,    46,    47,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    66,    67,
      68,    -1,    -1,    71,    72,    73,    74,    -1,    -1,    77,
      78,    -1,    -1,    -1,    -1,    -1,    84,    85,    -1,    -1,
      88,    89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,    25,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,    -1,   121,   122,   123,   124,   125,    -1,    -1,
     128,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    71,    72,    73,
      74,    -1,    -1,    77,    78,    -1,    -1,    81,    -1,    -1,
      84,    -1,    -1,    -1,    88,    89,    90,    91,    92,    93,
      65,    -1,    -1,    -1,    -1,    -1,    71,    -1,    -1,    -1,
      -1,    -1,    77,    78,    -1,    -1,    35,    -1,    37,    84,
      -1,    -1,    -1,   117,   118,   119,    45,   121,   122,   123,
     124,   125,    -1,    -1,   128,    -1,    -1,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    66,    67,    68,
      -1,    -1,    -1,    -1,   119,    -1,   121,   122,    -1,    -1,
      -1,    -1,    -1,   128,    -1,    -1,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   117,   118,
     119,    48,   121,   122,   123,   124,   125,    -1,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    -1,    66,
      67,    68,    -1,    -1,    71,    72,    73,    74,    -1,    -1,
      77,    78,    -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,
      -1,    88,    89,    90,    91,    92,    93,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     117,   118,   119,    -1,   121,   122,   123,   124,   125,    48,
      -1,   128,    -1,    -1,    -1,    -1,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    -1,    66,    67,    68,
      -1,    -1,    71,    72,    73,    74,    -1,    -1,    77,    78,
      -1,    -1,    -1,    -1,    -1,    84,    -1,    -1,    -1,    88,
      89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   117,   118,
     119,    -1,   121,   122,   123,   124,   125,    49,    -1,   128,
      -1,    -1,    -1,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    66,    67,    68,    -1,    -1,    71,
      72,    73,    74,    -1,    76,    77,    78,    -1,    -1,    -1,
      -1,    -1,    84,    -1,    -1,    -1,    88,    89,    90,    91,
      92,    93,    48,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      66,    67,    68,    -1,    -1,   117,   118,   119,    -1,   121,
     122,   123,   124,   125,    -1,    -1,   128,    -1,    -1,    -1,
      -1,    -1,    88,    89,    90,    91,    92,    93,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   117,   118,   119,    -1,   121,   122,   123,   124,   125,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      -1,    66,    67,    68,    -1,    -1,    71,    72,    73,    74,
      -1,    -1,    77,    78,    -1,    -1,    -1,    -1,    -1,    84,
      -1,    -1,    -1,    88,    89,    90,    91,    92,    93,    -1,
      -1,    -1,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   117,   118,   119,    -1,   121,   122,   123,   124,
     125,    -1,    -1,   128,    88,    89,    90,    91,    92,    93,
      -1,    -1,    -1,    -1,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    -1,    66,    67,    68,    -1,    -1,    -1,
      -1,    -1,    -1,   117,   118,   119,    -1,   121,   122,   123,
     124,   125,    -1,    -1,    -1,   129,    88,    89,    90,    91,
      92,    93,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    -1,    66,    67,    68,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   117,   118,   119,    -1,   121,
     122,   123,   124,   125,    88,    89,    90,    91,    92,    93,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    -1,
      66,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   117,   118,   119,    -1,   121,   122,   123,
     124,   125,    88,    89,    90,    91,    92,    93,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    -1,    66,    67,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   118,   119,    -1,   121,   122,   123,   124,   125,
      88,    89,    90,    91,    92,    93,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   119,    -1,   121,   122,   123,   124,   125
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,   132,   133,     0,   134,   141,   142,   135,   137,   138,
     143,   144,   145,   247,    48,    75,   140,   160,   226,   227,
     137,     3,     4,     5,     6,    12,    13,    16,    17,    18,
      19,    20,    21,    27,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    44,    45,    46,    47,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    66,    67,
      68,    71,    72,    73,    74,    77,    78,    84,    85,    88,
      89,    90,    91,    92,    93,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   121,   122,
     123,   124,   125,   128,   144,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   156,   157,   158,   159,   161,
     162,   163,   164,   165,   166,   167,   168,   169,   170,   171,
     172,   173,   174,   175,   179,   180,   185,   186,   187,   188,
     218,   219,   220,   221,   222,   223,   224,   225,   228,   229,
     230,   232,   233,   237,   238,   239,   240,   241,   242,   244,
     249,   257,   259,   260,   261,   262,   263,   264,   265,   266,
     267,   268,   269,   270,   271,   272,   273,   274,   275,   276,
     277,   278,   279,   280,   281,   310,   311,   314,   315,   142,
      48,   281,     1,   281,     1,   281,   282,    14,    15,   140,
       1,    39,    40,    71,    72,    77,    78,    84,   183,   252,
     253,   254,   319,    49,    76,   189,   215,   281,   281,   323,
     140,   281,     1,    72,   326,   326,   323,   323,   176,     1,
      69,   235,   310,   316,   320,    72,   128,   324,   325,   281,
       1,     1,    50,    51,    52,   323,     1,     4,    22,    23,
      24,    25,    29,    46,    79,    80,   105,   106,   234,   236,
       1,    28,   316,   323,   323,     1,   140,     1,    25,    78,
     250,   252,   253,   254,    25,    81,   258,   323,   324,   326,
     140,   323,   323,   323,   323,   323,   323,   323,   323,   323,
     323,   323,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    66,    67,    68,    88,    89,    90,    91,    92,    93,
     117,   118,   119,   121,   122,   123,   124,   125,   312,   313,
     312,   142,    48,   309,   321,   323,   181,     1,    42,    70,
      83,    96,   255,     1,   255,   182,     1,    66,   211,    10,
      11,   217,   119,    66,   211,   141,     1,     1,     1,    25,
      26,    71,    72,    78,   128,   177,   190,   193,   320,   130,
     320,    65,    71,   119,   121,   122,   128,   317,   318,   319,
     325,     1,   324,   281,     1,    25,    53,    54,   231,   281,
     326,     1,   323,   106,   106,    46,   105,     1,   236,   235,
       1,   324,     1,    14,    45,   171,   172,   245,   246,   281,
       1,     1,    41,    43,    87,   255,   256,     1,   256,   281,
       1,     1,     1,   294,   287,   296,   298,   297,   299,   300,
     301,   302,   293,   292,   291,   303,   304,   305,   306,   307,
     308,   289,   290,   295,   283,   284,   285,   286,   288,   309,
       1,   129,   130,   184,   321,     1,    82,   251,     1,   184,
     281,   281,   140,   281,     1,    14,   178,   281,     1,   194,
     195,   196,   197,   201,   202,   203,   211,   281,   323,   316,
     128,   318,   128,   318,   128,   318,   310,   316,   129,   281,
       1,    26,   235,     1,     1,     1,   281,     1,   172,   246,
     248,     1,   251,     1,   281,   281,   281,   281,   281,   281,
     281,   281,   281,   281,   281,   281,   281,   281,   281,   281,
     281,   281,   281,   281,   281,   281,   281,   281,   281,   281,
     322,   129,     1,    72,    78,     1,   211,   211,   136,   139,
     143,   144,   211,   141,   191,   192,   191,     1,   198,   202,
     203,   211,   199,   201,   203,   211,   200,   201,   202,   211,
      94,    95,    96,   129,   191,   310,   310,   310,   129,   324,
     141,     1,   141,    45,   243,     1,   321,   143,   144,   187,
     144,     7,     8,     9,   216,     1,   137,   193,   203,   211,
     202,   211,   203,   211,   201,   211,   202,   211,   201,   211,
       1,   211,     1,   211,     1,   211,   211,   129,   129,   129,
      14,   139,   326,   281,   281,   281,   216,    15,   211,   211,
     211,   211,   211,   211,    97,   100,   101,   102,   103,   104,
     204,   206,    97,    98,    99,   205,   206,   205,   191,   141,
       1,   211,   211,   211,   216,   141,     1,   209,   210,     1,
     207,   208,     1,   209,     1,   209,     1,   206,     1,   206,
       1,   206,   143,     1,   137,   211,   211,    14,   212,   213,
     214,   319,   325,   213
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int16 yyr1[] =
{
       0,   131,   133,   132,   134,   134,   135,   135,   136,   136,
     137,   137,   138,   139,   139,   140,   140,   141,   141,   142,
     142,   143,   143,   144,   145,   146,   146,   146,   146,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   147,   147,   147,   147,   147,   147,
     147,   147,   147,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,   165,   166,   167,   168,   169,   170,   171,   172,   173,
     174,   176,   175,   177,   177,   177,   178,   177,   177,   177,
     179,   181,   180,   182,   180,   180,   180,   180,   180,   180,
     180,   180,   180,   183,   183,   184,   184,   185,   186,   187,
     187,   187,   188,   189,   189,   189,   189,   189,   189,   189,
     189,   190,   190,   190,   192,   191,   191,   193,   193,   193,
     194,   194,   194,   194,   195,   196,   197,   198,   198,   198,
     198,   198,   199,   199,   199,   199,   199,   200,   200,   200,
     200,   200,   201,   201,   202,   202,   203,   203,   204,   204,
     204,   204,   205,   205,   205,   205,   205,   205,   206,   206,
     206,   206,   206,   206,   206,   206,   208,   207,   210,   209,
     211,   211,   212,   212,   213,   214,   215,   216,   216,   216,
     217,   217,   217,   218,   218,   219,   219,   220,   221,   221,
     221,   221,   221,   221,   222,   223,   224,   225,   225,   225,
     226,   227,   228,   228,   228,   229,   229,   230,   230,   230,
     230,   230,   230,   230,   230,   230,   230,   231,   231,   232,
     233,   233,   233,   233,   234,   234,   234,   234,   234,   234,
     234,   235,   235,   236,   236,   236,   236,   236,   236,   236,
     236,   236,   237,   237,   237,   237,   237,   238,   239,   240,
     241,   242,   243,   243,   243,   244,   244,   244,   244,   244,
     244,   245,   245,   245,   246,   246,   246,   246,   247,   247,
     248,   248,   249,   249,   249,   249,   249,   249,   249,   249,
     249,   249,   249,   250,   250,   250,   251,   251,   251,   252,
     252,   253,   254,   255,   255,   255,   255,   256,   256,   256,
     256,   257,   257,   257,   257,   258,   259,   260,   261,   262,
     263,   264,   265,   266,   267,   268,   269,   270,   271,   272,
     273,   274,   275,   276,   277,   278,   279,   280,   282,   281,
     283,   281,   284,   281,   285,   281,   281,   286,   281,   281,
     287,   281,   281,   288,   281,   281,   289,   281,   281,   290,
     281,   281,   291,   281,   281,   292,   281,   281,   293,   281,
     281,   294,   281,   281,   281,   281,   295,   281,   281,   296,
     281,   281,   297,   281,   281,   298,   281,   281,   299,   281,
     281,   300,   281,   281,   301,   281,   281,   302,   281,   281,
     303,   281,   281,   304,   281,   281,   305,   281,   281,   306,
     281,   281,   307,   281,   281,   308,   281,   281,   281,   281,
     281,   281,   281,   281,   281,   281,   281,   281,   309,   309,
     309,   310,   311,   311,   313,   312,   314,   315,   316,   316,
     316,   317,   317,   317,   317,   317,   317,   317,   317,   317,
     318,   318,   319,   319,   319,   320,   320,   320,   322,   321,
     321,   323,   323,   324,   324,   324,   324,   325,   326,   326
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     2,     2,     1,     2,     1,     2,     2,
       1,     1,     3,     1,     1,     3,     2,     2,     1,     2,
       0,     1,     1,     0,     2,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     0,     3,     3,     1,     0,     0,     3,     3,     5,
       2,     0,     4,     0,     4,     3,     3,     4,     5,     4,
       3,     4,     3,     1,     1,     1,     2,     1,     1,     2,
       1,     3,     6,     7,     6,     5,     4,     2,     2,     2,
       0,     1,     1,     1,     0,     2,     0,     2,     3,     2,
       1,     1,     1,     1,     2,     2,     2,     2,     3,     2,
       3,     1,     2,     3,     2,     3,     1,     2,     3,     2,
       3,     1,     4,     3,     4,     3,     4,     3,     1,     2,
       2,     1,     1,     2,     2,     2,     2,     1,     2,     2,
       2,     2,     2,     2,     2,     2,     0,     3,     0,     3,
       1,     0,     1,     1,     1,     1,     1,     3,     3,     3,
       3,     3,     0,     3,     2,     3,     2,     2,     6,     9,
       9,     6,     2,     4,     2,     2,     2,     2,     3,     1,
       1,     1,     2,     3,     1,     1,     2,     3,     2,     4,
       3,     2,     4,     3,     2,     3,     2,     1,     1,     2,
       4,     3,     3,     2,     1,     2,     2,     1,     2,     2,
       1,     3,     1,     1,     1,     1,     1,     1,     1,     2,
       3,     2,     1,     2,     3,     4,     3,     2,     2,     2,
       2,     2,     2,     3,     1,     5,     4,     4,     2,     3,
       5,     2,     1,     1,     6,     7,     2,     2,     2,     2,
       3,     0,     3,     3,     2,     3,     3,     4,     5,     4,
       3,     4,     3,     1,     1,     1,     2,     2,     2,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     3,     2,     3,     2,     1,     2,     1,     2,     1,
       2,     1,     2,     1,     2,     1,     2,     1,     2,     1,
       2,     1,     2,     1,     2,     1,     2,     1,     0,     3,
       0,     4,     0,     4,     0,     4,     1,     0,     4,     1,
       0,     4,     1,     0,     4,     1,     0,     4,     1,     0,
       4,     1,     0,     4,     1,     0,     4,     1,     0,     4,
       1,     0,     4,     1,     2,     1,     0,     4,     1,     0,
       4,     1,     0,     4,     1,     0,     4,     1,     0,     4,
       1,     0,     4,     1,     0,     4,     1,     0,     4,     1,
       0,     4,     1,     0,     4,     1,     0,     4,     1,     0,
       4,     1,     0,     4,     1,     0,     4,     1,     1,     1,
       1,     1,     1,     1,     2,     2,     2,     2,     2,     2,
       1,     1,     2,     2,     0,     2,     1,     1,     3,     1,
       1,     2,     2,     1,     2,     3,     4,     4,     4,     1,
       1,     1,     1,     1,     1,     2,     2,     0,     0,     4,
       1,     1,     0,     2,     1,     4,     3,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
# ifndef YY_LOCATION_PRINT
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
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






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
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
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* $@1: %empty  */
#line 160 "./yaccsrc.y"
                                       { level = 0;
                                         if ( get_options_flag( parser_data.TSD->currlevel, EXT_CALLS_AS_FUNCS )
                                           && !get_options_flag( parser_data.TSD->currlevel, EXT_STRICT_ANSI ) )
                                            start_parendepth = 1;
                                         else
                                            start_parendepth = 0;
                                         parendepth = 0; }
#line 2428 "./yaccsrc.c"
    break;

  case 4: /* prog: nlncl stats  */
#line 170 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-1], yyvsp[0] );
                                         yyval->o.last = NULL;
                                         EndProg( yyval ) ; }
#line 2436 "./yaccsrc.c"
    break;

  case 5: /* prog: nlncl  */
#line 173 "./yaccsrc.y"
                                       { yyval = yyvsp[0];
                                         if ( yyval != NULL )
                                            yyval->o.last = NULL;
                                         EndProg( yyval ); }
#line 2445 "./yaccsrc.c"
    break;

  case 6: /* stats: stats ystatement  */
#line 179 "./yaccsrc.y"
                                       { /* fixes bug 579711 */
                                         yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 2452 "./yaccsrc.c"
    break;

  case 7: /* stats: ystatement  */
#line 181 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 2458 "./yaccsrc.c"
    break;

  case 8: /* xstats: xstats statement  */
#line 184 "./yaccsrc.y"
                                       { /* fixes bug 579711 */
                                         yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 2465 "./yaccsrc.c"
    break;

  case 9: /* xstats: statement gruff  */
#line 186 "./yaccsrc.y"
                                       { yyval = yyvsp[-1]; }
#line 2471 "./yaccsrc.c"
    break;

  case 10: /* ystatement: statement  */
#line 189 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 2477 "./yaccsrc.c"
    break;

  case 11: /* ystatement: lonely_end  */
#line 190 "./yaccsrc.y"
                                       { exiterror( ERR_UNMATCHED_END, 1 ); }
#line 2483 "./yaccsrc.c"
    break;

  case 13: /* nxstats: xstats  */
#line 198 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 2489 "./yaccsrc.c"
    break;

  case 14: /* nxstats: gruff  */
#line 199 "./yaccsrc.y"
                                       { yyval = NULL; }
#line 2495 "./yaccsrc.c"
    break;

  case 15: /* ncl: ncl STATSEP optLabels  */
#line 202 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-2], yyvsp[0] ); }
#line 2501 "./yaccsrc.c"
    break;

  case 16: /* ncl: STATSEP optLabels  */
#line 203 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 2507 "./yaccsrc.c"
    break;

  case 17: /* nlncl: optLabels ncl  */
#line 206 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 2513 "./yaccsrc.c"
    break;

  case 18: /* nlncl: optLabels  */
#line 207 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 2519 "./yaccsrc.c"
    break;

  case 19: /* optLabels: optLabels label_stat  */
#line 210 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 2525 "./yaccsrc.c"
    break;

  case 20: /* optLabels: %empty  */
#line 211 "./yaccsrc.y"
                                       { yyval = NULL; }
#line 2531 "./yaccsrc.c"
    break;

  case 23: /* gruff: %empty  */
#line 218 "./yaccsrc.y"
                                       { tmpchr=parser_data.tstart;
                                         tmplno=parser_data.tline; }
#line 2538 "./yaccsrc.c"
    break;

  case 24: /* mttstatement: gruff mtstatement  */
#line 222 "./yaccsrc.y"
                                       { yyval=yyvsp[0]; }
#line 2544 "./yaccsrc.c"
    break;

  case 25: /* mtstatement: nclstatement ncl  */
#line 225 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 2550 "./yaccsrc.c"
    break;

  case 64: /* call: CALL  */
#line 268 "./yaccsrc.y"
                                       { yyval = makenode(X_CALL,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2558 "./yaccsrc.c"
    break;

  case 65: /* do: DO  */
#line 272 "./yaccsrc.y"
                                       { yyval = makenode(X_DO,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         level++; }
#line 2567 "./yaccsrc.c"
    break;

  case 66: /* exit: EXIT  */
#line 277 "./yaccsrc.y"
                                       { yyval = makenode(X_EXIT,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2575 "./yaccsrc.c"
    break;

  case 67: /* if: IF  */
#line 281 "./yaccsrc.y"
                                       { yyval = makenode(X_IF,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         level++; }
#line 2584 "./yaccsrc.c"
    break;

  case 68: /* iterate: ITERATE  */
#line 286 "./yaccsrc.y"
                                       { yyval = makenode(X_ITERATE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2592 "./yaccsrc.c"
    break;

  case 69: /* leave: LEAVE  */
#line 290 "./yaccsrc.y"
                                       { yyval = makenode(X_LEAVE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2600 "./yaccsrc.c"
    break;

  case 70: /* say: SAY  */
#line 294 "./yaccsrc.y"
                                       { yyval = makenode(X_SAY,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2608 "./yaccsrc.c"
    break;

  case 71: /* return: RETURN  */
#line 298 "./yaccsrc.y"
                                       { yyval = makenode(X_RETURN,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2616 "./yaccsrc.c"
    break;

  case 72: /* address: ADDRESS  */
#line 302 "./yaccsrc.y"
                                       { yyval = makenode(X_ADDR_N,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2624 "./yaccsrc.c"
    break;

  case 73: /* arg: ARG  */
#line 306 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_ARG,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2632 "./yaccsrc.c"
    break;

  case 74: /* drop: DROP  */
#line 310 "./yaccsrc.y"
                                       { yyval = makenode(X_DROP,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2640 "./yaccsrc.c"
    break;

  case 75: /* interpret: INTERPRET  */
#line 314 "./yaccsrc.y"
                                       { yyval = makenode(X_IPRET,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2648 "./yaccsrc.c"
    break;

  case 76: /* label: LABEL  */
#line 318 "./yaccsrc.y"
                                       { yyval = makenode(X_LABEL,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2656 "./yaccsrc.c"
    break;

  case 77: /* nop: NOP  */
#line 322 "./yaccsrc.y"
                                       { yyval = makenode(X_NULL,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2664 "./yaccsrc.c"
    break;

  case 78: /* numeric: NUMERIC  */
#line 326 "./yaccsrc.y"
                                       { yyval = makenode(0,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2672 "./yaccsrc.c"
    break;

  case 79: /* options: OPTIONS  */
#line 330 "./yaccsrc.y"
                                       { yyval = makenode(X_OPTIONS,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2680 "./yaccsrc.c"
    break;

  case 80: /* parse: PARSE  */
#line 334 "./yaccsrc.y"
                                       { yyval = makenode(0,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2688 "./yaccsrc.c"
    break;

  case 81: /* proc: PROCEDURE  */
#line 338 "./yaccsrc.y"
                                       { yyval = makenode(X_PROC,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2696 "./yaccsrc.c"
    break;

  case 82: /* pull: PULL  */
#line 342 "./yaccsrc.y"
                                       { yyval = makenode(X_PULL,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2704 "./yaccsrc.c"
    break;

  case 83: /* push: PUSH  */
#line 346 "./yaccsrc.y"
                                       { yyval = makenode(X_PUSH,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2712 "./yaccsrc.c"
    break;

  case 84: /* queue: QUEUE  */
#line 350 "./yaccsrc.y"
                                       { yyval = makenode(X_QUEUE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2720 "./yaccsrc.c"
    break;

  case 85: /* select: SELECT  */
#line 354 "./yaccsrc.y"
                                       { yyval = makenode(X_SELECT,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         level++; }
#line 2729 "./yaccsrc.c"
    break;

  case 86: /* signal: SIGNAL  */
#line 359 "./yaccsrc.y"
                                       { yyval = makenode(X_SIG_LAB,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2737 "./yaccsrc.c"
    break;

  case 87: /* when: WHEN  */
#line 363 "./yaccsrc.y"
                                       { yyval = makenode(X_WHEN,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2745 "./yaccsrc.c"
    break;

  case 88: /* otherwise: OTHERWISE  */
#line 367 "./yaccsrc.y"
                                       { yyval = makenode(X_OTHERWISE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2753 "./yaccsrc.c"
    break;

  case 89: /* trace: TRACE  */
#line 371 "./yaccsrc.y"
                                       { yyval = makenode(X_TRACE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2761 "./yaccsrc.c"
    break;

  case 90: /* upper: UPPER  */
#line 375 "./yaccsrc.y"
                                       { yyval = makenode(X_UPPER_VAR,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 2769 "./yaccsrc.c"
    break;

  case 91: /* @2: %empty  */
#line 379 "./yaccsrc.y"
                                       { yyval = current = yyvsp[0] ; }
#line 2775 "./yaccsrc.c"
    break;

  case 93: /* address_stat2: VALUE expr naddr_with  */
#line 383 "./yaccsrc.y"
                                       { current->type = X_ADDR_V ;
                                         current->p[0] = yyvsp[-1] ;
                                         current->p[1] = yyvsp[0] ; }
#line 2783 "./yaccsrc.c"
    break;

  case 94: /* address_stat2: addr_with  */
#line 386 "./yaccsrc.y"
                                       { exiterror( ERR_STRING_EXPECTED, 1, __reginatext ) ;}
#line 2789 "./yaccsrc.c"
    break;

  case 95: /* address_stat2: %empty  */
#line 387 "./yaccsrc.y"
                                       { current->type = X_ADDR_S ; }
#line 2795 "./yaccsrc.c"
    break;

  case 96: /* $@3: %empty  */
#line 388 "./yaccsrc.y"
                                       { exiterror( ERR_STRING_EXPECTED, 1, __reginatext ) ;}
#line 2801 "./yaccsrc.c"
    break;

  case 98: /* address_stat2: nvir nexpr naddr_with  */
#line 390 "./yaccsrc.y"
                                       { current->name = (streng *)yyvsp[-2] ;
                                         current->type = X_ADDR_N ;
                                         current->p[0] = yyvsp[-1] ;
                                         current->p[1] = yyvsp[0] ; }
#line 2810 "./yaccsrc.c"
    break;

  case 99: /* address_stat2: '(' expr ')' nspace naddr_with  */
#line 394 "./yaccsrc.y"
                                              { current->type = X_ADDR_V ;
                                         current->p[0] = yyvsp[-3] ;
                                         current->p[1] = yyvsp[0] ;
                                         current->u.nonansi = 1 ; }
#line 2819 "./yaccsrc.c"
    break;

  case 100: /* arg_stat: arg templs  */
#line 400 "./yaccsrc.y"
                                       { yyval = makenode( X_PARSE, 2, yyvsp[-1], yyvsp[0] );
                                         yyval->u.parseflags = PARSE_UPPER;
                                         yyval->lineno = yyvsp[-1]->lineno;
                                         yyval->charnr = yyvsp[-1]->charnr; }
#line 2828 "./yaccsrc.c"
    break;

  case 101: /* $@4: %empty  */
#line 406 "./yaccsrc.y"
                                       { parendepth = start_parendepth; }
#line 2834 "./yaccsrc.c"
    break;

  case 102: /* call_stat: call call_name $@4 call_args  */
#line 407 "./yaccsrc.y"
                                       { yyval = yyvsp[-3];
                                         yyval->p[0] = yyvsp[0];
                                         yyval->name = (streng *) yyvsp[-2];
                                         parendepth = 0; }
#line 2843 "./yaccsrc.c"
    break;

  case 103: /* $@5: %empty  */
#line 411 "./yaccsrc.y"
                                       { parendepth = start_parendepth; }
#line 2849 "./yaccsrc.c"
    break;

  case 104: /* call_stat: call string $@5 call_args  */
#line 412 "./yaccsrc.y"
                                       { yyval = yyvsp[-3];
                                         yyval->type = X_EX_FUNC;
                                         yyval->p[0] = yyvsp[0];
                                         yyval->name = (streng *) yyvsp[-2];
                                         parendepth = 0; }
#line 2859 "./yaccsrc.c"
    break;

  case 105: /* call_stat: call on error  */
#line 417 "./yaccsrc.y"
                             { exiterror( ERR_INV_SUBKEYWORD, 1, "ERROR FAILURE HALT NOTREADY", __reginatext ) ;}
#line 2865 "./yaccsrc.c"
    break;

  case 106: /* call_stat: call off error  */
#line 418 "./yaccsrc.y"
                              { exiterror( ERR_INV_SUBKEYWORD, 2, "ERROR FAILURE HALT NOTREADY", __reginatext ) ;}
#line 2871 "./yaccsrc.c"
    break;

  case 107: /* call_stat: call on c_action error  */
#line 419 "./yaccsrc.y"
                                      { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 2877 "./yaccsrc.c"
    break;

  case 108: /* call_stat: call on c_action namespec error  */
#line 420 "./yaccsrc.y"
                                               { exiterror( ERR_STRING_EXPECTED, 3, __reginatext ) ;}
#line 2883 "./yaccsrc.c"
    break;

  case 109: /* call_stat: call on c_action namespec  */
#line 422 "./yaccsrc.y"
                                       { yyval = yyvsp[-3] ;
                                         yyval->type = X_CALL_SET ;
                                         yyval->p[0] = yyvsp[-2] ;
                                         yyval->name = (streng *)yyvsp[0] ;
                                         yyval->p[1] = yyvsp[-1] ; }
#line 2893 "./yaccsrc.c"
    break;

  case 110: /* call_stat: call on c_action  */
#line 427 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_CALL_SET ;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->name = NULL ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 2903 "./yaccsrc.c"
    break;

  case 111: /* call_stat: call off c_action error  */
#line 432 "./yaccsrc.y"
                                       { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 2909 "./yaccsrc.c"
    break;

  case 112: /* call_stat: call off c_action  */
#line 433 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_CALL_SET ;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 2918 "./yaccsrc.c"
    break;

  case 113: /* call_name: asymbol  */
#line 439 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 2924 "./yaccsrc.c"
    break;

  case 114: /* call_name: error  */
#line 440 "./yaccsrc.y"
                                       { exiterror( ERR_STRING_EXPECTED, 2, __reginatext );}
#line 2930 "./yaccsrc.c"
    break;

  case 115: /* call_args: exprs  */
#line 443 "./yaccsrc.y"
                                       {
                                         /*
                                          * "call_args" accepted probably with
                                          * surrounding parentheses. Strip them.
                                          */
                                         yyval = reduce_expr_list( yyvsp[0],
                                                                REDUCE_CALL );
                                       }
#line 2943 "./yaccsrc.c"
    break;

  case 116: /* call_args: exprs ')'  */
#line 451 "./yaccsrc.y"
                                       { exiterror(ERR_UNEXPECTED_PARAN, 2); }
#line 2949 "./yaccsrc.c"
    break;

  case 117: /* expr_stat: expr  */
#line 454 "./yaccsrc.y"
                                       { yyval = makenode(X_COMMAND,0) ;
                                         yyval->charnr = tmpchr ;
                                         yyval->lineno = tmplno;
                                         yyval->p[0] = yyvsp[0] ; }
#line 2958 "./yaccsrc.c"
    break;

  case 118: /* end_stat: END  */
#line 460 "./yaccsrc.y"
                                       { yyval = makenode(X_END,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         level--; }
#line 2967 "./yaccsrc.c"
    break;

  case 119: /* end: end_stat simsymb  */
#line 466 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->name = (streng*)(yyvsp[0]) ; }
#line 2974 "./yaccsrc.c"
    break;

  case 120: /* end: end_stat  */
#line 468 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 2980 "./yaccsrc.c"
    break;

  case 121: /* end: end_stat simsymb error  */
#line 469 "./yaccsrc.y"
                                       {  exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 2986 "./yaccsrc.c"
    break;

  case 122: /* do_stat: do repetitor conditional ncl nxstats end  */
#line 473 "./yaccsrc.y"
                                       { yyval = yyvsp[-5];
                                         yyval->p[0] = yyvsp[-4];
                                         yyval->p[1] = yyvsp[-3];
                                         yyval->p[2] = optgluelast( yyvsp[-2], yyvsp[-1] );
                                         if ( yyval->p[2] )
                                            yyval->p[2]->o.last = NULL;
                                         yyval->p[3] = yyvsp[0];
                          if ((yyval->p[0]==NULL || yyval->p[0]->name==NULL)
                              && yyval->p[3]->name)
                                            exiterror( ERR_UNMATCHED_END, 0 );
                          if ((yyval->p[0])&&(yyval->p[0]->name)&&
                              (yyval->p[3]->name)&&
                              ((yyval->p[3]->name->len != yyval->p[0]->name->len)||
                               (strncmp(yyval->p[3]->name->value,
                                        yyval->p[0]->name->value,
                                        yyval->p[0]->name->len))))
                                            exiterror( ERR_UNMATCHED_END, 0 );
                                       }
#line 3009 "./yaccsrc.c"
    break;

  case 123: /* repetitor: dovar '=' expr nspace tobyfor tobyfor tobyfor  */
#line 494 "./yaccsrc.y"
                                       { yyval =makenode(X_REP,4,yyvsp[-4],yyvsp[-2],yyvsp[-1],yyvsp[0]) ;
                                         yyval->name = (streng *)yyvsp[-6] ;
                                         checkdosyntax(yyval) ; }
#line 3017 "./yaccsrc.c"
    break;

  case 124: /* repetitor: dovar '=' expr nspace tobyfor tobyfor  */
#line 498 "./yaccsrc.y"
                                       { yyval =makenode(X_REP,3,yyvsp[-3],yyvsp[-1],yyvsp[0]) ;
                                         yyval->name = (streng *)yyvsp[-5] ;
                                         checkdosyntax(yyval) ; }
#line 3025 "./yaccsrc.c"
    break;

  case 125: /* repetitor: dovar '=' expr nspace tobyfor  */
#line 502 "./yaccsrc.y"
                                       { yyval = makenode(X_REP,2,yyvsp[-2],yyvsp[0]) ;
                                         yyval->name = (streng *)yyvsp[-4] ;
                                         checkdosyntax(yyval) ; }
#line 3033 "./yaccsrc.c"
    break;

  case 126: /* repetitor: dovar '=' expr nspace  */
#line 505 "./yaccsrc.y"
                                       { yyval = makenode(X_REP,1,yyvsp[-1]) ;
                                         yyval->name = (streng *)yyvsp[-3] ;
                                         checkdosyntax(yyval) ; }
#line 3041 "./yaccsrc.c"
    break;

  case 127: /* repetitor: FOREVER nspace  */
#line 508 "./yaccsrc.y"
                                       { yyval = makenode(X_REP_FOREVER,0) ; }
#line 3047 "./yaccsrc.c"
    break;

  case 128: /* repetitor: FOREVER error  */
#line 509 "./yaccsrc.y"
                             { exiterror( ERR_INV_SUBKEYWORD, 16, "WHILE UNTIL", __reginatext ) ; }
#line 3053 "./yaccsrc.c"
    break;

  case 129: /* repetitor: expr nspace  */
#line 510 "./yaccsrc.y"
                                       { yyvsp[-1] = makenode(X_DO_EXPR,1,yyvsp[-1]) ;
                                         yyval = makenode(X_REP,2,NULL,yyvsp[-1]) ; }
#line 3060 "./yaccsrc.c"
    break;

  case 130: /* repetitor: %empty  */
#line 512 "./yaccsrc.y"
                                       { yyval = NULL ; }
#line 3066 "./yaccsrc.c"
    break;

  case 131: /* nvir: CONSYMBOL  */
#line 515 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 3072 "./yaccsrc.c"
    break;

  case 132: /* nvir: SIMSYMBOL  */
#line 516 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 3078 "./yaccsrc.c"
    break;

  case 133: /* nvir: STRING  */
#line 517 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 3084 "./yaccsrc.c"
    break;

  case 134: /* @6: %empty  */
#line 520 "./yaccsrc.y"
                                       { SymbolDetect |= SD_ADDRWITH ;
                                         yyval = with = makenode(X_ADDR_WITH,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 3093 "./yaccsrc.c"
    break;

  case 135: /* naddr_with: @6 addr_with  */
#line 524 "./yaccsrc.y"
                                       { with = NULL ;
                                         SymbolDetect &= ~SD_ADDRWITH ; }
#line 3100 "./yaccsrc.c"
    break;

  case 136: /* naddr_with: %empty  */
#line 526 "./yaccsrc.y"
                                       { yyval = NULL ; }
#line 3106 "./yaccsrc.c"
    break;

  case 137: /* addr_with: WITH connection  */
#line 529 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 3112 "./yaccsrc.c"
    break;

  case 138: /* addr_with: WITH connection error  */
#line 530 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 5, __reginatext ) ; }
#line 3118 "./yaccsrc.c"
    break;

  case 139: /* addr_with: WITH nspace  */
#line 531 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 5, __reginatext ) ; }
#line 3124 "./yaccsrc.c"
    break;

  case 143: /* connection: error  */
#line 537 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 5, __reginatext ) ; }
#line 3130 "./yaccsrc.c"
    break;

  case 162: /* inputstmt: nspace INPUT nspace resourcei  */
#line 573 "./yaccsrc.y"
                                             { with->p[0] = yyvsp[0]; }
#line 3136 "./yaccsrc.c"
    break;

  case 163: /* inputstmt: nspace INPUT error  */
#line 574 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 6, __reginatext ) ; }
#line 3142 "./yaccsrc.c"
    break;

  case 164: /* outputstmt: nspace OUTPUT nspace resourceo  */
#line 577 "./yaccsrc.y"
                                              { with->p[1] = yyvsp[0]; }
#line 3148 "./yaccsrc.c"
    break;

  case 165: /* outputstmt: nspace OUTPUT error  */
#line 578 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 7, __reginatext ) ; }
#line 3154 "./yaccsrc.c"
    break;

  case 166: /* errorstmt: nspace ERROR nspace resourceo  */
#line 581 "./yaccsrc.y"
                                             { with->p[2] = yyvsp[0]; }
#line 3160 "./yaccsrc.c"
    break;

  case 167: /* errorstmt: nspace ERROR error  */
#line 582 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 14, __reginatext ) ; }
#line 3166 "./yaccsrc.c"
    break;

  case 168: /* resourcei: resources  */
#line 585 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3172 "./yaccsrc.c"
    break;

  case 169: /* resourcei: NOEOL resources  */
#line 586 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ;
                                         yyval->u.of.noeol = 1 ; }
#line 3179 "./yaccsrc.c"
    break;

  case 170: /* resourcei: NOEOL error  */
#line 588 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 8, __reginatext ) ; }
#line 3185 "./yaccsrc.c"
    break;

  case 171: /* resourcei: NORMAL  */
#line 589 "./yaccsrc.y"
                                       { yyval = makenode(X_ADDR_WITH, 0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 3193 "./yaccsrc.c"
    break;

  case 172: /* resourceo: resources  */
#line 594 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3199 "./yaccsrc.c"
    break;

  case 173: /* resourceo: APPEND resources  */
#line 595 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ;
                                         yyval->u.of.append = 1 ; }
#line 3206 "./yaccsrc.c"
    break;

  case 174: /* resourceo: APPEND error  */
#line 597 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 8, __reginatext ) ; }
#line 3212 "./yaccsrc.c"
    break;

  case 175: /* resourceo: REPLACE resources  */
#line 598 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3218 "./yaccsrc.c"
    break;

  case 176: /* resourceo: REPLACE error  */
#line 599 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 9, __reginatext ) ; }
#line 3224 "./yaccsrc.c"
    break;

  case 177: /* resourceo: NORMAL  */
#line 600 "./yaccsrc.y"
                                       { yyval = makenode(X_ADDR_WITH, 0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 3232 "./yaccsrc.c"
    break;

  case 178: /* resources: STREAM nnvir  */
#line 605 "./yaccsrc.y"
                                       { /* ANSI extension: nsimsymb is
                                          * used by the standard, but I think
                                          * there are no reasons why using
                                          * it here as a must. FGC
                                          */
                                         yyval = yyvsp[0] ;
                                         yyval->u.of.awt = awtSTREAM;
                                         SymbolDetect |= SD_ADDRWITH ; }
#line 3245 "./yaccsrc.c"
    break;

  case 179: /* resources: STREAM error  */
#line 613 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_OPTION, 1, __reginatext ) ; }
#line 3251 "./yaccsrc.c"
    break;

  case 180: /* resources: STEM nsimsymb  */
#line 614 "./yaccsrc.y"
                                       {
                                         streng *tmp = yyvsp[0]->name;
                                         char *p;

                                         /*
                                          * expect a single dot as the last character
                                          */
                                         p = (char *)memchr( tmp->value, '.', tmp->len );
                                         if ( p != tmp->value + tmp->len - 1 )
                                            exiterror( ERR_INVALID_OPTION, 3, __reginatext );
                                         yyval = yyvsp[0] ;
                                         yyval->u.of.awt = awtSTEM ;
                                         SymbolDetect |= SD_ADDRWITH ; }
#line 3269 "./yaccsrc.c"
    break;

  case 181: /* resources: STEM error  */
#line 627 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_OPTION, 2, __reginatext ) ; }
#line 3275 "./yaccsrc.c"
    break;

  case 182: /* resources: LIFO nnvir  */
#line 628 "./yaccsrc.y"
                                       {
                                         yyval = yyvsp[0] ;
                                         yyval->u.of.awt = awtLIFO ;
                                         SymbolDetect |= SD_ADDRWITH ; }
#line 3284 "./yaccsrc.c"
    break;

  case 183: /* resources: LIFO error  */
#line 632 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_OPTION, 100, __reginatext ) ; }
#line 3290 "./yaccsrc.c"
    break;

  case 184: /* resources: FIFO nnvir  */
#line 633 "./yaccsrc.y"
                                       {
                                         yyval = yyvsp[0] ;
                                         yyval->u.of.awt = awtFIFO ;
                                         SymbolDetect |= SD_ADDRWITH ; }
#line 3299 "./yaccsrc.c"
    break;

  case 185: /* resources: FIFO error  */
#line 637 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_OPTION, 101, __reginatext ) ; }
#line 3305 "./yaccsrc.c"
    break;

  case 186: /* $@7: %empty  */
#line 640 "./yaccsrc.y"
                                       { SymbolDetect &= ~SD_ADDRWITH ; }
#line 3311 "./yaccsrc.c"
    break;

  case 187: /* nsimsymb: $@7 nspace addrSim  */
#line 641 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3317 "./yaccsrc.c"
    break;

  case 188: /* $@8: %empty  */
#line 644 "./yaccsrc.y"
                                       { SymbolDetect &= ~SD_ADDRWITH ; }
#line 3323 "./yaccsrc.c"
    break;

  case 189: /* nnvir: $@8 nspace addrAll  */
#line 645 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3329 "./yaccsrc.c"
    break;

  case 192: /* addrAll: addrSim  */
#line 652 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3335 "./yaccsrc.c"
    break;

  case 193: /* addrAll: addrString  */
#line 653 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3341 "./yaccsrc.c"
    break;

  case 194: /* addrSim: xsimsymb  */
#line 656 "./yaccsrc.y"
                                       { yyval = makenode(X_ADDR_WITH, 0) ;
                                         yyval->name = (streng *) yyvsp[0] ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->u.of.ant = antSIMSYMBOL;
                                       }
#line 3352 "./yaccsrc.c"
    break;

  case 195: /* addrString: string  */
#line 664 "./yaccsrc.y"
                                       { yyval = makenode(X_ADDR_WITH, 0) ;
                                         yyval->name = (streng *) yyvsp[0] ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->u.of.ant = antSTRING;
                                       }
#line 3363 "./yaccsrc.c"
    break;

  case 196: /* dovar: DOVARIABLE  */
#line 672 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 3369 "./yaccsrc.c"
    break;

  case 197: /* tobyfor: TO expr nspace  */
#line 675 "./yaccsrc.y"
                                       { yyval = makenode(X_DO_TO,1,yyvsp[-1]) ; }
#line 3375 "./yaccsrc.c"
    break;

  case 198: /* tobyfor: FOR expr nspace  */
#line 676 "./yaccsrc.y"
                                       { yyval = makenode(X_DO_FOR,1,yyvsp[-1]) ; }
#line 3381 "./yaccsrc.c"
    break;

  case 199: /* tobyfor: BY expr nspace  */
#line 677 "./yaccsrc.y"
                                       { yyval = makenode(X_DO_BY,1,yyvsp[-1]) ; }
#line 3387 "./yaccsrc.c"
    break;

  case 200: /* conditional: WHILE expr nspace  */
#line 680 "./yaccsrc.y"
                                       { yyval = makenode(X_WHILE,1,yyvsp[-1]) ; }
#line 3393 "./yaccsrc.c"
    break;

  case 201: /* conditional: UNTIL expr nspace  */
#line 681 "./yaccsrc.y"
                                       { yyval = makenode(X_UNTIL,1,yyvsp[-1]) ; }
#line 3399 "./yaccsrc.c"
    break;

  case 202: /* conditional: %empty  */
#line 682 "./yaccsrc.y"
                                       { yyval = NULL ; }
#line 3405 "./yaccsrc.c"
    break;

  case 203: /* drop_stat: drop anyvars error  */
#line 685 "./yaccsrc.y"
                                  { exiterror( ERR_SYMBOL_EXPECTED, 1, __reginatext ) ;}
#line 3411 "./yaccsrc.c"
    break;

  case 204: /* drop_stat: drop anyvars  */
#line 686 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3418 "./yaccsrc.c"
    break;

  case 205: /* upper_stat: upper anyvars error  */
#line 690 "./yaccsrc.y"
                                   { exiterror( ERR_SYMBOL_EXPECTED, 1, __reginatext ) ;}
#line 3424 "./yaccsrc.c"
    break;

  case 206: /* upper_stat: upper anyvars  */
#line 691 "./yaccsrc.y"
                                        { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3431 "./yaccsrc.c"
    break;

  case 207: /* exit_stat: exit nexpr  */
#line 695 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3438 "./yaccsrc.c"
    break;

  case 208: /* if_stat: if expr nlncl THEN nlncl ystatement  */
#line 700 "./yaccsrc.y"
                                       { move_labels( yyvsp[-5], yyvsp[0], level - 1 );
                                         yyval = yyvsp[-5];
                                         yyval->p[0] = optgluelast( yyvsp[-4], yyvsp[-3] );
                                         yyval->p[0]->o.last = NULL;
                                         yyval->p[1] = optgluelast( yyvsp[-1], yyvsp[0] );
                                         yyval->p[1]->o.last = NULL;
                                         level--; }
#line 3450 "./yaccsrc.c"
    break;

  case 209: /* if_stat: if expr nlncl THEN nlncl ystatement ELSE nlncl ystatement  */
#line 708 "./yaccsrc.y"
                                       { move_labels( yyvsp[-8], yyvsp[0], level - 1 );
                                         yyval = yyvsp[-8];
                                         yyval->p[0] = optgluelast( yyvsp[-7], yyvsp[-6] );
                                         yyval->p[0]->o.last = NULL;
                                         yyval->p[1] = optgluelast( yyvsp[-4], yyvsp[-3] );
                                         yyval->p[1]->o.last = NULL;
                                         yyval->p[2] = optgluelast( yyvsp[-1], yyvsp[0] );
                                         yyval->p[2]->o.last = NULL;
                                         level--; }
#line 3464 "./yaccsrc.c"
    break;

  case 210: /* if_stat: if expr nlncl THEN nlncl ystatement ELSE nlncl error  */
#line 718 "./yaccsrc.y"
                                       {  exiterror( ERR_INCOMPLETE_STRUCT, 4 ) ;}
#line 3470 "./yaccsrc.c"
    break;

  case 211: /* if_stat: if expr nlncl THEN nlncl error  */
#line 720 "./yaccsrc.y"
                                       {  exiterror( ERR_INCOMPLETE_STRUCT, 3 ) ;}
#line 3476 "./yaccsrc.c"
    break;

  case 212: /* if_stat: if ncl  */
#line 721 "./yaccsrc.y"
                                       {  exiterror( ERR_INCOMPLETE_STRUCT, 0 ) ;}
#line 3482 "./yaccsrc.c"
    break;

  case 213: /* if_stat: if expr nlncl error  */
#line 722 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_EXPECTED, 1, parser_data.if_linenr, __reginatext )  ; }
#line 3488 "./yaccsrc.c"
    break;

  case 214: /* unexp_then: gruff THEN  */
#line 725 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_UNEXPECTED, 1 )  ; }
#line 3494 "./yaccsrc.c"
    break;

  case 215: /* unexp_else: gruff ELSE  */
#line 728 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_UNEXPECTED, 2 )  ; }
#line 3500 "./yaccsrc.c"
    break;

  case 216: /* ipret_stat: interpret expr  */
#line 731 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3507 "./yaccsrc.c"
    break;

  case 217: /* iterate_stat: iterate simsymb  */
#line 736 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 3514 "./yaccsrc.c"
    break;

  case 218: /* iterate_stat: iterate simsymb error  */
#line 738 "./yaccsrc.y"
                                     { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 3520 "./yaccsrc.c"
    break;

  case 219: /* iterate_stat: iterate  */
#line 739 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3526 "./yaccsrc.c"
    break;

  case 220: /* label_stat: labelname  */
#line 742 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ;
                                         yyval->u.trace_only =
                                                         (level == 0) ? 0 : 1;
                                         newlabel( (const tsd_t *)parser_data.TSD,
                                                   &parser_data,
                                                   yyvsp[0] ) ; }
#line 3537 "./yaccsrc.c"
    break;

  case 221: /* labelname: label  */
#line 750 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ;
                                         yyval->name = Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 3544 "./yaccsrc.c"
    break;

  case 222: /* leave_stat: leave simsymb  */
#line 754 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 3551 "./yaccsrc.c"
    break;

  case 223: /* leave_stat: leave simsymb error  */
#line 756 "./yaccsrc.y"
                                   { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 3557 "./yaccsrc.c"
    break;

  case 224: /* leave_stat: leave  */
#line 757 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3563 "./yaccsrc.c"
    break;

  case 225: /* nop_stat: nop  */
#line 760 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3569 "./yaccsrc.c"
    break;

  case 226: /* nop_stat: nop error  */
#line 761 "./yaccsrc.y"
                         {  exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 3575 "./yaccsrc.c"
    break;

  case 227: /* numeric_stat: numeric DIGITS expr  */
#line 764 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_NUM_D ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3583 "./yaccsrc.c"
    break;

  case 228: /* numeric_stat: numeric DIGITS  */
#line 767 "./yaccsrc.y"
                                       { yyval = yyvsp[-1]; yyval->type = X_NUM_DDEF ; }
#line 3589 "./yaccsrc.c"
    break;

  case 229: /* numeric_stat: numeric FORM form_expr error  */
#line 768 "./yaccsrc.y"
                                            { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 3595 "./yaccsrc.c"
    break;

  case 230: /* numeric_stat: numeric FORM form_expr  */
#line 769 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_NUM_F ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3603 "./yaccsrc.c"
    break;

  case 231: /* numeric_stat: numeric FORM  */
#line 772 "./yaccsrc.y"
                                       { /* NOTE! This clashes ANSI! */
                                         yyval = yyvsp[-1] ; yyval->type=X_NUM_FRMDEF ;}
#line 3610 "./yaccsrc.c"
    break;

  case 232: /* numeric_stat: numeric FORM VALUE expr  */
#line 774 "./yaccsrc.y"
                                       { yyval = yyvsp[-3] ; yyval->type=X_NUM_V ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3617 "./yaccsrc.c"
    break;

  case 233: /* numeric_stat: numeric FORM error  */
#line 776 "./yaccsrc.y"
                                  { exiterror( ERR_INV_SUBKEYWORD, 11, "ENGINEERING SCIENTIFIC", __reginatext ) ;}
#line 3623 "./yaccsrc.c"
    break;

  case 234: /* numeric_stat: numeric FUZZ  */
#line 777 "./yaccsrc.y"
                                       { yyval = yyvsp[-1]; yyval->type = X_NUM_FDEF ;}
#line 3629 "./yaccsrc.c"
    break;

  case 235: /* numeric_stat: numeric FUZZ expr  */
#line 778 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_NUM_FUZZ ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3637 "./yaccsrc.c"
    break;

  case 236: /* numeric_stat: numeric error  */
#line 781 "./yaccsrc.y"
                             { exiterror( ERR_INV_SUBKEYWORD, 15, "DIGITS FORM FUZZ", __reginatext ) ;}
#line 3643 "./yaccsrc.c"
    break;

  case 237: /* form_expr: SCIENTIFIC  */
#line 784 "./yaccsrc.y"
                                       { yyval = makenode(X_NUM_SCI,0) ; }
#line 3649 "./yaccsrc.c"
    break;

  case 238: /* form_expr: ENGINEERING  */
#line 785 "./yaccsrc.y"
                                       { yyval = makenode(X_NUM_ENG,0) ; }
#line 3655 "./yaccsrc.c"
    break;

  case 239: /* options_stat: options nexpr  */
#line 788 "./yaccsrc.y"
                                       { (yyval=yyvsp[-1])->p[0]=yyvsp[0] ; }
#line 3661 "./yaccsrc.c"
    break;

  case 240: /* parse_stat: parse parse_flags parse_param templs  */
#line 792 "./yaccsrc.y"
                                       { yyval = yyvsp[-3] ;
                                         yyval->type = X_PARSE ;
                                         yyval->u.parseflags = (int) yyvsp[-2] ;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 3671 "./yaccsrc.c"
    break;

  case 241: /* parse_stat: parse parse_param templs  */
#line 798 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_PARSE ;
                                         yyval->u.parseflags = 0;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 3681 "./yaccsrc.c"
    break;

  case 242: /* parse_stat: parse parse_flags error  */
#line 803 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 12, "ARG EXTERNAL LINEIN PULL SOURCE VAR VALUE VERSION", __reginatext ) ;}
#line 3687 "./yaccsrc.c"
    break;

  case 243: /* parse_stat: parse error  */
#line 804 "./yaccsrc.y"
                                       { exiterror( ERR_INV_SUBKEYWORD, 12, "ARG CASELESS EXTERNAL LINEIN LOWER PULL SOURCE UPPER VAR VALUE VERSION", __reginatext ) ;}
#line 3693 "./yaccsrc.c"
    break;

  case 244: /* parse_flags: UPPER  */
#line 807 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_UPPER  |
                                                         PARSE_NORMAL); }
#line 3700 "./yaccsrc.c"
    break;

  case 245: /* parse_flags: UPPER CASELESS  */
#line 809 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_UPPER  |
                                                         PARSE_CASELESS); }
#line 3707 "./yaccsrc.c"
    break;

  case 246: /* parse_flags: CASELESS UPPER  */
#line 811 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_UPPER  |
                                                         PARSE_CASELESS); }
#line 3714 "./yaccsrc.c"
    break;

  case 247: /* parse_flags: LOWER  */
#line 813 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_LOWER  |
                                                         PARSE_NORMAL); }
#line 3721 "./yaccsrc.c"
    break;

  case 248: /* parse_flags: LOWER CASELESS  */
#line 815 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_LOWER  |
                                                         PARSE_CASELESS); }
#line 3728 "./yaccsrc.c"
    break;

  case 249: /* parse_flags: CASELESS LOWER  */
#line 817 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_LOWER  |
                                                         PARSE_CASELESS); }
#line 3735 "./yaccsrc.c"
    break;

  case 250: /* parse_flags: CASELESS  */
#line 819 "./yaccsrc.y"
                                       { yyval = (nodeptr) (PARSE_NORMAL |
                                                         PARSE_CASELESS); }
#line 3742 "./yaccsrc.c"
    break;

  case 251: /* templs: templs ',' template  */
#line 823 "./yaccsrc.y"
                                       { /* fixes bugs like bug 579711 */
                                         yyval = optgluelast( yyvsp[-2], yyvsp[0] ); }
#line 3749 "./yaccsrc.c"
    break;

  case 252: /* templs: template  */
#line 825 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3755 "./yaccsrc.c"
    break;

  case 253: /* parse_param: ARG  */
#line 828 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_ARG,0) ; }
#line 3761 "./yaccsrc.c"
    break;

  case 254: /* parse_param: LINEIN  */
#line 829 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_EXT,0) ; }
#line 3767 "./yaccsrc.c"
    break;

  case 255: /* parse_param: EXTERNAL  */
#line 830 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_EXT,0) ; }
#line 3773 "./yaccsrc.c"
    break;

  case 256: /* parse_param: VERSION  */
#line 831 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_VER,0) ; }
#line 3779 "./yaccsrc.c"
    break;

  case 257: /* parse_param: PULL  */
#line 832 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_PULL,0) ; }
#line 3785 "./yaccsrc.c"
    break;

  case 258: /* parse_param: SOURCE  */
#line 833 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_SRC,0) ; }
#line 3791 "./yaccsrc.c"
    break;

  case 259: /* parse_param: VAR simsymb  */
#line 834 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_VAR,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 3798 "./yaccsrc.c"
    break;

  case 260: /* parse_param: VALUE nexpr WITH  */
#line 836 "./yaccsrc.y"
                                       { yyval = makenode(X_PARSE_VAL,1,yyvsp[-1]) ; }
#line 3804 "./yaccsrc.c"
    break;

  case 261: /* parse_param: VALUE error  */
#line 837 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_TEMPLATE, 3 ) ;}
#line 3810 "./yaccsrc.c"
    break;

  case 262: /* proc_stat: proc  */
#line 840 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3816 "./yaccsrc.c"
    break;

  case 263: /* proc_stat: proc error  */
#line 841 "./yaccsrc.y"
                          { exiterror( ERR_INV_SUBKEYWORD, 17, __reginatext ) ;}
#line 3822 "./yaccsrc.c"
    break;

  case 264: /* proc_stat: proc EXPOSE error  */
#line 842 "./yaccsrc.y"
                                 { exiterror( ERR_SYMBOL_EXPECTED, 1, __reginatext ) ;}
#line 3828 "./yaccsrc.c"
    break;

  case 265: /* proc_stat: proc EXPOSE anyvars error  */
#line 843 "./yaccsrc.y"
                                         { exiterror( ERR_SYMBOL_EXPECTED, 1, __reginatext ) ;}
#line 3834 "./yaccsrc.c"
    break;

  case 266: /* proc_stat: proc EXPOSE anyvars  */
#line 844 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3841 "./yaccsrc.c"
    break;

  case 267: /* pull_stat: pull template  */
#line 848 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3848 "./yaccsrc.c"
    break;

  case 268: /* push_stat: push nexpr  */
#line 852 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3855 "./yaccsrc.c"
    break;

  case 269: /* queue_stat: queue nexpr  */
#line 856 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3862 "./yaccsrc.c"
    break;

  case 270: /* say_stat: say nexpr  */
#line 860 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3869 "./yaccsrc.c"
    break;

  case 271: /* return_stat: return nexpr  */
#line 864 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 3876 "./yaccsrc.c"
    break;

  case 272: /* sel_end: END simsymb  */
#line 868 "./yaccsrc.y"
                                       { exiterror( ERR_UNMATCHED_END, 0 ) ;}
#line 3882 "./yaccsrc.c"
    break;

  case 273: /* sel_end: END simsymb error  */
#line 869 "./yaccsrc.y"
                                       { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 3888 "./yaccsrc.c"
    break;

  case 274: /* sel_end: END  */
#line 870 "./yaccsrc.y"
                                       { level--; }
#line 3894 "./yaccsrc.c"
    break;

  case 275: /* select_stat: select ncl when_stats otherwise_stat sel_end  */
#line 874 "./yaccsrc.y"
                                       { yyval = yyvsp[-4];
                                         yyval->p[0] = optgluelast( yyvsp[-3], yyvsp[-2] );
                                         yyval->p[0]->o.last = NULL;
                                         yyval->p[1] = yyvsp[-1]; }
#line 3903 "./yaccsrc.c"
    break;

  case 276: /* select_stat: select ncl END error  */
#line 878 "./yaccsrc.y"
                                       {  exiterror( ERR_WHEN_EXPECTED, 1, parser_data.select_linenr, __reginatext ) ;}
#line 3909 "./yaccsrc.c"
    break;

  case 277: /* select_stat: select ncl otherwise error  */
#line 880 "./yaccsrc.y"
                                       {  exiterror( ERR_WHEN_EXPECTED, 2, parser_data.select_linenr, __reginatext ) ;}
#line 3915 "./yaccsrc.c"
    break;

  case 278: /* select_stat: select error  */
#line 881 "./yaccsrc.y"
                                       {  exiterror( ERR_EXTRA_DATA, 1, __reginatext )  ;}
#line 3921 "./yaccsrc.c"
    break;

  case 279: /* select_stat: select ncl THEN  */
#line 882 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_UNEXPECTED, 0 ) ;}
#line 3927 "./yaccsrc.c"
    break;

  case 280: /* select_stat: select ncl when_stats otherwise error  */
#line 884 "./yaccsrc.y"
                                       {  exiterror( ERR_INCOMPLETE_STRUCT, 0 ) ;}
#line 3933 "./yaccsrc.c"
    break;

  case 281: /* when_stats: when_stats when_stat  */
#line 887 "./yaccsrc.y"
                                       { yyval = optgluelast( yyvsp[-1], yyvsp[0] ); }
#line 3939 "./yaccsrc.c"
    break;

  case 282: /* when_stats: when_stat  */
#line 888 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 3945 "./yaccsrc.c"
    break;

  case 283: /* when_stats: error  */
#line 889 "./yaccsrc.y"
                                       {  exiterror( ERR_WHEN_EXPECTED, 2, parser_data.select_linenr, __reginatext )  ;}
#line 3951 "./yaccsrc.c"
    break;

  case 284: /* when_stat: when expr nlncl THEN nlncl statement  */
#line 893 "./yaccsrc.y"
                                       { yyval = yyvsp[-5]; /* fixes bugs like bug 579711 */
                                         yyval->p[0] = optgluelast( yyvsp[-4], yyvsp[-3] );
                                         yyval->p[0]->o.last = NULL;
                                         yyval->p[1] = optgluelast( yyvsp[-1], yyvsp[0] );
                                         yyval->p[1]->o.last = NULL; }
#line 3961 "./yaccsrc.c"
    break;

  case 285: /* when_stat: when expr nlncl THEN nlncl statement THEN  */
#line 899 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_UNEXPECTED, 0 ) ;}
#line 3967 "./yaccsrc.c"
    break;

  case 286: /* when_stat: when expr  */
#line 900 "./yaccsrc.y"
                                       {  exiterror( ERR_THEN_EXPECTED, 2, parser_data.select_linenr, __reginatext )  ; }
#line 3973 "./yaccsrc.c"
    break;

  case 287: /* when_stat: when error  */
#line 901 "./yaccsrc.y"
                                       {  exiterror( ERR_INVALID_EXPRESSION, 0 ) ;}
#line 3979 "./yaccsrc.c"
    break;

  case 288: /* ex_when_stat: gruff when  */
#line 911 "./yaccsrc.y"
                                       { exiterror( ERR_WHEN_UNEXPECTED, 1 ); }
#line 3985 "./yaccsrc.c"
    break;

  case 289: /* ex_when_stat: gruff otherwise  */
#line 912 "./yaccsrc.y"
                                       { exiterror( ERR_WHEN_UNEXPECTED, 2 ); }
#line 3991 "./yaccsrc.c"
    break;

  case 290: /* otherwise_stat: otherwise nlncl nxstats  */
#line 915 "./yaccsrc.y"
                                         { yyval = yyvsp[-2];
                                         yyval->p[0] = optgluelast( yyvsp[-1], yyvsp[0] );
                                         if ( yyval->p[0] )
                                            yyval->p[0]->o.last = NULL; }
#line 4000 "./yaccsrc.c"
    break;

  case 291: /* otherwise_stat: %empty  */
#line 919 "./yaccsrc.y"
                                       { yyval = makenode(X_NO_OTHERWISE,0) ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->charnr = parser_data.tstart ; }
#line 4008 "./yaccsrc.c"
    break;

  case 292: /* signal_stat: signal VALUE expr  */
#line 925 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_SIG_VAL ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 4016 "./yaccsrc.c"
    break;

  case 293: /* signal_stat: signal signal_name error  */
#line 928 "./yaccsrc.y"
                                       { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 4022 "./yaccsrc.c"
    break;

  case 294: /* signal_stat: signal signal_name  */
#line 929 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->name = (streng *)yyvsp[0] ; }
#line 4029 "./yaccsrc.c"
    break;

  case 295: /* signal_stat: signal on error  */
#line 931 "./yaccsrc.y"
                              { exiterror( ERR_INV_SUBKEYWORD, 3, "ERROR FAILURE HALT NOTREADY NOVALUE SYNTAX LOSTDIGITS", __reginatext ) ;}
#line 4035 "./yaccsrc.c"
    break;

  case 296: /* signal_stat: signal off error  */
#line 932 "./yaccsrc.y"
                               { exiterror( ERR_INV_SUBKEYWORD, 4, "ERROR FAILURE HALT NOTREADY NOVALUE SYNTAX LOSTDIGITS", __reginatext ) ;}
#line 4041 "./yaccsrc.c"
    break;

  case 297: /* signal_stat: signal on s_action error  */
#line 933 "./yaccsrc.y"
                                       { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 4047 "./yaccsrc.c"
    break;

  case 298: /* signal_stat: signal on s_action namespec error  */
#line 934 "./yaccsrc.y"
                                                { exiterror( ERR_STRING_EXPECTED, 3, __reginatext ) ;}
#line 4053 "./yaccsrc.c"
    break;

  case 299: /* signal_stat: signal on s_action namespec  */
#line 936 "./yaccsrc.y"
                                       { yyval = yyvsp[-3] ;
                                         yyval->type = X_SIG_SET ;
                                         yyval->p[0] = yyvsp[-2] ;
                                         yyval->name = (streng *)yyvsp[0] ;
                                         yyval->p[1] = yyvsp[-1] ; }
#line 4063 "./yaccsrc.c"
    break;

  case 300: /* signal_stat: signal on s_action  */
#line 941 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_SIG_SET ;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 4072 "./yaccsrc.c"
    break;

  case 301: /* signal_stat: signal off s_action error  */
#line 945 "./yaccsrc.y"
                                        { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 4078 "./yaccsrc.c"
    break;

  case 302: /* signal_stat: signal off s_action  */
#line 946 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->type = X_SIG_SET ;
                                         yyval->p[0] = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ; }
#line 4087 "./yaccsrc.c"
    break;

  case 303: /* signal_name: asymbol  */
#line 952 "./yaccsrc.y"
                                       { yyval = yyvsp[0]; }
#line 4093 "./yaccsrc.c"
    break;

  case 304: /* signal_name: STRING  */
#line 953 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 4099 "./yaccsrc.c"
    break;

  case 305: /* signal_name: error  */
#line 954 "./yaccsrc.y"
                                       { exiterror( ERR_STRING_EXPECTED, 4, __reginatext );}
#line 4105 "./yaccsrc.c"
    break;

  case 306: /* namespec: NAME SIMSYMBOL  */
#line 957 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue);}
#line 4111 "./yaccsrc.c"
    break;

  case 307: /* namespec: NAME STRING  */
#line 958 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 4117 "./yaccsrc.c"
    break;

  case 308: /* namespec: NAME error  */
#line 959 "./yaccsrc.y"
                                       { exiterror( ERR_STRING_EXPECTED, 3, __reginatext ) ;}
#line 4123 "./yaccsrc.c"
    break;

  case 309: /* asymbol: CONSYMBOL  */
#line 962 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 4129 "./yaccsrc.c"
    break;

  case 310: /* asymbol: SIMSYMBOL  */
#line 963 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 4135 "./yaccsrc.c"
    break;

  case 311: /* on: ON  */
#line 966 "./yaccsrc.y"
                                       { yyval = makenode(X_ON,0) ; }
#line 4141 "./yaccsrc.c"
    break;

  case 312: /* off: OFF  */
#line 969 "./yaccsrc.y"
                                       { yyval = makenode(X_OFF,0) ; }
#line 4147 "./yaccsrc.c"
    break;

  case 313: /* c_action: ERROR  */
#line 972 "./yaccsrc.y"
                                       { yyval = makenode(X_S_ERROR,0) ; }
#line 4153 "./yaccsrc.c"
    break;

  case 314: /* c_action: HALT  */
#line 973 "./yaccsrc.y"
                                       { yyval = makenode(X_S_HALT,0) ; }
#line 4159 "./yaccsrc.c"
    break;

  case 315: /* c_action: NOTREADY  */
#line 974 "./yaccsrc.y"
                                       { yyval = makenode(X_S_NOTREADY,0) ; }
#line 4165 "./yaccsrc.c"
    break;

  case 316: /* c_action: FAILURE  */
#line 975 "./yaccsrc.y"
                                       { yyval = makenode(X_S_FAILURE,0) ; }
#line 4171 "./yaccsrc.c"
    break;

  case 317: /* s_action: c_action  */
#line 978 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 4177 "./yaccsrc.c"
    break;

  case 318: /* s_action: NOVALUE  */
#line 979 "./yaccsrc.y"
                                       { yyval = makenode(X_S_NOVALUE,0) ; }
#line 4183 "./yaccsrc.c"
    break;

  case 319: /* s_action: SYNTAX  */
#line 980 "./yaccsrc.y"
                                       { yyval = makenode(X_S_SYNTAX,0) ; }
#line 4189 "./yaccsrc.c"
    break;

  case 320: /* s_action: LOSTDIGITS  */
#line 981 "./yaccsrc.y"
                                       { yyval = makenode(X_S_LOSTDIGITS,0) ; }
#line 4195 "./yaccsrc.c"
    break;

  case 321: /* trace_stat: trace VALUE expr  */
#line 984 "./yaccsrc.y"
                                       { yyval = yyvsp[-2] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 4202 "./yaccsrc.c"
    break;

  case 322: /* trace_stat: trace nexpr  */
#line 986 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[0] = yyvsp[0] ; }
#line 4209 "./yaccsrc.c"
    break;

  case 323: /* trace_stat: trace whatever error  */
#line 988 "./yaccsrc.y"
                                   { exiterror( ERR_EXTRA_DATA, 1, __reginatext ) ;}
#line 4215 "./yaccsrc.c"
    break;

  case 324: /* trace_stat: trace whatever  */
#line 989 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 4222 "./yaccsrc.c"
    break;

  case 325: /* whatever: WHATEVER  */
#line 993 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 4228 "./yaccsrc.c"
    break;

  case 326: /* assignment: ass_part nexpr  */
#line 997 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         /*
                                          * An assignment is a numerical
                                          * assignment if and only if we have
                                          * to do a numerical operation, which
                                          * is equivalent to the existence of
                                          * one more argument to $2.
                                          * This fixes bug 720166.
                                          */
                                         if (yyvsp[0] &&
                                             yyvsp[0]->p[0] &&
                                             gettypeof(yyvsp[0]) == IS_A_NUMBER)
                                            yyval->type = X_NASSIGN ; }
#line 4247 "./yaccsrc.c"
    break;

  case 327: /* ass_part: ASSIGNMENTVARIABLE  */
#line 1013 "./yaccsrc.y"
                                       { yyval = makenode(X_ASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4256 "./yaccsrc.c"
    break;

  case 328: /* plusassignment: plus_ass_part nexpr  */
#line 1019 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "+=" );
                                       }
#line 4266 "./yaccsrc.c"
    break;

  case 329: /* plus_ass_part: PLUSASSIGNMENTVARIABLE  */
#line 1026 "./yaccsrc.y"
                                       { yyval = makenode(X_PLUSASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4275 "./yaccsrc.c"
    break;

  case 330: /* minusassignment: minus_ass_part nexpr  */
#line 1032 "./yaccsrc.y"
                                         { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "-=" );
                                       }
#line 4285 "./yaccsrc.c"
    break;

  case 331: /* minus_ass_part: MINUSASSIGNMENTVARIABLE  */
#line 1039 "./yaccsrc.y"
                                         { yyval = makenode(X_MINUSASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4294 "./yaccsrc.c"
    break;

  case 332: /* multassignment: mult_ass_part nexpr  */
#line 1045 "./yaccsrc.y"
                                       { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "*=" );
                                       }
#line 4304 "./yaccsrc.c"
    break;

  case 333: /* mult_ass_part: MULTASSIGNMENTVARIABLE  */
#line 1052 "./yaccsrc.y"
                                       { yyval = makenode(X_MULTASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4313 "./yaccsrc.c"
    break;

  case 334: /* divassignment: div_ass_part nexpr  */
#line 1058 "./yaccsrc.y"
                                     { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "/=" );
                                       }
#line 4323 "./yaccsrc.c"
    break;

  case 335: /* div_ass_part: DIVASSIGNMENTVARIABLE  */
#line 1065 "./yaccsrc.y"
                                     { yyval = makenode(X_DIVASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4332 "./yaccsrc.c"
    break;

  case 336: /* intdivassignment: intdiv_ass_part nexpr  */
#line 1071 "./yaccsrc.y"
                                           { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "%=" );
                                       }
#line 4342 "./yaccsrc.c"
    break;

  case 337: /* intdiv_ass_part: INTDIVASSIGNMENTVARIABLE  */
#line 1078 "./yaccsrc.y"
                                           { yyval = makenode(X_INTDIVASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4351 "./yaccsrc.c"
    break;

  case 338: /* modulusassignment: modulus_ass_part nexpr  */
#line 1084 "./yaccsrc.y"
                                             { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "//=" );
                                       }
#line 4361 "./yaccsrc.c"
    break;

  case 339: /* modulus_ass_part: MODULUSASSIGNMENTVARIABLE  */
#line 1091 "./yaccsrc.y"
                                             { yyval = makenode(X_MODULUSASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4370 "./yaccsrc.c"
    break;

  case 340: /* andassignment: and_ass_part nexpr  */
#line 1097 "./yaccsrc.y"
                                     { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "&=" );
                                       }
#line 4380 "./yaccsrc.c"
    break;

  case 341: /* and_ass_part: ANDASSIGNMENTVARIABLE  */
#line 1104 "./yaccsrc.y"
                                     { yyval = makenode(X_ANDASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4389 "./yaccsrc.c"
    break;

  case 342: /* xorassignment: xor_ass_part nexpr  */
#line 1110 "./yaccsrc.y"
                                     { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "&&=" );
                                       }
#line 4399 "./yaccsrc.c"
    break;

  case 343: /* xor_ass_part: XORASSIGNMENTVARIABLE  */
#line 1117 "./yaccsrc.y"
                                     { yyval = makenode(X_XORASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4408 "./yaccsrc.c"
    break;

  case 344: /* orassignment: or_ass_part nexpr  */
#line 1123 "./yaccsrc.y"
                                   { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                         if (yyvsp[0]==NULL)
                                            exiterror( ERR_BAD_ARITHMETIC, 2, "", "|=" );
                                       }
#line 4418 "./yaccsrc.c"
    break;

  case 345: /* or_ass_part: ORASSIGNMENTVARIABLE  */
#line 1130 "./yaccsrc.y"
                                   { yyval = makenode(X_ORASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4427 "./yaccsrc.c"
    break;

  case 346: /* concatassignment: concat_ass_part nexpr  */
#line 1136 "./yaccsrc.y"
                                           { yyval = yyvsp[-1] ;
                                         yyval->p[1] = yyvsp[0] ;
                                       }
#line 4435 "./yaccsrc.c"
    break;

  case 347: /* concat_ass_part: CONCATASSIGNMENTVARIABLE  */
#line 1141 "./yaccsrc.y"
                                           { yyval = makenode(X_CONCATASSIGN,0) ;
                                         yyval->charnr = parser_data.tstart ;
                                         yyval->lineno = parser_data.tline ;
                                         yyval->p[0] = (nodeptr)create_head( (const char *)retvalue ); }
#line 4444 "./yaccsrc.c"
    break;

  case 348: /* $@9: %empty  */
#line 1148 "./yaccsrc.y"
                                       { /* We have to accept exprs here even
                                          * if we just want to accept
                                          * '(' expr ')' only. We do this
                                          * because we appect
                                          * "call '(' exprs ')'" too.
                                          * This kann happen only if the
                                          * related control flag parendepth is
                                          * set. But since the parentheses are
                                          * voided just for the outer ones, we
                                          * can reduce the allowness level.
                                          * We don't have to set it back,
                                          * because the outer parentheses
                                          * either is THE one we look for or
                                          * none. This allows a faster error
                                          * detection and that's a good goal.*/
                                         parendepth--; }
#line 4465 "./yaccsrc.c"
    break;

  case 349: /* expr: '(' $@9 exprs_sub  */
#line 1164 "./yaccsrc.y"
                                       { parendepth++;
                                         if ( parendepth == 1 )
                                         {
                                            /* exprs on as-is basis */
                                            yyval = yyvsp[0];
                                         }
                                         else
                                         {
                                            /* Must already be a plain expr.
                                             * The nexpr part of exprs detects
                                             * mistakes. */
                                            yyval = reduce_expr_list( yyvsp[0],
                                                                REDUCE_EXPR );
                                            if ( yyval == yyvsp[0] )
                                               exiterror( ERR_INTERPRETER_FAILURE, 1, __FILE__, __LINE__, "Reduction of `exprs' not happened." );
                                         }
                                       }
#line 4487 "./yaccsrc.c"
    break;

  case 350: /* $@10: %empty  */
#line 1181 "./yaccsrc.y"
                                       { parendepth--; }
#line 4493 "./yaccsrc.c"
    break;

  case 351: /* expr: expr '+' $@10 expr  */
#line 1182 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "+" );
                                         yyval = makenode( X_PLUSS, 2, yyvsp[-3], yyvsp[0] ); }
#line 4500 "./yaccsrc.c"
    break;

  case 352: /* $@11: %empty  */
#line 1184 "./yaccsrc.y"
                                       { parendepth--; }
#line 4506 "./yaccsrc.c"
    break;

  case 353: /* expr: expr '-' $@11 expr  */
#line 1185 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "-" );
                                         yyval = makenode( X_MINUS, 2, yyvsp[-3], yyvsp[0] ); }
#line 4513 "./yaccsrc.c"
    break;

  case 354: /* $@12: %empty  */
#line 1187 "./yaccsrc.y"
                                       { parendepth--; }
#line 4519 "./yaccsrc.c"
    break;

  case 355: /* expr: expr '*' $@12 expr  */
#line 1188 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "*" );
                                         yyval = makenode( X_MULT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4526 "./yaccsrc.c"
    break;

  case 356: /* expr: '*'  */
#line 1190 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "*" ); }
#line 4532 "./yaccsrc.c"
    break;

  case 357: /* $@13: %empty  */
#line 1191 "./yaccsrc.y"
                                       { parendepth--; }
#line 4538 "./yaccsrc.c"
    break;

  case 358: /* expr: expr '/' $@13 expr  */
#line 1192 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "/" );
                                         yyval = makenode( X_DEVIDE, 2, yyvsp[-3], yyvsp[0] ); }
#line 4545 "./yaccsrc.c"
    break;

  case 359: /* expr: '/'  */
#line 1194 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "/" ); }
#line 4551 "./yaccsrc.c"
    break;

  case 360: /* $@14: %empty  */
#line 1195 "./yaccsrc.y"
                                       { parendepth--; }
#line 4557 "./yaccsrc.c"
    break;

  case 361: /* expr: expr MODULUS $@14 expr  */
#line 1196 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "//" );
                                         yyval = makenode( X_MODULUS, 2, yyvsp[-3], yyvsp[0] ); }
#line 4564 "./yaccsrc.c"
    break;

  case 362: /* expr: MODULUS  */
#line 1198 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "//" ); }
#line 4570 "./yaccsrc.c"
    break;

  case 363: /* $@15: %empty  */
#line 1199 "./yaccsrc.y"
                                       { parendepth--; }
#line 4576 "./yaccsrc.c"
    break;

  case 364: /* expr: expr '%' $@15 expr  */
#line 1200 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "%" );
                                         yyval = makenode( X_INTDIV, 2, yyvsp[-3], yyvsp[0] ); }
#line 4583 "./yaccsrc.c"
    break;

  case 365: /* expr: '%'  */
#line 1202 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "%" ); }
#line 4589 "./yaccsrc.c"
    break;

  case 366: /* $@16: %empty  */
#line 1203 "./yaccsrc.y"
                                       { parendepth--; }
#line 4595 "./yaccsrc.c"
    break;

  case 367: /* expr: expr '|' $@16 expr  */
#line 1204 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "|" );
                                         yyval = makenode( X_LOG_OR, 2, yyvsp[-3], yyvsp[0] ); }
#line 4602 "./yaccsrc.c"
    break;

  case 368: /* expr: '|'  */
#line 1206 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "|" ); }
#line 4608 "./yaccsrc.c"
    break;

  case 369: /* $@17: %empty  */
#line 1207 "./yaccsrc.y"
                                       { parendepth--; }
#line 4614 "./yaccsrc.c"
    break;

  case 370: /* expr: expr '&' $@17 expr  */
#line 1208 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "&" );
                                         yyval = makenode( X_LOG_AND, 2, yyvsp[-3], yyvsp[0] ); }
#line 4621 "./yaccsrc.c"
    break;

  case 371: /* expr: '&'  */
#line 1210 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "&" ); }
#line 4627 "./yaccsrc.c"
    break;

  case 372: /* $@18: %empty  */
#line 1211 "./yaccsrc.y"
                                       { parendepth--; }
#line 4633 "./yaccsrc.c"
    break;

  case 373: /* expr: expr XOR $@18 expr  */
#line 1212 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "&&" );
                                         yyval = makenode( X_LOG_XOR, 2, yyvsp[-3], yyvsp[0] ); }
#line 4640 "./yaccsrc.c"
    break;

  case 374: /* expr: XOR  */
#line 1214 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "&&" ); }
#line 4646 "./yaccsrc.c"
    break;

  case 375: /* $@19: %empty  */
#line 1215 "./yaccsrc.y"
                                       { parendepth--; }
#line 4652 "./yaccsrc.c"
    break;

  case 376: /* expr: expr EXP $@19 expr  */
#line 1216 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "**" );
                                         yyval = makenode( X_EXP, 2, yyvsp[-3], yyvsp[0] ); }
#line 4659 "./yaccsrc.c"
    break;

  case 377: /* expr: EXP  */
#line 1218 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "**" ); }
#line 4665 "./yaccsrc.c"
    break;

  case 378: /* $@20: %empty  */
#line 1219 "./yaccsrc.y"
                                       { parendepth--; }
#line 4671 "./yaccsrc.c"
    break;

  case 379: /* expr: expr SPACE $@20 expr  */
#line 1220 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], " " );
                                         yyval = makenode( X_SPACE, 2, yyvsp[-3], yyvsp[0] ); }
#line 4678 "./yaccsrc.c"
    break;

  case 380: /* expr: SPACE  */
#line 1222 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, " " ); }
#line 4684 "./yaccsrc.c"
    break;

  case 381: /* $@21: %empty  */
#line 1223 "./yaccsrc.y"
                                       { parendepth--; }
#line 4690 "./yaccsrc.c"
    break;

  case 382: /* expr: expr CONCATENATE $@21 expr  */
#line 1224 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "||" );
                                         yyval = makenode( X_CONCAT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4697 "./yaccsrc.c"
    break;

  case 383: /* expr: CONCATENATE  */
#line 1226 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "||" ); }
#line 4703 "./yaccsrc.c"
    break;

  case 384: /* expr: NOT expr  */
#line 1227 "./yaccsrc.y"
                                       { AUTO_REDUCE( yyvsp[0], "\\" );
                                         yyval = makenode( X_LOG_NOT, 1, yyvsp[0] ); }
#line 4710 "./yaccsrc.c"
    break;

  case 385: /* expr: NOT  */
#line 1229 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "\\" ); }
#line 4716 "./yaccsrc.c"
    break;

  case 386: /* $@22: %empty  */
#line 1230 "./yaccsrc.y"
                                       { parendepth--; }
#line 4722 "./yaccsrc.c"
    break;

  case 387: /* expr: expr '=' $@22 expr  */
#line 1231 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "=" );
                                         yyval = makenode( X_EQUAL, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ); }
#line 4730 "./yaccsrc.c"
    break;

  case 388: /* expr: '='  */
#line 1234 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "=" ); }
#line 4736 "./yaccsrc.c"
    break;

  case 389: /* $@23: %empty  */
#line 1235 "./yaccsrc.y"
                                       { parendepth--; }
#line 4742 "./yaccsrc.c"
    break;

  case 390: /* expr: expr GTE $@23 expr  */
#line 1236 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], ">=" );
                                         yyval = makenode( X_GTE, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ) ; }
#line 4750 "./yaccsrc.c"
    break;

  case 391: /* expr: GTE  */
#line 1239 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, ">=" ); }
#line 4756 "./yaccsrc.c"
    break;

  case 392: /* $@24: %empty  */
#line 1240 "./yaccsrc.y"
                                       { parendepth--; }
#line 4762 "./yaccsrc.c"
    break;

  case 393: /* expr: expr LTE $@24 expr  */
#line 1241 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "<=" );
                                         yyval = makenode( X_LTE, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ) ; }
#line 4770 "./yaccsrc.c"
    break;

  case 394: /* expr: LTE  */
#line 1244 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "<=" ); }
#line 4776 "./yaccsrc.c"
    break;

  case 395: /* $@25: %empty  */
#line 1245 "./yaccsrc.y"
                                       { parendepth--; }
#line 4782 "./yaccsrc.c"
    break;

  case 396: /* expr: expr GT $@25 expr  */
#line 1246 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], ">" );
                                         yyval = makenode( X_GT, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ) ; }
#line 4790 "./yaccsrc.c"
    break;

  case 397: /* expr: GT  */
#line 1249 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, ">" ); }
#line 4796 "./yaccsrc.c"
    break;

  case 398: /* $@26: %empty  */
#line 1250 "./yaccsrc.y"
                                       { parendepth--; }
#line 4802 "./yaccsrc.c"
    break;

  case 399: /* expr: expr LT $@26 expr  */
#line 1251 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "<" );
                                         yyval = makenode( X_LT, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ) ; }
#line 4810 "./yaccsrc.c"
    break;

  case 400: /* expr: LT  */
#line 1254 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "<" ); }
#line 4816 "./yaccsrc.c"
    break;

  case 401: /* $@27: %empty  */
#line 1255 "./yaccsrc.y"
                                       { parendepth--; }
#line 4822 "./yaccsrc.c"
    break;

  case 402: /* expr: expr DIFFERENT $@27 expr  */
#line 1256 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "\\=" );
                                         yyval = makenode( X_DIFF, 2, yyvsp[-3], yyvsp[0] );
                                         transform( yyval ) ; }
#line 4830 "./yaccsrc.c"
    break;

  case 403: /* expr: DIFFERENT  */
#line 1259 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "\\=" ); }
#line 4836 "./yaccsrc.c"
    break;

  case 404: /* $@28: %empty  */
#line 1260 "./yaccsrc.y"
                                       { parendepth--; }
#line 4842 "./yaccsrc.c"
    break;

  case 405: /* expr: expr EQUALEQUAL $@28 expr  */
#line 1261 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "==" );
                                         yyval = makenode( X_S_EQUAL, 2, yyvsp[-3], yyvsp[0] ); }
#line 4849 "./yaccsrc.c"
    break;

  case 406: /* expr: EQUALEQUAL  */
#line 1263 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "==" ); }
#line 4855 "./yaccsrc.c"
    break;

  case 407: /* $@29: %empty  */
#line 1264 "./yaccsrc.y"
                                       { parendepth--; }
#line 4861 "./yaccsrc.c"
    break;

  case 408: /* expr: expr NOTEQUALEQUAL $@29 expr  */
#line 1265 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "\\==" );
                                         yyval = makenode( X_S_DIFF, 2, yyvsp[-3], yyvsp[0] ); }
#line 4868 "./yaccsrc.c"
    break;

  case 409: /* expr: NOTEQUALEQUAL  */
#line 1267 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "\\==" ); }
#line 4874 "./yaccsrc.c"
    break;

  case 410: /* $@30: %empty  */
#line 1268 "./yaccsrc.y"
                                       { parendepth--; }
#line 4880 "./yaccsrc.c"
    break;

  case 411: /* expr: expr GTGT $@30 expr  */
#line 1269 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], ">>" );
                                         yyval = makenode( X_S_GT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4887 "./yaccsrc.c"
    break;

  case 412: /* expr: GTGT  */
#line 1271 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, ">>" ); }
#line 4893 "./yaccsrc.c"
    break;

  case 413: /* $@31: %empty  */
#line 1272 "./yaccsrc.y"
                                       { parendepth--; }
#line 4899 "./yaccsrc.c"
    break;

  case 414: /* expr: expr LTLT $@31 expr  */
#line 1273 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "<<" );
                                         yyval = makenode( X_S_LT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4906 "./yaccsrc.c"
    break;

  case 415: /* expr: LTLT  */
#line 1275 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "<<" ); }
#line 4912 "./yaccsrc.c"
    break;

  case 416: /* $@32: %empty  */
#line 1276 "./yaccsrc.y"
                                       { parendepth--; }
#line 4918 "./yaccsrc.c"
    break;

  case 417: /* expr: expr NOTGTGT $@32 expr  */
#line 1277 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "\\>>" );
                                         yyval = makenode( X_S_NGT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4925 "./yaccsrc.c"
    break;

  case 418: /* expr: NOTGTGT  */
#line 1279 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "\\>>" ); }
#line 4931 "./yaccsrc.c"
    break;

  case 419: /* $@33: %empty  */
#line 1280 "./yaccsrc.y"
                                       { parendepth--; }
#line 4937 "./yaccsrc.c"
    break;

  case 420: /* expr: expr NOTLTLT $@33 expr  */
#line 1281 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "\\<<" );
                                         yyval = makenode( X_S_NLT, 2, yyvsp[-3], yyvsp[0] ); }
#line 4944 "./yaccsrc.c"
    break;

  case 421: /* expr: NOTLTLT  */
#line 1283 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "\\<<" ); }
#line 4950 "./yaccsrc.c"
    break;

  case 422: /* $@34: %empty  */
#line 1284 "./yaccsrc.y"
                                       { parendepth--; }
#line 4956 "./yaccsrc.c"
    break;

  case 423: /* expr: expr GTGTE $@34 expr  */
#line 1285 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], ">>=" );
                                         yyval = makenode( X_S_GTE, 2, yyvsp[-3], yyvsp[0] ); }
#line 4963 "./yaccsrc.c"
    break;

  case 424: /* expr: GTGTE  */
#line 1287 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, ">>=" ); }
#line 4969 "./yaccsrc.c"
    break;

  case 425: /* $@35: %empty  */
#line 1288 "./yaccsrc.y"
                                       { parendepth--; }
#line 4975 "./yaccsrc.c"
    break;

  case 426: /* expr: expr LTLTE $@35 expr  */
#line 1289 "./yaccsrc.y"
                                       { parendepth++; AUTO_REDUCE( yyvsp[-3], "<<=" );
                                         yyval = makenode( X_S_LTE, 2, yyvsp[-3], yyvsp[0] ); }
#line 4982 "./yaccsrc.c"
    break;

  case 427: /* expr: LTLTE  */
#line 1291 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, "<<=" ); }
#line 4988 "./yaccsrc.c"
    break;

  case 428: /* expr: symbtree  */
#line 1292 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 4994 "./yaccsrc.c"
    break;

  case 429: /* expr: CONSYMBOL  */
#line 1293 "./yaccsrc.y"
                                       { yyval = makenode( X_STRING, 0 );
                                         yyval->name = Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5001 "./yaccsrc.c"
    break;

  case 430: /* expr: HEXSTRING  */
#line 1295 "./yaccsrc.y"
                                       { yyval = makenode( X_STRING, 0 );
                                         yyval->name = Str_make_TSD(parser_data.TSD,retlength) ;
                                         memcpy(yyval->name->value,retvalue,
                                                    yyval->name->len=retlength); }
#line 5010 "./yaccsrc.c"
    break;

  case 431: /* expr: BINSTRING  */
#line 1299 "./yaccsrc.y"
                                       { yyval = makenode( X_STRING, 0 );
                                         yyval->name = Str_make_TSD(parser_data.TSD,retlength) ;
                                         memcpy(yyval->name->value,retvalue,
                                                    yyval->name->len=retlength); }
#line 5019 "./yaccsrc.c"
    break;

  case 432: /* expr: STRING  */
#line 1303 "./yaccsrc.y"
                                       { yyval = makenode( X_STRING, 0 );
                                         yyval->name = Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5026 "./yaccsrc.c"
    break;

  case 433: /* expr: function  */
#line 1305 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 5032 "./yaccsrc.c"
    break;

  case 434: /* expr: '+' expr  */
#line 1306 "./yaccsrc.y"
                                       { AUTO_REDUCE( yyvsp[0], nullptr );
                                         yyval = makenode( X_U_PLUSS, 1, yyvsp[0] ); }
#line 5039 "./yaccsrc.c"
    break;

  case 435: /* expr: '-' expr  */
#line 1308 "./yaccsrc.y"
                                       { AUTO_REDUCE( yyvsp[0], nullptr );
                                         yyval = makenode( X_U_MINUS, 1, yyvsp[0] ); }
#line 5046 "./yaccsrc.c"
    break;

  case 436: /* expr: '+' error  */
#line 1310 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, __reginatext ); }
#line 5052 "./yaccsrc.c"
    break;

  case 437: /* expr: '-' error  */
#line 1311 "./yaccsrc.y"
                                       { exiterror( ERR_INVALID_EXPRESSION, 1, __reginatext ); }
#line 5058 "./yaccsrc.c"
    break;

  case 438: /* exprs_sub: exprs ')'  */
#line 1314 "./yaccsrc.y"
                                       { yyval = yyvsp[-1]; }
#line 5064 "./yaccsrc.c"
    break;

  case 439: /* exprs_sub: exprs error  */
#line 1315 "./yaccsrc.y"
                                       { exiterror( ERR_UNMATCHED_PARAN, 0 ); }
#line 5070 "./yaccsrc.c"
    break;

  case 440: /* exprs_sub: STATSEP  */
#line 1316 "./yaccsrc.y"
                                       { exiterror( ERR_UNMATCHED_PARAN, 0 ); }
#line 5076 "./yaccsrc.c"
    break;

  case 441: /* symbtree: SIMSYMBOL  */
#line 1319 "./yaccsrc.y"
                                       { yyval = (nodeptr)create_head( (const char *)retvalue ) ; }
#line 5082 "./yaccsrc.c"
    break;

  case 442: /* function: extfunc func_args  */
#line 1323 "./yaccsrc.y"
                                       { yyval = makenode(X_EX_FUNC,1,yyvsp[0]) ;
                                         yyval->name = (streng *)yyvsp[-1] ; }
#line 5089 "./yaccsrc.c"
    break;

  case 443: /* function: intfunc func_args  */
#line 1325 "./yaccsrc.y"
                                       { yyval = makenode(X_IN_FUNC,1,yyvsp[0]) ;
                                         yyval->name = (streng *)yyvsp[-1] ; }
#line 5096 "./yaccsrc.c"
    break;

  case 444: /* @36: %empty  */
#line 1329 "./yaccsrc.y"
                                       { /* ugly fake preservs parendepth */
                                         yyval = (YYSTYPE) parendepth;
                                         parendepth = 0; }
#line 5104 "./yaccsrc.c"
    break;

  case 445: /* func_args: @36 exprs_sub  */
#line 1332 "./yaccsrc.y"
                                       { parendepth = (int) yyval;
                                         yyval = yyvsp[0]; }
#line 5111 "./yaccsrc.c"
    break;

  case 446: /* intfunc: INFUNCNAME  */
#line 1336 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5117 "./yaccsrc.c"
    break;

  case 447: /* extfunc: EXFUNCNAME  */
#line 1339 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5123 "./yaccsrc.c"
    break;

  case 448: /* template: pv solid template  */
#line 1342 "./yaccsrc.y"
                                       { yyval =makenode(X_TPL_SOLID,3,yyvsp[-2],yyvsp[-1],yyvsp[0]);}
#line 5129 "./yaccsrc.c"
    break;

  case 449: /* template: pv  */
#line 1343 "./yaccsrc.y"
                                       { yyval =makenode(X_TPL_SOLID,1,yyvsp[0]) ; }
#line 5135 "./yaccsrc.c"
    break;

  case 450: /* template: error  */
#line 1344 "./yaccsrc.y"
                    { exiterror( ERR_INVALID_TEMPLATE, 1, __reginatext ) ;}
#line 5141 "./yaccsrc.c"
    break;

  case 451: /* solid: '-' offset  */
#line 1347 "./yaccsrc.y"
                                       { yyval = makenode(X_NEG_OFFS,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5148 "./yaccsrc.c"
    break;

  case 452: /* solid: '+' offset  */
#line 1349 "./yaccsrc.y"
                                       { yyval = makenode(X_POS_OFFS,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5155 "./yaccsrc.c"
    break;

  case 453: /* solid: offset  */
#line 1351 "./yaccsrc.y"
                                       { yyval = makenode(X_ABS_OFFS,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5162 "./yaccsrc.c"
    break;

  case 454: /* solid: '=' offset  */
#line 1353 "./yaccsrc.y"
                                       { yyval = makenode(X_ABS_OFFS,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5169 "./yaccsrc.c"
    break;

  case 455: /* solid: '(' symbtree ')'  */
#line 1355 "./yaccsrc.y"
                                       { yyval = makenode(X_TPL_VAR,0) ;
                                         yyval->p[0] = yyvsp[-1] ; }
#line 5176 "./yaccsrc.c"
    break;

  case 456: /* solid: '-' '(' symbtree ')'  */
#line 1357 "./yaccsrc.y"
                                       { yyval = makenode(X_NEG_OFFS,0) ;
                                         yyval->p[0] = yyvsp[-1] ; }
#line 5183 "./yaccsrc.c"
    break;

  case 457: /* solid: '+' '(' symbtree ')'  */
#line 1359 "./yaccsrc.y"
                                       { yyval = makenode(X_POS_OFFS,0) ;
                                         yyval->p[0] = yyvsp[-1] ; }
#line 5190 "./yaccsrc.c"
    break;

  case 458: /* solid: '=' '(' symbtree ')'  */
#line 1361 "./yaccsrc.y"
                                       { yyval = makenode(X_ABS_OFFS,0) ;
                                         yyval->p[0] = yyvsp[-1] ; }
#line 5197 "./yaccsrc.c"
    break;

  case 459: /* solid: string  */
#line 1363 "./yaccsrc.y"
                                       { yyval = makenode(X_TPL_MVE,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5204 "./yaccsrc.c"
    break;

  case 460: /* offset: OFFSET  */
#line 1367 "./yaccsrc.y"
                                       { yyval = (nodeptr)Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5210 "./yaccsrc.c"
    break;

  case 461: /* offset: CONSYMBOL  */
#line 1368 "./yaccsrc.y"
                                       { streng *sptr = Str_cre_TSD(parser_data.TSD,retvalue) ;
                                          if (myisnumber(parser_data.TSD, sptr))
                                          {
                                             exiterror( ERR_INVALID_INTEGER, 4, sptr->value ) ;
                                          }
                                          else
                                             exiterror( ERR_INVALID_TEMPLATE, 0 ) ;}
#line 5222 "./yaccsrc.c"
    break;

  case 462: /* string: STRING  */
#line 1377 "./yaccsrc.y"
                                       { yyval = (nodeptr) Str_cre_TSD(parser_data.TSD,retvalue) ; }
#line 5228 "./yaccsrc.c"
    break;

  case 463: /* string: HEXSTRING  */
#line 1378 "./yaccsrc.y"
                                       { streng *sptr = Str_make_TSD(parser_data.TSD,retlength) ;
                                         memcpy(sptr->value,retvalue,
                                                 sptr->len=retlength) ;
                                         yyval = (nodeptr) sptr ; }
#line 5237 "./yaccsrc.c"
    break;

  case 464: /* string: BINSTRING  */
#line 1382 "./yaccsrc.y"
                                       { streng *sptr = Str_make_TSD(parser_data.TSD,retlength) ;
                                         memcpy(sptr->value,retvalue,
                                                 sptr->len=retlength) ;
                                         yyval = (nodeptr) sptr ; }
#line 5246 "./yaccsrc.c"
    break;

  case 465: /* pv: PLACEHOLDER pv  */
#line 1388 "./yaccsrc.y"
                                       { yyval = makenode(X_TPL_POINT,1,yyvsp[0]) ; }
#line 5252 "./yaccsrc.c"
    break;

  case 466: /* pv: symbtree pv  */
#line 1389 "./yaccsrc.y"
                                       { yyval = makenode(X_TPL_SYMBOL,2,yyvsp[0],yyvsp[-1]) ; }
#line 5258 "./yaccsrc.c"
    break;

  case 467: /* pv: %empty  */
#line 1390 "./yaccsrc.y"
                                       { yyval = NULL ; }
#line 5264 "./yaccsrc.c"
    break;

  case 468: /* $@37: %empty  */
#line 1400 "./yaccsrc.y"
                                       { /* detect
                                          * "x = ( a,. b )",
                                          * "x = ( ,. b )",
                                          * "x = a,. b",
                                          * "x = ,. b",
                                          * "x = f( ( x,. b ) )",
                                          * "x = f( ( ,. b ) )"      */
                                         if ( parendepth < 0 )
                                            exiterror( ERR_UNEXPECTED_PARAN, 1 );

                                         /* With call being the extended kind
                                          * of CALL we may have:
                                          * "x = f( (a),. b )",
                                          * "x = f( a,. b )",
                                          * "x = f( ,. b )",
                                          * "CALL s (a),. b",
                                          * "CALL s a,. b",
                                          * "CALL s ,. b",
                                          * "call s (a),. b",
                                          * "call s a,. b",
                                          * "call s ,. b",
                                          * "call s( (a),. b )",
                                          * "call s( a,. b )",
                                          * "call s( ,. b )",
                                          * "call s (a,a),. b",
                                          * "call s (a),. b",
                                          * "call s (),. b"
                                          *
                                          * detect "(a),." and transform it
                                          * to "a,."                         */
                                         yyvsp[-1] = reduce_expr_list( yyvsp[-1],
                                                                REDUCE_EXPR );

                                         /* detect "call s (a,b),. b" and
                                          * "call s (),. b", but every list on
                                          * the left side of "," is wrong, so
                                          * complain about every exprlist.   */
                                         if ( IS_EXPRLIST( yyvsp[-1] ) )
                                            exiterror( ERR_UNEXPECTED_PARAN, 1 );

                                         yyvsp[-1] = reduce_expr_list( yyvsp[-1],
                                                              REDUCE_SUBEXPR );
                                       }
#line 5312 "./yaccsrc.c"
    break;

  case 469: /* exprs: nexpr ',' $@37 exprs  */
#line 1443 "./yaccsrc.y"
                                       { /*
                                          * Fixes bug 961301.
                                          */
                                         nodeptr curr;

                                         assert( IS_EXPRLIST( yyvsp[0] ) );

                                         /* detect ",()." */
                                         if ( IS_EXPRLIST( yyvsp[0]->p[0] )
                                           && ( yyvsp[0]->p[1] == NULL )
                                           && ( yyvsp[0]->p[0]->p[0] == NULL ) )
                                            exiterror( ERR_UNEXPECTED_PARAN, 0 );

                                         /* detect ",(a,b)." */
                                         if ( IS_EXPRLIST( yyvsp[0]->p[0] )
                                           && ( yyvsp[0]->p[1] == NULL )
                                           && IS_EXPRLIST( yyvsp[0]->p[0]->p[1] ) )
                                            exiterror( ERR_UNEXPECTED_PARAN, 1 );

                                         /* detect ",(a)." and transform it
                                          * to ",a."                         */
                                         yyvsp[0] = reduce_expr_list( yyvsp[0],
                                                                REDUCE_RIGHT );
                                         assert( IS_EXPRLIST( yyvsp[0] ) );

                                         /* Detect something like
                                          * "call s (a,b)+1"                 */
                                         curr = yyvsp[0]->p[0];
                                         if ( ( curr != NULL )
                                           && !IS_EXPRLIST( curr )
                                           && !IS_FUNCTION( curr )
                                           && ( IS_EXPRLIST( curr->p[0] )
                                             || IS_EXPRLIST( curr->p[1] ) ) )
                                            exiterror( ERR_INVALID_EXPRESSION, 0 );

                                         yyval = makenode( X_EXPRLIST, 2, yyvsp[-3], yyvsp[0] );
                                         checkconst( yyval ); }
#line 5354 "./yaccsrc.c"
    break;

  case 470: /* exprs: nexpr  */
#line 1480 "./yaccsrc.y"
                                       { /* detect
                                          * "x = ()."
                                          * "x = f(().)"
                                          * "call s (().)"
                                          * "CALL s ()."                     */
                                         if ( ( parendepth < 0 ) && ( yyvsp[0] == NULL ) )
                                            exiterror( ERR_UNEXPECTED_PARAN, 0 );

                                         /* With call being the extended kind
                                          * of CALL we may have:
                                          * "x = ( a. )",
                                          * "x = f( . )",
                                          * "x = f( ., )",
                                          * "x = f( a. )",
                                          * "x = f( a., )",
                                          * "x = f( a, . )",
                                          * "x = f( a, b. )",
                                          * "CALL s .",
                                          * "CALL s .,",
                                          * "CALL s a.,",
                                          * "CALL s a, .",
                                          * "CALL s a, b.",
                                          * "call s .",
                                          * "call s .,",
                                          * "call s a.,",
                                          * "call s a, .",
                                          * "call s a, b.",
                                          * "call s (a.)",
                                          * "call s (a)+1, .",
                                          * "call s (a), .",
                                          * "call s (a), a.",
                                          * "call s (a), (a).",
                                          * "call s ( ., )",
                                          * "call s ( a., )",
                                          * "call s ( a, . )",
                                          * "call s ( a, b. )"               */

                                         yyvsp[0] = reduce_expr_list( yyvsp[0],
                                                              REDUCE_SUBEXPR );
                                         yyval = makenode( X_EXPRLIST, 1, yyvsp[0] );
                                         checkconst( yyval );
                                        }
#line 5401 "./yaccsrc.c"
    break;

  case 471: /* nexpr: expr  */
#line 1524 "./yaccsrc.y"
                                       { yyval = yyvsp[0] ; }
#line 5407 "./yaccsrc.c"
    break;

  case 472: /* nexpr: %empty  */
#line 1525 "./yaccsrc.y"
                                       { yyval = NULL ; }
#line 5413 "./yaccsrc.c"
    break;

  case 473: /* anyvars: xsimsymb anyvars  */
#line 1528 "./yaccsrc.y"
                                        { yyval = makenode(X_SIM_SYMBOL,1,yyvsp[0]) ;
                                         yyval->name = (streng *) yyvsp[-1] ; }
#line 5420 "./yaccsrc.c"
    break;

  case 474: /* anyvars: xsimsymb  */
#line 1530 "./yaccsrc.y"
                                        { yyval = makenode(X_SIM_SYMBOL,0) ;
                                         yyval->name = (streng *) yyvsp[0] ; }
#line 5427 "./yaccsrc.c"
    break;

  case 475: /* anyvars: '(' xsimsymb ')' anyvars  */
#line 1532 "./yaccsrc.y"
                                        { yyval = makenode(X_IND_SYMBOL,1,yyvsp[0]) ;
                                         yyval->name = (streng *) yyvsp[-2] ; }
#line 5434 "./yaccsrc.c"
    break;

  case 476: /* anyvars: '(' xsimsymb ')'  */
#line 1534 "./yaccsrc.y"
                                        { yyval = makenode(X_IND_SYMBOL,0) ;
                                         yyval->name = (streng *) yyvsp[-1] ; }
#line 5441 "./yaccsrc.c"
    break;

  case 477: /* xsimsymb: SIMSYMBOL  */
#line 1538 "./yaccsrc.y"
                                       { yyval = (treenode *) Str_cre_TSD(parser_data.TSD,retvalue);}
#line 5447 "./yaccsrc.c"
    break;

  case 478: /* simsymb: SIMSYMBOL  */
#line 1541 "./yaccsrc.y"
                                       { yyval = (treenode *) Str_cre_TSD(parser_data.TSD,retvalue);}
#line 5453 "./yaccsrc.c"
    break;

  case 479: /* simsymb: error  */
#line 1542 "./yaccsrc.y"
                                       { exiterror( ERR_SYMBOL_EXPECTED, 1, __reginatext ) ;}
#line 5459 "./yaccsrc.c"
    break;


#line 5463 "./yaccsrc.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

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


#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 1545 "./yaccsrc.y"


static nodeptr makenode( int type, int numb, ... )
{
   nodeptr thisleave ;
   va_list argptr ;
   int i ;

   assert(numb <= (int) (sizeof(thisleave->p)/sizeof(thisleave->p[0])));
#ifdef REXXDEBUG
   printf("makenode: making new node, type: %d\n",type) ;
#endif /* REXXDEBUG */

   thisleave = FreshNode( ) ;
   /* thisleave is initialized to zero except for nodeindex */
   va_start( argptr, numb ) ;
   thisleave->type = type ;
   thisleave->lineno = -1 ;
   thisleave->charnr = -1 ;
   for (i=0;i<numb;i++)
      thisleave->p[i]=va_arg(argptr, nodeptr) ;

   va_end( argptr ) ;

   return( thisleave ) ;
}

static char *getdokeyword( int type )
{
   char *ptr;
   switch( type )
   {
      case X_DO_TO: ptr="TO";break;
      case X_DO_BY: ptr="BY";break;
      case X_DO_FOR: ptr="FOR";break;
      default: ptr="";break;
   }
   return ptr;
}

static void checkdosyntax( cnodeptr thisptr )
{
   if ((thisptr->p[1]!=NULL)&&(thisptr->p[2]!=NULL))
   {
      if ((thisptr->p[1]->type)==(thisptr->p[2]->type))
      {
         exiterror( ERR_INVALID_DO_SYNTAX, 1, getdokeyword(thisptr->p[1]->type) )  ;
      }
   }
   if ((thisptr->p[2]!=NULL)&&(thisptr->p[3]!=NULL))
   {
      if ((thisptr->p[2]->type)==(thisptr->p[3]->type))
      {
         exiterror( ERR_INVALID_DO_SYNTAX, 1, getdokeyword(thisptr->p[2]->type) )  ;
      }
   }
   if ((thisptr->p[1]!=NULL)&&(thisptr->p[3]!=NULL))
   {
      if ((thisptr->p[1]->type)==(thisptr->p[3]->type))
      {
         exiterror( ERR_INVALID_DO_SYNTAX, 1, getdokeyword(thisptr->p[1]->type) )  ;
      }
   }
   return ;
}


void newlabel( const tsd_t *TSD, internal_parser_type *ipt, nodeptr thisptr )
{
   labelboxptr newptr ;

   assert( thisptr ) ;

   newptr = (labelboxptr)MallocTSD(sizeof(labelbox)) ;

   newptr->next = NULL ;
   newptr->entry = thisptr ;
   if (ipt->first_label == NULL)
   {
      ipt->first_label = newptr ;
      ipt->last_label = newptr ; /* must be NULL, too */
   }
   else
   {
      ipt->last_label->next = newptr ;
      ipt->last_label = newptr ;
   }
   ipt->numlabels++;
}

static nodeptr create_tail( const char *name )
{
   const char *cptr ;
   nodeptr node ;
   int constant ;
   streng *tname ;
   tsd_t *TSD = parser_data.TSD;

   if (!*name)
   {
      node = makenode( X_CTAIL_SYMBOL, 0 ) ;
      node->name = Str_make_TSD( parser_data.TSD, 0) ;
      return node ;
   }

   cptr = name ;
   constant = rx_isdigit(*cptr) || *cptr=='.' || (!*cptr) ;
   node = makenode( (constant) ? X_CTAIL_SYMBOL : X_VTAIL_SYMBOL, 0 ) ;

   for (;*cptr && *cptr!='.'; cptr++) ;
   node->name = Str_ncre_TSD( parser_data.TSD, name, cptr-name ) ;

   if (*cptr)
   {
      node->p[0] = create_tail( ++cptr ) ;
      if (constant && node->p[0]->type==X_CTAIL_SYMBOL)
      {
         streng *first, *second ;
         nodeptr tptr ;

         first = node->name ;
         second = node->p[0]->name ;
         node->name = NULL;
         node->p[0]->name = NULL;
         tname = Str_makeTSD( first->len + second->len + 1) ;
         memcpy( tname->value, first->value, first->len ) ;
         tname->value[first->len] = '.' ;
         memcpy( tname->value+first->len+1, second->value, second->len) ;
         tname->len = first->len + second->len + 1 ;

         Free_stringTSD( first ) ;
         Free_stringTSD( second ) ;
         node->name = tname ;
         tptr = node->p[0] ;
         node->p[0] = tptr->p[0] ;
         RejectNode(tptr);
      }
   }

   return node ;
}

static nodeptr create_head( const char *name )
{
   const char *cptr ;
   nodeptr node ;

   /* Bypass reserved variables */
   cptr = ( *name ) ? ( name + 1 ) : name;
   for (; *cptr && *cptr!='.'; cptr++) ;
   node = makenode( X_SIM_SYMBOL, 0 ) ;
   node->name = Str_ncre_TSD( parser_data.TSD, name, cptr-name+(*cptr=='.')) ;

   if (*cptr)
   {
      if (*(++cptr))
         node->p[0] = create_tail( cptr ) ;
      else
         node->p[0] = NULL ;

      node->type = (node->p[0]) ? X_HEAD_SYMBOL : X_STEM_SYMBOL ;
   }

   return node ;
}


static node_type gettypeof( nodeptr thisptr )
{
   tsd_t *TSD = parser_data.TSD;

   switch(thisptr->type)
   {
      case X_PLUSS:
      case X_MINUS:
      case X_MULT:
      case X_U_PLUSS:
      case X_U_MINUS:
      case X_DEVIDE:
      case X_INTDIV:
      case X_MODULUS:
      case X_EQUAL:
      case X_DIFF:
      case X_GTE:
      case X_GT:
      case X_LTE:
      case X_LT:
      case X_SEQUAL:
      case X_SDIFF:
      case X_SGTE:
      case X_SGT:
      case X_SLTE:
      case X_SLT:
      case X_NEQUAL:
      case X_NDIFF:
      case X_NGTE:
      case X_NGT:
      case X_NLTE:
      case X_NLT:
         return IS_A_NUMBER ;


      case X_SIM_SYMBOL:
         return IS_SIM_SYMBOL ;

      case X_HEAD_SYMBOL:
         return IS_COMP_SYMBOL ;

      case X_STRING:
      case X_CON_SYMBOL:
      {
         if (thisptr->u.number)
         {
            fprintf( stderr, "Found an internal spot of investigation of the Regina interpreter.\n"
                             "Please inform Mark Hessling or Florian Grosse-Coosmann about the\n"
                             "circumstances and this script.\n"
                             "\n"
                             "Many thanks!\n"
                             "email addresses:\n"
                             "mark@rexx.org\n"
                             "florian@grosse-coosmann.de\n");
            /* FIXME: When does this happen?
             * It doesn't happen if no feedback is send until end of 2005.
             */
            return IS_A_NUMBER ;
         }

         if ( ( thisptr->u.number = is_a_descr( TSD, thisptr->name ) ) != NULL )
            return IS_A_NUMBER;
         return IS_NO_NUMBER;
      }
   }
   return IS_UNKNOWN ;
}



static void transform( nodeptr thisptr )
{
   int type ;
   node_type left,rght;

   left = gettypeof( thisptr->p[0] ) ;
   rght = gettypeof( thisptr->p[1] ) ;
   type = thisptr->type ;

   if ( ( left == IS_A_NUMBER ) && ( rght == IS_A_NUMBER ) )
   {
      if (type==X_EQUAL)
         thisptr->type = X_NEQUAL ;
      else if (type==X_DIFF)
         thisptr->type = X_NDIFF ;
      else if (type==X_GTE)
         thisptr->type = X_NGTE ;
      else if (type==X_GT)
         thisptr->type = X_NGT ;
      else if (type==X_LTE)
         thisptr->type = X_NLTE ;
      else if (type==X_LT)
         thisptr->type = X_NLT ;
   }
   else if ( ( left == IS_NO_NUMBER ) || ( rght == IS_NO_NUMBER ) )
   {
      if (type==X_EQUAL)
         thisptr->type = X_SEQUAL ;
      else if (type==X_DIFF)
         thisptr->type = X_SDIFF ;
      else if (type==X_GTE)
         thisptr->type = X_SGTE ;
      else if (type==X_GT)
         thisptr->type = X_SGT ;
      else if (type==X_LTE)
         thisptr->type = X_SLTE ;
      else if (type==X_LT)
         thisptr->type = X_SLT ;
   }
   else
   {
      type = thisptr->p[0]->type ;
      if ( ( left == IS_A_NUMBER )
        && ( ( type == X_STRING ) || ( type == X_CON_SYMBOL ) ) )
         thisptr->u.flags.lnum = 1 ;
      else if ( left == IS_SIM_SYMBOL )
         thisptr->u.flags.lsvar = 1 ;
      else if ( left == IS_COMP_SYMBOL )
         thisptr->u.flags.lcvar = 1 ;

      type = thisptr->p[1]->type ;
      if ( ( rght == IS_A_NUMBER )
        && ( ( type == X_STRING ) || ( type == X_CON_SYMBOL ) ) )
         thisptr->u.flags.rnum = 1 ;
      else if ( rght == IS_SIM_SYMBOL )
         thisptr->u.flags.rsvar = 1 ;
      else if ( rght == IS_COMP_SYMBOL )
         thisptr->u.flags.rcvar = 1 ;
   }
}


static int is_const( cnodeptr thisptr )
{
   if (!thisptr)
      return 1 ;

   switch (thisptr->type)
   {
      case X_STRING:
      case X_CON_SYMBOL:
         return 1 ;

#if 0
      Pre-evaluation is not allowed. DIGITS and FUZZ may change within loops
      and the resulting value may or may not be the same. Concatenation with
      or without spaces is the sole allowed operation.

      NEVER ENABLE THIS AGAIN WITHOUT SOLVING THIS PROBLEMS!

      case X_U_PLUSS:
      case X_U_MINUS:
         return is_const( thisptr->p[0] ) ;

      case X_PLUSS:
      case X_MINUS:
      case X_MULT:
/*    case X_DEVIDE: Bug 20000807-41821 */
      case X_INTDIV:
      case X_MODULUS:
      case X_EQUAL:
      case X_DIFF:
      case X_GTE:
      case X_GT:
      case X_LTE:
      case X_LT:
      case X_SEQUAL:
      case X_SDIFF:
      case X_SGTE:
      case X_SGT:
      case X_SLTE:
      case X_SLT:
      case X_NEQUAL:
      case X_NDIFF:
      case X_NGTE:
      case X_NGT:
      case X_NLTE:
      case X_NLT:
#endif

      case X_SPACE:
      case X_CONCAT:
         return is_const( thisptr->p[0] ) && is_const( thisptr->p[1] ) ;
   }
   return 0 ;
}


static void checkconst( nodeptr thisptr )
{
   tsd_t *TSD = parser_data.TSD;

   assert( thisptr->type == X_EXPRLIST ) ;
   if (is_const(thisptr->p[0]))
   {
      if (thisptr->p[0])
         thisptr->u.strng = evaluate( TSD, thisptr->p[0], NULL ) ;
      else
         thisptr->u.strng = NULL ;

      thisptr->type = X_CEXPRLIST ;
   }
}

/*
 * reduce_expr_list will be invoked if the reduction of a list expression for
 * "call" arguments or a plain "(expr)" is needed. The reduction of the
 * outer parentheses of the extended CALL syntax is done with
 * mode==REDUCE_CALL, the reduction of a simple "(expr)" is done with
 * mode==REDUCE_EXPR. REDUCE_RIGHT is a synonym for REDUCE_CALL currently and
 * is intended to be used for reducing the right side of an expression list.
 *
 * REDUCE_SUBEXPR detect "call s (a)+1," and "call s 1+(a)," and reduces it.
 * Furthermore it detects "call s ()+1", "call s 1+()", "call s 1+(a,b)",
 * "call s (a,b)+1" and raises an error in this case.
 */
static nodeptr reduce_expr_list( nodeptr thisptr, reduce_mode mode )
{
   nodeptr h, retval = thisptr;

   if ( !thisptr )
      return retval;

   if ( mode == REDUCE_SUBEXPR )
   {
      if ( ( parendepth == 1 ) && !IS_FUNCTION( thisptr ) && !IS_EXPRLIST( thisptr ) )
      {
         if ( IS_EXPRLIST( thisptr->p[0] ) )
         {
            h = thisptr->p[0];
            if ( ( h->p[0] == NULL ) || ( h->p[1] != NULL ) )
               exiterror( ERR_INVALID_EXPRESSION, 0 );
            thisptr->p[0] = h->p[0];
            RejectNode( h );
         }
         if ( IS_EXPRLIST( thisptr->p[1] ) )
         {
            h = thisptr->p[1];
            if ( ( h->p[0] == NULL ) || ( h->p[1] != NULL ) )
               exiterror( ERR_INVALID_EXPRESSION, 0 );
            thisptr->p[1] = h->p[0];
            RejectNode( h );
         }
      }
      return retval;
   }

   if ( !IS_EXPRLIST( thisptr ) )
      return retval;

   if ( ( mode == REDUCE_CALL ) || ( mode == REDUCE_RIGHT ) )
   {
      if ( IS_EXPRLIST( thisptr->p[0] ) && ( thisptr->p[1] == NULL ) )
      {
         retval = thisptr->p[0];
         RejectNode( thisptr );
      }
   }
   else
   {
      /*
       * mode == REDUCE_EXPR
       */
      if ( ( thisptr->p[0] != NULL ) && ( thisptr->p[1] == NULL ) )
      {
         if ( !IS_EXPRLIST( thisptr->p[0] ) )
         {
            retval = thisptr->p[0];
            RejectNode( thisptr );
         }
      }
   }
   return retval;
}

/*
 * optgluelast connect p2 as the ->next element to p1. Every element may be
 * NULL.
 * If both are non-NULL, the connection is performed using the o.last elements.
 * Just the o.last element of p1 remains non-NULL.
 *
 * Returns: NULL if p1 and p2 are NULL.
 *          The non-NULL element if one argumet is NULL.
 *          p1 otherwise.
 */
static nodeptr optgluelast( nodeptr p1, nodeptr p2 )
{
   nodeptr p2last;

   if ( p1 == NULL )
      return p2;
   if ( p2 == NULL )
      return p1;

   /*
    * This is performed very often, so keep the code fast.
    *
    * p2last is the "o.last"-element of p2 or just p2 if p2 has no next
    * elements. We set p1's o.last further down, but we have to ensure that
    * p2->o.last is NULL first. Therefore every element in the ->next chain
    * of p1 will have NULL as its o.last field.
    */
   if ( ( p2last = p2->o.last ) == NULL )
      p2last = p2;
   else
      p2->o.last = NULL;

   if ( p1->o.last == NULL )
      p1->next = p2;
   else
      p1->o.last->next = p2;
   p1->o.last = p2last;

   return p1;
}

/*
 * justlabels returns 1, if n consists of a sequence of labels. The return
 * value is 0 otherwise.
 */
static int justlabels( nodeptr n )
{
   while ( n != NULL )
   {
      if ( n->type != X_LABEL )
         return 0;
      n = n->next;
   }

   return 1;
}

/*
 * move_labels move the labels from the end of "end" to the end of "front".
 * The labels are marked "read_only" if level is nonnull, the read-only flag
 * is removed if level is 0.
 * NOTE: At least one element of the "end" chain must contain a non-label
 * element.
 */
static void move_labels( nodeptr front, nodeptr end, int level )
{
   nodeptr oend = end;
   nodeptr labels;

   assert( front != NULL );
   assert( !justlabels( end ) );

   while ( !justlabels( end->next ) )
      end = end->next;

   if ( ( labels = end->next ) == NULL )
      return;

   /*
    * extract the labels.
    */
   labels->o.last = oend->o.last;
   end->next = NULL;
   if ( end == oend )
      oend->o.last = NULL;
   else
      oend->o.last = end;

   if ( labels->next == NULL )
      labels->o.last = NULL;

   /*
    * add the labels to the end of front and then re-mark the labels.
    */
   optgluelast( front, labels );
   while ( labels ) {
      labels->u.trace_only = ( level == 0 ) ? 0 : 1;
      labels = labels->next;
   }
}
