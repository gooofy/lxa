/*
 * dfprobe.h - shared helpers of the diskfont.library probes (Phase 222f).
 *
 * The probes do not depend on any Commodore font: they write their own
 * bitmap fonts (contents files plus LoadSeg()-able font hunk files) to
 * T:lxdf/ and point FONTS: at that directory while they run.  The original
 * FONTS: assign is restored and every file is deleted at the end.
 *
 * Fonts written (all glyph bitmaps are generated from a hash):
 *   lxprop.font   FCH_ID   proportional, sizes 8 and 11, with kerning
 *   lxfixed.font  FCH_ID   fixed width 8, size 10
 *   lxtag.font    TFCH_ID  proportional, size 9 with TA_DeviceDPI tag,
 *                          and an untagged size 6
 *   lxcolor.font  FCH_ID   2-plane colour font, size 7
 *   lxbroken.font FCH_ID   one entry whose font file does not exist
 *   lxempty.font  FCH_ID   no entries
 */
#ifndef LXA_DFPROBE_H
#define LXA_DFPROBE_H

#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/doshunks.h>
#include <graphics/text.h>
#include <graphics/gfxbase.h>
#include <diskfont/diskfont.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/diskfont.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
struct Library *DiskfontBase;

#define DF_DIR "T:lxdf"

/* ------------------------------------------------------------------ */
/* font generation                                                     */

struct df_spec {
    const char *file;           /* "lxprop/8" */
    const char *dfh_name;       /* dfh_Name inside the file */
    UWORD ysize;
    UBYTE style;
    UBYTE flags;
    UBYTE lo, hi;
    UBYTE prop;                 /* proportional: charspace + charkern */
    UBYTE depth;                /* 1, or 2 for a colour font */
    ULONG dpi;                  /* TA_DeviceDPI tag value, 0 = untagged */
    UWORD revision;
    UBYTE pattern;              /* 0 = hashed glyphs, 1 = row/column analysis glyphs */
};

/* pattern 1: glyph i < ysize has only row i set (width 8); then for
 * widths 1..8 one glyph per column with only that column set; then a
 * full 8-wide glyph and a single pixel */
static WORD df_ana_w(const struct df_spec *s, WORD i)
{
    WORD w, k = (WORD)(i - s->ysize);
    if (i < (WORD)s->ysize)
        return 8;
    for (w = 1; w <= 8; w++) {
        if (k < w)
            return w;
        k -= w;
    }
    return k == 0 ? 8 : 1;
}

static UBYTE df_ana_bit(const struct df_spec *s, WORD i, WORD x, WORD y)
{
    WORD w, k = (WORD)(i - s->ysize);
    if (i < (WORD)s->ysize)
        return (UBYTE)(y == i);
    for (w = 1; w <= 8; w++) {
        if (k < w)
            return (UBYTE)(x == k);
        k -= w;
    }
    return (UBYTE)(k == 0 || (x == 0 && y == 0));
}

static WORD df_ana_w(const struct df_spec *s, WORD i);
static UBYTE df_ana_bit(const struct df_spec *s, WORD i, WORD x, WORD y);

static WORD df_glyph_w(const struct df_spec *s, WORD c)
{
    if (s->pattern)
        return c > s->hi ? 1 : df_ana_w(s, (WORD)(c - s->lo));
    if (!s->prop)
        return 8;
    if (c % 13 == 0)            /* a few missing glyphs */
        return 0;
    return (WORD)(1 + (c * 5 + s->ysize) % 7);
}

static UBYTE df_glyph_bit(const struct df_spec *s, WORD c, WORD x, WORD y, WORD plane)
{
    UWORD h;
    if (s->pattern)
        return c > s->hi ? (UBYTE)(x == 0) : df_ana_bit(s, (WORD)(c - s->lo), x, y);
    h = (UWORD)(c * 31 + x * 7 + y * 13 + s->ysize * 5 + plane * 17);
    h = (UWORD)(h ^ (h >> 3) ^ (h << 2));
    return (UBYTE)((h & 5) == 1 || (x == 0 && y > 1) || y == s->ysize - 2);
}

