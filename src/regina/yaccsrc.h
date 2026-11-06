/* A Bison parser, made by GNU Bison 3.7.5.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY__REGINA_YACCSRC_H_INCLUDED
# define YY__REGINA_YACCSRC_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int __reginadebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    ADDRESS = 258,                 /* ADDRESS  */
    ARG = 259,                     /* ARG  */
    CALL = 260,                    /* CALL  */
    DO = 261,                      /* DO  */
    TO = 262,                      /* TO  */
    BY = 263,                      /* BY  */
    FOR = 264,                     /* FOR  */
    WHILE = 265,                   /* WHILE  */
    UNTIL = 266,                   /* UNTIL  */
    EXIT = 267,                    /* EXIT  */
    IF = 268,                      /* IF  */
    THEN = 269,                    /* THEN  */
    ELSE = 270,                    /* ELSE  */
    ITERATE = 271,                 /* ITERATE  */
    INTERPRET = 272,               /* INTERPRET  */
    LEAVE = 273,                   /* LEAVE  */
    NOP = 274,                     /* NOP  */
    NUMERIC = 275,                 /* NUMERIC  */
    PARSE = 276,                   /* PARSE  */
    EXTERNAL = 277,                /* EXTERNAL  */
    SOURCE = 278,                  /* SOURCE  */
    VAR = 279,                     /* VAR  */
    VALUE = 280,                   /* VALUE  */
    WITH = 281,                    /* WITH  */
    PROCEDURE = 282,               /* PROCEDURE  */
    EXPOSE = 283,                  /* EXPOSE  */
    PULL = 284,                    /* PULL  */
    PUSH = 285,                    /* PUSH  */
    QUEUE = 286,                   /* QUEUE  */
    SAY = 287,                     /* SAY  */
    RETURN = 288,                  /* RETURN  */
    SELECT = 289,                  /* SELECT  */
    WHEN = 290,                    /* WHEN  */
    DROP = 291,                    /* DROP  */
    OTHERWISE = 292,               /* OTHERWISE  */
    SIGNAL = 293,                  /* SIGNAL  */
    ON = 294,                      /* ON  */
    OFF = 295,                     /* OFF  */
    SYNTAX = 296,                  /* SYNTAX  */
    HALT = 297,                    /* HALT  */
    NOVALUE = 298,                 /* NOVALUE  */
    TRACE = 299,                   /* TRACE  */
    END = 300,                     /* END  */
    UPPER = 301,                   /* UPPER  */
    ASSIGNMENTVARIABLE = 302,      /* ASSIGNMENTVARIABLE  */
    STATSEP = 303,                 /* STATSEP  */
    FOREVER = 304,                 /* FOREVER  */
    DIGITS = 305,                  /* DIGITS  */
    FORM = 306,                    /* FORM  */
    FUZZ = 307,                    /* FUZZ  */
    SCIENTIFIC = 308,              /* SCIENTIFIC  */
    ENGINEERING = 309,             /* ENGINEERING  */
    NOT = 310,                     /* NOT  */
    CONCATENATE = 311,             /* CONCATENATE  */
    MODULUS = 312,                 /* MODULUS  */
    GTE = 313,                     /* GTE  */
    GT = 314,                      /* GT  */
    LTE = 315,                     /* LTE  */
    LT = 316,                      /* LT  */
    DIFFERENT = 317,               /* DIFFERENT  */
    EQUALEQUAL = 318,              /* EQUALEQUAL  */
    NOTEQUALEQUAL = 319,           /* NOTEQUALEQUAL  */
    OFFSET = 320,                  /* OFFSET  */
    SPACE = 321,                   /* SPACE  */
    EXP = 322,                     /* EXP  */
    XOR = 323,                     /* XOR  */
    PLACEHOLDER = 324,             /* PLACEHOLDER  */
    NOTREADY = 325,                /* NOTREADY  */
    CONSYMBOL = 326,               /* CONSYMBOL  */
    SIMSYMBOL = 327,               /* SIMSYMBOL  */
    EXFUNCNAME = 328,              /* EXFUNCNAME  */
    INFUNCNAME = 329,              /* INFUNCNAME  */
    LABEL = 330,                   /* LABEL  */
    DOVARIABLE = 331,              /* DOVARIABLE  */
    HEXSTRING = 332,               /* HEXSTRING  */
    STRING = 333,                  /* STRING  */
    VERSION = 334,                 /* VERSION  */
    LINEIN = 335,                  /* LINEIN  */
    WHATEVER = 336,                /* WHATEVER  */
    NAME = 337,                    /* NAME  */
    FAILURE = 338,                 /* FAILURE  */
    BINSTRING = 339,               /* BINSTRING  */
    OPTIONS = 340,                 /* OPTIONS  */
    ENVIRONMENT = 341,             /* ENVIRONMENT  */
    LOSTDIGITS = 342,              /* LOSTDIGITS  */
    GTGT = 343,                    /* GTGT  */
    LTLT = 344,                    /* LTLT  */
    NOTGTGT = 345,                 /* NOTGTGT  */
    NOTLTLT = 346,                 /* NOTLTLT  */
    GTGTE = 347,                   /* GTGTE  */
    LTLTE = 348,                   /* LTLTE  */
    INPUT = 349,                   /* INPUT  */
    OUTPUT = 350,                  /* OUTPUT  */
    ERROR = 351,                   /* ERROR  */
    NORMAL = 352,                  /* NORMAL  */
    APPEND = 353,                  /* APPEND  */
    REPLACE = 354,                 /* REPLACE  */
    STREAM = 355,                  /* STREAM  */
    STEM = 356,                    /* STEM  */
    LIFO = 357,                    /* LIFO  */
    FIFO = 358,                    /* FIFO  */
    NOEOL = 359,                   /* NOEOL  */
    LOWER = 360,                   /* LOWER  */
    CASELESS = 361,                /* CASELESS  */
    PLUSASSIGNMENTVARIABLE = 362,  /* PLUSASSIGNMENTVARIABLE  */
    MINUSASSIGNMENTVARIABLE = 363, /* MINUSASSIGNMENTVARIABLE  */
    MULTASSIGNMENTVARIABLE = 364,  /* MULTASSIGNMENTVARIABLE  */
    DIVASSIGNMENTVARIABLE = 365,   /* DIVASSIGNMENTVARIABLE  */
    MODULUSASSIGNMENTVARIABLE = 366, /* MODULUSASSIGNMENTVARIABLE  */
    INTDIVASSIGNMENTVARIABLE = 367, /* INTDIVASSIGNMENTVARIABLE  */
    ORASSIGNMENTVARIABLE = 368,    /* ORASSIGNMENTVARIABLE  */
    XORASSIGNMENTVARIABLE = 369,   /* XORASSIGNMENTVARIABLE  */
    ANDASSIGNMENTVARIABLE = 370,   /* ANDASSIGNMENTVARIABLE  */
    CONCATASSIGNMENTVARIABLE = 371, /* CONCATASSIGNMENTVARIABLE  */
    CCAT = 372,                    /* CCAT  */
    UMINUS = 373,                  /* UMINUS  */
    UPLUS = 374                    /* UPLUS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE __reginalval;

int __reginaparse (void);

#endif /* !YY__REGINA_YACCSRC_H_INCLUDED  */
