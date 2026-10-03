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

/* GadgetType bit GadTools sets on all its gadgets (not in the NDK) */
#define GTYP_GADTOOLS 0x0100

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
    struct Gadget *context;
    struct Gadget *button;
    struct Gadget *string_gad;
    struct NewGadget ng;
    struct Window *win = NULL;

    printf("Testing GadTools context and refresh wrappers...\n");

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

    context = CreateContext(&glist);
    if (!context || glist != context)
        return fail("CreateContext did not initialize the gadget list");
    /* AmigaOS 3.1: an invisible, zero-size GadTools gadget without a class */
    if (context->GadgetType != GTYP_GADTOOLS)
        return fail("context gadget type mismatch");
    if (context->Width != 0 || context->Height != 0 ||
        (context->Flags & GFLG_GADGHIGHBITS) != GFLG_GADGHNONE)
        return fail("context gadget should be an invisible zero-size gadget");
    if (context->NextGadget != NULL)
        return fail("fresh context gadget should not have a successor");
    printf("OK: CreateContext initialized gadget list head\n");

    memset(&ng, 0, sizeof(ng));
    ng.ng_VisualInfo = vi;
    ng.ng_TextAttr = screen->Font;
    ng.ng_LeftEdge = 20;
    ng.ng_TopEdge = screen->WBorTop + screen->Font->ta_YSize + 21;
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = (CONST_STRPTR)"Button";
    ng.ng_GadgetID = 1;

    button = CreateGadget(BUTTON_KIND, context, &ng, TAG_DONE);
    if (!button || context->NextGadget != button)
        return fail("button gadget creation/linking failed");

    ng.ng_TopEdge += 20;
    ng.ng_GadgetText = (CONST_STRPTR)"String";
    ng.ng_GadgetID = 2;
    string_gad = CreateGadget(STRING_KIND, button, &ng,
        GTST_String, (ULONG)"refresh",
        GTST_MaxChars, 16,
        TAG_DONE);
    if (!string_gad || button->NextGadget != string_gad)
        return fail("string gadget creation/linking failed");
    if (glist != context)
        return fail("context head should stay stable after gadget appends");

    win = OpenWindowTags(NULL,
        WA_Title, (ULONG)"Context Refresh Test",
        WA_Gadgets, (ULONG)glist,
        WA_AutoAdjust, TRUE,
        WA_Width, 260,
        WA_InnerHeight, 90,
        WA_DragBar, TRUE,
        WA_DepthGadget, TRUE,
        WA_CloseGadget, TRUE,
        WA_SimpleRefresh, TRUE,
        WA_PubScreen, (ULONG)screen,
        WA_IDCMP, IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if (!win)
        return fail("OpenWindowTags failed");
    /* The context gadget stays in the window's list (after the window's
     * own system gadgets), directly followed by the application gadgets. */
    {
        struct Gadget *g;

        for (g = win->FirstGadget; g && g != context; g = g->NextGadget)
            ;
        if (!g)
            return fail("OpenWindowTags should keep the context gadget in the window gadget list");
        if (context->NextGadget != button || button->NextGadget != string_gad)
            return fail("application gadgets should follow the context gadget");
    }
    printf("OK: window gadget list contains the context gadget and its gadgets\n");

    GT_RefreshWindow(win, NULL);
    printf("OK: GT_RefreshWindow accepted a window gadget list\n");

    GT_BeginRefresh(win);
    if (!(win->Flags & WFLG_WINDOWREFRESH))
        return fail("GT_BeginRefresh did not enter refresh mode");
    GT_EndRefresh(win, TRUE);
    if (win->Flags & WFLG_WINDOWREFRESH)
        return fail("GT_EndRefresh did not leave refresh mode");
    printf("OK: GT_BeginRefresh/GT_EndRefresh wrapped Intuition refresh state\n");

    CloseWindow(win);
    FreeGadgets(glist);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, screen);
    CloseLibrary(GadToolsBase);
    CloseLibrary(IntuitionBase);

    printf("PASS: context/refresh test complete\n");
    return 0;
}
