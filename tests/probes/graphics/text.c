/*
 * Probe (Phase 222b): graphics.library text - Text(), TextLength(),
 * TextExtent(), TextFit(), FontExtent(), SetSoftStyle()/AskSoftStyle()
 * and the algorithmic styles (bold, italic, underlined) in every draw
 * mode.  Rendering is checked with fonts built in memory (proportional
 * with kerning, and fixed width), so the result does not depend on the
 * ROM font glyphs; topaz 8 is used for metrics only.  Compared as
 * per-plane bitmap hashes; run with a section name for hex dumps.
 */
#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include "gfxprobe.h"

#define W 128
#define H 32
#define LO 32
#define HI 95
#define NCH (HI - LO + 1)
#define FH 9

static struct gp_bm g;
static struct RastPort rp;
static struct TextFont pfont, ffont;
static ULONG ploc[NCH + 1], floc[NCH + 1];
static WORD pspace[NCH + 1], pkern[NCH + 1];

/* glyph widths 2..7, bitmap from a hash of (char, x, y) */
static WORD glyph_w(WORD c) { return (WORD)(2 + (c * 7) % 6); }

static UBYTE glyph_bit(WORD c, WORD x, WORD y)
{
    UWORD h = (UWORD)(c * 31 + x * 7 + y * 13);
    return (UBYTE)(((h ^ (h >> 3)) & 3) == 0 || x == 0 || y == FH - 2);
}

static UBYTE *build_font(struct TextFont *tf, ULONG *loc, WORD *space, WORD *kern, BOOL prop)
{
    WORD c, x, y, total = 0, modulo;
    UBYTE *data;
    for (c = LO; c <= HI + 1; c++)
        total += glyph_w(c);
    modulo = (WORD)(((total + 15) >> 4) << 1);
    data = gp_chip(NULL, (ULONG)modulo * FH);
    if (!data)
        return NULL;
    total = 0;
    for (c = LO; c <= HI + 1; c++) {
        WORD i = (WORD)(c - LO), w = glyph_w(c);
        loc[i] = ((ULONG)total << 16) | (ULONG)w;
        if (space)
            space[i] = (WORD)(w + 1 + (c % 3 == 0));
        if (kern)
            kern[i] = (WORD)((c % 4) - 1);
        for (y = 0; y < FH; y++)
            for (x = 0; x < w; x++)
                if (c != ' ' && glyph_bit(c, x, y)) {
                    WORD bx = (WORD)(total + x);
                    data[y * modulo + (bx >> 3)] |= (UBYTE)(0x80 >> (bx & 7));
                }
        total += w;
    }
    tf->tf_Message.mn_Node.ln_Name = (char *)(prop ? "probeprop.font" : "probefix.font");
    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    tf->tf_YSize = FH;
    tf->tf_Style = FS_NORMAL;
    tf->tf_Flags = (UBYTE)(FPF_DESIGNED | (prop ? FPF_PROPORTIONAL : 0));
    tf->tf_XSize = 8;
    tf->tf_Baseline = 6;
    tf->tf_BoldSmear = 1;
    tf->tf_Accessors = 0;
    tf->tf_LoChar = LO;
    tf->tf_HiChar = HI;
    tf->tf_CharData = data;
    tf->tf_Modulo = (UWORD)modulo;
    tf->tf_CharLoc = loc;
    tf->tf_CharSpace = space;
    tf->tf_CharKern = kern;
    return data;
}

static void show_extent(const char *label, struct TextExtent *te)
{
    probe_s(label);
    probe_s(": w ");
    probe_dec(te->te_Width);
    probe_s(" h ");
    probe_dec(te->te_Height);
    probe_s(" extent ");
    probe_dec(te->te_Extent.MinX);
    probe_ch(',');
    probe_dec(te->te_Extent.MinY);
    probe_ch(' ');
    probe_dec(te->te_Extent.MaxX);
    probe_ch(',');
    probe_dec(te->te_Extent.MaxY);
    probe_ch('\n');
}

static const char *const strs[] = {"A", "Hello", "Amiga 3.1!", "WWW,,,", "", " _^\\Z", "a~{}\x7f"};

