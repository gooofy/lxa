/*
 * Test: icon/diskobject
 * Tests icon.library DiskObject read/write and fallback behavior.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/screens.h>
#include <utility/tagitem.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/icon_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/icon.h>
#include <inline/intuition.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;
struct Library *IconBase;

#ifndef ICONA_ErrorCode
#define ICONA_ErrorCode              (TAG_USER + 0x9001)
#define ICONCTRLA_SetFrameless       (TAG_USER + 0x9020)
#define ICONCTRLA_GetFrameless       (TAG_USER + 0x9021)
#define ICONCTRLA_GetWidth           (TAG_USER + 0x9027)
#define ICONCTRLA_HasRealImage2      (TAG_USER + 0x902c)
#define ICONA_ErrorTagItem           (TAG_USER + 0x904b)
#endif

#define STR(s) ((CONST_STRPTR)(s))

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static BOOL str_equal(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        if (*s1++ != *s2++)
            return FALSE;
    }

    return *s1 == *s2;
}

static BOOL write_file(CONST_STRPTR name)
{
    BPTR fh = Open(name, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    Close(fh);
    return TRUE;
}

static STRPTR dup_string(const char *src)
{
    LONG len = 0;
    STRPTR copy;

    while (src[len] != '\0')
        len++;

    copy = (STRPTR)AllocMem(len + 1, MEMF_CLEAR);
    if (!copy)
        return NULL;

    CopyMem((APTR)src, copy, len);
    copy[len] = '\0';
    return copy;
}

static struct Image *alloc_icon_image(UWORD width, UWORD height, UWORD depth, UWORD pattern)
{
    struct Image *image;
    LONG words;
    LONG bytes;
    UWORD *data;
    LONG i;

    image = (struct Image *)AllocMem(sizeof(struct Image), MEMF_CLEAR);
    if (!image)
        return NULL;

    words = ((width + 15) / 16) * height * depth;
    bytes = words * 2;
    data = (UWORD *)AllocMem(bytes, MEMF_CHIP | MEMF_CLEAR);
    if (!data)
    {
        FreeMem(image, sizeof(struct Image));
        return NULL;
    }

    for (i = 0; i < words; i++)
        data[i] = pattern;

    image->Width = width;
    image->Height = height;
    image->Depth = depth;
    image->ImageData = data;
    image->PlanePick = 1;
    image->PlaneOnOff = 0;
    return image;
}

int main(void)
{
    int errors = 0;
    struct DiskObject *dobj;
    struct DiskObject *loaded;
    struct DiskObject *selected_icon;
    BPTR lock;
    STRPTR *tool_types;
    struct ColorRegister color;
    struct TagItem iconcontrol_tags[5];
    struct TagItem *bad_tag = NULL;
    LONG icon_error = 0;
    LONG width_value = 0;
    ULONG frameless_value = 0;
    LONG has_real_image2 = 0;
    struct Rectangle rect;
    struct BitMap bitmap;
    struct RastPort rp;
    PLANEPTR plane = NULL;
    struct Screen *screen = NULL;
    struct Window *window = NULL;
    struct DrawInfo *draw_info = NULL;
    struct NewScreen ns = {
        0, 0, 320, 200, 2,
        0, 1,
        0,
        CUSTOMSCREEN,
        NULL,
        (UBYTE *)"Icon Test Screen",
        NULL,
        NULL
    };
    struct NewWindow nw = {
        10, 10, 180, 90,
        0, 1,
        IDCMP_CLOSEWINDOW,
        WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_ACTIVATE,
        NULL,
        NULL,
        (UBYTE *)"Icon Test Window",
        NULL,
        NULL,
        50, 40, 280, 180,
        CUSTOMSCREEN
    };

    print("Testing icon.library DiskObject functions...\n");

    IconBase = (struct Library *)OpenLibrary(STR("icon.library"), 0);
    if (!IconBase)
    {
        print("FAIL: Could not open icon.library\n");
        return 20;
    }

    DeleteDiskObject(STR("T:phase78t_icon_roundtrip"));
    DeleteDiskObject(STR("ENV:Sys/def_tool"));
    DeleteFile(STR("T:phase78t_tool"));
    DeleteFile(STR("T:phase78t_project"));
    lock = Lock(STR("T:phase78t_drawer"), ACCESS_READ);
    if (lock)
    {
        UnLock(lock);
        DeleteFile(STR("T:phase78t_drawer"));
    }

    print("Test 1: PutDiskObject/GetDiskObject round-trip...\n");
    dobj = GetDefDiskObject(WBTOOL);
    if (!dobj)
    {
        print("FAIL: GetDefDiskObject returned NULL\n");
        CloseLibrary(IconBase);
        return 20;
    }

    tool_types = (STRPTR *)AllocMem(3 * sizeof(STRPTR), MEMF_CLEAR);
    if (!tool_types)
    {
        print("FAIL: Could not allocate ToolTypes array\n");
        FreeDiskObject(dobj);
        CloseLibrary(IconBase);
        return 20;
    }

    tool_types[0] = dup_string("PUBSCREEN=Workbench");
    tool_types[1] = dup_string("DONOTWAIT");
    tool_types[2] = NULL;
    dobj->do_DefaultTool = dup_string("SYS:Utilities/Clock");
    dobj->do_ToolWindow = dup_string("CON:0/0/200/50/Icon Test");
    dobj->do_StackSize = 8192;
    dobj->do_Gadget.Width = 37;
    dobj->do_Gadget.Height = 21;
    dobj->do_ToolTypes = tool_types;

    if (!tool_types[0] || !tool_types[1] || !dobj->do_DefaultTool || !dobj->do_ToolWindow)
    {
        print("FAIL: Could not allocate DiskObject strings\n");
        FreeDiskObject(dobj);
        CloseLibrary(IconBase);
        return 20;
    }

    if (!PutDiskObject(STR("T:phase78t_icon_roundtrip"), dobj))
    {
        print("FAIL: PutDiskObject failed\n");
        FreeDiskObject(dobj);
        CloseLibrary(IconBase);
        return 20;
    }
    print("OK: PutDiskObject succeeded\n");
    FreeDiskObject(dobj);

    loaded = GetDiskObject(STR("T:phase78t_icon_roundtrip"));
    if (!loaded)
    {
        print("FAIL: GetDiskObject failed\n");
        CloseLibrary(IconBase);
        return 20;
    }

    if (loaded->do_Magic != WB_DISKMAGIC || loaded->do_Version != WB_DISKVERSION)
    {
        print("FAIL: DiskObject header mismatch\n");
        errors++;
    }
    else
    {
        print("OK: DiskObject header round-trips\n");
    }

    if (loaded->do_Type != WBTOOL || loaded->do_StackSize != 8192)
    {
        print("FAIL: DiskObject type/stack mismatch\n");
        errors++;
    }
    else
    {
        print("OK: DiskObject type and stack round-trip\n");
    }

    if (!loaded->do_DefaultTool || !str_equal((const char *)loaded->do_DefaultTool, "SYS:Utilities/Clock"))
    {
        print("FAIL: do_DefaultTool mismatch\n");
        errors++;
    }
    else
    {
        print("OK: do_DefaultTool round-trips\n");
    }

    if (!loaded->do_ToolWindow || !str_equal((const char *)loaded->do_ToolWindow, "CON:0/0/200/50/Icon Test"))
    {
        print("FAIL: do_ToolWindow mismatch\n");
        errors++;
    }
    else
    {
        print("OK: do_ToolWindow round-trips\n");
    }

    if (!loaded->do_ToolTypes || !loaded->do_ToolTypes[0] || !loaded->do_ToolTypes[1] || loaded->do_ToolTypes[2] != NULL)
    {
        print("FAIL: do_ToolTypes layout mismatch\n");
        errors++;
    }
    else if (!str_equal((const char *)loaded->do_ToolTypes[0], "PUBSCREEN=Workbench") ||
             !str_equal((const char *)loaded->do_ToolTypes[1], "DONOTWAIT"))
    {
        print("FAIL: do_ToolTypes contents mismatch\n");
        errors++;
    }
    else
    {
        print("OK: do_ToolTypes round-trip\n");
    }

    FreeDiskObject(loaded);

    if (!DeleteDiskObject(STR("T:phase78t_icon_roundtrip")))
    {
        print("FAIL: DeleteDiskObject failed\n");
        errors++;
    }
    else
    {
        print("OK: DeleteDiskObject removed .info file\n");
    }

    print("Test 2: GetDiskObject(NULL)...\n");
    loaded = GetDiskObject(NULL);
    if (!loaded)
    {
        print("FAIL: GetDiskObject(NULL) returned NULL\n");
        errors++;
    }
    else
    {
        print("OK: GetDiskObject(NULL) allocates a blank object\n");
        FreeDiskObject(loaded);
    }

    print("Test 3: GetDiskObjectNew fallback for drawers/tools/projects...\n");
    if (!write_file(STR("T:phase78t_tool")))
    {
        print("FAIL: could not create tool file\n");
        errors++;
    }
    if (!write_file(STR("T:phase78t_project")))
    {
        print("FAIL: could not create project file\n");
        errors++;
    }
    if (!CreateDir(STR("T:phase78t_drawer")))
    {
        print("FAIL: could not create drawer directory\n");
        errors++;
    }
    if (!SetProtection(STR("T:phase78t_tool"), 0))
    {
        print("FAIL: SetProtection for tool file failed\n");
        errors++;
    }
    if (!SetProtection(STR("T:phase78t_project"), FIBF_EXECUTE))
    {
        print("FAIL: SetProtection for project file failed\n");
        errors++;
    }

    loaded = GetDiskObjectNew(STR("T:phase78t_tool"));
    if (!loaded || loaded->do_Type != WBTOOL)
    {
        print("FAIL: GetDiskObjectNew did not classify tool file as WBTOOL\n");
        errors++;
    }
    else
    {
        print("OK: executable fallback yields WBTOOL\n");
    }
    FreeDiskObject(loaded);

    loaded = GetDiskObjectNew(STR("T:phase78t_project"));
    if (!loaded || loaded->do_Type != WBPROJECT)
    {
        print("FAIL: GetDiskObjectNew did not classify project file as WBPROJECT\n");
        errors++;
    }
    else
    {
        print("OK: non-executable fallback yields WBPROJECT\n");
    }
    FreeDiskObject(loaded);

    loaded = GetDiskObjectNew(STR("T:phase78t_drawer"));
    if (!loaded || loaded->do_Type != WBDRAWER)
    {
        print("FAIL: GetDiskObjectNew did not classify drawer as WBDRAWER\n");
        errors++;
    }
    else
    {
        print("OK: drawer fallback yields WBDRAWER\n");
    }
    FreeDiskObject(loaded);

    print("Test 4: PutDefDiskObject/GetDefDiskObject round-trip...\n");
    dobj = GetDefDiskObject(WBTOOL);
    if (!dobj)
    {
        print("FAIL: GetDefDiskObject for default round-trip returned NULL\n");
        errors++;
    }
    else
    {
        dobj->do_StackSize = 12345;
        dobj->do_Gadget.Width = 29;
        dobj->do_Gadget.Height = 17;
        if (!PutDefDiskObject(dobj))
        {
            print("FAIL: PutDefDiskObject failed\n");
            errors++;
        }
        else
        {
            print("OK: PutDefDiskObject succeeded\n");
        }
        FreeDiskObject(dobj);

        loaded = GetDefDiskObject(WBTOOL);
        if (!loaded)
        {
            print("FAIL: GetDefDiskObject could not read saved default icon\n");
            errors++;
        }
        else if (loaded->do_StackSize != 12345 || loaded->do_Gadget.Width != 29 || loaded->do_Gadget.Height != 17)
        {
            print("FAIL: default icon contents did not round-trip\n");
            errors++;
        }
        else
        {
            print("OK: default icon contents round-trip\n");
        }
        FreeDiskObject(loaded);
    }

    print("Test 5: IconControlA / LayoutIconA / GetIconRectangleA / DrawIconStateA...\n");
    selected_icon = GetDefDiskObject(WBTOOL);
    if (!selected_icon)
    {
        print("FAIL: could not allocate icon for control/layout tests\n");
        errors++;
    }
    else
    {
        selected_icon->do_Gadget.Width = 8;
        selected_icon->do_Gadget.Height = 8;
        selected_icon->do_Gadget.GadgetRender = (APTR)alloc_icon_image(8, 8, 1, 0xF0F0);
        selected_icon->do_Gadget.SelectRender = (APTR)alloc_icon_image(8, 8, 1, 0xFFFF);
        if (!selected_icon->do_Gadget.GadgetRender || !selected_icon->do_Gadget.SelectRender)
        {
            print("FAIL: could not allocate icon images\n");
            errors++;
        }
        else
        {
            iconcontrol_tags[0].ti_Tag = ICONCTRLA_SetFrameless;
            iconcontrol_tags[0].ti_Data = TRUE;
            iconcontrol_tags[1].ti_Tag = ICONCTRLA_GetFrameless;
            iconcontrol_tags[1].ti_Data = (ULONG)&frameless_value;
            iconcontrol_tags[2].ti_Tag = ICONCTRLA_GetWidth;
            iconcontrol_tags[2].ti_Data = (ULONG)&width_value;
            iconcontrol_tags[3].ti_Tag = ICONCTRLA_HasRealImage2;
            iconcontrol_tags[3].ti_Data = (ULONG)&has_real_image2;
            iconcontrol_tags[4].ti_Tag = TAG_DONE;
            iconcontrol_tags[4].ti_Data = 0;

            if (IconControlA(selected_icon, iconcontrol_tags) != 4 || !frameless_value || width_value != 8 || !has_real_image2)
            {
                print("FAIL: IconControlA did not report expected values\n");
                errors++;
            }
            else
            {
                print("OK: IconControlA applies and reports supported tags\n");
            }

            iconcontrol_tags[0].ti_Tag = ICONA_ErrorCode;
            iconcontrol_tags[0].ti_Data = (ULONG)&icon_error;
            iconcontrol_tags[1].ti_Tag = ICONA_ErrorTagItem;
            iconcontrol_tags[1].ti_Data = (ULONG)&bad_tag;
            iconcontrol_tags[2].ti_Tag = TAG_USER + 0x1234;
            iconcontrol_tags[2].ti_Data = 1;
            iconcontrol_tags[3].ti_Tag = TAG_DONE;
            iconcontrol_tags[3].ti_Data = 0;
            if (IconControlA(selected_icon, iconcontrol_tags) != 0 || icon_error == 0 || bad_tag != &iconcontrol_tags[2])
            {
                print("FAIL: IconControlA did not surface unsupported tag errors\n");
                errors++;
            }
            else
            {
                print("OK: IconControlA reports unsupported tags\n");
            }

            screen = OpenScreen(&ns);
            if (!screen)
            {
                print("FAIL: OpenScreen failed for icon layout test\n");
                errors++;
            }
            else
            {
                nw.Screen = screen;
                window = OpenWindow(&nw);
                draw_info = GetScreenDrawInfo(screen);
                if (!window || !draw_info)
                {
                    print("FAIL: OpenWindow/GetScreenDrawInfo failed for icon layout test\n");
                    errors++;
                }
                else
                {
                    if (!LayoutIconA(selected_icon, screen, NULL))
                    {
                        print("FAIL: LayoutIconA failed\n");
                        errors++;
                    }
                    else if (!GetIconRectangleA(window->RPort, selected_icon, STR("ICON"), &rect, NULL))
                    {
                        print("FAIL: GetIconRectangleA failed\n");
                        errors++;
                    }
                    else if ((rect.MaxX - rect.MinX + 1) < 32 || (rect.MaxY - rect.MinY + 1) < 18)
                    {
                        print("FAIL: GetIconRectangleA returned an unexpectedly small box\n");
                        errors++;
                    }
                    else
                    {
                        print("OK: LayoutIconA/GetIconRectangleA produce usable geometry\n");
                    }

                    DrawIconStateA(window->RPort, selected_icon, NULL, 20, 20, IDS_SELECTED, NULL);
                    if (ReadPixel(window->RPort, 22, 22) == 0)
                    {
                        print("FAIL: DrawIconStateA did not draw selected icon pixels\n");
                        errors++;
                    }
                    else
                    {
                        print("OK: DrawIconStateA renders selected icon state\n");
                    }
                }
            }

            if (draw_info)
                FreeScreenDrawInfo(screen, draw_info);
            if (window)
                CloseWindow(window);
            if (screen)
                CloseScreen(screen);
        }
        FreeDiskObject(selected_icon);
    }

    print("Test 6: ChangeToSelectedIconColor...\n");
    color.red = 10;
    color.green = 20;
    color.blue = 30;
    ChangeToSelectedIconColor(&color);
    if (color.red <= 10 || color.green <= 20 || color.blue <= 30)
    {
        print("FAIL: ChangeToSelectedIconColor did not brighten the color\n");
        errors++;
    }
    else
    {
        print("OK: ChangeToSelectedIconColor brightens the color\n");
    }

    print("Test 7: GetIconRectangleA with off-screen RastPort...\n");
    plane = AllocRaster(64, 32);
    if (!plane)
    {
        print("FAIL: AllocRaster failed for icon rectangle test\n");
        errors++;
    }
    else
    {
        InitBitMap(&bitmap, 1, 64, 32);
        bitmap.Planes[0] = plane;
        InitRastPort(&rp);
        rp.BitMap = &bitmap;

        loaded = GetDefDiskObject(WBPROJECT);
        if (!loaded)
        {
            print("FAIL: GetDefDiskObject(WBPROJECT) failed for rectangle probe\n");
            errors++;
        }
        else if (!GetIconRectangleA(&rp, loaded, STR("AB"), &rect, NULL))
        {
            print("FAIL: GetIconRectangleA failed on off-screen RastPort\n");
            errors++;
        }
        else if (rect.MaxX <= rect.MinX || rect.MaxY <= rect.MinY)
        {
            print("FAIL: GetIconRectangleA returned degenerate geometry\n");
            errors++;
        }
        else
        {
            print("OK: GetIconRectangleA works with off-screen RastPorts\n");
        }
        FreeDiskObject(loaded);
        FreeRaster(plane, 64, 32);
    }

    DeleteFile(STR("T:phase78t_tool"));
    DeleteFile(STR("T:phase78t_project"));
    lock = Lock(STR("T:phase78t_drawer"), ACCESS_READ);
    if (lock)
    {
        UnLock(lock);
        DeleteFile(STR("T:phase78t_drawer"));
    }

    CloseLibrary(IconBase);

    if (errors == 0)
    {
        print("PASS: icon/diskobject all tests passed\n");
        return 0;
    }

    print("FAIL: icon/diskobject had errors\n");
    return 20;
}
