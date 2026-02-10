/* menulayout.c - Adapted from RKM sample
 * Example showing how to do menu layout in general.  This example
 * also illustrates handling menu events, including IDCMP_MENUHELP events.
 *
 * Note that handling arbitrary fonts is fairly complex.  Applications that require V37
 * should use the simpler menu layout routines found in the GadTools library.
 *
 * Original: rkm/libraries/intuition/menus/menulayout.c
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Our function prototypes */
BOOL processMenus(USHORT selection, BOOL done);
BOOL handleIDCMP(struct Window *win);
USHORT MaxLength(struct RastPort *textRPort, struct MenuItem *first_item, USHORT char_size);
VOID setITextAttr(struct IntuiText *first_IText, struct TextAttr *textAttr);
VOID adjustItems(struct RastPort *textRPort, struct MenuItem *first_item, struct TextAttr *textAttr,
                 USHORT char_size, USHORT height, USHORT level, USHORT left_edge);
BOOL adjustMenus(struct Menu *first_menu, struct TextAttr *textAttr);
LONG doWindow(void);

/* Settings Item IntuiText */
struct IntuiText SettText[] = {
        {0,1,JAM2,2,         1, NULL, "Sound...",        NULL },
        {0,1,JAM2,CHECKWIDTH,1, NULL, " Auto Save",      NULL },
        {0,1,JAM2,CHECKWIDTH,1, NULL, " Have Your Cake", NULL },
        {0,1,JAM2,CHECKWIDTH,1, NULL, " Eat It Too",     NULL }
    };

struct MenuItem SettItem[] = {
        { /* "Sound..." */
            &SettItem[1], 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&SettText[0], NULL, NULL, NULL, MENUNULL },
        { /* "Auto Save" (toggle-select, initially selected) */
            &SettItem[2], 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP|CHECKIT|MENUTOGGLE|CHECKED, 0,
            (APTR)&SettText[1], NULL, NULL, NULL, MENUNULL },
        { /* "Have Your Cake" (initially selected, excludes "Eat It Too") */
            &SettItem[3], 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP|CHECKIT|CHECKED, 8,
            (APTR)&SettText[2], NULL, NULL, NULL, MENUNULL },
        { /* "Eat It Too" (excludes "Have Your Cake") */
            NULL, 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP|CHECKIT, 4,
            (APTR)&SettText[3], NULL, NULL, NULL, MENUNULL }
    };

/* Edit Menu Item IntuiText */
struct IntuiText EditText[] = {
        {0,1,JAM2,2,1, NULL, "Cut",       NULL },
        {0,1,JAM2,2,1, NULL, "Copy",      NULL },
        {0,1,JAM2,2,1, NULL, "Paste",     NULL },
        {0,1,JAM2,2,1, NULL, "Erase",     NULL },
        {0,1,JAM2,2,1, NULL, "Undo",      NULL }
    };

/* Edit Menu Items */
struct MenuItem EditItem[] = {
        { /* "Cut" (key-equivalent: 'X') */
            &EditItem[1], 0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&EditText[0], NULL, 'X', NULL, MENUNULL },
        { /* "Copy" (key-equivalent: 'C') */
            &EditItem[2], 0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&EditText[1], NULL, 'C', NULL, MENUNULL },
        { /* "Paste" (key-equivalent: 'V') */
            &EditItem[3], 0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&EditText[2], NULL, 'V', NULL, MENUNULL },
        { /* "Erase" (disabled) */
            &EditItem[4], 0, 0, 0, 0, ITEMTEXT|HIGHCOMP, 0,
            (APTR)&EditText[3], NULL, NULL, NULL, MENUNULL },
        { /* "Undo" MenuItem (key-equivalent: 'Z') */
            NULL, 0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&EditText[4], NULL, 'Z', NULL, MENUNULL }
    };

/* IntuiText for the Print Sub-Items */
struct IntuiText PrtText[] = {
        {0,1, JAM2,2,1, NULL, "NLQ",   NULL },
        {0,1, JAM2,2,1, NULL, "Draft", NULL }
    };

