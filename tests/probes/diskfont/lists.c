/*
 * Probe (Phase 222f): diskfont.library AvailFonts() (AFF_DISK, AFF_MEMORY,
 * AFF_SCALED, AFF_TAGGED, buffer-too-small results) and
 * NewFontContents()/DisposeFontContents(), on the probe's own fonts
 * (dfprobe.h).  Entries are printed sorted, because the order of a
 * directory scan depends on the file system; memory entries are limited
 * to the probe's fonts and ROM topaz 8.
 */
#include "dfprobe.h"

#define BUFSZ 8192

struct row {
    UWORD type;
    const char *name;
    UWORD ysize;
    UBYTE style, flags;
    struct TagItem *tags;
};

static struct row rows[64];

static int cmp_row(const struct row *a, const struct row *b)
{
    int c = strcmp(a->name, b->name);
    if (c)
        return c;
    if (a->ysize != b->ysize)
        return a->ysize < b->ysize ? -1 : 1;
    if (a->type != b->type)
        return a->type < b->type ? -1 : 1;
    if (a->style != b->style)
        return a->style < b->style ? -1 : 1;
    return a->flags < b->flags ? -1 : a->flags > b->flags;
}

static void print_avail(const char *label, struct AvailFontsHeader *afh, ULONG flags, BOOL memfilter)
{
    UWORD i, n = 0, j;
    char l[64];
    UBYTE *e = (UBYTE *)(afh + 1);
    for (i = 0; i < afh->afh_NumEntries; i++) {
        struct row r;
        if (flags & AFF_TAGGED) {
            struct TAvailFonts *t = (struct TAvailFonts *)e;
            r.type = t->taf_Type;
            r.name = (const char *)t->taf_Attr.tta_Name;
            r.ysize = t->taf_Attr.tta_YSize;
            r.style = t->taf_Attr.tta_Style;
            r.flags = t->taf_Attr.tta_Flags;
            r.tags = t->taf_Attr.tta_Tags;
            e += sizeof(struct TAvailFonts);
        } else {
            struct AvailFonts *a = (struct AvailFonts *)e;
            r.type = a->af_Type;
            r.name = (const char *)a->af_Attr.ta_Name;
            r.ysize = a->af_Attr.ta_YSize;
            r.style = a->af_Attr.ta_Style;
            r.flags = a->af_Attr.ta_Flags;
            r.tags = NULL;
            e += sizeof(struct AvailFonts);
        }
        if (memfilter && (r.type & AFF_MEMORY) &&
            !df_is_lx(r.name) && !(strcmp(r.name, "topaz.font") == 0 && r.ysize == 8))
            continue;
        if (n < 64) {
            for (j = n; j > 0 && cmp_row(&rows[j - 1], &r) > 0; j--)
                rows[j] = rows[j - 1];
            rows[j] = r;
            n++;
        }
    }
    if (!memfilter) {
        df_lbl(l, label, ".num");
        P_LONG(l, afh->afh_NumEntries);
    }
    for (i = 0; i < n; i++) {
        probe_s(label);
        probe_s(": type=");
        probe_hex(rows[i].type, 4);
        probe_s(" name=\"");
        probe_s(rows[i].name);
        probe_s("\" ysize=");
        probe_dec(rows[i].ysize);
        probe_s(" style=");
        probe_hex(rows[i].style, 2);
        probe_s(" flags=");
        probe_hex(rows[i].flags, 2);
        if (flags & AFF_TAGGED) {
            probe_s(" tags=");
            if (rows[i].tags) {
                struct TagItem *t = rows[i].tags;
                int k;
                for (k = 0; k < 4; k++, t++) {
                    probe_hex(t->ti_Tag, 8);
                    probe_ch(':');
                    probe_hex(t->ti_Data, 8);
                    probe_ch(' ');
                    if (t->ti_Tag == TAG_DONE)
                        break;
                }
            } else
                probe_s("NULL");
        }
        probe_ch('\n');
    }
}

static UBYTE *buf;

static LONG avail(const char *label, ULONG flags, LONG size, BOOL show, BOOL memfilter)
{
    LONG r, i;
    char l[64];
    /* zero: AmigaOS 3.1 leaves tta_Tags of FCH_ID disk entries untouched */
    for (i = 0; i < BUFSZ; i++)
        buf[i] = 0;
    ((struct AvailFontsHeader *)buf)->afh_NumEntries = 0x7777;
    r = AvailFonts((STRPTR)buf, size, (LONG)flags);
    df_lbl(l, label, ".result");
    if (!memfilter || r == 0)
        P_LONG(l, r);
    else
        P_BOOL(l, r > 0);
    if (r == 0 && show)
        print_avail(label, (struct AvailFontsHeader *)buf, flags, memfilter);
    else if (r != 0 && !memfilter) {
        df_lbl(l, label, ".untouched");
        P_HEX(l, ((struct AvailFontsHeader *)buf)->afh_NumEntries);
    }
    return r;
}