struct df_img {
    UBYTE *data;
    ULONG size;
    ULONG used;
    ULONG relocs[24];
    UWORD nrelocs;
};

static ULONG df_alloc(struct df_img *im, ULONG n)
{
    ULONG o = (im->used + 3) & ~3UL;
    im->used = o + n;
    return o;
}

static void df_put_ptr(struct df_img *im, ULONG at, ULONG target)
{
    *(ULONG *)(im->data + at) = target;
    im->relocs[im->nrelocs++] = at;
}

/* writes one font hunk file; returns TRUE on success */
static BOOL df_write_font(const struct df_spec *s)
{
    struct df_img im;
    WORD n = (WORD)(s->hi - s->lo + 2), i, c, x, y, p;
    WORD total = 0, modulo;
    ULONG tf_off, dfh_off, data_off[2], loc_off, space_off = 0, kern_off = 0, tag_off = 0, col_off = 0, tab_off = 0;
    ULONG hdr[6];
    ULONG len;
    BPTR fh;
    BOOL ok;
    struct TextFont *tf;
    struct DiskFontHeader *dfh;
    char path[64];

    for (c = s->lo; c <= s->hi + 1; c++)
        total += df_glyph_w(s, c);
    modulo = (WORD)(((total + 15) >> 4) << 1);

    im.size = 8192;
    im.data = AllocVec(im.size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!im.data)
        return FALSE;
    im.used = 0;
    im.nrelocs = 0;

    *(ULONG *)im.data = 0x70004e75;                         /* moveq #0,d0; rts */
    im.used = 4;
    dfh_off = 4;
    im.used = dfh_off + sizeof(struct DiskFontHeader) + (s->depth > 1 ? sizeof(struct ColorTextFont) - sizeof(struct TextFont) : 0);
    tf_off = dfh_off + (ULONG)((UBYTE *)&((struct DiskFontHeader *)0)->dfh_TF - (UBYTE *)0);

    for (p = 0; p < s->depth; p++)
        data_off[p] = df_alloc(&im, (ULONG)modulo * s->ysize);
    loc_off = df_alloc(&im, (ULONG)n * 4);
    if (s->prop) {
        space_off = df_alloc(&im, (ULONG)n * 2);
        kern_off = df_alloc(&im, (ULONG)n * 2);
    }
    if (s->dpi)
        tag_off = df_alloc(&im, 16);
    if (s->depth > 1) {
        col_off = df_alloc(&im, 8);
        tab_off = df_alloc(&im, 8);
    }
    im.used = (im.used + 3) & ~3UL;

    dfh = (struct DiskFontHeader *)(im.data + dfh_off);
    tf = (struct TextFont *)(im.data + tf_off);

    dfh->dfh_DF.ln_Type = NT_FONT;
    df_put_ptr(&im, dfh_off + 10, dfh_off + 26);            /* ln_Name -> dfh_Name */
    dfh->dfh_FileID = DFH_ID;
    dfh->dfh_Revision = s->revision;
    if (s->dpi)
        df_put_ptr(&im, dfh_off + 22, tag_off);             /* dfh_TagList */
    for (i = 0; s->dfh_name[i]; i++)
        dfh->dfh_Name[i] = s->dfh_name[i];

    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    df_put_ptr(&im, tf_off + 10, dfh_off + 26);             /* ln_Name -> dfh_Name */
    tf->tf_Message.mn_Length = (UWORD)(im.used - tf_off);
    tf->tf_YSize = s->ysize;
    tf->tf_Style = s->style;
    tf->tf_Flags = s->flags;
    tf->tf_XSize = s->prop ? 6 : 8;
    tf->tf_Baseline = (UWORD)(s->ysize - 3);
    tf->tf_BoldSmear = 1;
    tf->tf_LoChar = s->lo;
    tf->tf_HiChar = s->hi;
    df_put_ptr(&im, tf_off + 34, data_off[0]);
    tf->tf_Modulo = modulo;
    df_put_ptr(&im, tf_off + 40, loc_off);
    if (s->prop) {
        df_put_ptr(&im, tf_off + 44, space_off);
        df_put_ptr(&im, tf_off + 48, kern_off);
    }
    if (s->depth > 1) {
        struct ColorTextFont *ctf = (struct ColorTextFont *)tf;
        UWORD *cfc = (UWORD *)(im.data + col_off);
        UWORD *tab = (UWORD *)(im.data + tab_off);
        ctf->ctf_Flags = CT_COLORFONT;
        ctf->ctf_Depth = s->depth;
        ctf->ctf_FgColor = 0xff;
        ctf->ctf_Low = 0;
        ctf->ctf_High = 3;
        ctf->ctf_PlanePick = 3;
        ctf->ctf_PlaneOnOff = 0;
        df_put_ptr(&im, tf_off + 60, col_off);
        for (p = 0; p < s->depth; p++)
            df_put_ptr(&im, tf_off + 64 + p * 4, data_off[p]);
        cfc[0] = 0;
        cfc[1] = 4;
        df_put_ptr(&im, col_off + 4, tab_off);
        tab[0] = 0x0000; tab[1] = 0x0f00; tab[2] = 0x00f0; tab[3] = 0x000f;
    }
    if (s->dpi) {
        struct TagItem *t = (struct TagItem *)(im.data + tag_off);
        t[0].ti_Tag = TA_DeviceDPI;
        t[0].ti_Data = s->dpi;
        t[1].ti_Tag = TAG_DONE;
        t[1].ti_Data = 0;
    }

    total = 0;
    for (c = s->lo; c <= s->hi + 1; c++) {
        WORD w = df_glyph_w(s, c);
        i = (WORD)(c - s->lo);
        ((ULONG *)(im.data + loc_off))[i] = w ? (((ULONG)total << 16) | (ULONG)w) : 0;
        if (s->prop) {
            ((WORD *)(im.data + space_off))[i] = (WORD)(w ? w + 1 : 4);
            ((WORD *)(im.data + kern_off))[i] = (WORD)((c % 3) - 1);
        }
        for (p = 0; p < s->depth; p++)
            for (y = 0; y < (WORD)s->ysize; y++)
                for (x = 0; x < w; x++)
                    if (c != ' ' && df_glyph_bit(s, c, x, y, p)) {
                        WORD bx = (WORD)(total + x);
                        im.data[data_off[p] + (ULONG)y * modulo + (bx >> 3)] |= (UBYTE)(0x80 >> (bx & 7));
                    }
        total += w;
    }

    len = im.used / 4;
    strcpy(path, DF_DIR "/");
    strcat(path, s->file);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        FreeVec(im.data);
        return FALSE;
    }
    hdr[0] = HUNK_HEADER; hdr[1] = 0; hdr[2] = 1; hdr[3] = 0; hdr[4] = 0; hdr[5] = len;
    ok = Write(fh, hdr, 24) == 24;
    hdr[0] = HUNK_CODE; hdr[1] = len;
    ok = ok && Write(fh, hdr, 8) == 8;
    ok = ok && Write(fh, im.data, (LONG)im.used) == (LONG)im.used;
    hdr[0] = HUNK_RELOC32; hdr[1] = im.nrelocs; hdr[2] = 0;
    ok = ok && Write(fh, hdr, 12) == 12;
    ok = ok && Write(fh, im.relocs, (LONG)im.nrelocs * 4) == (LONG)im.nrelocs * 4;
    hdr[0] = 0; hdr[1] = HUNK_END;
    ok = ok && Write(fh, hdr, 8) == 8;
    Close(fh);
    FreeVec(im.data);
    return ok;
}

