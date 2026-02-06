/*
 * SimpleGTGadget.c - GadTools gadget demonstration
 * 
 * Based on RKM Libraries sample.
 * Tests GadTools library: CreateContext, CreateGadget, GetVisualInfo,
 * GT_RefreshWindow, GT_GetIMsg, GT_ReplyIMsg, and gadget functionality.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/gfxbase.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

/* Gadget IDs */
#define MYGAD_BUTTON     1
#define MYGAD_CHECKBOX   2
#define MYGAD_INTEGER    3
#define MYGAD_CYCLE      4

/* Font for gadgets */
struct TextAttr Topaz80 = { "topaz.font", 8, 0, 0, };

/* Cycle gadget labels */
UBYTE *CycleLabels[] = { "Option 1", "Option 2", "Option 3", NULL };

struct Library *IntuitionBase = NULL;
struct Library *GadToolsBase = NULL;
struct Library *GfxBase = NULL;

int main(void)
{
    struct Screen *mysc;
    struct Window *mywin;
    struct Gadget *glist = NULL, *gad;
    struct NewGadget ng;
    void *vi;
    struct IntuiMessage *imsg;
    ULONG msgClass;
    BOOL done = FALSE;

    printf("SimpleGTGadget: GadTools gadget demonstration\n\n");

    if ((IntuitionBase = OpenLibrary("intuition.library", 37)) == NULL)
    {
        printf("ERROR: Can't open intuition.library v37\n");
        return 1;
    }

    if ((GadToolsBase = OpenLibrary("gadtools.library", 37)) == NULL)
    {
        printf("ERROR: Can't open gadtools.library v37\n");
        CloseLibrary(IntuitionBase);
        return 1;
    }
    
    if ((GfxBase = OpenLibrary("graphics.library", 37)) == NULL)
    {
        printf("ERROR: Can't open graphics.library v37\n");
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Lock the default public screen */
    if ((mysc = LockPubScreen(NULL)) == NULL)
    {
        printf("ERROR: Can't lock public screen\n");
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Get visual info for the screen */
    if ((vi = GetVisualInfo(mysc, TAG_END)) == NULL)
    {
        printf("ERROR: Can't get visual info\n");
        UnlockPubScreen(NULL, mysc);
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Create gadget context */
    gad = CreateContext(&glist);
    if (gad == NULL)
    {
        printf("ERROR: Can't create gadget context\n");
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, mysc);
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Common gadget settings */
    ng.ng_TextAttr = &Topaz80;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = 0;

    /* Create a button gadget */
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 20 + mysc->WBorTop + (mysc->Font->ta_YSize + 1);
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = "Click Me";
    ng.ng_GadgetID = MYGAD_BUTTON;
    gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);
    if (gad) printf("Created BUTTON_KIND gadget at %d,%d size %dx%d\n", gad->LeftEdge, gad->TopEdge, gad->Width, gad->Height);

    /* Create a checkbox gadget */
    ng.ng_TopEdge += 20;
    ng.ng_Width = 26;
    ng.ng_Height = 11;
    ng.ng_GadgetText = "Checkbox";
    ng.ng_GadgetID = MYGAD_CHECKBOX;
    ng.ng_Flags = PLACETEXT_RIGHT;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
        GTCB_Checked, FALSE,
        TAG_END);
    if (gad) printf("Created CHECKBOX_KIND gadget\n");

    /* Create an integer gadget */
    ng.ng_TopEdge += 20;
    ng.ng_Width = 80;
    ng.ng_Height = 14;
    ng.ng_GadgetText = "Number:";
    ng.ng_GadgetID = MYGAD_INTEGER;
    ng.ng_Flags = PLACETEXT_LEFT;
    gad = CreateGadget(INTEGER_KIND, gad, &ng,
        GTIN_Number, 42,
        GTIN_MaxChars, 6,
        TAG_END);
    if (gad) printf("Created INTEGER_KIND gadget\n");

    /* Create a cycle gadget */
    ng.ng_TopEdge += 20;
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = "Cycle:";
    ng.ng_GadgetID = MYGAD_CYCLE;
    ng.ng_Flags = PLACETEXT_LEFT;
    gad = CreateGadget(CYCLE_KIND, gad, &ng,
        GTCY_Labels, (ULONG)CycleLabels,
        GTCY_Active, 0,
        TAG_END);
    if (gad) printf("Created CYCLE_KIND gadget\n");

    if (gad == NULL)
    {
        printf("ERROR: Gadget creation failed\n");
        FreeGadgets(glist);
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, mysc);
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Open a window with our gadgets */
    mywin = OpenWindowTags(NULL,
        WA_Title, (ULONG)"GadTools Gadget Demo",
        WA_Gadgets, (ULONG)glist,
        WA_AutoAdjust, TRUE,
        WA_Width, 300,
        WA_InnerHeight, 120,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate, TRUE,
        WA_CloseGadget, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | 
                  BUTTONIDCMP | CHECKBOXIDCMP | INTEGERIDCMP | CYCLEIDCMP,
        WA_PubScreen, (ULONG)mysc,
        TAG_END);

    if (mywin == NULL)
    {
        printf("ERROR: Can't open window\n");
        FreeGadgets(glist);
        FreeVisualInfo(vi);
        UnlockPubScreen(NULL, mysc);
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(IntuitionBase);
        return 1;
    }

    /* Important: refresh the window after gadgets are added */
    GT_RefreshWindow(mywin, NULL);

    /* Event loop */
    while (!done)
    {
        Wait(1L << mywin->UserPort->mp_SigBit);

        while ((imsg = GT_GetIMsg(mywin->UserPort)) != NULL)
        {
            msgClass = imsg->Class;
            printf("Received IDCMP class 0x%08lx\n", msgClass);
            
            if (msgClass == IDCMP_CLOSEWINDOW)
            {
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
                printf("IDCMP_GADGETUP: gadget ID %d, code %d\n", g->GadgetID, imsg->Code);
            }

            GT_ReplyIMsg(imsg);
        }
    }

    /* Cleanup */
    CloseWindow(mywin);
    FreeGadgets(glist);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, mysc);
    CloseLibrary(GfxBase);
    CloseLibrary(GadToolsBase);
    CloseLibrary(IntuitionBase);

    return 0;
}