static void contents(const char *label, const char *name)
{
    struct FontContentsHeader *fch;
    char l[64];
    UWORD i, j, n;
    static struct TFontContents *sorted[16];

    SetIoErr(1234);
    fch = NewFontContents(df_dirlock, (STRPTR)name);
    df_lbl(l, label, ".result");
    P_NULL(l, fch);
    if (!fch) {
        df_lbl(l, label, ".ioerr");
        P_LONG(l, IoErr());
        return;
    }
    df_lbl(l, label, ".id");  P_HEX(l, fch->fch_FileID);
    df_lbl(l, label, ".num"); P_LONG(l, fch->fch_NumEntries);
    n = 0;
    for (i = 0; i < fch->fch_NumEntries && i < 16; i++) {
        struct TFontContents *t = (struct TFontContents *)((UBYTE *)(fch + 1) + (ULONG)i * sizeof(struct TFontContents));
        for (j = n; j > 0 && strcmp((char *)sorted[j - 1]->tfc_FileName, (char *)t->tfc_FileName) > 0; j--)
            sorted[j] = sorted[j - 1];
        sorted[j] = t;
        n++;
    }
    for (i = 0; i < n; i++) {
        struct TFontContents *t = sorted[i];
        probe_s(label);
        probe_s(": file=\"");
        probe_s((char *)t->tfc_FileName);
        probe_s("\" ysize=");
        probe_dec(t->tfc_YSize);
        probe_s(" style=");
        probe_hex(t->tfc_Style, 2);
        probe_s(" flags=");
        probe_hex(t->tfc_Flags, 2);
        if (fch->fch_FileID == TFCH_ID) {
            probe_s(" tagcount=");
            probe_dec(t->tfc_TagCount);
            if (t->tfc_TagCount && t->tfc_TagCount <= 8) {
                struct TagItem *tg = (struct TagItem *)&t->tfc_FileName[MAXFONTPATH - t->tfc_TagCount * sizeof(struct TagItem)];
                UWORD k;
                probe_s(" tags=");
                for (k = 0; k < t->tfc_TagCount; k++) {
                    probe_hex(tg[k].ti_Tag, 8);
                    probe_ch(':');
                    probe_hex(tg[k].ti_Data, 8);
                    probe_ch(' ');
                }
            }
        }
        probe_ch('\n');
    }
    DisposeFontContents(fch);
}

