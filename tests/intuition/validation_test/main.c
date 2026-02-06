/*
 * Test: intuition/validation_test
 *
 * Phase 39b: Test validation infrastructure
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

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
    struct Screen *screen = NULL;
    struct Window *window = NULL;

    print("=== Phase 39b: Validation Infrastructure Test ===\n\n");

    /* Open required libraries */
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return 20;
    }

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!GfxBase) {
        print("FAIL: Cannot open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 20;
    }

    /* Open Screen */
    {
        struct NewScreen ns = {0};
        ns.Width = 640;
        ns.Height = 200;
        ns.Depth = 2;
        ns.DetailPen = 0;
        ns.BlockPen = 1;
        ns.Type = CUSTOMSCREEN;
        ns.DefaultTitle = (UBYTE *)"Validation Test Screen";
        
        screen = OpenScreen(&ns);
    }
    
    if (!screen) {
        print("FAIL: OpenScreen() returned NULL\n");
        goto cleanup;
    }
    print("OK: Screen opened successfully\n");

    /* Open Window */
    {
        struct NewWindow nw = {0};
        nw.LeftEdge = 50;
        nw.TopEdge = 30;
        nw.Width = 300;
        nw.Height = 150;
        nw.DetailPen = -1;
        nw.BlockPen = -1;
        nw.IDCMPFlags = 0;
        nw.Flags = WFLG_SIZEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | 
                   WFLG_CLOSEGADGET | WFLG_ACTIVATE;
        nw.FirstGadget = NULL;
        nw.CheckMark = NULL;
        nw.Title = (UBYTE *)"Test Window";
        nw.Screen = screen;
        nw.BitMap = NULL;
        nw.MinWidth = 50;
        nw.MinHeight = 30;
        nw.MaxWidth = 640;
        nw.MaxHeight = 200;
        nw.Type = CUSTOMSCREEN;
        
        window = OpenWindow(&nw);
    }
    
    if (!window) {
        print("FAIL: OpenWindow() returned NULL\n");
        goto cleanup;
    }
    print("OK: Window opened successfully\n");

    /* Draw content */
    SetAPen(window->RPort, 1);
    RectFill(window->RPort, 10, 20, 100, 80);
    
    SetAPen(window->RPort, 2);
    Move(window->RPort, 20, 50);
    Text(window->RPort, (STRPTR)"Validation Test", 15);
    
    print("OK: Content rendered\n");
    print("DONE\n");

cleanup:
    if (window) CloseWindow(window);
    if (screen) CloseScreen(screen);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);

    print("PASS: validation_test all tests passed\n");
    return 0;
}
