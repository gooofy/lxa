/*
 * Probe (Phase 242): Picasso96API.library on screens (Picasso96 2.0, rtg
 * profile) - a 32 bit screen and its ViewPort, the pixel functions in a
 * window (layer clipping), WritePixelArray8, p96PIP_OpenTagList and
 * p96OpenScreenTagList by size, by format and with an unknown mode.
 * Phase 246: the colour of CLUT arrays on true colour bitmaps, the
 * ViewPort Modes/DHeight and Screen.Flags of an RTG screen.
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

static void pixtests(const char *n, struct RastPort *rp, struct BitMap *bm, int ox, int oy)
{
    UBYTE buf[64];
    struct RenderInfo ri;
    int i;
    ULONG fmt = p96GetBitMapAttr(bm, P96BMA_RGBFORMAT);
    P_SECTION(n);
    P_LONG("fmt", fmt);
    p96RectFill(rp, 0, 0, 39, 29, 0x00000000);
    /* write ARGB array */
    {
        static const ULONG src[4] = {0xff112233, 0x80445566, 0x00778899, 0x12aabbcc};
        ri.Memory = (APTR)src; ri.BytesPerRow = 16; ri.pad = 0; ri.RGBFormat = RGBFB_A8R8G8B8;
        p96WritePixelArray(&ri, 0, 0, rp, 2, 2, 4, 1);
        for (i = 0; i < 4; i++) { probe_s("wpa argb "); probe_dec(i); probe_s(" = "); probe_hex(pix(bm, ox + 2 + i, oy + 2), 8); probe_ch('\n'); }
        for (i = 0; i < 4; i++) { probe_s("rp96 "); probe_dec(i); probe_s(" = "); probe_hex(p96ReadPixel(rp, 2 + i, 2), 8); probe_ch('\n'); }
    }
    /* read back into various formats */
    {
        static const ULONG f[] = {RGBFB_A8R8G8B8, RGBFB_R8G8B8, RGBFB_R5G6B5, RGBFB_R5G6B5PC, RGBFB_CLUT, RGBFB_B8G8R8A8};
        int k;
        for (k = 0; k < 6; k++) {
            if (f[k] == fmt) continue;  /* hangs Picasso96 2.0 */
            memset(buf, 0xee, sizeof(buf));
            ri.Memory = buf; ri.BytesPerRow = 32; ri.RGBFormat = f[k];
            p96ReadPixelArray(&ri, 1, 0, rp, 2, 2, 4, 1);
            probe_s("rpa fmt"); probe_dec(f[k]); P_BYTES("", buf, 20);
        }
    }
    /* write R5G6B5 array with source offset */
    {
        static const UWORD s16[4] = {0xf800, 0x07e0, 0x001f, 0xffff};
        ri.Memory = (APTR)s16; ri.BytesPerRow = 8; ri.RGBFormat = RGBFB_R5G6B5;
        p96WritePixelArray(&ri, 1, 0, rp, 2, 3, 3, 1);
        for (i = 0; i < 3; i++) { probe_s("wpa 565 "); probe_dec(i); probe_s(" = "); probe_hex(p96ReadPixel(rp, 2 + i, 3), 8); probe_ch('\n'); }
    }
    /* write CLUT array */
    {
        static const UBYTE s8[4] = {0, 1, 2, 3};
        ri.Memory = (APTR)s8; ri.BytesPerRow = 4; ri.RGBFormat = RGBFB_CLUT;
        p96WritePixelArray(&ri, 0, 0, rp, 2, 4, 4, 1);
        for (i = 0; i < 4; i++) { probe_s("wpa clut "); probe_dec(i); probe_s(" = "); probe_hex(p96ReadPixel(rp, 2 + i, 4), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 2 + i, oy + 4), 8); probe_ch('\n'); }
    }
    /* WritePixel / RectFill */
    P_HEX("wp ret", p96WritePixel(rp, 1, 1, 0x00102030));
    P_HEX("wp", p96ReadPixel(rp, 1, 1));
    P_HEX("wp raw", pix(bm, ox + 1, oy + 1));
    p96RectFill(rp, 8, 8, 11, 9, 0x00405060);
    P_HEX("rf", p96ReadPixel(rp, 9, 9));
    P_HEX("rf edge", p96ReadPixel(rp, 12, 9));
    P_HEX("rf raw", pix(bm, ox + 8, oy + 8));
    /* RastPort pens after */
    P_LONG("apen", rp->FgPen);
    /* TrueColorData */
    {
        UBYTE r[8] = {1, 2, 3, 4, 5, 6, 7, 8}, g[8] = {10, 20, 30, 40, 50, 60, 70, 80}, b[8] = {100, 110, 120, 130, 140, 150, 160, 170};
        struct TrueColorInfo tci;
        tci.PixelDistance = 1; tci.BytesPerRow = 4; tci.RedData = r; tci.GreenData = g; tci.BlueData = b;
        p96WriteTrueColorData(&tci, 1, 0, rp, 20, 20, 2, 2);
        P_HEX("tc 0", p96ReadPixel(rp, 20, 20));
        P_HEX("tc 1", p96ReadPixel(rp, 21, 21));
        memset(r, 0, 8); memset(g, 0, 8); memset(b, 0, 8);
        p96ReadTrueColorData(&tci, 0, 0, rp, 20, 20, 2, 2);
        P_BYTES("rtc r", r, 8); P_BYTES("rtc g", g, 8); P_BYTES("rtc b", b, 8);
    }
}

