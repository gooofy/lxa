/*
 * Test: intuition/idcmp_menuverify
 * Tests IDCMP_MENUVERIFY message delivery when the user presses
 * the right mouse button (RMB) to open menus.  Per RKRM, the active
 * window gets MENUHOT and other MENUVERIFY-interested windows get
 * MENUWAITING.
 *
 * The host driver injects an RMB-down event to trigger the message.
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

static void print_hex(const char *label, ULONG val)
{
    char buf[32];
    char *p = buf;
    const char *hex = "0123456789ABCDEF";
    int i;

    print(label);

    *p++ = '0';
    *p++ = 'x';
    for (i = 7; i >= 0; i--)
    {
        *p++ = hex[(val >> (i * 4)) & 0xF];
    }
    *p++ = '\n';
    *p = 0;

    print(buf);
}

/* Simple menu setup for a window */
static struct IntuiText menu_text = {
    0, 1, JAM2, 0, 0, NULL, (UBYTE *)"Test Item", NULL
};

static struct MenuItem menu_item = {
    NULL, 0, 0, 80, 10, ITEMTEXT | ITEMENABLED | HIGHCOMP,
    0, (APTR)&menu_text, NULL, 0, NULL, 0
};

static struct Menu menu = {
    NULL, 5, 0, 60, 10, MENUENABLED, (BYTE *)"File", &menu_item,
    0, 0, 0, 0
};

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct IntuiMessage *msg;
    int errors = 0;
    int got_menuverify = 0;
    UWORD code;

    print("Testing IDCMP_MENUVERIFY...\n");

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
    ns.DefaultTitle = (UBYTE *)"MenuVerify Test";
    ns.Gadgets = NULL;
    ns.CustomBitMap = NULL;

    screen = OpenScreen(&ns);
    if (!screen)
    {
        print("FAIL: Could not open screen\n");
        return 20;
    }
    print("OK: Screen opened\n");

    /* Open window with MENUVERIFY and a menu strip */
    nw.LeftEdge = 50;
    nw.TopEdge = 30;
    nw.Width = 400;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_MENUVERIFY | IDCMP_MENUPICK;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE;
    nw.FirstGadget = NULL;
    nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"MenuVerify Window";
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
    print("OK: Window opened with MENUVERIFY|MENUPICK\n");

    /* Attach menu strip */
    SetMenuStrip(window, &menu);
    print("OK: Menu strip attached\n");

    /* Drain startup messages */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    /* Signal to host driver that we're ready for RMB */
    print("READY: press RMB\n");

    /* Wait for MENUVERIFY message with MENUHOT code.
     * Use WaitPort() to yield to the host until a message arrives. */
    WaitPort(window->UserPort);
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        if (msg->Class == IDCMP_MENUVERIFY)
        {
            code = msg->Code;
            print("  OK: IDCMP_MENUVERIFY received\n");
            print_hex("  Code=", (ULONG)code);

            if (code == MENUHOT)
            {
                print("  OK: Code is MENUHOT\n");
                got_menuverify = 1;
            }
            else if (code == MENUWAITING)
            {
                print("  OK: Code is MENUWAITING (unexpected for active window)\n");
            }
            else
            {
                print("  FAIL: Unknown MENUVERIFY code\n");
                errors++;
            }
        }
        else if (msg->Class == IDCMP_MENUPICK)
        {
            print("  OK: IDCMP_MENUPICK received\n");
        }
        ReplyMsg((struct Message *)msg);
    }

    if (!got_menuverify)
    {
        print("  FAIL: No IDCMP_MENUVERIFY with MENUHOT received\n");
        errors++;
    }

    /* Drain remaining menu messages */
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        ReplyMsg((struct Message *)msg);
    }

    /* Test 2: Without MENUVERIFY, the flag gating should prevent delivery.
     * We verify this by checking that ModifyIDCMP removes MENUVERIFY
     * from the window's IDCMPFlags and that MENUVERIFY is not in the mask. */
    print("Test 2: No MENUVERIFY without flag...\n");
    ModifyIDCMP(window, IDCMP_MENUPICK);

    if (window->IDCMPFlags & IDCMP_MENUVERIFY)
    {
        print("  FAIL: MENUVERIFY flag still set after ModifyIDCMP\n");
        errors++;
    }
    else
    {
        print("  OK: MENUVERIFY flag cleared, no delivery possible\n");
    }

    /* Cleanup */
    ClearMenuStrip(window);
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n");

    if (errors == 0)
    {
        print("PASS: idcmp_menuverify all tests passed\n");
        return 0;
    }
    else
    {
        print("FAIL: idcmp_menuverify had errors\n");
        return 20;
    }
}
