/*
 * Test: intuition/requester_basic
 * Tests InitRequester, Request, EndRequest functions
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

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Requester requester;
    BOOL result;
    
    print("Testing Requester Functions...\n\n");
    
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
    print("OK: Screen opened\n");
    
    /* Open a window */
    nw.LeftEdge = 100;
    nw.TopEdge = 50;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE;
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
    print("OK: Window opened\n\n");
    
    /* Test 1: InitRequester */
    print("Test 1: InitRequester()...\n");
    InitRequester(&requester);
    print("  OK: InitRequester called\n");
    if (requester.LeftEdge == 0 && requester.TopEdge == 0) {
        print("  OK: Requester initialized to zero\n");
    } else {
        print("  FAIL: Requester not properly initialized\n");
    }
    print("\n");
    
    /* Test 2: Request */
    print("Test 2: Request()...\n");
    requester.LeftEdge = 50;
    requester.TopEdge = 30;
    requester.Width = 200;
    requester.Height = 100;
    requester.RelLeft = 0;
    requester.RelTop = 0;
    requester.ReqGadget = NULL;
    requester.ReqBorder = NULL;
    requester.ReqText = NULL;
    requester.Flags = 0;
    requester.BackFill = 1;
    requester.ReqLayer = NULL;
    requester.ImageBMap = NULL;
    requester.RWindow = NULL;
    requester.ReqImage = NULL;
    
    result = Request(&requester, window);
    if (result) {
        print("  OK: Request returned TRUE\n");
    } else {
        print("  FAIL: Request returned FALSE\n");
    }
    print("\n");
    
    /* Test 3: EndRequest */
    print("Test 3: EndRequest()...\n");
    EndRequest(&requester, window);
    print("  OK: EndRequest called\n\n");
    
    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    print("PASS: requester_basic all tests completed\n");
    return 0;
}
