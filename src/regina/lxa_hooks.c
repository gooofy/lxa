/*
 * lxa_hooks.c - the ARexx personality of Regina in lxa (Phase 250).
 *
 * Called from the LXA_AMIGA patches in Regina's sources (see README.lxa):
 * ARexx error numbers and messages, the ARexx set of built-in functions,
 * SHOW('P'|'L'|'C'), PARSE SOURCE, OPTIONS RESULTS/FAILAT/PROMPT, the
 * ARexx search for external programs and the RC/condition/trace handling
 * of host commands.  The AmigaOS side is sys/System/ARexx.c.
 *
 * This file is part of lxa, not of Regina.
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include "rexx.h"
#include "rxiface.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lxa_arexx.h"
#include "../../sys/Rexxc/rexx_errtext.h"

int lxa_arexx_errcode = 0;
static int forced_code = -2;

void lxa_arexx_raise( int code )
{
   forced_code = code;
   exiterror( ERR_SYSTEM_FAILURE, 0 );
}

/* ---- error numbers ---------------------------------------------------- */

static int is_operator( char c )
{
   return strchr( "+-*/%|&=<>\\^~(,", c ) != NULL;
}

/* Regina reports both "x = (" and "x = (1" as error 36, "x = 1 +" and
 * "x = 1)" as 64.1; ARexx distinguishes an incomplete expression (41)
 * from unbalanced parentheses (42) - decide from the source line */
static int expression_error( int lineno )
{
   int len, i, depth = 0;
   char q = 0, last = 0;
   const char *s = lxa_ax_line( lineno, &len );

   if ( !s )
      return 41;
   for ( i = 0; i < len; i++ )
   {
      char c = s[i];
      if ( q )
      {
         if ( c == q )
            q = 0;
      }
      else if ( c == '\'' || c == '"' )
         q = c;
      else if ( c == '(' )
         depth++;
      else if ( c == ')' )
         depth--;
      if ( c != ' ' && c != '\t' && c != ';' )
         last = c;
   }
   if ( last && is_operator( last ) )
      return 41;
   return depth != 0 ? 42 : 41;
}

static int first_word_is( int lineno, const char *word )
{
   int len, i = 0, n = strlen( word );
   const char *s = lxa_ax_line( lineno, &len );

   if ( !s )
      return 0;
   while ( i < len && ( s[i] == ' ' || s[i] == '\t' ) )
      i++;
   if ( len - i < n )
      return 0;
   for ( ; *word; word++, i++ )
      if ( ( s[i] | 0x20 ) != *word )
         return 0;
   return i >= len || s[i] == ' ' || s[i] == ';';
}

