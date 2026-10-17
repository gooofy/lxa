/*
 * Probe (Phase 241): the RTG part of the display database - every mode the
 * uaegfx board adds (Picasso96 2.0, `rtg` reference profile) with its
 * DisplayInfo, DimensionInfo, MonitorInfo and NameInfo, and how
 * FindDisplayInfo/ModeNotAvailable/BestModeIDA/OpenMonitor treat them.
 *
 * rdd: profile rtg
 */
#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <graphics/monitor.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include "probe.h"

extern struct GfxBase *GfxBase;

static void rect(const char *n, struct Rectangle *r)
{
    probe_s(" ");
    probe_s(n);
    probe_s("=");
    probe_dec(r->MinX);
    probe_ch(',');
    probe_dec(r->MinY);
    probe_ch(',');
    probe_dec(r->MaxX);
    probe_ch(',');
    probe_dec(r->MaxY);
}

static void pt(const char *n, Point *p)
{
    probe_s(" ");
    probe_s(n);
    probe_s("=");
    probe_dec(p->x);
    probe_ch(',');
    probe_dec(p->y);
}

static int is_rtg(ULONG id)
{
    return (id & 0xF0000000UL) != 0;
}

static void dump(ULONG id)
{
    struct DisplayInfo di;
    struct DimensionInfo dims;
    struct MonitorInfo mi;
    struct NameInfo ni;
    DisplayInfoHandle h = FindDisplayInfo(id);
    ULONG n;

    probe_hex(id, 8);
    probe_s(h ? " handle" : " nohandle");
    probe_s(" avail=");
    probe_hex(ModeNotAvailable(id), 8);
    probe_ch('\n');

    n = GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP, id);
    probe_s("  disp n=");
    probe_dec(n);
    if (n >= sizeof(struct QueryHeader)) {
        probe_s(" len=");
        probe_dec(di.Header.Length);
        probe_s(" props=");
        probe_hex(di.PropertyFlags, 8);
        probe_s(" na=");
        probe_dec(di.NotAvailable);
        pt("res", &di.Resolution);
        probe_s(" ticks=");
        probe_dec(di.PixelSpeed);
        probe_s(" spr=");
        probe_dec(di.NumStdSprites);
        probe_s(" pal=");
        probe_dec(di.PaletteRange);
        pt("sprres", &di.SpriteResolution);
        probe_s(" bits=");
        probe_dec(di.RedBits);
        probe_ch(',');
        probe_dec(di.GreenBits);
        probe_ch(',');
        probe_dec(di.BlueBits);
    }
    probe_ch('\n');

    n = GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, id);
    probe_s("  dims n=");
    probe_dec(n);
    if (n >= sizeof(struct QueryHeader)) {
        probe_s(" len=");
        probe_dec(dims.Header.Length);
        probe_s(" depth=");
        probe_dec(dims.MaxDepth);
        probe_s(" raster=");
        probe_dec(dims.MinRasterWidth);
        probe_ch(',');
        probe_dec(dims.MinRasterHeight);
        probe_ch('-');
        probe_dec(dims.MaxRasterWidth);
        probe_ch(',');
        probe_dec(dims.MaxRasterHeight);
        rect("nom", &dims.Nominal);
        rect("max", &dims.MaxOScan);
        rect("vid", &dims.VideoOScan);
        rect("txt", &dims.TxtOScan);
        rect("std", &dims.StdOScan);
    }
    probe_ch('\n');

    n = GetDisplayInfoData(NULL, (UBYTE *)&mi, sizeof(mi), DTAG_MNTR, id);
    probe_s("  mntr n=");
    probe_dec(n);
    if (n >= sizeof(struct QueryHeader)) {
        probe_s(" len=");
        probe_dec(mi.Header.Length);
        probe_s(mi.Mspc ? " mspc" : " nomspc");
        if (mi.Mspc && mi.Mspc->ms_Node.xln_Name) {
            probe_s("=");
            probe_s(mi.Mspc->ms_Node.xln_Name);
        }
        pt("pos", &mi.ViewPosition);
        pt("res", &mi.ViewResolution);
        rect("range", &mi.ViewPositionRange);
        probe_s(" rows=");
        probe_dec(mi.TotalRows);
        probe_s(" clocks=");
        probe_dec(mi.TotalColorClocks);
        probe_s(" minrow=");
        probe_dec(mi.MinRow);
        probe_s(" compat=");
        probe_dec(mi.Compatibility);
        pt("ticks", &mi.MouseTicks);
        pt("defpos", &mi.DefaultViewPosition);
        probe_s(" pref=");
        probe_hex(mi.PreferredModeID, 8);
    }
    probe_ch('\n');

    n = GetDisplayInfoData(NULL, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, id);
    probe_s("  name n=");
    probe_dec(n);
    if (n >= sizeof(struct QueryHeader)) {
        probe_s(" \"");
        probe_s((char *)ni.Name);
        probe_ch('"');
    }
    probe_ch('\n');
}

