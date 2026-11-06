/*
 * lxa_arexx.h - the ARexx personality of Regina in lxa (Phase 250).
 *
 * lxa_hooks.c (Regina side, called from the LXA_AMIGA patches in Regina's
 * sources) and sys/System/ARexx.c (AmigaOS side: packets, ports, lists)
 * talk through these functions.  Not part of Regina.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#ifndef LXA_AREXX_H
#define LXA_AREXX_H

/* ---- ARexx.c (AmigaOS side) ------------------------------------------ */

/* the source line <lineno> of the running program (NULL: unknown) */
const char *lxa_ax_line(int lineno, int *len);
/* OPTIONS: RESULTS, FAILAT, PROMPT */
void lxa_ax_set_results(int on);
void lxa_ax_set_failat(long failat);
void lxa_ax_set_prompt(const char *s, int len);
void lxa_ax_reset_options(void);
/* SHOW('P'|'L'|'C', name): 1/0; without a name: a malloc()ed list */
int lxa_ax_show(char opt, const char *name, int len);
char *lxa_ax_show_list(char opt, const char *sep, int seplen);
/* "1 called resolved ext host" for PARSE SOURCE (file: the program file) */
const char *lxa_ax_source(const char *file, int len);
/* poll Ctrl-C/HI and TS/TE: LXA_AX_HALT, LXA_AX_TRACE_ON, LXA_AX_TRACE_OFF */
#define LXA_AX_HALT      1
#define LXA_AX_TRACE_ON  2
#define LXA_AX_TRACE_OFF 4
int lxa_ax_poll(void);
/* allocate the scanner's large buffers (lexsrc.c); 0 when out of memory */
int lxa_lex_buffers(void);
/* find an external program (cwd, then REXX:); 1 when <buf> names a
 * program file that starts with a comment */
int lxa_ax_locate(const char *name, int len, char *buf, int bufsize);

/* ---- lxa_hooks.c (Regina side) ---------------------------------------- */

/* the ARexx error code of a Regina error; -1: not an error on ARexx */
int lxa_arexx_error_code(int errorno, int suberrorno, int lineno);
/* force the code of the next exiterror() (host and library errors) */
void lxa_arexx_raise(int code);
/* the error that ended the program (0: none, -1: silent end) */
extern int lxa_arexx_errcode;
/* is <name> an ARexx built-in function? */
int lxa_arexx_bif(const char *name, int len);
/* "+++ Error N in line L: text" */
void lxa_arexx_report(int code, int lineno);

#define LXA_AREXX_VERSION "ARexx V1.15 68070 68881 PAL 50HZ"

#ifdef __REXX_H_INCLUDED
/* Regina-internal hooks (lxa_hooks.c) */
streng *lxa_arexx_show( tsd_t *TSD, char opt, const streng *name, const streng *sep );
streng *lxa_arexx_source( tsd_t *TSD, const char *invoked );
void lxa_arexx_options( tsd_t *TSD, streng *opts );
void lxa_arexx_poll( tsd_t *TSD );
streng *lxa_arexx_find_routine( const tsd_t *TSD, const char *inname, FILE **fp );
void lxa_arexx_command_done( tsd_t *TSD, const streng *cmd, int rc_code,
                             const streng *rc_value, cnodeptr thisptr );
#endif

#endif
