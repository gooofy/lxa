/* SimpleMenu.c - Adapted from RKM sample
 * Demonstrates creating and using menus with a window
 * Modified for automated testing (no interactive event loop)
 */

#define INTUI_V36_NAMES_ONLY

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>

#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>

#include <stdio.h>
#include <string.h>

/* Menu dimensions based on Topaz 8 font */
#define MENWIDTH  (56+8)  /* Longest menu item name * font width + trim */
#define MENHEIGHT (10)    /* Font height + 2 pixels */

struct Library *GfxBase = NULL;
struct Library *IntuitionBase = NULL;

/* Font for menu items */
struct TextAttr Topaz80 =
{
    "topaz.font", 8, 0, 0
};

/* IntuiText for each menu item */
struct IntuiText menuIText[] =
{
    { 0, 1, JAM2, 0, 1, &Topaz80, "Open...",  NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Save",     NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Print \273",  NULL },  /* Right arrow for submenu */
    { 0, 1, JAM2, 0, 1, &Topaz80, "Draft",    NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "NLQ",      NULL },
    { 0, 1, JAM2, 0, 1, &Topaz80, "Quit",     NULL }
};

/* Submenu items for Print */
struct MenuItem submenu1[] =
{
    { /* Draft  */
    &submenu1[1], MENWIDTH-2,  -2,             MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[3], NULL, NULL, NULL, NULL
    },
    { /* NLQ    */
    NULL,         MENWIDTH-2, MENHEIGHT-2, MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[4], NULL, NULL, NULL, NULL
    }
};

/* Main menu items */
struct MenuItem menu1[] =
{
    { /* Open... */
    &menu1[1], 0, 0,            MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[0], NULL, NULL, NULL, NULL
    },
    { /* Save    */
    &menu1[2], 0,  MENHEIGHT ,  MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[1], NULL, NULL, NULL, NULL
    },
    { /* Print   */
    &menu1[3], 0, 2*MENHEIGHT , MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[2], NULL, NULL, &submenu1[0] , NULL
    },
    { /* Quit    */
    NULL, 0, 3*MENHEIGHT , MENWIDTH, MENHEIGHT,
    ITEMTEXT | MENUTOGGLE | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menuIText[5], NULL, NULL, NULL, NULL
    },
};

/* Number of menus */
#define NUM_MENUS 1

STRPTR menutitle[NUM_MENUS] =  { "Project" };

struct Menu menustrip[NUM_MENUS] =
{
    {
    NULL,                    /* Next Menu          */
    0, 0,                    /* LeftEdge, TopEdge  */
    0, MENHEIGHT,            /* Width, Height      */
    MENUENABLED,             /* Flags              */
    NULL,                    /* Title              */
    &menu1[0]                /* First item         */
    }
};

struct NewWindow mynewWindow =
{
    40, 40, 300, 100, 0, 1, 
    IDCMP_CLOSEWINDOW | IDCMP_MENUPICK,
    WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_CLOSEGADGET, 
    NULL, NULL,
    "Menu Test Window", 
    NULL, NULL, 0, 0, 0, 0, WBENCHSCREEN
};

/* Function to count menu items */
int countMenuItems(struct MenuItem *item)
{
    int count = 0;
    while (item)
    {
        count++;
        item = item->NextItem;
    }
    return count;
}

/* Function to count submenus */
int countSubItems(struct MenuItem *item)
{
    int count = 0;
    while (item)
    {
        if (item->SubItem)
        {
            struct MenuItem *sub = item->SubItem;
            while (sub)
            {
                count++;
                sub = sub->NextItem;
            }
        }
        item = item->NextItem;
    }
    return count;
}

/* Function to verify ItemAddress works correctly */
void testItemAddress(struct Menu *menuStrip)
{
    UWORD menuNumber;
    struct MenuItem *item;
    struct IntuiText *itext;
    
    printf("\nSimpleMenu: Testing ItemAddress()...\n");
    
    /* Test menu 0, item 0 (Open) */
    menuNumber = SHIFTMENU(0) | SHIFTITEM(0) | SHIFTSUB(NOSUB);
    item = ItemAddress(menuStrip, menuNumber);
    if (item)
    {
        itext = (struct IntuiText *)item->ItemFill;
        printf("  Menu 0, Item 0: %s\n", itext ? itext->IText : "(null)");
    }
    else
    {
        printf("  Menu 0, Item 0: NOT FOUND (ERROR)\n");
    }
    
    /* Test menu 0, item 3 (Quit) */
    menuNumber = SHIFTMENU(0) | SHIFTITEM(3) | SHIFTSUB(NOSUB);
    item = ItemAddress(menuStrip, menuNumber);
    if (item)
    {
        itext = (struct IntuiText *)item->ItemFill;
        printf("  Menu 0, Item 3: %s\n", itext ? itext->IText : "(null)");
    }
    else
    {
        printf("  Menu 0, Item 3: NOT FOUND (ERROR)\n");
    }
    
    /* Test menu 0, item 2, sub 0 (Draft) */
    menuNumber = SHIFTMENU(0) | SHIFTITEM(2) | SHIFTSUB(0);
    item = ItemAddress(menuStrip, menuNumber);
    if (item)
    {
        itext = (struct IntuiText *)item->ItemFill;
        printf("  Menu 0, Item 2, Sub 0: %s\n", itext ? itext->IText : "(null)");
    }
    else
    {
        printf("  Menu 0, Item 2, Sub 0: NOT FOUND (ERROR)\n");
    }
    
    /* Test menu 0, item 2, sub 1 (NLQ) */
    menuNumber = SHIFTMENU(0) | SHIFTITEM(2) | SHIFTSUB(1);
    item = ItemAddress(menuStrip, menuNumber);
    if (item)
    {
        itext = (struct IntuiText *)item->ItemFill;
        printf("  Menu 0, Item 2, Sub 1: %s\n", itext ? itext->IText : "(null)");
    }
    else
    {
        printf("  Menu 0, Item 2, Sub 1: NOT FOUND (ERROR)\n");
    }
}

