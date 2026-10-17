/*
 * Probe (Phase 242): Picasso96API.library mode queries on the uaegfx board
 * (Picasso96 2.0, rtg profile) - p96BestModeIDTagList for sizes, depths and
 * format filters, p96AllocModeListTagList with size/depth/format filters.
 * P96Mode ln_Type/ln_Pri are left out (not initialised).
 *
 * rdd: profile rtg
 */
#define LXA_P96_INLINES
#include <exec/types.h>
#include <string.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/displayinfo.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <lxa_p96.h>
#include "probe.h"

struct Library *P96Base = NULL;

static ULONG pix(struct BitMap *bm, int x, int y)
{
    UBYTE *m = (UBYTE *)p96GetBitMapAttr(bm, P96BMA_MEMORY);
    ULONG bpr = p96GetBitMapAttr(bm, P96BMA_BYTESPERROW);
    ULONG bpp = p96GetBitMapAttr(bm, P96BMA_BYTESPERPIXEL);
    UBYTE *p = m + y * bpr + x * bpp;
    if (bpp == 1) return p[0];
    if (bpp == 2) return (p[0] << 8) | p[1];
    if (bpp == 3) return (p[0] << 16) | (p[1] << 8) | p[2];
    return ((ULONG)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}
static void P_PIX(const char *l, struct BitMap *bm, int x, int y)
{
    probe_s(l); probe_s(" = "); probe_hex(pix(bm, x, y), 8); probe_ch('\n');
}

static ULONG best(ULONG t1, ULONG d1, ULONG t2, ULONG d2, ULONG t3, ULONG d3, ULONG t4, ULONG d4)
{
    struct TagItem t[5];
    t[0].ti_Tag = t1; t[0].ti_Data = d1;
    t[1].ti_Tag = t2; t[1].ti_Data = d2;
    t[2].ti_Tag = t3; t[2].ti_Data = d3;
    t[3].ti_Tag = t4; t[3].ti_Data = d4;
    t[4].ti_Tag = TAG_DONE;
    return p96BestModeIDTagList(t);
}

static void modelist(const char *n, struct TagItem *t)
{
    struct List *l = p96AllocModeListTagList(t);
    struct P96Mode *m;
    int c = 0;
    P_SECTION(n);
    P_NULL("list", l);
    if (!l) return;
    for (m = (struct P96Mode *)l->lh_Head; m->Node.ln_Succ; m = (struct P96Mode *)m->Node.ln_Succ) {
        probe_s(" "); probe_hex(m->DisplayID, 8); probe_ch(' '); probe_dec(m->Width); probe_ch('x');
        probe_dec(m->Height); probe_ch('x'); probe_dec(m->Depth); probe_s(" \""); probe_s(m->Description);
        probe_s("\" name="); probe_s(m->Node.ln_Name == m->Description ? "desc" : (m->Node.ln_Name ? "other" : "null"));
        probe_ch('\n');
        c++;
    }
    P_LONG("count", c);
    p96FreeModeList(l);
}

int main(void)
{
    P96Base = OpenLibrary(P96NAME, 2);
    if (!P96Base) return 0;

    P_SECTION("best");
    P_HEX("def", best(TAG_DONE, 0, 0, 0, 0, 0, 0, 0));
    P_HEX("640x480x8", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 8, TAG_DONE, 0));
    P_HEX("640x480x16", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 16, TAG_DONE, 0));
    P_HEX("640x480x15", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 15, TAG_DONE, 0));
    P_HEX("640x480x24", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 24, TAG_DONE, 0));
    P_HEX("640x480x32", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 32, TAG_DONE, 0));
    P_HEX("800x600x4", best(P96BIDTAG_NominalWidth, 800, P96BIDTAG_NominalHeight, 600, P96BIDTAG_Depth, 4, TAG_DONE, 0));
    P_HEX("700x500x8", best(P96BIDTAG_NominalWidth, 700, P96BIDTAG_NominalHeight, 500, P96BIDTAG_Depth, 8, TAG_DONE, 0));
    P_HEX("2000x2000x8", best(P96BIDTAG_NominalWidth, 2000, P96BIDTAG_NominalHeight, 2000, P96BIDTAG_Depth, 8, TAG_DONE, 0));
    P_HEX("100x100x8", best(P96BIDTAG_NominalWidth, 100, P96BIDTAG_NominalHeight, 100, P96BIDTAG_Depth, 8, TAG_DONE, 0));
    P_HEX("1024x768x24", best(P96BIDTAG_NominalWidth, 1024, P96BIDTAG_NominalHeight, 768, P96BIDTAG_Depth, 24, TAG_DONE, 0));
    P_HEX("allowed clut d24", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_Depth, 24, P96BIDTAG_FormatsAllowed, RGBFF_CLUT));
    P_HEX("allowed 565pc", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_FormatsAllowed, RGBFF_R5G6B5PC, TAG_DONE, 0));
    P_HEX("forbid clut", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_FormatsForbidden, RGBFF_CLUT, TAG_DONE, 0));
    P_HEX("allowed argb", best(P96BIDTAG_NominalWidth, 640, P96BIDTAG_NominalHeight, 480, P96BIDTAG_FormatsAllowed, RGBFF_A8R8G8B8, TAG_DONE, 0));
    P_HEX("forbid all", best(P96BIDTAG_FormatsForbidden, 0xffff, TAG_DONE, 0, 0, 0, 0, 0));
    P_HEX("320x200x8", best(P96BIDTAG_NominalWidth, 320, P96BIDTAG_NominalHeight, 200, P96BIDTAG_Depth, 8, TAG_DONE, 0));
    P_HEX("320x240x16", best(P96BIDTAG_NominalWidth, 320, P96BIDTAG_NominalHeight, 240, P96BIDTAG_Depth, 16, TAG_DONE, 0));
    P_HEX("w only 1280", best(P96BIDTAG_NominalWidth, 1280, TAG_DONE, 0, 0, 0, 0, 0));
    P_HEX("depth1", best(P96BIDTAG_Depth, 1, TAG_DONE, 0, 0, 0, 0, 0));

    {
        struct TagItem t[] = {{TAG_DONE, 0}};
        modelist("all", t);
    }
    {
        struct TagItem t[] = {{P96MA_MinWidth, 640}, {P96MA_MaxWidth, 800}, {P96MA_MinDepth, 16}, {TAG_DONE, 0}};
        modelist("640-800 d16+", t);
    }
    {
        struct TagItem t[] = {{P96MA_FormatsAllowed, RGBFF_CLUT}, {P96MA_MaxHeight, 480}, {TAG_DONE, 0}};
        modelist("clut h<=480", t);
    }
    {
        struct TagItem t[] = {{P96MA_MinWidth, 5000}, {TAG_DONE, 0}};
        modelist("none", t);
    }

    CloseLibrary(P96Base);
    return 0;
}
