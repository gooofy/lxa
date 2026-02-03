/*
 * Test: intuition/gadget_refresh
 * Tests RefreshGList, OnGadget, OffGadget functions
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
    struct Gadget gadget1, gadget2;
    
    print("Testing Gadget Refresh Functions...\n\n");
    
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
    
    /* Initialize gadgets */
    gadget1.NextGadget = &gadget2;
    gadget1.LeftEdge = 10;
    gadget1.TopEdge = 10;
    gadget1.Width = 100;
    gadget1.Height = 20;
    gadget1.Flags = GFLG_GADGHCOMP;
    gadget1.Activation = 0;
    gadget1.GadgetType = GTYP_BOOLGADGET;
    gadget1.GadgetRender = NULL;
    gadget1.SelectRender = NULL;
    gadget1.GadgetText = NULL;
    gadget1.MutualExclude = 0;
    gadget1.SpecialInfo = NULL;
    gadget1.GadgetID = 1;
    gadget1.UserData = NULL;
    
    gadget2.NextGadget = NULL;
    gadget2.LeftEdge = 10;
    gadget2.TopEdge = 40;
    gadget2.Width = 100;
    gadget2.Height = 20;
    gadget2.Flags = GFLG_GADGHCOMP;
    gadget2.Activation = 0;
    gadget2.GadgetType = GTYP_BOOLGADGET;
    gadget2.GadgetRender = NULL;
    gadget2.SelectRender = NULL;
    gadget2.GadgetText = NULL;
    gadget2.MutualExclude = 0;
    gadget2.SpecialInfo = NULL;
    gadget2.GadgetID = 2;
    gadget2.UserData = NULL;
    
    /* Open a window with gadgets */
    nw.LeftEdge = 100;
    nw.TopEdge = 50;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = &gadget1;
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
    print("OK: Window opened with gadgets\n\n");
    
    /* Test 1: RefreshGList - refresh all gadgets */
    print("Test 1: RefreshGList(gadget1, window, NULL, -1)...\n");
    RefreshGList(&gadget1, window, NULL, -1);
    print("  OK: RefreshGList called for all gadgets\n\n");
    
    /* Test 2: RefreshGList - refresh one gadget */
    print("Test 2: RefreshGList(gadget2, window, NULL, 1)...\n");
    RefreshGList(&gadget2, window, NULL, 1);
    print("  OK: RefreshGList called for one gadget\n\n");
    
    /* Test 3: OffGadget */
    print("Test 3: OffGadget(gadget1, window, NULL)...\n");
    OffGadget(&gadget1, window, NULL);
    if (gadget1.Flags & GFLG_DISABLED) {
        print("  OK: Gadget disabled flag set\n");
    } else {
        print("  FAIL: Gadget disabled flag not set\n");
    }
    print("\n");
    
    /* Test 4: OnGadget */
    print("Test 4: OnGadget(gadget1, window, NULL)...\n");
    OnGadget(&gadget1, window, NULL);
    if (!(gadget1.Flags & GFLG_DISABLED)) {
        print("  OK: Gadget enabled (disabled flag cleared)\n");
    } else {
        print("  FAIL: Gadget still disabled\n");
    }
    print("\n");
    
    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    print("PASS: gadget_refresh all tests completed\n");
    return 0;
}
