/*
 * Test: intuition/requester_basic
 * Tests InitRequester, Request, EndRequest, BuildSysRequest, FreeSysRequest,
 * SysReqHandler, and DMRequest helpers.
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
#include <exec/ports.h>
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

static struct Border req_border = {
    0, 0,
    2, 0,
    JAM1,
    5,
    NULL,
    NULL
};

static SHORT req_border_xy[] = {
    0, 39,
    0, 0,
    119, 0,
    119, 39,
    0, 39
};

static struct IntuiText req_text = {
    1, 0,
    JAM1,
    8, 12,
    NULL,
    (UBYTE *)"Requester active",
    NULL
};

static struct IntuiText sys_body_2 = {
    1, 0,
    JAM1,
    0, 10,
    NULL,
    (UBYTE *)"Continue test?",
    NULL
};

static struct IntuiText sys_body_1 = {
    1, 0,
    JAM1,
    0, 0,
    NULL,
    (UBYTE *)"System requester",
    &sys_body_2
};

static struct IntuiText sys_pos = {
    1, 0,
    JAM1,
    0, 0,
    NULL,
    (UBYTE *)"Retry",
    NULL
};

static struct IntuiText sys_neg = {
    1, 0,
    JAM1,
    0, 0,
    NULL,
    (UBYTE *)"Cancel",
    NULL
};

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Window *sys_window;
    struct Requester requester;
    BOOL result;
    LONG sys_result;
    ULONG idcmp_class;
    struct IntuiMessage *msg;
    
    print("Testing Requester Functions...\n\n");
    
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
    
    /* Open a window */
    nw.LeftEdge = 100;
    nw.TopEdge = 50;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
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
    print("OK: Window opened\n\n");
    
    /* Test 1: InitRequester */
    print("Test 1: InitRequester()...\n");
    InitRequester(&requester);
    print("  OK: InitRequester called\n");
    if (requester.LeftEdge == 0 && requester.TopEdge == 0 &&
        requester.Width == 0 && requester.Height == 0 &&
        requester.ReqGadget == NULL && requester.ReqBorder == NULL &&
        requester.ReqText == NULL && requester.RWindow == NULL &&
        requester.ReqLayer == NULL && requester.OlderRequest == NULL &&
        requester.Flags == 0) {
        print("  OK: InitRequester clears the whole structure\n");
    } else {
        print("  FAIL: InitRequester did not clear the whole structure\n");
    }
    print("\n");
    
    /* Test 2: Request */
    print("Test 2: Request()...\n");
    requester.LeftEdge = 50;
    requester.TopEdge = 30;
    requester.Width = 200;
    requester.Height = 100;
    requester.RelLeft = 7;
    requester.RelTop = -5;
    requester.ReqGadget = NULL;
    requester.ReqBorder = &req_border;
    requester.ReqText = &req_text;
    requester.Flags = POINTREL | SIMPLEREQ;
    requester.BackFill = 1;
    requester.ReqLayer = NULL;
    requester.ImageBMap = NULL;
    requester.RWindow = NULL;
    requester.ReqImage = NULL;
    req_border.XY = req_border_xy;
    
    result = Request(&requester, window);
    if (result && window->FirstRequest == &requester &&
        (requester.Flags & REQACTIVE) && requester.RWindow == window &&
        requester.ReqLayer == window->WLayer) {
        print("  OK: Request linked and activated the requester\n");
    } else {
        print("  FAIL: Request did not fully activate the requester\n");
    }
    print("\n");
    
    /* Test 3: EndRequest */
    print("Test 3: EndRequest()...\n");
    EndRequest(&requester, window);
    if (window->FirstRequest == NULL && !(requester.Flags & REQACTIVE) &&
        requester.RWindow == NULL && requester.ReqLayer == NULL &&
        requester.OlderRequest == NULL) {
        print("  OK: EndRequest unlinked and cleaned up the requester\n\n");
    } else {
        print("  FAIL: EndRequest did not fully clean up the requester\n\n");
    }

    /* Test 3b: SetDMRequest / ClearDMRequest */
    print("Test 3b: SetDMRequest() / ClearDMRequest()...\n");
    requester.Flags = 0;
    if (!SetDMRequest(window, &requester)) {
        print("  FAIL: SetDMRequest returned FALSE for inactive requester\n\n");
    } else if (window->DMRequest != &requester) {
        print("  FAIL: SetDMRequest did not attach the requester\n\n");
    } else if (!ClearDMRequest(window)) {
        print("  FAIL: ClearDMRequest returned FALSE for detached requester\n\n");
    } else if (window->DMRequest != NULL) {
        print("  FAIL: ClearDMRequest did not clear the window DMRequest pointer\n\n");
    } else {
        requester.Flags = REQACTIVE;
        window->DMRequest = &requester;
        if (ClearDMRequest(window)) {
            print("  FAIL: ClearDMRequest detached an active requester\n\n");
        } else if (SetDMRequest(window, NULL)) {
            print("  FAIL: SetDMRequest replaced an active requester\n\n");
        } else {
            requester.Flags = 0;
            window->DMRequest = NULL;
            print("  OK: DMRequest helpers honor attach, clear, and active-requester refusal\n\n");
        }
    }

    /* Test 4: BuildSysRequest and SysReqHandler cancel path (negative gadget).
     *
     * The host-side driver clicks the requester's negative gadget (e.g.
     * "Cancel") after seeing the READY: marker. SysReqHandler blocks via
     * Wait() until the IDCMP_GADGETUP for that gadget arrives. The test
     * then checks that SysReqHandler reported the negative gadget's ID. */
    print("Test 4: BuildSysRequest() / SysReqHandler(negative gadget)...\n");
    sys_window = BuildSysRequest(window, &sys_body_1, &sys_pos, &sys_neg,
                                 IDCMP_CLOSEWINDOW, 0, 0);
    if (!sys_window || sys_window == (struct Window *)1) {
        print("  FAIL: BuildSysRequest did not open requester window\n\n");
    } else {
        struct Gadget *first_gad = sys_window->FirstGadget;
        struct Gadget *neg_gad = NULL;
        if (first_gad && first_gad->NextGadget)
            neg_gad = first_gad->NextGadget;

        if (sys_window->UserPort != NULL && first_gad != NULL && neg_gad != NULL) {
            print("  OK: BuildSysRequest opened a requester window\n");
        } else {
            print("  FAIL: BuildSysRequest window missing IDCMP/gadget setup\n");
        }

        /* Drain any startup messages */
        while ((msg = (struct IntuiMessage *)GetMsg(sys_window->UserPort)) != NULL) {
            ReplyMsg((struct Message *)msg);
        }

        /* Sync point: tell host driver the requester is ready for a click */
        print("READY: requester1_negative\n");

        idcmp_class = 0;
        sys_result = SysReqHandler(sys_window, &idcmp_class, TRUE);
        if (idcmp_class == IDCMP_GADGETUP && neg_gad &&
            sys_result == neg_gad->GadgetID) {
            print("  OK: SysReqHandler returns cancel for IDCMP_CLOSEWINDOW\n");
        } else {
            print("  FAIL: SysReqHandler did not report IDCMP_CLOSEWINDOW correctly\n");
        }
        FreeSysRequest(sys_window);
        print("\n");
    }

    /* Test 5: BuildSysRequest and SysReqHandler positive gadget path.
     *
     * Host driver clicks the positive gadget ("Retry") after the READY:
     * marker. SysReqHandler blocks until IDCMP_GADGETUP arrives. */
    print("Test 5: BuildSysRequest() / SysReqHandler(IDCMP_GADGETUP)...\n");
    sys_window = BuildSysRequest(window, &sys_body_1, &sys_pos, &sys_neg,
                                 IDCMP_CLOSEWINDOW, 0, 0);
    if (!sys_window || sys_window == (struct Window *)1) {
        print("  FAIL: BuildSysRequest did not open second requester window\n\n");
    } else {
        struct Gadget *gad = sys_window->FirstGadget;

        if (gad && gad->NextGadget != NULL) {
            print("  OK: BuildSysRequest created response gadgets\n");
        } else {
            print("  FAIL: BuildSysRequest did not create response gadgets\n");
        }

        /* Drain any startup messages */
        while ((msg = (struct IntuiMessage *)GetMsg(sys_window->UserPort)) != NULL) {
            ReplyMsg((struct Message *)msg);
        }

        /* Sync point: tell host driver the requester is ready for a gadget click */
        print("READY: requester2_positive\n");

        idcmp_class = 0;
        sys_result = SysReqHandler(sys_window, &idcmp_class, TRUE);
        if (idcmp_class == IDCMP_GADGETUP && gad &&
            sys_result == gad->GadgetID) {
            print("  OK: SysReqHandler returns the gadget ID\n");
        } else {
            print("  FAIL: SysReqHandler did not return the gadget ID\n");
        }

        FreeSysRequest(sys_window);
        print("  OK: FreeSysRequest closed the system requester\n\n");
    }
    
    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    print("PASS: requester_basic all tests completed\n");
    return 0;
}
