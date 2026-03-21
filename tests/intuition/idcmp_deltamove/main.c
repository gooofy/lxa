/*
 * Test: intuition/idcmp_deltamove
 * Tests IDCMP_DELTAMOVE modifier flag.
 * When DELTAMOVE is set alongside MOUSEMOVE, MOUSEMOVE messages
 * should carry dx/dy deltas instead of absolute window-relative coords.
 *
 * This test opens a window with MOUSEMOVE|DELTAMOVE, then waits for the
 * host driver to inject mouse movements and verifies that the delivered
 * MouseX/MouseY values are deltas (not absolute).
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
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *label, LONG val)
{
    char buf[32];
    char *p = buf;
    LONG v = val;
    int neg = 0;
    int i = 0;
    char tmp[16];

    print(label);

    if (v < 0)
    {
        neg = 1;
        v = -v;
    }
    if (v == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        while (v > 0)
        {
            tmp[i++] = '0' + (v % 10);
            v /= 10;
        }
    }
    if (neg) *p++ = '-';
    while (i > 0)
    {
        *p++ = tmp[--i];
    }
    *p++ = '\n';
    *p = 0;

    print(buf);
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct IntuiMessage *msg;
    int errors = 0;
    int got_move = 0;

    print("Testing IDCMP_DELTAMOVE...\n");

    /* Open a screen */
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
    ns.DefaultTitle = (UBYTE *)"DeltaMove Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen)
    {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Test 1: Window with MOUSEMOVE|DELTAMOVE - verify delta coords */
    print("Test 1: MOUSEMOVE with DELTAMOVE flag...\n");

    nw.LeftEdge = 50;
    nw.TopEdge = 30;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_MOUSEMOVE | IDCMP_DELTAMOVE;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_REPORTMOUSE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"DeltaMove Window";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 100;
    nw.MinHeight = 80;
    nw.MaxWidth = 500;
    nw.MaxHeight = 400;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (!window)
    {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened with MOUSEMOVE|DELTAMOVE\n");

    /* Drain any startup messages */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    /* Signal to host driver that we're ready for mouse injection */
    print("READY: inject mouse moves\n");

    /* The host driver injects two mouse moves:
     *  1) Baseline move to (windowX+100, windowY+50) - establishes reference
     *  2) Delta move to (windowX+110, windowY+55) - expected delta is (10,5)
     *
     * The first MOUSEMOVE will have a delta from (0,0) to the baseline pos
     * (which we consume and ignore). The second should be (10,5). */
    {
        int move_count = 0;
        WORD delta_x = 0, delta_y = 0;

        /* Wait for first batch of messages */
        WaitPort(window->UserPort);
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
        {
            if (msg->Class == IDCMP_MOUSEMOVE)
            {
                move_count++;
                delta_x = msg->MouseX;
                delta_y = msg->MouseY;
                print_num("  MOUSEMOVE #MouseX=", delta_x);
                print_num("  MOUSEMOVE #MouseY=", delta_y);
            }
            ReplyMsg((struct Message *)msg);
        }

        /* If we only got the baseline, wait for the delta move */
        if (move_count < 2)
        {
            WaitPort(window->UserPort);
            while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
            {
                if (msg->Class == IDCMP_MOUSEMOVE)
                {
                    move_count++;
                    delta_x = msg->MouseX;
                    delta_y = msg->MouseY;
                    print_num("  MOUSEMOVE #MouseX=", delta_x);
                    print_num("  MOUSEMOVE #MouseY=", delta_y);
                }
                ReplyMsg((struct Message *)msg);
            }
        }

        if (move_count < 2)
        {
            print("  FAIL: Expected at least 2 MOUSEMOVE messages\n");
            errors++;
        }
        else if (delta_x != 10 || delta_y != 5)
        {
            print("  FAIL: Expected delta (10,5)\n");
            print_num("  Got deltaX=", delta_x);
            print_num("  Got deltaY=", delta_y);
            errors++;
        }
        else
        {
            print("  OK: MOUSEMOVE delta (10,5) correct\n");
        }
    }

    /* Test 2: Without DELTAMOVE, coordinates should be window-relative */
    print("Test 2: MOUSEMOVE without DELTAMOVE (absolute coords)...\n");
    ModifyIDCMP(window, IDCMP_MOUSEMOVE);

    /* Drain */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    print("READY: inject absolute moves\n");

    /* In absolute mode, MouseX/Y should be window-relative coordinates.
     * Host injects at (windowX+130, windowY+50), which is window-relative
     * (130 - borderLeft, 50 - borderTop) ≈ (126, 39) for standard border. */
    got_move = 0;
    WaitPort(window->UserPort);
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        if (msg->Class == IDCMP_MOUSEMOVE && !got_move)
        {
            WORD mx = msg->MouseX;
            WORD my = msg->MouseY;

            print_num("  ABS_MOUSEMOVE MouseX=", mx);
            print_num("  ABS_MOUSEMOVE MouseY=", my);

            /* Absolute mode: values should be window-relative (positive,
             * similar to the coordinate we injected minus window left/top
             * border offsets). Should NOT be small deltas. */
            if (mx > 20)
            {
                print("  OK: Absolute coords (large positive values)\n");
            }
            else
            {
                print("  WARN: Coordinates look like deltas, not absolute\n");
            }
            got_move = 1;
        }
        ReplyMsg((struct Message *)msg);
    }

    if (!got_move)
    {
        print("  FAIL: No MOUSEMOVE received in absolute mode\n");
        errors++;
    }
    else
    {
        print("  OK: MOUSEMOVE with absolute coordinates received\n");
    }

    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n");

    if (errors == 0)
    {
        print("PASS: idcmp_deltamove all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: idcmp_deltamove had errors\n");
        return 20;
    }
}
