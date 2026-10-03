/*
 * Probe (Phase 222b): graphics.library area fills (AreaMove/AreaDraw/
 * AreaEllipse/AreaEnd), outline mode, area patterns and Flood() on an
 * off-screen bitmap (no layer).  Compared as per-plane bitmap hashes; run
 * with an argument for hex dumps.
 */
#include <graphics/gfxmacros.h>
#include "gfxprobe.h"

#define W 64
#define H 48
#define MAXVEC 20

static struct gp_bm g;
static struct RastPort rp;
static struct AreaInfo ai;
static struct TmpRas tr;
static WORD areabuf[MAXVEC * 5 / 2 + 2];
static PLANEPTR tmpras;

static void reset(void)
{
    gp_clear(&g);
    gp_rp(&rp, &g);
    InitArea(&ai, areabuf, MAXVEC);
    rp.AreaInfo = &ai;
    rp.TmpRas = InitTmpRas(&tr, tmpras, RASSIZE(W, H));
}

static void poly(const WORD *v, WORD n)
{
    WORD i;
    AreaMove(&rp, v[0], v[1]);
    for (i = 1; i < n; i++)
        AreaDraw(&rp, v[2 * i], v[2 * i + 1]);
}

static const WORD tri[] = {5, 5, 58, 12, 20, 44};
static const WORD star[] = {32, 2, 40, 42, 4, 16, 60, 16, 24, 42};
static const WORD concave[] = {2, 2, 61, 2, 61, 45, 32, 20, 2, 45};
static const WORD thin[] = {10, 10, 50, 11, 10, 12};