struct df_entry {
    const char *file;
    UWORD ysize;
    UBYTE style, flags;
    ULONG dpi;                  /* TFCH: TA_DeviceDPI tag, 0 = no tags */
};

static BOOL df_write_contents(const char *name, UWORD id, const struct df_entry *e, UWORD n)
{
    char path[64];
    BPTR fh;
    BOOL ok;
    UWORD i;
    struct FontContentsHeader fch;
    UBYTE *buf = AllocVec(sizeof(struct TFontContents), MEMF_PUBLIC);

    if (!buf)
        return FALSE;
    strcpy(path, DF_DIR "/");
    strcat(path, name);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh) {
        FreeVec(buf);
        return FALSE;
    }
    fch.fch_FileID = id;
    fch.fch_NumEntries = n;
    ok = Write(fh, &fch, 4) == 4;
    for (i = 0; i < n && ok; i++) {
        struct TFontContents *tfc = (struct TFontContents *)buf;
        int k;
        for (k = 0; k < (int)sizeof(struct TFontContents); k++)
            buf[k] = 0;
        strcpy((char *)tfc->tfc_FileName, e[i].file);
        if (id == TFCH_ID && e[i].dpi) {
            struct TagItem *t = (struct TagItem *)&tfc->tfc_FileName[MAXFONTPATH - 2 * sizeof(struct TagItem)];
            t[0].ti_Tag = TA_DeviceDPI;
            t[0].ti_Data = e[i].dpi;
            t[1].ti_Tag = TAG_DONE;
            t[1].ti_Data = 0;
            tfc->tfc_TagCount = 2;
        }
        tfc->tfc_YSize = e[i].ysize;
        tfc->tfc_Style = e[i].style;
        tfc->tfc_Flags = e[i].flags;
        ok = Write(fh, buf, sizeof(struct TFontContents)) == sizeof(struct TFontContents);
    }
    Close(fh);
    FreeVec(buf);
    return ok;
}

