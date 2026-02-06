/*
Copyright (c) 1992 Commodore-Amiga, Inc.

This example is provided in electronic form by Commodore-Amiga, Inc. for
use with the "Amiga ROM Kernel Reference Manual: Libraries", 3rd Edition,
published by Addison-Wesley (ISBN 0-201-56774-1).

The "Amiga ROM Kernel Reference Manual: Libraries" contains additional
information on the correct usage of the techniques and operating system
functions presented in these examples.  The source and executable code
of these examples may only be distributed in free electronic form, via
bulletin board or as part of a fully non-commercial and freely
redistributable diskette.  Both the source and executable code (including
comments) must be included, without modification, in any copy.  This
example may not be published in printed form or distributed with any
commercial product.  However, the programming techniques and support
routines set forth in these examples may be used in the development
of original executable software products for Commodore Amiga computers.

All other rights reserved.

This example is provided "as-is" and is subject to change; no
warranties are made.  All use is at your own risk. No liability or
responsibility is assumed.
*/


/*
** simplegad.c - show the use of a button gadget.
*/

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

#define BUTTON_GAD_NUM   (3)
#define MYBUTTONGADWIDTH (100)
#define MYBUTTONGADHEIGHT (50)

/* NOTE that the use of constant size and positioning values are
** not recommended; it just makes it easy to show what is going on.
** The position of the gadget should be dynamically adjusted depending
** on the height of the font in the title bar of the window.
*/

UWORD buttonBorderData[] =
    {
    0,0, MYBUTTONGADWIDTH + 1,0, MYBUTTONGADWIDTH + 1,MYBUTTONGADHEIGHT + 1,
    0,MYBUTTONGADHEIGHT + 1, 0,0,
    };

struct Border buttonBorder =
    {
    -1,-1,1,0,JAM1,5,buttonBorderData,NULL,
    };

struct Gadget buttonGad =
    {
    NULL, 20,20, MYBUTTONGADWIDTH,MYBUTTONGADHEIGHT,
    GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_IMMEDIATE,
    GTYP_BOOLGADGET, &buttonBorder, NULL, NULL,0,NULL,BUTTON_GAD_NUM,NULL,
    };

/*
** routine to show the use of a button (boolean) gadget.
*/
VOID main(int argc, char **argv)
{
struct Window *win;
struct IntuiMessage *msg;
struct Gadget *gad;
ULONG  class;
BOOL done;

/* make sure to get intuition version 37, for OpenWindowTags() */
IntuitionBase = OpenLibrary("intuition.library", 37);
if (IntuitionBase)
    {
    if (win = OpenWindowTags(NULL,
                            WA_Width, 400,
                            WA_Height, 100,
                            WA_Gadgets, &buttonGad,
                            WA_Activate, TRUE,
                            WA_CloseGadget, TRUE,
                            WA_IDCMP, IDCMP_GADGETDOWN | IDCMP_GADGETUP |
                                      IDCMP_CLOSEWINDOW,
                            TAG_END))
        {
        done = FALSE;
        while (done == FALSE)
            {
            Wait(1L << win->UserPort->mp_SigBit);
            while ( (done == FALSE) &&
                    (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
                {
                /* Stash message contents and reply, important when message
                ** triggers some lengthy processing
                */
                class = msg->Class;

                /* gadget address is ONLY valid for gadget messages! */
                if ((class == IDCMP_GADGETUP) || (class == IDCMP_GADGETDOWN))
                    gad = (struct Gadget *)(msg->IAddress);

                ReplyMsg((struct Message *)msg);

                /* switch on the type of the event */
                switch (class)
                    {
                    case IDCMP_GADGETUP:
                        /* caused by GACT_RELVERIFY */
                        printf("received an IDCMP_GADGETUP, gadget number %d\n",
                               gad->GadgetID);
                        break;
                    case IDCMP_GADGETDOWN:
                        /* caused by GACT_IMMEDIATE */
                        printf("received an IDCMP_GADGETDOWN, gadget number %d\n",
                               gad->GadgetID);
                        break;
                    case IDCMP_CLOSEWINDOW:
                        /* set a flag that we are done processing events... */
                        printf("received an IDCMP_CLOSEWINDOW\n");
                        done = TRUE;
                        break;
                    }
                }
            }
        CloseWindow(win);
        }
    CloseLibrary(IntuitionBase);
    }
}
