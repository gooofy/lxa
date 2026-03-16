/*
 * Test: workbench/appobjects
 * Tests workbench.library compatibility entry points.
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

    print("Test 2: Phase 89 object helpers...\n");

    if (!OpenWorkbenchObjectA(STR("SYS:Tests/Workbench"), NULL))
    {
        print("FAIL: OpenWorkbenchObjectA should open drawers\n");
        errors++;
    }
    else
    {
        print("OK: OpenWorkbenchObjectA opens drawers\n");
    }

    open_tags[0].ti_Tag = WBOPENA_Show;
    open_tags[0].ti_Data = DDFLAGS_SHOWALL;
    open_tags[1].ti_Tag = WBOPENA_ViewBy;
    open_tags[1].ti_Data = DDVM_BYNAME;
    open_tags[2].ti_Tag = TAG_DONE;
    open_tags[2].ti_Data = 0;
    if (!OpenWorkbenchObjectA(STR("SYS:Tests/Exec/Lists"), open_tags))
    {
        print("FAIL: OpenWorkbenchObjectA should treat files as launchable objects\n");
        errors++;
    }
    else
    {
        print("OK: OpenWorkbenchObjectA launches files via Workbench semantics\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_GetProgramList;
    wbctrl_tags[0].ti_Data = (ULONG)&copied_list;
    wbctrl_tags[1].ti_Tag = TAG_DONE;
    wbctrl_tags[1].ti_Data = 0;
    copied_list = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) || !list_contains_name(copied_list, "SYS:Tests/Exec/Lists"))
    {
        print("FAIL: WorkbenchControlA should expose launched program paths\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA exposes launched program paths\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_FreeProgramList;
    wbctrl_tags[0].ti_Data = (ULONG)copied_list;
    wbctrl_tags[1].ti_Tag = TAG_DONE;
    wbctrl_tags[1].ti_Data = 0;
    copied_list = NULL;
    WorkbenchControlA(NULL, wbctrl_tags);

    wbctrl_tags[0].ti_Tag = WBCTRLA_IsOpen;
    wbctrl_tags[0].ti_Data = (ULONG)&is_open;
    wbctrl_tags[1].ti_Tag = WBCTRLA_GetDefaultStackSize;
    wbctrl_tags[1].ti_Data = (ULONG)&default_stack;
    wbctrl_tags[2].ti_Tag = WBCTRLA_GetOpenDrawerList;
    wbctrl_tags[2].ti_Data = (ULONG)&copied_list;
    wbctrl_tags[3].ti_Tag = TAG_DONE;
    wbctrl_tags[3].ti_Data = 0;
    if (!WorkbenchControlA(STR("SYS:Tests/Workbench"), wbctrl_tags) || !is_open || default_stack < 4096 || !copied_list)
    {
        print("FAIL: WorkbenchControlA should report open drawers and defaults\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA reports open state and drawer lists\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_FreeOpenDrawerList;
    wbctrl_tags[0].ti_Data = (ULONG)copied_list;
    wbctrl_tags[1].ti_Tag = WBCTRLA_SetDefaultStackSize;
    wbctrl_tags[1].ti_Data = 2048;
    wbctrl_tags[2].ti_Tag = WBCTRLA_GetDefaultStackSize;
    wbctrl_tags[2].ti_Data = (ULONG)&default_stack;
    wbctrl_tags[3].ti_Tag = WBCTRLA_SetGlobalFlags;
    wbctrl_tags[3].ti_Data = WBF_BOUNDTEXTVIEW;
    wbctrl_tags[4].ti_Tag = TAG_DONE;
    wbctrl_tags[4].ti_Data = 0;
    copied_list = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) || default_stack != 4096)
    {
        print("FAIL: WorkbenchControlA should clamp default stack size\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA clamps default stack size\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_GetGlobalFlags;
    wbctrl_tags[0].ti_Data = (ULONG)&global_flags;
    wbctrl_tags[1].ti_Tag = WBCTRLA_SetTypeRestartTime;
    wbctrl_tags[1].ti_Data = 9;
    wbctrl_tags[2].ti_Tag = WBCTRLA_GetTypeRestartTime;
    wbctrl_tags[2].ti_Data = (ULONG)&type_restart;
    wbctrl_tags[3].ti_Tag = WBCTRLA_DuplicateSearchPath;
    wbctrl_tags[3].ti_Data = (ULONG)&search_path_copy;
    wbctrl_tags[4].ti_Tag = TAG_DONE;
    wbctrl_tags[4].ti_Data = 0;
    if (!WorkbenchControlA(NULL, wbctrl_tags) || global_flags != WBF_BOUNDTEXTVIEW || type_restart != 9)
    {
        print("FAIL: WorkbenchControlA should retain global settings\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA retains configurable state\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_AddHiddenDeviceName;
    wbctrl_tags[0].ti_Data = (ULONG)STR("RAM:");
    wbctrl_tags[1].ti_Tag = WBCTRLA_GetHiddenDeviceList;
    wbctrl_tags[1].ti_Data = (ULONG)&copied_list;
    wbctrl_tags[2].ti_Tag = TAG_DONE;
    wbctrl_tags[2].ti_Data = 0;
    copied_list = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) || !list_contains_name(copied_list, "RAM:"))
    {
        print("FAIL: WorkbenchControlA should track hidden device names\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA tracks hidden device names\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_FreeHiddenDeviceList;
    wbctrl_tags[0].ti_Data = (ULONG)copied_list;
    wbctrl_tags[1].ti_Tag = WBCTRLA_RemoveHiddenDeviceName;
    wbctrl_tags[1].ti_Data = (ULONG)STR("RAM:");
    wbctrl_tags[2].ti_Tag = TAG_DONE;
    wbctrl_tags[2].ti_Data = 0;
    copied_list = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags))
    {
        print("FAIL: WorkbenchControlA should free and remove hidden device names\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA frees and removes hidden device names\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_SetCopyHook;
    wbctrl_tags[0].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[1].ti_Tag = WBCTRLA_GetCopyHook;
    wbctrl_tags[1].ti_Data = (ULONG)&select_hook.h_SubEntry;
    wbctrl_tags[2].ti_Tag = WBCTRLA_SetDeleteHook;
    wbctrl_tags[2].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[3].ti_Tag = WBCTRLA_GetDeleteHook;
    wbctrl_tags[3].ti_Data = (ULONG)&select_hook.h_Data;
    wbctrl_tags[4].ti_Tag = TAG_DONE;
    wbctrl_tags[4].ti_Data = 0;
    select_hook.h_SubEntry = NULL;
    select_hook.h_Data = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) ||
        select_hook.h_SubEntry != (APTR)&select_hook ||
        select_hook.h_Data != (APTR)&select_hook)
    {
        print("FAIL: WorkbenchControlA should round-trip copy/delete hooks\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA round-trips copy/delete hooks\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_SetTextInputHook;
    wbctrl_tags[0].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[1].ti_Tag = WBCTRLA_GetTextInputHook;
    wbctrl_tags[1].ti_Data = (ULONG)&select_hook.h_SubEntry;
    wbctrl_tags[2].ti_Tag = WBCTRLA_SetDiskInfoHook;
    wbctrl_tags[2].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[3].ti_Tag = WBCTRLA_GetDiskInfoHook;
    wbctrl_tags[3].ti_Data = (ULONG)&select_hook.h_Data;
    wbctrl_tags[4].ti_Tag = TAG_DONE;
    wbctrl_tags[4].ti_Data = 0;
    select_hook.h_SubEntry = NULL;
    select_hook.h_Data = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) ||
        select_hook.h_SubEntry != (APTR)&select_hook ||
        select_hook.h_Data != (APTR)&select_hook)
    {
        print("FAIL: WorkbenchControlA should round-trip text/disk hooks\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA round-trips text/disk hooks\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_AddSetupCleanupHook;
    wbctrl_tags[0].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[1].ti_Tag = WBCTRLA_RemSetupCleanupHook;
    wbctrl_tags[1].ti_Data = (ULONG)&select_hook;
    wbctrl_tags[2].ti_Tag = TAG_DONE;
    wbctrl_tags[2].ti_Data = 0;
    if (!WorkbenchControlA(NULL, wbctrl_tags))
    {
        print("FAIL: WorkbenchControlA should add and remove setup hooks\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA adds and removes setup hooks\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_FreeSearchPath;
    wbctrl_tags[0].ti_Data = (ULONG)search_path_copy;
    wbctrl_tags[1].ti_Tag = TAG_DONE;
    wbctrl_tags[1].ti_Data = 0;
    search_path_copy = 0;
    if (!WorkbenchControlA(NULL, wbctrl_tags))
    {
        print("FAIL: WorkbenchControlA should free duplicated search paths\n");
        errors++;
    }

    drop_zone_tags[0].ti_Tag = WBDZA_Left;
    drop_zone_tags[0].ti_Data = 4;
    drop_zone_tags[1].ti_Tag = WBDZA_Top;
    drop_zone_tags[1].ti_Data = 5;
    drop_zone_tags[2].ti_Tag = WBDZA_Width;
    drop_zone_tags[2].ti_Data = 20;
    drop_zone_tags[3].ti_Tag = WBDZA_Height;
    drop_zone_tags[3].ti_Data = 15;
    drop_zone_tags[4].ti_Tag = TAG_DONE;
    drop_zone_tags[4].ti_Data = 0;
    app_window = AddAppWindowA(6, 0x6666, window, port, NULL);
    drop_zone = AddAppWindowDropZoneA(app_window, 7, 0x7777, drop_zone_tags);
    if (!app_window || !drop_zone)
    {
        print("FAIL: AddAppWindowDropZoneA should accept valid geometry\n");
        errors++;
    }
    else
    {
        print("OK: AddAppWindowDropZoneA creates drop zones\n");
    }

    icon_name[0] = '\0';
    which_tags[0].ti_Tag = WBOBJA_Type;
    which_tags[0].ti_Data = (ULONG)&hit_type;
    which_tags[1].ti_Tag = WBOBJA_Left;
    which_tags[1].ti_Data = (ULONG)&hit_left;
    which_tags[2].ti_Tag = WBOBJA_Top;
    which_tags[2].ti_Data = (ULONG)&hit_top;
    which_tags[3].ti_Tag = WBOBJA_Width;
    which_tags[3].ti_Data = (ULONG)&hit_width;
    which_tags[4].ti_Tag = WBOBJA_Height;
    which_tags[4].ti_Data = (ULONG)&hit_height;
    which_tags[5].ti_Tag = WBOBJA_Name;
    which_tags[5].ti_Data = (ULONG)icon_name;
    which_tags[6].ti_Tag = WBOBJA_State;
    which_tags[6].ti_Data = (ULONG)&hit_state;
    which_tags[7].ti_Tag = WBOBJA_IsFake;
    which_tags[7].ti_Data = (ULONG)&is_fake;
    which_tags[8].ti_Tag = TAG_DONE;
    which_tags[8].ti_Data = 0;
    if (WhichWorkbenchObjectA(window, 8, 8, which_tags) != WBO_ICON ||
        hit_type != WBAPPICON || hit_left != 4 || hit_top != 5 ||
        hit_width != 20 || hit_height != 15 || hit_state != IDS_NORMAL || !is_fake)
    {
        print("FAIL: WhichWorkbenchObjectA should describe drop zones as icons\n");
        errors++;
    }
    else
    {
        print("OK: WhichWorkbenchObjectA reports drop zone icon hits\n");
    }

    if (WhichWorkbenchObjectA(window, 40, 20, NULL) != WBO_DRAWER)
    {
        print("FAIL: WhichWorkbenchObjectA should report drawer hits in empty window areas\n");
        errors++;
    }
    else
    {
        print("OK: WhichWorkbenchObjectA reports drawer hits\n");
    }

    select_hook.h_Entry = (ULONG (*)())hook_select_icon;
    select_hook.h_SubEntry = NULL;
    select_hook.h_Data = &select_calls;
    if (!ChangeWorkbenchSelectionA(STR("SYS:Tests/Workbench"), &select_hook, NULL) || select_calls <= 0)
    {
        print("FAIL: ChangeWorkbenchSelectionA should visit open drawer entries\n");
        errors++;
    }
    else
    {
        print("OK: ChangeWorkbenchSelectionA visits open drawer entries\n");
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_GetSelectedIconList;
    wbctrl_tags[0].ti_Data = (ULONG)&copied_list;
    wbctrl_tags[1].ti_Tag = TAG_DONE;
    wbctrl_tags[1].ti_Data = 0;
    copied_list = NULL;
    if (!WorkbenchControlA(NULL, wbctrl_tags) || !copied_list)
    {
        print("FAIL: WorkbenchControlA should return selected icon lists\n");
        errors++;
    }
    else
    {
        print("OK: WorkbenchControlA returns selected icon lists\n");
    }

    update_lock = Lock(STR("SYS:Tests/Workbench"), SHARED_LOCK);
    if (!update_lock)
    {
        print("FAIL: lock for UpdateWorkbench should succeed\n");
        errors++;
    }
    else
    {
        UpdateWorkbench(STR("AppObjects.info"), update_lock, UPDATEWB_ObjectRemoved);

        wbctrl_tags[0].ti_Tag = WBCTRLA_FreeSelectedIconList;
        wbctrl_tags[0].ti_Data = (ULONG)copied_list;
        wbctrl_tags[1].ti_Tag = WBCTRLA_GetSelectedIconList;
        wbctrl_tags[1].ti_Data = (ULONG)&copied_list;
        wbctrl_tags[2].ti_Tag = TAG_DONE;
        wbctrl_tags[2].ti_Data = 0;
        copied_list = NULL;
        if (!WorkbenchControlA(NULL, wbctrl_tags) || list_contains_name(copied_list, "SYS:Tests/Workbench/AppObjects.info"))
        {
            print("FAIL: UpdateWorkbench should remove deleted objects from selection state\n");
            errors++;
        }
        else
        {
            print("OK: UpdateWorkbench removes deleted objects from selection state\n");
        }

        wbctrl_tags[0].ti_Tag = WBCTRLA_FreeSelectedIconList;
        wbctrl_tags[0].ti_Data = (ULONG)copied_list;
        wbctrl_tags[1].ti_Tag = TAG_DONE;
        wbctrl_tags[1].ti_Data = 0;
        copied_list = NULL;
        WorkbenchControlA(NULL, wbctrl_tags);

        UpdateWorkbench(STR("AppObjects.info"), update_lock, UPDATEWB_ObjectAdded);
        UnLock(update_lock);
        update_lock = 0;
    }

    wbctrl_tags[0].ti_Tag = WBCTRLA_FreeSelectedIconList;
    wbctrl_tags[0].ti_Data = (ULONG)copied_list;
    wbctrl_tags[1].ti_Tag = TAG_DONE;
    wbctrl_tags[1].ti_Data = 0;
    copied_list = NULL;
    WorkbenchControlA(NULL, wbctrl_tags);

    if (!MakeWorkbenchObjectVisibleA(STR("SYS:Tests/Workbench/AppObjects"), NULL))
    {
        print("FAIL: MakeWorkbenchObjectVisibleA should succeed for entries in open drawers\n");
        errors++;
    }
    else
    {
        print("OK: MakeWorkbenchObjectVisibleA succeeds for open drawer entries\n");
    }

    if (!RemoveAppWindowDropZone(app_window, drop_zone))
    {
        print("FAIL: RemoveAppWindowDropZone should remove valid drop zones\n");
        errors++;
    }
    else
    {
        print("OK: RemoveAppWindowDropZone removes valid drop zones\n");
    }

    if (!RemoveAppWindow(app_window))
    {
        print("FAIL: RemoveAppWindow should still remove windows after drop zone teardown\n");
        errors++;
    }

    if (!CloseWorkbenchObjectA(STR("SYS:Tests/Workbench"), NULL))
    {
        print("FAIL: CloseWorkbenchObjectA should close opened drawers\n");
        errors++;
    }
    else
    {
        print("OK: CloseWorkbenchObjectA closes opened drawers\n");
    }

    if (!WBInfo((BPTR)0, STR("SYS:Tests/Workbench"), screen))
    {
        print("FAIL: WBInfo should build an information requester\n");
        errors++;
    }
    else
    {
        print("OK: WBInfo builds an information requester\n");
    }

    if (OpenWorkbenchObjectA(STR("SYS:MissingPhase89"), NULL) ||
        CloseWorkbenchObjectA(STR("SYS:MissingPhase89"), NULL) ||
        MakeWorkbenchObjectVisibleA(STR("SYS:MissingPhase89"), NULL) ||
        RemoveAppWindowDropZone(NULL, drop_zone) ||
        ChangeWorkbenchSelectionA(STR("SYS:Tests/Workbench"), NULL, NULL))
    {
        print("FAIL: invalid Phase 89 calls should fail cleanly\n");
        errors++;
    }
    else
    {
        print("OK: invalid Phase 89 calls fail cleanly\n");
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
