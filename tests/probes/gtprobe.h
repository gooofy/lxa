/*
 * gtprobe.h - CreateGadget() for every GadTools kind and the common flag
 * and tag variants, printing the resulting gadget list (Phase 222e).
 * The gadgets/<font>.c probes run it with one font each.
 */
#ifndef LXA_PROBE_GTPROBE_H
#define LXA_PROBE_GTPROBE_H

#include <exec/memory.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;
struct Library *DiskfontBase;

static APTR g_vi;
static struct TextAttr *g_ta;
static struct NewGadget g_ng;

static STRPTR cy_labels[] = { (STRPTR)"First", (STRPTR)"Second", (STRPTR)"Third", NULL };
static STRPTR mx_labels[] = { (STRPTR)"Alpha", (STRPTR)"Beta", (STRPTR)"Gamma", NULL };
static struct Node lv_nodes[5];
static struct List lv_list;
static const char *lv_names[5] = { "One", "Two", "Three", "Four", "Five" };

static void ng_reset(void)
{
    g_ng.ng_LeftEdge = 100;
    g_ng.ng_TopEdge = 40;
    g_ng.ng_Width = 120;
    g_ng.ng_Height = 14;
    g_ng.ng_GadgetText = (UBYTE *)"Label";
    g_ng.ng_TextAttr = g_ta;
    g_ng.ng_GadgetID = 7;
    g_ng.ng_Flags = 0;
    g_ng.ng_VisualInfo = g_vi;
    g_ng.ng_UserData = (APTR)0x1234;
}

/* create one gadget of `kind` on a fresh context and dump the list */
static void probe_kind(const char *name, ULONG kind, Tag tag1, ...)
{
    struct Gadget *glist = NULL, *ctx, *g, *x;
    int n, ret = -1;

    P_SECTION(name);
    ctx = CreateContext(&glist);
    if (!ctx)
    {
        probe_s("CreateContext failed\n");
        return;
    }
    g = CreateGadgetA(kind, ctx, &g_ng, (struct TagItem *)&tag1);
    if (!g)
        probe_s("result = NULL\n");
    else
    {
        for (n = 0, x = glist; x; x = x->NextGadget, n++)
            if (x == g)
                ret = n;
        P_LONG("returned index", ret);
        P_BOOL("returned UserData kept", g->UserData == (APTR)0x1234);
        P_BOOL("returned is last", g->NextGadget == NULL);
        dump_glist("", glist);
    }
    FreeGadgets(glist);
    ng_reset();
}

