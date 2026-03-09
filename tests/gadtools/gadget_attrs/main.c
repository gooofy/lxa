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
    struct Gadget *gad = NULL;
    struct Gadget *checkbox_gad;
    struct Gadget *slider_gad;
    struct Gadget *string_gad;
    struct Gadget *integer_gad;
    struct Gadget *cycle_gad;
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

    gad = CreateContext(&glist);
    if (!gad)
        return fail("CreateContext failed");

    checkbox_gad = gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
        GTCB_Checked, FALSE,
        TAG_DONE);
    if (!checkbox_gad)
        return fail("checkbox creation failed");

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

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"String";
    ng.ng_GadgetID = 3;
    string_gad = gad = CreateGadget(STRING_KIND, gad, &ng,
        GTST_String, (ULONG)"hello",
        GTST_MaxChars, 16,
        TAG_DONE);
    if (!string_gad)
        return fail("string creation failed");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Integer";
    ng.ng_GadgetID = 4;
    integer_gad = gad = CreateGadget(INTEGER_KIND, gad, &ng,
        GTIN_Number, 42,
        GTIN_MaxChars, 8,
        TAG_DONE);
    if (!integer_gad)
        return fail("integer creation failed");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (UBYTE *)"Cycle";
    ng.ng_GadgetID = 5;
    cycle_gad = gad = CreateGadget(CYCLE_KIND, gad, &ng,
        GTCY_Labels, (ULONG)cycle_labels,
        GTCY_Active, 2,
        TAG_DONE);
    if (!cycle_gad)
        return fail("cycle creation failed");

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
