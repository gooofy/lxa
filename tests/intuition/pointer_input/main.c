/*
 * Test: intuition/pointer_input
 * Tests SetPointer/ClearPointer, SetMouseQueue, CurrentTime, DisplayAlert,
 * and DisplayBeep.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
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

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static UWORD busy_pointer[] = {
    0x0000, 0x0000,
    0x0400, 0x07C0,
    0x0000, 0x07C0,
    0x0100, 0x0380,
    0x0000, 0x07E0,
    0x07C0, 0x1FF8,
    0x1FF0, 0x3FEC,
    0x3FF8, 0x7FDE,
    0x3FF8, 0x7FBE,
    0x7FFC, 0xFF7F,
    0x7EFC, 0xFFFF,
    0x7FFC, 0xFFFF,
    0x3FF8, 0x7FFE,
    0x3FF8, 0x7FFE,
    0x1FF0, 0x3FFC,
    0x07C0, 0x1FF8,
    0x0000, 0x07E0,
    0x0000, 0x0000
};

static UBYTE alert_text[] = {
    0x00,
    'T', 'e', 's', 't', ' ', 'a', 'l', 'e', 'r', 't', 0,
    0
};

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct IntuiMessage *first_move;
    struct IntuiMessage *second_move;
    struct IntuiMessage *msg;
    ULONG secs1, micros1, secs2, micros2;
    LONG old_queue;
    BOOL alert_result;
    int i;
    int errors = 0;

    print("Testing pointer/input Intuition helpers...\n\n");

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
    ns.DefaultTitle = (UBYTE *)"Pointer Input";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen) {
        print("FAIL: Could not open screen\n");
        return 20;
    }

    nw.LeftEdge = 20;
    nw.TopEdge = 15;
    nw.Width = 220;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_MOUSEMOVE | IDCMP_CLOSEWINDOW;
    nw.Flags = WFLG_DRAGBAR | WFLG_CLOSEGADGET | WFLG_ACTIVATE | WFLG_REPORTMOUSE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Pointer Input Test";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }

    print("Test 1: SetPointer() / ClearPointer()...\n");
    SetPointer(window, busy_pointer, 16, 16, -6, 0);
    if (window->Pointer == busy_pointer &&
        window->PtrHeight == 16 && window->PtrWidth == 16 &&
        window->XOffset == -6 && window->YOffset == 0) {
        print("  OK: SetPointer stores pointer metadata\n");
    } else {
        print("  FAIL: SetPointer did not store pointer metadata\n");
        errors++;
    }

    ClearPointer(window);
    if (window->Pointer == NULL &&
        window->PtrHeight == 0 && window->PtrWidth == 0 &&
        window->XOffset == 0 && window->YOffset == 0) {
        print("  OK: ClearPointer restores default pointer state\n\n");
    } else {
        print("  FAIL: ClearPointer did not restore pointer state\n\n");
        errors++;
    }

    print("Test 2: CurrentTime()...\n");
    CurrentTime(&secs1, &micros1);
    Delay(2);
    CurrentTime(&secs2, &micros2);
    if ((secs2 > secs1) || (secs2 == secs1 && micros2 >= micros1)) {
        print("  OK: CurrentTime returns monotonic snapshots\n\n");
    } else {
        print("  FAIL: CurrentTime moved backwards\n\n");
        errors++;
    }

    print("Test 3: SetMouseQueue()...\n");
    old_queue = SetMouseQueue(window, 1);
    if (old_queue == 5) {
        print("  OK: SetMouseQueue returns the default queue length\n");
    } else {
        print("  FAIL: SetMouseQueue did not return the default queue length\n");
        errors++;
    }

    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
        ReplyMsg((struct Message *)msg);

    print("  READY: queue test\n");

    first_move = NULL;
    for (i = 0; i < 100; i++) {
        WaitTOF();
        first_move = (struct IntuiMessage *)GetMsg(window->UserPort);
        if (first_move != NULL)
            break;
    }

    if (first_move != NULL && first_move->Class == IDCMP_MOUSEMOVE) {
        print("  OK: One mouse move message queued\n");

        for (i = 0; i < 10; i++)
            WaitTOF();

        second_move = (struct IntuiMessage *)GetMsg(window->UserPort);
        if (second_move == NULL) {
            print("  OK: SetMouseQueue limits outstanding mouse moves\n\n");
        } else {
            print("  FAIL: SetMouseQueue allowed too many mouse moves\n\n");
            ReplyMsg((struct Message *)second_move);
            errors++;
        }

        ReplyMsg((struct Message *)first_move);
    } else {
        print("  FAIL: No mouse move message was queued\n\n");
        errors++;
    }

    print("Test 4: DisplayAlert() / DisplayBeep()...\n");
    alert_result = DisplayAlert(RECOVERY_ALERT, alert_text, 20);
    if (alert_result == TRUE) {
        print("  OK: DisplayAlert returns continue for recovery alerts\n");
    } else {
        print("  FAIL: DisplayAlert did not return continue for recovery alerts\n");
        errors++;
    }

    alert_result = DisplayAlert(DEADEND_ALERT, alert_text, 20);
    if (alert_result == FALSE) {
        print("  OK: DisplayAlert returns FALSE for dead-end alerts\n");
    } else {
        print("  FAIL: DisplayAlert did not return FALSE for dead-end alerts\n");
        errors++;
    }

    DisplayBeep(screen);
    DisplayBeep(NULL);
    print("  OK: DisplayBeep accepted screen-specific and global calls\n\n");

    CloseWindow(window);
    CloseScreen(screen);

    if (errors == 0) {
        print("PASS: pointer_input all tests completed\n");
        return 0;
    }

    print("FAIL: pointer_input had errors\n");
    return 20;
}
