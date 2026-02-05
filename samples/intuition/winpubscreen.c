/*
 * winpubscreen.c - Windows on Public Screens Sample
 *
 * This is an RKM-style sample demonstrating opening a window on a public screen.
 * It opens a window on the default public screen (Workbench).
 *
 * Key Functions Demonstrated:
 *   - LockPubScreen(NULL) to get the default public screen
 *   - WA_PubScreen tag to open window on a specific screen
 *   - UnlockPubScreen() after window is open (window acts as a lock)
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *IntuitionBase;

/*
** Open a simple window on the default public screen,
** then leave it open for a delay before closing.
*/
VOID main(int argc, char **argv)
{
    struct Window *test_window = NULL;
    struct Screen *test_screen = NULL;

    printf("WinPubScreen: Starting window on public screen demonstration\n");

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (IntuitionBase)
    {
        printf("WinPubScreen: Opened intuition.library v%d\n",
               IntuitionBase->lib_Version);

        /* get a lock on the default public screen */
        if (test_screen = LockPubScreen(NULL))
        {
            printf("WinPubScreen: LockPubScreen(NULL) succeeded\n");
            printf("WinPubScreen: Public screen size: %dx%d\n",
                   test_screen->Width, test_screen->Height);

            /* open the window on the public screen */
            test_window = OpenWindowTags(NULL,
                    WA_Left,  10,    WA_Top,    20,
                    WA_Width, 300,   WA_Height, 100,
                    WA_DragBar,         TRUE,
                    WA_CloseGadget,     TRUE,
                    WA_DepthGadget,     TRUE,
                    WA_SmartRefresh,    TRUE,
                    WA_NoCareRefresh,   TRUE,
                    WA_IDCMP,           IDCMP_CLOSEWINDOW,
                    WA_Title,           "Window On Public Screen",
                    WA_PubScreen,       test_screen,
                    TAG_END);

            /* Unlock the screen. The window now acts as a lock on
            ** the screen, and we do not need the screen after the
            ** window has been closed.
            */
            UnlockPubScreen(NULL, test_screen);
            printf("WinPubScreen: UnlockPubScreen() - screen unlocked\n");

            /* if we have a valid window open, display it then close */
            if (test_window)
            {
                printf("WinPubScreen: OpenWindowTags() succeeded\n");
                printf("WinPubScreen: Window at (%d,%d) size %dx%d\n",
                       test_window->LeftEdge, test_window->TopEdge,
                       test_window->Width, test_window->Height);

                printf("WinPubScreen: Window displayed (delay 2 sec)\n");
                Delay(100);  /* 2 second delay */

                CloseWindow(test_window);
                printf("WinPubScreen: CloseWindow() - window closed\n");
            }
            else
            {
                printf("WinPubScreen: OpenWindowTags() failed\n");
            }
        }
        else
        {
            printf("WinPubScreen: LockPubScreen(NULL) failed\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("WinPubScreen: Failed to open intuition.library v37\n");
    }
    printf("WinPubScreen: Done\n");
}
