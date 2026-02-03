/*
 * Test: intuition/screen_manipulation
 * Tests ScreenToBack, ScreenToFront, ScreenDepth, ScreenPosition functions
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
    struct NewScreen ns1, ns2;
    struct Screen *screen1, *screen2;
    
    print("Testing Screen Manipulation Functions...\n\n");
    
    /* Open first screen */
    ns1.LeftEdge = 0;
    ns1.TopEdge = 0;
    ns1.Width = 640;
    ns1.Height = 480;
    ns1.Depth = 2;
    ns1.DetailPen = 0;
    ns1.BlockPen = 1;
    ns1.ViewModes = 0;
    ns1.Type = CUSTOMSCREEN;
    ns1.Font = NULL;
    ns1.DefaultTitle = (UBYTE *)"Screen 1";
    ns1.Gadgets = NULL;
    ns1.CustomBitMap = NULL;
    
    screen1 = OpenScreen(&ns1);
    if (!screen1) {
        print("ERROR: Could not open screen1\n");
        return 1;
    }
    print("OK: Screen 1 opened\n");
    
    /* Open second screen */
    ns2.LeftEdge = 0;
    ns2.TopEdge = 0;
    ns2.Width = 320;
    ns2.Height = 200;
    ns2.Depth = 2;
    ns2.DetailPen = 0;
    ns2.BlockPen = 1;
    ns2.ViewModes = 0;
    ns2.Type = CUSTOMSCREEN;
    ns2.Font = NULL;
    ns2.DefaultTitle = (UBYTE *)"Screen 2";
    ns2.Gadgets = NULL;
    ns2.CustomBitMap = NULL;
    
    screen2 = OpenScreen(&ns2);
    if (!screen2) {
        print("ERROR: Could not open screen2\n");
        CloseScreen(screen1);
        return 1;
    }
    print("OK: Screen 2 opened\n\n");
    
    /* Test 1: ScreenToBack */
    print("Test 1: ScreenToBack(screen2)...\n");
    ScreenToBack(screen2);
    print("  OK: ScreenToBack called\n\n");
    
    /* Test 2: ScreenToFront */
    print("Test 2: ScreenToFront(screen2)...\n");
    ScreenToFront(screen2);
    print("  OK: ScreenToFront called\n\n");
    
    /* Test 3: ScreenDepth with SDEPTH_TOBACK */
    print("Test 3: ScreenDepth(screen1, SDEPTH_TOBACK, NULL)...\n");
    ScreenDepth(screen1, SDEPTH_TOBACK, NULL);
    print("  OK: ScreenDepth TOBACK called\n\n");
    
    /* Test 4: ScreenDepth with SDEPTH_TOFRONT */
    print("Test 4: ScreenDepth(screen1, SDEPTH_TOFRONT, NULL)...\n");
    ScreenDepth(screen1, SDEPTH_TOFRONT, NULL);
    print("  OK: ScreenDepth TOFRONT called\n\n");
    
    /* Test 5: ScreenPosition */
    print("Test 5: ScreenPosition(screen1, SPOS_RELATIVE, 10, 20, 0, 0)...\n");
    ScreenPosition(screen1, SPOS_RELATIVE, 10, 20, 0, 0);
    print("  OK: ScreenPosition called\n\n");
    
    /* Cleanup */
    CloseScreen(screen2);
    print("OK: Screen 2 closed\n");
    CloseScreen(screen1);
    print("OK: Screen 1 closed\n\n");
    
    print("PASS: screen_manipulation all tests completed\n");
    return 0;
}
