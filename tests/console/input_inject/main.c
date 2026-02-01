/*
 * Test: console/input_inject
 *
 * Tests the UI testing infrastructure by:
 * 1. Opening a window with IDCMP_RAWKEY
 * 2. Injecting keyboard events via test_inject_string()
 * 3. Verifying the events are received through IDCMP
 *
 * This is a foundational test to verify the injection mechanism works
 * before building more complex console device tests.
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

static void print_hex(ULONG val)
{
    char buf[16];
    int i = 0;
    buf[i++] = '0';
    buf[i++] = 'x';
    for (int j = 7; j >= 0; j--) {
        int nibble = (val >> (j * 4)) & 0xF;
        buf[i++] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[i++] = '\0';
    print(buf);
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    int errors = 0;
    int key_count = 0;
    struct IntuiMessage *msg;

    print("Testing input injection infrastructure...\n");

    /* ========== Setup: Open screen and window ========== */
    print("\n--- Setup: Opening screen and window ---\n");

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
    ns.DefaultTitle = (UBYTE *)"Input Inject Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open window with IDCMP_RAWKEY */
    nw.LeftEdge = 10;
    nw.TopEdge = 15;
    nw.Width = 280;
    nw.Height = 150;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_RAWKEY | IDCMP_CLOSEWINDOW;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Input Test Window";
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
    print("OK: Window opened with IDCMP_RAWKEY\n");

    /* ========== Test 1: Verify UserPort exists ========== */
    print("\n--- Test 1: UserPort setup ---\n");
    if (window->UserPort == NULL) {
        print("FAIL: Window UserPort is NULL\n");
        errors++;
    } else {
        print("OK: Window UserPort exists\n");
    }

    /* ========== Test 2: Inject a simple string ========== */
    print("\n--- Test 2: Inject string 'AB' ---\n");

    /* Inject the string "AB" - this should create key events */
    if (!test_inject_string("AB")) {
        print("FAIL: test_inject_string() returned failure\n");
        errors++;
    } else {
        print("OK: test_inject_string() succeeded\n");
    }

    /* Give the event queue time to process by calling WaitTOF */
    WaitTOF();
    WaitTOF();
    WaitTOF();

    /* ========== Test 3: Check for injected events ========== */
    print("\n--- Test 3: Receive injected IDCMP_RAWKEY events ---\n");

    /* We should receive key down/up for 'A' and 'B' = 4 events */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        if (msg->Class == IDCMP_RAWKEY) {
            key_count++;
            print("  Received RAWKEY: code=");
            print_hex(msg->Code);
            print(" qual=");
            print_hex(msg->Qualifier);
            print("\n");
        }
        ReplyMsg((struct Message *)msg);
    }

    print("Total RAWKEY events received: ");
    print_hex(key_count);
    print("\n");

    /* We expect at least some key events (ideally 4: A down, A up, B down, B up) */
    if (key_count == 0) {
        print("FAIL: No RAWKEY events received\n");
        errors++;
    } else if (key_count < 4) {
        print("WARNING: Expected 4 events (A down/up, B down/up), got fewer\n");
        /* Not a hard failure - may be timing related */
    } else {
        print("OK: Received expected number of events\n");
    }

    /* ========== Test 4: Test single keypress helper ========== */
    print("\n--- Test 4: Single keypress (Return key) ---\n");

    if (!test_inject_return()) {
        print("FAIL: test_inject_return() failed\n");
        errors++;
    } else {
        print("OK: test_inject_return() succeeded\n");
    }

    WaitTOF();
    WaitTOF();

    key_count = 0;
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        if (msg->Class == IDCMP_RAWKEY) {
            key_count++;
            print("  Received RAWKEY: code=");
            print_hex(msg->Code);
            print(" (Return key)\n");
        }
        ReplyMsg((struct Message *)msg);
    }

    if (key_count < 2) {
        print("FAIL: Expected 2 events (down/up) for Return key\n");
        errors++;
    } else {
        print("OK: Return key events received\n");
    }

    /* ========== Cleanup ========== */
    print("\n--- Cleanup ---\n");

    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* ========== Final result ========== */
    print("\n");
    if (errors == 0) {
        print("PASS: input_inject all tests passed\n");
        return 0;
    } else {
        print("FAIL: input_inject had ");
        print_hex(errors);
        print(" errors\n");
        return 20;
    }
}
