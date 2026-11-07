/*
 * Probe (Phase 252): what buttongclass's GM_RENDER draws - with GA_Text or
 * GA_IntuiText alone (nothing), with a frameiclass GA_Image (the image, in
 * IDS_SELECTED when selected), with and without GA_DrawInfo.
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static void art(struct RastPort *rp, LONG x0, LONG y0, LONG w, LONG h)
{
    LONG x, y;
    for (y = y0; y < y0 + h; y++)
    {
        probe_s("   ");
        for (x = x0; x < x0 + w; x++)
            probe_ch((char)('0' + ReadPixel(rp, x, y)));
        probe_ch('\n');
    }
}

int main(void)
{
    struct Screen *wb;
    struct DrawInfo *dri;
    struct Window *win;
    struct Gadget *g;
    int v;
    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    wb = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(wb);
    win = OpenWindowTags(NULL, WA_Left, 0, WA_Top, 20, WA_Width, 200, WA_Height, 60,
                         WA_Borderless, TRUE, WA_Activate, FALSE, TAG_END);
    static struct IntuiText it = { 1, 0, JAM1, 2, 2, NULL, (UBYTE *)"IT", NULL };
    Object *fr = NewObject(NULL, (UBYTE *)"frameiclass", IA_FrameType, FRAME_BUTTON,
                           IA_Width, 40, IA_Height, 14, TAG_END);
    for (v = 0; v < 6; v++)
    {
        SetRast(win->RPort, 0);
        g = (struct Gadget *)NewObject(NULL, (UBYTE *)"buttongclass", GA_Left, 10, GA_Top, 5,
                                       GA_Width, 40, GA_Height, 14,
                                       v < 4 ? GA_Text : GA_IntuiText, v < 4 ? (ULONG)"Btn" : (ULONG)&it,
                                       v < 4 ? GA_Image : TAG_IGNORE, (ULONG)fr,
                                       v & 1 ? GA_DrawInfo : TAG_IGNORE, (ULONG)dri,
                                       v & 2 ? GA_Selected : TAG_IGNORE, TRUE, TAG_END);
        probe_s(v < 4 ? "image+text " : "intuitext ");
        probe_s(v & 1 ? "dri" : "nodri");
        probe_s(v & 2 ? " selected\n" : "\n");
        AddGList(win, g, -1, 1, NULL);
        RefreshGList(g, win, NULL, 1);
        art(win->RPort, 8, 3, 46, 18);
        RemoveGList(win, g, 1);
        DisposeObject((Object *)g);
    }
    DisposeObject(fr);
    CloseWindow(win);
    FreeScreenDrawInfo(wb, dri);
    UnlockPubScreen(NULL, wb);
    return 0;
}
