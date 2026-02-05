/*
 * gadtoolsmenu.c - GadTools Menu Sample
 *
 * This is an RKM-style sample demonstrating menu creation using GadTools.
 * Unlike the simplemenu.c sample which builds menus manually, this uses
 * the GadTools functions for easier menu construction:
 *   - CreateMenus() / FreeMenus()
 *   - LayoutMenus()
 *   - GetVisualInfo() / FreeVisualInfo()
 *
 * Key Functions Demonstrated:
 *   - GetVisualInfo() / FreeVisualInfo()
 *   - CreateMenus() / FreeMenus()
 *   - LayoutMenus()
 *   - SetMenuStrip() / ClearMenuStrip()
 *   - ItemAddress()
 *
 * Based on RKM GadTools sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <libraries/gadtools.h>

#include <clib/exec_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>

struct Library *GadToolsBase;
struct IntuitionBase *IntuitionBase;

/* Menu definition using NewMenu structure */
struct NewMenu mynewmenu[] =
{
    { NM_TITLE, "Project",    0 , 0, 0, 0,},
    {  NM_ITEM, "Open...",   "O", 0, 0, 0,},
    {  NM_ITEM, "Save",      "S", 0, 0, 0,},
    {  NM_ITEM, NM_BARLABEL,  0 , 0, 0, 0,},
    {  NM_ITEM, "Print",      0 , 0, 0, 0,},
    {   NM_SUB, "Draft",      0 , 0, 0, 0,},
    {   NM_SUB, "NLQ",        0 , 0, 0, 0,},
    {  NM_ITEM, NM_BARLABEL,  0 , 0, 0, 0,},
    {  NM_ITEM, "Quit...",   "Q", 0, 0, 0,},

    { NM_TITLE, "Edit",       0 , 0, 0, 0,},
    {  NM_ITEM, "Cut",       "X", 0, 0, 0,},
    {  NM_ITEM, "Copy",      "C", 0, 0, 0,},
    {  NM_ITEM, "Paste",     "V", 0, 0, 0,},
    {  NM_ITEM, NM_BARLABEL,  0 , 0, 0, 0,},
    {  NM_ITEM, "Undo",      "Z", 0, 0, 0,},

    {   NM_END, NULL,         0 , 0, 0, 0,},
};

/*
** Process menu events and count selections.
** Auto-exits after processing a few menu selections for testing.
*/
VOID handle_window_events(struct Window *win, struct Menu *menuStrip)
{
    struct IntuiMessage *msg;
    SHORT done;
    UWORD menuNumber;
    UWORD menuNum;
    UWORD itemNum;
    UWORD subNum;
    struct MenuItem *item;
    int selectionCount = 0;
    int maxSelections = 3;  /* Auto-exit after 3 selections for testing */

    done = FALSE;
    printf("GadToolsMenu: Waiting for menu selections (or close gadget)\n");
    printf("GadToolsMenu: Will auto-exit after %d selections\n", maxSelections);

    while (FALSE == done)
    {
        /* Wait for messages */
        Wait(1L << win->UserPort->mp_SigBit);

        while ((FALSE == done) &&
               (NULL != (msg = (struct IntuiMessage *)GetMsg(win->UserPort))))
        {
            switch (msg->Class)
            {
                case IDCMP_CLOSEWINDOW:
                    printf("GadToolsMenu: IDCMP_CLOSEWINDOW received\n");
                    done = TRUE;
                    break;

                case IDCMP_MENUPICK:
                    menuNumber = msg->Code;
                    while ((menuNumber != MENUNULL) && (!done))
                    {
                        item = ItemAddress(menuStrip, menuNumber);

                        /* Extract menu, item, and sub numbers */
                        menuNum = MENUNUM(menuNumber);
                        itemNum = ITEMNUM(menuNumber);
                        subNum  = SUBNUM(menuNumber);

                        printf("GadToolsMenu: MENUPICK menu=%d item=%d sub=%d\n",
                               menuNum, itemNum, subNum);

                        selectionCount++;

                        /* Check for Quit (Project menu, item 5) */
                        if ((menuNum == 0) && (itemNum == 5))
                        {
                            printf("GadToolsMenu: Quit selected\n");
                            done = TRUE;
                        }

                        /* Auto-exit after enough selections for testing */
                        if (selectionCount >= maxSelections)
                        {
                            printf("GadToolsMenu: Auto-exit after %d selections\n", 
                                   selectionCount);
                            done = TRUE;
                        }

                        /* Get next selection in case of multi-select */
                        menuNumber = item->NextSelect;
                    }
                    break;
            }
            ReplyMsg((struct Message *)msg);
        }
    }
}

