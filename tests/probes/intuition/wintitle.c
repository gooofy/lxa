/*
 * Probe (Phase 252): where the window title starts in the title bar, per
 * screen resolution and window gadgets.  Two windows, one titled "" and
 * one titled "MMMM", are compared row by row: the first and last columns
 * that differ are the title's extent.
 */
#include <exec/memory.h>
#include <graphics/displayinfo.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct Window *openwin(struct Screen *s, const char *title, ULONG flags)
{
    return OpenWindowTags(NULL, WA_CustomScreen, (ULONG)s, WA_Left, 10, WA_Top, 20,
                          WA_Width, 200, WA_Height, 60, WA_Title, (ULONG)title,
                          WA_Flags, flags, TAG_END);
}

static void measure(struct Screen *s, const char *what, ULONG flags)
{
    struct Window *a = openwin(s, "", flags);
    struct Window *b = openwin(s, "MMMM", flags);
    LONG x, y, x0 = 9999, x1 = -1, y0 = 9999, y1 = -1;

    probe_s(what);
    if (!a || !b)
    {
        probe_s(" no window\n");
    }
    else
    {
        for (y = 0; y < b->BorderTop; y++)
            for (x = 0; x < b->Width; x++)
                if (ReadPixel(a->RPort, x, y) != ReadPixel(b->RPort, x, y))
                {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
        ps_kv("bt", b->BorderTop);
        if (x1 < 0)
            probe_s(" same");
        else
            ps_box(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
        probe_ch('\n');
    }
    if (b) CloseWindow(b);
    if (a) CloseWindow(a);
}

int main(void)
{
    static const ULONG modes[] = { LORES_KEY, HIRES_KEY, HIRESLACE_KEY, SUPER_KEY, LORESLACE_KEY };
    static const char *names[] = { "lores", "hires", "hireslace", "superhires", "loreslace" };
    static UWORD pens[] = { (UWORD)~0 };
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    for (i = 0; i < 5; i++)
    {
        int look;
        for (look = 0; look < 2; look++)
        {
            struct Screen *s = OpenScreenTags(NULL, SA_Width, 320, SA_Height, 200, SA_Depth, 2,
                                              SA_DisplayID, modes[i],
                                              look ? SA_Pens : TAG_IGNORE, (ULONG)pens,
                                              SA_Quiet, TRUE, TAG_END);
            probe_s("-- ");
            probe_s(names[i]);
            probe_s(look ? " newlook\n" : " oldlook\n");
            if (!s)
                continue;
            measure(s, "close drag depth", WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET);
            measure(s, "drag depth", WFLG_DRAGBAR | WFLG_DEPTHGADGET);
            measure(s, "close", WFLG_CLOSEGADGET);
            measure(s, "close size depth", WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET |
                                           WFLG_SIZEGADGET);
            CloseScreen(s);
        }
    }
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
