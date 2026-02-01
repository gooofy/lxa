/*
 * Test: intuition/idcmp_input
 * Tests the IDCMP input processing infrastructure:
 * - Screen linking to IntuitionBase
 * - WaitTOF triggering input processing
 * - Window UserPort setup for IDCMP
 * 
 * Note: This test verifies the infrastructure is set up correctly.
 * Actual SDL event delivery cannot be tested in headless mode.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
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
    struct NewWindow nw;
    struct Screen *screen1, *screen2;
    struct Window *window;
    int errors = 0;

    print("Testing IDCMP input infrastructure...\n");

    /* ========== Test 1: Screen linking ========== */
    print("\n--- Test 1: Screen linking to IntuitionBase ---\n");

    /* Verify IntuitionBase starts with no screens */
    if (IntuitionBase->FirstScreen != NULL) {
        /* There may be existing screens - that's OK, just note it */
        print("Note: FirstScreen is not NULL at start\n");
    }

    /* Open first screen */
    ns1.LeftEdge = 0;
    ns1.TopEdge = 0;
    ns1.Width = 320;
    ns1.Height = 200;
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
    if (screen1 == NULL) {
        print("FAIL: Could not open screen 1\n");
        return 20;
    }
    print("OK: Screen 1 opened\n");

    /* Verify screen1 is now in FirstScreen */
    if (IntuitionBase->FirstScreen != screen1) {
        print("FAIL: Screen 1 not linked as FirstScreen\n");
        errors++;
    } else {
        print("OK: Screen 1 is FirstScreen\n");
    }

    /* Verify screen1 is ActiveScreen */
    if (IntuitionBase->ActiveScreen != screen1) {
        print("FAIL: Screen 1 not set as ActiveScreen\n");
        errors++;
    } else {
        print("OK: Screen 1 is ActiveScreen\n");
    }

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
    if (screen2 == NULL) {
        print("FAIL: Could not open screen 2\n");
        CloseScreen(screen1);
        return 20;
    }
    print("OK: Screen 2 opened\n");

    /* Verify screen2 is now FirstScreen (new screens go to front) */
    if (IntuitionBase->FirstScreen != screen2) {
        print("FAIL: Screen 2 not linked as FirstScreen\n");
        errors++;
    } else {
        print("OK: Screen 2 is FirstScreen\n");
    }

    /* Verify screen1 is still linked (as NextScreen of screen2) */
    if (screen2->NextScreen != screen1) {
        print("FAIL: Screen 1 not linked as NextScreen of Screen 2\n");
        errors++;
    } else {
        print("OK: Screen 1 is NextScreen of Screen 2\n");
    }

    /* ========== Test 2: Window IDCMP setup ========== */
    print("\n--- Test 2: Window IDCMP port setup ---\n");

    /* Open window with IDCMP flags on screen2 */
    nw.LeftEdge = 10;
    nw.TopEdge = 15;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Input Test";
    nw.Screen = screen2;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (window == NULL) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen2);
        CloseScreen(screen1);
        return 20;
    }
    print("OK: Window opened\n");

    /* Verify UserPort exists */
    if (window->UserPort == NULL) {
        print("FAIL: Window UserPort is NULL\n");
        errors++;
    } else {
        print("OK: Window UserPort created\n");
    }

    /* Verify IDCMP flags are set */
    if (!(window->IDCMPFlags & IDCMP_CLOSEWINDOW)) {
        print("FAIL: IDCMP_CLOSEWINDOW not set\n");
        errors++;
    } else {
        print("OK: IDCMP_CLOSEWINDOW flag set\n");
    }

    if (!(window->IDCMPFlags & IDCMP_MOUSEBUTTONS)) {
        print("FAIL: IDCMP_MOUSEBUTTONS not set\n");
        errors++;
    } else {
        print("OK: IDCMP_MOUSEBUTTONS flag set\n");
    }

    if (!(window->IDCMPFlags & IDCMP_RAWKEY)) {
        print("FAIL: IDCMP_RAWKEY not set\n");
        errors++;
    } else {
        print("OK: IDCMP_RAWKEY flag set\n");
    }

    /* Verify window is linked to screen */
    if (screen2->FirstWindow != window) {
        print("FAIL: Window not linked to screen\n");
        errors++;
    } else {
        print("OK: Window linked to screen\n");
    }

    /* ========== Test 3: WaitTOF input processing ========== */
    print("\n--- Test 3: WaitTOF input processing ---\n");

    /* Call WaitTOF multiple times - this exercises the input processing path */
    /* Without SDL events, the message queue should remain empty */
    WaitTOF();
    WaitTOF();
    WaitTOF();
    print("OK: WaitTOF called (input processing hook)\n");

    /* Verify no spurious messages appeared (no SDL events in headless mode) */
    if (window->UserPort) {
        struct Message *msg = GetMsg(window->UserPort);
        if (msg != NULL) {
            print("Note: Received unexpected IDCMP message\n");
            /* Reply to it and free */
            ReplyMsg(msg);
        } else {
            print("OK: No spurious IDCMP messages (expected in headless mode)\n");
        }
    }

    /* ========== Cleanup ========== */
    print("\n--- Cleanup ---\n");

    CloseWindow(window);
    print("OK: Window closed\n");

    /* Close screen2 and verify screen1 becomes FirstScreen */
    CloseScreen(screen2);
    print("OK: Screen 2 closed\n");

    if (IntuitionBase->FirstScreen != screen1) {
        print("FAIL: Screen 1 not restored as FirstScreen after Screen 2 closed\n");
        errors++;
    } else {
        print("OK: Screen 1 is FirstScreen after Screen 2 closed\n");
    }

    CloseScreen(screen1);
    print("OK: Screen 1 closed\n");

    /* ========== Final result ========== */
    print("\n");
    if (errors == 0) {
        print("PASS: idcmp_input all tests passed\n");
        return 0;
    } else {
        print("FAIL: idcmp_input had errors\n");
        return 20;
    }
}
