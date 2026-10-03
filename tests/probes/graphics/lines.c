/*
 * Probe (Phase 222b): graphics.library lines, pixels, rectangles and
 * ellipses on an off-screen bitmap (no layer).  Each case is compared as a
 * per-plane bitmap hash; run with a section name for hex dumps.
 */
#include "gfxprobe.h"

#define W 64
#define H 48

static struct gp_bm g;
static struct RastPort rp;

static void reset(void)
{
    gp_clear(&g);
    gp_rp(&rp, &g);
}

static void star(void)
{
    /* lines in all eight octants, from the centre */
    static const WORD ends[][2] = {
        {60, 24}, {60, 30}, {60, 46}, {40, 46}, {32, 46}, {24, 46}, {4, 46}, {4, 30},
        {4, 24}, {4, 18}, {4, 2}, {24, 2}, {32, 2}, {40, 2}, {60, 2}, {60, 18},
        {33, 25}, {31, 23}, {33, 23}, {31, 25}, {32, 24}
    };
    WORD i;
    for (i = 0; i < (WORD)(sizeof(ends) / sizeof(ends[0])); i++) {
        SetAPen(&rp, 1 + (i % 7));
        Move(&rp, 32, 24);
        Draw(&rp, ends[i][0], ends[i][1]);
    }
}