static void metrics(const char *name)
{
    WORD i;
    struct TextExtent te;
    for (i = 0; i < (WORD)(sizeof(strs) / sizeof(strs[0])); i++) {
        WORD n = 0;
        while (strs[i][n])
            n++;
        probe_s(name);
        probe_s(" \"");
        probe_s(strs[i]);
        probe_s("\" TextLength = ");
        probe_dec(TextLength(&rp, (STRPTR)strs[i], n));
        probe_ch('\n');
        TextExtent(&rp, (STRPTR)strs[i], n, &te);
        show_extent("  TextExtent", &te);
    }
}

static void fits(const char *name)
{
    static const char s[] = "The quick brown fox";
    static const WORD widths[] = {0, 1, 7, 30, 61, 200};
    struct TextExtent te, ce;
    WORD i, d;
    for (d = 1; d >= -1; d -= 2)
        for (i = 0; i < (WORD)(sizeof(widths) / sizeof(widths[0])); i++) {
            ULONG n = TextFit(&rp, (STRPTR)(d > 0 ? s : s + 18), 19, &te, NULL, d, widths[i], FH + 4);
            probe_s(name);
            probe_s(" TextFit dir ");
            probe_dec(d);
            probe_s(" width ");
            probe_dec(widths[i]);
            probe_s(" = ");
            probe_dec(n);
            probe_ch('\n');
            show_extent("  fit", &te);
        }
    ce.te_Width = 25;
    ce.te_Height = 20;
    ce.te_Extent.MinX = -1;
    ce.te_Extent.MinY = -8;
    ce.te_Extent.MaxX = 22;
    ce.te_Extent.MaxY = 3;
    probe_s(name);
    probe_s(" TextFit constraining extent = ");
    probe_dec(TextFit(&rp, (STRPTR)s, 19, &te, &ce, 1, 100, 100));
    probe_ch('\n');
    show_extent("  fit", &te);
    probe_s(name);
    probe_s(" TextFit height too small = ");
    probe_dec(TextFit(&rp, (STRPTR)s, 19, &te, NULL, 1, 100, 3));
    probe_ch('\n');
}

static void render(const char *label, struct TextFont *tf)
{
    static const UBYTE dm[] = {JAM1, JAM2, COMPLEMENT, JAM1 | INVERSVID, JAM2 | INVERSVID};
    static const UBYTE st[] = {FS_NORMAL, FSF_BOLD, FSF_ITALIC, FSF_UNDERLINED,
                               FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED};
    WORD i;
    for (i = 0; i < 5; i++) {
        gp_clear(&g);
        gp_rp(&rp, &g);
        SetRast(&rp, 2);
        SetFont(&rp, tf);
        SetAPen(&rp, 5);
        SetBPen(&rp, 6);
        SetDrMd(&rp, dm[i]);
        Move(&rp, 4, 8);
        Text(&rp, (STRPTR)"Hello, Amiga!", 13);
        probe_s(label);
        probe_s(" dm ");
        probe_dec(dm[i]);
        probe_s(" cp_x ");
        probe_dec(rp.cp_x);
        gp_hash("", &g);
    }
    for (i = 0; i < 5; i++) {
        ULONG r;
        gp_clear(&g);
        gp_rp(&rp, &g);
        SetFont(&rp, tf);
        SetAPen(&rp, 3);
        SetBPen(&rp, 4);
        SetDrMd(&rp, JAM2);
        r = SetSoftStyle(&rp, st[i], 0xff);
        Move(&rp, 6, 10);
        Text(&rp, (STRPTR)"Style SAMPLE @", 14);
        Move(&rp, 6, 24);
        Text(&rp, (STRPTR)"[x]", 3);
        probe_s(label);
        probe_s(" style ");
        probe_dec(st[i]);
        probe_s(" SetSoftStyle ");
        probe_hex(r, 2);
        probe_s(" rp.AlgoStyle ");
        probe_hex(rp.AlgoStyle, 2);
        probe_s(" cp_x ");
        probe_dec(rp.cp_x);
        probe_s(" TxWidth ");
        probe_dec(rp.TxWidth);
        gp_hash("", &g);
    }
}

