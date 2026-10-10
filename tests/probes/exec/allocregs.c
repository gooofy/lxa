/*
 * Probe (Phase 237d): the scratch registers AllocMem() leaves behind.
 * Fred Fish AskFirst and SofTrack (disks 753, 754) call AllocMem(134,
 * MEMF_CLEAR) and then AllocMem(134) without setting D1 again, so the
 * second call's requirements are whatever the first left in D1.  Prints
 * whether A1 holds the result and, after MEMF_CLEAR, whether D1 is 0,
 * and whether the Z flag reflects D0 (Fish Missile, GadToolsBox branch on
 * it right after the call).
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* acall(size, flags, out[4]) : out = d0 d1 a0 a1 after AllocMem */
void acall(ULONG size, ULONG flags, ULONG *out);
__asm__(
    "    .text\n"
    "    .globl _acall\n"
    "_acall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),d0\n"
    "    move.l 52(sp),d1\n"
    "    move.l #0x13579bdf,a0\n"
    "    move.l #0x2468ace0,a1\n"
    "    move.l _SysBase,a6\n"
    "    jsr -198(a6)\n"
    "    seq d2\n"
    "    move.l 56(sp),a2\n"
    "    movem.l d0-d1/a0-a1,(a2)\n"
    "    move.b d2,16(a2)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static void t(const char *name, ULONG size, ULONG flags)
{
    ULONG out[5];
    out[4] = 0;
    acall(size, flags, out);
    probe_s(name);
    probe_s(out[0] ? ": ok" : ": NULL");
    /* A1: the block (or 0); D1 after a MEMF_CLEAR allocation (the
     * clearing loop's counter); other values are internal scratch */
    probe_s(out[3] == out[0] ? ", a1 = result" : ", a1 other");
    if (flags & MEMF_CLEAR)
        probe_s(out[1] == 0 ? ", d1 = 0" : ", d1 not 0");
    /* the condition codes follow D0: compilers branch on them */
    probe_s(((out[4] >> 24) != 0) == (out[0] == 0) ? ", Z = (d0 == 0)" : ", Z other");
    probe_ch('\n');
    if (out[0])
        FreeMem((APTR)out[0], size);
}

int main(void)
{
    t("134, MEMF_CLEAR", 134, MEMF_CLEAR);
    t("134, MEMF_ANY", 134, MEMF_ANY);
    t("134, MEMF_PUBLIC|MEMF_CLEAR", 134, MEMF_PUBLIC | MEMF_CLEAR);
    t("134, MEMF_CHIP", 134, MEMF_CHIP);
    t("4000, MEMF_CLEAR", 4000, MEMF_CLEAR);
    t("too large", 0x7ffffff0, MEMF_ANY);
    return 0;
}
