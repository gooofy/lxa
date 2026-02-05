/*
 * custompointer.c - Custom Mouse Pointer Sample
 *
 * This is an RKM-style sample demonstrating custom mouse pointer handling.
 * It creates a window that:
 *   - Uses SetPointer() to set a custom busy/wait pointer
 *   - Uses ClearPointer() to restore the default pointer
 *   - Uses a NULL requester to block input while busy
 *   - Demonstrates Request()/EndRequest() for input blocking
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

/* Custom wait/busy pointer - hourglass/watch shape
 * Format: Each row is 2 words (plane 0, plane 1)
 * 16 pixels wide, 16 pixels tall plus 2 reserved words at start and end
 */
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

    printf("CustomPointer: Starting custom pointer demonstration\n");

    if (IntuitionBase = OpenLibrary("intuition.library", 37))
    {
        printf("CustomPointer: Opened intuition.library\n");
        
        /* the window is opened as active (WA_Activate) so that the busy
        ** pointer will be visible.  If the window was not active, the
        ** user would have to activate it to see the change in the pointer.
        */
        if (win = OpenWindowTags(NULL,
                                 WA_Width, 200,
                                 WA_Height, 100,
                                 WA_Activate, TRUE,
                                 WA_CloseGadget, TRUE,
                                 WA_DragBar, TRUE,
                                 WA_DepthGadget, TRUE,
                                 WA_Title, "CustomPointer Demo",
                                 TAG_END))
        {
            printf("CustomPointer: Window opened successfully\n");
            
            /* a NULL requester can be used to block input
            ** in a window without any imagery provided.
            */
            InitRequester(&null_request);
            printf("CustomPointer: Initialized NULL requester\n");

            /* First delay - normal window state */
            printf("CustomPointer: Normal pointer active (delay 1 sec)\n");
            Delay(50);  /* 1 second delay */

            /* Put up the requester to block user input in the window,
            ** and set the pointer to the busy pointer.
            */
            if (Request(&null_request, win))
            {
                printf("CustomPointer: Request() succeeded - input blocked\n");
                
                /* Set the custom busy pointer
                 * Parameters: window, pointer data, height=16, width=16, xOffset=-6, yOffset=0
                 */
                SetPointer(win, waitPointer, 16, 16, -6, 0);
                printf("CustomPointer: SetPointer() - busy pointer active (delay 2 sec)\n");

                Delay(100);  /* 2 second delay with busy pointer */

                /* clear the pointer (which resets the window to the default
                ** pointer) and remove the requester.
                */
                ClearPointer(win);
                printf("CustomPointer: ClearPointer() - default pointer restored\n");
                
                EndRequest(&null_request, win);
                printf("CustomPointer: EndRequest() - input unblocked\n");
            }
            else
            {
                printf("CustomPointer: Request() failed\n");
            }

            /* Final delay - back to normal */
            printf("CustomPointer: Back to normal state (delay 1 sec)\n");
            Delay(50);  /* 1 second delay */

            CloseWindow(win);
            printf("CustomPointer: Window closed\n");
        }
        else
        {
            printf("CustomPointer: Failed to open window\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("CustomPointer: Failed to open intuition.library\n");
    }
    printf("CustomPointer: Done\n");
}
