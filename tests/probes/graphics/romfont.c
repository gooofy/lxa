/*
 * Probe (Phase 225): the ROM fonts of graphics.library - topaz 8 and
 * topaz 9 as OpenFont() returns them without diskfont.library.
 *
 * Prints every TextFont field a program can rely on, the CharLoc /
 * CharSpace / CharKern tables, a hash of the glyph strike, and every
 * glyph as Text() draws it (one line per character, rows as hex).  The
 * output captured on AmigaOS 3.1 is the source of lxa's ROM font tables
 * (tools/gen_romfont.py, doc/rom-fonts.md); lxa must reproduce it exactly.  The last
 * sections render topaz in every algorithmic style.
 */
#include <graphics/text.h>
#include <graphics/gfxbase.h>
#include <clib/graphics_protos.h>
#include "gfxprobe.h"

#define GW 32           /* glyph bitmap: the glyph is drawn at x = 8 */
#define SW 208          /* style bitmap: wide enough for topaz 9 + italic */
#define SH 16

static struct gp_bm g, s;
static struct RastPort rp;

static void hexrow(const UBYTE *row, WORD bytes)
{
    static const char hx[] = "0123456789abcdef";
    WORD x;
    for (x = 0; x < bytes; x++) {
        probe_ch(hx[row[x] >> 4]);
        probe_ch(hx[row[x] & 15]);
    }
}

static void table(const char *label, struct TextFont *tf, const void *tab, BOOL longs)
{
    WORD i, n = (WORD)(tf->tf_HiChar - tf->tf_LoChar + 2);
    if (!tab) {
        P_NULL(label, tab);
        return;
    }
    for (i = 0; i < n; i++) {
        if (i % 8 == 0) {
            probe_s(label);
            probe_ch(' ');
            probe_dec(tf->tf_LoChar + i);
            probe_ch(':');
        }
        probe_ch(' ');
        if (longs)
            probe_hex(((const ULONG *)tab)[i], 8);
        else
            probe_dec(((const WORD *)tab)[i]);
        if (i % 8 == 7 || i == n - 1)
            probe_ch('\n');
    }
}

/* draw character c alone and print its rows */
static void glyph(struct TextFont *tf, WORD c)
{
    UBYTE ch = (UBYTE)c;
    WORD y;
    gp_clear(&g);
    gp_rp(&rp, &g);
    SetFont(&rp, tf);
    SetAPen(&rp, 1);
    SetDrMd(&rp, JAM1);
    Move(&rp, 8, tf->tf_Baseline);
    Text(&rp, (STRPTR)&ch, 1);
    probe_s("glyph ");
    probe_dec(c);
    probe_s(" cp_x ");
    probe_dec(rp.cp_x - 8);
    probe_ch(':');
    for (y = 0; y < tf->tf_YSize; y++) {
        probe_ch(' ');
        hexrow(g.bm.Planes[0] + (LONG)y * g.bm.BytesPerRow, GW / 8);
    }
    probe_ch('\n');
}

static void dump_font(const char *name, struct TextFont *tf)
{
    WORD c;
    struct TextExtent te;

    gp_section(name);
    P_STR("ln_Name", tf->tf_Message.mn_Node.ln_Name);
    P_LONG("ln_Type", tf->tf_Message.mn_Node.ln_Type);
    P_LONG("ln_Pri", tf->tf_Message.mn_Node.ln_Pri);
    P_LONG("tf_YSize", tf->tf_YSize);
    P_LONG("tf_XSize", tf->tf_XSize);
    P_LONG("tf_Baseline", tf->tf_Baseline);
    P_HEX("tf_Style", tf->tf_Style);
    P_HEX("tf_Flags", tf->tf_Flags);
    P_LONG("tf_BoldSmear", tf->tf_BoldSmear);
    P_LONG("tf_LoChar", tf->tf_LoChar);
    P_LONG("tf_HiChar", tf->tf_HiChar);
    P_LONG("tf_Modulo", tf->tf_Modulo);
    P_NULL("tf_CharData", tf->tf_CharData);
    if (tf->tf_CharData)
        P_HEX("strike hash", probe_hash(tf->tf_CharData, (LONG)tf->tf_Modulo * tf->tf_YSize));
    table("loc", tf, tf->tf_CharLoc, TRUE);
    table("space", tf, tf->tf_CharSpace, FALSE);
    table("kern", tf, tf->tf_CharKern, FALSE);
    FontExtent(tf, &te);
    probe_s("FontExtent: w ");
    probe_dec(te.te_Width);
    probe_s(" h ");
    probe_dec(te.te_Height);
    probe_s(" extent ");
    probe_dec(te.te_Extent.MinX);
    probe_ch(',');
    probe_dec(te.te_Extent.MinY);
    probe_ch(' ');
    probe_dec(te.te_Extent.MaxX);
    probe_ch(',');
    probe_dec(te.te_Extent.MaxY);
    probe_ch('\n');

    for (c = tf->tf_LoChar; c <= tf->tf_HiChar; c++)
        glyph(tf, c);
    /* the default glyph (characters outside LoChar..HiChar) */
    if (tf->tf_LoChar > 0)
        glyph(tf, tf->tf_LoChar - 1);
    else if (tf->tf_HiChar < 255)
        glyph(tf, tf->tf_HiChar + 1);
}

