/*
 * Test: intuition/window_manipulation
 * Tests MoveWindow, SizeWindow, WindowLimits, ChangeWindowBox functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include <inline/dos.h>

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

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    do
    {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    if (neg) *(--p) = '-';
    print(p);
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    
    print("Testing Window Manipulation Functions...\n\n");
    
    /* Open a screen for the window */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 640;
    ns.Height = 480;
    ns.Depth = 2;
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;
    
    screen = OpenScreen(&ns);
    if (!screen) {
        print("ERROR: Could not open screen\n");
        return 1;
    }
    print("OK: Screen opened\n\n");
    
    /* Open a window */
    nw.LeftEdge = 100;
    nw.TopEdge = 50;
    nw.Width = 300;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_SIZEGADGET;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Test Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 100;
    nw.MinHeight = 80;
    nw.MaxWidth = 500;
    nw.MaxHeight = 400;
    nw.Type = CUSTOMSCREEN;
    
    window = OpenWindow(&nw);
    if (!window) {
        print("ERROR: Could not open window\n");
        CloseScreen(screen);
        return 1;
    }
    print("OK: Window opened at position/size: ");
    print_num(window->LeftEdge);
    print(",");
    print_num(window->TopEdge);
    print(" ");
    print_num(window->Width);
    print("x");
    print_num(window->Height);
    print("\n\n");
    
    /* Test 1: MoveWindow */
    print("Test 1: MoveWindow(50, 30)...\n");
    MoveWindow(window, 50, 30);
    print("  OK: MoveWindow called\n\n");
    
    /* Test 2: SizeWindow */
    print("Test 2: SizeWindow(50, 40)...\n");
    SizeWindow(window, 50, 40);
    print("  OK: SizeWindow called\n\n");
    
    /* Test 3: WindowLimits */
    print("Test 3: WindowLimits(150, 100, 400, 300)...\n");
    if (WindowLimits(window, 150, 100, 400, 300)) {
        print("  OK: WindowLimits returned TRUE\n");
    } else {
        print("  FAIL: WindowLimits returned FALSE\n");
    }
    print("\n");
    
    /* Test 4: ChangeWindowBox */
    print("Test 4: ChangeWindowBox(50, 40, 250, 180)...\n");
    ChangeWindowBox(window, 50, 40, 250, 180);
    print("  OK: ChangeWindowBox called\n\n");
    
    /* Test 5: ZipWindow */
    print("Test 5: ZipWindow()...\n");
    ZipWindow(window);
    print("  OK: ZipWindow called (first time)\n");
    ZipWindow(window);
    print("  OK: ZipWindow called (second time)\n\n");
    
    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    print("PASS: window_manipulation all tests completed\n");
    return 0;
}