static void best(const char *label, struct TagItem *tags)
{
    probe_s("BestModeID ");
    probe_s(label);
    probe_s(" = ");
    probe_hex(BestModeIDA(tags), 8);
    probe_ch('\n');
}

int main(void)
{
    ULONG id = INVALID_ID;
    LONG total = 0, rtg = 0;

    P_SECTION("modes");
    while ((id = NextDisplayInfo(id)) != INVALID_ID) {
        total++;
        if (is_rtg(id)) {
            rtg++;
            dump(id);
        }
    }
    P_LONG("total", total);
    P_LONG("rtg", rtg);

    P_SECTION("unlisted ids");
    dump(0x50041200);       /* a format the board does not have */
    dump(0x50ff0000);
    dump(0x50000000);

    P_SECTION("MonitorList");
    {
        struct Node *n;
        Forbid();
        for (n = GfxBase->MonitorList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
            probe_s(n->ln_Name ? n->ln_Name : "(null)");
            probe_s(" total_rows=");
            probe_dec(((struct MonitorSpec *)n)->total_rows);
            probe_s(" total_colorclocks=");
            probe_dec(((struct MonitorSpec *)n)->total_colorclocks);
            probe_s(" flags=");
            probe_hex(((struct MonitorSpec *)n)->ms_Flags, 4);
            probe_ch('\n');
        }
        Permit();
    }

    P_SECTION("OpenMonitor");
    {
        struct MonitorSpec *ms = OpenMonitor(NULL, 0x50041000);
        P_NULL("OpenMonitor(NULL, 0x50041000)", ms);
        if (ms) {
            P_STR("name", ms->ms_Node.xln_Name);
            CloseMonitor(ms);
        }
    }

    P_SECTION("BestModeIDA");
    {
        struct TagItem t1[] = {{BIDTAG_Depth, 16}, {TAG_DONE, 0}};
        struct TagItem t2[] = {{BIDTAG_Depth, 24}, {TAG_DONE, 0}};
        struct TagItem t3[] = {{BIDTAG_Depth, 24}, {BIDTAG_NominalWidth, 800}, {BIDTAG_NominalHeight, 600}, {TAG_DONE, 0}};
        struct TagItem t4[] = {{BIDTAG_Depth, 8}, {BIDTAG_NominalWidth, 1024}, {BIDTAG_NominalHeight, 768}, {TAG_DONE, 0}};
        struct TagItem t5[] = {{BIDTAG_Depth, 8}, {BIDTAG_DesiredWidth, 640}, {BIDTAG_DesiredHeight, 480}, {TAG_DONE, 0}};
        struct TagItem t6[] = {{BIDTAG_Depth, 15}, {BIDTAG_NominalWidth, 640}, {BIDTAG_NominalHeight, 480}, {TAG_DONE, 0}};
        struct TagItem t7[] = {{BIDTAG_Depth, 32}, {TAG_DONE, 0}};
        struct TagItem t8[] = {{BIDTAG_MonitorID, 0x50000000}, {TAG_DONE, 0}};
        struct TagItem t9[] = {{BIDTAG_SourceID, 0x50031100}, {TAG_DONE, 0}};
        struct TagItem t10[] = {{BIDTAG_Depth, 4}, {BIDTAG_NominalWidth, 800}, {BIDTAG_NominalHeight, 600}, {TAG_DONE, 0}};
        struct TagItem t11[] = {{BIDTAG_DIPFMustHave, DIPF_IS_FOREIGN}, {TAG_DONE, 0}};
        struct TagItem t12[] = {{BIDTAG_Depth, 9}, {BIDTAG_NominalWidth, 320}, {BIDTAG_NominalHeight, 240}, {TAG_DONE, 0}};
        best("depth16", t1);
        best("depth24", t2);
        best("depth24 800x600", t3);
        best("depth8 1024x768", t4);
        best("depth8 desired 640x480", t5);
        best("depth15 640x480", t6);
        best("depth32", t7);
        best("monitor 0x50000000", t8);
        best("source 0x50031100", t9);
        best("depth4 800x600", t10);
        best("must foreign", t11);
        best("depth9 320x240", t12);
    }
    return 0;
}
