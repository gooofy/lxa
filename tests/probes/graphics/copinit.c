/*
 * Probe (Phase 251): GfxBase->copinit, the system's initial copper list.
 * Programs walk it (Fish KCommodity looks for the SPR0PTH/SPR0PTL moves to
 * blank the mouse pointer), so it must be there and look like 3.1's.
 * Printed: every copper word, except the sprite pointers, which are shown
 * as what they point to.
 */
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <graphics/copper.h>
#include <graphics/sprite.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase = NULL;

int main(void)
{
    struct copinit *ci;
    UWORD *w;
    LONG i, n = sizeof(struct copinit) / 2;
    ULONG p;
    char label[32];

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    if (!GfxBase)
        return 20;
    P_SECTION("copinit");
    P_NULL("GfxBase->copinit", GfxBase->copinit);
    P_LONG("sizeof(struct copinit)", n * 2);
    if (!GfxBase->copinit)
        return 0;
    ci = GfxBase->copinit;
    w = (UWORD *)ci;
    for (i = 0; i < n; i++) {
        /* SPRxPTH/SPRxPTL values: printed below */
        if (i > 0 && w[i - 1] >= 0x120 && w[i - 1] <= 0x13e && (i & 1))
            continue;
        label[0] = 'w';
        label[1] = '0' + (char)(i / 10);
        label[2] = '0' + (char)(i % 10);
        label[3] = 0;
        P_HEX(label, w[i]);
    }

    P_SECTION("sprite pointers");
    for (i = 0; i < 8; i++) {
        p = ((ULONG)ci->sprstrtup[i * 4 + 1] << 16) | ci->sprstrtup[i * 4 + 3];
        label[0] = 's';
        label[1] = 'p';
        label[2] = 'r';
        label[3] = '0' + (char)i;
        label[4] = 0;
        probe_s(label);
        if (p == (ULONG)ci->sprstop)
            probe_s(" = copinit->sprstop\n");
        else if (!p)
            probe_s(" = 0\n");
        else if (p & 1)
            probe_s(" = odd\n");
        else
            probe_s(" = other chip data\n");
    }
    p = ((ULONG)ci->sprstrtup[1] << 16) | ci->sprstrtup[3];
    P_BOOL("spr0 data is chip memory", p && (TypeOfMem((APTR)p) & MEMF_CHIP));
    P_BOOL("spr0 data is in copinit", p >= (ULONG)ci && p < (ULONG)ci + sizeof(*ci));
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