int lxa_arexx_error_code( int errorno, int sub, int lineno )
{
   if ( forced_code >= -1 )
   {
      int code = forced_code;
      forced_code = -2;
      return code;
   }
   switch ( errorno )
   {
      case ERR_PROG_UNREADABLE:           return 1;   /* 3 */
      case ERR_PROG_INTERRUPT:            return 2;   /* 4 */
      case ERR_STORAGE_EXHAUSTED:         return 3;   /* 5 */
      case ERR_UNMATCHED_QUOTE:           return sub == 1 ? 6 : 5;
      case ERR_WHEN_EXPECTED:             return 23;  /* 7 */
      case ERR_THEN_UNEXPECTED:           return sub == 1 ? 20 : 21;
      case ERR_WHEN_UNEXPECTED:           return sub == 1 ? 20 : 21;
      case ERR_UNMATCHED_END:             return 26;  /* 10 */
      case ERR_FULL_CTRL_STACK:           return 43;  /* 11 */
      case ERR_TOO_LONG_LINE:             return 9;   /* 12 */
      case ERR_INVALID_CHAR:              return 8;   /* 13 */
      case ERR_INCOMPLETE_STRUCT:         return sub <= 2 ? 26 : 29;
      case ERR_INVALID_HEX_CONST:         return 8;   /* 15 */
      case ERR_UNEXISTENT_LABEL:          return 30;  /* 16 */
      case ERR_UNEXPECTED_PROC:           return 19;  /* 17 */
      case ERR_THEN_EXPECTED:             return 24;  /* 18 */
      case ERR_STRING_EXPECTED:           return 32;  /* 19 */
      case ERR_SYMBOL_EXPECTED:           return first_word_is( lineno, "parse" ) ? 37 : 31;
      case ERR_EXTRA_DATA:                return 35;  /* 21 */
      case 22:                            return 8;   /* invalid character string */
      case ERR_INVALID_TRACE:             return 33;  /* 24 */
      case ERR_INV_SUBKEYWORD:            return 33;  /* 25 */
      case ERR_INVALID_INTEGER:           return 44;  /* 26 */
      case ERR_INVALID_DO_SYNTAX:         return 28;  /* 27 */
      case ERR_INVALID_LEAVE:             return 22;  /* 28 */
      case ERR_ENVIRON_TOO_LONG:          return 9;   /* 29 */
      case ERR_TOO_LONG_STRING:           return 9;   /* 30 */
      case ERR_INVALID_START:             return 40;  /* 31 */
      case ERR_INVALID_RESULT:            return 44;  /* 33 */
      case ERR_UNLOGICAL_VALUE:           return 46;  /* 34 */
      case ERR_INVALID_EXPRESSION:        return 41;  /* 35 */
      case ERR_UNMATCHED_PARAN:           return expression_error( lineno );
      case ERR_UNEXPECTED_PARAN:          return 42;  /* 37 */
      case ERR_INVALID_TEMPLATE:          return 37;  /* 38 */
      case ERR_INCORRECT_CALL:
         if ( sub == 1 )
            return 12;
         return ( sub >= 3 && sub <= 5 ) ? 17 : 18;
      case ERR_BAD_ARITHMETIC:            return 47;  /* 41 */
      case ERR_ARITH_OVERFLOW:            return sub == 3 ? 48 : -1;
      case ERR_ROUTINE_NOT_FOUND:         return 15;  /* 43 */
      case ERR_NO_DATA_RETURNED:          return 16;  /* 44 */
      case ERR_DATA_NOT_SPEC:             return 16;  /* 45 */
      case ERR_INVALID_STEM:              return 40;  /* 32 */
      case ERR_STACK_OVERFLOW:            return 43;  /* 39 */
      case ERR_RESERVED_SYMBOL:           return 31;  /* 50 */
      case ERR_UNQUOTED_FUNC_STOP:        return 15;  /* 51 */
      case ERR_INVALID_OPTION:            return 18;  /* 53 */
      case ERR_INVALID_STEM_OPTION:       return 18;  /* 54 */
      case ERR_YACC_SYNTAX:               return expression_error( lineno );
      default:
         break;
   }
   return 0;
}

void lxa_arexx_report( int code, int lineno )
{
   lxa_arexx_errcode = code;
   if ( code < 0 )
      return;
   fflush( stdout );
   fprintf( stdout, "+++ Error %d in line %d: %s\n", code, lineno,
            lxa_rexx_error_text( code ) );
   fflush( stdout );
}

/* ---- built-in functions ------------------------------------------------ */

