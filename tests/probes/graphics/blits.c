/*
 * Probe (Phase 222b): graphics.library blits on off-screen bitmaps -
 * BltBitMap (minterms, plane masks, overlap, shifts), BltClear,
 * BltTemplate, BltPattern, BltMaskBitMapRastPort, BltBitMapRastPort,
 * ClipBlit, BitMapScale and the chunky pixel functions.  Compared as
 * per-plane bitmap hashes; run with a section name for hex dumps.
 */
#include <graphics/scale.h>
#include "gfxprobe.h"

#define W 64
#define H 32

static struct gp_bm src, dst;
static struct RastPort srp, drp;

/* a recognisable source image: diagonal stripes per plane */
static void fill_src(void)
{
    WORD x, y;
    gp_clear(&src);
    gp_rp(&srp, &src);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            UBYTE pen = (UBYTE)(((x + y) >> 2) & 7);
            if ((x * 3 + y) % 11 == 0)
                pen ^= 5;
            SetAPen(&srp, pen);
            WritePixel(&srp, x, y);
        }
}

static void fill_dst(void)
{
    WORD y;
    gp_clear(&dst);
    gp_rp(&drp, &dst);
    for (y = 0; y < H; y++) {
        SetAPen(&drp, (UBYTE)((y / 3) & 7));
        Move(&drp, 0, y);
        Draw(&drp, W - 1, y);
    }
}

