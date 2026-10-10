/*
 * Probe (Phase 237b): the condition codes the math libraries' compare and
 * test functions return.  The autodocs promise them so that a branch can
 * follow the call; Manx Aztec C does exactly that (Fred Fish Gwin's
 * "Window must be at least 20.0 by 20.0 pixels" for a 100 x 100 window).
 * D0 and the CCR bits X N Z V C are printed for each call.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* ULONG mcall(base, lvo, args[4], ccr*): d0-d3 = args, a6 = base */
ULONG mcall(struct Library *base, LONG lvo, ULONG *args, ULONG *ccr);
__asm__(
    "    .text\n"
    "    .globl _mcall\n"
    "_mcall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),a6\n"
    "    move.l 48(sp),a2\n"
    "    add.l 52(sp),a2\n"
    "    move.l 56(sp),a0\n"
    "    movem.l (a0),d0-d3\n"
    "    move.w #0,ccr\n"              /* X clear: only the callee sets it */
    "    jsr (a2)\n"
    "    .short 0x42c7\n"              /* move.w ccr,d7 */
    "    move.l 60(sp),a0\n"
    "    and.l #0x1f,d7\n"
    "    move.l d7,(a0)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static void show(const char *label, struct Library *base, LONG lvo, ULONG a0, ULONG a1, ULONG a2, ULONG a3)
{
    ULONG args[4], ccr = 0, r;
    args[0] = a0;
    args[1] = a1;
    args[2] = a2;
    args[3] = a3;
    r = mcall(base, lvo, args, &ccr);
    probe_s(label);
    probe_s(": d0 ");
    probe_dec((LONG)r);
    probe_s(" ccr");
    probe_s(ccr & 0x10 ? " X" : " -");
    probe_s(ccr & 0x08 ? "N" : "-");
    probe_s(ccr & 0x04 ? "Z" : "-");
    probe_s(ccr & 0x02 ? "V" : "-");
    probe_s(ccr & 0x01 ? "C" : "-");
    probe_ch('\n');
}

/* FFP: 20.0 = 0xa0000045, 100.0 = 0xc8000047, -3.0 = 0xc00000c2 */
#define F20   0xa0000045UL
#define F100  0xc8000047UL
#define FM3   0xc00000c2UL
/* IEEE single */
#define S20   0x41a00000UL
#define S100  0x42c80000UL
#define SM3   0xc0400000UL
/* IEEE double (high words, low word 0) */
#define D20   0x40340000UL
#define D100  0x40590000UL
#define DM3   0xc0080000UL

int main(void)
{
    struct Library *ffp = OpenLibrary((CONST_STRPTR)"mathffp.library", 0);
    struct Library *sb = OpenLibrary((CONST_STRPTR)"mathieeesingbas.library", 0);
    struct Library *db = OpenLibrary((CONST_STRPTR)"mathieeedoubbas.library", 0);

    if (ffp) {
        /* SPCmp(leftParm d1, rightParm d0) */
        show("SPCmp(left 20, right 100)", ffp, -42, F100, F20, 0, 0);
        show("SPCmp(left 100, right 20)", ffp, -42, F20, F100, 0, 0);
        show("SPCmp(left 20, right 20)", ffp, -42, F20, F20, 0, 0);
        show("SPCmp(left -3, right 20)", ffp, -42, F20, FM3, 0, 0);
        show("SPTst(100)", ffp, -48, 0, F100, 0, 0);
        show("SPTst(-3)", ffp, -48, 0, FM3, 0, 0);
        show("SPTst(0)", ffp, -48, 0, 0, 0, 0);
        CloseLibrary(ffp);
    }
    if (sb) {
        /* IEEESPCmp(leftParm d0, rightParm d1) */
        show("IEEESPCmp(left 20, right 100)", sb, -42, S20, S100, 0, 0);
        show("IEEESPCmp(left 100, right 20)", sb, -42, S100, S20, 0, 0);
        show("IEEESPCmp(left 20, right 20)", sb, -42, S20, S20, 0, 0);
        show("IEEESPCmp(left -3, right 20)", sb, -42, SM3, S20, 0, 0);
        show("IEEESPTst(100)", sb, -48, S100, 0, 0, 0);
        show("IEEESPTst(-3)", sb, -48, SM3, 0, 0, 0);
        show("IEEESPTst(0)", sb, -48, 0, 0, 0, 0);
        CloseLibrary(sb);
    }
    if (db) {
        /* IEEEDPCmp(leftParm d0/d1, rightParm d2/d3) */
        show("IEEEDPCmp(left 20, right 100)", db, -42, D20, 0, D100, 0);
        show("IEEEDPCmp(left 100, right 20)", db, -42, D100, 0, D20, 0);
        show("IEEEDPCmp(left 20, right 20)", db, -42, D20, 0, D20, 0);
        show("IEEEDPCmp(left -3, right 20)", db, -42, DM3, 0, D20, 0);
        show("IEEEDPTst(100)", db, -48, D100, 0, 0, 0);
        show("IEEEDPTst(-3)", db, -48, DM3, 0, 0, 0);
        show("IEEEDPTst(0)", db, -48, 0, 0, 0, 0);
        CloseLibrary(db);
    }
    return 0;
}