#define PF (FPF_PROPORTIONAL | FPF_DESIGNED)

static const struct df_spec df_fonts[] = {
    { "lxprop/8",   "lxprop8",  8, 0, PF, 32, 90, 1, 1, 0, 3 },
    { "lxprop/11",  "lxprop11", 11, 0, PF, 32, 90, 1, 1, 0, 4 },
    { "lxfixed/10", "lxfixed",  10, 0, FPF_DESIGNED, 32, 90, 0, 1, 0, 1 },
    { "lxtag/9",    "lxtag9",   9, FSF_TAGGED, PF, 40, 80, 1, 1, 0x00640032, 2 },
    { "lxtag/6",    "lxtag6",   6, 0, PF, 40, 80, 1, 1, 0, 2 },
    { "lxcolor/7",  "lxcolor",  7, FSF_COLORFONT, PF, 48, 70, 1, 2, 0, 1 },
};

static const struct df_entry df_prop_e[] = {
    { "lxprop/8", 8, 0, PF, 0 }, { "lxprop/11", 11, 0, PF, 0 } };
static const struct df_entry df_fixed_e[] = { { "lxfixed/10", 10, 0, FPF_DESIGNED, 0 } };
static const struct df_entry df_tag_e[] = {
    { "lxtag/9", 9, FSF_TAGGED, PF, 0x00640032 }, { "lxtag/6", 6, 0, PF, 0 } };
static const struct df_entry df_color_e[] = { { "lxcolor/7", 7, FSF_COLORFONT, PF, 0 } };
static const struct df_entry df_broken_e[] = { { "lxbroken/12", 12, 0, PF, 0 } };

static const char *df_dirs[] = { "lxprop", "lxfixed", "lxtag", "lxcolor", "lxbroken", "lxempty", NULL };

static BPTR df_old_fonts;
static BPTR df_dirlock;

static void df_delete(const char *rel)
{
    char path[64];
    strcpy(path, DF_DIR "/");
    strcat(path, rel);
    DeleteFile((STRPTR)path);
}

