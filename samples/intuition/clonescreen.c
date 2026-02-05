/*
 * clonescreen.c - Screen Cloning Sample
 *
 * This is an RKM-style sample demonstrating screen cloning in Intuition.
 * It clones an existing public screen (Workbench) by copying its:
 *   - Width, Height, Depth
 *   - Pens (drawing colors)
 *   - Font
 *   - DisplayID
 *
 * Key Functions Demonstrated:
 *   - LockPubScreen() / UnlockPubScreen()
 *   - GetScreenDrawInfo() / FreeScreenDrawInfo()
 *   - GetVPModeID()
 *   - OpenScreenTags() / CloseScreen()
 *   - OpenFont() / CloseFont()
 *
 * Based on RKM Intuition sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>
#include <string.h>

VOID cloneScreen(UBYTE *);

struct Library *IntuitionBase;
struct GfxBase *GfxBase;

/*
** Open all libraries for the cloneScreen() subroutine.
*/
VOID main(int argc, char **argv)
{
    UBYTE *pub_screen_name = "Workbench";

    printf("CloneScreen: Starting screen cloning demonstration\n");

    IntuitionBase = OpenLibrary("intuition.library", 0);
    if (IntuitionBase != NULL)
    {
        /* Require version 37 of Intuition. */
        if (IntuitionBase->lib_Version >= 37)
        {
            printf("CloneScreen: Opened intuition.library v%d\n", 
                   IntuitionBase->lib_Version);
            
            GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
            if (GfxBase != NULL)
            {
                printf("CloneScreen: Opened graphics.library\n");
                
                cloneScreen(pub_screen_name);

                CloseLibrary((struct Library *)GfxBase);
            }
            else
            {
                printf("CloneScreen: Failed to open graphics.library v37\n");
            }
        }
        else
        {
            printf("CloneScreen: Requires intuition.library v37+\n");
        }
        CloseLibrary(IntuitionBase);
    }
    else
    {
        printf("CloneScreen: Failed to open intuition.library\n");
    }
    printf("CloneScreen: Done\n");
}

/* Clone a public screen whose name is passed to the routine.
**    Width, Height, Depth, Pens, Font and DisplayID attributes are
** all copied from the screen.
*/
VOID cloneScreen(UBYTE *pub_screen_name)
{
    struct Screen *my_screen;
    ULONG screen_modeID;
    UBYTE *pub_scr_font_name;
    UBYTE *font_name;
    ULONG font_name_size;
    struct TextAttr pub_screen_font;
    struct TextFont *opened_font;

    struct Screen *pub_screen = NULL;
    struct DrawInfo *screen_drawinfo = NULL;

    printf("CloneScreen: Attempting to clone '%s' screen\n", pub_screen_name);

    /* name is a (UBYTE *) pointer to the name of the public screen to clone */
    pub_screen = LockPubScreen(pub_screen_name);
    if (pub_screen != NULL)
    {
        printf("CloneScreen: LockPubScreen('%s') succeeded\n", pub_screen_name);
        printf("CloneScreen: Source screen size: %dx%d\n", 
               pub_screen->Width, pub_screen->Height);

        /* Get the DrawInfo structure from the locked screen */
        screen_drawinfo = GetScreenDrawInfo(pub_screen);
        if (screen_drawinfo != NULL)
        {
            printf("CloneScreen: GetScreenDrawInfo() succeeded, depth=%d\n",
                   screen_drawinfo->dri_Depth);

            screen_modeID = GetVPModeID(&(pub_screen->ViewPort));
            if (screen_modeID != INVALID_ID)
            {
                printf("CloneScreen: GetVPModeID() returned 0x%lx\n", screen_modeID);

                /* Get a copy of the font */
                pub_scr_font_name = screen_drawinfo->dri_Font->tf_Message.mn_Node.ln_Name;
                font_name_size = 1 + strlen(pub_scr_font_name);
                font_name = AllocMem(font_name_size, MEMF_CLEAR);
                if (font_name != NULL)
                {
                    strcpy(font_name, pub_scr_font_name);
                    pub_screen_font.ta_Name = font_name;
                    pub_screen_font.ta_YSize = screen_drawinfo->dri_Font->tf_YSize;
                    pub_screen_font.ta_Style = screen_drawinfo->dri_Font->tf_Style;
                    pub_screen_font.ta_Flags = screen_drawinfo->dri_Font->tf_Flags;

                    printf("CloneScreen: Source font: '%s' size=%d\n",
                           font_name, pub_screen_font.ta_YSize);

                    opened_font = OpenFont(&pub_screen_font);
                    if (opened_font != NULL)
                    {
                        printf("CloneScreen: OpenFont() succeeded\n");

                        /* Open the cloned screen */
                        my_screen = OpenScreenTags(NULL,
                            SA_Width,      pub_screen->Width,
                            SA_Height,     pub_screen->Height,
                            SA_Depth,      screen_drawinfo->dri_Depth,
                            SA_Overscan,   OSCAN_TEXT,
                            SA_AutoScroll, TRUE,
                            SA_Pens,       (ULONG)(screen_drawinfo->dri_Pens),
                            SA_Font,       (ULONG)&pub_screen_font,
                            SA_DisplayID,  screen_modeID,
                            SA_Title,      "Cloned Screen",
                            TAG_END);
                        if (my_screen != NULL)
                        {
                            printf("CloneScreen: OpenScreenTags() succeeded\n");
                            printf("CloneScreen: Cloned screen size: %dx%d depth=%d\n",
                                   my_screen->Width, my_screen->Height,
                                   my_screen->BitMap.Depth);

                            /* Free the drawinfo and public screen */
                            FreeScreenDrawInfo(pub_screen, screen_drawinfo);
                            screen_drawinfo = NULL;
                            UnlockPubScreen(pub_screen_name, pub_screen);
                            pub_screen = NULL;

                            printf("CloneScreen: Screen displayed (delay 2 sec)\n");
                            Delay(100);  /* 2 second delay */

                            CloseScreen(my_screen);
                            printf("CloneScreen: CloseScreen() - cloned screen closed\n");
                        }
                        else
                        {
                            printf("CloneScreen: OpenScreenTags() failed\n");
                        }
                        CloseFont(opened_font);
                    }
                    else
                    {
                        printf("CloneScreen: OpenFont() failed\n");
                    }
                    FreeMem(font_name, font_name_size);
                }
                else
                {
                    printf("CloneScreen: Failed to allocate font name\n");
                }
            }
            else
            {
                printf("CloneScreen: GetVPModeID() returned INVALID_ID\n");
            }
        }
        else
        {
            printf("CloneScreen: GetScreenDrawInfo() failed\n");
        }
    }
    else
    {
        printf("CloneScreen: LockPubScreen('%s') failed\n", pub_screen_name);
    }

    /* Cleanup if something went wrong */
    if (screen_drawinfo != NULL)
        FreeScreenDrawInfo(pub_screen, screen_drawinfo);
    if (pub_screen != NULL)
        UnlockPubScreen(pub_screen_name, pub_screen);
}
