/*
 * Test: intuition/idcmp
 * Tests ModifyIDCMP() and IDCMP message port handling
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
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
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct MsgPort *port;
    int errors = 0;

    print("Testing ModifyIDCMP()...\n");

    /* First open a screen */
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
    ns.DefaultTitle = (UBYTE *)"Test Screen";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (screen == NULL) {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open a window WITHOUT IDCMP flags first */
    nw.LeftEdge = 10;
    nw.TopEdge = 15;
    nw.Width = 200;
    nw.Height = 100;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = 0;  /* No IDCMP initially */
    nw.Flags = WFLG_CLOSEGADGET | WFLG_DRAGBAR;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"IDCMP Test";
    nw.Screen = screen;
    nw.BitMap = NULL;
    nw.MinWidth = 0;
    nw.MinHeight = 0;
    nw.MaxWidth = 0;
    nw.MaxHeight = 0;
    nw.Type = CUSTOMSCREEN;

    window = OpenWindow(&nw);
    if (window == NULL) {
        print("FAIL: OpenWindow() returned NULL\n");
        CloseScreen(screen);
        return 20;
    }
    print("OK: Window opened without IDCMP\n");

    /* Verify no UserPort since IDCMPFlags was 0 */
    if (window->UserPort != NULL) {
        print("FAIL: UserPort should be NULL when IDCMPFlags=0\n");
        errors++;
    } else {
        print("OK: UserPort is NULL (no IDCMP requested)\n");
    }

    /* Now add IDCMP flags using ModifyIDCMP */
    if (!ModifyIDCMP(window, IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS)) {
        print("FAIL: ModifyIDCMP() returned FALSE\n");
        errors++;
    } else {
        print("OK: ModifyIDCMP() succeeded\n");
    }

    /* Verify UserPort was created */
    if (window->UserPort == NULL) {
        print("FAIL: UserPort not created after ModifyIDCMP\n");
        errors++;
    } else {
        print("OK: UserPort created after ModifyIDCMP\n");
    }

    /* Verify IDCMPFlags were updated */
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

    /* Save the port pointer for comparison */
    port = window->UserPort;

    /* Modify IDCMP flags - change to different flags */
    ModifyIDCMP(window, IDCMP_RAWKEY);

    /* Verify flags changed but port is the same */
    if (window->UserPort != port) {
        print("FAIL: UserPort pointer changed when just modifying flags\n");
        errors++;
    } else {
        print("OK: UserPort pointer unchanged after flag modification\n");
    }

    if (window->IDCMPFlags & IDCMP_CLOSEWINDOW) {
        print("FAIL: IDCMP_CLOSEWINDOW still set after modify\n");
        errors++;
    } else {
        print("OK: IDCMP_CLOSEWINDOW cleared after modify\n");
    }

    if (!(window->IDCMPFlags & IDCMP_RAWKEY)) {
        print("FAIL: IDCMP_RAWKEY not set after modify\n");
        errors++;
    } else {
        print("OK: IDCMP_RAWKEY set after modify\n");
    }

    /* Port should still exist (just flags changed) */
    if (window->UserPort == NULL) {
        print("FAIL: UserPort removed when just changing flags\n");
        errors++;
    } else {
        print("OK: UserPort still exists after flag change\n");
    }

    /* Now clear all IDCMP flags */
    ModifyIDCMP(window, 0);

    /* Verify port was removed */
    if (window->UserPort != NULL) {
        print("FAIL: UserPort not removed when IDCMPFlags=0\n");
        errors++;
    } else {
        print("OK: UserPort removed when IDCMPFlags cleared\n");
    }

    /* Verify flags are zero */
    if (window->IDCMPFlags != 0) {
        print("FAIL: IDCMPFlags not zero after clear\n");
        errors++;
    } else {
        print("OK: IDCMPFlags cleared\n");
    }

    /* Clean up */
    CloseWindow(window);
    print("OK: Window closed\n");

    CloseScreen(screen);
    print("OK: Screen closed\n");

    /* Final result */
    if (errors == 0) {
        print("PASS: idcmp all tests passed\n");
        return 0;
    } else {
        print("FAIL: idcmp had errors\n");
        return 20;
    }
}
