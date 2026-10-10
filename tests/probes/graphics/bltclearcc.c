/*
 * Probe (Phase 237e): what BltClear() leaves in D0, D1, A0, A1 and the
 * condition codes.  Fred Fish AmigaCOMAL (disk 296) clears its planes with
 * BltClear() at the end of a routine and the caller branches on Z.
 * Prints D0, D1, A0/A1 relative to the memory and the flags (XNZVC) for
 * several byte counts and flag combinations.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include "probe.h"
extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
static ULONG out[6];
/* call BltClear(a1=mem,d0=count,d1=flags) with d2/a0 preset; record d0,d1,a0,a1,ccr */
static void bc(APTR mem, ULONG cnt, ULONG flags, ULONG ccr_in, ULONG d2v, ULONG a0v)
{
    register ULONG r_d0 __asm("d0") = cnt;
    register ULONG r_d1 __asm("d1") = flags;
    register APTR r_a1 __asm("a1") = mem;
    register ULONG r_d5 __asm("d5") = ccr_in;
    register ULONG r_d2 __asm("d2") = d2v;
    register ULONG r_a0 __asm("a0") = a0v;
    register APTR r_a3 __asm("a3") = out;
    register APTR r_a4 __asm("a4") = GfxBase;
    __asm__ __volatile__ ("move.l a6,-(sp)\n\tmove.l a4,a6\n\tmove.w d5,ccr\n\tjsr -300(a6)\n\t"
                          ".short 0x42c5\n\tmovem.l d0-d1/a0-a1,(a3)\n\tmove.l d5,16(a3)\n\tmove.l (sp)+,a6"
                          : "+r" (r_d0), "+r" (r_d1), "+r" (r_a1), "+r" (r_d5), "+r" (r_d2), "+r" (r_a0), "+r" (r_a3), "+r" (r_a4)
                          : : "cc", "memory");
}
static void hex(ULONG v)
{
    int i;
    for (i = 28; i >= 0; i -= 4)
        probe_ch("0123456789abcdef"[(v >> i) & 15]);
}
static void show(const char *w, ULONG mem)
{
    probe_s(w); probe_s(" d0="); hex(out[0]); probe_s(" d1="); hex(out[1]);
    probe_s(" a1-mem="); hex(out[3] - mem);
    probe_s(" ccr="); hex(out[4] & 0x1f); probe_ch('\n');
}
int main(void)
{
    UBYTE *chip, *p1, *p2;
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
    if (!GfxBase) return 20;
    chip = AllocMem(16000, MEMF_CHIP);
    bc(chip, 16000, 1, 4, 0, 0); show("mem 16000 f1", (ULONG)chip);
    bc(chip, 16000, 0, 4, 0, 0); show("mem 16000 f0", (ULONG)chip);
    bc(chip, 16000, 1, 0, 1, 0x12345678); show("mem 16000 f1 d2=1", (ULONG)chip);
    p1 = AllocRaster(640, 200);
    p2 = AllocRaster(640, 200);
    bc(p1, 16000, 1, 4, 1, 0); show("raster1 16000 f1", (ULONG)p1);
    bc(p2, 16000, 1, 4, 0, 0); show("raster2 16000 f1", (ULONG)p2);
    bc(chip, 100, 1, 4, 0, 0); show("mem 100 f1", (ULONG)chip);
    bc(chip, 2, 1, 4, 0, 0); show("mem 2 f1", (ULONG)chip);
    bc(chip, 0, 1, 4, 0, 0); show("mem 0 f1", (ULONG)chip);
    WaitBlit();
    FreeRaster(p1, 640, 200);
    FreeRaster(p2, 640, 200);
    FreeMem(chip, 16000);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