int main(int argc, char **argv)
{
    LONG rc;
    WORD i;

    gp_args(argc, argv);
    if (!gp_alloc(&g, W, H, 3))
        return 20;
    tmpras = AllocRaster(W, H);
    if (!tmpras)
        return 20;

    gp_section("InitArea");
    reset();
    P_LONG("MaxCount", ai.MaxCount);
    P_LONG("Count", ai.Count);
    P_BOOL("VctrTbl == buffer", ai.VctrTbl == areabuf);
    P_BOOL("FlagPtr after vectors", ai.FlagTbl == (BYTE *)(areabuf + MAXVEC * 2));
    P_BOOL("TmpRas RasPtr", tr.RasPtr == (BYTE *)tmpras);
    P_LONG("TmpRas Size", tr.Size);

    gp_section("polygons");
    reset();
    SetAPen(&rp, 3);
    poly(tri, 3);
    P_LONG("Count after 3 vertices", ai.Count);
    rc = AreaEnd(&rp);
    P_LONG("AreaEnd rc", rc);
    P_LONG("Count after AreaEnd", ai.Count);
    P_LONG("cp_x", rp.cp_x);
    P_LONG("cp_y", rp.cp_y);
    gp_hash("triangle", &g);
    reset();
    SetAPen(&rp, 5);
    poly(star, 5);
    AreaEnd(&rp);
    gp_hash("star", &g);
    reset();
    SetAPen(&rp, 6);
    poly(concave, 5);
    AreaEnd(&rp);
    gp_hash("concave", &g);
    reset();
    SetAPen(&rp, 7);
    poly(thin, 3);
    AreaEnd(&rp);
    gp_hash("thin", &g);
    reset();
    SetAPen(&rp, 1);
    poly(tri, 3);
    AreaMove(&rp, 40, 30);
    AreaDraw(&rp, 62, 30);
    AreaDraw(&rp, 62, 46);
    AreaDraw(&rp, 40, 46);
    P_LONG("Count two polygons", ai.Count);
    AreaEnd(&rp);
    gp_hash("two polygons", &g);

    gp_section("outline");
    reset();
    SetAPen(&rp, 2);
    SetOPen(&rp, 5);
    P_HEX("Flags after SetOPen", rp.Flags);
    P_LONG("AOlPen", rp.AOlPen);
    poly(star, 5);
    AreaEnd(&rp);
    gp_hash("star outlined", &g);
    reset();
    SetAPen(&rp, 2);
    SetOPen(&rp, 4);
    poly(tri, 3);
    AreaEllipse(&rp, 44, 30, 15, 10);
    AreaEnd(&rp);
    BNDRYOFF(&rp);
    P_HEX("Flags after BNDRYOFF", rp.Flags);
    gp_hash("triangle+ellipse outlined", &g);

    gp_section("AreaEllipse");
    {
        static const WORD el[][4] = {
            {32, 24, 30, 20}, {32, 24, 10, 10}, {10, 10, 5, 3}, {50, 10, 3, 8}, {20, 38, 1, 1}, {45, 38, 12, 2},
        };
        for (i = 0; i < (WORD)(sizeof(el) / sizeof(el[0])); i++) {
            reset();
            SetAPen(&rp, 1 + i);
            rc = AreaEllipse(&rp, el[i][0], el[i][1], el[i][2], el[i][3]);
            probe_s("AreaEllipse ");
            probe_dec(i);
            probe_s(": rc ");
            probe_dec(rc);
            probe_s(", Count ");
            probe_dec(ai.Count);
            probe_ch('\n');
            AreaEnd(&rp);
            gp_hash("  ellipse", &g);
        }
    }

    gp_section("draw modes and patterns");
    {
        static UWORD pat[4] = {0xcccc, 0x3333, 0xf0f0, 0x0f0f};
        reset();
        SetRast(&rp, 1);
        SetAPen(&rp, 6);
        SetBPen(&rp, 3);
        SetDrMd(&rp, COMPLEMENT);
        poly(concave, 5);
        AreaEnd(&rp);
        gp_hash("complement", &g);
        reset();
        SetRast(&rp, 1);
        SetAPen(&rp, 6);
        SetBPen(&rp, 3);
        SetDrMd(&rp, JAM1);
        SetAfPt(&rp, pat, 2);
        poly(star, 5);
        AreaEnd(&rp);
        gp_hash("pattern JAM1", &g);
        reset();
        SetRast(&rp, 1);
        SetAPen(&rp, 6);
        SetBPen(&rp, 3);
        SetDrMd(&rp, JAM2);
        SetAfPt(&rp, pat, 2);
        poly(star, 5);
        AreaEnd(&rp);
        gp_hash("pattern JAM2", &g);
        reset();
        SetRast(&rp, 1);
        SetAPen(&rp, 6);
        SetBPen(&rp, 3);
        SetDrMd(&rp, JAM2 | INVERSVID);
        SetAfPt(&rp, pat, 2);
        SetOPen(&rp, 2);
        AreaEllipse(&rp, 32, 24, 25, 18);
        AreaEnd(&rp);
        gp_hash("pattern JAM2|INVERSVID outlined", &g);
        SetAfPt(&rp, NULL, 0);
        BNDRYOFF(&rp);
        SetDrMd(&rp, JAM1);
    }

    gp_section("vector table limits");
    reset();
    SetAPen(&rp, 4);
    AreaMove(&rp, 1, 1);
    for (i = 0; i < MAXVEC + 3; i++) {
        rc = AreaDraw(&rp, 2 + (i * 3) % 60, 2 + (i * 7) % 44);
        if (rc) {
            probe_s("AreaDraw ");
            probe_dec(i);
            probe_s(" rc = ");
            probe_dec(rc);
            probe_ch('\n');
        }
    }
    P_LONG("Count", ai.Count);
    rc = AreaEnd(&rp);
    P_LONG("AreaEnd rc", rc);
    gp_hash("overflow", &g);
    reset();
    SetAPen(&rp, 4);
    /* (AreaEnd() on an empty vector table hangs AmigaOS 3.1) */
    AreaMove(&rp, 10, 10);
    AreaDraw(&rp, 30, 10);
    P_LONG("AreaEnd line rc", AreaEnd(&rp));
    gp_hash("degenerate", &g);

    gp_section("Flood");
    reset();
    SetAPen(&rp, 2);
    Move(&rp, 4, 4);
    Draw(&rp, 59, 4);
    Draw(&rp, 59, 43);
    Draw(&rp, 4, 43);
    Draw(&rp, 4, 4);
    Move(&rp, 20, 4);
    Draw(&rp, 40, 30);
    Draw(&rp, 20, 43);
    SetAPen(&rp, 3);
    RectFill(&rp, 45, 10, 52, 20);
    SetAPen(&rp, 6);
    SetOPen(&rp, 2);
    BNDRYOFF(&rp);
    P_BOOL("Flood outline rc", Flood(&rp, 0, 50, 30));
    gp_hash("flood outline mode", &g);
    SetAPen(&rp, 5);
    P_BOOL("Flood colour rc", Flood(&rp, 1, 10, 10));
    gp_hash("flood colour mode", &g);
    SetAPen(&rp, 1);
    P_BOOL("Flood colour outside", Flood(&rp, 1, 0, 0));
    gp_hash("flood outside", &g);
    reset();
    {
        static UWORD pat[2] = {0xaaaa, 0x5555};
        SetAPen(&rp, 7);
        DrawEllipse(&rp, 32, 24, 20, 15);
        SetAPen(&rp, 4);
        SetBPen(&rp, 1);
        SetDrMd(&rp, JAM2);
        SetAfPt(&rp, pat, 1);
        SetOPen(&rp, 7);
        BNDRYOFF(&rp);
        Flood(&rp, 0, 32, 24);
        SetAfPt(&rp, NULL, 0);
        SetDrMd(&rp, JAM1);
    }
    gp_hash("flood pattern", &g);

    /* a maze with many branches (deep span stack) */
    reset();
    SetAPen(&rp, 3);
    for (i = 1; i < W; i += 2) {
        WORD y;
        for (y = 0; y < H; y++)
            if (y % 4 != (i % 4 == 1 ? 0 : 2) && !(y % 6 == 5 && i % 6 == 3))
                WritePixel(&rp, i, y);
    }
    SetAPen(&rp, 5);
    P_BOOL("Flood maze rc", Flood(&rp, 1, 0, 0));
    gp_hash("flood maze", &g);
    SetAPen(&rp, 6);
    SetOPen(&rp, 3);
    BNDRYOFF(&rp);
    P_BOOL("Flood maze outline rc", Flood(&rp, 0, 2, 47));
    gp_hash("flood maze outline", &g);

    FreeRaster(tmpras, W, H);
    gp_free(&g);
    return 0;
}
