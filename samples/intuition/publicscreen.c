/*
 * publicscreen.c - Public Screen Sample
 *
 * This is an RKM-style sample demonstrating public screen handling.
 * It opens a new screen using the pens from the Workbench screen.
 *
 * Key Functions Demonstrated:
 *   - LockPubScreen() / UnlockPubScreen()
 *   - GetScreenDrawInfo() / FreeScreenDrawInfo()
 *   - OpenScreenTagList() / CloseScreen()
 *   - SA_Pens tag for custom pen colors
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

VOID usePubScreenPens(void);

struct Library *IntuitionBase;

/* main(): open libraries, clean up when done.
*/
VOID main(int argc, char **argv)
{
    printf("PublicScreen: Starting public screen pens demonstration\n");

    IntuitionBase = OpenLibrary("intuition.library", 0);
    if (IntuitionBase != NULL)
    {
        /* Check the version number; Release 2 is required */
        if (IntuitionBase->lib_Version >= 37)
        {
            printf("PublicScreen: Opened intuition.library v%d\n",
                   IntuitionBase->lib_Version);
            usePubScreenPens();
        }
        else
        {
            printf("PublicScreen: Requires intuition.library v37+\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("PublicScreen: Failed to open intuition.library\n");
    }
    printf("PublicScreen: Done\n");
}

/* Open a screen that uses the pens of an existing public screen
** (the Workbench screen in this case).
*/
VOID usePubScreenPens(void)
{
    struct Screen *my_screen;
    struct TagItem screen_tags[3];
    UBYTE *pubScreenName = "Workbench";

    struct Screen *pub_screen = NULL;
    struct DrawInfo *screen_drawinfo = NULL;

    /* Get a lock on the Workbench screen */
    pub_screen = LockPubScreen(pubScreenName);
    if (pub_screen != NULL)
    {
        printf("PublicScreen: LockPubScreen('%s') succeeded\n", pubScreenName);
        printf("PublicScreen: Source screen size: %dx%d\n",
               pub_screen->Width, pub_screen->Height);

        /* get the DrawInfo structure from the locked screen */
        screen_drawinfo = GetScreenDrawInfo(pub_screen);
        if (screen_drawinfo != NULL)
        {
            printf("PublicScreen: GetScreenDrawInfo() succeeded, depth=%d\n",
                   screen_drawinfo->dri_Depth);

            /* The pens are copied in the OpenScreenTagList() call,
            ** so we can simply use a pointer to the pens in the tag list.
            **
            ** Match the depth of the original screen for proper colors.
            */
            screen_tags[0].ti_Tag = SA_Pens;
            screen_tags[0].ti_Data = (ULONG)(screen_drawinfo->dri_Pens);
            screen_tags[1].ti_Tag = SA_Depth;
            screen_tags[1].ti_Data = screen_drawinfo->dri_Depth;
            screen_tags[2].ti_Tag = TAG_END;
            screen_tags[2].ti_Data = 0;

            printf("PublicScreen: Opening screen with inherited pens\n");

            my_screen = OpenScreenTagList(NULL, screen_tags);
            if (my_screen != NULL)
            {
                printf("PublicScreen: OpenScreenTagList() succeeded\n");
                printf("PublicScreen: New screen size: %dx%d depth=%d\n",
                       my_screen->Width, my_screen->Height,
                       my_screen->BitMap.Depth);

                /* We no longer need to hold the lock on the public screen */
                FreeScreenDrawInfo(pub_screen, screen_drawinfo);
                screen_drawinfo = NULL;
                UnlockPubScreen(pubScreenName, pub_screen);
                pub_screen = NULL;

                printf("PublicScreen: Screen displayed (delay 2 sec)\n");
                Delay(100);  /* 2 second delay */

                CloseScreen(my_screen);
                printf("PublicScreen: CloseScreen() - screen closed\n");
            }
            else
            {
                printf("PublicScreen: OpenScreenTagList() failed\n");
            }
        }
        else
        {
            printf("PublicScreen: GetScreenDrawInfo() failed\n");
        }
    }
    else
    {
        printf("PublicScreen: LockPubScreen('%s') failed\n", pubScreenName);
    }

    /* Cleanup if something went wrong */
    if (screen_drawinfo != NULL)
        FreeScreenDrawInfo(pub_screen, screen_drawinfo);
    if (pub_screen != NULL)
        UnlockPubScreen(pubScreenName, pub_screen);
}
