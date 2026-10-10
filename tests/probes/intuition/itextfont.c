/*
 * Probe (Phase 244): IntuiTextLength() and PrintIText() with the
 * IntuiText's own font (ITextFont).  FinalWriter centres its button labels
 * with IntuiTextLength() in its proportional screen font.
 * rdd: fonts wb31
 */
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/diskfont.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *DiskfontBase;

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };
static struct TextAttr topaz9 = { (STRPTR)"topaz.font", 9, 0, 0 };
static struct TextAttr helv11 = { (STRPTR)"helvetica.font", 11, 0, 0 };
static struct TextAttr helv13 = { (STRPTR)"helvetica.font", 13, 0, 0 };   /* not opened: not in memory */
static struct TextAttr missing = { (STRPTR)"nosuch.font", 8, 0, 0 };
static struct TextAttr topaz8b = { (STRPTR)"topaz.font", 8, FSF_BOLD, 0 };

static void len(const char *name, struct TextAttr *ta)
{
    struct IntuiText it = { 1, 0, JAM1, 0, 0, NULL, (UBYTE *)"Abbruch Wie", NULL };

    it.ITextFont = ta;
    probe_s(name);
    ps_kv("len", IntuiTextLength(&it));
    probe_ch('\n');
}

static void print(const char *name, struct RastPort *rp, struct TextAttr *ta, WORD le, WORD te)
{
    struct IntuiText it = { 2, 1, JAM2, 0, 0, NULL, (UBYTE *)"Abbruch Wie", NULL };
    struct TextFont *before = rp->Font;
    LONG y, x, h = 0;

    it.ITextFont = ta;
    it.LeftEdge = le;
    it.TopEdge = te;
    SetRast(rp, 0);
    SetAPen(rp, 3);
    SetBPen(rp, 0);
    SetDrMd(rp, JAM1);
    PrintIText(rp, &it, 4, 3);
    for (y = 0; y < 40; y++)
        for (x = 0; x < 160; x++)
            h = h * 31 + ReadPixel(rp, x, y);
    probe_s(name);
    ps_kx("hash", (ULONG)h, 8);
    ps_kv("apen", rp->FgPen);
    ps_kv("bpen", rp->BgPen);
    ps_kv("drmd", rp->DrawMode);
    probe_s(rp->Font == before ? " font kept" : " font changed");
    ps_kv("cpx", rp->cp_x);
    ps_kv("cpy", rp->cp_y);
    probe_ch('\n');
}

int main(void)
{
    struct TextFont *h11, *t8;
    struct BitMap *bm;
    struct RastPort rp;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !DiskfontBase)
        return 20;
    h11 = OpenDiskFont(&helv11);
    P_NULL("helvetica 11", h11);

    len("null", NULL);
    len("topaz 8", &topaz8);
    len("topaz 9", &topaz9);
    len("helvetica 11", &helv11);
    len("helvetica 13 not in memory", &helv13);
    len("missing", &missing);
    len("topaz 8 bold", &topaz8b);

    bm = AllocBitMap(160, 40, 2, BMF_CLEAR, NULL);
    t8 = OpenFont(&topaz8);
    if (bm && t8)
    {
        InitRastPort(&rp);
        rp.BitMap = bm;
        SetFont(&rp, t8);
        print("print null", &rp, NULL, 0, 0);
        print("print topaz 8", &rp, &topaz8, 0, 0);
        print("print topaz 9", &rp, &topaz9, 0, 0);
        print("print helvetica 11", &rp, &helv11, 2, 5);
        print("print missing", &rp, &missing, 0, 0);
        WaitBlit();
        FreeBitMap(bm);
        CloseFont(t8);
    }
    if (h11)
        CloseFont(h11);
    CloseLibrary(DiskfontBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