int main(void)
{
    struct Screen *s;
    P96Base = OpenLibrary(P96NAME, 2);
    if (!P96Base) return 0;

    /* screen with window */
    s = OpenScreenTags(NULL, SA_DisplayID, 0x50031303, SA_Depth, 24, SA_Width, 640, SA_Height, 480,
                       SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Behind, TRUE, TAG_DONE);
    P_NULL("screen", s);
    if (s) {
        struct Window *w;
        P_LONG("BarHeight", s->BarHeight);
        P_LONG("BarHBorder", s->BarHBorder);
        P_LONG("BarVBorder", s->BarVBorder);
        P_LONG("WBorTop", s->WBorTop);
        P_LONG("WBorLeft", s->WBorLeft);
        P_LONG("vp.DWidth", s->ViewPort.DWidth);
        P_LONG("vp.DxOffset", s->ViewPort.DxOffset);
        P_HEX("vp.Modes", s->ViewPort.Modes);
        P_LONG("vp.DHeight", s->ViewPort.DHeight);
        P_HEX("Flags", s->Flags);
        w = OpenWindowTags(NULL, WA_CustomScreen, s, WA_Left, 10, WA_Top, 20, WA_Width, 100, WA_Height, 60,
                           WA_Borderless, TRUE, WA_Activate, FALSE, TAG_DONE);
        P_NULL("win", w);
        if (w) {
            pixtests("window px", w->RPort, s->RastPort.BitMap, 10, 20);
            /* clipped at window edge */
            p96RectFill(w->RPort, 95, 0, 120, 3, 0x00ffffff);
            P_PIX("clip in", s->RastPort.BitMap, 10 + 99, 21);
            P_PIX("clip out", s->RastPort.BitMap, 10 + 100, 21);
            CloseWindow(w);
        }
        /* WritePixelArray8 array clearing */
        {
            UBYTE arr[32]; struct RastPort tmp; struct BitMap *tbm = AllocBitMap(16, 1, 8, BMF_CLEAR, NULL);
            int i;
            for (i = 0; i < 32; i++) arr[i] = i;
            InitRastPort(&tmp); tmp.BitMap = tbm;
            P_LONG("wpa8", WritePixelArray8(&s->RastPort, 0, 0, 15, 0, arr, &tmp));
            P_BYTES("wpa8 arr after", arr, 16);
            FreeBitMap(tbm);
        }
        /* PIP */
        {
            struct Window *pw = p96PIP_OpenTagList((struct TagItem *)0);
            P_NULL("pip null tags", pw);
            if (pw) p96PIP_Close(pw);
        }
        CloseScreen(s);
    }
    /* p96OpenScreenTagList */
    {
        struct TagItem t[] = {{P96SA_Width, 640}, {P96SA_Height, 480}, {P96SA_Depth, 16}, {P96SA_Quiet, TRUE},
                              {P96SA_Behind, TRUE}, {P96SA_ShowTitle, FALSE}, {TAG_DONE, 0}};
        struct Screen *ps = p96OpenScreenTagList(t);
        P_SECTION("p96screen");
        P_NULL("ps", ps);
        if (ps) {
            P_HEX("id", GetVPModeID(&ps->ViewPort));
            P_LONG("w", ps->Width); P_LONG("h", ps->Height);
            P_HEX("vp.Modes", ps->ViewPort.Modes);
            P_LONG("vp.DHeight", ps->ViewPort.DHeight);
            P_HEX("Flags", ps->Flags);
            P_LONG("fmt", p96GetBitMapAttr(ps->RastPort.BitMap, P96BMA_RGBFORMAT));
            P_LONG("depth", p96GetBitMapAttr(ps->RastPort.BitMap, P96BMA_DEPTH));
            {
                struct TagItem pt[] = {{P96PIP_SourceFormat, RGBFB_R5G6B5PC}, {P96PIP_SourceWidth, 64}, {P96PIP_SourceHeight, 48},
                                       {WA_Left, 10}, {WA_Top, 10}, {WA_Width, 100}, {WA_Height, 80}, {WA_CustomScreen, (ULONG)ps}, {TAG_DONE, 0}};
                struct Window *pw = p96PIP_OpenTagList(pt);
                P_NULL("pip", pw);
                if (pw) {
                    struct BitMap *pbm = 0; struct RastPort *prp = 0; LONG err = -1;
                    struct TagItem gt[] = {{P96PIP_SourceBitMap, (ULONG)&pbm}, {P96PIP_SourceRPort, (ULONG)&prp}, {P96PIP_ErrorCode, (ULONG)&err}, {TAG_DONE, 0}};
                    P_LONG("pipget", p96PIP_GetTagList(pw, gt));
                    P_NULL("pipbm", pbm);
                    P_NULL("piprp", prp);
                    if (pbm) { P_LONG("pip fmt", p96GetBitMapAttr(pbm, P96BMA_RGBFORMAT)); P_LONG("pip w", p96GetBitMapAttr(pbm, P96BMA_WIDTH)); }
                    P_LONG("pip close", p96PIP_Close(pw));
                }
            }
            P_LONG("close", p96CloseScreen(ps));
        }
    }
    {
        struct TagItem t[] = {{P96SA_Width, 320}, {P96SA_Height, 240}, {P96SA_RGBFormat, RGBFB_B8G8R8A8}, {P96SA_Quiet, TRUE},
                              {P96SA_Behind, TRUE}, {TAG_DONE, 0}};
        struct Screen *ps = p96OpenScreenTagList(t);
        P_SECTION("p96screen fmt");
        P_NULL("ps", ps);
        if (ps) {
            P_HEX("id", GetVPModeID(&ps->ViewPort));
            P_LONG("w", ps->Width); P_LONG("h", ps->Height);
            P_LONG("fmt", p96GetBitMapAttr(ps->RastPort.BitMap, P96BMA_RGBFORMAT));
            P_LONG("depth", ps->RastPort.BitMap->Depth);
            P_LONG("close", p96CloseScreen(ps));
        }
    }
    {
        LONG err = 12345;
        struct TagItem t[] = {{P96SA_DisplayID, 0x12345678}, {P96SA_ErrorCode, (ULONG)&err}, {TAG_DONE, 0}};
        struct Screen *ps = p96OpenScreenTagList(t);
        P_SECTION("p96screen bad");
        P_NULL("ps", ps);
        P_LONG("err", err);
        if (ps) p96CloseScreen(ps);
    }
    CloseLibrary(P96Base);
    return 0;
}