static void probe_all(void)
{
    struct Gadget *glist = NULL, *ctx, *g;
    struct Screen *scr = LockPubScreen(NULL);
    int i;

    if (!scr)
        return;
    g_vi = GetVisualInfo(scr, TAG_END);
    NewList(&lv_list);
    for (i = 0; i < 5; i++)
    {
        lv_nodes[i].ln_Name = (char *)lv_names[i];
        AddTail(&lv_list, &lv_nodes[i]);
    }
    ng_reset();

    P_SECTION("CreateContext");
    ctx = CreateContext(&glist);
    P_BOOL("glist == ctx", glist == ctx);
    if (ctx)
        dump_gadget("", 0, ctx);
    P_NULL("CreateGadget(prev NULL)", CreateGadget(BUTTON_KIND, NULL, &g_ng, TAG_END));
    FreeGadgets(glist);

    probe_kind("BUTTON", BUTTON_KIND, TAG_END);
    g_ng.ng_Flags = PLACETEXT_LEFT;
    probe_kind("BUTTON left", BUTTON_KIND, TAG_END);
    g_ng.ng_Flags = PLACETEXT_RIGHT | NG_HIGHLABEL;
    probe_kind("BUTTON right highlabel", BUTTON_KIND, TAG_END);
    g_ng.ng_GadgetText = (UBYTE *)"_Under";
    probe_kind("BUTTON underscore", BUTTON_KIND, GT_Underscore, '_', TAG_END);
    probe_kind("BUTTON disabled", BUTTON_KIND, GA_Disabled, TRUE, TAG_END);
    probe_kind("BUTTON immediate", BUTTON_KIND, GA_Immediate, TRUE, TAG_END);
    g_ng.ng_GadgetText = NULL;
    probe_kind("BUTTON notext", BUTTON_KIND, TAG_END);

    probe_kind("CHECKBOX", CHECKBOX_KIND, TAG_END);
    probe_kind("CHECKBOX checked", CHECKBOX_KIND, GTCB_Checked, TRUE, TAG_END);
    g_ng.ng_Flags = PLACETEXT_RIGHT;
    probe_kind("CHECKBOX right", CHECKBOX_KIND, TAG_END);
    g_ng.ng_Width = 30; g_ng.ng_Height = 20;
    probe_kind("CHECKBOX scaled", CHECKBOX_KIND, GTCB_Scaled, TRUE, TAG_END);
    g_ng.ng_Width = 10; g_ng.ng_Height = 5;
    probe_kind("CHECKBOX small", CHECKBOX_KIND, TAG_END);
    probe_kind("CHECKBOX disabled", CHECKBOX_KIND, GA_Disabled, TRUE, TAG_END);

    probe_kind("INTEGER", INTEGER_KIND, GTIN_Number, 42, TAG_END);
    probe_kind("INTEGER negative maxchars", INTEGER_KIND, GTIN_Number, -5, GTIN_MaxChars, 3, TAG_END);
    g_ng.ng_Flags = PLACETEXT_ABOVE;
    probe_kind("INTEGER above disabled", INTEGER_KIND, GA_Disabled, TRUE, TAG_END);
    probe_kind("INTEGER notabcycle", INTEGER_KIND, GA_TabCycle, FALSE, TAG_END);

    probe_kind("STRING", STRING_KIND, GTST_String, (ULONG)"abc", TAG_END);
    probe_kind("STRING maxchars", STRING_KIND, GTST_String, (ULONG)"abcdefgh", GTST_MaxChars, 5, TAG_END);
    g_ng.ng_Flags = PLACETEXT_RIGHT;
    probe_kind("STRING right", STRING_KIND, TAG_END);
    probe_kind("STRING disabled", STRING_KIND, GA_Disabled, TRUE, TAG_END);
    probe_kind("STRING justify", STRING_KIND, STRINGA_Justification, GACT_STRINGCENTER, TAG_END);

    probe_kind("MX", MX_KIND, GTMX_Labels, (ULONG)mx_labels, TAG_END);
    probe_kind("MX spacing active", MX_KIND, GTMX_Labels, (ULONG)mx_labels, GTMX_Spacing, 4,
               GTMX_Active, 2, TAG_END);
    g_ng.ng_Flags = PLACETEXT_LEFT;
    probe_kind("MX left", MX_KIND, GTMX_Labels, (ULONG)mx_labels, TAG_END);
    probe_kind("MX titleplace", MX_KIND, GTMX_Labels, (ULONG)mx_labels, GTMX_TitlePlace, PLACETEXT_ABOVE,
               TAG_END);
    g_ng.ng_Width = 20; g_ng.ng_Height = 12;
    probe_kind("MX scaled", MX_KIND, GTMX_Labels, (ULONG)mx_labels, GTMX_Scaled, TRUE, TAG_END);
    probe_kind("MX disabled", MX_KIND, GTMX_Labels, (ULONG)mx_labels, GA_Disabled, TRUE, TAG_END);
    probe_kind("MX nolabels", MX_KIND, TAG_END);

    probe_kind("CYCLE", CYCLE_KIND, GTCY_Labels, (ULONG)cy_labels, TAG_END);
    probe_kind("CYCLE active", CYCLE_KIND, GTCY_Labels, (ULONG)cy_labels, GTCY_Active, 1, TAG_END);
    g_ng.ng_Flags = PLACETEXT_RIGHT;
    probe_kind("CYCLE right disabled", CYCLE_KIND, GTCY_Labels, (ULONG)cy_labels, GA_Disabled, TRUE, TAG_END);

    probe_kind("SLIDER", SLIDER_KIND, GTSL_Min, 0, GTSL_Max, 20, GTSL_Level, 5, TAG_END);
    probe_kind("SLIDER level", SLIDER_KIND, GTSL_Min, -10, GTSL_Max, 10, GTSL_Level, 3,
               GTSL_LevelFormat, (ULONG)"%3ld", GTSL_MaxLevelLen, 3, TAG_END);
    probe_kind("SLIDER level right", SLIDER_KIND, GTSL_Max, 100, GTSL_Level, 50,
               GTSL_LevelFormat, (ULONG)"%ld", GTSL_MaxLevelLen, 3, GTSL_LevelPlace, PLACETEXT_RIGHT,
               TAG_END);
    g_ng.ng_Width = 16; g_ng.ng_Height = 60;
    probe_kind("SLIDER vertical", SLIDER_KIND, PGA_Freedom, LORIENT_VERT, GTSL_Max, 9, GTSL_Level, 9,
               TAG_END);
    probe_kind("SLIDER disabled", SLIDER_KIND, GA_Disabled, TRUE, TAG_END);
    probe_kind("SLIDER relverify", SLIDER_KIND, GA_RelVerify, TRUE, GA_Immediate, TRUE, TAG_END);

    probe_kind("SCROLLER", SCROLLER_KIND, GTSC_Total, 20, GTSC_Visible, 5, GTSC_Top, 3, TAG_END);
    probe_kind("SCROLLER arrows", SCROLLER_KIND, GTSC_Total, 20, GTSC_Visible, 5, GTSC_Top, 15,
               GTSC_Arrows, 16, TAG_END);
    g_ng.ng_Width = 18; g_ng.ng_Height = 80;
    probe_kind("SCROLLER vertical arrows", SCROLLER_KIND, PGA_Freedom, LORIENT_VERT, GTSC_Total, 10,
               GTSC_Visible, 10, GTSC_Arrows, 10, TAG_END);
    probe_kind("SCROLLER disabled", SCROLLER_KIND, GA_Disabled, TRUE, TAG_END);

    g_ng.ng_Height = 60;
    probe_kind("LISTVIEW", LISTVIEW_KIND, GTLV_Labels, (ULONG)&lv_list, TAG_END);
    g_ng.ng_Height = 60;
    probe_kind("LISTVIEW showsel", LISTVIEW_KIND, GTLV_Labels, (ULONG)&lv_list, GTLV_ShowSelected, 0,
               GTLV_Selected, 2, TAG_END);
    g_ng.ng_Height = 60;
    probe_kind("LISTVIEW readonly top", LISTVIEW_KIND, GTLV_Labels, (ULONG)&lv_list, GTLV_ReadOnly, TRUE,
               GTLV_Top, 2, TAG_END);
    g_ng.ng_Height = 60;
    probe_kind("LISTVIEW scrollwidth", LISTVIEW_KIND, GTLV_Labels, (ULONG)&lv_list, GTLV_ScrollWidth, 20,
               TAG_END);
    g_ng.ng_Height = 60; g_ng.ng_Flags = PLACETEXT_LEFT;
    probe_kind("LISTVIEW nolabels left", LISTVIEW_KIND, TAG_END);

    /* listview with a string gadget for the selection */
    P_SECTION("LISTVIEW showsel string");
    ctx = CreateContext(&glist);
    g_ng.ng_Height = 14; g_ng.ng_GadgetText = NULL; g_ng.ng_GadgetID = 8;
    g = CreateGadget(STRING_KIND, ctx, &g_ng, TAG_END);
    g_ng.ng_Height = 60; g_ng.ng_GadgetText = (UBYTE *)"Label"; g_ng.ng_GadgetID = 7;
    g = CreateGadget(LISTVIEW_KIND, g, &g_ng, GTLV_Labels, (ULONG)&lv_list, GTLV_ShowSelected, (ULONG)g,
                     TAG_END);
    P_NULL("result", g);
    dump_glist("", glist);
    FreeGadgets(glist);
    ng_reset();

    probe_kind("PALETTE", PALETTE_KIND, GTPA_Depth, 2, TAG_END);
    g_ng.ng_Width = 160; g_ng.ng_Height = 20;
    probe_kind("PALETTE indicator width", PALETTE_KIND, GTPA_Depth, 3, GTPA_Color, 5,
               GTPA_IndicatorWidth, 20, TAG_END);
    g_ng.ng_Width = 60; g_ng.ng_Height = 40;
    probe_kind("PALETTE indicator height", PALETTE_KIND, GTPA_Depth, 2, GTPA_IndicatorHeight, 10,
               GTPA_ColorOffset, 1, TAG_END);
    g_ng.ng_Flags = PLACETEXT_LEFT;
    probe_kind("PALETTE left", PALETTE_KIND, GTPA_Depth, 2, TAG_END);
    g_ng.ng_Flags = PLACETEXT_BELOW;
    probe_kind("PALETTE below indicator width", PALETTE_KIND, GTPA_Depth, 2, GTPA_IndicatorWidth, 20, TAG_END);
    probe_kind("PALETTE disabled", PALETTE_KIND, GTPA_Depth, 1, GA_Disabled, TRUE, TAG_END);

    probe_kind("TEXT", TEXT_KIND, GTTX_Text, (ULONG)"Hello", TAG_END);
    probe_kind("TEXT border copy", TEXT_KIND, GTTX_Text, (ULONG)"Hello", GTTX_Border, TRUE,
               GTTX_CopyText, TRUE, TAG_END);
    g_ng.ng_Flags = PLACETEXT_RIGHT;
    probe_kind("TEXT right justify", TEXT_KIND, GTTX_Text, (ULONG)"Hi", GTTX_Justification, GTJ_RIGHT,
               TAG_END);

    probe_kind("NUMBER", NUMBER_KIND, GTNM_Number, 42, TAG_END);
    probe_kind("NUMBER border format", NUMBER_KIND, GTNM_Number, -7, GTNM_Border, TRUE,
               GTNM_Format, (ULONG)"%ld!", TAG_END);
    g_ng.ng_Flags = PLACETEXT_ABOVE;
    probe_kind("NUMBER above maxlen", NUMBER_KIND, GTNM_Number, 12, GTNM_MaxNumberLen, 3, TAG_END);

    probe_kind("GENERIC", GENERIC_KIND, TAG_END);

    /* a chain of several gadgets: where each one is linked */
    P_SECTION("chain");
    ctx = CreateContext(&glist);
    g = CreateGadget(BUTTON_KIND, ctx, &g_ng, TAG_END);
    g_ng.ng_TopEdge += 20;
    g = CreateGadget(SLIDER_KIND, g, &g_ng, TAG_END);
    g_ng.ng_TopEdge += 20;
    g = CreateGadget(CHECKBOX_KIND, g, &g_ng, TAG_END);
    P_NULL("result", g);
    {
        struct Gadget *x;
        int n = 0;
        for (x = glist; x; x = x->NextGadget)
            n++;
        P_LONG("count", n);
    }
    FreeGadgets(glist);
    ng_reset();

    FreeVisualInfo(g_vi);
    UnlockPubScreen(NULL, scr);
}

static int gtprobe_main(const char *fontname, int size)
{
    static struct TextAttr ta;
    struct TextFont *f = NULL;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase || !DiskfontBase)
        return 20;
    ta.ta_Name = (STRPTR)fontname;
    ta.ta_YSize = size;
    f = OpenDiskFont(&ta);
    if (!f)
    {
        probe_s("font missing\n");
        return 10;
    }
    g_ta = &ta;
    P_LONG("font ysize", f->tf_YSize);
    P_LONG("font baseline", f->tf_Baseline);
    probe_all();
    CloseFont(f);
    CloseLibrary(DiskfontBase);
    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}

#endif