/* Print Sub-Items */
struct MenuItem PrtItem[] = {
        { /* "NLQ" */
            &PrtItem[1], 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&PrtText[0], NULL, NULL, NULL, MENUNULL },
        { /* "Draft" */
            NULL, 0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&PrtText[1], NULL, NULL, NULL, MENUNULL }
    };

/* Project Menu Item IntuiText */
struct IntuiText ProjText[] = {
        {0,1, JAM2,2,1, NULL, "New",         NULL },
        {0,1, JAM2,2,1, NULL, "Open...",     NULL },
        {0,1, JAM2,2,1, NULL, "Save",        NULL },
        {0,1, JAM2,2,1, NULL, "Save As...",  NULL },
        {0,1, JAM2,2,1, NULL, "Print     \273", NULL },
        {0,1, JAM2,2,1, NULL, "About...",    NULL },
        {0,1, JAM2,2,1, NULL, "Quit",        NULL }
    };


/* Project Menu Items */
struct MenuItem ProjItem[] = {
        { /* "New" (key-equivalent: 'N' */
            &ProjItem[1],0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[0], NULL, 'N', NULL, MENUNULL },
        { /* "Open..." (key-equivalent: 'O') */
            &ProjItem[2],0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[1], NULL, 'O', NULL, MENUNULL },
        { /* "Save" (key-equivalent: 'S') */
            &ProjItem[3],0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[2], NULL, 'S', NULL, MENUNULL },
        { /* "Save As..." (key-equivalent: 'A') */
            &ProjItem[4],0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[3], NULL, 'A', NULL, MENUNULL },
        { /* "Print" (has sub-menu) */
            &ProjItem[5],0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[4], NULL, NULL, &PrtItem[0], MENUNULL },
        { /* "About..." */
            &ProjItem[6],0, 0, 0, 0, ITEMTEXT|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[5], NULL, NULL, NULL, MENUNULL },
        { /* "Quit" (key-equivalent: 'Q' */
            NULL, 0, 0, 0, 0, ITEMTEXT|COMMSEQ|ITEMENABLED|HIGHCOMP, 0,
            (APTR)&ProjText[6], NULL, 'Q', NULL, MENUNULL }
    };

/* Menu Titles */
struct Menu Menus[] = {
        {&Menus[1],  0, 0, 63, 0, MENUENABLED, "Project",    &ProjItem[0]},
        {&Menus[2], 70, 0, 39, 0, MENUENABLED, "Edit",       &EditItem[0]},
        {NULL,     120, 0, 88, 0, MENUENABLED, "Settings",   &SettItem[0]},
    };

/* A pointer to the first menu for easy reference */
struct Menu *FirstMenu = &Menus[0];

/* Window Text for Explanation of Program */
struct IntuiText WinText[] = {
        {0, 0, JAM2, 0, 0, NULL, "How to do a Menu", NULL},
        {0, 0, JAM2, 0, 0, NULL, "(with Style)", &WinText[0]}
    };

/* Globals */
struct Library  *IntuitionBase = NULL;
struct Library  *GfxBase = NULL;

/* open all of the required libraries.  Note that we require
** Intuition V37, as the routine uses OpenWindowTags().
*/
int main(int argc, char **argv)
{
LONG returnValue;

/* This gets set to RETURN_OK if everything goes well. */
returnValue = RETURN_FAIL;

/* Open the Intuition Library */
IntuitionBase = OpenLibrary("intuition.library", 37);
if (IntuitionBase)
    {
    /* Open the Graphics Library */
    GfxBase = (struct Library *)OpenLibrary("graphics.library", 33);
    if (GfxBase)
        {
        returnValue = doWindow();

        CloseLibrary(GfxBase);
        }
    CloseLibrary(IntuitionBase);
    }
return(returnValue);
}

/* Open a window with some properly positioned text.  Layout and set
** the menus, then process any events received.  Cleanup when done.
*/