static const char sample[] = "Amiga 3.1 [x] Wg_|";
static const UBYTE styles[] = {FS_NORMAL, FSF_BOLD, FSF_ITALIC, FSF_UNDERLINED,
                               FSF_BOLD | FSF_ITALIC, FSF_BOLD | FSF_UNDERLINED,
                               FSF_ITALIC | FSF_UNDERLINED,
                               FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED};

static void styled(const char *name, struct TextFont *tf)
{
    WORD i, y, n = sizeof(sample) - 1;
    gp_section(name);
    for (i = 0; i < (WORD)sizeof(styles); i++) {
        struct TextExtent te;
        gp_clear(&s);
        gp_rp(&rp, &s);
        SetFont(&rp, tf);
        SetSoftStyle(&rp, styles[i], 0xff);
        SetAPen(&rp, 1);
        SetDrMd(&rp, JAM1);
        Move(&rp, 4, 2 + tf->tf_Baseline);
        Text(&rp, (STRPTR)sample, n);
        TextExtent(&rp, (STRPTR)sample, n, &te);
        probe_s("style ");
        probe_dec(styles[i]);
        probe_s(" cp_x ");
        probe_dec(rp.cp_x);
        probe_s(" TextLength ");
        probe_dec(TextLength(&rp, (STRPTR)sample, n));
        probe_s(" extent ");
        probe_dec(te.te_Extent.MinX);
        probe_ch(',');
        probe_dec(te.te_Extent.MinY);
        probe_ch(' ');
        probe_dec(te.te_Extent.MaxX);
        probe_ch(',');
        probe_dec(te.te_Extent.MaxY);
        probe_s(" w ");
        probe_dec(te.te_Width);
        probe_ch('\n');
        for (y = 0; y < SH; y++) {
            probe_s("  ");
            if (y < 10)
                probe_ch(' ');
            probe_dec(y);
            probe_ch(' ');
            hexrow(s.bm.Planes[0] + (LONG)y * s.bm.BytesPerRow, SW / 8);
            probe_ch('\n');
        }
    }
}

int main(int argc, char **argv)
{
    static const WORD sizes[] = {0, 5, 7, 8, 9, 10, 11, 13, 20};
    struct TextAttr ta;
    struct TextFont *t8, *t9, *tf;
    WORD i;

    gp_args(argc, argv);
    if (!gp_alloc(&g, GW, 12, 1) || !gp_alloc(&s, SW, SH, 1))
        return 20;

    gp_section("OpenFont sizes");
    ta.ta_Name = (STRPTR)"topaz.font";
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    for (i = 0; i < (WORD)(sizeof(sizes) / sizeof(sizes[0])); i++) {
        ta.ta_YSize = (UWORD)sizes[i];
        tf = OpenFont(&ta);
        probe_s("OpenFont topaz ");
        probe_dec(sizes[i]);
        probe_s(" = ");
        if (tf) {
            probe_s("ysize ");
            probe_dec(tf->tf_YSize);
            probe_s(" flags ");
            probe_hex(tf->tf_Flags, 2);
            CloseFont(tf);
        } else
            probe_s("NULL");
        probe_ch('\n');
    }
    ta.ta_YSize = 8;
    ta.ta_Style = FSF_BOLD;
    tf = OpenFont(&ta);
    probe_s("OpenFont topaz 8 bold = ");
    if (tf) {
        probe_s("style ");
        probe_hex(tf->tf_Style, 2);
        CloseFont(tf);
    } else
        probe_s("NULL");
    probe_ch('\n');
    ta.ta_Style = 0;
    ta.ta_Flags = FPF_DISKFONT;
    tf = OpenFont(&ta);
    P_NULL("OpenFont topaz 8 FPF_DISKFONT", tf);
    if (tf)
        CloseFont(tf);
    ta.ta_Flags = 0;

    /* the ROM fonts in GfxBase->TextFonts, in list order */
    {
        struct Node *n;
        Forbid();
        for (n = GfxBase->TextFonts.lh_Head; n->ln_Succ; n = n->ln_Succ) {
            struct TextFont *f = (struct TextFont *)n;
            if (!(f->tf_Flags & FPF_ROMFONT))
                continue;
            probe_s("TextFonts: ");
            probe_s(n->ln_Name);
            probe_ch(' ');
            probe_dec(f->tf_YSize);
            probe_ch('\n');
        }
        Permit();
    }
    P_BOOL("DefaultFont is topaz 8",
           GfxBase->DefaultFont && GfxBase->DefaultFont->tf_YSize == 8);

    ta.ta_YSize = 8;
    t8 = OpenFont(&ta);
    ta.ta_YSize = 9;
    t9 = OpenFont(&ta);
    if (!t8 || !t9 || t8->tf_YSize != 8 || t9->tf_YSize != 9) {
        probe_s("ROM topaz 8/9 missing\n");
        return 20;
    }
    dump_font("topaz 8", t8);
    dump_font("topaz 9", t9);
    styled("topaz 8 styles", t8);
    styled("topaz 9 styles", t9);

    CloseFont(t8);
    CloseFont(t9);
    gp_free(&g);
    gp_free(&s);
    return 0;
}
