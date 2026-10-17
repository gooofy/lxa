/*
 * Probe (Phase 242): Picasso96API.library pixel functions on offscreen
 * bitmaps of every board format (Picasso96 2.0, rtg profile) - p96LockBitMap,
 * p96Write/ReadPixelArray format conversions, p96WritePixel, p96ReadPixel,
 * p96RectFill, p96Write/ReadTrueColorData, a planar bitmap.
 * Left out (not reproducible): RenderInfo.pad after p96LockBitMap, the colour
 * of unwritten BMF_CLEAR pixels and of CLUT arrays on true colour bitmaps,
 * p96ReadPixelArray from CLUT bitmaps and from R5G6B5PC into B8G8R8A8.
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
            if (fmt == RGBFB_CLUT || (fmt == RGBFB_R5G6B5PC && f[k] == RGBFB_B8G8R8A8)) continue;
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
        for (i = 0; fmt == RGBFB_CLUT && i < 4; i++) { probe_s("wpa clut "); probe_dec(i); probe_s(" = "); probe_hex(p96ReadPixel(rp, 2 + i, 4), 8); probe_s(" raw "); probe_hex(pix(bm, ox + 2 + i, oy + 4), 8); probe_ch('\n'); }
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
    P96Base = OpenLibrary(P96NAME, 2);
    if (!P96Base) return 0;

    /* offscreen bitmaps */
    {
        static const ULONG fmts[] = {RGBFB_B8G8R8A8, RGBFB_R5G6B5PC, RGBFB_CLUT, RGBFB_R8G8B8};
        int k;
        for (k = 0; k < 4; k++) {
            struct BitMap *bm = p96AllocBitMap(40, 30, fmts[k] == RGBFB_CLUT ? 8 : 24, BMF_CLEAR, NULL, fmts[k]);
            struct RastPort rp;
            LONG lk;
            UBYTE rib[64];
            struct RenderInfo *ri = (struct RenderInfo *)rib;
            P_SECTION("offscreen");
            P_NULL("bm", bm);
            if (!bm) continue;
            memset(rib, 0xee, sizeof(rib));
            lk = p96LockBitMap(bm, rib, sizeof(struct RenderInfo));
            P_BOOL("lock", lk != 0);
            P_BOOL("ri.mem ok", ri->Memory == (APTR)p96GetBitMapAttr(bm, P96BMA_MEMORY));
            P_LONG("ri.bpr", ri->BytesPerRow);
            P_LONG("ri.fmt", ri->RGBFormat);
            P_HEX("rib12", rib[12]);
            {
                LONG lk2 = p96LockBitMap(bm, rib, sizeof(struct RenderInfo));
                P_BOOL("lock2", lk2 != 0);
                P_BOOL("lock2 same", lk2 == lk);
                if (lk2) p96UnlockBitMap(bm, lk2);
            }
            if (lk) p96UnlockBitMap(bm, lk);
            lk = p96LockBitMap(bm, rib, 4);
            P_LONG("lock small", lk != 0);
            if (lk) p96UnlockBitMap(bm, lk);
            InitRastPort(&rp);
            rp.BitMap = bm;
            pixtests("offscreen px", &rp, bm, 0, 0);
            p96FreeBitMap(bm);
        }
    }
    /* planar bitmap with p96 functions */
    {
        struct BitMap *bm = AllocBitMap(32, 8, 2, BMF_CLEAR, NULL);
        struct RastPort rp;
        UBYTE rib[16];
        P_SECTION("planar");
        InitRastPort(&rp); rp.BitMap = bm;
        P_LONG("planar lock", p96LockBitMap(bm, rib, sizeof(struct RenderInfo)) != 0);
        P_HEX("planar rp96", p96ReadPixel(&rp, 1, 1));
        FreeBitMap(bm);
    }

    CloseLibrary(P96Base);
    return 0;
}
