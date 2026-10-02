/*
 * Test: workbench/appobjects
 * Tests the workbench.library V37 AppWindow/AppIcon/AppMenuItem API
 * (reference-validated against AmigaOS 3.1; the V44 entry points are covered
 * by workbench/appobjectsv44).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <utility/hooks.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <workbench/workbench.h>
#include <clib/alib_protos.h>
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

static int str_eq(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
}

static int list_contains_name(struct List *list, const char *name)
{
    struct Node *node;

    if (!list || !name)
        return 0;

    for (node = list->lh_Head; node && node->ln_Succ != NULL; node = node->ln_Succ)
    {
        if (node->ln_Name && str_eq(node->ln_Name, name))
            return 1;
    }

    return 0;
}

static ULONG hook_select_icon(register struct Hook *hook __asm("a0"),
                              register APTR object __asm("a2"),
                              register struct IconSelectMsg *ism __asm("a1"))
{
    ULONG *state = (ULONG *)hook->h_Data;

    (void)object;

    if (state)
        (*state)++;

    if (ism && ism->ism_Name && str_eq((const char *)ism->ism_Name, "AppObjects.info"))
        return ISMACTION_Select;

    return ISMACTION_Ignore;
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
    struct AppWindowDropZone *drop_zone = NULL;
    struct AppIcon *app_icon = NULL;
    struct AppMenuItem *app_menu = NULL;
    struct List *copied_list = NULL;
    LONG is_open = 0;
    ULONG default_stack = 0;
    ULONG global_flags = 0;
    ULONG type_restart = 0;
    char icon_name[64];
    LONG hit_left = -1;
    LONG hit_top = -1;
    ULONG hit_width = 0;
    ULONG hit_height = 0;
    ULONG hit_type = 0;
    ULONG hit_state = 0;
    ULONG is_fake = 0;
    ULONG is_link = 1;
    ULONG select_calls = 0;
    struct Hook select_hook;
    BPTR search_path_copy = 0;
    BPTR update_lock = 0;
    struct TagItem open_tags[3];
    struct TagItem drop_zone_tags[5];
    struct TagItem wbctrl_tags[5];
    struct TagItem which_tags[9];

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
