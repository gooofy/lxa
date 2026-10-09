/*
 * Probe (Phase 237): where pr_ReturnAddr points while a command runs
 * (RunCommand), relative to the command's entry sp.  Detaching startup
 * code (Fred Fish JbSpool, ColorSaver) unwinds through it.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
static ULONG regs[20];

int main(void)
{
    ULONG *mem = AllocMem(80, MEMF_PUBLIC | MEMF_CLEAR);
    UWORD *c;
    LONG rel;
    if (!mem)
        return 20;
    mem[0] = 80;
    mem[1] = 0;
    c = (UWORD *)&mem[2];
    /* movem.l d0-d7/a0-a7,regs */
    c[0] = 0x48f9; c[1] = 0xffff; *(ULONG *)&c[2] = (ULONG)regs;
    /* movea.l 4.w,a0 ; movea.l 276(a0),a0 ; movea.l 176(a0),a1 */
    c[4] = 0x2078; c[5] = 0x0004;
    c[6] = 0x2068; c[7] = 276;
    c[8] = 0x2268; c[9] = 176;
    /* move.l a1,regs+64 ; move.l (a1),regs+68 ; move.l -4(a1),regs+72 */
    c[10] = 0x23c9; *(ULONG *)&c[11] = (ULONG)&regs[16];
    c[13] = 0x23d1; *(ULONG *)&c[14] = (ULONG)&regs[17];
    c[16] = 0x23e9; c[17] = 0xfffc; *(ULONG *)&c[18] = (ULONG)&regs[18];
    /* move.l (sp),regs+76 ; moveq #0,d0 ; rts */
    c[20] = 0x23d7; *(ULONG *)&c[21] = (ULONG)&regs[19];
    c[23] = 0x7000; c[24] = 0x4e75;
    CacheClearU();
    RunCommand(MKBADDR(&mem[1]), 8000, (STRPTR)"\n", 1);
    rel = (LONG)regs[16] - (LONG)regs[15];
    P_LONG("pr_ReturnAddr - entry sp", rel);
    P_BOOL("(pr_ReturnAddr) = stack size", regs[17] == 8000);
    P_BOOL("-4(pr_ReturnAddr) = return address at entry", regs[18] == regs[19]);
    FreeMem(mem, 80);
    return 0;
}
