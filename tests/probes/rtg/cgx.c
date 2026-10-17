/*
 * Probe (Phase 242): cybergraphics.library on the uaegfx board (the
 * CyberGraphX v41 emulation of Picasso96 2.0, rtg profile) - identity,
 * IsCyberModeID/GetCyberIDAttr, BestCModeIDTagList, AllocCModeListTagList
 * filters, GetCyberMapAttr and LockBitMapTagList for planar, Workbench and
 * offscreen bitmaps, and the pixel functions on offscreen bitmaps of every
 * board format, in a window and on a 16 bit screen.
 *
 * Left out (not reproducible): lib_PosSize, CyberModeNode ln_Type/ln_Pri,
 * the colour of unwritten BMF_CLEAR pixels and of RECTFMT_LUT8 pens on
 * true colour bitmaps, ReadPixelArray from CLUT bitmaps, ScalePixelArray
 * beyond its first pixel and R5G5B5 bitmaps.
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
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <lxa_p96.h>
#include "probe.h"

struct Library *P96Base = NULL;
struct Library *CyberGfxBase = NULL;

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

static void mapattrs(const char *n, struct BitMap *bm)
{
    ULONG a;
    P_SECTION(n);
    for (a = 0x80000001; a <= 0x80000009; a++) {
        ULONG v = GetCyberMapAttr(bm, a);
        probe_s("map "); probe_hex(a & 0xff, 2); probe_s(" = ");
        if (a == CYBRMATTR_DISPADR)
            probe_s(v == 0 ? "0" : (v == (ULONG)bm->Planes[0] ? "planes0" : "other"));
        else
            probe_hex(v, 8);
        probe_ch('\n');
    }
    P_HEX("map bad", GetCyberMapAttr(bm, 0x80000010));
    P_HEX("map 0", GetCyberMapAttr(bm, 0));
}

static ULONG cbest(ULONG t1, ULONG d1, ULONG t2, ULONG d2, ULONG t3, ULONG d3)
{
    struct TagItem t[4];
    t[0].ti_Tag = t1; t[0].ti_Data = d1;
    t[1].ti_Tag = t2; t[1].ti_Data = d2;
    t[2].ti_Tag = t3; t[2].ti_Data = d3;
    t[3].ti_Tag = TAG_DONE;
    return BestCModeIDTagList(t);
}

static void cmodelist(const char *n, struct TagItem *t)
{
    struct List *l = AllocCModeListTagList(t);
    struct CyberModeNode *m;
    int c = 0;
    P_SECTION(n);
    P_NULL("list", l);
    if (!l) return;
    for (m = (struct CyberModeNode *)l->lh_Head; m->Node.ln_Succ; m = (struct CyberModeNode *)m->Node.ln_Succ) {
        probe_s(" "); probe_hex(m->DisplayID, 8); probe_ch(' '); probe_dec(m->Width); probe_ch('x');
        probe_dec(m->Height); probe_ch('x'); probe_dec(m->Depth); probe_s(" \""); probe_s(m->ModeText);
        probe_s("\" name="); probe_s(m->Node.ln_Name == m->ModeText ? "text" : (m->Node.ln_Name ? "other" : "null"));
        probe_s(" tags="); probe_s(m->DisplayTagList ? "set" : "null");
        probe_ch('\n');
        c++;
    }
    P_LONG("count", c);
    FreeCModeList(l);
}

static ULONG hook_calls;
static struct CDrawMsg hook_msg;
static struct RastPort *hook_rp;
static ULONG hook_func(register struct Hook *h __asm("a0"), register struct RastPort *rp __asm("a2"),
                       register struct CDrawMsg *m __asm("a1"))
{
    hook_calls++;
    hook_msg = *m;
    hook_rp = rp;
    return 0;
}

static void pixtests(const char *n, struct RastPort *rp, struct BitMap *bm, int ox, int oy)
{
    UBYTE buf[64];
    int i;
    BOOL clut = GetCyberMapAttr(bm, CYBRMATTR_PIXFMT) == PIXFMT_LUT8;
    P_SECTION(n);
    P_HEX("prefill ret", FillPixelArray(rp, 0, 0, 40, 30, 0x00000000));
    P_HEX("pixfmt", GetCyberMapAttr(bm, CYBRMATTR_PIXFMT));
    P_HEX("wrgb ret", WriteRGBPixel(rp, 1, 1, 0xff102030));
    P_HEX("rrgb", ReadRGBPixel(rp, 1, 1));
    P_HEX("raw", pix(bm, ox + 1, oy + 1));
    P_HEX("fill ret", FillPixelArray(rp, 4, 4, 3, 2, 0x80405060));
    P_HEX("fill", ReadRGBPixel(rp, 5, 5));
    P_HEX("fill raw", pix(bm, ox + 5, oy + 5));
    P_HEX("fill edge", ReadRGBPixel(rp, 7, 5));
    P_HEX("inv ret", InvertPixelArray(rp, 4, 4, 2, 1));
    P_HEX("inv", ReadRGBPixel(rp, 4, 4));
    P_HEX("inv raw", pix(bm, ox + 4, oy + 4));
    P_HEX("inv not", ReadRGBPixel(rp, 4, 5));
    {
        static const UBYTE rgb[12] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc};
        static const UBYTE rgba[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01};
        static const UBYTE grey[4] = {0x00, 0x40, 0x80, 0xff};
        static const UBYTE lut[4] = {0, 1, 2, 3};
        static const ULONG ctab[4] = {0x00ff0000, 0x0000ff00, 0x000000ff, 0x00abcdef};
        P_HEX("wpa rgb ret", WritePixelArray((APTR)rgb, 1, 0, 12, rp, 10, 10, 3, 1, RECTFMT_RGB));
        for (i = 0; i < 3; i++) { probe_s("wpa rgb "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 10), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 10), 8); probe_ch('\n'); }
        P_HEX("wpa rgba ret", WritePixelArray((APTR)rgba, 0, 0, 16, rp, 10, 11, 4, 1, RECTFMT_RGBA));
        for (i = 0; i < 4; i++) { probe_s("wpa rgba "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 11), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 11), 8); probe_ch('\n'); }
        P_HEX("wpa argb ret", WritePixelArray((APTR)rgba, 0, 0, 16, rp, 10, 12, 4, 1, RECTFMT_ARGB));
        for (i = 0; i < 4; i++) { probe_s("wpa argb "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 12), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 12), 8); probe_ch('\n'); }
        P_HEX("wpa grey ret", WritePixelArray((APTR)grey, 0, 0, 4, rp, 10, 13, 4, 1, RECTFMT_GREY8));
        for (i = 0; i < 4; i++) { probe_s("wpa grey "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 13), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 13), 8); probe_ch('\n'); }
        P_HEX("wpa lut ret", WritePixelArray((APTR)lut, 0, 0, 4, rp, 10, 14, 4, 1, RECTFMT_LUT8));
        for (i = 0; clut && i < 4; i++) { probe_s("wpa lut "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 14), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 14), 8); probe_ch('\n'); }
        P_HEX("wlut ret", WriteLUTPixelArray((APTR)lut, 0, 0, 4, rp, (APTR)ctab, 10, 15, 4, 1, CTABFMT_XRGB8));
        for (i = 0; i < 4; i++) { probe_s("wlut "); probe_dec(i); probe_s(" = "); probe_hex(ReadRGBPixel(rp, 10 + i, 15), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 10 + i, oy + 15), 8); probe_ch('\n'); }
        P_HEX("wlut bad fmt ret", WriteLUTPixelArray((APTR)lut, 0, 0, 4, rp, (APTR)ctab, 10, 16, 4, 1, 1));
    }
    {
        static const ULONG f[] = {RECTFMT_RGB, RECTFMT_RGBA, RECTFMT_ARGB, RECTFMT_LUT8, RECTFMT_GREY8};
        int k;
        for (k = 0; k < 5; k++) {
            memset(buf, 0xee, sizeof(buf));
            probe_s("rpa ret "); probe_dec(f[k]); probe_s(" = ");
            probe_hex(ReadPixelArray(buf, 1, 0, 20, rp, 10, 11, 4, 2, f[k]), 8); probe_ch('\n');
            if (!clut) { probe_s("rpa fmt"); probe_dec(f[k]); P_BYTES("", buf, 48); }
        }
    }
    P_HEX("move ret", MovePixelArray(10, 11, rp, 20, 11, 4, 2));
    P_HEX("move 0", ReadRGBPixel(rp, 20, 11));
    P_HEX("move 1", ReadRGBPixel(rp, 23, 12));
    {
        static const UBYTE src[16] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0, 0xb0, 0xc0};
        P_HEX("scale ret", ScalePixelArray((APTR)src, 2, 2, 6, rp, 24, 20, 4, 4, RECTFMT_RGB));
        if (!clut) P_HEX("scale 0", ReadRGBPixel(rp, 24, 20));
    }
    {
        struct Hook h;
        struct TagItem t[] = {{TAG_DONE, 0}};
        memset(&h, 0, sizeof(h));
        h.h_Entry = (ULONG (*)())hook_func;
        hook_calls = 0; memset(&hook_msg, 0, sizeof(hook_msg)); hook_rp = 0;
        DoCDrawMethodTagList(&h, rp, t);
        P_LONG("hook calls", hook_calls);
        P_BOOL("hook mem", hook_msg.cdm_MemPtr == (APTR)p96GetBitMapAttr(bm, P96BMA_MEMORY));
        P_BOOL("hook mem set", hook_msg.cdm_MemPtr != 0);
        P_LONG("hook offx", hook_msg.cdm_offx);
        P_LONG("hook offy", hook_msg.cdm_offy);
        P_LONG("hook xsize", hook_msg.cdm_xsize);
        P_LONG("hook ysize", hook_msg.cdm_ysize);
        P_LONG("hook bpr", hook_msg.cdm_BytesPerRow);
        P_LONG("hook bpp", hook_msg.cdm_BytesPerPix);
        P_LONG("hook cm", hook_msg.cdm_ColorModel);
        P_BOOL("hook rp same", hook_rp == rp);
    }
}

int main(void)
{
    struct Screen *wb;
    P96Base = OpenLibrary(P96NAME, 2);
    CyberGfxBase = OpenLibrary(CYBERGFXNAME, 40);
    P_NULL("cgx", CyberGfxBase);
    if (!CyberGfxBase || !P96Base) return 0;
    P_LONG("ver", CyberGfxBase->lib_Version);
    P_LONG("rev", CyberGfxBase->lib_Revision);
    P_LONG("neg", CyberGfxBase->lib_NegSize);
    P_STR("id", (char *)CyberGfxBase->lib_IdString);
    P_STR("name", CyberGfxBase->lib_Node.ln_Name);

    P_SECTION("ids");
    {
        static const ULONG ids[] = {0x50041000, 0x50041100, 0x50041303, 0x50001000, 0xfff01000, 0x29000, 0x8000, 0x12345678, 0xffffffff};
        int i;
        ULONG a;
        for (i = 0; i < 9; i++) {
            probe_s("id "); probe_hex(ids[i], 8); probe_s(" cyber="); probe_hex(IsCyberModeID(ids[i]), 8);
            for (a = 0x80000001; a <= 0x80000005; a++) { probe_ch(' '); probe_hex(GetCyberIDAttr(a, ids[i]), 8); }
            probe_s(" bad="); probe_hex(GetCyberIDAttr(0x80000006, ids[i]), 8);
            probe_ch('\n');
        }
    }
    P_SECTION("best");
    P_HEX("def", cbest(TAG_DONE, 0, 0, 0, 0, 0));
    P_HEX("640x480x8", cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 8));
    P_HEX("640x480x15", cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 15));
    P_HEX("640x480x16", cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 16));
    P_HEX("640x480x24", cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 24));
    P_HEX("640x480x32", cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 32));
    P_HEX("800x600x4", cbest(CYBRBIDTG_NominalWidth, 800, CYBRBIDTG_NominalHeight, 600, CYBRBIDTG_Depth, 4));
    P_HEX("700x500x16", cbest(CYBRBIDTG_NominalWidth, 700, CYBRBIDTG_NominalHeight, 500, CYBRBIDTG_Depth, 16));
    P_HEX("320x200x8", cbest(CYBRBIDTG_NominalWidth, 320, CYBRBIDTG_NominalHeight, 200, CYBRBIDTG_Depth, 8));
    P_HEX("1024x768x24", cbest(CYBRBIDTG_NominalWidth, 1024, CYBRBIDTG_NominalHeight, 768, CYBRBIDTG_Depth, 24));
    P_HEX("2000x2000x8", cbest(CYBRBIDTG_NominalWidth, 2000, CYBRBIDTG_NominalHeight, 2000, CYBRBIDTG_Depth, 8));
    P_HEX("d33", cbest(CYBRBIDTG_Depth, 33, TAG_DONE, 0, 0, 0));
    P_HEX("board uaegfx", cbest(CYBRBIDTG_BoardName, (ULONG)"uaegfx", CYBRBIDTG_Depth, 16, TAG_DONE, 0));
    P_HEX("board uaegfx.card", cbest(CYBRBIDTG_BoardName, (ULONG)"uaegfx.card", CYBRBIDTG_Depth, 16, TAG_DONE, 0));
    P_HEX("board other", cbest(CYBRBIDTG_BoardName, (ULONG)"CVision3D", CYBRBIDTG_Depth, 16, TAG_DONE, 0));
    P_HEX("monitor", cbest(CYBRBIDTG_MonitorID, 0x5004, CYBRBIDTG_Depth, 16, TAG_DONE, 0));
    P_HEX("monitor bad", cbest(CYBRBIDTG_MonitorID, 0x1234, CYBRBIDTG_Depth, 16, TAG_DONE, 0));

    cmodelist("all", NULL);
    {
        struct TagItem t[] = {{CYBRMREQ_MinWidth, 640}, {CYBRMREQ_MaxWidth, 800}, {CYBRMREQ_MinDepth, 16}, {TAG_DONE, 0}};
        cmodelist("640-800 d16+", t);
    }
    {
        static UWORD models[] = {PIXFMT_LUT8, (UWORD)-1};
        struct TagItem t[] = {{CYBRMREQ_CModelArray, (ULONG)models}, {CYBRMREQ_MaxHeight, 480}, {TAG_DONE, 0}};
        cmodelist("lut8 h<=480", t);
    }
    {
        struct TagItem t[] = {{CYBRMREQ_MaxDepth, 8}, {CYBRMREQ_MinHeight, 700}, {TAG_DONE, 0}};
        cmodelist("d<=8 h>=700", t);
    }
    {
        struct TagItem t[] = {{CYBRMREQ_MinWidth, 5000}, {TAG_DONE, 0}};
        cmodelist("none", t);
    }

    wb = LockPubScreen(NULL);
    mapattrs("wb", wb->RastPort.BitMap);
    {
        struct BitMap *bm = AllocBitMap(32, 8, 2, BMF_CLEAR, NULL);
        mapattrs("planar", bm);
        {
            ULONG lw = 0x1234, lh = 0x1234;
            struct TagItem t[] = {{LBMI_WIDTH, (ULONG)&lw}, {LBMI_HEIGHT, (ULONG)&lh}, {TAG_DONE, 0}};
            APTR h = LockBitMapTagList(bm, t);
            P_NULL("planar lock", h);
            P_HEX("planar lw", lw);
            if (h) UnLockBitMap(h);
        }
        {
            struct RastPort rp;
            InitRastPort(&rp); rp.BitMap = bm;
            SetAPen(&rp, 3); RectFill(&rp, 0, 0, 3, 0);
            P_HEX("planar rrgb", ReadRGBPixel(&rp, 1, 0));
            P_HEX("planar wrgb ret", WriteRGBPixel(&rp, 1, 0, 0x00ffffff));
            P_HEX("planar fill ret", FillPixelArray(&rp, 0, 1, 4, 1, 0x00ffffff));
        }
        FreeBitMap(bm);
    }
    {
        static const ULONG fmts[] = {RGBFB_B8G8R8A8, RGBFB_R5G6B5PC, RGBFB_CLUT, RGBFB_R8G8B8, RGBFB_A8R8G8B8};
        int k;
        for (k = 0; k < 5; k++) {
            struct BitMap *bm = p96AllocBitMap(40, 30, fmts[k] == RGBFB_CLUT ? 8 : 24, BMF_CLEAR, NULL, fmts[k]);
            struct RastPort rp;
            probe_s("offscreen fmt "); probe_dec(fmts[k]); probe_ch('\n');
            if (!bm) { probe_s("NULL\n"); continue; }
            mapattrs("map", bm);
            {
                ULONG v[7];
                APTR h, h2;
                struct TagItem t[] = {{LBMI_WIDTH, (ULONG)&v[0]}, {LBMI_HEIGHT, (ULONG)&v[1]}, {LBMI_DEPTH, (ULONG)&v[2]},
                                      {LBMI_PIXFMT, (ULONG)&v[3]}, {LBMI_BYTESPERPIX, (ULONG)&v[4]}, {LBMI_BYTESPERROW, (ULONG)&v[5]},
                                      {LBMI_BASEADDRESS, (ULONG)&v[6]}, {TAG_DONE, 0}};
                int i;
                memset(v, 0xee, sizeof(v));
                h = LockBitMapTagList(bm, t);
                P_NULL("lock", h);
                for (i = 0; i < 6; i++) { probe_s("lbmi "); probe_dec(i); probe_s(" = "); probe_hex(v[i], 8); probe_ch('\n'); }
                P_BOOL("lbmi base ok", v[6] == p96GetBitMapAttr(bm, P96BMA_MEMORY));
                h2 = LockBitMapTagList(bm, t);
                P_BOOL("lock2 same", h2 == h);
                if (h2) UnLockBitMap(h2);
                if (h) {
                    struct TagItem ut[] = {{UBMI_REALLYUNLOCK, TRUE}, {TAG_DONE, 0}};
                    UnLockBitMapTagList(h, ut);
                }
            }
            InitRastPort(&rp);
            rp.BitMap = bm;
            pixtests("offscreen px", &rp, bm, 0, 0);
            if (fmts[k] == RGBFB_CLUT) {
                /* ExtractColor: pen 2 pixels to a single plane map */
                struct BitMap *sm = AllocBitMap(40, 30, 1, BMF_CLEAR, NULL);
                SetAPen(&rp, 2);
                RectFill(&rp, 0, 0, 7, 0);
                P_HEX("extract ret", ExtractColor(&rp, sm, 2, 0, 0, 16, 2));
                P_HEX("extract plane", ((UBYTE *)sm->Planes[0])[0]);
                P_HEX("extract plane1", ((UBYTE *)sm->Planes[0])[1]);
                FreeBitMap(sm);
            }
            if (fmts[k] == RGBFB_B8G8R8A8) {
                struct BitMap *sm = AllocBitMap(40, 30, 1, BMF_CLEAR, NULL);
                FillPixelArray(&rp, 0, 0, 8, 1, 0x00123456);
                P_HEX("extract32 ret", ExtractColor(&rp, sm, 0x00123456, 0, 0, 16, 2));
                P_HEX("extract32 plane", ((UBYTE *)sm->Planes[0])[0]);
                P_HEX("extract32 plane1", ((UBYTE *)sm->Planes[0])[1]);
                FreeBitMap(sm);
            }
            p96FreeBitMap(bm);
        }
    }
    /* a window on the RTG Workbench */
    {
        struct Window *w = OpenWindowTags(NULL, WA_PubScreen, wb, WA_Left, 30, WA_Top, 40, WA_Width, 100, WA_Height, 60,
                                          WA_Borderless, TRUE, WA_Activate, FALSE, TAG_DONE);
        P_NULL("win", w);
        if (w) {
            pixtests("window px", w->RPort, wb->RastPort.BitMap, 30, 40);
            P_HEX("clip fill ret", FillPixelArray(w->RPort, 95, 0, 20, 2, 0x00ffffff));
            P_HEX("clip in", pix(wb->RastPort.BitMap, 30 + 99, 40));
            P_HEX("clip out", pix(wb->RastPort.BitMap, 30 + 100, 40));
            CloseWindow(w);
        }
    }
    UnlockPubScreen(NULL, wb);
    /* a 16 bit screen */
    {
        struct Screen *s = OpenScreenTags(NULL, SA_DisplayID, cbest(CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, CYBRBIDTG_Depth, 16), SA_Depth, 16, SA_Width, 640, SA_Height, 480,
                                          SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Behind, TRUE, TAG_DONE);
        P_NULL("screen16", s);
        if (s) {
            mapattrs("screen16 map", s->RastPort.BitMap);
            P_HEX("vpid", GetVPModeID(&s->ViewPort));
            P_HEX("cyber vpid", IsCyberModeID(GetVPModeID(&s->ViewPort)));
            {
                struct TagItem t[] = {{SETVC_DPMSLevel, DPMS_ON}, {TAG_DONE, 0}};
                CVideoCtrlTagList(&s->ViewPort, t);
                P_LONG("vctrl", 1);
            }
            CloseScreen(s);
        }
    }
    CloseLibrary(CyberGfxBase);
    CloseLibrary(P96Base);
    return 0;
}
