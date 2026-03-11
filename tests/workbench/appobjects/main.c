/*
 * Test: workbench/appobjects
 * Tests workbench.library compatibility entry points.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <workbench/workbench.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/icon_protos.h>
#include <clib/intuition_protos.h>
#include <clib/wb_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/icon.h>
#include <inline/intuition.h>
#include <inline/wb.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct IntuitionBase *IntuitionBase;
struct Library *IconBase;
struct Library *WorkbenchBase;

#define STR(s) ((CONST_STRPTR)(s))

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
    int errors = 0;
    struct MsgPort *port = NULL;
    struct Screen *screen = NULL;
    struct Window *window = NULL;
    struct DiskObject *icon = NULL;
    struct AppWindow *app_window = NULL;
    struct AppWindow *app_window_two = NULL;
    struct AppIcon *app_icon = NULL;
    struct AppMenuItem *app_menu = NULL;

    struct NewScreen ns = {
        0, 0, 320, 200, 2,
        0, 1,
        0,
        CUSTOMSCREEN,
        NULL,
        (UBYTE *)"Workbench Test Screen",
        NULL,
        NULL
    };

    struct NewWindow nw = {
        10, 10, 180, 60,
        0, 1,
        IDCMP_CLOSEWINDOW,
        WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE,
        NULL,
        NULL,
        (UBYTE *)"Workbench Test Window",
        NULL,
        NULL,
        50, 30, 300, 180,
        CUSTOMSCREEN
    };

    print("Testing workbench.library compatibility functions...\n");

    WorkbenchBase = (struct Library *)OpenLibrary(STR("workbench.library"), 0);
    IconBase = (struct Library *)OpenLibrary(STR("icon.library"), 0);
    if (!WorkbenchBase || !IconBase)
    {
        print("FAIL: Could not open workbench.library or icon.library\n");
        if (IconBase)
            CloseLibrary(IconBase);
        if (WorkbenchBase)
            CloseLibrary(WorkbenchBase);
        return 20;
    }

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: CreateMsgPort failed\n");
        CloseLibrary(IconBase);
        CloseLibrary(WorkbenchBase);
        return 20;
    }

    screen = OpenScreen(&ns);
    if (!screen)
    {
        print("FAIL: OpenScreen failed\n");
        DeleteMsgPort(port);
        CloseLibrary(IconBase);
        CloseLibrary(WorkbenchBase);
        return 20;
    }

    nw.Screen = screen;
    window = OpenWindow(&nw);
    if (!window)
    {
        print("FAIL: OpenWindow failed\n");
        CloseScreen(screen);
        DeleteMsgPort(port);
        CloseLibrary(IconBase);
        CloseLibrary(WorkbenchBase);
        return 20;
    }

    icon = GetDefDiskObject(WBTOOL);
    if (!icon)
    {
        print("FAIL: GetDefDiskObject failed\n");
        CloseWindow(window);
        CloseScreen(screen);
        DeleteMsgPort(port);
        CloseLibrary(IconBase);
        CloseLibrary(WorkbenchBase);
        return 20;
    }

    print("Test 1: AppWindow/AppIcon/AppMenuItem lifecycle...\n");
    app_window = AddAppWindowA(1, 0x1111, window, port, NULL);
    app_window_two = AddAppWindowA(2, 0x2222, window, port, NULL);
    app_icon = AddAppIconA(3, 0x3333, STR("Phase78T"), port, (BPTR)0, icon, NULL);
    app_menu = AddAppMenuItemA(4, 0x4444, STR("Phase78T Menu"), port, NULL);

    if (!app_window || !app_window_two || !app_icon || !app_menu)
    {
        print("FAIL: AddApp* returned NULL\n");
        errors++;
    }
    else if (app_window == app_window_two)
    {
        print("FAIL: AddAppWindowA reused the same handle\n");
        errors++;
    }
    else
    {
        print("OK: AddApp* returns allocated opaque handles\n");
    }

    if (!RemoveAppWindow(app_window) || !RemoveAppWindow(app_window_two) ||
        !RemoveAppIcon(app_icon) || !RemoveAppMenuItem(app_menu))
    {
        print("FAIL: RemoveApp* failed for valid handles\n");
        errors++;
    }
    else
    {
        print("OK: RemoveApp* releases valid handles\n");
    }

    if (RemoveAppWindow(app_window) || RemoveAppIcon(app_icon) || RemoveAppMenuItem(app_menu))
    {
        print("FAIL: RemoveApp* accepted stale handles\n");
        errors++;
    }
    else
    {
        print("OK: stale App* handles are rejected\n");
    }

    app_window = AddAppWindowA(5, 0x5555, window, port, NULL);
    if (!app_window)
    {
        print("FAIL: AddAppWindowA failed after earlier removals\n");
        errors++;
    }
    else if (!RemoveAppWindow(app_window))
    {
        print("FAIL: recycled AppWindow handle was not removable\n");
        errors++;
    }
    else
    {
        print("OK: AppWindow bookkeeping stays reusable after removals\n");
    }

    print("Test 2: Unsupported compatibility helpers stay well-defined...\n");
    if (WBInfo((BPTR)0, STR("SYS:"), screen) != 0)
    {
        print("FAIL: WBInfo should report unavailable requester\n");
        errors++;
    }
    else
    {
        print("OK: WBInfo reports unavailable requester\n");
    }

    if (OpenWorkbenchObjectA(STR("SYS:"), NULL) ||
        CloseWorkbenchObjectA(STR("SYS:"), NULL) ||
        ChangeWorkbenchSelectionA(STR("SYS:"), NULL, NULL) ||
        MakeWorkbenchObjectVisibleA(STR("SYS:"), NULL))
    {
        print("FAIL: unsupported Workbench object helpers should return FALSE\n");
        errors++;
    }
    else
    {
        print("OK: unsupported Workbench object helpers return FALSE\n");
    }

    if (WhichWorkbenchObjectA(window, 5, 5, NULL) != WBO_NONE)
    {
        print("FAIL: WhichWorkbenchObjectA should report WBO_NONE\n");
        errors++;
    }
    else
    {
        print("OK: WhichWorkbenchObjectA reports WBO_NONE\n");
    }

    FreeDiskObject(icon);
    CloseWindow(window);
    CloseScreen(screen);
    DeleteMsgPort(port);
    CloseLibrary(IconBase);
    CloseLibrary(WorkbenchBase);

    if (errors == 0)
    {
        print("PASS: workbench/appobjects all tests passed\n");
        return 0;
    }

    print("FAIL: workbench/appobjects had errors\n");
    return 20;
}
