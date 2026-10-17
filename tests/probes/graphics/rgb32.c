/*
 * Probe (Phase 240): 256-colour planar screens and 24-bit palettes on AGA.
 * A depth-8 screen with SA_Colors32, then LoadRGB32() of all 256 colours,
 * SetRGB32(), SetRGB32CM() and SetRGB4(); every change is read back with
 * GetRGB32(), GetRGB4() and GetColorMap()-level fields.  ObtainBestPen()
 * shows which pen the palette offers for a 24-bit colour.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/gfx.h>
#include <graphics/view.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static void rgb(struct ColorMap *cm, ULONG pen)
{
    ULONG t[3];

    GetRGB32(cm, pen, 1, t);
    probe_s("GetRGB32 ");
    probe_dec(pen);
    probe_s(" = ");
    probe_hex(t[0], 8);
    probe_ch(' ');
    probe_hex(t[1], 8);
    probe_ch(' ');
    probe_hex(t[2], 8);
    probe_s(" GetRGB4 = ");
    probe_hex(GetRGB4(cm, pen), 4);
    probe_ch('\n');
}

static void pens(struct ColorMap *cm)
{
    rgb(cm, 0);
    rgb(cm, 1);
    rgb(cm, 17);
    rgb(cm, 100);
    rgb(cm, 254);
    rgb(cm, 255);
}

static void dump_pe(struct ColorMap *cm)
{
    struct PaletteExtra *pe = cm->PalExtra;
    UWORD *ref;
    UBYTE *link;
    UWORD p;
    ULONG i;

    probe_s("PalExtra = ");
    probe_s(pe ? "non-NULL" : "NULL");
    probe_ch('\n');
    if (!pe)
        return;
    ref = (UWORD *)pe->pe_RefCnt;
    link = (UBYTE *)pe->pe_AllocList;
    probe_s("NFree = "); probe_dec(pe->pe_NFree);
    probe_s(" FirstFree = "); probe_dec(pe->pe_FirstFree);
    probe_s(" NShared = "); probe_dec(pe->pe_NShared);
    probe_s(" FirstShared = "); probe_dec(pe->pe_FirstShared);
    probe_s(" SharableColors = "); probe_dec(pe->pe_SharableColors);
    probe_ch('\n');
    p = pe->pe_FirstShared;
    probe_s("shared list:");
    for (i = 0; i < pe->pe_NShared && i < 20 && p < 256; i++) {
        probe_ch(' ');
        probe_dec(p);
        probe_ch('/');
        probe_dec(ref[p]);
        p = link[p];
    }
    probe_ch('\n');
}

/* the pens Intuition shares on a screen of the given depth */
static void depth_pens(UWORD depth)
{
    struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 0},
                           {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE}, {TAG_DONE, 0} };
    struct Screen *s;

    t[2].ti_Data = depth;
    s = OpenScreenTagList(NULL, t);
    probe_s("-- depth ");
    probe_dec(depth);
    probe_ch('\n');
    if (s) {
        dump_pe(s->ViewPort.ColorMap);
        CloseScreen(s);
    }
}

int main(void)
{
    static ULONG load[2 + 256 * 3];
    ULONG colors32[] = { (3L << 16) | 16,
                         0x11111111, 0x22222222, 0x33333333,
                         0x44444444, 0x55555555, 0x66666666,
                         0x77777777, 0x88888888, 0x99999999,
                         0 };
    struct Screen *s;
    struct ColorMap *cm;
    ULONG i;
    LONG pen;

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!GfxBase || !IntuitionBase)
        return 20;

    {
        struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 8},
                               {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE},
                               {SA_Colors32, (ULONG)colors32}, {TAG_DONE, 0} };
        s = OpenScreenTagList(NULL, t);
    }
    probe_s("OpenScreen depth 8 = ");
    probe_s(s ? "non-NULL" : "NULL");
    probe_ch('\n');
    if (!s)
        goto out;

    cm = s->ViewPort.ColorMap;
    probe_s("Depth = ");
    probe_dec(s->RastPort.BitMap->Depth);
    probe_s(" cm Count = ");
    probe_dec(cm->Count);
    probe_s(" cm Type = ");
    probe_dec(cm->Type);
    probe_ch('\n');

    probe_s("-- SA_Colors32 at 16\n");
    rgb(cm, 15);
    rgb(cm, 16);
    rgb(cm, 17);
    rgb(cm, 18);
    rgb(cm, 19);

    /* all 256 colours with 32 distinct bits each */
    load[0] = (256L << 16) | 0;
    for (i = 0; i < 256; i++) {
        load[1 + i * 3 + 0] = (i << 24) | 0x00a5a5a5;
        load[1 + i * 3 + 1] = ((255 - i) << 24) | 0x005a5a5a;
        load[1 + i * 3 + 2] = ((i * 7) << 24) | 0x00123456;
    }
    load[1 + 256 * 3] = 0;
    LoadRGB32(&s->ViewPort, load);
    probe_s("-- LoadRGB32 256\n");
    pens(cm);

    SetRGB32(&s->ViewPort, 100, 0x12345678, 0x9abcdef0, 0x0fedcba9);
    probe_s("-- SetRGB32 100\n");
    rgb(cm, 100);

    SetRGB32CM(cm, 254, 0xffffffff, 0x80000000, 0x01010101);
    probe_s("-- SetRGB32CM 254\n");
    rgb(cm, 254);

    SetRGB4(&s->ViewPort, 255, 1, 2, 3);
    probe_s("-- SetRGB4 255\n");
    rgb(cm, 255);

    probe_s("-- GetRGB32 range\n");
    {
        ULONG t[3 * 4];
        GetRGB32(cm, 98, 4, t);
        for (i = 0; i < 12; i++) {
            probe_hex(t[i], 8);
            probe_ch(i % 3 == 2 ? '\n' : ' ');
        }
    }

    /* the PaletteExtra Intuition attached: 256 pens with UBYTE links */
    probe_s("-- PaletteExtra\n");
    dump_pe(cm);

    probe_s("-- ObtainBestPen\n");
    pen = ObtainBestPen(cm, 0x12345678, 0x9abcdef0, 0x0fedcba9,
                        OBP_Precision, PRECISION_EXACT, TAG_DONE);
    probe_s("exact SetRGB32 colour = ");
    probe_dec(pen);
    probe_ch('\n');
    if (pen >= 0) {
        rgb(cm, pen);
        ReleasePen(cm, pen);
    }
    pen = ObtainBestPen(cm, 0x10000000, 0xf0000000, 0x08000000, TAG_DONE);
    probe_s("near pen 1, default precision = ");
    probe_dec(pen);
    probe_ch('\n');
    if (pen >= 0)
        ReleasePen(cm, pen);
    pen = ObtainPen(cm, -1, 0x11111111, 0x22222222, 0x33333333, 0);
    probe_s("ObtainPen(-1) = ");
    probe_dec(pen);
    probe_ch('\n');
    if (pen >= 0)
        ReleasePen(cm, pen);

    CloseScreen(s);

    for (i = 4; i <= 8; i++)
        depth_pens((UWORD)i);
out:
    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