int main(int argc, char **argv)
{
    struct TextAttr ta;
    struct TextFont *topaz;
    ULONG r;

    gp_args(argc, argv);
    if (!gp_alloc(&g, W, H, 3))
        return 20;
    if (!build_font(&pfont, ploc, pspace, pkern, TRUE) || !build_font(&ffont, floc, NULL, NULL, FALSE))
        return 20;

    gp_section("topaz 8");
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    topaz = OpenFont(&ta);
    P_NULL("OpenFont topaz 8", topaz);
    if (!topaz)
        return 20;
    P_LONG("tf_YSize", topaz->tf_YSize);
    P_LONG("tf_XSize", topaz->tf_XSize);
    P_LONG("tf_Baseline", topaz->tf_Baseline);
    P_HEX("tf_Style", topaz->tf_Style);
    P_HEX("tf_Flags", topaz->tf_Flags);
    P_LONG("tf_BoldSmear", topaz->tf_BoldSmear);
    P_LONG("tf_LoChar", topaz->tf_LoChar);
    P_LONG("tf_HiChar", topaz->tf_HiChar);
    P_NULL("tf_CharSpace", topaz->tf_CharSpace);
    P_NULL("tf_CharKern", topaz->tf_CharKern);
    gp_rp(&rp, &g);
    SetFont(&rp, topaz);
    P_LONG("rp TxHeight", rp.TxHeight);
    P_LONG("rp TxWidth", rp.TxWidth);
    P_LONG("rp TxBaseline", rp.TxBaseline);
    P_LONG("rp TxSpacing", rp.TxSpacing);
    P_HEX("AskSoftStyle", AskSoftStyle(&rp));
    metrics("topaz");
    fits("topaz");
    r = SetSoftStyle(&rp, FSF_BOLD | FSF_ITALIC, 0xff);
    P_HEX("SetSoftStyle bold|italic", r);
    P_LONG("bold|italic TxWidth", rp.TxWidth);
    metrics("topaz b|i");
    fits("topaz b|i");
    r = SetSoftStyle(&rp, FSF_UNDERLINED, FSF_UNDERLINED);
    P_HEX("SetSoftStyle +underlined (enable underlined)", r);
    r = SetSoftStyle(&rp, 0, FSF_BOLD);
    P_HEX("SetSoftStyle -bold (enable bold)", r);
    r = SetSoftStyle(&rp, FSF_EXTENDED | FSF_COLORFONT, 0xff);
    P_HEX("SetSoftStyle extended|colorfont", r);
    {
        struct TextExtent te;
        FontExtent(topaz, &te);
        show_extent("FontExtent topaz", &te);
    }

    gp_section("proportional font metrics");
    gp_rp(&rp, &g);
    SetFont(&rp, &pfont);
    P_HEX("AskSoftStyle", AskSoftStyle(&rp));
    P_LONG("rp TxWidth", rp.TxWidth);
    metrics("prop");
    fits("prop");
    SetSoftStyle(&rp, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED, 0xff);
    metrics("prop b|i|u");
    {
        struct TextExtent te;
        FontExtent(&pfont, &te);
        show_extent("FontExtent prop", &te);
    }

    gp_section("fixed font metrics");
    gp_rp(&rp, &g);
    SetFont(&rp, &ffont);
    metrics("fixed");
    fits("fixed");

    gp_section("proportional rendering");
    render("prop", &pfont);
    gp_section("fixed rendering");
    render("fixed", &ffont);

    gp_section("special cases");
    gp_clear(&g);
    gp_rp(&rp, &g);
    SetFont(&rp, &pfont);
    SetAPen(&rp, 7);
    Move(&rp, 2, 8);
    Text(&rp, (STRPTR)"\x01" "a\x7f\xff" "Z", 5);
    P_LONG("out-of-range chars cp_x", rp.cp_x);
    Move(&rp, 2, 20);
    Text(&rp, (STRPTR)"xyz", 0);
    P_LONG("zero length cp_x", rp.cp_x);
    rp.TxSpacing = 3;
    Move(&rp, 40, 20);
    Text(&rp, (STRPTR)"ABC", 3);
    P_LONG("TxSpacing 3 cp_x", rp.cp_x);
    P_LONG("TxSpacing 3 TextLength", TextLength(&rp, (STRPTR)"ABC", 3));
    rp.TxSpacing = 0;
    SetFont(&rp, &ffont);
    rp.Mask = 0x03;
    SetAPen(&rp, 6);
    Move(&rp, 80, 28);
    Text(&rp, (STRPTR)"Mask", 4);
    rp.Mask = 0xff;
    gp_hash("special", &g);

    CloseFont(topaz);
    gp_chip_free();
    gp_free(&g);
    return 0;
}
