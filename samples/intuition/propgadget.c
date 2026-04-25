/*
 * propgadget.c - Standalone PROPGADGET sample for Phase 152.
 *
 * Opens a window containing one vertical and one horizontal proportional
 * gadget so tests can verify the recessed track-frame chrome (shadow on
 * top/left, shine on bottom/right) drawn around each prop gadget.
 *
 * Both gadgets use AUTOKNOB | PROPNEWLOOK and a partial body so that an
 * exposed track strip is visible alongside the knob.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

#define VPROP_ID 1
#define HPROP_ID 2

#define VPROP_LEFT   30
#define VPROP_TOP    20
#define VPROP_WIDTH  16
#define VPROP_HEIGHT 100

#define HPROP_LEFT   60
#define HPROP_TOP    20
#define HPROP_WIDTH  200
#define HPROP_HEIGHT 14

static struct PropInfo vpropInfo =
{
    AUTOKNOB | FREEVERT | PROPNEWLOOK,
    0, 0,        /* HorizPot, VertPot */
    0, 0x4000    /* HorizBody, VertBody (~25%) */
};

static struct Image vpropImage = { 0 };

static struct Gadget vpropGad =
{
    NULL,
    VPROP_LEFT, VPROP_TOP,
    VPROP_WIDTH, VPROP_HEIGHT,
    GFLG_GADGHCOMP, GACT_IMMEDIATE | GACT_FOLLOWMOUSE | GACT_RELVERIFY,
    GTYP_PROPGADGET, &vpropImage, NULL, NULL,
    0, (APTR)&vpropInfo, VPROP_ID, NULL,
};

static struct PropInfo hpropInfo =
{
    AUTOKNOB | FREEHORIZ | PROPNEWLOOK,
    0, 0,
    0x4000, 0
};

static struct Image hpropImage = { 0 };

static struct Gadget hpropGad =
{
    &vpropGad,
    HPROP_LEFT, HPROP_TOP,
    HPROP_WIDTH, HPROP_HEIGHT,
    GFLG_GADGHCOMP, GACT_IMMEDIATE | GACT_FOLLOWMOUSE | GACT_RELVERIFY,
    GTYP_PROPGADGET, &hpropImage, NULL, NULL,
    0, (APTR)&hpropInfo, HPROP_ID, NULL,
};

VOID main(int argc, char **argv)
{
    struct Window *win;
    struct IntuiMessage *msg;
    BOOL done;
    ULONG class;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase) return;

    win = OpenWindowTags(NULL,
        WA_Width, 320,
        WA_Height, 160,
        WA_Title, (ULONG)"PropGad",
        WA_Gadgets, (ULONG)&hpropGad,
        WA_Activate, TRUE,
        WA_CloseGadget, TRUE,
        WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP | IDCMP_CLOSEWINDOW,
        TAG_END);

    if (win)
    {
        printf("propgadget ready\n");

        done = FALSE;
        while (!done)
        {
            Wait(1L << win->UserPort->mp_SigBit);
            while (!done && (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
            {
                class = msg->Class;
                ReplyMsg((struct Message *)msg);
                if (class == IDCMP_CLOSEWINDOW) done = TRUE;
            }
        }
        CloseWindow(win);
    }

    CloseLibrary(IntuitionBase);
}
