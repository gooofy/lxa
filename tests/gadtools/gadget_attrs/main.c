#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <stdio.h>
#include <string.h>

struct Library *IntuitionBase = NULL;
struct Library *GadToolsBase = NULL;

static int fail(const char *message)
{
    printf("FAIL: %s\n", message);
    return 1;
}

int main(void)
{
    struct Screen *screen = NULL;
    APTR vi = NULL;
    struct Gadget *glist = NULL;
    struct Gadget *context = NULL;
    struct Gadget *gad = NULL;
    struct Gadget *checkbox_gad;
    struct Gadget *slider_gad;
    struct Gadget *string_gad;
    struct Gadget *integer_gad;
    struct Gadget *cycle_gad;
    struct Gadget *mx_gad;
    struct Gadget *listview_gad;
    struct Gadget *palette_gad;
    struct Gadget *scroller_gad;
    struct Gadget *text_gad;
    struct Gadget *number_gad;
    struct NewGadget ng;
    STRPTR cycle_labels[] = { (STRPTR)"One", (STRPTR)"Two", (STRPTR)"Three", NULL };
    ULONG checked = 99;
    ULONG level = 0;
    ULONG min = 0;
    ULONG max = 0;
    ULONG number = 0;
    ULONG active = 0;
    STRPTR text = NULL;
    struct TagItem get_slider_more[] = {
        { GTSL_Max, (ULONG)&max },
        { TAG_DONE, 0 }
    };
    struct TagItem get_slider_tags[] = {
        { GTSL_Min, (ULONG)&min },
        { TAG_IGNORE, 0 },
        { GTSL_Level, (ULONG)&level },
        { TAG_MORE, (ULONG)get_slider_more }
    };
    struct TagItem set_checkbox_tags[] = {
        { GTCB_Checked, TRUE },
        { TAG_DONE, 0 }
    };
    struct TagItem set_slider_tags[] = {
        { TAG_IGNORE, 0 },
        { GTSL_Level, 13 },
        { TAG_DONE, 0 }
    };
    struct TagItem set_string_tags[] = {
        { GTST_String, (ULONG)"world" },
        { TAG_DONE, 0 }
    };
    struct TagItem set_integer_tags[] = {
        { GTIN_Number, (ULONG)-17 },
        { TAG_DONE, 0 }
    };
    struct TagItem set_cycle_more[] = {
        { GTCY_Active, 1 },
        { TAG_DONE, 0 }
    };
    struct TagItem set_cycle_tags[] = {
        { TAG_MORE, (ULONG)set_cycle_more }
    };
    struct TagItem get_checkbox_tags[] = {
        { GTCB_Checked, (ULONG)&checked },
        { TAG_DONE, 0 }
    };
    struct TagItem get_string_tags[] = {
        { GTST_String, (ULONG)&text },
        { TAG_DONE, 0 }
    };
    struct TagItem get_integer_tags[] = {
        { GTIN_Number, (ULONG)&number },
        { TAG_DONE, 0 }
    };
    struct TagItem get_cycle_tags[] = {
        { GTCY_Active, (ULONG)&active },
        { TAG_DONE, 0 }
    };

    printf("Testing GadTools gadget attrs...\n");

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    GadToolsBase = OpenLibrary((CONST_STRPTR)"gadtools.library", 37);
    if (!IntuitionBase || !GadToolsBase)
        return fail("cannot open required libraries");

    screen = LockPubScreen(NULL);
    if (!screen)
        return fail("cannot lock public screen");

    vi = GetVisualInfoA(screen, NULL);
    if (!vi)
        return fail("GetVisualInfoA returned NULL");

    memset(&ng, 0, sizeof(ng));
    ng.ng_VisualInfo = vi;
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 20;
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = (UBYTE *)"Check";
    ng.ng_GadgetID = 1;

    if (CreateContext(NULL) != NULL)
        return fail("CreateContext(NULL) should fail safely");

    context = gad = CreateContext(&glist);
    if (!gad)
        return fail("CreateContext failed");
    if (glist != context)
        return fail("CreateContext did not store the context gadget in the list head");
    if (context->NextGadget != NULL)
        return fail("fresh context gadget should not have a successor");
    if ((context->Flags & GFLG_GADGHIGHBITS) != GFLG_GADGHNONE)
        return fail("context gadget should be invisible and unselectable");
    if ((context->GadgetType & GTYP_GTYPEMASK) != GTYP_CUSTOMGADGET)
        return fail("context gadget type mismatch");
    printf("OK: CreateContext returned invisible sentinel gadget\n");

    checkbox_gad = gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
        GTCB_Checked, FALSE,
        TAG_DONE);
    if (!checkbox_gad)
        return fail("checkbox creation failed");
    if (context->NextGadget != checkbox_gad)
        return fail("context gadget did not link to the first created gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Slider";
    ng.ng_GadgetID = 2;
    slider_gad = gad = CreateGadget(SLIDER_KIND, gad, &ng,
        GTSL_Min, 1,
        GTSL_Max, 20,
        GTSL_Level, 5,
        TAG_DONE);
    if (!slider_gad)
        return fail("slider creation failed");
    if (checkbox_gad->NextGadget != slider_gad)
        return fail("checkbox gadget did not link to slider gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"String";
    ng.ng_GadgetID = 3;
    string_gad = gad = CreateGadget(STRING_KIND, gad, &ng,
        GTST_String, (ULONG)"hello",
        GTST_MaxChars, 16,
        TAG_DONE);
    if (!string_gad)
        return fail("string creation failed");
    if (slider_gad->NextGadget != string_gad)
        return fail("slider gadget did not link to string gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Integer";
    ng.ng_GadgetID = 4;
    integer_gad = gad = CreateGadget(INTEGER_KIND, gad, &ng,
        GTIN_Number, 42,
        GTIN_MaxChars, 8,
        TAG_DONE);
    if (!integer_gad)
        return fail("integer creation failed");
    if (string_gad->NextGadget != integer_gad)
        return fail("string gadget did not link to integer gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Cycle";
    ng.ng_GadgetID = 5;
    cycle_gad = gad = CreateGadget(CYCLE_KIND, gad, &ng,
        GTCY_Labels, (ULONG)cycle_labels,
        GTCY_Active, 2,
        TAG_DONE);
    if (!cycle_gad)
        return fail("cycle creation failed");
    if (integer_gad->NextGadget != cycle_gad)
        return fail("integer gadget did not link to cycle gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"MX";
    ng.ng_GadgetID = 6;
    mx_gad = gad = CreateGadget(MX_KIND, gad, &ng, TAG_DONE);
    if (!mx_gad)
        return fail("mx creation failed");
    if (cycle_gad->NextGadget != mx_gad)
        return fail("cycle gadget did not link to mx gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"ListView";
    ng.ng_GadgetID = 7;
    listview_gad = gad = CreateGadget(LISTVIEW_KIND, gad, &ng, TAG_DONE);
    if (!listview_gad)
        return fail("listview creation failed");
    if (mx_gad->NextGadget != listview_gad)
        return fail("mx gadget did not link to listview gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Palette";
    ng.ng_GadgetID = 8;
    palette_gad = gad = CreateGadget(PALETTE_KIND, gad, &ng, TAG_DONE);
    if (!palette_gad)
        return fail("palette creation failed");
    if (listview_gad->NextGadget != palette_gad)
        return fail("listview gadget did not link to palette gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Scroller";
    ng.ng_GadgetID = 9;
    scroller_gad = gad = CreateGadget(SCROLLER_KIND, gad, &ng, TAG_DONE);
    if (!scroller_gad)
        return fail("scroller creation failed");
    if (palette_gad->NextGadget != scroller_gad)
        return fail("palette gadget did not link to scroller gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Text";
    ng.ng_GadgetID = 10;
    text_gad = gad = CreateGadget(TEXT_KIND, gad, &ng, TAG_DONE);
    if (!text_gad)
        return fail("text creation failed");
    if (scroller_gad->NextGadget != text_gad)
        return fail("scroller gadget did not link to text gadget");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Number";
    ng.ng_GadgetID = 11;
    number_gad = gad = CreateGadget(NUMBER_KIND, gad, &ng, TAG_DONE);
    if (!number_gad)
        return fail("number creation failed");
    if (text_gad->NextGadget != number_gad)
        return fail("text gadget did not link to number gadget");
    if (number_gad->NextGadget != NULL)
        return fail("last gadget should terminate the list");

    if ((mx_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET)
        return fail("mx gadget type mismatch");
    if ((listview_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET)
        return fail("listview gadget type mismatch");
    if ((palette_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET)
        return fail("palette gadget type mismatch");
    if ((scroller_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_PROPGADGET)
        return fail("scroller gadget type mismatch");
    if ((text_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_BOOLGADGET)
        return fail("text gadget type mismatch");
    if ((number_gad->GadgetType & GTYP_GTYPEMASK) != GTYP_BOOLGADGET)
        return fail("number gadget type mismatch");
    printf("OK: CreateGadget supports all current gadget kinds\n");

    if (GT_GetGadgetAttrsA(checkbox_gad, NULL, NULL, get_checkbox_tags) != 1 || checked != FALSE)
        return fail("checkbox get attrs did not report initial unchecked state");
    printf("OK: checkbox initial checked state read back\n");

    if (GT_GetGadgetAttrsA(slider_gad, NULL, NULL, get_slider_tags) != 3 || min != 1 || max != 20 || level != 5)
        return fail("slider get attrs did not return initial min/max/level");
    printf("OK: slider initial min/max/level read back\n");

    if (GT_GetGadgetAttrsA(string_gad, NULL, NULL, get_string_tags) != 1 || !text || strcmp((char *)text, "hello") != 0)
        return fail("string get attrs did not return initial text");
    printf("OK: string initial text read back\n");

    if (GT_GetGadgetAttrsA(integer_gad, NULL, NULL, get_integer_tags) != 1 || (LONG)number != 42)
        return fail("integer get attrs did not return initial number");
    printf("OK: integer initial value read back\n");

    if (GT_GetGadgetAttrsA(cycle_gad, NULL, NULL, get_cycle_tags) != 1 || active != 2)
        return fail("cycle get attrs did not return initial active item");
    printf("OK: cycle initial active item read back\n");

    GT_SetGadgetAttrsA(checkbox_gad, NULL, NULL, set_checkbox_tags);
    GT_SetGadgetAttrsA(slider_gad, NULL, NULL, set_slider_tags);
    GT_SetGadgetAttrsA(string_gad, NULL, NULL, set_string_tags);
    GT_SetGadgetAttrsA(integer_gad, NULL, NULL, set_integer_tags);
    GT_SetGadgetAttrsA(cycle_gad, NULL, NULL, set_cycle_tags);

    checked = 99;
    level = 0;
    number = 0;
    active = 0;
    text = NULL;

    if (GT_GetGadgetAttrsA(checkbox_gad, NULL, NULL, get_checkbox_tags) != 1 || checked != TRUE)
        return fail("checkbox set attrs did not update checked state");
    printf("OK: checkbox checked state updated\n");

    if (GT_GetGadgetAttrsA(slider_gad, NULL, NULL, get_slider_tags) != 3 || level != 13)
        return fail("slider set attrs did not update level");
    printf("OK: slider level updated\n");

    if (GT_GetGadgetAttrsA(string_gad, NULL, NULL, get_string_tags) != 1 || !text || strcmp((char *)text, "world") != 0)
        return fail("string set attrs did not update text");
    printf("OK: string text updated\n");

    if (GT_GetGadgetAttrsA(integer_gad, NULL, NULL, get_integer_tags) != 1 || (LONG)number != -17)
        return fail("integer set attrs did not update value");
    if (strcmp((char *)((struct StringInfo *)integer_gad->SpecialInfo)->Buffer, "-17") != 0)
        return fail("integer buffer did not update after GTIN_Number change");
    printf("OK: integer value updated\n");

    if (GT_GetGadgetAttrsA(cycle_gad, NULL, NULL, get_cycle_tags) != 1 || active != 1)
        return fail("cycle set attrs did not update active item");
    printf("OK: cycle active item updated\n");

    FreeGadgets(glist);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, screen);
    CloseLibrary(GadToolsBase);
    CloseLibrary(IntuitionBase);

    printf("PASS: gadget attrs test complete\n");
    return 0;
}
