/*
 * Probe (Phase 222f): diskfont.library OpenDiskFont() and
 * NewScaledDiskFont() - exact sizes, missing sizes (bitmap scaling),
 * styles, FPF_DESIGNED, TA_DeviceDPI tags, tagged and colour fonts,
 * broken contents files, CloseFont().  Uses the probe's own fonts
 * (dfprobe.h); ROM topaz is checked by metrics only.
 */
#include "dfprobe.h"

#define MAXOPEN 64
static struct TextFont *opened[MAXOPEN];
static int nopened;
static struct TextFont *f8, *f11, *fx;

static struct TextFont *df_open(const char *label, const char *name, UWORD ysize,
                                UBYTE style, UBYTE flags, struct TagItem *tags)
{
    struct TTextAttr ta;
    struct TextFont *tf;
    BOOL ours = df_is_lx(name);
    ta.tta_Name = (STRPTR)name;
    ta.tta_YSize = ysize;
    ta.tta_Style = (UBYTE)(style | (tags ? FSF_TAGGED : 0));
    ta.tta_Flags = flags;
    ta.tta_Tags = tags;
    tf = OpenDiskFont((struct TextAttr *)&ta);
    df_font(label, tf, ours);
    if (tf && nopened < MAXOPEN)
        opened[nopened++] = tf;
    return tf;
}

