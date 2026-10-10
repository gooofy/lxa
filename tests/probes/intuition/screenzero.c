/*
 * Probe (Phase 244): OpenScreen() with a zero width or height.  GFA-BASIC
 * opens its editor screen with NewScreen.Height 0 (HIRES, depth 1); AmigaOS
 * 3.1 opens it at the display height, lxa refused it and GFA-BASIC kept
 * opening windows on the Workbench.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static void try(const char *name, WORD w, WORD h, UWORD depth, UWORD modes)
{
    struct NewScreen ns = { 0, 0, 0, 0, 1, 0, 1, 0, CUSTOMSCREEN, NULL, (UBYTE *)"Z", NULL, NULL };
    struct Screen *s;

    ns.Width = w;
    ns.Height = h;
    ns.Depth = depth;
    ns.ViewModes = modes;
    s = OpenScreen(&ns);
    probe_s(name);
    if (!s)
    {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": ");
    probe_dec(s->Width);
    probe_ch('x');
    probe_dec(s->Height);
    probe_s(" depth ");
    probe_dec(s->BitMap.Depth);
    probe_s(" rows ");
    probe_dec(s->BitMap.Rows);
    probe_ch('\n');
    CloseScreen(s);
}

int main(void)
{
    struct Screen sd;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    /* what GFA-BASIC reads to size its screen: the first 0x4e bytes */
    if (GetScreenData(&sd, 0x4e, WBENCHSCREEN, NULL))
    {
        probe_s("screendata ");
        probe_dec(sd.Width); probe_ch('x'); probe_dec(sd.Height);
        probe_s(" vp ");
        probe_dec(sd.ViewPort.DWidth); probe_ch('x'); probe_dec(sd.ViewPort.DHeight);
        probe_s(" off ");
        probe_dec(sd.ViewPort.DxOffset); probe_ch(','); probe_dec(sd.ViewPort.DyOffset);
        probe_s(" modes ");
        probe_hex(sd.ViewPort.Modes, 4);
        probe_ch('\n');
    }
    {
        /* WBENCHSCREEN ignores the screen argument (GFA-BASIC passes garbage) */
        static struct Screen junk;
        if (GetScreenData(&sd, 0x4e, WBENCHSCREEN, &junk))
        {
            probe_s("screendata junk screen ");
            probe_dec(sd.Width); probe_ch('x'); probe_dec(sd.Height);
            probe_ch('\n');
        }
    }
    try("640x0 hires", 640, 0, 1, HIRES);
    try("640x0 modes 0", 640, 0, 1, 0);
    try("320x0 lores", 320, 0, 2, 0);
    try("0x200 hires", 0, 200, 1, HIRES);
    try("640x0 hires lace", 640, 0, 1, HIRES | LACE);
    try("640x-1 hires", 640, -1, 1, HIRES);
    try("640x-5 hires", 640, -5, 1, HIRES);
    try("0x0 hires", 0, 0, 1, HIRES);
    CloseLibrary(IntuitionBase);
    return 0;
}