static void df_cleanup(void)
{
    unsigned i;
    /* restore FONTS: (the assign owns the lock afterwards) */
    AssignLock((STRPTR)"FONTS", df_old_fonts);
    df_old_fonts = 0;
    if (df_dirlock)
        UnLock(df_dirlock);
    df_dirlock = 0;
    for (i = 0; i < sizeof(df_fonts) / sizeof(df_fonts[0]); i++)
        df_delete(df_fonts[i].file);
    for (i = 0; df_dirs[i]; i++) {
        char n[32];
        strcpy(n, df_dirs[i]);
        df_delete(n);
        strcat(n, ".font");
        df_delete(n);
    }
    DeleteFile((STRPTR)DF_DIR);
}

/* creates the fonts and assigns FONTS: to them; FALSE on failure */
static BOOL df_setup(void)
{
    unsigned i;
    BPTR l;
    BOOL ok = TRUE;

    l = CreateDir((STRPTR)DF_DIR);
    if (l)
        UnLock(l);
    for (i = 0; df_dirs[i]; i++) {
        char n[32];
        strcpy(n, DF_DIR "/");
        strcat(n, df_dirs[i]);
        l = CreateDir((STRPTR)n);
        if (l)
            UnLock(l);
    }
    for (i = 0; i < sizeof(df_fonts) / sizeof(df_fonts[0]); i++)
        ok = ok && df_write_font(&df_fonts[i]);
    ok = ok && df_write_contents("lxprop.font", FCH_ID, df_prop_e, 2);
    ok = ok && df_write_contents("lxfixed.font", FCH_ID, df_fixed_e, 1);
    ok = ok && df_write_contents("lxtag.font", TFCH_ID, df_tag_e, 2);
    ok = ok && df_write_contents("lxcolor.font", FCH_ID, df_color_e, 1);
    ok = ok && df_write_contents("lxbroken.font", FCH_ID, df_broken_e, 1);
    ok = ok && df_write_contents("lxempty.font", FCH_ID, NULL, 0);

    df_old_fonts = Lock((STRPTR)"FONTS:", SHARED_LOCK);
    l = Lock((STRPTR)DF_DIR, SHARED_LOCK);
    df_dirlock = Lock((STRPTR)DF_DIR, SHARED_LOCK);
    if (!l || !AssignLock((STRPTR)"FONTS", l)) {
        if (l)
            UnLock(l);
        ok = FALSE;
    }
    P_BOOL("setup", ok);
    return ok;
}

/* ------------------------------------------------------------------ */
/* printing                                                            */

static BOOL df_is_lx(const char *name)
{
    return name && name[0] == 'l' && name[1] == 'x';
}

static void df_flags(const char *label, LONG v)
{
    P_HEX(label, (ULONG)v);
}

static void df_tags(const char *label, struct TagItem *t)
{
    int n = 0;
    probe_s(label);
    probe_s(" =");
    while (t && n < 8) {
        probe_ch(' ');
        probe_hex(t->ti_Tag, 8);
        probe_ch(':');
        probe_hex(t->ti_Data, 8);
        if (t->ti_Tag == TAG_DONE)
            break;
        t++;
        n++;
    }
    probe_ch('\n');
}

static void df_lbl(char *buf, const char *a, const char *b)
{
    strcpy(buf, a);
    strcat(buf, b);
}

