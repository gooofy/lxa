/*
 * Probe (Phase 252): intuition's public image classes.
 *  - sysiclass: the size of every image for SYSISIZE_LOWRES/MEDRES/HIRES,
 *    on the Workbench (new look), on an old-look screen (no SA_Pens) and
 *    on screens with larger fonts (topaz 11, times 18), plus a hash of the
 *    rendered pixels (IDS_NORMAL, IDS_SELECTED)
 *  - frameiclass: IM_FRAMEBOX for each frame type
 *  - itexticlass: box and pixels of an IntuiText image
 *  - pointerclass: the object and its attributes
 * rdd: fonts wb31
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/pointerclass.h>
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
struct Library *DiskfontBase;

static struct RastPort g_rp;
static struct BitMap *g_bm;

static const char *which_names[] = {
    "DEPTH", "ZOOM", "SIZE", "CLOSE", "?4", "SDEPTH", "?6", "?7", "?8", "?9",
    "LEFT", "UP", "RIGHT", "DOWN", "CHECK", "MX", "MENUCHECK", "AMIGAKEY"
};

static ULONG hash_box(LONG w, LONG h)
{
    LONG x, y;
    ULONG v = 2166136261UL;
    for (y = 0; y < h + 2; y++)
        for (x = 0; x < w + 2; x++)
        {
            v ^= (ULONG)ReadPixel(&g_rp, x, y);
            v *= 16777619UL;
        }
    return v;
}

static void sysimages(const char *label, struct DrawInfo *dri)
{
    LONG size, which;

    P_SECTION(label);
    probe_s("dri"); ps_kx("flags", dri->dri_Flags, 8); ps_kv("depth", dri->dri_Depth);
    ps_kv("fonty", dri->dri_Font ? dri->dri_Font->tf_YSize : -1);
    ps_kv("resx", dri->dri_Resolution.X); ps_kv("resy", dri->dri_Resolution.Y);
    probe_ch('\n');
    for (size = SYSISIZE_MEDRES; size <= SYSISIZE_HIRES; size++)
    {
        for (which = 0; which <= AMIGAKEY; which++)
        {
            Object *o;
            struct Image *im;
            if (which == 4 || (which >= 6 && which <= 9))
                continue;
            o = NewObject(NULL, (UBYTE *)"sysiclass", SYSIA_Which, which, SYSIA_Size, size,
                          SYSIA_DrawInfo, (ULONG)dri, TAG_END);
            probe_s(which_names[which]);
            probe_s(size == SYSISIZE_MEDRES ? "/med" : size == SYSISIZE_LOWRES ? "/low" : "/hi");
            if (!o)
            {
                probe_s(" NULL\n");
                continue;
            }
            im = (struct Image *)o;
            ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
            if (im->Width > 0 && im->Height > 0 && im->Width < 60 && im->Height < 40)
            {
                SetRast(&g_rp, 0);
                DrawImageState(&g_rp, im, 1, 1, IDS_NORMAL, dri);
                ps_kx("n", hash_box(im->Width, im->Height), 8);
                SetRast(&g_rp, 0);
                DrawImageState(&g_rp, im, 1, 1, IDS_SELECTED, dri);
                ps_kx("s", hash_box(im->Width, im->Height), 8);
                SetRast(&g_rp, 0);
                DrawImageState(&g_rp, im, 1, 1, IDS_INACTIVENORMAL, dri);
                ps_kx("i", hash_box(im->Width, im->Height), 8);
            }
            probe_ch('\n');
            DisposeObject(o);
        }
    }
}

static void frames(struct DrawInfo *dri)
{
    static const ULONG types[] = { FRAME_DEFAULT, FRAME_BUTTON, FRAME_RIDGE, FRAME_ICONDROPBOX };
    static const char *names[] = { "default", "button", "ridge", "icondropbox" };
    int i, r;

    P_SECTION("frameiclass");
    for (i = 0; i < 4; i++)
        for (r = 0; r < 2; r++)
        {
            Object *o = NewObject(NULL, (UBYTE *)"frameiclass", IA_FrameType, types[i], IA_Recessed, r, TAG_END);
            struct IBox in = { 10, 20, 30, 12 }, out = { 0, 0, 0, 0 };
            struct impFrameBox fb;
            struct Image *im = (struct Image *)o;
            if (!o)
                continue;
            probe_s(names[i]);
            probe_s(r ? "/rec" : "/norm");
            ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
            fb.MethodID = IM_FRAMEBOX;
            fb.imp_ContentsBox = &in;
            fb.imp_FrameBox = &out;
            fb.imp_DrInfo = dri;
            fb.imp_FrameFlags = 0;
            ps_kv("ret", (LONG)DoMethodA(o, (Msg)&fb));
            probe_s(" frame"); ps_box(out.Left, out.Top, out.Width, out.Height);
            fb.imp_FrameFlags = FRAMEF_SPECIFY;
            out.Left = out.Top = 0; out.Width = 60; out.Height = 30;
            DoMethodA(o, (Msg)&fb);
            probe_s(" specify"); ps_box(out.Left, out.Top, out.Width, out.Height);
            im->Width = 30; im->Height = 12;
            SetRast(&g_rp, 0);
            DrawImageState(&g_rp, im, 1, 1, IDS_NORMAL, dri);
            ps_kx("n", hash_box(30, 12), 8);
            SetRast(&g_rp, 0);
            DrawImageState(&g_rp, im, 1, 1, IDS_SELECTED, dri);
            ps_kx("s", hash_box(30, 12), 8);
            probe_ch('\n');
            DisposeObject(o);
        }
}

static struct IntuiText it2 = { 3, 0, JAM1, 0, 10, NULL, (UBYTE *)"Two", NULL };
static struct IntuiText it1 = { 1, 2, JAM2, 4, 1, NULL, (UBYTE *)"Text", &it2 };

static void itext(struct DrawInfo *dri)
{
    Object *o;
    struct Image *im;

    P_SECTION("itexticlass");
    o = NewObject(NULL, (UBYTE *)"itexticlass", IA_Data, (ULONG)&it1, TAG_END);
    P_NULL("object", o);
    if (!o)
        return;
    im = (struct Image *)o;
    probe_s("itext"); ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
    ps_kv("depth", im->Depth);
    probe_ch('\n');
    SetRast(&g_rp, 0);
    DrawImageState(&g_rp, im, 2, 2, IDS_NORMAL, dri);
    probe_s("normal"); ps_kx("hash", hash_box(48, 24), 8); probe_ch('\n');
    SetRast(&g_rp, 0);
    DrawImageState(&g_rp, im, 2, 2, IDS_SELECTED, dri);
    probe_s("selected"); ps_kx("hash", hash_box(48, 24), 8); probe_ch('\n');
    SetRast(&g_rp, 0);
    DrawImageState(&g_rp, im, 2, 2, IDS_NORMAL, NULL);
    probe_s("nodri"); ps_kx("hash", hash_box(48, 24), 8); probe_ch('\n');
    DisposeObject(o);
    o = NewObject(NULL, (UBYTE *)"itexticlass", IA_Data, (ULONG)&it1, IA_FGPen, 3, IA_Left, 5, IA_Top, 6,
                  IA_Width, 30, IA_Height, 9, TAG_END);
    if (o)
    {
        im = (struct Image *)o;
        probe_s("itext pens box"); ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
        probe_ch('\n');
        SetRast(&g_rp, 0);
        DrawImageState(&g_rp, im, 2, 2, IDS_NORMAL, dri);
        probe_s("normal"); ps_kx("hash", hash_box(48, 24), 8); probe_ch('\n');
        DisposeObject(o);
    }
}

static void pointer(void)
{
    struct BitMap *pbm = AllocBitMap(16, 8, 2, BMF_CLEAR, NULL);
    Object *o;
    ULONG v;

    P_SECTION("pointerclass");
    if (!pbm)
        return;
    o = NewObject(NULL, (UBYTE *)"pointerclass", POINTERA_BitMap, (ULONG)pbm, POINTERA_XOffset, -3,
                  POINTERA_YOffset, -2, POINTERA_WordWidth, 1, POINTERA_XResolution, POINTERXRESN_HIRES,
                  POINTERA_YResolution, POINTERYRESN_HIGH, TAG_END);
    P_NULL("object", o);
    if (o)
    {
        static const ULONG attrs[] = { POINTERA_BitMap, POINTERA_XOffset, POINTERA_YOffset, POINTERA_WordWidth,
                                       POINTERA_XResolution, POINTERA_YResolution, 0 };
        static const char *names[] = { "BitMap", "XOffset", "YOffset", "WordWidth", "XResolution", "YResolution" };
        int i;
        for (i = 0; attrs[i]; i++)
        {
            v = 0xdeadbeef;
            probe_s(names[i]);
            if (GetAttr(attrs[i], o, &v))
            {
                probe_s(" = ");
                if (attrs[i] == POINTERA_BitMap)
                    probe_s(v == (ULONG)pbm ? "caller" : (v ? "other" : "NULL"));
                else
                    probe_dec((LONG)v);
                probe_ch('\n');
            }
            else
                probe_s(" not gettable\n");
        }
        DisposeObject(o);
    }
    o = NewObject(NULL, (UBYTE *)"pointerclass", TAG_END);
    P_NULL("default object", o);
    if (o)
        DisposeObject(o);
    FreeBitMap(pbm);
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

int main(void)
{
    struct Screen *wb, *s;
    struct DrawInfo *dri, *sdri;
    struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct TextAttr topaz11 = { (STRPTR)"topaz.font", 11, 0, 0 };
    struct TextAttr times18 = { (STRPTR)"times.font", 18, 0, 0 };
    struct TextFont *tf;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !DiskfontBase)
        return 20;
    g_bm = AllocBitMap(160, 80, 3, BMF_CLEAR, NULL);
    if (!g_bm)
        return 20;
    InitRastPort(&g_rp);
    g_rp.BitMap = g_bm;

    wb = LockPubScreen(NULL);
    dri = GetScreenDrawInfo(wb);
    sysimages("sysiclass on the Workbench", dri);
    frames(dri);
    itext(dri);
    pointer();

    sdri = screen_dri(&s, &topaz8, FALSE);
    if (sdri)
    {
        sysimages("sysiclass old look topaz 8", sdri);
        FreeScreenDrawInfo(s, sdri);
        CloseScreen(s);
    }
    tf = OpenDiskFont(&topaz11);
    sdri = screen_dri(&s, &topaz11, TRUE);
    if (sdri)
    {
        sysimages("sysiclass new look topaz 11", sdri);
        FreeScreenDrawInfo(s, sdri);
        CloseScreen(s);
    }
    sdri = screen_dri(&s, &topaz11, FALSE);
    if (sdri)
    {
        sysimages("sysiclass old look topaz 11", sdri);
        FreeScreenDrawInfo(s, sdri);
        CloseScreen(s);
    }
    if (tf)
        CloseFont(tf);
    tf = OpenDiskFont(&times18);
    P_NULL("times 18", tf);
    sdri = screen_dri(&s, &times18, TRUE);
    if (sdri)
    {
        sysimages("sysiclass new look times 18", sdri);
        FreeScreenDrawInfo(s, sdri);
        CloseScreen(s);
    }
    if (tf)
        CloseFont(tf);

    FreeScreenDrawInfo(wb, dri);
    UnlockPubScreen(NULL, wb);
    FreeBitMap(g_bm);
    CloseLibrary(DiskfontBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
