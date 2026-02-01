/*
 * Test: intuition/screen_basic
 * Tests OpenScreen() and CloseScreen() functions
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
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
    struct Screen *screen;
    int errors = 0;

    print("Testing OpenScreen()/CloseScreen()...\n");

    /* Set up NewScreen structure */
    ns.LeftEdge = 0;
    ns.TopEdge = 0;
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 2;  /* 4 colors */
    ns.DetailPen = 0;
    ns.BlockPen = 1;
    ns.ViewModes = 0;
    ns.Type = CUSTOMSCREEN;
    ns.Font = NULL;
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    /* Open the screen */
    screen = OpenScreen(&ns);

    if (screen == NULL) {
        print("FAIL: OpenScreen() returned NULL\n");
        return 20;
    }
    print("OK: OpenScreen() succeeded\n");

    /* Verify screen dimensions */
    if (screen->Width != 320) {
        print("FAIL: screen->Width != 320\n");
        errors++;
    } else {
        print("OK: screen->Width = 320\n");
    }

    if (screen->Height != 200) {
        print("FAIL: screen->Height != 200\n");
        errors++;
    } else {
        print("OK: screen->Height = 200\n");
    }

    /* Verify BitMap initialization */
    if (screen->BitMap.Depth != 2) {
        print("FAIL: BitMap.Depth != 2\n");
        errors++;
    } else {
        print("OK: BitMap.Depth = 2\n");
    }

    if (screen->BitMap.BytesPerRow != 40) {  /* 320/8 = 40 */
        print("FAIL: BitMap.BytesPerRow != 40\n");
        errors++;
    } else {
        print("OK: BitMap.BytesPerRow = 40\n");
    }

    if (screen->BitMap.Rows != 200) {
        print("FAIL: BitMap.Rows != 200\n");
        errors++;
    } else {
        print("OK: BitMap.Rows = 200\n");
    }

    /* Verify bitplanes are allocated */
    if (screen->BitMap.Planes[0] == NULL) {
        print("FAIL: BitMap.Planes[0] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[0] allocated\n");
    }

    if (screen->BitMap.Planes[1] == NULL) {
        print("FAIL: BitMap.Planes[1] is NULL\n");
        errors++;
    } else {
        print("OK: BitMap.Planes[1] allocated\n");
    }

    /* Verify RastPort is connected to BitMap */
    if (screen->RastPort.BitMap != &screen->BitMap) {
        print("FAIL: RastPort.BitMap not connected\n");
        errors++;
    } else {
        print("OK: RastPort.BitMap connected\n");
    }

    /* Verify ViewPort dimensions */
    if (screen->ViewPort.DWidth != 320) {
        print("FAIL: ViewPort.DWidth != 320\n");
        errors++;
    } else {
        print("OK: ViewPort.DWidth = 320\n");
    }

    if (screen->ViewPort.DHeight != 200) {
        print("FAIL: ViewPort.DHeight != 200\n");
        errors++;
    } else {
        print("OK: ViewPort.DHeight = 200\n");
    }

    /* Test drawing to the screen */
    SetAPen(&screen->RastPort, 1);
    RectFill(&screen->RastPort, 10, 10, 50, 50);
    
    /* Verify pixel was set */
    if (ReadPixel(&screen->RastPort, 30, 30) != 1) {
        print("FAIL: Drawing to screen failed\n");
        errors++;
    } else {
        print("OK: Drawing to screen works\n");
    }

    /* Close the screen */
    CloseScreen(screen);
    print("OK: CloseScreen() completed\n");

    /* Final result */
    if (errors == 0) {
        print("PASS: screen_basic all tests passed\n");
        return 0;
    } else {
        print("FAIL: screen_basic had errors\n");
        return 20;
    }
}
