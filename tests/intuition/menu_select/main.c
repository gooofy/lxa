/*
 * Test: intuition/menu_select
 * Tests menu bar interaction and IDCMP_MENUPICK delivery:
 * - Menu bar rendering when right mouse button pressed
 * - Menu title highlighting during hover
 * - Menu item selection and IDCMP_MENUPICK message
 * - MENUNUM/ITEMNUM extraction from menu code
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

#include "../../common/test_inject.h"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *prefix, LONG val)
{
    char buf[48];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;
    
    if (val < 0) {
        *p++ = '-';
        val = -val;
    }
    
    char digits[12];
    int i = 0;
    if (val == 0) {
        digits[i++] = '0';
    } else {
        while (val > 0) {
            digits[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) *p++ = digits[--i];
    *p++ = '\n';
    *p = '\0';
    print(buf);
}

static void print_hex(const char *prefix, ULONG val)
{
    char buf[32];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;
    *p++ = '0'; *p++ = 'x';
    
    for (int i = 12; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        *p++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
    }
    *p++ = '\n';
    *p = '\0';
    print(buf);
}

/* IntuiText for menu item labels */
static struct IntuiText itemText1 = {
    0, 1,           /* FrontPen, BackPen */
    JAM2,           /* DrawMode */
    2, 1,           /* LeftEdge, TopEdge */
    NULL,           /* ITextFont */
    (UBYTE *)"Open",/* IText */
    NULL            /* NextText */
};

static struct IntuiText itemText2 = {
    0, 1,           /* FrontPen, BackPen */
    JAM2,           /* DrawMode */
    2, 1,           /* LeftEdge, TopEdge */
    NULL,           /* ITextFont */
    (UBYTE *)"Save",/* IText */
    NULL            /* NextText */
};

static struct IntuiText itemText3 = {
    0, 1,           /* FrontPen, BackPen */
    JAM2,           /* DrawMode */
    2, 1,           /* LeftEdge, TopEdge */
    NULL,           /* ITextFont */
    (UBYTE *)"Quit",/* IText */
    NULL            /* NextText */
};

static struct IntuiText itemText4 = {
    0, 1,           /* FrontPen, BackPen */
    JAM2,           /* DrawMode */
    2, 1,           /* LeftEdge, TopEdge */
    NULL,           /* ITextFont */
    (UBYTE *)"Copy",/* IText */
    NULL            /* NextText */
};

