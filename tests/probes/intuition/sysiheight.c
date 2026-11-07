/*
 * Probe (Phase 252): sysiclass window and screen gadget images drawn at the
 * heights a larger screen font gives the title bar (IA_Height; the width
 * as sysiclass chooses it), new look, medres and lowres.  The art is
 * printed one character per pixel (pen number) with a one-pixel margin
 * of pen 3.
 * rdd: fonts wb31
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct RastPort g_rp;

#define OX 4
#define OY 4

static const char *which_names[] = {
    "DEPTH", "ZOOM", "SIZE", "CLOSE", "?4", "SDEPTH"
};

static void art(struct Image *im)
{
    LONG x, y;
    for (y = im->TopEdge - 1; y <= im->TopEdge + im->Height; y++)
    {
        probe_s("   ");
        for (x = im->LeftEdge - 1; x <= im->LeftEdge + im->Width; x++)
            probe_ch((char)('0' + ReadPixel(&g_rp, OX + x, OY + y)));
        probe_ch('\n');
    }
}

static void dumpimg(struct DrawInfo *dri, LONG which, LONG size, LONG h)
{
    Object *o = NewObject(NULL, (UBYTE *)"sysiclass", SYSIA_Which, which, SYSIA_Size, size,
                          SYSIA_DrawInfo, (ULONG)dri, IA_Height, h, TAG_END);
    struct Image *im = (struct Image *)o;
    probe_s(which_names[which]);
    probe_s(size == SYSISIZE_LOWRES ? "/low h=" : "/med h=");
    probe_dec(h);
    if (!o)
    {
        probe_s(" fail\n");
        return;
    }
    ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
    probe_ch('\n');
    SetRast(&g_rp, 3);
    DrawImageState(&g_rp, im, OX, OY, IDS_NORMAL, dri);
    art(im);
    DisposeObject(o);
}

int main(void)
{
    static UWORD pens[] = { (UWORD)~0 };
    struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct Screen *s;
    struct DrawInfo *dri;
    static const LONG sizes[] = { SYSISIZE_MEDRES, SYSISIZE_LOWRES };
    LONG which, h, size;
    int si;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    InitRastPort(&g_rp);
    g_rp.BitMap = AllocBitMap(64, 48, 2, BMF_CLEAR, NULL);
    if (!g_rp.BitMap)
        return 20;
    s = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2,
                       SA_DisplayID, HIRES_KEY, SA_Font, (ULONG)&topaz8, SA_Pens, (ULONG)pens,
                       SA_Quiet, TRUE, SA_ShowTitle, FALSE, TAG_END);
    dri = s ? GetScreenDrawInfo(s) : NULL;
    if (!dri)
        return 20;
    for (si = 0; si < 2; si++)
        for (size = sizes[si], which = DEPTHIMAGE; which <= SDEPTHIMAGE; which++)
        {
            if (which == 4)
                continue;
            for (h = 8; h <= 20; h++)
                dumpimg(dri, which, size, h);
        }
    FreeScreenDrawInfo(s, dri);
    CloseScreen(s);
    FreeBitMap(g_rp.BitMap);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