LONG doWindow()
{
struct Window *window;
struct Screen *screen;
struct DrawInfo *drawinfo;
ULONG  win_width, alt_width, win_height;
LONG   returnValue = RETURN_FAIL;
BOOL   done = FALSE;

if (screen = LockPubScreen(NULL))
    {
    if (drawinfo = GetScreenDrawInfo(screen))
        {
        /* get the colors for the window text */
        WinText[0].FrontPen = WinText[1].FrontPen = drawinfo->dri_Pens[TEXTPEN];
        WinText[0].BackPen  = WinText[1].BackPen  = drawinfo->dri_Pens[BACKGROUNDPEN];

        /* use the screen's font for the text */
        WinText[0].ITextFont = WinText[1].ITextFont = screen->Font;

        /* calculate window size */
        win_width  = 100 + IntuiTextLength(&(WinText[0]));
        alt_width  = 100 + IntuiTextLength(&(WinText[1]));
        if (win_width < alt_width)
            win_width  = alt_width;
        win_height = 1 + screen->WBorTop + screen->WBorBottom +
                     (screen->Font->ta_YSize * 5);

        /* calculate the correct positions for the text in the window */
        WinText[0].LeftEdge = (win_width - IntuiTextLength(&(WinText[0]))) >> 1;
        WinText[0].TopEdge  = 1 + screen->WBorTop + (2 * screen->Font->ta_YSize);
        WinText[1].LeftEdge = (win_width - IntuiTextLength(&(WinText[1]))) >> 1;
        WinText[1].TopEdge  = WinText[0].TopEdge + screen->Font->ta_YSize;

        /* Open the window */
        window = OpenWindowTags(NULL,
            WA_PubScreen, screen,
            WA_IDCMP,     IDCMP_MENUPICK | IDCMP_CLOSEWINDOW | IDCMP_MENUHELP,
            WA_Flags,     WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET |
                              WFLG_ACTIVATE | WFLG_NOCAREREFRESH,
            WA_Left,      10,             WA_Top,       screen->BarHeight + 1,
            WA_Width,     win_width,      WA_Height,    win_height,
            WA_Title,     "Menu Example", WA_MenuHelp,  TRUE,
            TAG_END);



        if (window)
            {
            returnValue = RETURN_OK;  /* program initialized ok */

            /* Give a brief explanation of the program */
            PrintIText(window->RPort,&WinText[1],0,0);

            /* Adjust the menu to conform to the font (TextAttr) */
            adjustMenus(FirstMenu, window->WScreen->Font);

            /* attach the menu to the window */
            SetMenuStrip(window, FirstMenu);

            printf("Menu Example running. Right-click to access menus.\n");
            printf("Select Quit from the Project menu to exit.\n");

            /* And wait to hear from your signals */
            while (!done)
                {
                Wait(1L << window->UserPort->mp_SigBit);
                done = handleIDCMP(window);
                };

            /* clean up everything used here */
            ClearMenuStrip(window);
            CloseWindow(window);
            }
        FreeScreenDrawInfo(screen,drawinfo);
        }
    UnlockPubScreen(NULL,screen);
    }
return(returnValue);
}

