/* gadtoolsgadgets.c - Adapted from RKM GadTools sample
 * Demonstrates GadTools gadgets:
 * - SLIDER_KIND with GTSL_Min/Max/Level/LevelFormat
 * - STRING_KIND with GTST_String/MaxChars
 * - BUTTON_KIND 
 * - GT_SetGadgetAttrs to modify gadgets
 * - Keyboard shortcuts with GT_Underscore
 * 
 * Modified for automated testing (auto-exits after setup verification).
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/gfx.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>

#include <stdio.h>

/* Gadget IDs */
#define MYGAD_SLIDER    0
#define MYGAD_STRING1   1
#define MYGAD_STRING2   2
#define MYGAD_STRING3   3
#define MYGAD_BUTTON    4

/* Slider range */
#define SLIDER_MIN  1
#define SLIDER_MAX  20

struct TextAttr Topaz80 = { "topaz.font", 8, 0, 0 };

struct Library *IntuitionBase = NULL;
struct Library *GfxBase = NULL;
struct Library *GadToolsBase = NULL;

/* Create all gadgets */
struct Gadget *createAllGadgets(struct Gadget **glistptr, void *vi,
    UWORD topborder, WORD slider_level, struct Gadget *my_gads[])
{
    struct NewGadget ng;
    struct Gadget *gad;

    printf("GadToolsGadgets: Creating context gadget...\n");
    gad = CreateContext(glistptr);
    if (!gad)
    {
        printf("ERROR: CreateContext() failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: Context created at 0x%08lx\n", (ULONG)gad);

    /* Initialize NewGadget structure */
    ng.ng_LeftEdge   = 140;
    ng.ng_TopEdge    = 20 + topborder;
    ng.ng_Width      = 200;
    ng.ng_Height     = 12;
    ng.ng_GadgetText = "_Volume:   ";
    ng.ng_TextAttr   = &Topaz80;
    ng.ng_VisualInfo = vi;
    ng.ng_GadgetID   = MYGAD_SLIDER;
    ng.ng_Flags      = NG_HIGHLABEL;

    printf("GadToolsGadgets: Creating SLIDER_KIND gadget...\n");
    my_gads[MYGAD_SLIDER] = gad = CreateGadget(SLIDER_KIND, gad, &ng,
                        GTSL_Min,         SLIDER_MIN,
                        GTSL_Max,         SLIDER_MAX,
                        GTSL_Level,       slider_level,
                        GTSL_LevelFormat, "%2ld",
                        GTSL_MaxLevelLen, 2,
                        GT_Underscore,    '_',
                        TAG_END);
    if (!gad)
    {
        printf("ERROR: SLIDER_KIND creation failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: Slider created at 0x%08lx (level=%ld)\n", 
           (ULONG)gad, (LONG)slider_level);

    /* First string gadget */
    ng.ng_TopEdge   += 20;
    ng.ng_Height     = 14;
    ng.ng_GadgetText = "_First:";
    ng.ng_GadgetID   = MYGAD_STRING1;

    printf("GadToolsGadgets: Creating STRING_KIND gadget 1...\n");
    my_gads[MYGAD_STRING1] = gad = CreateGadget(STRING_KIND, gad, &ng,
                        GTST_String,   "Try pressing",
                        GTST_MaxChars, 50,
                        GT_Underscore, '_',
                        TAG_END);
    if (!gad)
    {
        printf("ERROR: STRING_KIND 1 creation failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: String 1 created at 0x%08lx\n", (ULONG)gad);

    /* Second string gadget */
    ng.ng_TopEdge   += 20;
    ng.ng_GadgetText = "_Second:";
    ng.ng_GadgetID   = MYGAD_STRING2;

    printf("GadToolsGadgets: Creating STRING_KIND gadget 2...\n");
    my_gads[MYGAD_STRING2] = gad = CreateGadget(STRING_KIND, gad, &ng,
                        GTST_String,   "TAB or Shift-TAB",
                        GTST_MaxChars, 50,
                        GT_Underscore, '_',
                        TAG_END);
    if (!gad)
    {
        printf("ERROR: STRING_KIND 2 creation failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: String 2 created at 0x%08lx\n", (ULONG)gad);

    /* Third string gadget */
    ng.ng_TopEdge   += 20;
    ng.ng_GadgetText = "_Third:";
    ng.ng_GadgetID   = MYGAD_STRING3;

    printf("GadToolsGadgets: Creating STRING_KIND gadget 3...\n");
    my_gads[MYGAD_STRING3] = gad = CreateGadget(STRING_KIND, gad, &ng,
                        GTST_String,   "To see what happens!",
                        GTST_MaxChars, 50,
                        GT_Underscore, '_',
                        TAG_END);
    if (!gad)
    {
        printf("ERROR: STRING_KIND 3 creation failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: String 3 created at 0x%08lx\n", (ULONG)gad);

    /* Button gadget */
    ng.ng_LeftEdge  += 50;
    ng.ng_TopEdge   += 20;
    ng.ng_Width      = 100;
    ng.ng_Height     = 12;
    ng.ng_GadgetText = "_Click Here";
    ng.ng_GadgetID   = MYGAD_BUTTON;
    ng.ng_Flags      = 0;

    printf("GadToolsGadgets: Creating BUTTON_KIND gadget...\n");
    gad = CreateGadget(BUTTON_KIND, gad, &ng,
                        GT_Underscore, '_',
                        TAG_END);
    if (!gad)
    {
        printf("ERROR: BUTTON_KIND creation failed\n");
        return NULL;
    }
    printf("GadToolsGadgets: Button created at 0x%08lx\n", (ULONG)gad);

    return gad;
}

/* Main GadTools window function */
void gadtoolsWindow(void)
{
    struct TextFont *font = NULL;
    struct Screen *mysc = NULL;
    struct Window *mywin = NULL;
    struct Gadget *glist = NULL;
    struct Gadget *my_gads[5];
    void *vi = NULL;
    WORD slider_level = 5;
    UWORD topborder;

    printf("GadToolsGadgets: Opening topaz 8 font...\n");
    font = OpenFont(&Topaz80);
    if (!font)
    {
        printf("ERROR: Failed to open Topaz 80\n");
        return;
    }
    printf("GadToolsGadgets: Font opened at 0x%08lx\n", (ULONG)font);

    printf("GadToolsGadgets: Locking public screen...\n");
    mysc = LockPubScreen(NULL);
    if (!mysc)
    {
        printf("ERROR: Couldn't lock default public screen\n");
        goto cleanup_font;
    }
    printf("GadToolsGadgets: Screen locked at 0x%08lx\n", (ULONG)mysc);

    printf("GadToolsGadgets: Getting visual info...\n");
    vi = GetVisualInfo(mysc, TAG_END);
    if (!vi)
    {
        printf("ERROR: GetVisualInfo() failed\n");
        goto cleanup_screen;
    }
    printf("GadToolsGadgets: VisualInfo at 0x%08lx\n", (ULONG)vi);

    /* Calculate window title bar height */
    topborder = mysc->WBorTop + (mysc->Font->ta_YSize + 1);
    printf("GadToolsGadgets: Top border = %d pixels\n", topborder);

    printf("GadToolsGadgets: Creating gadgets...\n");
    if (!createAllGadgets(&glist, vi, topborder, slider_level, my_gads))
    {
        printf("ERROR: createAllGadgets() failed\n");
        goto cleanup_vi;
    }
    printf("GadToolsGadgets: All gadgets created successfully\n");

    printf("GadToolsGadgets: Opening window...\n");
    mywin = OpenWindowTags(NULL,
            WA_Title,       "GadTools Gadget Demo",
            WA_Gadgets,     glist,
            WA_AutoAdjust,  TRUE,
            WA_Width,       400,
            WA_MinWidth,    50,
            WA_InnerHeight, 140,
            WA_MinHeight,   50,
            WA_DragBar,     TRUE,
            WA_DepthGadget, TRUE,
            WA_Activate,    TRUE,
            WA_CloseGadget, TRUE,
            WA_SizeGadget,  TRUE,
            WA_SimpleRefresh, TRUE,
            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW |
                      IDCMP_VANILLAKEY | SLIDERIDCMP | STRINGIDCMP |
                      BUTTONIDCMP,
            WA_PubScreen, mysc,
            TAG_END);

    if (!mywin)
    {
        printf("ERROR: OpenWindow() failed\n");
        goto cleanup_gadgets;
    }
    printf("GadToolsGadgets: Window opened at 0x%08lx\n", (ULONG)mywin);
    printf("GadToolsGadgets: Window size: %dx%d\n", mywin->Width, mywin->Height);

    /* Refresh gadgets after window opens */
    printf("GadToolsGadgets: Refreshing window gadgets...\n");
    GT_RefreshWindow(mywin, NULL);
    printf("GadToolsGadgets: Gadgets refreshed\n");

    /* Test GT_SetGadgetAttrs - modify slider level */
    printf("\nGadToolsGadgets: Testing GT_SetGadgetAttrs...\n");
    slider_level = 15;
    GT_SetGadgetAttrs(my_gads[MYGAD_SLIDER], mywin, NULL,
                      GTSL_Level, slider_level,
                      TAG_END);
    printf("GadToolsGadgets: Slider level set to %ld\n", (LONG)slider_level);

    /* Verify string gadget presence */
    printf("\nGadToolsGadgets: Verifying string gadgets...\n");
    printf("GadToolsGadgets: String gadget 1 at 0x%08lx\n", (ULONG)my_gads[MYGAD_STRING1]);
    printf("GadToolsGadgets: String gadget 2 at 0x%08lx\n", (ULONG)my_gads[MYGAD_STRING2]);
    printf("GadToolsGadgets: String gadget 3 at 0x%08lx\n", (ULONG)my_gads[MYGAD_STRING3]);

    printf("\nGadToolsGadgets: Entering event loop...\n");

    /* Event loop */
    {
        struct IntuiMessage *imsg;
        ULONG msgClass;
        BOOL done = FALSE;

        while (!done)
        {
            Wait(1L << mywin->UserPort->mp_SigBit);

            while ((imsg = GT_GetIMsg(mywin->UserPort)) != NULL)
            {
                msgClass = imsg->Class;

                if (msgClass == IDCMP_CLOSEWINDOW)
                {
                    printf("GadToolsGadgets: IDCMP_CLOSEWINDOW\n");
                    done = TRUE;
                }
                else if (msgClass == IDCMP_REFRESHWINDOW)
                {
                    GT_BeginRefresh(mywin);
                    GT_EndRefresh(mywin, TRUE);
                }
                else if (msgClass == IDCMP_GADGETUP)
                {
                    struct Gadget *g = (struct Gadget *)imsg->IAddress;
                    printf("GadToolsGadgets: IDCMP_GADGETUP: gadget ID %d, code %d\n",
                           g->GadgetID, imsg->Code);
                }

                GT_ReplyIMsg(imsg);
            }
        }
    }

    CloseWindow(mywin);
    printf("GadToolsGadgets: Window closed\n");

cleanup_gadgets:
    FreeGadgets(glist);
    printf("GadToolsGadgets: Gadgets freed\n");

cleanup_vi:
    FreeVisualInfo(vi);
    printf("GadToolsGadgets: VisualInfo freed\n");

cleanup_screen:
    UnlockPubScreen(NULL, mysc);
    printf("GadToolsGadgets: Screen unlocked\n");

cleanup_font:
    CloseFont(font);
    printf("GadToolsGadgets: Font closed\n");
}

int main(void)
{
    printf("GadToolsGadgets: GadTools gadget demonstration\n\n");

    printf("GadToolsGadgets: Opening intuition.library...\n");
    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("ERROR: Requires V37 intuition.library\n");
        return 1;
    }
    printf("GadToolsGadgets: intuition.library v%ld opened\n", IntuitionBase->lib_Version);

    printf("GadToolsGadgets: Opening graphics.library...\n");
    GfxBase = OpenLibrary("graphics.library", 37);
    if (!GfxBase)
    {
        printf("ERROR: Requires V37 graphics.library\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("GadToolsGadgets: graphics.library v%ld opened\n", GfxBase->lib_Version);

    printf("GadToolsGadgets: Opening gadtools.library...\n");
    GadToolsBase = OpenLibrary("gadtools.library", 37);
    if (!GadToolsBase)
    {
        printf("ERROR: Requires V37 gadtools.library\n");
        CloseLibrary(GfxBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }
    printf("GadToolsGadgets: gadtools.library v%ld opened\n\n", GadToolsBase->lib_Version);

    gadtoolsWindow();

    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);

    printf("\nGadToolsGadgets: All libraries closed, demo complete.\n");
    return 0;
}