static const char *const arexx_bifs[] =
{
   "ABBREV", "ABS", "ADDRESS", "ARG", "B2C", "BITAND", "BITCHG", "BITCLR",
   "BITCOMP", "BITOR", "BITSET", "BITTST", "BITXOR", "C2B", "C2D", "C2X",
   "CENTER", "CENTRE", "CLOSE", "COMPARE", "COMPRESS", "COPIES", "D2C",
   "D2X", "DATATYPE", "DATE", "DELSTR", "DELWORD", "DIGITS", "EOF",
   "ERRORTEXT", "EXISTS", "EXPORT", "FIND", "FORM", "FREESPACE", "FUZZ",
   "GETSPACE", "HASH", "IMPORT", "INDEX", "INSERT", "LASTPOS", "LEFT",
   "LENGTH", "LINES", "MAX", "MIN", "OPEN", "OVERLAY", "POS", "RANDOM",
   "RANDU", "READCH", "READLN", "REVERSE", "RIGHT", "SEEK", "SHOW", "SIGN",
   "SOURCELINE", "SPACE", "STORAGE", "STRIP", "SUBSTR", "SUBWORD", "SYMBOL",
   "TIME", "TRACE", "TRANSLATE", "TRIM", "TRUNC", "UPPER", "VALUE",
   "VERIFY", "WORD", "WORDINDEX", "WORDLENGTH", "WORDS", "WRITECH",
   "WRITELN", "X2C", "X2D", "XRANGE", NULL
};

int lxa_arexx_bif( const char *name, int len )
{
   int i;

   for ( i = 0; arexx_bifs[i]; i++ )
      if ( (int) strlen( arexx_bifs[i] ) == len && memcmp( arexx_bifs[i], name, len ) == 0 )
         return 1;
   return 0;
}

/* ---- SHOW, PARSE SOURCE, OPTIONS --------------------------------------- */

streng *lxa_arexx_show( tsd_t *TSD, char opt, const streng *name, const streng *sep )
{
   char *list;
   streng *retval;

   if ( name )
      return int_to_streng( TSD, lxa_ax_show( opt, name->value, name->len ) );
   list = lxa_ax_show_list( opt, sep->value, sep->len );
   if ( !list )
      return nullstringptr();
   retval = Str_cre_TSD( TSD, list );
   free( list );
   return retval;
}

streng *lxa_arexx_source( tsd_t *TSD, const char *invoked )
{
   streng *file = TSD->systeminfo->input_file;
   const char *tail = lxa_ax_source( file ? file->value : NULL, file ? file->len : 0 );
   streng *retval = Str_makeTSD( strlen( invoked ) + strlen( tail ) + 2 );

   retval->len = 0;
   Str_catstrTSD( retval, invoked );
   Str_catstrTSD( retval, " " );
   Str_catstrTSD( retval, tail );
   return retval;
}

static int word_eq( const char *s, int len, const char *w )
{
   int i;

   if ( (int) strlen( w ) != len )
      return 0;
   for ( i = 0; i < len; i++ )
      if ( rx_toupper( s[i] ) != w[i] )
         return 0;
   return 1;
}

void lxa_arexx_options( tsd_t *TSD, streng *opts )
{
   const char *s = opts->value;
   int len = opts->len, i = 0, w0, w1, no = 0;

   while ( i < len && rx_isspace( s[i] ) )
      i++;
   if ( i >= len )
   {
      lxa_ax_reset_options();
      Free_stringTSD( opts );
      return;
   }
   w0 = i;
   while ( i < len && !rx_isspace( s[i] ) )
      i++;
   w1 = i;
   if ( word_eq( s + w0, w1 - w0, "NO" ) )
   {
      no = 1;
      while ( i < len && rx_isspace( s[i] ) )
         i++;
      w0 = i;
      while ( i < len && !rx_isspace( s[i] ) )
         i++;
      w1 = i;
   }
   while ( i < len && rx_isspace( s[i] ) )
      i++;

   if ( word_eq( s + w0, w1 - w0, "RESULTS" ) )
      lxa_ax_set_results( !no );
   else if ( word_eq( s + w0, w1 - w0, "FAILAT" ) )
      lxa_ax_set_failat( atol( s + i ) );
   else if ( word_eq( s + w0, w1 - w0, "PROMPT" ) )
      lxa_ax_set_prompt( s + i, len - i );
   else if ( word_eq( s + w0, w1 - w0, "CACHE" ) )
      ;  /* no program cache to switch */
   else
   {
      do_options( TSD, TSD->currlevel, opts, 0 );
      return;
   }
   Free_stringTSD( opts );
}

