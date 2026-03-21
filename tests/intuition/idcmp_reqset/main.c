/*
 * Test: intuition/idcmp_reqset
 * Tests IDCMP_REQSET and IDCMP_REQCLEAR message delivery
 * when requesters are opened and closed in a window.
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
    struct Requester req1, req2;
    struct IntuiMessage *msg;
    int errors = 0;

    print("Testing IDCMP_REQSET and IDCMP_REQCLEAR...\n");

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
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open a window with REQSET and REQCLEAR IDCMP */
    nw.LeftEdge = 50;
    nw.TopEdge = 30;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_REQSET | IDCMP_REQCLEAR;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"REQSET Test";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 100;
    nw.MinHeight = 80;
    nw.MaxWidth = 500;
    nw.MaxHeight = 400;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (!window) {
        print("FAIL: Could not open window\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened with REQSET|REQCLEAR\n");

    /* Drain any startup messages */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        ReplyMsg((struct Message *)msg);
    }

    /* Test 1: Opening a requester should post IDCMP_REQSET */
    print("Test 1: Request() posts IDCMP_REQSET...\n");
    InitRequester(&req1);
    req1.LeftEdge = 10;
    req1.TopEdge = 10;
    req1.Width = 100;
    req1.Height = 50;
    req1.BackFill = 1;
    req1.Flags = SIMPLEREQ;

    Request(&req1, window);

    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg) {
        print("  FAIL: No IDCMP message after Request()\n");
        errors++;
    } else if (msg->Class != IDCMP_REQSET) {
        print("  FAIL: Expected IDCMP_REQSET, got different class\n");
        ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: IDCMP_REQSET received\n");
        ReplyMsg((struct Message *)msg);
    }

    /* Test 2: EndRequest should post IDCMP_REQCLEAR */
    print("Test 2: EndRequest() posts IDCMP_REQCLEAR...\n");
    EndRequest(&req1, window);

    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg) {
        print("  FAIL: No IDCMP message after EndRequest()\n");
        errors++;
    } else if (msg->Class != IDCMP_REQCLEAR) {
        print("  FAIL: Expected IDCMP_REQCLEAR, got different class\n");
        ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: IDCMP_REQCLEAR received\n");
        ReplyMsg((struct Message *)msg);
    }

    /* Test 3: Two requesters produce two REQSET and two REQCLEAR */
    print("Test 3: Two requesters produce correct message sequence...\n");
    InitRequester(&req2);
    req2.LeftEdge = 20;
    req2.TopEdge = 20;
    req2.Width = 80;
    req2.Height = 40;
    req2.BackFill = 2;
    req2.Flags = SIMPLEREQ;

    Request(&req1, window);
    Request(&req2, window);

    /* Should get two REQSET messages */
    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg || msg->Class != IDCMP_REQSET) {
        print("  FAIL: First REQSET missing or wrong class\n");
        if (msg) ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: First IDCMP_REQSET received\n");
        ReplyMsg((struct Message *)msg);
    }

    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg || msg->Class != IDCMP_REQSET) {
        print("  FAIL: Second REQSET missing or wrong class\n");
        if (msg) ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: Second IDCMP_REQSET received\n");
        ReplyMsg((struct Message *)msg);
    }

    /* Close both requesters */
    EndRequest(&req2, window);
    EndRequest(&req1, window);

    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg || msg->Class != IDCMP_REQCLEAR) {
        print("  FAIL: First REQCLEAR missing or wrong class\n");
        if (msg) ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: First IDCMP_REQCLEAR received\n");
        ReplyMsg((struct Message *)msg);
    }

    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg || msg->Class != IDCMP_REQCLEAR) {
        print("  FAIL: Second REQCLEAR missing or wrong class\n");
        if (msg) ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: Second IDCMP_REQCLEAR received\n");
        ReplyMsg((struct Message *)msg);
    }

    /* Test 4: Without IDCMP flags, no messages should be posted */
    print("Test 4: No messages without IDCMP flags...\n");
    ModifyIDCMP(window, 0);
    /* Re-add just REQSET to verify we can selectively listen */
    ModifyIDCMP(window, IDCMP_REQSET);

    InitRequester(&req1);
    req1.LeftEdge = 10;
    req1.TopEdge = 10;
    req1.Width = 100;
    req1.Height = 50;
    req1.BackFill = 1;
    req1.Flags = SIMPLEREQ;

    Request(&req1, window);
    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (!msg || msg->Class != IDCMP_REQSET) {
        print("  FAIL: REQSET not received when flag set\n");
        if (msg) ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: IDCMP_REQSET received with flag set\n");
        ReplyMsg((struct Message *)msg);
    }

    EndRequest(&req1, window);
    /* REQCLEAR should NOT be posted since we only have REQSET */
    msg = (struct IntuiMessage *)GetMsg(window->UserPort);
    if (msg) {
        print("  FAIL: Unexpected message when REQCLEAR not requested\n");
        ReplyMsg((struct Message *)msg);
        errors++;
    } else {
        print("  OK: No IDCMP_REQCLEAR when flag not set\n");
    }

    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n");

    if (errors == 0) {
        print("PASS: idcmp_reqset all tests passed\n");
        return 0;
    } else {
        print("FAIL: idcmp_reqset had errors\n");
        return 20;
    }
}
