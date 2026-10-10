/*
 * Probe (Phase 237c): disk.resource - the drive IDs, unit allocation and
 * GetUnit()/GiveUnit() as Fred Fish SuperDuper (x6) uses them before it
 * opens its window.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/interrupts.h>
#include <resources/disk.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* d0 = call(base, lvo, d0, a1) */
ULONG dcall(APTR base, LONG lvo, ULONG d0, APTR a1);
__asm__(
    "    .text\n"
    "    .globl _dcall\n"
    "_dcall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l 48(sp),a6\n"
    "    move.l 48(sp),a2\n"
    "    add.l 52(sp),a2\n"
    "    move.l 56(sp),d0\n"
    "    move.l 60(sp),a1\n"
    "    jsr (a2)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static struct DiscResourceUnit dru;

int main(void)
{
    APTR disk = OpenResource((CONST_STRPTR)DISKNAME);
    ULONG r;
    int u;

    P_NULL("OpenResource(disk.resource)", disk);
    if (!disk)
        return 0;
    for (u = 0; u < 4; u++) {
        probe_s("unit ");
        probe_dec(u);
        probe_s(": GetUnitID ");
        probe_hex(dcall(disk, -30, u, NULL), 8);
        probe_s(" ReadUnitID ");
        probe_hex(dcall(disk, -36, u, NULL), 8);
        probe_ch('\n');
    }
    r = dcall(disk, -6, 1, NULL);
    P_LONG("AllocUnit(1)", r);
    r = dcall(disk, -6, 1, NULL);
    P_LONG("AllocUnit(1) again", r);
    dcall(disk, -12, 1, NULL);
    r = dcall(disk, -6, 1, NULL);
    P_LONG("AllocUnit(1) after FreeUnit", r);
    dcall(disk, -12, 1, NULL);
    r = dcall(disk, -6, 3, NULL);
    P_LONG("AllocUnit(3)", r);
    dcall(disk, -12, 3, NULL);

    dru.dru_Message.mn_Node.ln_Type = NT_MESSAGE;
    r = dcall(disk, -18, 0, &dru);
    P_BOOL("GetUnit() result non-NULL", r != 0);
    P_BOOL("GetUnit() result is our unit", r == (ULONG)&dru);
    if (r)
        dcall(disk, -24, 0, NULL);
    P_BOOL("GiveUnit() done", TRUE);
    return 0;
}