/* print out what menu was selected.  Properly handle the IDCMP_MENUHELP
** events.  Set done to TRUE if quit is selected.
*/
BOOL processMenus(USHORT selection, BOOL done)
{
USHORT flags;
USHORT menuNum, itemNum, subNum;

menuNum = MENUNUM(selection);
itemNum = ITEMNUM(selection);
subNum  = SUBNUM(selection);

/* when processing IDCMP_MENUHELP, you are not guaranteed
** to get a menu item.
*/
if (itemNum != NOITEM)
    {
    flags = ((struct MenuItem *)ItemAddress(FirstMenu,(LONG)selection))->Flags;
    if (flags & CHECKED)
        printf("(Checked) ");
    }

switch (menuNum)
    {
    case 0:      /* Project Menu */
        switch (itemNum)
            {
            case NOITEM: printf("Project Menu\n");        break;
            case 0:      printf("New\n");                 break;
            case 1:      printf("Open\n");                break;
            case 2:      printf("Save\n");                break;
            case 3:      printf("Save As\n");             break;
            case 4:      printf("Print ");
                switch (subNum)
                    {
                    case NOSUB: printf("Item\n");  break;
                    case 0:     printf("NLQ\n");   break;
                    case 1:     printf("Draft\n"); break;
                    }
                break;
            case 5:      printf("About\n");               break;
            case 6:      printf("Quit\n");   done = TRUE; break;
            }
        break;
    case 1:      /* Edit Menu */
        switch (itemNum) {
            case NOITEM: printf("Edit Menu\n"); break;
            case 0:      printf("Cut\n");       break;
            case 1:      printf("Copy\n");      break;
            case 2:      printf("Paste\n");     break;
            case 3:      printf("Erase\n");     break;
            case 4:      printf("Undo\n");      break;
            }
        break;
    case 2:      /* Settings Menu */
        switch (itemNum) {
            case NOITEM: printf("Settings Menu\n"); break;
            case 0:      printf("Sound\n");            break;
            case 1:      printf("Auto Save\n");        break;
            case 2:      printf("Have Your Cake\n");   break;
            case 3:      printf("Eat It Too\n");       break;
            }
        break;
    case NOMENU: /* No menu selected, can happen with IDCMP_MENUHELP */
        printf("no menu\n");
        break;
    }
return(done);
}

/* Handle the IDCMP messages.  Set done to TRUE if quit or closewindow is selected. */
BOOL handleIDCMP(struct Window *win)
{
BOOL done;
USHORT code, selection;
struct IntuiMessage *message = NULL;
ULONG  class;

done = FALSE;

/* Examine pending messages */
while (message = (struct IntuiMessage *)GetMsg(win->UserPort)) {
    class = message->Class;
    code = message->Code;

    /* When we're through with a message, reply */
    ReplyMsg((struct Message *)message);

    /* See what events occurred */
    switch (class) {
        case IDCMP_CLOSEWINDOW:
            done = TRUE;
            break;
        case IDCMP_MENUHELP:
            printf("IDCMP_MENUHELP: Help on ");
            processMenus(code,done);
            break;
        case IDCMP_MENUPICK:
            for ( selection = code; selection != MENUNULL;
                  selection = (ItemAddress(FirstMenu,(LONG)selection))->NextSelect)
                {
                printf("IDCMP_MENUPICK: Selected ");
                done = processMenus(selection,done);
                }
            break;
        }
    }
return(done);
}
/* Steps thru each item to determine the maximum width of the strip */
USHORT MaxLength(struct RastPort *textRPort, struct MenuItem *first_item, USHORT char_size)
{
USHORT maxLength;
USHORT total_textlen;
struct MenuItem  *cur_item;
struct IntuiText *itext;
USHORT extra_width;
USHORT maxCommCharWidth;
USHORT commCharWidth;

extra_width = char_size;  /* used as padding for each item. */

/* Find the maximum length of a command character, if any.
** If found, it will be added to the extra_width field.
*/
maxCommCharWidth = 0;
for (cur_item = first_item; cur_item != NULL; cur_item = cur_item->NextItem)
    {
    if (cur_item->Flags & COMMSEQ)
        {
        commCharWidth = TextLength(textRPort,&(cur_item->Command),1);
        if (commCharWidth > maxCommCharWidth)
            maxCommCharWidth = commCharWidth;
        }
    }

/* if we found a command sequence, add it to the extra required space.  Add
** space for the Amiga key glyph plus space for the command character.  Note
** this only works for HIRES screens, for LORES, use LOWCOMMWIDTH.
*/
if (maxCommCharWidth > 0)
    extra_width += maxCommCharWidth + COMMWIDTH;

/* Find the maximum length of the menu items, given the extra width calculated above. */
maxLength = 0;
for (cur_item = first_item; cur_item != NULL; cur_item = cur_item->NextItem)
    {
    itext = (struct IntuiText *)cur_item->ItemFill;
    total_textlen = extra_width + itext->LeftEdge +
          TextLength(textRPort, itext->IText, strlen(itext->IText));

    /* returns the greater of the two */
    if (total_textlen > maxLength)
        maxLength = total_textlen;
    }
return(maxLength);
}

