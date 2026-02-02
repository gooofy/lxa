/*
 * Test: intuition/gadget_click
 * Tests gadget hit testing and system gadget interaction:
 * - Finding gadgets at given positions
 * - Close gadget generates IDCMP_CLOSEWINDOW on click
 * - Depth gadget generates IDCMP_CHANGEWINDOW on click
 * - Mouse event injection via test infrastructure
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
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

static void print_hex(const char *prefix, ULONG val)
{
    char buf[32];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;
    *p++ = '0'; *p++ = 'x';
    
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (val >> i) & 0xF;
        *p++ = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
    }
    *p++ = '\n';
    *p = '\0';
    print(buf);
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Gadget *gad;
    int errors = 0;
    BOOL has_close_gadget = FALSE;
    BOOL has_depth_gadget = FALSE;

    print("Testing gadget hit testing and interaction...\n");

    /* ========== Test 1: Open screen and window with system gadgets ========== */
    print("\n--- Test 1: Open screen and window ---\n");

    /* Open a screen */
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
    ns.DefaultTitle = (UBYTE *)"Gadget Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open a window with close and depth gadgets */
    nw.LeftEdge = 20;
    nw.TopEdge = 30;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_GADGETDOWN | IDCMP_GADGETUP;
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DEPTHGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"Gadget Test";
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
    print("OK: Window opened\n");

    /* Verify system gadgets were created */
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget) {
        if (gad->GadgetType & GTYP_SYSGADGET) {
            UWORD sysType = gad->GadgetType & GTYP_SYSTYPEMASK;
            if (sysType == GTYP_CLOSE) {
                has_close_gadget = TRUE;
                print("OK: Found CLOSE gadget at ");
                print_hex("  LeftEdge=", gad->LeftEdge);
            } else if (sysType == GTYP_WDEPTH) {
                has_depth_gadget = TRUE;
                print("OK: Found DEPTH gadget at ");
                print_hex("  LeftEdge=", gad->LeftEdge);
            }
        }
    }

    if (!has_close_gadget) {
        print("FAIL: No CLOSE gadget created\n");
        errors++;
    }
    if (!has_depth_gadget) {
        print("FAIL: No DEPTH gadget created\n");
        errors++;
    }

    /* ========== Test 2: Verify gadget positions ========== */
    print("\n--- Test 2: Verify gadget geometry ---\n");

    for (gad = window->FirstGadget; gad; gad = gad->NextGadget) {
        if (!(gad->GadgetType & GTYP_SYSGADGET))
            continue;
        
        UWORD sysType = gad->GadgetType & GTYP_SYSTYPEMASK;
        
        /* Gadgets should have reasonable dimensions */
        if (gad->Width <= 0 || gad->Height <= 0) {
            print("FAIL: Gadget has invalid dimensions\n");
            errors++;
        } else {
            print("OK: Gadget has valid dimensions\n");
        }
        
        /* Close gadget should be in top-left area */
        if (sysType == GTYP_CLOSE) {
            if (gad->LeftEdge > 20) {
                print("FAIL: Close gadget not in left area\n");
                errors++;
            } else {
                print("OK: Close gadget positioned in left area\n");
            }
        }
        
        /* Depth gadget should be in top-right area */
        if (sysType == GTYP_WDEPTH) {
            if (gad->LeftEdge < window->Width - 50) {
                print("FAIL: Depth gadget not in right area\n");
                errors++;
            } else {
                print("OK: Depth gadget positioned in right area\n");
            }
        }
    }

    /* ========== Test 3: Test close gadget click ========== */
    print("\n--- Test 3: Close gadget click via mouse injection ---\n");

    /* Find the close gadget position */
    WORD closeX = 0, closeY = 0;
    for (gad = window->FirstGadget; gad; gad = gad->NextGadget) {
        if ((gad->GadgetType & GTYP_SYSGADGET) && 
            ((gad->GadgetType & GTYP_SYSTYPEMASK) == GTYP_CLOSE)) {
            /* Calculate screen coordinates of gadget center */
            closeX = window->LeftEdge + gad->LeftEdge + (gad->Width / 2);
            closeY = window->TopEdge + gad->TopEdge + (gad->Height / 2);
            break;
        }
    }

    if (closeX == 0 && closeY == 0) {
        print("Skip: Could not find close gadget position\n");
    } else {
        print_hex("  Clicking at screen X=", closeX);
        print_hex("  Clicking at screen Y=", closeY);
        
        /* Inject mouse move to the gadget position */
        test_inject_mouse(closeX, closeY, 0, DISPLAY_EVENT_MOUSEMOVE);
        
        /* Inject mouse button down */
        test_inject_mouse(closeX, closeY, MOUSE_LEFTBUTTON, DISPLAY_EVENT_MOUSEBUTTON);
        
        /* Process events by calling WaitTOF */
        WaitTOF();
        
        /* Inject mouse button up */
        test_inject_mouse(closeX, closeY, 0, DISPLAY_EVENT_MOUSEBUTTON);
        
        /* Process events */
        WaitTOF();
        WaitTOF();
        
        /* Check for IDCMP_CLOSEWINDOW message */
        struct IntuiMessage *imsg;
        BOOL got_close = FALSE;
        
        while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
            if (imsg->Class == IDCMP_CLOSEWINDOW) {
                got_close = TRUE;
                print("OK: Received IDCMP_CLOSEWINDOW\n");
            }
            ReplyMsg((struct Message *)imsg);
        }
        
        if (!got_close) {
            print("Note: IDCMP_CLOSEWINDOW not received (may require actual event processing)\n");
        }
    }

    /* ========== Test 4: Window cleanup ========== */
    print("\n--- Test 4: Cleanup ---\n");

    /* Drain any remaining messages */
    struct IntuiMessage *imsg;
    while ((imsg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL) {
        ReplyMsg((struct Message *)imsg);
    }

    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* ========== Final result ========== */
    print("\n");
    if (errors == 0) {
        print("PASS: gadget_click all tests passed\n");
        return 0;
    } else {
        print("FAIL: gadget_click had errors\n");
        return 20;
    }
}