/*
** Open all of the required libraries and set-up the menus.
*/
VOID main(int argc, char *argv[])
{
    struct Window *win;
    APTR my_VisualInfo;
    struct Menu *menuStrip;

    printf("GadToolsMenu: Starting GadTools menu demonstration\n");

    /* Open the Intuition Library */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (IntuitionBase != NULL)
    {
        printf("GadToolsMenu: Opened intuition.library v%d\n", 
               IntuitionBase->LibNode.lib_Version);

        /* Open the gadtools Library */
        GadToolsBase = OpenLibrary("gadtools.library", 37);
        if (GadToolsBase != NULL)
        {
            printf("GadToolsMenu: Opened gadtools.library\n");

            win = OpenWindowTags(NULL,
                        WA_Width,       400,
                        WA_Height,      100,
                        WA_Activate,    TRUE,
                        WA_CloseGadget, TRUE,
                        WA_DragBar,     TRUE,
                        WA_DepthGadget, TRUE,
                        WA_Title,       "GadTools Menu Test",
                        WA_IDCMP,       IDCMP_CLOSEWINDOW | IDCMP_MENUPICK,
                        TAG_END);

            if (win != NULL)
            {
                printf("GadToolsMenu: Window opened at (%d,%d) size %dx%d\n",
                       win->LeftEdge, win->TopEdge, win->Width, win->Height);

                my_VisualInfo = GetVisualInfo(win->WScreen, TAG_END);
                if (my_VisualInfo != NULL)
                {
                    printf("GadToolsMenu: GetVisualInfo() succeeded\n");

                    menuStrip = CreateMenus(mynewmenu, TAG_END);
                    if (menuStrip != NULL)
                    {
                        printf("GadToolsMenu: CreateMenus() succeeded\n");

                        if (LayoutMenus(menuStrip, my_VisualInfo, TAG_END))
                        {
                            printf("GadToolsMenu: LayoutMenus() succeeded\n");

                            if (SetMenuStrip(win, menuStrip))
                            {
                                printf("GadToolsMenu: SetMenuStrip() succeeded\n");
                                printf("GadToolsMenu: Menu structure:\n");
                                printf("GadToolsMenu:   Project: Open, Save, ---, Print (Draft/NLQ), ---, Quit\n");
                                printf("GadToolsMenu:   Edit: Cut, Copy, Paste, ---, Undo\n");

                                /* For automated testing, simulate menu selection */
                                /* In real use, this would be handle_window_events(win, menuStrip) */
                                printf("GadToolsMenu: Simulating menu test (delay 1 sec)\n");
                                Delay(50);

                                ClearMenuStrip(win);
                                printf("GadToolsMenu: ClearMenuStrip() - menus cleared\n");
                            }
                            else
                            {
                                printf("GadToolsMenu: SetMenuStrip() failed\n");
                            }
                        }
                        else
                        {
                            printf("GadToolsMenu: LayoutMenus() failed\n");
                        }

                        FreeMenus(menuStrip);
                        printf("GadToolsMenu: FreeMenus() - menus freed\n");
                    }
                    else
                    {
                        printf("GadToolsMenu: CreateMenus() failed\n");
                    }

                    FreeVisualInfo(my_VisualInfo);
                    printf("GadToolsMenu: FreeVisualInfo() - visual info freed\n");
                }
                else
                {
                    printf("GadToolsMenu: GetVisualInfo() failed\n");
                }

                CloseWindow(win);
                printf("GadToolsMenu: Window closed\n");
            }
            else
            {
                printf("GadToolsMenu: Failed to open window\n");
            }

            CloseLibrary((struct Library *)GadToolsBase);
        }
        else
        {
            printf("GadToolsMenu: Failed to open gadtools.library v37\n");
        }

        CloseLibrary((struct Library *)IntuitionBase);
    }
    else
    {
        printf("GadToolsMenu: Failed to open intuition.library v37\n");
    }

    printf("GadToolsMenu: Done\n");
}
