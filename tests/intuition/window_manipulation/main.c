/*
 * Test: intuition/window_manipulation
 * Tests window geometry, z-order, menu lending, and helper wrappers.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
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

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    do
    {
        *(--p) = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    if (neg) *(--p) = '-';
    print(p);
}

static BOOL expect_message(struct MsgPort *port, ULONG expected_class, UWORD expected_code)
{
    struct IntuiMessage *msg;

    if (!port)
        return FALSE;

    msg = (struct IntuiMessage *)GetMsg(port);
    if (!msg)
        return FALSE;

    if (msg->Class != expected_class || msg->Code != expected_code)
    {
        ReplyMsg((struct Message *)msg);
        return FALSE;
    }

    ReplyMsg((struct Message *)msg);
    return TRUE;
}


/*
 * Phase 220: on AmigaOS, MoveWindow(), SizeWindow(), ChangeWindowBox(),
 * WindowToBack() & co. are deferred to the input handler task.  Following
 * the RKRM, wait for the IDCMP_CHANGEWINDOW message before looking at the
 * new geometry.  Every message received on the way is printed, so the
 * exact message sequence is compared against the reference.
 */
static void print_msg(struct IntuiMessage *msg)
{
    switch (msg->Class)
    {
        case IDCMP_CHANGEWINDOW:
            print(msg->Code == CWCODE_DEPTH ? "  msg CHANGEWINDOW depth\n" : "  msg CHANGEWINDOW movesize\n");
            break;
        case IDCMP_NEWSIZE:        print("  msg NEWSIZE\n"); break;
        case IDCMP_ACTIVEWINDOW:   print("  msg ACTIVEWINDOW\n"); break;
        case IDCMP_INACTIVEWINDOW: print("  msg INACTIVEWINDOW\n"); break;
        case IDCMP_REFRESHWINDOW:  print("  msg REFRESHWINDOW\n"); break;
        default:
            print("  msg class ");
            print_num((LONG)msg->Class);
            print("\n");
            break;
    }
}

/* Wait (max. ~1 s) until a message of class wanted_class arrives on the
 * window's port, then collect what follows within two more ticks. */
static BOOL sync_window(struct Window *window, ULONG wanted_class)
{
    struct IntuiMessage *msg;
    BOOL seen = FALSE;
    int ticks = 0;

    while (ticks < 50)
    {
        while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
        {
            if (msg->Class == IDCMP_REFRESHWINDOW)
            {
                BeginRefresh(window);
                EndRefresh(window, TRUE);
            }
            print_msg(msg);
            if (msg->Class == wanted_class)
                seen = TRUE;
            ReplyMsg((struct Message *)msg);
        }
        if (seen)
            break;
        Delay(1);
        ticks++;
    }
    if (!seen)
        return FALSE;
    Delay(2);
    while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
    {
        if (msg->Class == IDCMP_REFRESHWINDOW)
        {
            BeginRefresh(window);
            EndRefresh(window, TRUE);
        }
        print_msg(msg);
        ReplyMsg((struct Message *)msg);
    }
    return TRUE;
}

static void print_box(struct Window *window)
{
    print("  box ");
    print_num(window->LeftEdge);
    print(",");
    print_num(window->TopEdge);
    print(" ");
    print_num(window->Width);
    print("x");
    print_num(window->Height);
    print("\n");
}

/* TRUE if layer a is in front of layer b */
static BOOL in_front_of(struct Window *a, struct Window *b)
{
    struct Layer *l;

    for (l = a->WLayer; l; l = l->back)
        if (l == b->WLayer)
            return TRUE;
    return FALSE;
}

static BOOL seed_refresh_damage(struct Window *window)
{
    struct Rectangle rect;

    if (!window || !window->WLayer)
        return FALSE;

    if (!window->WLayer->DamageList)
    {
        window->WLayer->DamageList = NewRegion();
        if (!window->WLayer->DamageList)
            return FALSE;
    }

    rect.MinX = window->LeftEdge;
    rect.MinY = window->TopEdge;
    rect.MaxX = window->LeftEdge + 15;
    rect.MaxY = window->TopEdge + 15;
    OrRectRegion(window->WLayer->DamageList, &rect);
    window->WLayer->Flags |= LAYERREFRESH;
    return TRUE;
}