/* metrics only (fonts whose glyphs are not ours, e.g. ROM topaz) */
static void df_metrics(const char *p, struct TextFont *tf)
{
    char l[64];
    df_lbl(l, p, ".ysize");     P_LONG(l, tf->tf_YSize);
    df_lbl(l, p, ".xsize");     P_LONG(l, tf->tf_XSize);
    df_lbl(l, p, ".style");     df_flags(l, tf->tf_Style);
    df_lbl(l, p, ".flags");     df_flags(l, tf->tf_Flags);
    df_lbl(l, p, ".baseline");  P_LONG(l, tf->tf_Baseline);
    df_lbl(l, p, ".boldsmear"); P_LONG(l, tf->tf_BoldSmear);
    df_lbl(l, p, ".lo");        P_LONG(l, tf->tf_LoChar);
    df_lbl(l, p, ".hi");        P_LONG(l, tf->tf_HiChar);
    df_lbl(l, p, ".space");     P_NULL(l, tf->tf_CharSpace);
    df_lbl(l, p, ".kern");      P_NULL(l, tf->tf_CharKern);
}

static void df_font(const char *p, struct TextFont *tf, BOOL ours)
{
    char l[64];
    WORD n;
    if (!tf) {
        P_NULL(p, tf);
        return;
    }
    df_lbl(l, p, ".name"); P_STR(l, tf->tf_Message.mn_Node.ln_Name);
    df_metrics(p, tf);
    if (!ours)
        return;
    n = (WORD)(tf->tf_HiChar - tf->tf_LoChar + 2);
    df_lbl(l, p, ".type");      P_LONG(l, tf->tf_Message.mn_Node.ln_Type);
    df_lbl(l, p, ".accessors"); P_LONG(l, tf->tf_Accessors);
    df_lbl(l, p, ".modulo");    P_LONG(l, tf->tf_Modulo);
    df_lbl(l, p, ".data");      P_HEX(l, probe_hash(tf->tf_CharData, (LONG)tf->tf_Modulo * tf->tf_YSize));
    df_lbl(l, p, ".loc");       P_HEX(l, probe_hash(tf->tf_CharLoc, (LONG)n * 4));
    if (tf->tf_CharSpace) {
        df_lbl(l, p, ".spacetab"); P_HEX(l, probe_hash(tf->tf_CharSpace, (LONG)n * 2));
    }
    if (tf->tf_CharKern) {
        df_lbl(l, p, ".kerntab");  P_HEX(l, probe_hash(tf->tf_CharKern, (LONG)n * 2));
    }
    if (tf->tf_Style & FSF_COLORFONT) {
        struct ColorTextFont *ctf = (struct ColorTextFont *)tf;
        WORD i;
        df_lbl(l, p, ".ctf_flags"); df_flags(l, ctf->ctf_Flags);
        df_lbl(l, p, ".ctf_depth"); P_LONG(l, ctf->ctf_Depth);
        df_lbl(l, p, ".ctf_fg");    P_LONG(l, ctf->ctf_FgColor);
        df_lbl(l, p, ".ctf_low");   P_LONG(l, ctf->ctf_Low);
        df_lbl(l, p, ".ctf_high");  P_LONG(l, ctf->ctf_High);
        df_lbl(l, p, ".ctf_pick");  P_LONG(l, ctf->ctf_PlanePick);
        df_lbl(l, p, ".ctf_onoff"); P_LONG(l, ctf->ctf_PlaneOnOff);
        df_lbl(l, p, ".ctf_colors"); P_NULL(l, ctf->ctf_ColorFontColors);
        if (ctf->ctf_ColorFontColors) {
            df_lbl(l, p, ".ctf_count"); P_LONG(l, ctf->ctf_ColorFontColors->cfc_Count);
        }
        for (i = 0; i < ctf->ctf_Depth && i < 8; i++) {
            char m[64];
            df_lbl(m, p, ".plane0");
            m[strlen(m) - 1] = (char)('0' + i);
            if (ctf->ctf_CharData[i])
                P_HEX(m, probe_hash(ctf->ctf_CharData[i], (LONG)tf->tf_Modulo * tf->tf_YSize));
            else
                P_NULL(m, NULL);
        }
        df_lbl(l, p, ".plane0_is_chardata"); P_BOOL(l, ctf->ctf_CharData[0] == tf->tf_CharData);
    }
    df_lbl(l, p, ".extension"); P_NULL(l, tf->tf_Extension);
    if (tf->tf_Extension) {
        struct TextFontExtension *tfe = (struct TextFontExtension *)tf->tf_Extension;
        df_lbl(l, p, ".tfe_match"); df_flags(l, tfe->tfe_MatchWord);
        df_lbl(l, p, ".tfe_flags0"); df_flags(l, tfe->tfe_Flags0);
        df_lbl(l, p, ".tfe_backptr"); P_BOOL(l, tfe->tfe_BackPtr == tf);
        df_lbl(l, p, ".tfe_tags"); P_NULL(l, tfe->tfe_Tags);
        if (tfe->tfe_Tags) {
            df_lbl(l, p, ".tags"); df_tags(l, tfe->tfe_Tags);
        }
    }
    if (tf->tf_Flags & FPF_DISKFONT) {
        struct DiskFontHeader *dfh = (struct DiskFontHeader *)((UBYTE *)tf - 54);
        df_lbl(l, p, ".dfh_id");       df_flags(l, dfh->dfh_FileID);
        df_lbl(l, p, ".dfh_rev");      P_LONG(l, dfh->dfh_Revision);
        df_lbl(l, p, ".dfh_name");     P_STR(l, (char *)dfh->dfh_Name);
        df_lbl(l, p, ".dfh_lnname");   P_BOOL(l, dfh->dfh_DF.ln_Name == (char *)dfh->dfh_Name);
        df_lbl(l, p, ".dfh_lntype");   P_LONG(l, dfh->dfh_DF.ln_Type);
        df_lbl(l, p, ".dfh_lnpri");    P_LONG(l, dfh->dfh_DF.ln_Pri);
        df_lbl(l, p, ".dfh_ln");       P_STR(l, dfh->dfh_DF.ln_Name);
        df_lbl(l, p, ".dfh_seg");      P_NULL(l, (APTR)dfh->dfh_Segment);
        df_lbl(l, p, ".tfname_is_dfhname"); P_BOOL(l, tf->tf_Message.mn_Node.ln_Name == (char *)dfh->dfh_Name);
    }
}

