/*
 * Test: intuition/menu_strip
 * Tests SetMenuStrip(), ClearMenuStrip(), ResetMenuStrip(), and ItemAddress()
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Menu number encoding macros are already in intuition/intuition.h:
 * MENUNUM, ITEMNUM, SUBNUM, FULLMENUNUM, MENUNULL, NOITEM, NOSUB
 */

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Menu menu1, menu2;
    struct MenuItem item1, item2, item3;
    struct MenuItem sub1, sub2;
    struct MenuItem *result;
    int errors = 0;

    print("Testing menu functions...\n");

    /* Open a screen */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Menu Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open a window */
    nw.LeftEdge = 10;
    nw.TopEdge = 15;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Menu Test Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 50;
    nw.MinHeight = 30;
    nw.MaxWidth = 300;
    nw.MaxHeight = 200;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (window == NULL) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened\n");

    /* Verify window has no menu initially */
    if (window->MenuStrip != NULL) {
        print("FAIL: window->MenuStrip not NULL initially\n");
        errors++;
    } else {
        print("OK: window->MenuStrip is NULL initially\n");
    }

    /* Set up a simple menu structure:
     * Menu1 -> Item1 -> Sub1
     *                -> Sub2
     *       -> Item2
     * Menu2 -> Item3
     */

    /* Initialize sub-items */
    sub1.NextItem = &sub2;
    sub1.LeftEdge = 0;
    sub1.TopEdge = 0;
    sub1.Width = 80;
    sub1.Height = 10;
    sub1.Flags = 0;
    sub1.MutualExclude = 0;
    sub1.ItemFill = NULL;
    sub1.SelectFill = NULL;
    sub1.Command = 0;
    sub1.SubItem = NULL;
    sub1.NextSelect = MENUNULL;

    sub2.NextItem = NULL;
    sub2.LeftEdge = 0;
    sub2.TopEdge = 10;
    sub2.Width = 80;
    sub2.Height = 10;
    sub2.Flags = 0;
    sub2.MutualExclude = 0;
    sub2.ItemFill = NULL;
    sub2.SelectFill = NULL;
    sub2.Command = 0;
    sub2.SubItem = NULL;
    sub2.NextSelect = MENUNULL;

    /* Initialize menu items */
    item1.NextItem = &item2;
    item1.LeftEdge = 0;
    item1.TopEdge = 0;
    item1.Width = 80;
    item1.Height = 10;
    item1.Flags = 0;
    item1.MutualExclude = 0;
    item1.ItemFill = NULL;
    item1.SelectFill = NULL;
    item1.Command = 0;
    item1.SubItem = &sub1;  /* Item1 has sub-items */
    item1.NextSelect = MENUNULL;

    item2.NextItem = NULL;
    item2.LeftEdge = 0;
    item2.TopEdge = 10;
    item2.Width = 80;
    item2.Height = 10;
    item2.Flags = 0;
    item2.MutualExclude = 0;
    item2.ItemFill = NULL;
    item2.SelectFill = NULL;
    item2.Command = 0;
    item2.SubItem = NULL;
    item2.NextSelect = MENUNULL;

    item3.NextItem = NULL;
    item3.LeftEdge = 0;
    item3.TopEdge = 0;
    item3.Width = 80;
    item3.Height = 10;
    item3.Flags = 0;
    item3.MutualExclude = 0;
    item3.ItemFill = NULL;
    item3.SelectFill = NULL;
    item3.Command = 0;
    item3.SubItem = NULL;
    item3.NextSelect = MENUNULL;

    /* Initialize menus */
    menu1.NextMenu = &menu2;
    menu1.LeftEdge = 0;
    menu1.TopEdge = 0;
    menu1.Width = 60;
    menu1.Height = 10;
    menu1.Flags = MENUENABLED;
    menu1.MenuName = (CONST_STRPTR)"Menu1";
    menu1.FirstItem = &item1;

    menu2.NextMenu = NULL;
    menu2.LeftEdge = 60;
    menu2.TopEdge = 0;
    menu2.Width = 60;
    menu2.Height = 10;
    menu2.Flags = MENUENABLED;
    menu2.MenuName = (CONST_STRPTR)"Menu2";
    menu2.FirstItem = &item3;

    /* Test SetMenuStrip() */
    print("Testing SetMenuStrip()...\n");
    if (!SetMenuStrip(window, &menu1)) {
        print("FAIL: SetMenuStrip() returned FALSE\n");
        errors++;
    } else {
        print("OK: SetMenuStrip() returned TRUE\n");
    }

    if (window->MenuStrip != &menu1) {
        print("FAIL: window->MenuStrip not set correctly\n");
        errors++;
    } else {
        print("OK: window->MenuStrip set correctly\n");
    }

    /* Test ItemAddress() with MENUNULL */
    print("Testing ItemAddress() with MENUNULL...\n");
    result = ItemAddress(&menu1, MENUNULL);
    if (result != NULL) {
        print("FAIL: ItemAddress(MENUNULL) should return NULL\n");
        errors++;
    } else {
        print("OK: ItemAddress(MENUNULL) returned NULL\n");
    }

    /* Test ItemAddress() - Menu 0, Item 0 (should be item1) */
    print("Testing ItemAddress() for Menu 0 Item 0...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(0, 0, NOSUB));
    if (result != &item1) {
        print("FAIL: ItemAddress(0,0,NOSUB) wrong result\n");
        errors++;
    } else {
        print("OK: ItemAddress(0,0,NOSUB) returned item1\n");
    }

    /* Test ItemAddress() - Menu 0, Item 1 (should be item2) */
    print("Testing ItemAddress() for Menu 0 Item 1...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(0, 1, NOSUB));
    if (result != &item2) {
        print("FAIL: ItemAddress(0,1,NOSUB) wrong result\n");
        errors++;
    } else {
        print("OK: ItemAddress(0,1,NOSUB) returned item2\n");
    }

    /* Test ItemAddress() - Menu 1, Item 0 (should be item3) */
    print("Testing ItemAddress() for Menu 1 Item 0...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(1, 0, NOSUB));
    if (result != &item3) {
        print("FAIL: ItemAddress(1,0,NOSUB) wrong result\n");
        errors++;
    } else {
        print("OK: ItemAddress(1,0,NOSUB) returned item3\n");
    }

    /* Test ItemAddress() - Menu 0, Item 0, Sub 0 (should be sub1) */
    print("Testing ItemAddress() for Menu 0 Item 0 Sub 0...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(0, 0, 0));
    if (result != &sub1) {
        print("FAIL: ItemAddress(0,0,0) wrong result\n");
        errors++;
    } else {
        print("OK: ItemAddress(0,0,0) returned sub1\n");
    }

    /* Test ItemAddress() - Menu 0, Item 0, Sub 1 (should be sub2) */
    print("Testing ItemAddress() for Menu 0 Item 0 Sub 1...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(0, 0, 1));
    if (result != &sub2) {
        print("FAIL: ItemAddress(0,0,1) wrong result\n");
        errors++;
    } else {
        print("OK: ItemAddress(0,0,1) returned sub2\n");
    }

    /* Test ItemAddress() - Invalid menu number (should return NULL) */
    print("Testing ItemAddress() for invalid menu number...\n");
    result = ItemAddress(&menu1, FULLMENUNUM(5, 0, NOSUB));
    if (result != NULL) {
        print("FAIL: ItemAddress(5,0,NOSUB) should return NULL\n");
        errors++;
    } else {
        print("OK: ItemAddress(5,0,NOSUB) returned NULL\n");
    }

    /* Test ClearMenuStrip() */
    print("Testing ClearMenuStrip()...\n");
    ClearMenuStrip(window);
    if (window->MenuStrip != NULL) {
        print("FAIL: ClearMenuStrip() did not clear MenuStrip\n");
        errors++;
    } else {
        print("OK: ClearMenuStrip() cleared MenuStrip\n");
    }

    /* Test ResetMenuStrip() */
    print("Testing ResetMenuStrip()...\n");
    if (!ResetMenuStrip(window, &menu1)) {
        print("FAIL: ResetMenuStrip() returned FALSE\n");
        errors++;
    } else {
        print("OK: ResetMenuStrip() returned TRUE\n");
    }

    if (window->MenuStrip != &menu1) {
        print("FAIL: ResetMenuStrip() did not set MenuStrip\n");
        errors++;
    } else {
        print("OK: ResetMenuStrip() set MenuStrip correctly\n");
    }

    /* Clear menu before closing */
    ClearMenuStrip(window);

    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* Final result */
    if (errors == 0) {
        print("PASS: menu_strip all tests passed\n");
        return 0;
    } else {
        print("FAIL: menu_strip had errors\n");
        return 20;
    }
}
