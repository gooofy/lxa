/*
 * locprobe.h - shared helpers for the locale.library probes (Phase 222f).
 */
#ifndef LXA_LOCPROBE_H
#define LXA_LOCPROBE_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <utility/hooks.h>
#include <libraries/locale.h>
#include <clib/exec_protos.h>
#include <clib/locale_protos.h>
#include <inline/exec.h>
#include <inline/locale.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct LocaleBase *LocaleBase;

/* PutChar hook: a0 = hook, a1 = character, a2 = locale.
 * h_Data points to a Sink; at most SINK_MAX bytes are stored. */
#define SINK_MAX 400
struct Sink {
    UBYTE *p;
    ULONG n;
    UBYTE buf[SINK_MAX + 4];
};

__asm__(
    "    .text\n"
    "    .globl _probe_putch\n"
    "_probe_putch:\n"
    "    move.l 16(a0),a0\n"
    "    cmpi.l #400,4(a0)\n"
    "    bcc.s 1f\n"
    "    move.l a1,d1\n"
    "    move.l (a0),a1\n"
    "    move.b d1,(a1)+\n"
    "    move.l a1,(a0)\n"
    "    addq.l #1,4(a0)\n"
    "    rts\n"
    "1:  addq.l #1,4(a0)\n"
    "    rts\n");
extern ULONG probe_putch(void);

/* GetChar hook: a0 = hook, a2 = locale.  h_Data points to a Source;
 * returns the next byte, NUL forever at the end; counts the calls. */
struct Source {
    const UBYTE *p;
    ULONG calls;
};

__asm__(
    "    .text\n"
    "    .globl _probe_getch\n"
    "_probe_getch:\n"
    "    move.l 16(a0),a0\n"
    "    addq.l #1,4(a0)\n"
    "    move.l (a0),a1\n"
    "    moveq #0,d0\n"
    "    move.b (a1),d0\n"
    "    beq.s 1f\n"
    "    addq.l #1,(a0)\n"
    "1:  rts\n");
extern ULONG probe_getch(void);

static struct Sink sink;
static struct Hook put_hook;

static void sink_reset(void)
{
    sink.p = sink.buf;
    sink.n = 0;
    put_hook.h_Entry = (ULONG (*)())probe_putch;
    put_hook.h_SubEntry = 0;
    put_hook.h_Data = &sink;
}

/* print the collected stream, escaping NUL and non-printables */
static void sink_print(void)
{
    ULONG i;
    probe_ch('"');
    for (i = 0; i < sink.n && i < SINK_MAX; i++) {
        UBYTE c = sink.buf[i];
        if (c == 0)
            probe_s("\\0");
        else if (c < 32 || c > 126 || c == '"' || c == '\\') {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[c >> 4]);
            probe_ch("0123456789abcdef"[c & 15]);
        } else
            probe_ch((char)c);
    }
    probe_ch('"');
    probe_s(" n=");
    probe_dec((LONG)sink.n);
}

/* print a string with escapes, or NULL */
static void probe_qs(const UBYTE *s)
{
    if (!s) {
        probe_s("NULL");
        return;
    }
    probe_ch('"');
    while (*s) {
        UBYTE c = *s++;
        if (c < 32 || c > 126 || c == '"' || c == '\\') {
            probe_s("\\x");
            probe_ch("0123456789abcdef"[c >> 4]);
            probe_ch("0123456789abcdef"[c & 15]);
        } else
            probe_ch((char)c);
    }
    probe_ch('"');
}

static void P_QS(const char *label, const UBYTE *s)
{
    probe_s(label);
    probe_s(" = ");
    probe_qs(s);
    probe_ch('\n');
}

static int open_locale_lib(void)
{
    LocaleBase = (struct LocaleBase *)OpenLibrary((STRPTR)"locale.library", 38);
    return LocaleBase != NULL;
}

#endif
