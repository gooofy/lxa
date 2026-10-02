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
    struct DiskObject *saved_def_tool;
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
    /* Test 4 replaces the default tool icon: keep the system's one */
    saved_def_tool = GetDiskObject(STR("ENV:Sys/def_tool"));
    DeleteDiskObject(STR("ENV:Sys/def_tool"));
    DeleteFile(STR("T:phase78t_tool"));
    DeleteFile(STR("T:phase78t_project"));
    lock = Lock(STR("T:phase78t_drawer"), ACCESS_READ);
    if (lock)
    {
        UnLock(lock);
        DeleteFile(STR("T:phase78t_drawer"));
    }

    /* No do_ToolWindow here: AmigaOS 3.1's icon.library writes it, but
     * GetDiskObject() then fails to read the icon back (reference-verified). */
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
    dobj->do_StackSize = 8192;
    dobj->do_Gadget.Width = 37;
    dobj->do_Gadget.Height = 21;
    dobj->do_ToolTypes = tool_types;

    if (!tool_types[0] || !tool_types[1] || !dobj->do_DefaultTool)
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

    if (loaded)
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
        if (loaded)
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
    /* CreateDir() returns an exclusive lock: release it, or the drawer
     * cannot be examined (AmigaOS 3.1). */
    lock = CreateDir(STR("T:phase78t_drawer"));
    if (!lock)
    {
        print("FAIL: could not create drawer directory\n");
        errors++;
    }
    else
    {
        UnLock(lock);
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
    if (loaded)
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
    if (loaded)
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
    if (loaded)
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
        if (loaded)
            FreeDiskObject(loaded);
    }

    DeleteFile(STR("T:phase78t_tool"));
    DeleteFile(STR("T:phase78t_project"));
    lock = Lock(STR("T:phase78t_drawer"), ACCESS_READ);
    if (lock)
    {
        UnLock(lock);
        DeleteFile(STR("T:phase78t_drawer"));
    }

    if (saved_def_tool)
    {
        PutDiskObject(STR("ENV:Sys/def_tool"), saved_def_tool);
        FreeDiskObject(saved_def_tool);
    }
    else
    {
        DeleteDiskObject(STR("ENV:Sys/def_tool"));
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
