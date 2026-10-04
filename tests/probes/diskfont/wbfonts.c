/*
 * Probe (Phase 225): the Workbench 3.1 disk fonts through OpenDiskFont() -
 * TextFont fields, the per-character CharLoc width / CharSpace / CharKern
 * tables, TextLength(), TextExtent() and Text() rendering (plain and in
 * the algorithmic styles) of label-like strings.
 *
 * rdd: fonts wb31
 * (the marker above makes `rdd suite-ref` run it on lxa with the reference
 * system's Workbench 3.1 fonts as FONTS:; skipped where they are missing)
 */
#include <string.h>
#include <graphics/text.h>
#include <diskfont/diskfont.h>
#include <clib/diskfont_protos.h>
#include <inline/diskfont.h>
#include "../graphics/gfxprobe.h"

struct Library *DiskfontBase;
static struct gp_bm g;
static struct RastPort rp;

static const char *const strs[] = {
    "Normal", "Under", "Selected", "Disabled", "Cyc Off", "Str Off", "Off", "First",
    "Second", " 5", "Workbench", "AVAWAy", "jfff,;", "Label", "If you {j}"
};
#define NSTRS ((WORD)(sizeof(strs) / sizeof(strs[0])))

static const UBYTE styles[] = {FSF_BOLD, FSF_ITALIC, FSF_UNDERLINED,
                               FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED};

static void extent(struct TextExtent *te)
{
    probe_s(" ext ");
    probe_dec(te->te_Width);
    probe_ch(' ');
    probe_dec(te->te_Extent.MinX);
    probe_ch(',');
    probe_dec(te->te_Extent.MinY);
    probe_ch(' ');
    probe_dec(te->te_Extent.MaxX);
    probe_ch(',');
    probe_dec(te->te_Extent.MaxY);
}

static void one(const char *name, UWORD size)
{
    struct TextAttr ta;
    struct TextFont *tf;
    struct TextExtent te;
    WORD i, k, n, c;

    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = size;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    tf = OpenDiskFont(&ta);
    probe_s("-- ");
    probe_s(name);
    probe_ch(' ');
    probe_dec(size);
    probe_ch('\n');
    if (!tf) {
        probe_s("OpenDiskFont = NULL\n");
        return;
    }
    gp_verbose = gp_filter && gp_contains(name, gp_filter);
    P_LONG("tf_YSize", tf->tf_YSize);
    P_LONG("tf_XSize", tf->tf_XSize);
    P_LONG("tf_Baseline", tf->tf_Baseline);
    P_HEX("tf_Flags", tf->tf_Flags);
    P_HEX("tf_Style", tf->tf_Style);
    P_LONG("tf_BoldSmear", tf->tf_BoldSmear);
    P_LONG("tf_LoChar", tf->tf_LoChar);
    P_LONG("tf_HiChar", tf->tf_HiChar);
    P_NULL("tf_CharSpace", tf->tf_CharSpace);
    P_NULL("tf_CharKern", tf->tf_CharKern);
    n = (WORD)(tf->tf_HiChar - tf->tf_LoChar + 2);
    for (c = 0; c < n; c++) {
        if (c % 12 == 0) {
            probe_s("width/space/kern ");
            probe_dec(c + tf->tf_LoChar);
            probe_ch(':');
        }
        probe_ch(' ');
        probe_dec(((ULONG *)tf->tf_CharLoc)[c] & 0xffff);
        probe_ch('/');
        if (tf->tf_CharSpace)
            probe_dec(((WORD *)tf->tf_CharSpace)[c]);
        else
            probe_ch('-');
        probe_ch('/');
        if (tf->tf_CharKern)
            probe_dec(((WORD *)tf->tf_CharKern)[c]);
        else
            probe_ch('-');
        if (c % 12 == 11 || c == n - 1)
            probe_ch('\n');
    }

    for (k = -1; k < (WORD)sizeof(styles); k++) {
        for (i = 0; i < NSTRS; i++) {
            WORD len = (WORD)strlen(strs[i]);
            gp_clear(&g);
            gp_rp(&rp, &g);
            SetFont(&rp, tf);
            if (k >= 0)
                SetSoftStyle(&rp, styles[k], 0xff);
            TextExtent(&rp, (STRPTR)strs[i], len, &te);
            probe_s("style ");
            probe_dec(k >= 0 ? styles[k] : 0);
            probe_s(" \"");
            probe_s(strs[i]);
            probe_s("\" len ");
            probe_dec(TextLength(&rp, (STRPTR)strs[i], len));
            extent(&te);
            SetAPen(&rp, 1);
            SetDrMd(&rp, JAM1);
            Move(&rp, 8, tf->tf_Baseline + 1);   /* glyphs stay inside the bitmap */
            Text(&rp, (STRPTR)strs[i], len);
            probe_s(" cp_x ");
            probe_dec(rp.cp_x);
            gp_hash("", &g);
        }
    }
    CloseFont(tf);
}

int main(int argc, char **argv)
{
    gp_args(argc, argv);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 0);
    if (!DiskfontBase || !gp_alloc(&g, 144, 24, 1))
        return 20;
    one("helvetica.font", 13);
    one("helvetica.font", 11);
    one("times.font", 13);
    one("courier.font", 13);
    one("topaz.font", 11);
    one("garnet.font", 16);
    gp_free(&g);
    CloseLibrary(DiskfontBase);
    return 0;
}