/* Set all IntuiText in a chain (they are linked through the NextText ** field) to the same font. */
VOID setITextAttr(struct IntuiText *first_IText, struct TextAttr *textAttr)
{
struct IntuiText *cur_IText;

for (cur_IText = first_IText; cur_IText != NULL; cur_IText = cur_IText->NextText)
    cur_IText->ITextFont = textAttr;
}


/* Adjust the MenuItems and SubItems */
VOID adjustItems(struct RastPort *textRPort, struct MenuItem *first_item,
                 struct TextAttr *textAttr, USHORT char_size, USHORT height,
                 USHORT level, USHORT left_edge)
{
register USHORT item_num;
struct   MenuItem *cur_item;
USHORT   strip_width, subitem_edge;

if (first_item == NULL)
    return;

/* The width of this strip is the maximum length of its members. */
strip_width = MaxLength(textRPort, first_item, char_size);

/* Position the items. */
for (cur_item = first_item, item_num = 0; cur_item != NULL; cur_item = cur_item->NextItem, item_num++)
    {
    cur_item->TopEdge  = (item_num * height) - level;
    cur_item->LeftEdge = left_edge;
    cur_item->Width    = strip_width;
    cur_item->Height   = height;

    /* place the sub_item 3/4 of the way over on the item. */
    subitem_edge = strip_width - (strip_width >> 2);

    setITextAttr((struct IntuiText *)cur_item->ItemFill, textAttr);
    adjustItems(textRPort,cur_item->SubItem,textAttr,char_size,height,1,subitem_edge);
    }
}


/* The following routines adjust an entire menu system to conform to the specified fonts' width and
** height.  Allows for Proportional Fonts. This is necessary for a clean look regardless of what the
** users preference in Fonts may be.  Using these routines, you don't need to specify TopEdge,
** LeftEdge, Width or Height in the MenuItem structures.
**
** NOTE that this routine does not work for menus with images, but assumes that all menu items are
** rendered with IntuiText.
**
** This set of routines does NOT check/correct if the menu runs off
** the screen due to large fonts, too many items, lo-res screen.
*/
BOOL adjustMenus(struct Menu *first_menu, struct TextAttr *textAttr)
{
struct RastPort   textrp = {0};             /* Temporary RastPort */
struct Menu      *cur_menu;
struct TextFont  *font;                     /* Font to use */
USHORT            start, char_size, height;
BOOL              returnValue = FALSE;

/* open the font */
if (font = OpenFont(textAttr))
    {
    SetFont(&textrp, font);       /* Put font into temporary RastPort */

    char_size = TextLength(&textrp, "n", 1);    /* Get the Width of the Font */

    /* To prevent crowding of the Amiga key when using COMMSEQ, don't allow the items to be less
    ** than 8 pixels high.  Also, add an extra pixel for inter-line spacing.
    */
    if (font->tf_YSize > 8)
        height = 1 + font->tf_YSize;
    else
        height = 1 + 8;

    start = 2;      /* Set Starting Pixel */

    /* Step thru the menu structure and adjust it */
    for (cur_menu = first_menu; cur_menu != NULL; cur_menu = cur_menu->NextMenu)
        {
        cur_menu->LeftEdge = start;
        cur_menu->Width = char_size +
            TextLength(&textrp, cur_menu->MenuName, strlen(cur_menu->MenuName));
        adjustItems(&textrp, cur_menu->FirstItem, textAttr, char_size, height, 0, 0);
        start += cur_menu->Width + char_size + char_size;
        }
    CloseFont(font);              /* Close the Font */
    returnValue = TRUE;
    }
return(returnValue);
}