/* ---- external programs -------------------------------------------------- */

streng *lxa_arexx_find_routine( const tsd_t *TSD, const char *inname, FILE **fp )
{
   char buf[512];

   *fp = NULL;
   if ( !lxa_ax_locate( inname, strlen( inname ), buf, sizeof( buf ) ) )
      return NULL;
   *fp = fopen( buf, "rb" );
   if ( *fp == NULL )
      return NULL;
   return Str_crestrTSD( buf );
}

/* ---- host commands ------------------------------------------------------ */

/* After a host command: RC is the host's return code.  A code at or
 * above OPTIONS FAILAT is a FAILURE (an ERROR when only ERROR is
 * trapped), any other non-zero code an ERROR.  An untrapped failure is
 * traced at TRACE NORMAL, any error at TRACE ERROR and above:
 *   "  1 *-* 'FAIL';"  "+++ Command returned 10" */
void lxa_arexx_command_done( tsd_t *TSD, const streng *cmd, int rc_code,
                             const streng *rc_value, cnodeptr thisptr )
{
   trap *traps;
   int type, trace;
   char t = TSD->trace_stat;
   long rc;

   set_reserved_value( TSD, POOL0_RC, rc_value ? Str_dupTSD( rc_value ) : NULL,
                       0, rc_value ? VFLAG_STR : VFLAG_NUM );
   if ( !rc_code )
      return;
   rc = rc_value ? atol( rc_value->value ) : 0;
   traps = gettraps( TSD, TSD->currlevel );
   type = ( rc_code == RXFLAG_FAILURE ) ? SIGNAL_FAILURE : SIGNAL_ERROR;
   if ( type == SIGNAL_FAILURE && !traps[SIGNAL_FAILURE].on_off )
      type = SIGNAL_ERROR;
   if ( traps[type].on_off )
   {
      condition_hook( TSD, type, (int) rc, 0, thisptr->lineno, Str_dupTSD( cmd ), NULL );
      return;
   }

   trace = ( t == 'E' || t == 'A' || t == 'C' || t == 'R' || t == 'I' || t == 'L' )
        || ( rc_code == RXFLAG_FAILURE && ( t == 'N' || t == 'F' ) );
   if ( trace && thisptr->charnr >= 0 && thisptr->lineno >= 0 )
   {
      streng *src = getsourceline( TSD, thisptr->lineno, thisptr->charnr,
                                   &TSD->systeminfo->tree );
      int len;
      const char *line = lxa_ax_line( thisptr->lineno, &len );
      int semi = 0;

      /* ARexx shows the clause with its terminating semicolon */
      if ( line && src->len > 0 )
      {
         const char *p = line + thisptr->charnr - 1 + src->len;
         if ( p >= line && p < line + len && *p == ';' )
            semi = 1;
      }
      fflush( stdout );
      fprintf( stdout, "%3d *-* %.*s%s\n", thisptr->lineno, (int) src->len, src->value,
               semi ? ";" : "" );
      fprintf( stdout, "+++ Command returned %ld\n", rc );
      fflush( stdout );
      Free_stringTSD( src );
   }
}

/* once per clause: HI or Ctrl-C raise HALT; TS starts interactive tracing
 * (TRACE ?R), TE ends it (as on ARexx: the global tracing flag) */
void lxa_arexx_poll( tsd_t *TSD )
{
   int ev = lxa_ax_poll();

   if ( ev & LXA_AX_HALT )
      TSD->HaltRaised = 2; /* signals_names[2]: SIGINT */
   if ( ( ev & LXA_AX_TRACE_ON ) && !TSD->systeminfo->interactive )
   {
      set_trace_char( TSD, 'R' );
      set_trace_char( TSD, '?' );
   }
   if ( ( ev & LXA_AX_TRACE_OFF ) && TSD->systeminfo->interactive )
      set_trace_char( TSD, 'O' );
}
