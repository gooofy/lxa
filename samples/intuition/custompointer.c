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
** custompointer.c - Show the use of a custom busy pointer, as well as
** using a requester to block input to a window.
*/
#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

struct Library *IntuitionBase;


UWORD waitPointer[] =
    {
    0x0000, 0x0000,     /* reserved, must be NULL */

    0x0400, 0x07C0,
    0x0000, 0x07C0,
    0x0100, 0x0380,
    0x0000, 0x07E0,
    0x07C0, 0x1FF8,
    0x1FF0, 0x3FEC,
    0x3FF8, 0x7FDE,
    0x3FF8, 0x7FBE,
    0x7FFC, 0xFF7F,
    0x7EFC, 0xFFFF,
    0x7FFC, 0xFFFF,
    0x3FF8, 0x7FFE,
    0x3FF8, 0x7FFE,
    0x1FF0, 0x3FFC,
    0x07C0, 0x1FF8,
    0x0000, 0x07E0,

    0x0000, 0x0000,     /* reserved, must be NULL */
    };


/*
** The main() routine
*/
VOID main(int argc, char **argv)
{
struct Window *win;
struct Requester null_request;

if (IntuitionBase = OpenLibrary("intuition.library",37))
    {
    /* the window is opened as active (WA_Activate) so that the busy
    ** pointer will be visible.  If the window was not active, the
    ** user would have to activate it to see the change in the pointer.
    */
    if (win = OpenWindowTags(NULL,
                             WA_Activate, TRUE,
                             TAG_END))
        {
        /* a NULL requester can be used to block input
        ** in a window without any imagery provided.
        */
        InitRequester(&null_request);

        Delay(50);  /* simulate activity in the program. */

        /* Put up the requester to block user input in the window,
        ** and set the pointer to the busy pointer.
        */
        if (Request(&null_request, win))
            {
            SetPointer(win, waitPointer, 16, 16, -6, 0);

            Delay(100);  /* simulate activity in the program. */

            /* clear the pointer (which resets the window to the default
            ** pointer) and remove the requester.
            */
            ClearPointer(win);
            EndRequest(&null_request, win);
            }

        Delay(100);  /* simulate activity in the program. */

        CloseWindow(win);
        }
    CloseLibrary(IntuitionBase);
    }
}
