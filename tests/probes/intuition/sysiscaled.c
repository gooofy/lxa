/*
 * Probe (Phase 252): sysiclass pixel art that tools/rdd/sysitable.py turns
 * into lxa's image tables (lxa_sysi_data.h):
 *  - every image in every size and state on an old-look screen (no
 *    SA_Pens), drawn on pen 3 so that untouched pixels stay visible
 *  - MENUCHECK and AMIGAKEY on new-look screens with every Workbench 3.1
 *    font; their size follows the screen font (width: advance of 'm' + 7,
 *    resp. twice that - 7; height: the baseline, at least 8)
 * rdd: fonts wb31
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/text.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *DiskfontBase = NULL;

static struct RastPort g_rp;

#define OX 4
#define OY 4

static const char *which_names[] = {
    "DEPTH", "ZOOM", "SIZE", "CLOSE", "?4", "SDEPTH", "?6", "?7", "?8", "?9",
    "LEFT", "UP", "RIGHT", "DOWN", "CHECK", "MX", "MENUCHECK", "AMIGAKEY"
};

/* the image box plus a one-pixel margin */
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

static void dumpimg(struct DrawInfo *dri, LONG which, LONG size, int nstates, LONG bg)
{
    static const ULONG states[] = { IDS_NORMAL, IDS_SELECTED, IDS_INACTIVENORMAL, IDS_DISABLED };
    static const char *sn[] = { "normal", "selected", "inactive", "disabled" };
    Object *o = NewObject(NULL, (UBYTE *)"sysiclass", SYSIA_Which, which, SYSIA_Size, size,
                          SYSIA_DrawInfo, (ULONG)dri, TAG_END);
    struct Image *im = (struct Image *)o;
    int s;
    if (!o)
        return;
    for (s = 0; s < nstates; s++)
    {
        probe_s(which_names[which]);
        probe_s(size == SYSISIZE_MEDRES ? "/med " : size == SYSISIZE_LOWRES ? "/low " : "/hi ");
        probe_s(sn[s]);
        ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
        probe_ch('\n');
        SetRast(&g_rp, bg);
        DrawImageState(&g_rp, im, OX, OY, states[s], dri);
        art(im);
    }
    DisposeObject(o);
}

static struct DrawInfo *screen_dri(struct Screen **sp, struct TextAttr *ta, BOOL newlook)
{
    static UWORD pens[] = { (UWORD)~0 };
    struct Screen *s = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2,
                                      SA_DisplayID, HIRES_KEY, SA_Font, (ULONG)ta,
                                      newlook ? SA_Pens : TAG_IGNORE, (ULONG)pens,
                                      SA_Quiet, TRUE, SA_ShowTitle, FALSE, TAG_END);
    *sp = s;
    return s ? GetScreenDrawInfo(s) : NULL;
}

static void pens(struct DrawInfo *dri)
{
    int i;
    probe_s("pens");
    for (i = 0; i < dri->dri_NumPens && i < 12; i++)
    {
        probe_ch(' ');
        probe_dec(dri->dri_Pens[i]);
    }
    probe_ch('\n');
}

int main(void)
{
    struct Screen *s;
    struct DrawInfo *dri;
    struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };
    static const char *fn[] = {
        "topaz.font", "topaz.font",
        "courier.font", "courier.font", "courier.font", "courier.font", "courier.font",
        "diamond.font", "diamond.font", "emerald.font", "emerald.font",
        "garnet.font", "garnet.font",
        "helvetica.font", "helvetica.font", "helvetica.font", "helvetica.font", "helvetica.font",
        "helvetica.font", "opal.font", "opal.font", "ruby.font", "ruby.font", "ruby.font",
        "sapphire.font", "sapphire.font",
        "times.font", "times.font", "times.font", "times.font", "times.font", NULL };
    static const UWORD fs[] = { 9, 11, 11, 13, 15, 18, 24, 12, 20, 17, 20, 9, 16, 9, 11, 13, 15, 18, 24,
                                9, 12, 8, 12, 15, 14, 19, 11, 13, 15, 18, 24 };
    LONG which, size;
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !DiskfontBase)
        return 20;
    InitRastPort(&g_rp);
    g_rp.BitMap = AllocBitMap(96, 48, 2, BMF_CLEAR, NULL);
    if (!g_rp.BitMap)
        return 20;

    dri = screen_dri(&s, &topaz8, FALSE);
    if (dri)
    {
        P_SECTION("old look topaz 8");
        pens(dri);
        for (size = SYSISIZE_MEDRES; size <= SYSISIZE_HIRES; size++)
            for (which = 0; which <= AMIGAKEY; which++)
                if (!(which == 4 || (which >= 6 && which <= 9)))
                    dumpimg(dri, which, size, 4, 3);
        FreeScreenDrawInfo(s, dri);
        CloseScreen(s);
    }
    for (i = 0; fn[i]; i++)
    {
        struct TextAttr ta;
        struct TextFont *tf;
        ta.ta_Name = (STRPTR)fn[i];
        ta.ta_YSize = fs[i];
        ta.ta_Style = 0;
        ta.ta_Flags = 0;
        tf = OpenDiskFont(&ta);
        probe_s("-- font ");
        probe_s(fn[i]);
        probe_ch(' ');
        probe_dec(fs[i]);
        if (!tf)
        {
            probe_s(" missing\n");
            continue;
        }
        probe_s(" base=");
        probe_dec(tf->tf_Baseline);
        probe_ch('\n');
        dri = screen_dri(&s, &ta, TRUE);
        if (dri)
        {
            dumpimg(dri, MENUCHECK, SYSISIZE_MEDRES, 1, 0);
            dumpimg(dri, AMIGAKEY, SYSISIZE_MEDRES, 1, 0);
            FreeScreenDrawInfo(s, dri);
            CloseScreen(s);
        }
        CloseFont(tf);
    }
    FreeBitMap(g_rp.BitMap);
    CloseLibrary(DiskfontBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