int main(void)
{
    struct TextFont *f16, *t;
    struct TagItem dpi[2];
    int i;

    if (df_open_libs())
        return 20;
    if (!df_setup()) {
        df_cleanup();
        df_close_libs();
        return 0;
    }

    P_SECTION("exact sizes");
    f8 = df_open("prop8", "lxprop.font", 8, 0, 0, NULL);
    f11 = df_open("prop11", "lxprop.font", 11, 0, 0, NULL);
    fx = df_open("fixed10", "lxfixed.font", 10, 0, 0, NULL);
    df_open("tag9", "lxtag.font", 9, 0, 0, NULL);
    df_open("tag6", "lxtag.font", 6, 0, 0, NULL);
    df_open("color7", "lxcolor.font", 7, 0, 0, NULL);

    P_SECTION("reopen");
    t = df_open("prop8b", "lxprop.font", 8, 0, 0, NULL);
    P_BOOL("prop8b.same", t == f8);
    t = df_open("prop8c", "lxprop.font", 8, FSF_BOLD, 0, NULL);
    P_BOOL("prop8c.same", t == f8);
    t = df_open("prop8d", "lxprop.font", 8, 0, FPF_DESIGNED, NULL);
    P_BOOL("prop8d.same", t == f8);
    t = df_open("prop8e", "lxprop.font", 8, 0, FPF_PROPORTIONAL | FPF_DISKFONT, NULL);
    P_BOOL("prop8e.same", t == f8);
    t = df_open("prop8f", "LXPROP.FONT", 8, 0, 0, NULL);
    P_BOOL("prop8f.same", t == f8);
    t = df_open("prop8g", "lxprop", 8, 0, 0, NULL);
    t = df_open("prop8h", DF_DIR "/lxprop.font", 8, 0, 0, NULL);
    P_BOOL("prop8h.same", t == f8);

    P_SECTION("missing sizes");
    f16 = df_open("prop16", "lxprop.font", 16, 0, 0, NULL);
    t = df_open("prop16b", "lxprop.font", 16, 0, 0, NULL);
    P_BOOL("prop16b.same", t == f16);
    df_open("prop4", "lxprop.font", 4, 0, 0, NULL);
    df_open("prop5", "lxprop.font", 5, 0, 0, NULL);
    df_open("prop9", "lxprop.font", 9, 0, 0, NULL);
    df_open("prop10", "lxprop.font", 10, 0, 0, NULL);
    df_open("prop12", "lxprop.font", 12, 0, 0, NULL);
    df_open("prop22", "lxprop.font", 22, 0, 0, NULL);
    df_open("prop1", "lxprop.font", 1, 0, 0, NULL);
    df_open("prop0", "lxprop.font", 0, 0, 0, NULL);
    df_open("prop13d", "lxprop.font", 13, 0, FPF_DESIGNED, NULL);
    df_open("prop14b", "lxprop.font", 14, FSF_BOLD | FSF_ITALIC, 0, NULL);
    df_open("fixed20", "lxfixed.font", 20, 0, 0, NULL);
    df_open("fixed5", "lxfixed.font", 5, 0, 0, NULL);
    df_open("fixed15", "lxfixed.font", 15, 0, 0, NULL);
    df_open("tag18", "lxtag.font", 18, 0, 0, NULL);
    df_open("tag7", "lxtag.font", 7, 0, 0, NULL);
    df_open("color14", "lxcolor.font", 14, 0, 0, NULL);

    P_SECTION("device dpi");
    dpi[0].ti_Tag = TA_DeviceDPI;
    dpi[1].ti_Tag = TAG_DONE;
    dpi[1].ti_Data = 0;
    dpi[0].ti_Data = 0x00640064;
    df_open("dpi8_100x100", "lxprop.font", 8, 0, 0, dpi);
    dpi[0].ti_Data = 0x00640032;
    df_open("dpi8_100x50", "lxprop.font", 8, 0, 0, dpi);
    df_open("dpi9_100x50", "lxtag.font", 9, 0, 0, dpi);
    dpi[0].ti_Data = 0x00320064;
    df_open("dpi8_50x100", "lxprop.font", 8, 0, 0, dpi);
    df_open("dpi9_50x100", "lxtag.font", 9, 0, 0, dpi);
    df_open("dpi10_50x100", "lxfixed.font", 10, 0, 0, dpi);
    dpi[0].ti_Data = 0x00c80064;
    df_open("dpi16_200x100", "lxprop.font", 16, 0, 0, dpi);
    dpi[0].ti_Data = 0x00640032;
    df_open("dpi16_100x50", "lxprop.font", 16, 0, 0, dpi);
    df_open("dpi16_100x50b", "lxprop.font", 16, 0, 0, dpi);

    P_SECTION("failures");
    df_open("missing", "lxnone.font", 8, 0, 0, NULL);
    df_open("broken12", "lxbroken.font", 12, 0, 0, NULL);
    df_open("broken8", "lxbroken.font", 8, 0, 0, NULL);
    df_open("empty8", "lxempty.font", 8, 0, 0, NULL);

    P_SECTION("rom topaz");
    df_open("topaz8", "topaz.font", 8, 0, 0, NULL);
    df_open("topaz16", "topaz.font", 16, 0, 0, NULL);
    df_open("topaz11d", "topaz.font", 11, 0, FPF_DESIGNED, NULL);

    P_SECTION("NewScaledDiskFont");
    {
        static const struct { const char *label; struct TextFont **src; UWORD ysize; UBYTE style; ULONG dpi; } sc[] = {
            { "s8to16", &f8, 16, 0, 0 },
            { "s8to6", &f8, 6, 0, 0 },
            { "s8to12", &f8, 12, 0, 0 },
            { "s11to7", &f11, 7, 0, 0 },
            { "s8to8", &f8, 8, 0, 0 },
            { "s8to16b", &f8, 16, FSF_BOLD, 0 },
            { "s10to20", &fx, 20, 0, 0 },
            { "s10to13", &fx, 13, 0, 0 },
            { "s8dpi", &f8, 8, 0, 0x00640032 },
            { "s8dpi2", &f8, 12, 0, 0x00320064 },
        };
        unsigned k;
        for (k = 0; k < sizeof(sc) / sizeof(sc[0]); k++) {
            struct TTextAttr ta;
            struct DiskFontHeader *dfh;
            struct TagItem tg[2];
            char l[64];
            tg[0].ti_Tag = TA_DeviceDPI;
            tg[0].ti_Data = sc[k].dpi;
            tg[1].ti_Tag = TAG_DONE;
            tg[1].ti_Data = 0;
            ta.tta_Name = (STRPTR)"lxscaled.font";
            ta.tta_YSize = sc[k].ysize;
            ta.tta_Style = (UBYTE)(sc[k].style | (sc[k].dpi ? FSF_TAGGED : 0));
            ta.tta_Flags = 0;
            ta.tta_Tags = sc[k].dpi ? tg : NULL;
            if (!*sc[k].src) {
                P_NULL(sc[k].label, NULL);
                continue;
            }
            dfh = NewScaledDiskFont(*sc[k].src, (struct TextAttr *)&ta);
            df_lbl(l, sc[k].label, ".result");
            P_NULL(l, dfh);
            if (!dfh)
                continue;
            df_lbl(l, sc[k].label, ".dfh_id");    df_flags(l, dfh->dfh_FileID);
            df_lbl(l, sc[k].label, ".dfh_seg");   P_NULL(l, (APTR)dfh->dfh_Segment);
            df_font(sc[k].label, &dfh->dfh_TF, TRUE);
            if (k < 2)
                df_dump(sc[k].label, &dfh->dfh_TF);
            StripFont(&dfh->dfh_TF);
            UnLoadSeg(dfh->dfh_Segment);
        }
    }

    P_SECTION("dumps");
    df_dump("prop8", f8);
    df_dump("prop16", f16);

    P_SECTION("close");
    for (i = nopened - 1; i >= 0; i--)
        CloseFont(opened[i]);
    P_LONG("prop8.accessors", f8 ? f8->tf_Accessors : -1);
    P_LONG("prop16.accessors", f16 ? f16->tf_Accessors : -1);

    df_cleanup();
    df_close_libs();
    return 0;
}
