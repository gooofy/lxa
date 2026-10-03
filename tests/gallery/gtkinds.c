/*
 * GalleryGT - every GadTools kind in normal / selected / disabled state
 * (Phase 223).
 *
 *   GalleryGT                      topaz 8 (ROM font)
 *   GalleryGT <font.font> <size>   any disk font (OpenDiskFont)
 *
 * Layout uses the font height, so the same page works for every font.
 */

#include "gallery.h"

static STRPTR cycle_labels[] = { (STRPTR)"First", (STRPTR)"Second", (STRPTR)"Third", NULL };
static STRPTR mx_labels[] = { (STRPTR)"Alpha", (STRPTR)"Beta", (STRPTR)"Gamma", NULL };
static struct Node lv_nodes[6];
static struct List lv_list;
static const char *lv_names[6] = { "Amiga", "Workbench", "Intuition", "GadTools", "Graphics", "Layers" };

int main(int argc, char **argv)
{
    struct TextAttr ta = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct TextFont *font = NULL;
    struct Screen *scr;
    APTR vi;
    struct Gadget *glist = NULL, *gad, *sel_button = NULL;
    struct NewGadget ng;
    struct Window *win;
    WORD fh, row, top, x1, x2, x3;
    int i;

    if (!gallery_open_libs())
        return 20;

    if (argc >= 3)
    {
        ta.ta_Name = (STRPTR)argv[1];
        ta.ta_YSize = atoi(argv[2]);
        if (!DiskfontBase || !(font = OpenDiskFont(&ta)))
        {
            printf("GalleryGT: cannot open %s %ld\n", argv[1], (long)ta.ta_YSize);
            gallery_close_libs();
            return 10;
        }
    }
    else
        font = OpenFont(&ta);
    fh = font ? font->tf_YSize : 8;

    NewList(&lv_list);
    for (i = 0; i < 6; i++)
    {
        lv_nodes[i].ln_Name = (char *)lv_names[i];
        AddTail(&lv_list, &lv_nodes[i]);
    }

    scr = LockPubScreen(NULL);
    vi = GetVisualInfo(scr, TAG_END);
    gad = CreateContext(&glist);

    row = fh + 6;
    top = scr->WBorTop + scr->Font->ta_YSize + 1 + 4;
    x1 = 10 + 9 * TextLength(&scr->RastPort, (STRPTR)"M", 1);
    x2 = 230;
    x3 = 440;

    memset(&ng, 0, sizeof(ng));
    ng.ng_TextAttr = &ta;
    ng.ng_VisualInfo = vi;

    /* --- column 1: buttons, checkboxes, strings, cycles ----------------- */
    ng.ng_LeftEdge = x1; ng.ng_TopEdge = top; ng.ng_Width = 100; ng.ng_Height = row;
    ng.ng_GadgetText = (UBYTE *)"Normal"; ng.ng_Flags = PLACETEXT_IN; ng.ng_GadgetID = 1;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"_Under"; ng.ng_GadgetID = 2;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, GT_Underscore, '_', TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Selected"; ng.ng_GadgetID = 3;
    gad = sel_button = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Disabled"; ng.ng_GadgetID = 4;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, GA_Disabled, TRUE, TAG_END);

    ng.ng_TopEdge += row + 3; ng.ng_Width = 26; ng.ng_Height = 11;
    ng.ng_GadgetText = (UBYTE *)"Check"; ng.ng_Flags = PLACETEXT_LEFT; ng.ng_GadgetID = 5;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng, TAG_END);
    ng.ng_LeftEdge = x1 + 40; ng.ng_GadgetText = NULL; ng.ng_GadgetID = 6;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng, GTCB_Checked, TRUE, TAG_END);
    ng.ng_LeftEdge = x1 + 80; ng.ng_GadgetID = 7;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng, GTCB_Checked, TRUE, GA_Disabled, TRUE, TAG_END);

    ng.ng_LeftEdge = x1; ng.ng_TopEdge += 11 + 3; ng.ng_Width = 100; ng.ng_Height = row;
    ng.ng_GadgetText = (UBYTE *)"Integer"; ng.ng_GadgetID = 8;
    gad = CreateGadget(INTEGER_KIND, gad, &ng, GTIN_Number, 1234, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"String"; ng.ng_GadgetID = 9;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_String, (ULONG)"Hello", TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Str Off"; ng.ng_GadgetID = 10;
    gad = CreateGadget(STRING_KIND, gad, &ng, GTST_String, (ULONG)"Ghost", GA_Disabled, TRUE, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Cycle"; ng.ng_GadgetID = 11;
    gad = CreateGadget(CYCLE_KIND, gad, &ng, GTCY_Labels, (ULONG)cycle_labels, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Cyc Off"; ng.ng_GadgetID = 12;
    gad = CreateGadget(CYCLE_KIND, gad, &ng, GTCY_Labels, (ULONG)cycle_labels, GTCY_Active, 1,
                       GA_Disabled, TRUE, TAG_END);

    /* --- column 2: mx, sliders, scrollers, palette, text/number -------- */
    ng.ng_LeftEdge = x2; ng.ng_TopEdge = top; ng.ng_Width = 17; ng.ng_Height = 9;
    ng.ng_GadgetText = NULL; ng.ng_Flags = PLACETEXT_RIGHT; ng.ng_GadgetID = 20;
    gad = CreateGadget(MX_KIND, gad, &ng, GTMX_Labels, (ULONG)mx_labels, GTMX_Active, 1,
                       GTMX_Spacing, 2, TAG_END);
    ng.ng_LeftEdge = x2 + 90; ng.ng_GadgetID = 21;
    gad = CreateGadget(MX_KIND, gad, &ng, GTMX_Labels, (ULONG)mx_labels, GTMX_Active, 2,
                       GTMX_Spacing, 2, GA_Disabled, TRUE, TAG_END);

    ng.ng_LeftEdge = x2 + 50; ng.ng_TopEdge = top + 3 * (fh + 2) + 6; ng.ng_Width = 120; ng.ng_Height = 11;
    ng.ng_GadgetText = (UBYTE *)"Sld"; ng.ng_Flags = PLACETEXT_LEFT; ng.ng_GadgetID = 22;
    gad = CreateGadget(SLIDER_KIND, gad, &ng, GTSL_Min, 0, GTSL_Max, 20, GTSL_Level, 5,
                       GTSL_LevelFormat, (ULONG)"%2ld", GTSL_MaxLevelLen, 2,
                       GTSL_LevelPlace, PLACETEXT_RIGHT, TAG_END);
    ng.ng_TopEdge += 11 + 3; ng.ng_GadgetText = (UBYTE *)"Off"; ng.ng_GadgetID = 23;
    gad = CreateGadget(SLIDER_KIND, gad, &ng, GTSL_Min, 0, GTSL_Max, 20, GTSL_Level, 15,
                       GA_Disabled, TRUE, TAG_END);
    ng.ng_TopEdge += 11 + 3; ng.ng_GadgetText = (UBYTE *)"Scr"; ng.ng_GadgetID = 24;
    gad = CreateGadget(SCROLLER_KIND, gad, &ng, GTSC_Top, 2, GTSC_Total, 10, GTSC_Visible, 3,
                       GTSC_Arrows, 16, TAG_END);
    ng.ng_TopEdge += 11 + 3; ng.ng_GadgetText = (UBYTE *)"Pal"; ng.ng_Height = 20; ng.ng_GadgetID = 25;
    gad = CreateGadget(PALETTE_KIND, gad, &ng, GTPA_Depth, 2, GTPA_Color, 3, TAG_END);
    ng.ng_TopEdge += 20 + 3; ng.ng_Height = row; ng.ng_GadgetText = (UBYTE *)"Txt"; ng.ng_GadgetID = 26;
    gad = CreateGadget(TEXT_KIND, gad, &ng, GTTX_Text, (ULONG)"Text kind", GTTX_Border, TRUE, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Num"; ng.ng_GadgetID = 27;
    gad = CreateGadget(NUMBER_KIND, gad, &ng, GTNM_Number, 4711, GTNM_Border, TRUE, TAG_END);
    ng.ng_TopEdge += row + 3; ng.ng_GadgetText = (UBYTE *)"Txt"; ng.ng_GadgetID = 28;
    gad = CreateGadget(TEXT_KIND, gad, &ng, GTTX_Text, (ULONG)"No border", TAG_END);

    /* vertical slider/scroller */
    ng.ng_LeftEdge = x2 - 28; ng.ng_TopEdge = top + 3 * (fh + 2) + 6; ng.ng_Width = 18; ng.ng_Height = 60;
    ng.ng_GadgetText = NULL; ng.ng_GadgetID = 29;
    gad = CreateGadget(SCROLLER_KIND, gad, &ng, GTSC_Top, 3, GTSC_Total, 10, GTSC_Visible, 4,
                       GTSC_Arrows, 10, PGA_Freedom, LORIENT_VERT, TAG_END);

    /* --- column 3: listviews --------------------------------------------- */
    ng.ng_LeftEdge = x3; ng.ng_TopEdge = top + fh + 4; ng.ng_Width = 150; ng.ng_Height = 4 * fh + 16;
    ng.ng_GadgetText = (UBYTE *)"List"; ng.ng_Flags = PLACETEXT_ABOVE; ng.ng_GadgetID = 30;
    gad = CreateGadget(LISTVIEW_KIND, gad, &ng, GTLV_Labels, (ULONG)&lv_list, GTLV_ShowSelected, 0,
                       GTLV_Selected, 2, TAG_END);
    ng.ng_TopEdge += ng.ng_Height + fh + 8; ng.ng_GadgetText = (UBYTE *)"ReadOnly"; ng.ng_GadgetID = 31;
    gad = CreateGadget(LISTVIEW_KIND, gad, &ng, GTLV_Labels, (ULONG)&lv_list, GTLV_ReadOnly, TRUE,
                       GTLV_Top, 1, TAG_END);

    if (!gad)
    {
        printf("GalleryGT: gadget creation failed\n");
        return 10;
    }

    win = OpenWindowTags(NULL,
        WA_Left, 10, WA_Top, 12, WA_Width, 620, WA_Height, 240,
        WA_Title, (ULONG)"GT Gallery",
        WA_Gadgets, (ULONG)glist,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        /* no INTUITICKS (ARROWIDCMP): the reference agent waits for an
         * empty window port */
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN,
        WA_PubScreen, (ULONG)scr,
        TAG_END);
    if (!win)
        return 10;
    gallery_add_window(win);
    GT_RefreshWindow(win, NULL);
    if (sel_button)
    {
        sel_button->Flags |= GFLG_SELECTED;
        RefreshGList(sel_button, win, NULL, 1);
    }

    gallery_wait();
    gallery_close_windows();
    FreeGadgets(glist);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);
    if (font)
        CloseFont(font);
    gallery_close_libs();
    return 0;
}