int main(int argc, char **argv)
{
    struct Window *win = NULL;
    UWORD left, m;
    
    printf("SimpleMenu: Demonstrating Intuition menu system\n\n");
    
    /* Open the Graphics Library */
    GfxBase = OpenLibrary("graphics.library", 33);
    if (!GfxBase)
    {
        printf("SimpleMenu: ERROR - Can't open graphics.library v33\n");
        return 20;
    }
    printf("SimpleMenu: Opened graphics.library v%d\n", GfxBase->lib_Version);
    
    /* Open the Intuition Library */
    IntuitionBase = OpenLibrary("intuition.library", 33);
    if (!IntuitionBase)
    {
        printf("SimpleMenu: ERROR - Can't open intuition.library v33\n");
        CloseLibrary(GfxBase);
        return 20;
    }
    printf("SimpleMenu: Opened intuition.library v%d\n", IntuitionBase->lib_Version);
    
    /* Open the window */
    printf("\nSimpleMenu: Opening window...\n");
    win = OpenWindow(&mynewWindow);
    if (!win)
    {
        printf("SimpleMenu: ERROR - Can't open window\n");
        CloseLibrary(IntuitionBase);
        CloseLibrary(GfxBase);
        return 20;
    }
    printf("SimpleMenu: Window opened at %d,%d size %dx%d\n",
           win->LeftEdge, win->TopEdge, win->Width, win->Height);
    
    /* Set up menu positions */
    printf("\nSimpleMenu: Setting up menu strip...\n");
    left = 2;
    for (m = 0; m < NUM_MENUS; m++)
    {
        menustrip[m].LeftEdge = left;
        menustrip[m].MenuName = menutitle[m];
        menustrip[m].Width = TextLength(&win->WScreen->RastPort,
            menutitle[m], strlen(menutitle[m])) + 8;
        left += menustrip[m].Width;
        printf("  Menu '%s': Left=%d, Width=%d\n", 
               menutitle[m], menustrip[m].LeftEdge, menustrip[m].Width);
    }
    
    /* Attach menu strip to window */
    printf("\nSimpleMenu: Attaching menu strip with SetMenuStrip()...\n");
    if (SetMenuStrip(win, menustrip))
    {
        printf("SimpleMenu: Menu strip attached successfully!\n");
        
        /* Verify menu structure */
        printf("\nSimpleMenu: Verifying menu structure...\n");
        printf("  Number of menus: %d\n", NUM_MENUS);
        printf("  Menu 0 items: %d\n", countMenuItems(menu1));
        printf("  Submenu items: %d\n", countSubItems(menu1));
        
        /* Verify window's MenuStrip pointer */
        if (win->MenuStrip == menustrip)
        {
            printf("  Window->MenuStrip: Correctly set\n");
        }
        else
        {
            printf("  Window->MenuStrip: ERROR - not set correctly!\n");
        }
        
        /* Test ItemAddress function */
        testItemAddress(menustrip);
        
        /* Clear the menu strip before closing */
        printf("\nSimpleMenu: Clearing menu strip...\n");
        ClearMenuStrip(win);
        printf("SimpleMenu: Menu strip cleared\n");
        
        /* Verify MenuStrip is cleared */
        if (win->MenuStrip == NULL)
        {
            printf("  Window->MenuStrip: Correctly cleared\n");
        }
        else
        {
            printf("  Window->MenuStrip: WARNING - not cleared!\n");
        }
    }
    else
    {
        printf("SimpleMenu: ERROR - SetMenuStrip failed!\n");
    }
    
    /* Clean up */
    printf("\nSimpleMenu: Closing window...\n");
    CloseWindow(win);
    
    printf("SimpleMenu: Closing libraries...\n");
    CloseLibrary(IntuitionBase);
    CloseLibrary(GfxBase);
    
    printf("\nSimpleMenu: Demo complete.\n");
    return 0;
}
