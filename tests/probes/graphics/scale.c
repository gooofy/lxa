/*
 * Probe (Phase 222b remainder): what BitMapScale() writes back into its
 * BitScaleArgs and what WritePixelArray8()/WritePixelLine8() leave in the
 * caller's chunky array, next to the pixels they produce.
 *
 * Not probed, because AmigaOS 3.1 is not deterministic there: scaling with
 * equal X factors (1:1, 2:2) - the destination gets a filled rectangle one
 * pixel narrower, or nothing, from run to run - and bsa_XDDA/bsa_YDDA,
 * which 3.1 leaves at values of its own DDA (e.g. X 1:2 -> -3, 2:1 -> -1,
 * 3:5 -> -5, 5:3 -> -3; Y 3:1 -> -5, 7:4 -> -8, 1:2 -> 66 or 67 depending
 * on the height, 1:3 -> 130) that lxa does not reproduce (Phase 222b).
 */
#include <exec/memory.h>
#include <graphics/scale.h>
#include "gfxprobe.h"

#define W 48
#define H 20

static struct gp_bm src, dst;

static void p_bsa(struct BitScaleArgs *a)
{
    P_HEX("Src X/Y", ((ULONG)a->bsa_SrcX << 16) | a->bsa_SrcY);
    P_HEX("Src W/H", ((ULONG)a->bsa_SrcWidth << 16) | a->bsa_SrcHeight);
    P_HEX("SrcFactor X/Y", ((ULONG)a->bsa_XSrcFactor << 16) | a->bsa_YSrcFactor);
    P_HEX("Dest X/Y", ((ULONG)a->bsa_DestX << 16) | a->bsa_DestY);
    P_HEX("Dest W/H", ((ULONG)a->bsa_DestWidth << 16) | a->bsa_DestHeight);
    P_HEX("DestFactor X/Y", ((ULONG)a->bsa_XDestFactor << 16) | a->bsa_YDestFactor);
    P_HEX("Flags", a->bsa_Flags);
    P_HEX("Reserved1", a->bsa_Reserved1);
    P_HEX("Reserved2", a->bsa_Reserved2);
}

static void scale(const char *name, UWORD sw, UWORD sh, UWORD xs, UWORD xd, UWORD ys, UWORD yd)
{
    struct BitScaleArgs a;
    UBYTE *p = (UBYTE *)&a;
    LONG i;

    gp_section(name);
    for (i = 0; i < (LONG)sizeof(a); i++)
        p[i] = 0;
    gp_clear(&dst);
    a.bsa_SrcX = 3;
    a.bsa_SrcY = 2;
    a.bsa_SrcWidth = sw;
    a.bsa_SrcHeight = sh;
    a.bsa_XSrcFactor = xs;
    a.bsa_YSrcFactor = ys;
    a.bsa_DestX = 1;
    a.bsa_DestY = 1;
    a.bsa_XDestFactor = xd;
    a.bsa_YDestFactor = yd;
    a.bsa_SrcBitMap = &src.bm;
    a.bsa_DestBitMap = &dst.bm;
    a.bsa_XDDA = 0x1111;
    a.bsa_YDDA = 0x2222;
    a.bsa_Reserved1 = 0x33333333;
    a.bsa_Reserved2 = 0x44444444;
    BitMapScale(&a);
    WaitBlit();
    p_bsa(&a);
    gp_hash("dest", &dst);
}

static void pixel8(void)
{
    struct RastPort rp, temprp;
    struct BitMap tmpbm;
    UBYTE *arr;
    LONG i, r;
    WORD width = 21, stride = ((21 + 15) >> 4) << 4;

    arr = AllocMem(stride * 4, MEMF_PUBLIC | MEMF_CLEAR);
    if (!arr)
        return;
    gp_clear(&dst);
    gp_rp(&rp, &dst);
    temprp = rp;
    temprp.Layer = NULL;
    InitBitMap(&tmpbm, dst.bm.Depth, width, 1);
    tmpbm.BytesPerRow = ((width + 15) >> 4) << 1;
    for (i = 0; i < dst.bm.Depth; i++)
        tmpbm.Planes[i] = AllocRaster(width, 1);
    temprp.BitMap = &tmpbm;

    gp_section("WritePixelArray8");
    for (i = 0; i < stride * 4; i++)
        arr[i] = (UBYTE)((i * 5 + 1) & 3);
    r = WritePixelArray8(&rp, 2, 3, 2 + width - 1, 3 + 3, arr, &temprp);
    WaitBlit();
    P_LONG("result", r);
    P_BYTES("array row 0", arr, stride);
    P_BYTES("array row 3", arr + 3 * stride, stride);
    P_HEX("array hash", probe_hash(arr, stride * 4));
    gp_row("y3", &rp, 3, 0, 24);
    gp_row("y6", &rp, 6, 0, 24);
    gp_hash("dest", &dst);

    gp_section("WritePixelLine8");
    for (i = 0; i < stride; i++)
        arr[i] = (UBYTE)((i * 3 + 2) & 3);
    r = WritePixelLine8(&rp, 1, 9, width, arr, &temprp);
    WaitBlit();
    P_LONG("result", r);
    P_BYTES("array", arr, stride);
    gp_row("y9", &rp, 9, 0, 24);

    gp_section("ReadPixelArray8");
    for (i = 0; i < stride * 4; i++)
        arr[i] = 0xee;
    r = ReadPixelArray8(&rp, 2, 3, 2 + width - 1, 3 + 3, arr, &temprp);
    P_LONG("result", r);
    P_BYTES("array row 0", arr, stride);
    P_HEX("array hash", probe_hash(arr, stride * 4));

    for (i = 0; i < dst.bm.Depth; i++)
        FreeRaster(tmpbm.Planes[i], width, 1);
    FreeMem(arr, stride * 4);
}

int main(int argc, char **argv)
{
    LONG i, k;

    gp_args(argc, argv);
    if (!gp_alloc(&src, W, H, 2) || !gp_alloc(&dst, W, H, 2))
        return 20;
    for (k = 0; k < 2; k++)
        for (i = 0; i < (LONG)src.bm.BytesPerRow * H; i++)
            src.bm.Planes[k][i] = (UBYTE)(k ? 0x33 ^ (i * 7) : 0x5a + i * 13);

    scale("enlarge 2x 3x", 10, 5, 1, 2, 1, 3);
    scale("shrink 1/2 1/3", 30, 15, 2, 1, 3, 1);
    scale("odd 3:5 7:4", 13, 9, 3, 5, 7, 4);
    scale("odd 5:3 4:7", 25, 10, 5, 3, 4, 7);
    scale("large factors 300:301", 20, 10, 300, 301, 301, 300);
    scale("x 2:1 y 1:1", 16, 8, 2, 1, 1, 1);
    scale("enlarge 1:2 again", 10, 5, 1, 2, 1, 2);
    scale("enlarge 1:2 wider", 20, 9, 1, 2, 1, 2);

    pixel8();

    gp_free(&src);
    gp_free(&dst);
    return 0;
}