int main(int argc, char **argv)
{
    static const UBYTE minterms[] = {0x00, 0x0c, 0x30, 0x3c, 0x50, 0x5a, 0x60, 0x66,
                                     0x80, 0x88, 0xa0, 0xaa, 0xc0, 0xcc, 0xee, 0xf0, 0xff};
    LONG rc;
    WORD i;

    gp_args(argc, argv);
    if (!gp_alloc(&src, W, H, 3) || !gp_alloc(&dst, W, H, 3))
        return 20;
    fill_src();
    gp_hash("source", &src);
    fill_dst();
    gp_hash("destination", &dst);

    gp_section("BltBitMap minterms");
    for (i = 0; i < (WORD)sizeof(minterms); i++) {
        fill_dst();
        rc = BltBitMap(&src.bm, 3, 2, &dst.bm, 9, 5, 40, 20, minterms[i], 0xff, NULL);
        probe_s("minterm ");
        probe_hex(minterms[i], 2);
        probe_s(" rc ");
        probe_dec(rc);
        gp_hash("", &dst);
    }

    gp_section("BltBitMap masks and shifts");
    fill_dst();
    rc = BltBitMap(&src.bm, 0, 0, &dst.bm, 0, 0, W, H, 0xc0, 0x05, NULL);
    P_LONG("mask 5 rc", rc);
    gp_hash("mask 5", &dst);
    fill_dst();
    rc = BltBitMap(&src.bm, 0, 0, &dst.bm, 0, 0, W, H, 0xc0, 0x00, NULL);
    P_LONG("mask 0 rc", rc);
    gp_hash("mask 0", &dst);
    {
        static const WORD sh[][6] = {
            {0, 0, 1, 0, 30, 10}, {1, 0, 0, 0, 30, 10}, {7, 3, 13, 9, 33, 17}, {15, 1, 16, 2, 17, 5},
            {16, 0, 31, 0, 1, 32}, {5, 5, 50, 20, 14, 12}, {63, 31, 0, 0, 1, 1}, {2, 0, 2, 0, 60, 1},
        };
        for (i = 0; i < (WORD)(sizeof(sh) / sizeof(sh[0])); i++) {
            fill_dst();
            rc = BltBitMap(&src.bm, sh[i][0], sh[i][1], &dst.bm, sh[i][2], sh[i][3], sh[i][4], sh[i][5],
                           0xc0, 0xff, NULL);
            probe_s("copy ");
            probe_dec(i);
            probe_s(" rc ");
            probe_dec(rc);
            gp_hash("", &dst);
        }
    }

    gp_section("BltBitMap overlapping");
    {
        static const WORD ov[][4] = {{0, 0, 3, 2}, {3, 2, 0, 0}, {10, 10, 9, 10}, {9, 10, 10, 10},
                                     {8, 4, 8, 6}, {8, 6, 8, 4}, {0, 0, 17, 0}, {17, 0, 0, 0}};
        for (i = 0; i < (WORD)(sizeof(ov) / sizeof(ov[0])); i++) {
            fill_src();
            BltBitMap(&src.bm, ov[i][0], ov[i][1], &src.bm, ov[i][2], ov[i][3], 40, 20, 0xc0, 0xff, NULL);
            probe_s("overlap ");
            probe_dec(i);
            gp_hash("", &src);
        }
        fill_src();
        BltBitMap(&src.bm, 4, 4, &src.bm, 6, 5, 30, 20, 0x60, 0xff, NULL);
        gp_hash("overlap xor", &src);
        fill_src();
    }

    gp_section("BltClear");
    {
        UBYTE *buf = AllocMem(256, MEMF_CHIP);
        if (buf) {
            WORD k;
            for (k = 0; k < 256; k++)
                buf[k] = 0xa5;
            BltClear(buf, 100, 0);
            P_BYTES("bytes 96..103", buf + 96, 8);
            for (k = 0; k < 256; k++)
                buf[k] = 0xa5;
            BltClear(buf, (8 << 16) | 10, 2);
            P_BYTES("rows 10x8: 76..83", buf + 76, 8);
            P_BYTES("rows 10x8: 0..3", buf, 4);
            FreeMem(buf, 256);
        }
    }

    gp_section("BltTemplate");
    {
        static UWORD tmpl[8 * 2] = {
            0xf00f, 0x8001, 0x0ff0, 0x4002, 0x3c3c, 0x2004, 0xc3c3, 0x1008,
            0xaaaa, 0x0810, 0x5555, 0x0420, 0xffff, 0x0240, 0x0000, 0x0180,
        };
        static const UBYTE dm[] = {JAM1, JAM2, COMPLEMENT, JAM1 | INVERSVID, JAM2 | INVERSVID};
        PLANEPTR ct = gp_chip(tmpl, sizeof(tmpl));
        for (i = 0; ct && i < 5; i++) {
            fill_dst();
            SetAPen(&drp, 5);
            SetBPen(&drp, 2);
            SetDrMd(&drp, dm[i]);
            BltTemplate(ct, 3, 4, &drp, 7, 3, 25, 8);
            BltTemplate(ct, 0, 4, &drp, 30, 20, 32, 8);
            probe_s("template dm ");
            probe_dec(dm[i]);
            gp_hash("", &dst);
        }
        SetDrMd(&drp, JAM1);
    }

    gp_section("BltPattern");
    {
        static UWORD mask[8] = {0xffff, 0x7ffe, 0x3ffc, 0x1ff8, 0x0ff0, 0x07e0, 0x03c0, 0x0180};
        static UWORD pat[4] = {0xf0f0, 0x0f0f, 0xcccc, 0x3333};
        PLANEPTR cm = gp_chip(mask, sizeof(mask));
        UWORD *cp = gp_chip(pat, sizeof(pat));
        fill_dst();
        SetAPen(&drp, 6);
        SetBPen(&drp, 1);
        SetDrMd(&drp, JAM1);
        BltPattern(&drp, NULL, 3, 3, 20, 9, 0);
        BltPattern(&drp, cm, 30, 2, 45, 9, 2);
        SetDrMd(&drp, JAM2);
        BltPattern(&drp, cm, 30, 12, 45, 19, 2);
        SetAfPt(&drp, cp, 2);
        BltPattern(&drp, NULL, 3, 12, 20, 25, 0);
        BltPattern(&drp, cm, 47, 22, 62, 29, 2);
        SetDrMd(&drp, COMPLEMENT);
        BltPattern(&drp, cm, 22, 22, 37, 29, 2);
        SetAfPt(&drp, NULL, 0);
        SetDrMd(&drp, JAM1);
        gp_hash("bltpattern", &dst);
    }

    gp_section("BltBitMapRastPort/ClipBlit/BltMaskBitMapRastPort");
    {
        UWORD *bmask = gp_chip(NULL, H * 8);
        WORD k;
        for (k = 0; bmask && k < H; k++) {
            bmask[k * 4 + 0] = (UWORD)(0xffff >> (k & 15));
            bmask[k * 4 + 1] = (UWORD)(0xffff << (k & 15));
            bmask[k * 4 + 2] = 0xaaaa;
            bmask[k * 4 + 3] = 0x0f0f;
        }
        for (i = 0; i < 5; i++) {
            static const UBYTE mt[] = {0xc0, 0x30, 0x60, 0x80, 0xee};
            fill_dst();
            BltBitMapRastPort(&src.bm, 5, 3, &drp, 2, 7, 37, 14, mt[i]);
            probe_s("BltBitMapRastPort ");
            probe_hex(mt[i], 2);
            gp_hash("", &dst);
        }
        fill_dst();
        ClipBlit(&srp, 1, 1, &drp, 20, 10, 30, 15, 0xc0);
        gp_hash("ClipBlit", &dst);
        fill_dst();
        BltMaskBitMapRastPort(&src.bm, 2, 3, &drp, 5, 6, 50, 16, ABC | ABNC | ANBC, (PLANEPTR)bmask);
        gp_hash("BltMaskBitMapRastPort cookie", &dst);
        fill_dst();
        BltMaskBitMapRastPort(&src.bm, 2, 3, &drp, 5, 6, 50, 16, ANBC | ANBNC | ABC | ABNC, (PLANEPTR)bmask);
        gp_hash("BltMaskBitMapRastPort 0xe2", &dst);
        fill_dst();
        BltMaskBitMapRastPort(&src.bm, 0, 0, &drp, 7, 1, 40, 16, 0x60, (PLANEPTR)bmask);
        gp_hash("BltMaskBitMapRastPort 0x60", &dst);
    }

    gp_section("BitMapScale");
    {
        struct BitScaleArgs bsa;
        static const UWORD f[][4] = {{1, 1, 1, 1}, {2, 1, 1, 1}, {1, 2, 1, 2}, {3, 2, 2, 3}, {5, 7, 3, 4}};
        for (i = 0; i < 5; i++) {
            fill_dst();
            {
                UBYTE *b = (UBYTE *)&bsa;
                WORD k;
                for (k = 0; k < (WORD)sizeof(bsa); k++)
                    b[k] = 0;
            }
            /* 1:1 from an unaligned SrcX copies garbage on 3.1: use 16 */
            bsa.bsa_SrcX = (f[i][0] == f[i][1] && f[i][2] == f[i][3]) ? 16 : 3;
            bsa.bsa_SrcY = 2;
            bsa.bsa_SrcWidth = 20;
            bsa.bsa_SrcHeight = 12;
            bsa.bsa_XSrcFactor = f[i][0];
            bsa.bsa_YSrcFactor = f[i][2];
            bsa.bsa_DestX = 1;
            bsa.bsa_DestY = 1;
            bsa.bsa_DestWidth = 0;
            bsa.bsa_DestHeight = 0;
            bsa.bsa_XDestFactor = f[i][1];
            bsa.bsa_YDestFactor = f[i][3];
            bsa.bsa_SrcBitMap = &src.bm;
            bsa.bsa_DestBitMap = &dst.bm;
            bsa.bsa_Flags = 0;
            BitMapScale(&bsa);
            probe_s("scale ");
            probe_dec(i);
            probe_s(" dest ");
            probe_dec(bsa.bsa_DestWidth);
            probe_ch('x');
            probe_dec(bsa.bsa_DestHeight);
            gp_hash("", &dst);
        }
        P_LONG("ScalerDiv(100, 3, 2)", ScalerDiv(100, 3, 2));
        P_LONG("ScalerDiv(7, 2, 3)", ScalerDiv(7, 2, 3));
        P_LONG("ScalerDiv(1, 1, 16383)", ScalerDiv(1, 1, 16383));
    }

    /* rows of the chunky arrays are padded to 16 pixels; 3.1 reads and
     * writes the padding (with stale temprp data), so only the requested
     * pixels are compared, and WritePixelArray8() destroys its array */
    gp_section("chunky pixels");
    {
        static UBYTE line[80];
        static UBYTE arr[32 * 6];
        struct gp_bm tmp;
        struct RastPort trp;
        WORD k;

        if (gp_alloc(&tmp, 80, 1, 3)) {
            gp_rp(&trp, &tmp);
            trp.Layer = NULL;
            for (k = 0; k < 80; k++)
                line[k] = (UBYTE)((k * 5) & 7);
            fill_dst();
            P_LONG("WritePixelLine8 rc", WritePixelLine8(&drp, 3, 4, 50, line, &trp));
            gp_hash("WritePixelLine8", &dst);
            for (k = 0; k < 80; k++)
                line[k] = 0xee;
            P_LONG("ReadPixelLine8 rc", ReadPixelLine8(&drp, 1, 4, 60, line, &trp));
            P_BYTES("ReadPixelLine8", line, 60);
            for (k = 0; k < 32 * 6; k++)
                arr[k] = (UBYTE)((k / 3) & 7);
            fill_dst();
            P_LONG("WritePixelArray8 rc", WritePixelArray8(&drp, 9, 20, 9 + 21, 20 + 5, arr, &trp));
            gp_hash("WritePixelArray8", &dst);
            for (k = 0; k < 32 * 6; k++)
                arr[k] = 0xee;
            P_LONG("ReadPixelArray8 rc", ReadPixelArray8(&drp, 8, 19, 8 + 17, 19 + 2, arr, &trp));
            P_BYTES("ReadPixelArray8 row 0", arr, 18);
            P_BYTES("ReadPixelArray8 row 1", arr + 32, 18);
            P_BYTES("ReadPixelArray8 row 2", arr + 64, 18);
            for (k = 0; k < 32 * 6; k++)
                arr[k] = (UBYTE)((k * 7 / 5) & 7);
            fill_dst();
            WriteChunkyPixels(&drp, 2, 2, 13, 4, arr, 18);
            WriteChunkyPixels(&drp, 30, 10, 30, 12, arr + 5, 1);
            gp_hash("WriteChunkyPixels", &dst);
            gp_free(&tmp);
        }
    }
    gp_chip_free();
    gp_free(&src);
    gp_free(&dst);
    return 0;
}
