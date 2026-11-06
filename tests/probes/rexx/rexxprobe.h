/*
 * rexxprobe.h - common declarations for the rexxsyslib.library / ARexx
 * probes (Phase 250).
 */
#ifndef REXXPROBE_H
#define REXXPROBE_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <rexx/errors.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/rexxsyslib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/rexxsyslib.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct RxsLib *RexxSysBase = NULL;

/* the RexxArg in front of an argstring */
#define RARG(s) ((struct RexxArg *)((UBYTE *)(s) - 8))

static int str_eq(const char *a, const char *b)
{
    if (!a || !b)
        return a == b;
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static LONG str_len(const char *s)
{
    LONG n = 0;
    while (s[n])
        n++;
    return n;
}

/* an argstring: its text, length and the RexxArg header fields a program
 * can rely on (ra_Length, ra_Flags, ra_Hash) */
static void P_ARGSTR(const char *label, UBYTE *s)
{
    probe_s(label);
    if (!s) {
        probe_s(" = NULL\n");
        return;
    }
    probe_s(" = \"");
    probe_s((char *)s);
    probe_s("\" len=");
    probe_dec(LengthArgstring(s));
    probe_s(" ra_Length=");
    probe_dec(RARG(s)->ra_Length);
    probe_s(" ra_Flags=");
    probe_hex(RARG(s)->ra_Flags, 2);
    probe_s(" ra_Hash=");
    probe_hex(RARG(s)->ra_Hash, 2);
    probe_ch('\n');
}

static BOOL write_file(const char *name, const char *text)
{
    BPTR fh = Open((STRPTR)name, MODE_NEWFILE);
    if (!fh)
        return FALSE;
    Write(fh, (APTR)text, str_len(text));
    Close(fh);
    return TRUE;
}

/* print a text file line by line with a prefix */
static void P_FILE(const char *label, const char *name)
{
    static char buf[512];
    BPTR fh = Open((STRPTR)name, MODE_OLDFILE);
    LONG n, i;
    int bol = 1;

    if (!fh) {
        probe_s(label);
        probe_s(": (no file)\n");
        return;
    }
    n = Read(fh, buf, sizeof(buf) - 1);
    Close(fh);
    if (n < 0)
        n = 0;
    for (i = 0; i < n; i++) {
        if (bol) {
            probe_s(label);
            probe_s(": ");
            bol = 0;
        }
        probe_ch(buf[i]);
        if (buf[i] == '\n')
            bol = 1;
    }
    if (!bol)
        probe_ch('\n');
}

static struct MsgPort *find_port(const char *name)
{
    struct MsgPort *p;
    Forbid();
    p = FindPort((STRPTR)name);
    Permit();
    return p;
}

/* wait (up to ~10 s) until the public port appears or disappears */
static BOOL wait_port(const char *name, BOOL present)
{
    int i;
    for (i = 0; i < 500; i++) {
        if ((find_port(name) != NULL) == present)
            return TRUE;
        Delay(1);
    }
    return FALSE;
}

/* run a shell command synchronously with NIL: for input and output */
static LONG run_cmd(const char *cmd)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    LONG rc = SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, TAG_DONE);
    Close(in);
    Close(out);
    return rc;
}

/* start RexxMast (if it is not running yet) and wait for the REXX port */
static BOOL start_rexxmast(void)
{
    if (find_port("REXX"))
        return TRUE;
    run_cmd("Run >NIL: SYS:System/RexxMast >NIL:");
    return wait_port("REXX", TRUE);
}

/* stop RexxMast again (RXC) and wait until its port is gone */
static BOOL stop_rexxmast(void)
{
    run_cmd("SYS:Rexxc/RXC");
    return wait_port("REXX", FALSE);
}

#endif