int main(void)
{
    LONG need, r;
    struct TextFont *f;
    struct TextAttr ta;

    if (df_open_libs())
        return 20;
    buf = AllocVec(BUFSZ, MEMF_PUBLIC);
    if (!buf) {
        df_close_libs();
        return 20;
    }
    if (!df_setup()) {
        df_cleanup();
        FreeVec(buf);
        df_close_libs();
        return 0;
    }

    P_SECTION("AvailFonts disk");
    avail("disk", AFF_DISK, BUFSZ, TRUE, FALSE);
    avail("disk_scaled", AFF_DISK | AFF_SCALED, BUFSZ, TRUE, FALSE);
    avail("disk_tagged", AFF_DISK | AFF_TAGGED, BUFSZ, TRUE, FALSE);
    avail("disk_bitmap", AFF_DISK | AFF_BITMAP, BUFSZ, TRUE, FALSE);
    avail("none", 0, BUFSZ, TRUE, FALSE);

    P_SECTION("AvailFonts small buffers");
    need = avail("disk_0", AFF_DISK, 0, FALSE, FALSE);
    avail("disk_2", AFF_DISK, 2, FALSE, FALSE);
    avail("disk_20", AFF_DISK, 20, FALSE, FALSE);
    {
        LONG k;
        LONG hi = -1;
        /* the smallest buffer that succeeds */
        for (k = 2; k <= BUFSZ; k += 2) {
            r = AvailFonts((STRPTR)buf, k, AFF_DISK);
            if (r == 0) {
                hi = k;
                break;
            }
        }
        P_LONG("disk_min_ok", hi);
        P_LONG("disk_need", need);
        avail("disk_need", AFF_DISK, need, FALSE, FALSE);
        need = avail("tagged_0", AFF_DISK | AFF_TAGGED, 0, FALSE, FALSE);
        for (k = 2; k <= BUFSZ; k += 2) {
            r = AvailFonts((STRPTR)buf, k, AFF_DISK | AFF_TAGGED);
            if (r == 0) {
                hi = k;
                break;
            }
        }
        P_LONG("tagged_min_ok", hi);
    }

    /* every buffer size, with FONTS: holding one contents file (the
     * order of several files would depend on the file system) */
    {
        static const struct df_entry three[] = {
            { "lxthree/5", 5, 0, PF, 0 }, { "lxthree/6", 6, FSF_BOLD, PF, 0 }, { "lxthree/7", 7, 0, FPF_DESIGNED, 0 } };
        BPTR l = CreateDir((STRPTR)"T:lxdf/lxthree");
        BOOL ok;
        LONG k;
        ULONG fl;
        if (l)
            UnLock(l);
        ok = df_write_contents("lxthree/lxthree.font", FCH_ID, three, 3);
        l = Lock((STRPTR)"T:lxdf/lxthree", SHARED_LOCK);
        ok = ok && l && AssignLock((STRPTR)"FONTS", l);
        P_BOOL("three.setup", ok);
        for (fl = AFF_DISK; ok; fl = AFF_DISK | AFF_TAGGED) {
            for (k = 0; k <= 64; k++) {
                LONG i;
                for (i = 0; i < 80; i++)
                    buf[i] = 0;
                buf[0] = buf[1] = 0xa5;
                r = AvailFonts((STRPTR)buf, k, (LONG)fl);
                probe_s(fl & AFF_TAGGED ? "three_tagged " : "three ");
                probe_dec(k);
                probe_s(": result=");
                probe_dec(r);
                probe_s(" num=");
                probe_hex(*(UWORD *)buf, 4);
                /* where the names (and tags) went, as buffer offsets */
                if (k >= 2 && *(UWORD *)buf <= 3) {
                    UWORD e;
                    for (e = 0; e < *(UWORD *)buf; e++) {
                        UBYTE *ent = buf + 2 + e * (fl & AFF_TAGGED ? sizeof(struct TAvailFonts) : sizeof(struct AvailFonts));
                        UBYTE *nm = *(UBYTE **)(ent + 2);
                        probe_s(" name@");
                        probe_dec((LONG)(nm - buf));
                        if (fl & AFF_TAGGED) {
                            struct TagItem *tg = ((struct TAvailFonts *)ent)->taf_Attr.tta_Tags;
                            probe_s(" tags@");
                            if (tg)
                                probe_dec((LONG)((UBYTE *)tg - buf));
                            else
                                probe_s("NULL");
                        }
                    }
                }
                probe_ch('\n');
            }
            if (fl & AFF_TAGGED)
                break;
        }
        l = Lock((STRPTR)DF_DIR, SHARED_LOCK);
        P_BOOL("three.restore", l && AssignLock((STRPTR)"FONTS", l));
        DeleteFile((STRPTR)"T:lxdf/lxthree/lxthree.font");
        DeleteFile((STRPTR)"T:lxdf/lxthree");
    }

    P_SECTION("AvailFonts memory");
    avail("mem_before", AFF_MEMORY, BUFSZ, TRUE, TRUE);
    ta.ta_Name = (STRPTR)"lxprop.font";
    ta.ta_YSize = 8;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    f = OpenDiskFont(&ta);
    P_NULL("open8", f);
    ta.ta_YSize = 16;
    {
        struct TextFont *f16 = OpenDiskFont(&ta);
        P_NULL("open16", f16);
        ta.ta_Name = (STRPTR)"lxtag.font";
        ta.ta_YSize = 9;
        {
            struct TextFont *t9 = OpenDiskFont(&ta);
            P_NULL("opentag9", t9);
            avail("mem", AFF_MEMORY, BUFSZ, TRUE, TRUE);
            avail("mem_scaled", AFF_MEMORY | AFF_SCALED, BUFSZ, TRUE, TRUE);
            avail("mem_tagged", AFF_MEMORY | AFF_SCALED | AFF_TAGGED, BUFSZ, TRUE, TRUE);
            avail("all", AFF_MEMORY | AFF_DISK | AFF_SCALED, BUFSZ, TRUE, TRUE);
            avail("mem_0", AFF_MEMORY, 0, FALSE, TRUE);
            if (t9)
                CloseFont(t9);
        }
        if (f16)
            CloseFont(f16);
    }
    if (f)
        CloseFont(f);
    avail("mem_closed", AFF_MEMORY | AFF_SCALED, BUFSZ, TRUE, TRUE);

    P_SECTION("NewFontContents");
    contents("prop", "lxprop.font");
    contents("fixed", "lxfixed.font");
    contents("tag", "lxtag.font");
    contents("color", "lxcolor.font");
    contents("empty", "lxempty.font");
    contents("broken", "lxbroken.font");
    contents("none", "lxnone.font");
    contents("nosuffix", "lxprop");
    contents("upper", "LXPROP.FONT");

    df_cleanup();
    FreeVec(buf);
    df_close_libs();
    return 0;
}
