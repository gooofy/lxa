/*
 * Test: intuition/idcmp_sizeverify
 * Tests IDCMP_SIZEVERIFY message delivery when a window's sizing
 * gadget is activated.  The host driver injects a mouse click on
 * the sizing gadget to trigger the message.
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

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct IntuiMessage *msg;
    int errors = 0;
    int got_sizeverify = 0;

    print("Testing IDCMP_SIZEVERIFY...\n");

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
    ns.DefaultTitle = (UBYTE *)"SizeVerify Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen)
    {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open window with sizing gadget and SIZEVERIFY */
    nw.LeftEdge = 50;
    nw.TopEdge = 30;
    nw.Width = 300;
    nw.Height = 150;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_SIZEVERIFY | IDCMP_NEWSIZE;
    nw.Flags = WFLG_DRAGBAR | WFLG_SIZEGADGET | WFLG_ACTIVATE
             | WFLG_SIZEBBOTTOM;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"SizeVerify Window";
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
    print("OK: Window opened with SIZEVERIFY|NEWSIZE\n");

    /* Drain startup messages */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    /* Signal to host driver that we're ready for size gadget click */
    print("READY: click sizing gadget\n");

    /* Wait for SIZEVERIFY and/or NEWSIZE messages.
     * Use WaitPort() to yield to the host until a message arrives. */
    WaitPort(window->UserPort);
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        if (msg->Class == IDCMP_SIZEVERIFY)
        {
            print("  OK: IDCMP_SIZEVERIFY received\n");
            got_sizeverify = 1;
        }
        else if (msg->Class == IDCMP_NEWSIZE)
        {
            print("  OK: IDCMP_NEWSIZE received\n");
        }
        ReplyMsg((struct Message *)msg);
    }

    if (!got_sizeverify)
    {
        print("  FAIL: No IDCMP_SIZEVERIFY received\n");
        errors++;
    }

    /* Test 2: Without SIZEVERIFY flag, should not get the message */
    print("Test 2: No SIZEVERIFY without flag...\n");
    ModifyIDCMP(window, IDCMP_NEWSIZE);

    /* Drain */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    print("READY: click sizing gadget again\n");

    got_sizeverify = 0;
    WaitPort(window->UserPort);
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        if (msg->Class == IDCMP_SIZEVERIFY)
        {
            print("  FAIL: Got SIZEVERIFY when flag not set\n");
            got_sizeverify = 1;
            errors++;
        }
        else if (msg->Class == IDCMP_NEWSIZE)
        {
            print("  OK: Got only NEWSIZE (no SIZEVERIFY)\n");
        }
        ReplyMsg((struct Message *)msg);
    }

    if (!got_sizeverify)
    {
        print("  OK: No SIZEVERIFY without flag\n");
    }

    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n");

    if (errors == 0)
    {
        print("PASS: idcmp_sizeverify all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: idcmp_sizeverify had errors\n");
        return 20;
    }
}