int main(int argc, char **argv)
{
    WORD i;

    gp_args(argc, argv);
    if (!gp_alloc(&g, W, H, 3))
        return 20;

    gp_section("InitRastPort defaults");
    gp_rp(&rp, &g);
    P_LONG("FgPen", rp.FgPen);
    P_LONG("BgPen", rp.BgPen);
    P_LONG("AOlPen", rp.AOlPen);
    P_LONG("DrawMode", rp.DrawMode);
    P_HEX("LinePtrn", rp.LinePtrn);
    P_LONG("Mask", rp.Mask);
    P_HEX("Flags", rp.Flags);
    P_LONG("cp_x", rp.cp_x);
    P_LONG("cp_y", rp.cp_y);
    P_LONG("PenWidth", rp.PenWidth);
    P_LONG("PenHeight", rp.PenHeight);
    P_LONG("AreaPtSz", rp.AreaPtSz);
    P_LONG("linpatcnt", rp.linpatcnt);
    P_LONG("GetAPen", GetAPen(&rp));
    P_LONG("GetBPen", GetBPen(&rp));
    P_LONG("GetDrMd", GetDrMd(&rp));
    P_LONG("GetOutlinePen", GetOutlinePen(&rp));

    gp_section("WritePixel/ReadPixel");
    reset();
    for (i = 0; i < 8; i++) {
        SetAPen(&rp, i);
        P_LONG("WritePixel rc", WritePixel(&rp, i * 3, i));
    }
    gp_row("row 0..7 diag", &rp, 7, 0, 23);
    for (i = 0; i < 8; i++)
        P_LONG("ReadPixel", ReadPixel(&rp, i * 3, i));
    SetDrMd(&rp, COMPLEMENT);
    SetAPen(&rp, 5);
    WritePixel(&rp, 3, 1);
    WritePixel(&rp, 40, 40);
    P_LONG("complement 3,1", ReadPixel(&rp, 3, 1));
    P_LONG("complement 40,40", ReadPixel(&rp, 40, 40));
    SetDrMd(&rp, JAM2);
    rp.Mask = 0x05;
    SetAPen(&rp, 7);
    WritePixel(&rp, 10, 10);
    P_LONG("mask 5 pen 7", ReadPixel(&rp, 10, 10));
    SetAPen(&rp, 0);
    WritePixel(&rp, 6, 2);
    P_LONG("mask 5 pen 0 over 2", ReadPixel(&rp, 6, 2));
    rp.Mask = 0xff;
    gp_hash("pixels", &g);

    gp_section("Move/Draw");
    reset();
    star();
    P_LONG("cp_x", rp.cp_x);
    P_LONG("cp_y", rp.cp_y);
    gp_hash("star JAM1", &g);
    reset();
    Move(&rp, 5, 5);
    Draw(&rp, 5, 5);
    P_LONG("zero length 5,5", ReadPixel(&rp, 5, 5));
    Move(&rp, 0, 0);
    Draw(&rp, W - 1, 0);
    Draw(&rp, W - 1, H - 1);
    Draw(&rp, 0, H - 1);
    Draw(&rp, 0, 0);
    gp_hash("frame", &g);

    gp_section("line pattern");
    for (i = 0; i < 4; i++) {
        static const UBYTE modes[] = {JAM1, JAM2, COMPLEMENT, JAM2 | INVERSVID};
        static const char *const names[] = {"JAM1", "JAM2", "COMPLEMENT", "JAM2|INVERSVID"};
        reset();
        SetRast(&rp, 2);
        SetAPen(&rp, 5);
        SetBPen(&rp, 6);
        SetDrMd(&rp, modes[i]);
        SetDrPt(&rp, 0xf0c3);
        Move(&rp, 1, 1);
        Draw(&rp, 62, 1);
        Draw(&rp, 62, 30);
        Draw(&rp, 1, 46);
        Move(&rp, 3, 40);
        Draw(&rp, 50, 5);
        probe_s(names[i]);
        probe_s(": ");
        P_LONG("linpatcnt", rp.linpatcnt);
        gp_hash("pattern lines", &g);
    }
    SetDrPt(&rp, 0xffff);
    SetDrMd(&rp, JAM1);
    P_HEX("Flags after SetDrPt", rp.Flags);

    gp_section("PolyDraw");
    reset();
    {
        static WORD poly[] = {10, 10, 50, 12, 40, 40, 12, 30, 10, 10, 30, 20};
        SetAPen(&rp, 3);
        Move(&rp, 2, 2);
        PolyDraw(&rp, 6, poly);
        P_LONG("cp_x", rp.cp_x);
        P_LONG("cp_y", rp.cp_y);
        SetDrMd(&rp, COMPLEMENT);
        SetAPen(&rp, 1);
        Move(&rp, 0, 20);
        PolyDraw(&rp, 3, poly);
        SetDrMd(&rp, JAM1);
    }
    gp_hash("polydraw", &g);

    gp_section("RectFill");
    reset();
    SetAPen(&rp, 1);
    RectFill(&rp, 2, 2, 20, 10);
    SetAPen(&rp, 6);
    RectFill(&rp, 7, 1, 7, 40);
    RectFill(&rp, 15, 30, 15, 30);
    SetDrMd(&rp, COMPLEMENT);
    SetAPen(&rp, 3);
    RectFill(&rp, 10, 5, 50, 20);
    SetDrMd(&rp, JAM2 | INVERSVID);
    SetAPen(&rp, 4);
    SetBPen(&rp, 2);
    RectFill(&rp, 30, 25, 60, 45);
    SetDrMd(&rp, JAM1 | INVERSVID);
    SetAPen(&rp, 5);
    RectFill(&rp, 1, 35, 9, 46);
    SetDrMd(&rp, JAM1);
    gp_hash("rectfill modes", &g);

    gp_section("RectFill area pattern");
    {
        static UWORD pat1[2] = {0xaaaa, 0x5555};
        static UWORD pat4[4] = {0xff00, 0x0ff0, 0x00ff, 0xf00f};
        /* multicolour pattern: 4 rows x 3 planes */
        static UWORD patm[12] = {0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f,
                                 0xff00, 0x00ff, 0xff00, 0x00ff,
                                 0x3333, 0xcccc, 0x3333, 0xcccc};
        reset();
        SetAPen(&rp, 5);
        SetBPen(&rp, 2);
        SetAfPt(&rp, pat1, 1);
        RectFill(&rp, 3, 3, 30, 20);
        SetDrMd(&rp, JAM2);
        RectFill(&rp, 33, 3, 60, 20);
        SetAfPt(&rp, pat4, 2);
        RectFill(&rp, 3, 23, 30, 44);
        SetDrMd(&rp, COMPLEMENT);
        RectFill(&rp, 13, 13, 50, 30);
        gp_hash("pattern 1/4 rows", &g);
        reset();
        SetDrMd(&rp, JAM2);
        SetAPen(&rp, 7);
        SetBPen(&rp, 0);
        SetAfPt(&rp, patm, -2);
        RectFill(&rp, 0, 0, 40, 30);
        SetAPen(&rp, 0);
        RectFill(&rp, 21, 21, 63, 47);
        gp_hash("multicolour pattern", &g);
        SetAfPt(&rp, NULL, 0);
        SetDrMd(&rp, JAM1);
    }

    gp_section("DrawEllipse");
    reset();
    SetAPen(&rp, 1);
    DrawEllipse(&rp, 32, 24, 30, 20);
    SetAPen(&rp, 2);
    DrawEllipse(&rp, 32, 24, 10, 10);
    SetAPen(&rp, 4);
    DrawEllipse(&rp, 15, 15, 1, 1);
    DrawEllipse(&rp, 50, 10, 0, 5);
    DrawEllipse(&rp, 50, 40, 6, 0);
    DrawEllipse(&rp, 20, 38, 3, 7);
    gp_hash("ellipses", &g);

    /* EraseRect() is probed on layers only (layers/backfill): without a
     * layer, 3.1 clears an erratic part of the rectangle */
    gp_section("SetRast/ScrollRaster");
    reset();
    SetRast(&rp, 5);
    P_LONG("SetRast 5", ReadPixel(&rp, 63, 47));
    SetAPen(&rp, 2);
    RectFill(&rp, 10, 10, 30, 30);
    gp_hash("setrast", &g);
    reset();
    for (i = 0; i < H; i += 3) {
        SetAPen(&rp, 1 + (i % 7));
        Move(&rp, 0, i);
        Draw(&rp, W - 1 - i, i);
    }
    SetBPen(&rp, 6);
    ScrollRaster(&rp, 5, 3, 8, 4, 50, 40);
    gp_hash("scroll +5,+3", &g);
    ScrollRaster(&rp, -7, -2, 0, 0, 63, 47);
    gp_hash("scroll -7,-2", &g);
    ScrollRaster(&rp, 0, 50, 0, 0, 31, 47);
    gp_hash("scroll beyond", &g);

    gp_free(&g);
    return 0;
}
