/*
 * Probe (Phase 237): scratch registers (d0/d1/a0/a1) after the blitter
 * calls.  They are not preserved by contract, but AMOS clears its screens
 * with a loop that keeps a0 across WaitBlit() (Fish Planetarium, WhereK).
 */
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
static struct Library *GfxBase;
static ULONG out[4];

static void call(LONG lvo)
{
    register ULONG r_d0 __asm("d0") = 0x5a5a5a5a;
    register ULONG r_d1 __asm("d1") = 0x5a5a5a5a;
    register ULONG r_a0 __asm("a0") = 0x5a5a5a5a;
    register ULONG r_a1 __asm("a1") = 0x5a5a5a5a;
    register APTR r_a2 __asm("a2") = (APTR)((UBYTE *)GfxBase + lvo);
    register APTR r_a3 __asm("a3") = out;
    register APTR r_a4 __asm("a4") = GfxBase;
    __asm__ __volatile__ ("move.l a6,-(sp)\n\tmove.l a4,a6\n\tjsr (a2)\n\tmovem.l d0-d1/a0-a1,(a3)\n\tmove.l (sp)+,a6"
                          : "+r" (r_d0), "+r" (r_d1), "+r" (r_a0), "+r" (r_a1), "+r" (r_a2), "+r" (r_a3), "+r" (r_a4)
                          : : "cc", "memory");
}

static void show(const char *what)
{
    static const char *n[4] = { "d0", "d1", "a0", "a1" };
    int i;
    probe_s(what);
    for (i = 0; i < 4; i++) {
        probe_s(" ");
        probe_s(n[i]);
        probe_s(out[i] == 0x5a5a5a5a ? "=kept" : "=other");
    }
    probe_ch('\n');
}

int main(void)
{
    GfxBase = OpenLibrary("graphics.library", 0);
    if (!GfxBase)
        return 20;
    P_SECTION("registers after the call");
    call(-228);                 /* WaitBlit */
    show("WaitBlit");
    call(-456);                 /* OwnBlitter */
    show("OwnBlitter");
    call(-228);                 /* WaitBlit while owning */
    show("WaitBlit (owned)");
    call(-462);                 /* DisownBlitter */
    show("DisownBlitter");
    CloseLibrary(GfxBase);
    return 0;
}
