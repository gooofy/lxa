/*
 * Probe (Phase 222g): GadTools LISTVIEW_KIND line height - LAYOUTA_Spacing
 * (extra pixels between lines) and GTLV_ItemHeight (V39) change the list
 * gadget's height, which GadTools rounds down to whole lines.
 */
#include "gtprobe.h"

static void lv(const char *name, WORD height, Tag tag1, ULONG data1)
{
    struct TagItem tags[3];
    struct Gadget *glist = NULL, *ctx, *g;

    tags[0].ti_Tag = GTLV_Labels;
    tags[0].ti_Data = (ULONG)&lv_list;
    tags[1].ti_Tag = tag1;
    tags[1].ti_Data = data1;
    tags[2].ti_Tag = TAG_END;
    tags[2].ti_Data = 0;
    g_ng.ng_Height = height;
    P_SECTION(name);
    ctx = CreateContext(&glist);
    g = ctx ? CreateGadgetA(LISTVIEW_KIND, ctx, &g_ng, tags) : NULL;
    if (!g)
        probe_s("result = NULL\n");
    else
        dump_glist("", glist);
    FreeGadgets(glist);
    ng_reset();
}

int main(void)
{
    static struct TextAttr ta = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct Screen *scr;
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 39);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 39);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 39);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        return 20;
    scr = LockPubScreen(NULL);
    if (!scr)
        return 20;
    g_ta = &ta;
    g_vi = GetVisualInfo(scr, TAG_END);
    NewList(&lv_list);
    for (i = 0; i < 5; i++) {
        lv_nodes[i].ln_Name = (char *)lv_names[i];
        AddTail(&lv_list, &lv_nodes[i]);
    }
    ng_reset();

    lv("spacing 0", 60, LAYOUTA_Spacing, 0);
    lv("spacing 1", 60, LAYOUTA_Spacing, 1);
    lv("spacing 1 h=100", 100, LAYOUTA_Spacing, 1);
    lv("spacing 2", 60, LAYOUTA_Spacing, 2);
    lv("itemheight 12", 60, GTLV_ItemHeight, 12);
    lv("itemheight 6", 60, GTLV_ItemHeight, 6);
    g_ng.ng_Flags = PLACETEXT_ABOVE;
    lv("spacing 1 above", 75, LAYOUTA_Spacing, 1);

    FreeVisualInfo(g_vi);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
