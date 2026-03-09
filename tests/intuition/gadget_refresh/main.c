/*
 * Test: intuition/gadget_refresh
 * Tests gadget list and refresh helpers
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <string.h>
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
    struct Gadget gadget1, gadget2, gadget3, gadget4, string_gadget;
    struct StringInfo string_info;
    UBYTE string_buffer[16];
    UWORD pos;
    UWORD removed;
    int errors = 0;
    
    print("Testing Gadget Refresh Functions...\n\n");
    
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
    
    memset(&gadget1, 0, sizeof(gadget1));
    memset(&gadget2, 0, sizeof(gadget2));
    memset(&gadget3, 0, sizeof(gadget3));
    memset(&gadget4, 0, sizeof(gadget4));
    memset(&string_gadget, 0, sizeof(string_gadget));
    memset(&string_info, 0, sizeof(string_info));
    memset(string_buffer, 0, sizeof(string_buffer));

    /* Initialize gadgets */
    gadget1.NextGadget = &gadget2;
    gadget1.LeftEdge = 10;
    gadget1.TopEdge = 10;
    gadget1.Width = 100;
    gadget1.Height = 20;
    gadget1.Flags = GFLG_GADGHCOMP;
    gadget1.Activation = 0;
    gadget1.GadgetType = GTYP_BOOLGADGET;
    gadget1.GadgetRender = NULL;
    gadget1.SelectRender = NULL;
    gadget1.GadgetText = NULL;
    gadget1.MutualExclude = 0;
    gadget1.SpecialInfo = NULL;
    gadget1.GadgetID = 1;
    gadget1.UserData = NULL;
    
    gadget2.NextGadget = NULL;
    gadget2.LeftEdge = 10;
    gadget2.TopEdge = 40;
    gadget2.Width = 100;
    gadget2.Height = 20;
    gadget2.Flags = GFLG_GADGHCOMP;
    gadget2.Activation = 0;
    gadget2.GadgetType = GTYP_BOOLGADGET;
    gadget2.GadgetRender = NULL;
    gadget2.SelectRender = NULL;
    gadget2.GadgetText = NULL;
    gadget2.MutualExclude = 0;
    gadget2.SpecialInfo = NULL;
    gadget2.GadgetID = 2;
    gadget2.UserData = NULL;

    gadget3.NextGadget = &gadget4;
    gadget3.LeftEdge = 130;
    gadget3.TopEdge = 10;
    gadget3.Width = 100;
    gadget3.Height = 20;
    gadget3.Flags = GFLG_GADGHCOMP;
    gadget3.GadgetType = GTYP_BOOLGADGET;
    gadget3.GadgetID = 3;

    gadget4.NextGadget = NULL;
    gadget4.LeftEdge = 130;
    gadget4.TopEdge = 40;
    gadget4.Width = 100;
    gadget4.Height = 20;
    gadget4.Flags = GFLG_GADGHCOMP;
    gadget4.GadgetType = GTYP_BOOLGADGET;
    gadget4.GadgetID = 4;

    strcpy((char *)string_buffer, "HELLO");
    string_info.Buffer = string_buffer;
    string_info.BufferPos = 0;
    string_info.MaxChars = sizeof(string_buffer) - 1;

    string_gadget.NextGadget = NULL;
    string_gadget.LeftEdge = 10;
    string_gadget.TopEdge = 80;
    string_gadget.Width = 140;
    string_gadget.Height = 12;
    string_gadget.Flags = GFLG_GADGHCOMP;
    string_gadget.Activation = GACT_RELVERIFY;
    string_gadget.GadgetType = GTYP_STRGADGET;
    string_gadget.SpecialInfo = &string_info;
    string_gadget.GadgetID = 5;
    
    /* Open a window with gadgets */
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

    print("Test 1: AddGadget inserts at head...\n");
    pos = AddGadget(window, &gadget1, 0);
    if (pos == 0 && window->FirstGadget == &gadget1 && gadget1.NextGadget == NULL) {
        print("  OK: AddGadget inserted first gadget at position 0\n");
    } else {
        print("  FAIL: AddGadget head insert returned wrong result\n");
        errors++;
    }
    print("\n");

    print("Test 2: AddGadget appends with position ~0...\n");
    gadget1.NextGadget = NULL;
    pos = AddGadget(window, &gadget2, (UWORD)~0);
    if (pos == 1 && window->FirstGadget == &gadget1 && gadget1.NextGadget == &gadget2 && gadget2.NextGadget == NULL) {
        print("  OK: AddGadget appended gadget at end\n");
    } else {
        print("  FAIL: AddGadget append returned wrong result\n");
        errors++;
    }
    print("\n");

    print("Test 3: AddGList appends a gadget chain and reports insertion position...\n");
    pos = AddGList(window, &gadget3, (UWORD)~0, -1, NULL);
    if (pos == 2 && gadget2.NextGadget == &gadget3 && gadget3.NextGadget == &gadget4 && gadget4.NextGadget == NULL) {
        print("  OK: AddGList appended both gadgets and returned position 2\n");
    } else {
        print("  FAIL: AddGList append semantics are wrong\n");
        errors++;
    }
    print("\n");

    print("Test 4: RemoveGadget returns the removed ordinal position...\n");
    removed = RemoveGadget(window, &gadget3);
    if (removed == 2 && gadget2.NextGadget == &gadget4) {
        print("  OK: RemoveGadget removed gadget3 at ordinal 2\n");
    } else {
        print("  FAIL: RemoveGadget returned wrong position or left bad links\n");
        errors++;
    }
    gadget3.NextGadget = &gadget4;
    gadget4.NextGadget = NULL;
    gadget2.NextGadget = NULL;
    print("\n");

    print("Test 5: AddGList inserts at a specific position...\n");
    pos = AddGList(window, &gadget3, 1, 2, NULL);
    if (pos == 1 && gadget1.NextGadget == &gadget3 && gadget3.NextGadget == &gadget4 && gadget4.NextGadget == &gadget2) {
        print("  OK: AddGList inserted the chain at ordinal 1\n");
    } else {
        print("  FAIL: AddGList position insert semantics are wrong\n");
        errors++;
    }
    print("\n");

    print("Test 6: RemoveGList returns the start ordinal position...\n");
    removed = RemoveGList(window, &gadget3, 2);
    if (removed == 1 && gadget1.NextGadget == &gadget2 && gadget2.NextGadget == NULL) {
        print("  OK: RemoveGList removed two gadgets starting at ordinal 1\n");
    } else {
        print("  FAIL: RemoveGList returned wrong position or left bad links\n");
        errors++;
    }
    print("\n");

    print("Test 7: RefreshGList and RefreshGadgets redraw without changing list links...\n");
    RefreshGList(&gadget1, window, NULL, -1);
    RefreshGList(&gadget2, window, NULL, 1);
    RefreshGadgets(window->FirstGadget, window, NULL);
    if (window->FirstGadget == &gadget1 && gadget1.NextGadget == &gadget2) {
        print("  OK: Refresh helpers preserved gadget list links\n");
    } else {
        print("  FAIL: Refresh helpers disturbed gadget list links\n");
        errors++;
    }
    print("\n");

    print("Test 8: OffGadget and OnGadget toggle the disabled flag...\n");
    OffGadget(&gadget1, window, NULL);
    if (!(gadget1.Flags & GFLG_DISABLED)) {
        print("  FAIL: OffGadget did not set GFLG_DISABLED\n");
        errors++;
    } else {
        OnGadget(&gadget1, window, NULL);
        if (gadget1.Flags & GFLG_DISABLED) {
            print("  FAIL: OnGadget did not clear GFLG_DISABLED\n");
            errors++;
        } else {
            print("  OK: OnGadget/OffGadget update GFLG_DISABLED\n");
        }
    }
    print("\n");

    print("Test 9: ActivateGadget ignores non-string boolean gadgets...\n");
    ActivateWindow(window);
    if (ActivateGadget(&gadget1, window, NULL)) {
        print("  FAIL: ActivateGadget unexpectedly activated a boolean gadget\n");
        errors++;
    } else {
        print("  OK: ActivateGadget rejects non-activatable boolean gadgets\n");
    }
    print("\n");

    print("Test 10: ActivateGadget activates string gadgets and recomputes NumChars...\n");
    pos = AddGadget(window, &string_gadget, (UWORD)~0);
    if (pos != 2) {
        print("  FAIL: Could not append string gadget for activation test\n");
        errors++;
    } else if (!ActivateGadget(&string_gadget, window, NULL)) {
        print("  FAIL: ActivateGadget rejected the string gadget\n");
        errors++;
    } else if (!(string_gadget.Flags & GFLG_SELECTED) || string_info.NumChars != 5) {
        print("  FAIL: ActivateGadget did not select the string gadget correctly\n");
        errors++;
    } else {
        print("  OK: ActivateGadget selected string gadget and updated NumChars\n");
    }
    print("\n");
    
    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    if (errors == 0) {
        print("PASS: gadget_refresh all tests completed\n");
        return 0;
    }

    print("FAIL: gadget_refresh had errors\n");
    return 20;
}