/* full table dump, for small scaled fonts */
static void df_dump(const char *p, struct TextFont *tf)
{
    char l[64];
    WORD n, y;
    if (!tf)
        return;
    n = (WORD)(tf->tf_HiChar - tf->tf_LoChar + 2);
    df_lbl(l, p, ".locs");  P_BYTES(l, tf->tf_CharLoc, (LONG)n * 4);
    if (tf->tf_CharSpace) {
        df_lbl(l, p, ".spaces"); P_BYTES(l, tf->tf_CharSpace, (LONG)n * 2);
    }
    if (tf->tf_CharKern) {
        df_lbl(l, p, ".kerns");  P_BYTES(l, tf->tf_CharKern, (LONG)n * 2);
    }
    for (y = 0; y < (WORD)tf->tf_YSize; y++) {
        char m[64];
        df_lbl(m, p, ".row");
        probe_s(m);
        probe_dec(y);
        probe_s(" =");
        {
            const UBYTE *b = (const UBYTE *)tf->tf_CharData + (LONG)y * tf->tf_Modulo;
            WORD i;
            static const char hx[] = "0123456789abcdef";
            for (i = 0; i < tf->tf_Modulo; i++) {
                probe_ch(' ');
                probe_ch(hx[b[i] >> 4]);
                probe_ch(hx[b[i] & 15]);
            }
        }
        probe_ch('\n');
    }
}

static int df_open_libs(void)
{
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 36);
    if (!GfxBase || !DiskfontBase) {
        if (DiskfontBase)
            CloseLibrary(DiskfontBase);
        if (GfxBase)
            CloseLibrary((struct Library *)GfxBase);
        return 20;
    }
    return 0;
}

static void df_close_libs(void)
{
    CloseLibrary(DiskfontBase);
    CloseLibrary((struct Library *)GfxBase);
}

#endif