static void drain_port(struct MsgPort *port)
{
    struct Message *msg;

    if (!port)
        return;

    while ((msg = GetMsg(port)) != NULL)
        ReplyMsg(msg);
}

int main(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    struct Screen *screen;
    struct Window *window;
    struct Gadget relative_gadget;
    struct Border relative_border;
    WORD relative_xy[] = {0, 0, 19, 0, 19, 9, 0, 9, 0, 0};
    int errors = 0;
    
    print("Testing Window Manipulation Functions...\n\n");
    
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
    print("OK: Screen opened\n\n");
    
    /* Open a window */
    nw.LeftEdge = 100;
    nw.TopEdge = 50;
    nw.Width = 300;
    nw.Height = 200;
    nw.DetailPen = 0;
    nw.BlockPen = 1;
    nw.IDCMPFlags = IDCMP_CHANGEWINDOW | IDCMP_NEWSIZE;
    nw.Flags = WFLG_DRAGBAR | WFLG_ACTIVATE | WFLG_SIZEGADGET | WFLG_SIMPLE_REFRESH;
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
    print("OK: Window opened at position/size: ");
    print_num(window->LeftEdge);
    print(",");
    print_num(window->TopEdge);
    print(" ");
    print_num(window->Width);
    print("x");
    print_num(window->Height);
    print("\n\n");
    
    /* Test 1: MoveWindow */
    print("Test 1: MoveWindow(50, 30)...\n");
    MoveWindow(window, 50, 30);
    if (!sync_window(window, IDCMP_CHANGEWINDOW)) {
        print("  FAIL: MoveWindow did not emit IDCMP_CHANGEWINDOW\n\n");
        errors++;
    } else if (window->LeftEdge != 150 || window->TopEdge != 80) {
        print_box(window);
        print("  FAIL: MoveWindow did not update position\n\n");
        errors++;
    } else {
        print("  OK: MoveWindow updates geometry and posts IDCMP_CHANGEWINDOW\n\n");
    }

    /* Test 2: SizeWindow */
    print("Test 2: SizeWindow(50, 40)...\n");
    SizeWindow(window, 50, 40);
    if (!sync_window(window, IDCMP_CHANGEWINDOW)) {
        print("  FAIL: SizeWindow did not emit IDCMP_CHANGEWINDOW\n\n");
        errors++;
    } else if (window->Width != 350 || window->Height != 240) {
        print_box(window);
        print("  FAIL: SizeWindow did not update size\n\n");
        errors++;
    } else {
        print("  OK: SizeWindow updates geometry and posts size/change messages\n\n");
    }

    /* Test 3: WindowLimits */
    print("Test 3: WindowLimits(150, 100, 400, 300)...\n");
    if (WindowLimits(window, 150, 100, 400, 300)) {
        if (window->MinWidth != 150 || window->MinHeight != 100 ||
            window->MaxWidth != 400 || window->MaxHeight != 300) {
            print("  FAIL: WindowLimits did not store new limits\n");
            errors++;
        } else {
            print("  OK: WindowLimits returned TRUE and stored limits\n");
        }
    } else {
        print("  FAIL: WindowLimits returned FALSE\n");
        errors++;
    }
    print("\n");

    print("Test 3b: SizeWindow(200, 200) and WindowLimits...\n");
    SizeWindow(window, 200, 200);
    if (!sync_window(window, IDCMP_CHANGEWINDOW))
        print("  no IDCMP_CHANGEWINDOW\n");
    print_box(window);
    print("\n");

    /* Test 4: ChangeWindowBox */
    print("Test 4: ChangeWindowBox(50, 40, 250, 180)...\n");
    ChangeWindowBox(window, 50, 40, 250, 180);
    if (!sync_window(window, IDCMP_CHANGEWINDOW)) {
        print("  FAIL: ChangeWindowBox did not emit IDCMP_CHANGEWINDOW\n\n");
        errors++;
    } else if (window->LeftEdge != 50 || window->TopEdge != 40 ||
        window->Width != 250 || window->Height != 180) {
        print_box(window);
        print("  FAIL: ChangeWindowBox did not apply full box\n\n");
        errors++;
    } else {
        print("  OK: ChangeWindowBox updates the box\n\n");
    }

    print("Test 4b: SetWindowTitles() semantics...\n");
    SetWindowTitles(window, (UBYTE *)"Renamed Window", (UBYTE *)"Renamed Screen");
    if (!window->Title || !window->WScreen || !window->WScreen->Title ||
        window->Title[0] != 'R' || window->WScreen->Title[0] != 'R') {
        print("  FAIL: SetWindowTitles did not update both titles\n\n");
        errors++;
    } else {
        SetWindowTitles(window, (UBYTE *)-1, (UBYTE *)"");
        if (window->Title[0] != 'R' || window->WScreen->Title[0] != '\0') {
            print("  FAIL: SetWindowTitles did not honor no-change/blank semantics\n\n");
            errors++;
        } else {
            print("  OK: SetWindowTitles handles update, no-change, and blank semantics\n\n");
        }
    }

    print("Test 4c: WindowToBack()/WindowToFront() maintain z-order...\n");
    {
        struct NewWindow helper = nw;
        struct Window *other;

        helper.LeftEdge = 20;
        helper.TopEdge = 20;
        helper.Width = 180;
        helper.Height = 120;
        helper.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET;
        helper.IDCMPFlags = IDCMP_CHANGEWINDOW;
        helper.Title = (UBYTE *)"Other Window";

        other = OpenWindow(&helper);
        if (!other) {
            print("  FAIL: Could not open second window for z-order test\n\n");
            errors++;
        } else {
            sync_window(other, IDCMP_CHANGEWINDOW);   /* nothing expected */
            WindowToBack(other);
            sync_window(other, IDCMP_CHANGEWINDOW);
            if (!in_front_of(window, other)) {
                print("  FAIL: WindowToBack did not move other window behind\n\n");
                errors++;
            } else {
                WindowToFront(other);
                sync_window(other, IDCMP_CHANGEWINDOW);
                if (!in_front_of(other, window)) {
                    print("  FAIL: WindowToFront did not restore frontmost window\n\n");
                    errors++;
                } else {
                    print("  OK: WindowToBack/WindowToFront update the z-order\n\n");
                }
            }

            CloseWindow(other);
        }
    }

    print("Test 4c2: MoveWindowInFrontOf() reorders windows...\n");
    {
        struct NewWindow helper = nw;
        struct Window *middle;
        struct Window *back;

        helper.LeftEdge = 10;
        helper.TopEdge = 10;
        helper.Width = 140;
        helper.Height = 90;
        helper.Flags = WFLG_DRAGBAR;
        helper.IDCMPFlags = IDCMP_CHANGEWINDOW;
        helper.Title = (UBYTE *)"Middle";
        middle = OpenWindow(&helper);

        helper.LeftEdge = 40;
        helper.TopEdge = 40;
        helper.Title = (UBYTE *)"Back";
        back = OpenWindow(&helper);

        if (!middle || !back) {
            print("  FAIL: Could not open helper windows for MoveWindowInFrontOf\n\n");
            errors++;
            if (back) CloseWindow(back);
            if (middle) CloseWindow(middle);
        } else {
            drain_port(window->UserPort);
            MoveWindowInFrontOf(window, middle);
            sync_window(window, IDCMP_CHANGEWINDOW);
            if (!in_front_of(back, window) || !in_front_of(window, middle)) {
                print("  FAIL: MoveWindowInFrontOf did not splice the window before the target\n\n");
                errors++;
            } else {
                print("  OK: MoveWindowInFrontOf reorders depth relative to another window\n\n");
            }

            WindowToFront(window);
            sync_window(window, IDCMP_CHANGEWINDOW);
            CloseWindow(back);
            CloseWindow(middle);
            drain_port(window->UserPort);
        }
    }

    print("Test 4c3: LendMenus() lends menu activation...\n");
    {
        struct NewWindow helper = nw;
        struct Window *menu_window;
        struct MenuItem item;
        struct Menu menu;

        memset(&item, 0, sizeof(item));
        memset(&menu, 0, sizeof(menu));
        item.LeftEdge = 0;
        item.TopEdge = 0;
        item.Width = 40;
        item.Height = 10;
        menu.MenuName = (UBYTE *)"Project";
        menu.FirstItem = &item;
        helper.LeftEdge = 60;
        helper.TopEdge = 60;
        helper.Width = 160;
        helper.Height = 100;
        helper.Flags = WFLG_DRAGBAR;
        helper.IDCMPFlags = 0;
        helper.Title = (UBYTE *)"Menu Borrower";

        menu_window = OpenWindow(&helper);
        if (!menu_window) {
            print("  FAIL: Could not open helper window for LendMenus\n\n");
            errors++;
        } else if (!SetMenuStrip(menu_window, &menu)) {
            print("  FAIL: Could not attach source menu strip for LendMenus\n\n");
            errors++;
            CloseWindow(menu_window);
        } else {
            /* The lending is private Intuition state: neither window's
             * MenuStrip field changes (AmigaOS 3.1 reference). */
            LendMenus(window, menu_window);
            if (window->MenuStrip != NULL || menu_window->MenuStrip != &menu) {
                print("  FAIL: LendMenus changed a MenuStrip field\n\n");
                errors++;
            } else {
                LendMenus(window, NULL);
                if (window->MenuStrip != NULL) {
                    print("  FAIL: LendMenus(NULL) changed the MenuStrip field\n\n");
                    errors++;
                } else {
                    print("  OK: LendMenus keeps the MenuStrip fields\n\n");
                }
            }
            ClearMenuStrip(menu_window);
            CloseWindow(menu_window);
        }
    }

    print("Test 4d: BeginRefresh()/EndRefresh() toggle refresh state...\n");
    if (!seed_refresh_damage(window)) {
        print("  FAIL: Could not seed layer damage for refresh test\n\n");
        errors++;
    } else {
        BeginRefresh(window);
        if (!(window->Flags & WFLG_WINDOWREFRESH)) {
            print("  FAIL: BeginRefresh did not set WFLG_WINDOWREFRESH\n\n");
            errors++;
        } else {
            EndRefresh(window, FALSE);
            if (window->Flags & WFLG_WINDOWREFRESH) {
                print("  FAIL: EndRefresh(FALSE) did not clear WFLG_WINDOWREFRESH\n\n");
                errors++;
            } else if (!(window->WLayer->Flags & LAYERREFRESH)) {
                print("  FAIL: EndRefresh(FALSE) cleared LAYERREFRESH too early\n\n");
                errors++;
            } else {
                BeginRefresh(window);
                EndRefresh(window, TRUE);
                if (window->WLayer->Flags & LAYERREFRESH) {
                    print("  FAIL: EndRefresh(TRUE) did not clear LAYERREFRESH\n\n");
                    errors++;
                } else {
                    print("  OK: BeginRefresh/EndRefresh manage window and layer refresh flags\n\n");
                }
            }
        }
    }

    print("Test 4e: ActivateWindow() switches focus and posts IDCMP events...\n");
    {
        struct NewWindow helper = nw;
        struct Window *other;

        helper.LeftEdge = 30;
        helper.TopEdge = 30;
        helper.Width = 180;
        helper.Height = 120;
        helper.IDCMPFlags = IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW;
        helper.Flags = WFLG_DRAGBAR;
        helper.Title = (UBYTE *)"Inactive Helper";

        ModifyIDCMP(window, IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_NEWSIZE);
        ActivateWindow(window);
        drain_port(window->UserPort);

        other = OpenWindow(&helper);
        if (!other) {
            print("  FAIL: Could not open helper window for ActivateWindow test\n\n");
            errors++;
        } else {
            sync_window(other, IDCMP_ACTIVEWINDOW);
            drain_port(window->UserPort);
            ActivateWindow(other);
            sync_window(other, IDCMP_ACTIVEWINDOW);
            if (IntuitionBase->ActiveWindow != other ||
                (window->Flags & WFLG_WINDOWACTIVE) ||
                !(other->Flags & WFLG_WINDOWACTIVE)) {
                print("  FAIL: ActivateWindow did not switch the active window\n\n");
                errors++;
            } else if (!expect_message(window->UserPort, IDCMP_INACTIVEWINDOW, 0)) {
                print("  FAIL: ActivateWindow did not post IDCMP_INACTIVEWINDOW\n\n");
                errors++;
            } else {
                ActivateWindow(window);
                sync_window(window, IDCMP_ACTIVEWINDOW);
                if (IntuitionBase->ActiveWindow != window ||
                    !(window->Flags & WFLG_WINDOWACTIVE) ||
                    (other->Flags & WFLG_WINDOWACTIVE)) {
                    print("  FAIL: ActivateWindow did not restore the original window\n\n");
                    errors++;
                } else if (!expect_message(other->UserPort, IDCMP_INACTIVEWINDOW, 0)) {
                    print("  FAIL: ActivateWindow did not post the restore focus events\n\n");
                    errors++;
                } else {
                    print("  OK: ActivateWindow updates focus and IDCMP notifications\n\n");
                }
            }

            CloseWindow(other);
        }
    }

    print("Test 4f: RefreshWindowFrame() redraws the title bar...\n");
    {
        LONG gadget_x = window->LeftEdge + 8;
        LONG gadget_y = window->TopEdge + 5;

        SetAPen(&screen->RastPort, 3);
        RectFill(&screen->RastPort,
                 window->LeftEdge + 1,
                 window->TopEdge + 1,
                 window->LeftEdge + 15,
                 window->TopEdge + 8);

        if (ReadPixel(&screen->RastPort, gadget_x, gadget_y) != 3) {
            print("  FAIL: Could not corrupt the close gadget before refresh\n\n");
            errors++;
        } else {
            RefreshWindowFrame(window);
            if (ReadPixel(&screen->RastPort, gadget_x, gadget_y) == 3) {
                print("  FAIL: RefreshWindowFrame did not redraw the close gadget\n\n");
                errors++;
            } else {
                print("  OK: RefreshWindowFrame redraws the window frame\n\n");
            }
        }
    }

    print("Test 4g: SizeWindow repositions relative gadgets and clears old pixels...\n");
    memset(&relative_gadget, 0, sizeof(relative_gadget));
    memset(&relative_border, 0, sizeof(relative_border));
    relative_border.FrontPen = 1;
    relative_border.BackPen = 0;
    relative_border.DrawMode = JAM1;
    relative_border.Count = 5;
    relative_border.XY = relative_xy;

    relative_gadget.LeftEdge = -30;
    relative_gadget.TopEdge = -20;
    relative_gadget.Width = 20;
    relative_gadget.Height = 10;
    relative_gadget.Flags = GFLG_RELRIGHT | GFLG_RELBOTTOM;
    relative_gadget.GadgetType = GTYP_BOOLGADGET;
    relative_gadget.GadgetRender = &relative_border;
    relative_gadget.GadgetID = 99;

    AddGadget(window, &relative_gadget, (UWORD)~0);
    RefreshGadgets(&relative_gadget, window, NULL);
    {
        LONG old_x;
        LONG old_y;

        old_x = window->LeftEdge + relative_gadget.LeftEdge + window->Width - 1;
        old_y = window->TopEdge + relative_gadget.TopEdge + window->Height - 1;

        if (ReadPixel(&screen->RastPort, old_x, old_y) != 1) {
            print("  FAIL: Relative gadget did not render at its initial anchor\n\n");
            errors++;
        } else {
            SizeWindow(window, 20, 10);
            sync_window(window, IDCMP_CHANGEWINDOW);
            {
                LONG new_x;
                LONG new_y;

                new_x = window->LeftEdge + relative_gadget.LeftEdge + window->Width - 1;
                new_y = window->TopEdge + relative_gadget.TopEdge + window->Height - 1;

                print("  new anchor pen ");
                print_num(ReadPixel(&screen->RastPort, new_x, new_y));
                print(", old anchor pen ");
                print_num(ReadPixel(&screen->RastPort, old_x, old_y));
                print("\n\n");
            }
        }
    }
    RemoveGadget(window, &relative_gadget);

    /* Test 5: ZipWindow */
    print("Test 5: ZipWindow()...\n");
    ZipWindow(window);
    if (!sync_window(window, IDCMP_CHANGEWINDOW))
        print("  no IDCMP_CHANGEWINDOW\n");
    print_box(window);
    ZipWindow(window);
    if (!sync_window(window, IDCMP_CHANGEWINDOW))
        print("  no IDCMP_CHANGEWINDOW\n");
    print_box(window);
    print("\n");

    /* Cleanup */
    CloseWindow(window);
    print("OK: Window closed\n");
    CloseScreen(screen);
    print("OK: Screen closed\n\n");
    
    if (errors == 0) {
        print("PASS: window_manipulation all tests completed\n");
        return 0;
    }

    print("FAIL: window_manipulation had errors\n");
    return 20;
}