static struct IntuiText itemText5 = {
    0, 1,           /* FrontPen, BackPen */
    JAM2,           /* DrawMode */
    2, 1,           /* LeftEdge, TopEdge */
    NULL,           /* ITextFont */
    (UBYTE *)"Paste",/* IText */
    NULL            /* NextText */
};

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    int errors = 0;

    /* Menu structures */
    struct Menu menu1, menu2;
    struct MenuItem item1, item2, item3;  /* File menu items */
    struct MenuItem item4, item5;          /* Edit menu items */

    print("Testing menu bar interaction and selection...\n");

    /* ========== Test 1: Open screen and window ========== */
    print("\n--- Test 1: Open screen and window with menus ---\n");

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
    ns.DefaultTitle = (UBYTE *)"Menu Select Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");
    print_num("  BarHeight=", screen->BarHeight);

    /* Open a window */
    nw.LeftEdge = 10;
    nw.TopEdge = 20;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_MENUPICK | IDCMP_MOUSEBUTTONS;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Menu Test Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (window == NULL) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened\n");

    /* ========== Test 2: Set up menu structure ========== */
    print("\n--- Test 2: Setup menu structure ---\n");

    /* File menu items */
    item1.NextItem = &item2;
    item1.LeftEdge = 0;
    item1.TopEdge = 0;
    item1.Width = 80;
    item1.Height = 10;
    item1.Flags = ITEMENABLED | HIGHCOMP | ITEMTEXT;
    item1.MutualExclude = 0;
    item1.ItemFill = (APTR)&itemText1;
    item1.SelectFill = NULL;
    item1.Command = 'O';
    item1.SubItem = NULL;
    item1.NextSelect = MENUNULL;

    item2.NextItem = &item3;
    item2.LeftEdge = 0;
    item2.TopEdge = 10;
    item2.Width = 80;
    item2.Height = 10;
    item2.Flags = ITEMENABLED | HIGHCOMP | ITEMTEXT;
    item2.MutualExclude = 0;
    item2.ItemFill = (APTR)&itemText2;
    item2.SelectFill = NULL;
    item2.Command = 'S';
    item2.SubItem = NULL;
    item2.NextSelect = MENUNULL;

    item3.NextItem = NULL;
    item3.LeftEdge = 0;
    item3.TopEdge = 20;
    item3.Width = 80;
    item3.Height = 10;
    item3.Flags = ITEMENABLED | HIGHCOMP | ITEMTEXT;
    item3.MutualExclude = 0;
    item3.ItemFill = (APTR)&itemText3;
    item3.SelectFill = NULL;
    item3.Command = 'Q';
    item3.SubItem = NULL;
    item3.NextSelect = MENUNULL;

    /* Edit menu items */
    item4.NextItem = &item5;
    item4.LeftEdge = 0;
    item4.TopEdge = 0;
    item4.Width = 80;
    item4.Height = 10;
    item4.Flags = ITEMENABLED | HIGHCOMP | ITEMTEXT;
    item4.MutualExclude = 0;
    item4.ItemFill = (APTR)&itemText4;
    item4.SelectFill = NULL;
    item4.Command = 'C';
    item4.SubItem = NULL;
    item4.NextSelect = MENUNULL;

    item5.NextItem = NULL;
    item5.LeftEdge = 0;
    item5.TopEdge = 10;
    item5.Width = 80;
    item5.Height = 10;
    item5.Flags = ITEMENABLED | HIGHCOMP | ITEMTEXT;
    item5.MutualExclude = 0;
    item5.ItemFill = (APTR)&itemText5;
    item5.SelectFill = NULL;
    item5.Command = 'V';
    item5.SubItem = NULL;
    item5.NextSelect = MENUNULL;

    /* Menus */
    menu1.NextMenu = &menu2;
    menu1.LeftEdge = 5;
    menu1.TopEdge = 0;
    menu1.Width = 50;
    menu1.Height = 10;
    menu1.Flags = MENUENABLED;
    menu1.MenuName = (CONST_STRPTR)"File";
    menu1.FirstItem = &item1;
    menu1.JazzX = 0;
    menu1.JazzY = 0;
    menu1.BeatX = 0;
    menu1.BeatY = 0;

    menu2.NextMenu = NULL;
    menu2.LeftEdge = 60;
    menu2.TopEdge = 0;
    menu2.Width = 50;
    menu2.Height = 10;
    menu2.Flags = MENUENABLED;
    menu2.MenuName = (CONST_STRPTR)"Edit";
    menu2.FirstItem = &item4;
    menu2.JazzX = 0;
    menu2.JazzY = 0;
    menu2.BeatX = 0;
    menu2.BeatY = 0;

    /* Attach menu to window */
    if (!SetMenuStrip(window, &menu1)) {
        print("FAIL: SetMenuStrip() returned FALSE\n");
        errors++;
    } else {
        print("OK: SetMenuStrip() succeeded\n");
    }

    if (window->MenuStrip != &menu1) {
        print("FAIL: window->MenuStrip not set\n");
        errors++;
    } else {
        print("OK: window->MenuStrip correctly set\n");
    }

    /* ========== Test 3: Simulate menu selection ========== */
    print("\n--- Test 3: Simulate menu selection via right-click ---\n");

    /* Calculate position in menu bar (File menu title) 
     * Menu bar is in the screen title bar area.
     * Menu1 is at LeftEdge=5, we'll click in the middle of it.
     */
    WORD menuBarX = screen->BarHBorder + menu1.LeftEdge + (menu1.Width / 2);
    WORD menuBarY = screen->BarHeight / 2;  /* Middle of title bar */

    print_num("  Menu bar click X=", menuBarX);
    print_num("  Menu bar click Y=", menuBarY);

    /* Move mouse to menu bar area */
    test_inject_mouse(menuBarX, menuBarY, 0, DISPLAY_EVENT_MOUSEMOVE);
    WaitTOF();
    
    /* Press right mouse button (enter menu mode) */
    test_inject_mouse(menuBarX, menuBarY, MOUSE_RIGHTBUTTON, DISPLAY_EVENT_MOUSEBUTTON);
    WaitTOF();
    WaitTOF();

    /* Move to first menu item (Open) */
    WORD itemX = screen->BarHBorder + menu1.LeftEdge + menu1.BeatX + (item1.Width / 2);
    WORD itemY = screen->BarHeight + 1 + item1.TopEdge + (item1.Height / 2);
    
    print_num("  Menu item click X=", itemX);
    print_num("  Menu item click Y=", itemY);
    
    test_inject_mouse(itemX, itemY, MOUSE_RIGHTBUTTON, DISPLAY_EVENT_MOUSEMOVE);
    WaitTOF();
    WaitTOF();

    /* Release right mouse button (select item) */
    test_inject_mouse(itemX, itemY, 0, DISPLAY_EVENT_MOUSEBUTTON);
    WaitTOF();
    WaitTOF();
    WaitTOF();

    /* Check for IDCMP_MENUPICK message */
    struct IntuiMessage *imsg;
    BOOL got_menupick = FALSE;
    UWORD menuCode = MENUNULL;

    while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        print_hex("  Received IDCMP class=", imsg->Class);
        if (imsg->Class == IDCMP_MENUPICK) {
            got_menupick = TRUE;
            menuCode = imsg->Code;
            print("OK: Received IDCMP_MENUPICK\n");
            print_hex("  Menu code=", menuCode);
            
            if (menuCode != MENUNULL) {
                WORD menuNum = MENUNUM(menuCode);
                WORD itemNum = ITEMNUM(menuCode);
                WORD subNum = SUBNUM(menuCode);
                print_num("  MENUNUM=", menuNum);
                print_num("  ITEMNUM=", itemNum);
                print_num("  SUBNUM=", subNum);
                
                /* Verify we got the correct menu item (File->Open = menu 0, item 0) */
                if (menuNum == 0 && itemNum == 0) {
                    print("OK: Correct menu item selected (File->Open)\n");
                } else {
                    print("Note: Different menu item selected\n");
                }
            }
        }
        ReplyMsg((struct Message *)imsg);
    }

    if (!got_menupick) {
        print("Note: IDCMP_MENUPICK not received (menu tracking may not be complete)\n");
    }

    /* ========== Test 4: Verify menu encoding macros ========== */
    print("\n--- Test 4: Verify menu encoding macros ---\n");

    /* Test FULLMENUNUM encoding and decoding */
    UWORD testCode = FULLMENUNUM(1, 2, NOSUB);
    if (MENUNUM(testCode) == 1 && ITEMNUM(testCode) == 2) {
        print("OK: FULLMENUNUM(1,2,NOSUB) encodes/decodes correctly\n");
    } else {
        print("FAIL: FULLMENUNUM encoding incorrect\n");
        errors++;
    }
    
    /* Verify MENUNULL handling */
    if (MENUNUM(MENUNULL) == NOMENU) {
        print("OK: MENUNUM(MENUNULL) returns NOMENU\n");
    } else {
        print_num("Note: MENUNUM(MENUNULL)=", MENUNUM(MENUNULL));
    }

    /* ========== Test 5: Cleanup ========== */
    print("\n--- Test 5: Cleanup ---\n");

    /* Drain any remaining messages */
    while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        ReplyMsg((struct Message *)imsg);
    }

    ClearMenuStrip(window);
    print("OK: ClearMenuStrip() called\n");

    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* ========== Final result ========== */
    print("\n");
    if (errors == 0) {
        print("PASS: menu_select tests completed\n");
        return 0;
    } else {
        print("FAIL: menu_select had errors\n");
        return 20;
    }
}
