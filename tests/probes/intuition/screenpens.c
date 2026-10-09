/*
 * Probe (Phase 234): the PaletteExtra of screens Intuition opens, and
 * ObtainBestPenA() on them.  Directory Opus opens a depth 3 custom screen,
 * loads its palette with LoadRGB32() and then asks ObtainBestPenA() for
 * its eight colours: on AmigaOS 3.1 that shares pens 0-3 and allocates
 * 7, 6, 5, 4; on lxa ObtainBestPenA() failed (no PalExtra).
 */
#include <exec/memory.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

/* Directory Opus' colours, 8 bits per gun */
static const UBYTE dopus_rgb[8][3] = {
    { 0xaf, 0xaf, 0xaf }, { 0x00, 0x00, 0x00 }, { 0xff, 0xff, 0xff }, { 0x0f, 0x5f, 0xbf },
    { 0xef, 0xaf, 0x4f }, { 0x7f, 0x00, 0x7f }, { 0xff, 0xff, 0x00 }, { 0xcf, 0x2f, 0x00 },
};

static ULONG g32(UBYTE v)
{
    return (ULONG)v * 0x01010101UL;
}

static void p_pe(struct ColorMap *cm)
{
    struct PaletteExtra *pe = cm->PalExtra;
    int i, n = cm->Count < 16 ? cm->Count : 16;

    if (!pe)
    {
        probe_s(" no PalExtra\n");
        return;
    }
    probe_s(" palextra");
    ps_kv("nfree", pe->pe_NFree);
    ps_kv("firstfree", pe->pe_FirstFree);
    ps_kv("nshared", pe->pe_NShared);
    ps_kv("firstshared", pe->pe_FirstShared);
    ps_kv("sharable", pe->pe_SharableColors);
    probe_s(" vp=");
    probe_s(pe->pe_ViewPort ? "set" : "NULL");
    probe_ch('\n');
    probe_s(" refcnt");
    for (i = 0; i < n; i++)
    {
        probe_ch(' ');
        probe_dec(((UWORD *)pe->pe_RefCnt)[i]);
    }
    probe_ch('\n');
}

static void p_colors(struct ColorMap *cm)
{
    ULONG rgb[3];
    int i, n = cm->Count < 8 ? cm->Count : 8;

    probe_s(" colors");
    for (i = 0; i < n; i++)
    {
        GetRGB32(cm, i, 1, rgb);
        probe_ch(' ');
        probe_hex(rgb[0] >> 24, 2);
        probe_hex(rgb[1] >> 24, 2);
        probe_hex(rgb[2] >> 24, 2);
    }
    probe_ch('\n');
}

static void run(const char *name, struct Screen *s, BOOL load)
{
    struct ColorMap *cm;
    LONG pens[8];
    int i;

    P_SECTION(name);
    if (!s)
    {
        probe_s("OpenScreen = NULL\n");
        return;
    }
    cm = s->ViewPort.ColorMap;
    probe_s(" count");
    probe_dec(cm->Count);
    probe_ch('\n');
    p_pe(cm);
    if (load)
    {
        /* LoadRGB32 table: count, first, then RGB triplets, 0 */
        ULONG tab[1 + 8 * 3 + 1];
        tab[0] = (8UL << 16) | 0;
        for (i = 0; i < 8; i++)
        {
            tab[1 + i * 3] = g32(dopus_rgb[i][0]);
            tab[2 + i * 3] = g32(dopus_rgb[i][1]);
            tab[3 + i * 3] = g32(dopus_rgb[i][2]);
        }
        tab[25] = 0;
        LoadRGB32(&s->ViewPort, tab);
    }
    if (cm->PalExtra)
    {
        probe_s(" obtainbestpen");
        for (i = 0; i < 8; i++)
        {
            pens[i] = ObtainBestPenA(cm, g32(dopus_rgb[i][0]), g32(dopus_rgb[i][1]),
                                     g32(dopus_rgb[i][2]), NULL);
            probe_ch(' ');
            probe_dec(pens[i]);
        }
        probe_ch('\n');
        p_pe(cm);
        p_colors(cm);
        for (i = 0; i < 8; i++)
            if (pens[i] >= 0)
                ReleasePen(cm, pens[i]);
    }
    CloseScreen(s);
}

int main(void)
{
    struct NewScreen ns;
    UWORD nopens[] = { (UWORD)~0 };

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;

    memset(&ns, 0, sizeof(ns));
    ns.Width = 640; ns.Height = 256; ns.Depth = 3;
    ns.ViewModes = HIRES;
    ns.Type = CUSTOMSCREEN;
    ns.DetailPen = 0; ns.BlockPen = 1;
    run("NewScreen depth 3, LoadRGB32", OpenScreen(&ns), TRUE);
    run("NewScreen depth 3", OpenScreen(&ns), FALSE);
    ns.Depth = 2;
    run("NewScreen depth 2, LoadRGB32", OpenScreen(&ns), TRUE);
    run("tags depth 3, LoadRGB32",
        OpenScreenTags(NULL, SA_Depth, 3, SA_DisplayID, HIRES_KEY, TAG_END), TRUE);
    run("tags depth 3 SA_Pens, LoadRGB32",
        OpenScreenTags(NULL, SA_Depth, 3, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)nopens, TAG_END), TRUE);
    run("tags depth 3 SA_SharePens, LoadRGB32",
        OpenScreenTags(NULL, SA_Depth, 3, SA_DisplayID, HIRES_KEY, SA_SharePens, TRUE, TAG_END), TRUE);
    run("tags depth 3 SA_Pens SA_SharePens, LoadRGB32",
        OpenScreenTags(NULL, SA_Depth, 3, SA_DisplayID, HIRES_KEY, SA_Pens, (ULONG)nopens,
                       SA_SharePens, TRUE, TAG_END), TRUE);

    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
