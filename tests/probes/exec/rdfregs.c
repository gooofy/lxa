/*
 * Probe (Phase 237d): the registers RawDoFmt() hands its PutChProc and
 * gives back.  Fred Fish PhxAss (disk 853) counts the characters in D7
 * from its PutChProc ("addq.w #1,d7; move.b d0,(a3)+; clr.b (a3)") and
 * then writes D7-1 bytes from A3.  The PutChProc here records D1-D7 and
 * A3-A6 of each call, counts in D3-D5 and advances A3; the caller sets
 * D2-D7 and A3-A6 before the call and prints them afterwards.  (On 3.1
 * D2, D6 and D7 are RawDoFmt's working registers: a PutChProc that
 * changes them, like PhxAss's, garbles the output.)
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static ULONG seen[8][12];       /* per call: d1-d7, a3-a6 */
static ULONG ncalls;
static ULONG after[10];         /* d2-d7, a3-a6 after the call */
static UBYTE buf[64];

__asm__(
    "    .text\n"
    "    .globl _rdf_putch\n"
    "_rdf_putch:\n"
    "    move.l a0,-(sp)\n"
    "    move.l _ncalls,a0\n"
    "    cmp.l #8,a0\n"
    "    bcc.s 1f\n"
    "    move.l a0,-(sp)\n"
    "    add.l a0,a0\n"
    "    add.l a0,a0\n"
    "    move.l a0,-(sp)\n"
    "    add.l a0,a0\n"
    "    add.l (sp)+,a0\n"          /* index * 12 */
    "    add.l a0,a0\n"
    "    add.l a0,a0\n"             /* * 4 */
    "    add.l #_seen,a0\n"
    "    movem.l d1-d7,(a0)\n"
    "    movem.l a3-a6,28(a0)\n"
    "    move.l (sp)+,a0\n"
    "1:  addq.l #1,a0\n"
    "    move.l a0,_ncalls\n"
    "    move.l (sp)+,a0\n"
    "    addq.l #1,d3\n"
    "    addq.l #1,d4\n"
    "    addq.l #1,d5\n"
    "    move.b d0,(a3)+\n"
    "    clr.b (a3)\n"
    "    rts\n"
    "    .globl _rdf_call\n"
    /* rdf_call(fmt, data): D2-D7 = 0x100..0x600, A3 = buf, A4-A6 kept */
    "_rdf_call:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),a0\n"
    "    move.l 52(sp),a1\n"
    "    lea _rdf_putch,a2\n"
    "    lea _buf,a3\n"
    "    move.l #0x100,d2\n"
    "    move.l #0x200,d3\n"
    "    move.l #0x300,d4\n"
    "    move.l #0x400,d5\n"
    "    move.l #0x500,d6\n"
    "    moveq #0,d7\n"
    "    move.l _SysBase,a6\n"
    "    jsr -522(a6)\n"
    "    movem.l d2-d7/a3-a6,_after\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");
extern APTR rdf_call(const char *fmt, APTR data);

static const char *rname[] = { "d1", "d2", "d3", "d4", "d5", "d6", "d7", "a3", "a4", "a5", "a6" };

static void hexreg(const char *name, ULONG v, ULONG base)
{
    probe_s(name);
    probe_s("=");
    if (v >= base && v < base + 64) {
        probe_s("buf+");
        probe_dec((LONG)(v - base));
    } else if (v == (ULONG)SysBase)
        probe_s("SysBase");
    else
        probe_hex(v, 8);
    probe_ch(' ');
}

int main(void)
{
    static WORD data[] = { 7 };
    ULONG i, j, base = (ULONG)buf;

    ncalls = 0;
    rdf_call("ab%d", data);
    probe_s("calls ");
    probe_dec((LONG)ncalls);
    probe_s(" text ");
    probe_s((const char *)buf);
    probe_ch('\n');
    for (i = 0; i < ncalls && i < 8; i++) {
        probe_s("call ");
        probe_dec((LONG)i);
        probe_s(": ");
        for (j = 2; j < 5; j++)         /* d3-d5 */
            hexreg(rname[j], seen[i][j], base);
        hexreg("a3", seen[i][7], base);
        probe_ch('\n');
    }
    probe_s("after: ");
    for (j = 0; j < 6; j++)
        hexreg(rname[j + 1], after[j], base);
    hexreg("a3", after[6], base);
    hexreg("a6", after[9], base);
    probe_ch('\n');
    return 0;
}
