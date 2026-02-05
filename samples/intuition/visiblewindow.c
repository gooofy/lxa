/*
 * visiblewindow.c - Visible Window Placement Sample
 *
 * This is an RKM-style sample demonstrating proper window placement on a screen.
 * It opens a window as large as the visible part of the screen using:
 *   - QueryOverscan() to get the OSCAN_TEXT display rectangle
 *   - GetVPModeID() to get the screen's display mode
 *   - Proper coordinate calculations for scrolled screens
 *
 * Key Functions Demonstrated:
 *   - LockPubScreen() / UnlockPubScreen()
 *   - GetVPModeID()
 *   - QueryOverscan()
 *   - Coordinate calculations for proper window placement
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/displayinfo.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

/* Minimum window width and height */
#define MIN_WINDOW_WIDTH  (100)
#define MIN_WINDOW_HEIGHT (50)

/* min/max macros */
#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<=(b)?(a):(b))

struct Library *IntuitionBase;
struct Library *GfxBase;

/*
** open all the libraries and run the code. Cleanup when done.
*/
VOID main(int argc, char **argv)
{
    struct Window *test_window;
    struct Screen *pub_screen;
    struct Rectangle rect;
    ULONG screen_modeID;
    LONG width, height, left, top;

    printf("VisibleWindow: Starting visible window placement demonstration\n");

    /* these calls are only valid if we have Intuition version 37 or greater */
    if (GfxBase = OpenLibrary("graphics.library", 37))
    {
        printf("VisibleWindow: Opened graphics.library\n");
        
        if (IntuitionBase = OpenLibrary("intuition.library", 37))
        {
            printf("VisibleWindow: Opened intuition.library\n");

            /* set some reasonable defaults */
            left = 0;
            top = 0;
            width = 640;
            height = 200;

            /* get a lock on the default public screen */
            if (NULL != (pub_screen = LockPubScreen(NULL)))
            {
                printf("VisibleWindow: LockPubScreen(NULL) succeeded\n");
                printf("VisibleWindow: Screen size: %dx%d at (%d,%d)\n",
                       pub_screen->Width, pub_screen->Height,
                       pub_screen->LeftEdge, pub_screen->TopEdge);

                /* GetVPModeID() is a graphics call... */
                screen_modeID = GetVPModeID(&pub_screen->ViewPort);
                if (screen_modeID != INVALID_ID)
                {
                    printf("VisibleWindow: GetVPModeID() returned 0x%lx\n", screen_modeID);

                    if (QueryOverscan(screen_modeID, &rect, OSCAN_TEXT))
                    {
                        printf("VisibleWindow: QueryOverscan() OSCAN_TEXT: (%d,%d)-(%d,%d)\n",
                               rect.MinX, rect.MinY, rect.MaxX, rect.MaxY);

                        /* make sure window coordinates are positive or zero */
                        left = max(0, -pub_screen->LeftEdge);
                        top = max(0, -pub_screen->TopEdge);

                        /* get width and height from size of display clip */
                        width = rect.MaxX - rect.MinX + 1;
                        height = rect.MaxY - rect.MinY + 1;

                        /* adjust height for pulled-down screen */
                        if (pub_screen->TopEdge > 0)
                            height -= pub_screen->TopEdge;

                        /* insure that window fits on screen */
                        height = min(height, pub_screen->Height);
                        width = min(width, pub_screen->Width);

                        /* make sure window is at least minimum size */
                        width = max(width, MIN_WINDOW_WIDTH);
                        height = max(height, MIN_WINDOW_HEIGHT);

                        printf("VisibleWindow: Calculated window: left=%ld top=%ld width=%ld height=%ld\n",
                               left, top, width, height);
                    }
                    else
                    {
                        printf("VisibleWindow: QueryOverscan() failed\n");
                    }
                }
                else
                {
                    printf("VisibleWindow: GetVPModeID() returned INVALID_ID\n");
                }

                /* open the window on the public screen */
                test_window = OpenWindowTags(NULL,
                        WA_Left, left,            WA_Width, width,
                        WA_Top, top,              WA_Height, height,
                        WA_CloseGadget, TRUE,
                        WA_DragBar, TRUE,
                        WA_DepthGadget, TRUE,
                        WA_IDCMP, IDCMP_CLOSEWINDOW,
                        WA_Title, "Visible Window",
                        WA_PubScreen, pub_screen,
                        TAG_END);

                /* unlock the screen. The window now acts as a lock. */
                UnlockPubScreen(NULL, pub_screen);
                printf("VisibleWindow: UnlockPubScreen() - screen unlocked\n");

                if (test_window)
                {
                    printf("VisibleWindow: OpenWindowTags() succeeded\n");
                    printf("VisibleWindow: Window at (%d,%d) size %dx%d\n",
                           test_window->LeftEdge, test_window->TopEdge,
                           test_window->Width, test_window->Height);

                    printf("VisibleWindow: Window displayed (delay 2 sec)\n");
                    Delay(100);  /* 2 second delay */

                    CloseWindow(test_window);
                    printf("VisibleWindow: CloseWindow() - window closed\n");
                }
                else
                {
                    printf("VisibleWindow: OpenWindowTags() failed\n");
                }
            }
            else
            {
                printf("VisibleWindow: LockPubScreen(NULL) failed\n");
            }

            CloseLibrary(IntuitionBase);
        }
        else
        {
            printf("VisibleWindow: Failed to open intuition.library v37\n");
        }
        CloseLibrary(GfxBase);
    }
    else
    {
        printf("VisibleWindow: Failed to open graphics.library v37\n");
    }
    printf("VisibleWindow: Done\n");
}
