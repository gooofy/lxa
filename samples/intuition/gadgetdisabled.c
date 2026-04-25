/* gadgetdisabled.c - deterministic disabled-gadget rendering sample
 *
 * Renders enabled/disabled pairs for BUTTON_KIND, CHECKBOX_KIND and
 * CYCLE_KIND. Keyboard shortcuts:
 *   d - OffGadget() on the enabled button
 *   e - OnGadget() on the enabled button
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

#define GID_BTN_ENABLED   1
#define GID_BTN_DISABLED  2
#define GID_CB_ENABLED    3
#define GID_CB_DISABLED   4
#define GID_CY_ENABLED    5
#define GID_CY_DISABLED   6

struct TextAttr Topaz80 = { "topaz.font", 8, 0, 0 };
UBYTE *CycleLabels[] = { "Option 1", "Option 2", "Option 3", NULL };

struct Library *IntuitionBase;
struct Library *GadToolsBase;

int main(void)
{
    struct Screen *screen;
    struct Window *window;
    struct Gadget *glist = NULL;
    struct Gadget *gad;
    struct Gadget *btn_enabled = NULL;
    struct Gadget *btn_disabled = NULL;
    struct Gadget *cb_disabled = NULL;
    struct Gadget *cy_disabled = NULL;
    struct NewGadget ng;
    struct IntuiMessage *imsg;
    void *vi;
    BOOL done = FALSE;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    GadToolsBase = OpenLibrary("gadtools.library", 37);
    if (!IntuitionBase || !GadToolsBase)
        goto out;

    screen = LockPubScreen(NULL);
    if (!screen)
        goto out;

    vi = GetVisualInfo(screen, TAG_END);
    if (!vi)
        goto unlock;

    gad = CreateContext(&glist);
    if (!gad)
        goto free_vi;

    ng.ng_TextAttr = &Topaz80;
    ng.ng_VisualInfo = vi;
    ng.ng_Flags = 0;

    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = 20 + screen->WBorTop + (screen->Font->ta_YSize + 1);
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = "Btn En";
    ng.ng_GadgetID = GID_BTN_ENABLED;
    btn_enabled = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    ng.ng_LeftEdge = 190;
    ng.ng_GadgetText = "Btn Dis";
    ng.ng_GadgetID = GID_BTN_DISABLED;
    btn_disabled = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge += 22;
    ng.ng_Width = 26;
    ng.ng_Height = 11;
    ng.ng_GadgetText = "Check En";
    ng.ng_GadgetID = GID_CB_ENABLED;
    ng.ng_Flags = PLACETEXT_RIGHT;
    gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
                       GTCB_Checked, FALSE,
                       TAG_END);

    ng.ng_LeftEdge = 190;
    ng.ng_GadgetText = "Check Dis";
    ng.ng_GadgetID = GID_CB_DISABLED;
    cb_disabled = gad = CreateGadget(CHECKBOX_KIND, gad, &ng,
                                     GTCB_Checked, FALSE,
                                     TAG_END);

    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge += 22;
    ng.ng_Width = 140;
    ng.ng_Height = 14;
    ng.ng_GadgetText = "Cycle En";
    ng.ng_GadgetID = GID_CY_ENABLED;
    ng.ng_Flags = PLACETEXT_RIGHT;
    gad = CreateGadget(CYCLE_KIND, gad, &ng,
                       GTCY_Labels, (ULONG)CycleLabels,
                       GTCY_Active, 0,
                       TAG_END);

    ng.ng_LeftEdge = 190;
    ng.ng_GadgetText = "Cycle Dis";
    ng.ng_GadgetID = GID_CY_DISABLED;
    cy_disabled = gad = CreateGadget(CYCLE_KIND, gad, &ng,
                                     GTCY_Labels, (ULONG)CycleLabels,
                                     GTCY_Active, 0,
                                     TAG_END);

    if (!gad)
        goto free_gadgets;

    window = OpenWindowTags(NULL,
                            WA_Title, (ULONG)"GadgetDisabled",
                            WA_Gadgets, (ULONG)glist,
                            WA_AutoAdjust, TRUE,
                            WA_Width, 380,
                            WA_InnerHeight, 120,
                            WA_DragBar, TRUE,
                            WA_DepthGadget, TRUE,
                            WA_Activate, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
                            WA_PubScreen, (ULONG)screen,
                            TAG_END);
    if (!window)
        goto free_gadgets;

    GT_RefreshWindow(window, NULL);

    if (btn_disabled) {
        btn_disabled->Flags |= GFLG_DISABLED;
        RefreshGList(btn_disabled, window, NULL, 1);
    }
    if (cb_disabled) {
        cb_disabled->Flags |= GFLG_DISABLED;
        RefreshGList(cb_disabled, window, NULL, 1);
    }
    if (cy_disabled) {
        cy_disabled->Flags |= GFLG_DISABLED;
        RefreshGList(cy_disabled, window, NULL, 1);
    }

    printf("READY: gadgetdisabled\n");

    while (!done)
    {
        Wait(1L << window->UserPort->mp_SigBit);
        while ((imsg = GT_GetIMsg(window->UserPort)) != NULL)
        {
            ULONG cls = imsg->Class;
            UWORD code = imsg->Code;
            GT_ReplyIMsg(imsg);

            if (cls == IDCMP_CLOSEWINDOW)
            {
                done = TRUE;
            }
            else if (cls == IDCMP_REFRESHWINDOW)
            {
                GT_BeginRefresh(window);
                GT_EndRefresh(window, TRUE);
            }
            else if (cls == IDCMP_VANILLAKEY && btn_enabled)
            {
                if (code == 'd')
                {
                    btn_enabled->Flags |= GFLG_DISABLED;
                    RefreshGList(btn_enabled, window, NULL, 1);
                    printf("EVENT: off_button\n");
                }
                else if (code == 'e')
                {
                    btn_enabled->Flags &= ~GFLG_DISABLED;
                    RefreshGList(btn_enabled, window, NULL, 1);
                    printf("EVENT: on_button\n");
                }
            }
        }
    }

    CloseWindow(window);

free_gadgets:
    FreeGadgets(glist);
free_vi:
    FreeVisualInfo(vi);
unlock:
    UnlockPubScreen(NULL, screen);
out:
    if (GadToolsBase)
        CloseLibrary(GadToolsBase);
    if (IntuitionBase)
        CloseLibrary(IntuitionBase);

    return 0;
}
