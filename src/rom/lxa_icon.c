/*
 * lxa icon.library implementation
 *
 * Provides icon management functions for reading/writing .info files
 * and accessing icon tool types.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <workbench/workbench.h>
#include <workbench/icon.h>

#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <clib/utility_protos.h>
#include <inline/utility.h>

#include <datatypes/pictureclass.h>

#include "util.h"

/* Helper: convert char to lowercase */
static char tolower_simple(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

/* Helper: case-insensitive string comparison for n chars */
static int strncasecmp_simple(const char *s1, const char *s2, ULONG n)
{
    while (n > 0 && *s1 && *s2)
    {
        char c1 = tolower_simple(*s1);
        char c2 = tolower_simple(*s2);
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return tolower_simple(*s1) - tolower_simple(*s2);
}

/* Helper: compare n chars */
static int strncmp_simple(const char *s1, const char *s2, ULONG n)
{
    while (n > 0 && *s1 && *s2)
    {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *s1 - *s2;
}

#define VERSION    44
#define REVISION   1
#define EXLIBNAME  "icon"
#define EXLIBVER   " 44.1 (2025/02/01)"

char __aligned _g_icon_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_icon_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_icon_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_icon_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;
extern struct DosLibrary    *DOSBase;
extern struct GfxBase       *GfxBase;
extern struct UtilityBase   *UtilityBase;
extern struct IntuitionBase *IntuitionBase;

/* IconBase structure - minimal for now */
struct IconPrivateState
{
    struct Node       node;
    struct DiskObject *icon;
    struct Screen     *screen;
    ULONG              flags;
};

#define ICON_PRIVATE_FRAMELESS 0x00000001UL

struct IconBase {
    struct Library lib;
    UWORD          Pad;
    BPTR           SegList;
    struct List    PrivateStates;
    struct Screen *GlobalScreen;
    LONG           GlobalPrecision;
    struct Rectangle GlobalEmbossRect;
    BOOL           GlobalFrameless;
    BOOL           GlobalNewIconsSupport;
    BOOL           GlobalColorIconSupport;
    struct Hook   *GlobalIdentifyHook;
    LONG           GlobalMaxNameLength;
};

struct IconLayoutInfo
{
    WORD image_width;
    WORD image_height;
    WORD label_width;
    WORD label_height;
    WORD content_width;
    WORD total_width;
    WORD total_height;
    WORD image_x;
    WORD image_y;
    WORD label_x;
    WORD label_y;
    BOOL frameless;
    BOOL borderless;
};

static BOOL icon_is_valid_type(LONG type)
{
    return (type >= WBDISK && type <= WBAPPICON);
}

static CONST_STRPTR icon_default_type_name(LONG type)
{
    switch (type)
    {
        case WBDISK:    return (CONST_STRPTR)"def_disk";
        case WBDRAWER:  return (CONST_STRPTR)"def_drawer";
        case WBTOOL:    return (CONST_STRPTR)"def_tool";
        case WBPROJECT: return (CONST_STRPTR)"def_project";
        case WBGARBAGE: return (CONST_STRPTR)"def_garbage";
        case WBDEVICE:  return (CONST_STRPTR)"def_device";
        case WBKICK:    return (CONST_STRPTR)"def_kick";
        case WBAPPICON: return (CONST_STRPTR)"def_appicon";
    }

    return NULL;
}

static BOOL icon_build_default_path(LONG type, UBYTE *buffer, ULONG size)
{
    CONST_STRPTR type_name = icon_default_type_name(type);

    if (!type_name || !buffer || size < 16)
        return FALSE;

    strcpy((char *)buffer, "ENV:Sys/");
    strcat((char *)buffer, (const char *)type_name);
    return TRUE;
}

static void icon_ensure_default_directory(void)
{
    BPTR lock = Lock((CONST_STRPTR)"ENV:Sys", ACCESS_READ);

    if (lock)
    {
        UnLock(lock);
        return;
    }

    CreateDir((CONST_STRPTR)"ENV:Sys");
}

static struct IconPrivateState *icon_find_private_state(struct IconBase *IconBase,
                                                        CONST struct DiskObject *icon)
{
    struct Node *node;

    if (!IconBase || !icon)
        return NULL;

    for (node = IconBase->PrivateStates.lh_Head; node && node->ln_Succ; node = node->ln_Succ)
    {
        struct IconPrivateState *state = (struct IconPrivateState *)node;

        if (state->icon == icon)
            return state;
    }

    return NULL;
}

static struct IconPrivateState *icon_ensure_private_state(struct IconBase *IconBase,
                                                          struct DiskObject *icon)
{
    struct IconPrivateState *state;

    if (!IconBase || !icon)
        return NULL;

    state = icon_find_private_state(IconBase, icon);
    if (state)
        return state;

    state = AllocMem(sizeof(*state), MEMF_CLEAR | MEMF_PUBLIC);
    if (!state)
        return NULL;

    state->icon = icon;
    AddTail(&IconBase->PrivateStates, &state->node);
    return state;
}

static void icon_remove_private_state(struct IconBase *IconBase,
                                      CONST struct DiskObject *icon)
{
    struct IconPrivateState *state = icon_find_private_state(IconBase, icon);

    if (!state)
        return;

    Remove(&state->node);
    FreeMem(state, sizeof(*state));
}

static struct RastPort *icon_select_text_rastport(struct RastPort *rp,
                                                  struct Screen *screen,
                                                  struct RastPort *fallback)
{
    if (rp)
        return rp;

    if (screen)
        return &screen->RastPort;

    return fallback;
}

static BOOL icon_tag_enabled(CONST struct TagItem *tags, Tag tag, BOOL fallback)
{
    struct TagItem *item = tags ? FindTagItem(tag, (struct TagItem *)tags) : NULL;

    if (!item)
        return fallback;

    return item->ti_Data ? TRUE : FALSE;
}

static struct Screen *icon_select_screen(struct IconBase *IconBase,
                                         CONST struct DiskObject *icon)
{
    struct IconPrivateState *state = icon_find_private_state(IconBase, icon);

    if (state && state->screen)
        return state->screen;

    return IconBase ? IconBase->GlobalScreen : NULL;
}

static BOOL icon_is_frameless(struct IconBase *IconBase,
                              CONST struct DiskObject *icon,
                              CONST struct TagItem *tags)
{
    BOOL fallback = IconBase ? IconBase->GlobalFrameless : FALSE;
    struct IconPrivateState *state = icon_find_private_state(IconBase, icon);

    if (state && (state->flags & ICON_PRIVATE_FRAMELESS) != 0)
        fallback = TRUE;

    return icon_tag_enabled(tags, ICONDRAWA_Frameless, fallback);
}

static struct Image *icon_select_image(CONST struct DiskObject *icon, ULONG state)
{
    if (!icon)
        return NULL;

    if (state == IDS_SELECTED && icon->do_Gadget.SelectRender)
        return (struct Image *)icon->do_Gadget.SelectRender;

    return (struct Image *)icon->do_Gadget.GadgetRender;
}

static void icon_compute_layout(struct IconBase *IconBase,
                                struct RastPort *rp,
                                CONST struct DiskObject *icon,
                                CONST_STRPTR label,
                                CONST struct TagItem *tags,
                                struct IconLayoutInfo *layout)
{
    struct RastPort temp_rp;
    struct RastPort *text_rp;
    struct Screen *screen;
    WORD pad;

    memset(layout, 0, sizeof(*layout));

    if (!icon)
        return;

    screen = icon_select_screen(IconBase, icon);
    InitRastPort(&temp_rp);
    text_rp = icon_select_text_rastport(rp, screen, &temp_rp);

    layout->frameless = icon_is_frameless(IconBase, icon, tags);
    layout->borderless = icon_tag_enabled(tags, ICONDRAWA_Borderless, FALSE);

    pad = (layout->frameless || layout->borderless) ? 0 : 2;

    layout->image_width = icon->do_Gadget.Width;
    layout->image_height = icon->do_Gadget.Height;
    if (layout->image_width < 1)
        layout->image_width = 1;
    if (layout->image_height < 1)
        layout->image_height = 1;

    if (label)
    {
        layout->label_width = TextLength(text_rp, label, strlen((const char *)label));
        layout->label_height = text_rp->TxHeight ? text_rp->TxHeight : 8;
    }

    layout->content_width = layout->image_width;
    if (layout->label_width > layout->content_width)
        layout->content_width = layout->label_width;

    layout->total_width = layout->content_width + (pad * 2);
    layout->total_height = layout->image_height + (pad * 2);
    if (label)
        layout->total_height += 2 + layout->label_height;

    layout->image_x = pad + ((layout->content_width - layout->image_width) / 2);
    layout->image_y = pad;
    layout->label_x = pad + ((layout->content_width - layout->label_width) / 2);
    layout->label_y = pad + layout->image_height + 2;
}

static struct DrawInfo *icon_acquire_draw_info(struct IconBase *IconBase,
                                               CONST struct DiskObject *icon,
                                               CONST struct TagItem *tags,
                                               BOOL *must_free)
{
    struct DrawInfo *draw_info = (struct DrawInfo *)GetTagData(ICONDRAWA_DrawInfo, 0, (struct TagItem *)tags);
    struct Screen *screen;

    *must_free = FALSE;
    if (draw_info)
        return draw_info;

    screen = icon_select_screen(IconBase, icon);
    if (!screen)
        return NULL;

    draw_info = GetScreenDrawInfo(screen);
    if (draw_info)
        *must_free = TRUE;

    return draw_info;
}

static void icon_release_draw_info(CONST struct DiskObject *icon,
                                   struct DrawInfo *draw_info,
                                   BOOL must_free,
                                   struct IconBase *IconBase)
{
    struct Screen *screen;

    if (!must_free || !draw_info)
        return;

    screen = icon_select_screen(IconBase, icon);
    if (screen)
        FreeScreenDrawInfo(screen, draw_info);
}

static void icon_draw_frame(struct RastPort *rp,
                            WORD left,
                            WORD top,
                            WORD width,
                            WORD height,
                            CONST struct DrawInfo *draw_info,
                            BOOL selected)
{
    UBYTE saved_pen;
    UBYTE shine = draw_info ? (UBYTE)draw_info->dri_Pens[SHINEPEN] : 2;
    UBYTE shadow = draw_info ? (UBYTE)draw_info->dri_Pens[SHADOWPEN] : 1;
    UBYTE fill = draw_info ? (UBYTE)draw_info->dri_Pens[BACKGROUNDPEN] : 0;

    if (!rp || width < 2 || height < 2)
        return;

    saved_pen = rp->FgPen;

    SetAPen(rp, fill);
    RectFill(rp, left, top, left + width - 1, top + height - 1);

    SetAPen(rp, selected ? shadow : shine);
    Move(rp, left, top + height - 1);
    Draw(rp, left, top);
    Draw(rp, left + width - 1, top);

    SetAPen(rp, selected ? shine : shadow);
    Draw(rp, left + width - 1, top + height - 1);
    Draw(rp, left, top + height - 1);

    SetAPen(rp, saved_pen);
}

struct DiskObject * _icon_GetDefDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                             register LONG              type    __asm("d0"));

/* 
 * .info file format (simplified):
 * - DiskObject header (do_Magic, do_Version)
 * - Gadget structure
 * - Type byte
 * - Optional DrawerData
 * - Optional Image data
 * - DefaultTool string (if any)
 * - ToolTypes array (if any)
 */

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct IconBase * __g_lxa_icon_InitLib ( register struct IconBase *iconb    __asm("d0"),
                                          register BPTR             seglist __asm("a0"),
                                          register struct ExecBase *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_icon: InitLib() called\n");

    NEWLIST(&iconb->PrivateStates);
    iconb->SegList = seglist;
    iconb->GlobalScreen = NULL;
    iconb->GlobalPrecision = PRECISION_ICON;
    iconb->GlobalEmbossRect.MinX = 0;
    iconb->GlobalEmbossRect.MinY = 0;
    iconb->GlobalEmbossRect.MaxX = 0;
    iconb->GlobalEmbossRect.MaxY = 0;
    iconb->GlobalFrameless = FALSE;
    iconb->GlobalNewIconsSupport = FALSE;
    iconb->GlobalColorIconSupport = FALSE;
    iconb->GlobalIdentifyHook = NULL;
    iconb->GlobalMaxNameLength = 30;
    return iconb;
}

struct IconBase * __g_lxa_icon_OpenLib ( register struct IconBase *IconBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_icon: OpenLib() called\n");
    IconBase->lib.lib_OpenCnt++;
    IconBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return IconBase;
}

BPTR __g_lxa_icon_CloseLib ( register struct IconBase *iconb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_icon: CloseLib() called\n");
    iconb->lib.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_icon_ExpungeLib ( register struct IconBase *iconb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_icon_ExtFuncLib(void)
{
    return 0;
}

/****************************************************************************/
/* FreeList functions                                                        */
/****************************************************************************/

VOID _icon_FreeFreeList ( register struct IconBase *IconBase __asm("a6"),
                          register struct FreeList *freelist __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: FreeFreeList() called freelist=0x%08lx\n", freelist);
    
    if (!freelist)
        return;
    
    /* Free all memory chunks in the list */
    struct Node *node, *next;
    for (node = freelist->fl_MemList.lh_Head; node->ln_Succ; node = next)
    {
        next = node->ln_Succ;
        /* The node itself is part of the allocated memory */
        FreeMem(node, sizeof(struct Node) + (ULONG)node->ln_Name);
    }
    
    freelist->fl_NumFree = 0;
    NEWLIST(&freelist->fl_MemList);
}

BOOL _icon_AddFreeList ( register struct IconBase *IconBase __asm("a6"),
                         register struct FreeList *freelist __asm("a0"),
                         register CONST_APTR       mem      __asm("a1"),
                         register ULONG            size     __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_icon: AddFreeList() called freelist=0x%08lx mem=0x%08lx size=%ld\n", 
             freelist, mem, size);
    
    if (!freelist || !mem)
        return FALSE;
    
    /* We track the memory for later freeing */
    /* Store size in ln_Name field (abuse, but common practice) */
    struct Node *node = (struct Node *)mem;
    node->ln_Name = (char *)(size - sizeof(struct Node));
    
    AddTail(&freelist->fl_MemList, node);
    freelist->fl_NumFree++;
    
    return TRUE;
}

APTR _icon_FreeAlloc ( register struct IconBase *IconBase __asm("a6"),
                       register struct FreeList *freelist __asm("a0"),
                       register ULONG            len      __asm("d0"),
                       register ULONG            type     __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_icon: FreeAlloc() called len=%ld type=0x%08lx\n", len, type);
    
    if (!freelist)
        return NULL;
    
    /* Allocate memory and add to freelist */
    ULONG allocSize = len + sizeof(struct Node);
    APTR mem = AllocMem(allocSize, type);
    
    if (mem)
    {
        struct Node *node = (struct Node *)mem;
        node->ln_Name = (char *)len;
        AddTail(&freelist->fl_MemList, node);
        freelist->fl_NumFree++;
        return (APTR)((UBYTE *)mem + sizeof(struct Node));
    }
    
    return NULL;
}

VOID _icon_FreeFree ( register struct IconBase *IconBase __asm("a6"),
                      register struct FreeList *fl       __asm("a0"),
                      register APTR             address  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: FreeFree() called\n");
    
    if (!fl || !address)
        return;
    
    /* Find and remove the node from the freelist, then free it */
    struct Node *node = (struct Node *)((UBYTE *)address - sizeof(struct Node));
    Remove(node);
    fl->fl_NumFree--;
    FreeMem(node, sizeof(struct Node) + (ULONG)node->ln_Name);
}

/****************************************************************************/
/* Icon reading functions                                                    */
/****************************************************************************/

/* Helper to read a BCPL string from file */
static STRPTR ReadBCPLString(BPTR fh)
{
    ULONG len;
    if (Read(fh, &len, 4) != 4)
        return NULL;
    
    if (len == 0)
        return NULL;
    
    /* BCPL strings store length in first longword, actual string follows */
    STRPTR str = AllocMem(len + 1, MEMF_CLEAR);
    if (!str)
        return NULL;
    
    if (Read(fh, str, len) != (LONG)len)
    {
        FreeMem(str, len + 1);
        return NULL;
    }
    
    str[len] = '\0';
    return str;
}

static LONG GuessDiskObjectType(CONST_STRPTR name)
{
    struct FileInfoBlock fib;
    BPTR lock;

    if (!name)
        return WBPROJECT;

    lock = Lock(name, ACCESS_READ);
    if (!lock)
        return WBPROJECT;

    if (!Examine(lock, &fib))
    {
        UnLock(lock);
        return WBPROJECT;
    }

    UnLock(lock);

    if (fib.fib_DirEntryType > 0)
        return WBDRAWER;

    if ((fib.fib_Protection & FIBF_EXECUTE) == 0)
        return WBTOOL;

    return WBPROJECT;
}

struct DiskObject * _icon_GetDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                          register CONST_STRPTR     name     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() called name='%s'\n", name ? (char*)name : "(null)");
    
    if (!name)
        return AllocMem(sizeof(struct DiskObject), MEMF_CLEAR | MEMF_PUBLIC);
    
    /* Build .info filename */
    LONG nameLen = strlen((const char *)name);
    STRPTR infoName = AllocMem(nameLen + 6, MEMF_CLEAR);
    if (!infoName)
        return NULL;
    
    strcpy((char *)infoName, (const char *)name);
    strcat((char *)infoName, ".info");
    
    /* Open the .info file */
    BPTR fh = Open(infoName, MODE_OLDFILE);
    FreeMem(infoName, nameLen + 6);
    
    if (!fh)
    {
        DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() - file not found\n");
        return NULL;
    }
    
    /* Allocate DiskObject */
    struct DiskObject *dobj = AllocMem(sizeof(struct DiskObject), MEMF_CLEAR | MEMF_PUBLIC);
    if (!dobj)
    {
        Close(fh);
        return NULL;
    }
    
    /* Read DiskObject header */
    if (Read(fh, dobj, sizeof(struct DiskObject)) != sizeof(struct DiskObject))
    {
        FreeMem(dobj, sizeof(struct DiskObject));
        Close(fh);
        return NULL;
    }
    
    /* Verify magic number */
    if (dobj->do_Magic != WB_DISKMAGIC)
    {
        DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() - bad magic 0x%04x\n", dobj->do_Magic);
        FreeMem(dobj, sizeof(struct DiskObject));
        Close(fh);
        return NULL;
    }
    
    DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() - type=%d, magic=0x%04x, version=%d\n",
             dobj->do_Type, dobj->do_Magic, dobj->do_Version);
    
    /* Read DrawerData if present */
    if (dobj->do_DrawerData)
    {
        struct DrawerData *dd = AllocMem(sizeof(struct DrawerData), MEMF_CLEAR | MEMF_PUBLIC);
        if (dd)
        {
            if (Read(fh, dd, sizeof(struct OldDrawerData)) != sizeof(struct OldDrawerData))
            {
                FreeMem(dd, sizeof(struct DrawerData));
                dd = NULL;
            }
        }
        dobj->do_DrawerData = dd;
    }
    
    /* Read first image if present */
    struct Image *img1 = NULL;
    if (dobj->do_Gadget.GadgetRender)
    {
        img1 = AllocMem(sizeof(struct Image), MEMF_CLEAR | MEMF_PUBLIC);
        if (img1)
        {
            if (Read(fh, img1, sizeof(struct Image)) == sizeof(struct Image))
            {
                /* Read image data */
                LONG words = ((img1->Width + 15) / 16) * img1->Height * img1->Depth;
                LONG bytes = words * 2;
                if (bytes > 0)
                {
                    img1->ImageData = AllocMem(bytes, MEMF_CHIP | MEMF_CLEAR);
                    if (img1->ImageData)
                    {
                        Read(fh, img1->ImageData, bytes);
                    }
                }
                dobj->do_Gadget.GadgetRender = img1;
            }
            else
            {
                FreeMem(img1, sizeof(struct Image));
                img1 = NULL;
                dobj->do_Gadget.GadgetRender = NULL;
            }
        }
    }
    
    /* Read second image (select) if present */
    struct Image *img2 = NULL;
    if (dobj->do_Gadget.SelectRender)
    {
        img2 = AllocMem(sizeof(struct Image), MEMF_CLEAR | MEMF_PUBLIC);
        if (img2)
        {
            if (Read(fh, img2, sizeof(struct Image)) == sizeof(struct Image))
            {
                /* Read image data */
                LONG words = ((img2->Width + 15) / 16) * img2->Height * img2->Depth;
                LONG bytes = words * 2;
                if (bytes > 0)
                {
                    img2->ImageData = AllocMem(bytes, MEMF_CHIP | MEMF_CLEAR);
                    if (img2->ImageData)
                    {
                        Read(fh, img2->ImageData, bytes);
                    }
                }
                dobj->do_Gadget.SelectRender = img2;
            }
            else
            {
                FreeMem(img2, sizeof(struct Image));
                img2 = NULL;
                dobj->do_Gadget.SelectRender = NULL;
            }
        }
    }
    
    /* Read DefaultTool if present */
    if (dobj->do_DefaultTool)
    {
        dobj->do_DefaultTool = ReadBCPLString(fh);
    }
    
    /* Read ToolTypes if present */
    if (dobj->do_ToolTypes)
    {
        ULONG numTools;
        if (Read(fh, &numTools, 4) == 4 && numTools > 0)
        {
            /* Allocate array + NULL terminator */
            STRPTR *toolTypes = AllocMem((numTools + 1) * sizeof(STRPTR), MEMF_CLEAR | MEMF_PUBLIC);
            if (toolTypes)
            {
                ULONG i;
                for (i = 0; i < numTools; i++)
                {
                    toolTypes[i] = ReadBCPLString(fh);
                }
                toolTypes[numTools] = NULL;
                dobj->do_ToolTypes = toolTypes;
            }
            else
            {
                dobj->do_ToolTypes = NULL;
            }
        }
        else
        {
            dobj->do_ToolTypes = NULL;
        }
    }
    
    /* Read ToolWindow if present */
    if (dobj->do_ToolWindow)
    {
        dobj->do_ToolWindow = ReadBCPLString(fh);
    }
    
    Close(fh);
    
    DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() - success, dobj=0x%08lx\n", dobj);
    return dobj;
}

struct DiskObject * _icon_GetDiskObjectNew ( register struct IconBase *IconBase __asm("a6"),
                                             register CONST_STRPTR     name     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: GetDiskObjectNew() called name='%s'\n", STRORNULL(name));
    
    /* First try to get the actual icon */
    struct DiskObject *dobj = _icon_GetDiskObject(IconBase, name);
    
    if (!dobj)
    {
        LONG type = GuessDiskObjectType(name);

        DPRINTF (LOG_DEBUG, "_icon: GetDiskObjectNew() - using default type=%ld\n", type);
        dobj = _icon_GetDefDiskObject(IconBase, type);
    }
    
    return dobj;
}

struct DiskObject * _icon_GetDefDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                             register LONG              type    __asm("d0"))
{
    UBYTE default_name[64];
    struct DiskObject *saved;

    DPRINTF (LOG_DEBUG, "_icon: GetDefDiskObject() called type=%ld\n", type);

    if (!icon_is_valid_type(type))
    {
        SetIoErr(ERROR_BAD_NUMBER);
        return NULL;
    }

    if (icon_build_default_path(type, default_name, sizeof(default_name)))
    {
        saved = _icon_GetDiskObject(IconBase, (CONST_STRPTR)default_name);
        if (saved)
        {
            SetIoErr(0);
            return saved;
        }
    }
    
    /* Allocate a default DiskObject */
    struct DiskObject *dobj = AllocMem(sizeof(struct DiskObject), MEMF_CLEAR | MEMF_PUBLIC);
    if (!dobj)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    
    /* Set up basic defaults */
    dobj->do_Magic = WB_DISKMAGIC;
    dobj->do_Version = WB_DISKVERSION;
    dobj->do_Type = type;
    dobj->do_CurrentX = NO_ICON_POSITION;
    dobj->do_CurrentY = NO_ICON_POSITION;
    dobj->do_StackSize = 4096;
    
    /* Set gadget defaults */
    dobj->do_Gadget.Width = 40;
    dobj->do_Gadget.Height = 40;

    SetIoErr(0);
    
    return dobj;
}

/****************************************************************************/
/* Icon writing functions                                                    */
/****************************************************************************/

/* Helper to write a BCPL string to file */
static BOOL WriteBCPLString(BPTR fh, STRPTR str)
{
    ULONG len = str ? strlen((const char *)str) : 0;
    
    if (Write(fh, &len, 4) != 4)
        return FALSE;
    
    if (len > 0)
    {
        if (Write(fh, str, len) != (LONG)len)
            return FALSE;
    }
    
    return TRUE;
}

BOOL _icon_PutDiskObject ( register struct IconBase       *IconBase __asm("a6"),
                           register CONST_STRPTR           name     __asm("a0"),
                           register CONST struct DiskObject *dobj   __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: PutDiskObject() called name='%s' dobj=0x%08lx\n", 
             STRORNULL(name), dobj);
    
    if (!name || !dobj)
        return FALSE;
    
    /* Build .info filename */
    LONG nameLen = strlen((const char *)name);
    STRPTR infoName = AllocMem(nameLen + 6, MEMF_CLEAR);
    if (!infoName)
        return FALSE;
    
    strcpy((char *)infoName, (const char *)name);
    strcat((char *)infoName, ".info");
    
    /* Create the .info file */
    BPTR fh = Open(infoName, MODE_NEWFILE);
    FreeMem(infoName, nameLen + 6);
    
    if (!fh)
        return FALSE;
    
    BOOL success = TRUE;
    
    /* Write DiskObject header */
    /* Need to write a modified version with pointers converted to flags */
    struct DiskObject tempDO = *dobj;
    tempDO.do_DrawerData = dobj->do_DrawerData ? (struct DrawerData *)1 : NULL;
    tempDO.do_Gadget.GadgetRender = dobj->do_Gadget.GadgetRender ? (APTR)1 : NULL;
    tempDO.do_Gadget.SelectRender = dobj->do_Gadget.SelectRender ? (APTR)1 : NULL;
    tempDO.do_DefaultTool = dobj->do_DefaultTool ? (STRPTR)1 : NULL;
    tempDO.do_ToolTypes = dobj->do_ToolTypes ? (STRPTR *)1 : NULL;
    tempDO.do_ToolWindow = dobj->do_ToolWindow ? (STRPTR)1 : NULL;
    
    if (Write(fh, &tempDO, sizeof(struct DiskObject)) != sizeof(struct DiskObject))
        success = FALSE;
    
    /* Write DrawerData if present */
    if (success && dobj->do_DrawerData)
    {
        if (Write(fh, dobj->do_DrawerData, sizeof(struct OldDrawerData)) != sizeof(struct OldDrawerData))
            success = FALSE;
    }
    
    /* Write first image if present */
    if (success && dobj->do_Gadget.GadgetRender)
    {
        struct Image *img = (struct Image *)dobj->do_Gadget.GadgetRender;
        /* Write image structure with data pointer as flag */
        struct Image tempImg = *img;
        tempImg.ImageData = img->ImageData ? (UWORD *)1 : NULL;
        
        if (Write(fh, &tempImg, sizeof(struct Image)) != sizeof(struct Image))
            success = FALSE;
        
        /* Write image data */
        if (success && img->ImageData)
        {
            LONG words = ((img->Width + 15) / 16) * img->Height * img->Depth;
            LONG bytes = words * 2;
            if (bytes > 0)
            {
                if (Write(fh, img->ImageData, bytes) != bytes)
                    success = FALSE;
            }
        }
    }
    
    /* Write second image if present */
    if (success && dobj->do_Gadget.SelectRender)
    {
        struct Image *img = (struct Image *)dobj->do_Gadget.SelectRender;
        struct Image tempImg = *img;
        tempImg.ImageData = img->ImageData ? (UWORD *)1 : NULL;
        
        if (Write(fh, &tempImg, sizeof(struct Image)) != sizeof(struct Image))
            success = FALSE;
        
        if (success && img->ImageData)
        {
            LONG words = ((img->Width + 15) / 16) * img->Height * img->Depth;
            LONG bytes = words * 2;
            if (bytes > 0)
            {
                if (Write(fh, img->ImageData, bytes) != bytes)
                    success = FALSE;
            }
        }
    }
    
    /* Write DefaultTool */
    if (success && !WriteBCPLString(fh, dobj->do_DefaultTool))
        success = FALSE;
    
    /* Write ToolTypes */
    if (success && dobj->do_ToolTypes)
    {
        /* Count tool types */
        ULONG numTools = 0;
        STRPTR *tt = dobj->do_ToolTypes;
        while (*tt)
        {
            numTools++;
            tt++;
        }
        
        if (Write(fh, &numTools, 4) != 4)
            success = FALSE;
        
        if (success)
        {
            tt = dobj->do_ToolTypes;
            while (*tt && success)
            {
                if (!WriteBCPLString(fh, *tt))
                    success = FALSE;
                tt++;
            }
        }
    }
    else if (success)
    {
        ULONG zero = 0;
        if (Write(fh, &zero, 4) != 4)
            success = FALSE;
    }
    
    /* Write ToolWindow */
    if (success)
        WriteBCPLString(fh, dobj->do_ToolWindow);
    
    Close(fh);
    return success;
}

BOOL _icon_PutDefDiskObject ( register struct IconBase       *IconBase  __asm("a6"),
                              register CONST struct DiskObject *dobj    __asm("a0"))
{
    UBYTE default_name[64];

    DPRINTF (LOG_DEBUG, "_icon: PutDefDiskObject() called dobj=0x%08lx\n", dobj);

    if (!dobj)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!icon_is_valid_type(dobj->do_Type))
    {
        SetIoErr(ERROR_BAD_NUMBER);
        return FALSE;
    }

    if (!icon_build_default_path(dobj->do_Type, default_name, sizeof(default_name)))
    {
        SetIoErr(ERROR_BAD_NUMBER);
        return FALSE;
    }

    icon_ensure_default_directory();

    if (!_icon_PutDiskObject(IconBase, (CONST_STRPTR)default_name, dobj))
    {
        if (IoErr() == 0)
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    SetIoErr(0);
    return TRUE;
}

BOOL _icon_DeleteDiskObject ( register struct IconBase *IconBase __asm("a6"),
                              register CONST_STRPTR     name     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: DeleteDiskObject() called name='%s'\n", STRORNULL(name));
    
    if (!name)
        return FALSE;
    
    /* Build .info filename */
    LONG nameLen = strlen((const char *)name);
    STRPTR infoName = AllocMem(nameLen + 6, MEMF_CLEAR);
    if (!infoName)
        return FALSE;
    
    strcpy((char *)infoName, (const char *)name);
    strcat((char *)infoName, ".info");
    
    BOOL result = DeleteFile(infoName);
    FreeMem(infoName, nameLen + 6);
    
    return result;
}

/****************************************************************************/
/* Icon freeing function                                                     */
/****************************************************************************/

VOID _icon_FreeDiskObject ( register struct IconBase   *IconBase __asm("a6"),
                            register struct DiskObject *dobj     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: FreeDiskObject() called dobj=0x%08lx\n", dobj);
    
    if (!dobj)
        return;
    
    /* Free DrawerData */
    if (dobj->do_DrawerData)
    {
        FreeMem(dobj->do_DrawerData, sizeof(struct DrawerData));
    }
    
    /* Free first image */
    if (dobj->do_Gadget.GadgetRender)
    {
        struct Image *img = (struct Image *)dobj->do_Gadget.GadgetRender;
        if (img->ImageData)
        {
            LONG words = ((img->Width + 15) / 16) * img->Height * img->Depth;
            LONG bytes = words * 2;
            if (bytes > 0)
                FreeMem(img->ImageData, bytes);
        }
        FreeMem(img, sizeof(struct Image));
    }
    
    /* Free second image */
    if (dobj->do_Gadget.SelectRender)
    {
        struct Image *img = (struct Image *)dobj->do_Gadget.SelectRender;
        if (img->ImageData)
        {
            LONG words = ((img->Width + 15) / 16) * img->Height * img->Depth;
            LONG bytes = words * 2;
            if (bytes > 0)
                FreeMem(img->ImageData, bytes);
        }
        FreeMem(img, sizeof(struct Image));
    }
    
    /* Free DefaultTool */
    if (dobj->do_DefaultTool)
    {
        FreeMem(dobj->do_DefaultTool, strlen((const char *)dobj->do_DefaultTool) + 1);
    }
    
    /* Free ToolTypes */
    if (dobj->do_ToolTypes)
    {
        STRPTR *tt = dobj->do_ToolTypes;
        ULONG count = 0;
        while (*tt)
        {
            FreeMem(*tt, strlen((const char *)*tt) + 1);
            count++;
            tt++;
        }
        FreeMem(dobj->do_ToolTypes, (count + 1) * sizeof(STRPTR));
    }
    
    /* Free ToolWindow */
    if (dobj->do_ToolWindow)
    {
        FreeMem(dobj->do_ToolWindow, strlen((const char *)dobj->do_ToolWindow) + 1);
    }
    
    /* Free the DiskObject itself */
    icon_remove_private_state(IconBase, dobj);
    FreeMem(dobj, sizeof(struct DiskObject));
}

/****************************************************************************/
/* ToolType functions                                                        */
/****************************************************************************/

UBYTE * _icon_FindToolType ( register struct IconBase *IconBase      __asm("a6"),
                             register CONST_STRPTR    *toolTypeArray __asm("a0"),
                             register CONST_STRPTR     typeName      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: FindToolType() called toolTypeArray=0x%08lx typeName='%s'\n", toolTypeArray, typeName ? (char*)typeName : "(null)");
    
    if (!toolTypeArray || !typeName)
        return NULL;
    
    ULONG typeLen = strlen((const char *)typeName);
    
    while (*toolTypeArray)
    {
        const char *tt = (const char *)*toolTypeArray;
        
        /* Skip leading spaces */
        while (*tt == ' ' || *tt == '\t')
            tt++;
        
        /* Skip comments (lines starting with parentheses) */
        if (*tt == '(')
        {
            toolTypeArray++;
            continue;
        }
        
        /* Check for exact match or match with '=' */
        if (strncasecmp_simple(tt, (const char *)typeName, typeLen) == 0)
        {
            char nextChar = tt[typeLen];
            if (nextChar == '\0')
            {
                /* Exact match - return empty string (boolean TRUE) */
                return (UBYTE *)"";
            }
            else if (nextChar == '=')
            {
                /* Match with value */
                return (UBYTE *)&tt[typeLen + 1];
            }
        }
        
        toolTypeArray++;
    }
    
    return NULL;
}

BOOL _icon_MatchToolValue ( register struct IconBase *IconBase   __asm("a6"),
                            register CONST_STRPTR     typeString __asm("a0"),
                            register CONST_STRPTR     value      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: MatchToolValue() called typeString='%s' value='%s'\n",
             STRORNULL(typeString), STRORNULL(value));
    
    if (!typeString || !value)
        return FALSE;
    
    /* typeString can contain multiple values separated by '|' */
    ULONG valueLen = strlen((const char *)value);
    const char *ts = (const char *)typeString;
    
    while (*ts)
    {
        /* Skip spaces */
        while (*ts == ' ' || *ts == '\t')
            ts++;
        
        /* Compare up to next '|' or end */
        const char *start = ts;
        while (*ts && *ts != '|')
            ts++;
        
        /* Trim trailing spaces */
        const char *end = ts;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
            end--;
        
        /* Compare */
        if ((ULONG)(end - start) == valueLen && 
            strncasecmp_simple(start, (const char *)value, valueLen) == 0)
        {
            return TRUE;
        }
        
        /* Skip the '|' */
        if (*ts == '|')
            ts++;
    }
    
    return FALSE;
}

STRPTR _icon_BumpRevision ( register struct IconBase *IconBase __asm("a6"),
                            register STRPTR           newname  __asm("a0"),
                            register CONST_STRPTR     oldname  __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: BumpRevision() called oldname='%s'\n", STRORNULL(oldname));
    
    if (!newname || !oldname)
        return NULL;
    
    /* Copy oldname to newname, appending "copy_of_" prefix or incrementing copy number */
    const char *old = (const char *)oldname;
    char *dst = (char *)newname;
    
    /* Check if it already starts with "copy_of_" or "copy_<n>_of_" */
    if (strncmp_simple(old, "copy_of_", 8) == 0)
    {
        /* Already a copy, add number: "copy_2_of_<rest>" */
        strcpy(dst, "copy_2_of_");
        strcat(dst, old + 8);
    }
    else if (strncmp_simple(old, "copy_", 5) == 0 && old[5] >= '0' && old[5] <= '9')
    {
        /* Has a number, increment it */
        int num = 0;
        const char *p = old + 5;
        while (*p >= '0' && *p <= '9')
        {
            num = num * 10 + (*p - '0');
            p++;
        }
        if (strncmp_simple(p, "_of_", 4) == 0)
        {
            /* Build "copy_<num+1>_of_<rest>" */
            num++;
            strcpy(dst, "copy_");
            dst += 5;
            /* Convert num to string - simple integer to string */
            if (num >= 10)
            {
                *dst++ = '0' + (num / 10);
                num = num % 10;
            }
            *dst++ = '0' + num;
            strcpy(dst, "_of_");
            strcat(dst, p + 4);
        }
        else
        {
            strcpy(dst, "copy_of_");
            strcat(dst, old);
        }
    }
    else
    {
        strcpy(dst, "copy_of_");
        strcat(dst, old);
    }
    
    return newname;
}

STRPTR _icon_BumpRevisionLength ( register struct IconBase *IconBase  __asm("a6"),
                                  register STRPTR           newname   __asm("a0"),
                                  register CONST_STRPTR     oldname   __asm("a1"),
                                  register ULONG            maxLength __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_icon: BumpRevisionLength() called\n");
    
    /* Call BumpRevision then truncate if needed */
    STRPTR result = _icon_BumpRevision(IconBase, newname, oldname);
    
    if (result && strlen((const char *)result) >= maxLength)
    {
        result[maxLength - 1] = '\0';
    }
    
    return result;
}

/****************************************************************************/
/* V44+ functions (stubs for now)                                           */
/****************************************************************************/

struct DiskObject * _icon_DupDiskObjectA ( register struct IconBase       *IconBase __asm("a6"),
                                           register CONST struct DiskObject *dobj   __asm("a0"),
                                           register CONST struct TagItem  *tags     __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: DupDiskObjectA() called\n");
    
    if (!dobj)
        return NULL;
    
    /* Simple duplication - allocate and copy */
    struct DiskObject *newDobj = AllocMem(sizeof(struct DiskObject), MEMF_CLEAR | MEMF_PUBLIC);
    if (!newDobj)
        return NULL;
    
    /* Copy basic structure */
    *newDobj = *dobj;
    
    /* Clear pointers - we need to duplicate these separately */
    newDobj->do_DrawerData = NULL;
    newDobj->do_Gadget.GadgetRender = NULL;
    newDobj->do_Gadget.SelectRender = NULL;
    newDobj->do_DefaultTool = NULL;
    newDobj->do_ToolTypes = NULL;
    newDobj->do_ToolWindow = NULL;
    
    /* Duplicate strings */
    if (dobj->do_DefaultTool)
    {
        ULONG len = strlen((const char *)dobj->do_DefaultTool) + 1;
        newDobj->do_DefaultTool = AllocMem(len, MEMF_PUBLIC);
        if (newDobj->do_DefaultTool)
            strcpy((char *)newDobj->do_DefaultTool, (const char *)dobj->do_DefaultTool);
    }
    
    if (dobj->do_ToolWindow)
    {
        ULONG len = strlen((const char *)dobj->do_ToolWindow) + 1;
        newDobj->do_ToolWindow = AllocMem(len, MEMF_PUBLIC);
        if (newDobj->do_ToolWindow)
            strcpy((char *)newDobj->do_ToolWindow, (const char *)dobj->do_ToolWindow);
    }
    
    /* Duplicate ToolTypes */
    if (dobj->do_ToolTypes)
    {
        ULONG count = 0;
        STRPTR *tt = dobj->do_ToolTypes;
        while (*tt)
        {
            count++;
            tt++;
        }
        
        newDobj->do_ToolTypes = AllocMem((count + 1) * sizeof(STRPTR), MEMF_CLEAR | MEMF_PUBLIC);
        if (newDobj->do_ToolTypes)
        {
            ULONG i;
            for (i = 0; i < count; i++)
            {
                ULONG len = strlen((const char *)dobj->do_ToolTypes[i]) + 1;
                newDobj->do_ToolTypes[i] = AllocMem(len, MEMF_PUBLIC);
                if (newDobj->do_ToolTypes[i])
                    strcpy((char *)newDobj->do_ToolTypes[i], (const char *)dobj->do_ToolTypes[i]);
            }
            newDobj->do_ToolTypes[count] = NULL;
        }
    }
    
    return newDobj;
}

ULONG _icon_IconControlA ( register struct IconBase      *IconBase __asm("a6"),
                           register struct DiskObject    *icon     __asm("a0"),
                           register CONST struct TagItem *tags     __asm("a1"))
{
    struct TagItem *state = (struct TagItem *)tags;
    struct TagItem *tag;
    struct TagItem *error_tag = NULL;
    LONG error_code = 0;
    ULONG processed = 0;

    DPRINTF (LOG_DEBUG, "_icon: IconControlA() called icon=0x%08lx tags=0x%08lx\n", icon, tags);

    while ((tag = NextTagItem(&state)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case ICONA_ErrorCode:
            case ICONA_ErrorTagItem:
                break;

            case ICONCTRLA_SetGlobalScreen:
                IconBase->GlobalScreen = (struct Screen *)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetGlobalScreen:
                if ((APTR)tag->ti_Data)
                {
                    *(struct Screen **)tag->ti_Data = IconBase->GlobalScreen;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalPrecision:
                IconBase->GlobalPrecision = (LONG)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetGlobalPrecision:
                if ((APTR)tag->ti_Data)
                {
                    *(LONG *)tag->ti_Data = IconBase->GlobalPrecision;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalEmbossRect:
                if ((APTR)tag->ti_Data)
                {
                    IconBase->GlobalEmbossRect = *(struct Rectangle *)tag->ti_Data;
                    processed++;
                }
                break;

            case ICONCTRLA_GetGlobalEmbossRect:
                if ((APTR)tag->ti_Data)
                {
                    *(struct Rectangle *)tag->ti_Data = IconBase->GlobalEmbossRect;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalFrameless:
                IconBase->GlobalFrameless = tag->ti_Data ? TRUE : FALSE;
                processed++;
                break;

            case ICONCTRLA_GetGlobalFrameless:
                if ((APTR)tag->ti_Data)
                {
                    *(ULONG *)tag->ti_Data = IconBase->GlobalFrameless;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalNewIconsSupport:
                IconBase->GlobalNewIconsSupport = tag->ti_Data ? TRUE : FALSE;
                processed++;
                break;

            case ICONCTRLA_GetGlobalNewIconsSupport:
                if ((APTR)tag->ti_Data)
                {
                    *(ULONG *)tag->ti_Data = IconBase->GlobalNewIconsSupport;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalColorIconSupport:
                IconBase->GlobalColorIconSupport = tag->ti_Data ? TRUE : FALSE;
                processed++;
                break;

            case ICONCTRLA_GetGlobalColorIconSupport:
                if ((APTR)tag->ti_Data)
                {
                    *(ULONG *)tag->ti_Data = IconBase->GlobalColorIconSupport;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalIdentifyHook:
                IconBase->GlobalIdentifyHook = (struct Hook *)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetGlobalIdentifyHook:
                if ((APTR)tag->ti_Data)
                {
                    *(struct Hook **)tag->ti_Data = IconBase->GlobalIdentifyHook;
                    processed++;
                }
                break;

            case ICONCTRLA_SetGlobalMaxNameLength:
                IconBase->GlobalMaxNameLength = (LONG)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetGlobalMaxNameLength:
                if ((APTR)tag->ti_Data)
                {
                    *(LONG *)tag->ti_Data = IconBase->GlobalMaxNameLength;
                    processed++;
                }
                break;

            case ICONCTRLA_SetFrameless:
            case ICONCTRLA_GetFrameless:
            case ICONCTRLA_GetScreen:
            case ICONCTRLA_SetWidth:
            case ICONCTRLA_GetWidth:
            case ICONCTRLA_SetHeight:
            case ICONCTRLA_GetHeight:
            case ICONCTRLA_HasRealImage2:
            case ICONCTRLA_IsPaletteMapped:
            case ICONCTRLA_IsNewIcon:
            case ICONCTRLA_IsNativeIcon:
                if (!icon)
                {
                    error_code = ERROR_REQUIRED_ARG_MISSING;
                    error_tag = tag;
                    goto done;
                }
                break;

            default:
                error_code = ERROR_BAD_NUMBER;
                error_tag = tag;
                goto done;
        }

        switch (tag->ti_Tag)
        {
            case ICONCTRLA_SetFrameless:
            {
                struct IconPrivateState *private_state = icon_ensure_private_state(IconBase, icon);

                if (!private_state)
                {
                    error_code = ERROR_NO_FREE_STORE;
                    error_tag = tag;
                    goto done;
                }

                if (tag->ti_Data)
                    private_state->flags |= ICON_PRIVATE_FRAMELESS;
                else
                    private_state->flags &= ~ICON_PRIVATE_FRAMELESS;
                processed++;
                break;
            }

            case ICONCTRLA_GetFrameless:
                *(ULONG *)tag->ti_Data = icon_is_frameless(IconBase, icon, NULL);
                processed++;
                break;

            case ICONCTRLA_GetScreen:
                *(struct Screen **)tag->ti_Data = icon_select_screen(IconBase, icon);
                processed++;
                break;

            case ICONCTRLA_SetWidth:
                icon->do_Gadget.Width = (WORD)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetWidth:
                *(LONG *)tag->ti_Data = icon->do_Gadget.Width;
                processed++;
                break;

            case ICONCTRLA_SetHeight:
                icon->do_Gadget.Height = (WORD)tag->ti_Data;
                processed++;
                break;

            case ICONCTRLA_GetHeight:
                *(LONG *)tag->ti_Data = icon->do_Gadget.Height;
                processed++;
                break;

            case ICONCTRLA_HasRealImage2:
                *(LONG *)tag->ti_Data = icon->do_Gadget.SelectRender ? TRUE : FALSE;
                processed++;
                break;

            case ICONCTRLA_IsPaletteMapped:
                *(LONG *)tag->ti_Data = FALSE;
                processed++;
                break;

            case ICONCTRLA_IsNewIcon:
                *(LONG *)tag->ti_Data = FALSE;
                processed++;
                break;

            case ICONCTRLA_IsNativeIcon:
                *(LONG *)tag->ti_Data = TRUE;
                processed++;
                break;
        }
    }

done:
    if (tags)
    {
        LONG *tag_error = (LONG *)GetTagData(ICONA_ErrorCode, 0, (struct TagItem *)tags);
        struct TagItem **tag_error_item = (struct TagItem **)GetTagData(ICONA_ErrorTagItem, 0, (struct TagItem *)tags);

        if (tag_error)
            *tag_error = error_code;
        if (tag_error_item)
            *tag_error_item = error_tag;
    }

    SetIoErr(error_code);
    return processed;
}

VOID _icon_DrawIconStateA ( register struct IconBase       *IconBase   __asm("a6"),
                            register struct RastPort       *rp         __asm("a0"),
                            register CONST struct DiskObject *icon     __asm("a1"),
                            register CONST_STRPTR           label      __asm("a2"),
                            register LONG                   leftOffset __asm("d0"),
                            register LONG                   topOffset  __asm("d1"),
                            register ULONG                  state      __asm("d2"),
                            register CONST struct TagItem  *tags       __asm("a3"))
{
    struct IconLayoutInfo layout;
    struct Image *image;
    ULONG image_state;
    struct DrawInfo *draw_info;
    BOOL free_draw_info;
    UBYTE saved_apen;
    UBYTE saved_bpen;
    UBYTE saved_mode;
    UBYTE text_pen;

    DPRINTF (LOG_DEBUG, "_icon: DrawIconStateA() called icon=0x%08lx label='%s' state=0x%08lx\n",
             icon, STRORNULL(label), state);

    if (!rp || !icon)
        return;

    icon_compute_layout(IconBase, rp, icon, label, tags, &layout);
    image = icon_select_image(icon, state);
    image_state = state;
    if (state == IDS_SELECTED && icon->do_Gadget.SelectRender && image == (struct Image *)icon->do_Gadget.SelectRender)
        image_state = IDS_NORMAL;
    draw_info = icon_acquire_draw_info(IconBase, icon, tags, &free_draw_info);

    saved_apen = rp->FgPen;
    saved_bpen = rp->BgPen;
    saved_mode = rp->DrawMode;

    if (layout.frameless && icon_tag_enabled(tags, ICONDRAWA_EraseBackground, FALSE))
    {
        UBYTE bg = draw_info ? (UBYTE)draw_info->dri_Pens[BACKGROUNDPEN] : 0;
        SetAPen(rp, bg);
        RectFill(rp,
                 leftOffset,
                 topOffset,
                 leftOffset + layout.total_width - 1,
                 topOffset + layout.total_height - 1);
    }

    if (!layout.frameless && !layout.borderless)
    {
        icon_draw_frame(rp,
                        leftOffset,
                        topOffset,
                        layout.total_width,
                        layout.image_height + 4,
                        draw_info,
                        state == IDS_SELECTED);
    }

    if (image)
    {
        DrawImageState(rp,
                       image,
                       leftOffset + layout.image_x,
                       topOffset + layout.image_y,
                       image_state,
                       draw_info);
    }

    if (label)
    {
        text_pen = draw_info ? (UBYTE)draw_info->dri_Pens[(state == IDS_SELECTED) ? FILLTEXTPEN : TEXTPEN] : 1;
        SetAPen(rp, text_pen);
        Move(rp,
             leftOffset + layout.label_x,
             topOffset + layout.label_y + rp->TxBaseline);
        Text(rp, label, strlen((const char *)label));
    }

    SetAPen(rp, saved_apen);
    SetBPen(rp, saved_bpen);
    SetDrMd(rp, saved_mode);
    icon_release_draw_info(icon, draw_info, free_draw_info, IconBase);
}

BOOL _icon_GetIconRectangleA ( register struct IconBase       *IconBase __asm("a6"),
                               register struct RastPort       *rp       __asm("a0"),
                               register CONST struct DiskObject *icon   __asm("a1"),
                               register CONST_STRPTR           label    __asm("a2"),
                               register struct Rectangle      *rect     __asm("a3"),
                               register CONST struct TagItem  *tags     __asm("a4"))
{
    struct IconLayoutInfo layout;

    DPRINTF (LOG_DEBUG, "_icon: GetIconRectangleA() called icon=0x%08lx label='%s' rect=0x%08lx\n",
             icon, STRORNULL(label), rect);

    if (!icon || !rect)
        return FALSE;

    icon_compute_layout(IconBase, rp, icon, label, tags, &layout);
    rect->MinX = 0;
    rect->MinY = 0;
    rect->MaxX = layout.total_width - 1;
    rect->MaxY = layout.total_height - 1;
    return TRUE;
}

struct DiskObject * _icon_NewDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                          register LONG             type     __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_icon: NewDiskObject() called type=%ld\n", type);
    return _icon_GetDefDiskObject(IconBase, type);
}

struct DiskObject * _icon_GetIconTagList ( register struct IconBase      *IconBase __asm("a6"),
                                           register CONST_STRPTR          name     __asm("a0"),
                                           register CONST struct TagItem *tags     __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_icon: GetIconTagList() called name='%s'\n", STRORNULL(name));
    /* For now, just call GetDiskObjectNew */
    return _icon_GetDiskObjectNew(IconBase, name);
}

BOOL _icon_PutIconTagList ( register struct IconBase       *IconBase __asm("a6"),
                            register CONST_STRPTR           name     __asm("a0"),
                            register CONST struct DiskObject *icon   __asm("a1"),
                            register CONST struct TagItem   *tags    __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_icon: PutIconTagList() called\n");
    return _icon_PutDiskObject(IconBase, name, icon);
}

BOOL _icon_LayoutIconA ( register struct IconBase      *IconBase __asm("a6"),
                         register struct DiskObject    *icon     __asm("a0"),
                         register struct Screen        *screen   __asm("a1"),
                         register struct TagItem       *tags     __asm("a2"))
{
    struct IconPrivateState *state;

    DPRINTF (LOG_DEBUG, "_icon: LayoutIconA() called icon=0x%08lx screen=0x%08lx\n", icon, screen);

    if (!icon)
        return FALSE;

    state = icon_ensure_private_state(IconBase, icon);
    if (!state)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }

    state->screen = screen;

    if (icon->do_Gadget.GadgetRender)
    {
        struct Image *image = (struct Image *)icon->do_Gadget.GadgetRender;

        if (image->Width > 0)
            icon->do_Gadget.Width = image->Width;
        if (image->Height > 0)
            icon->do_Gadget.Height = image->Height;
    }

    SetIoErr(0);
    return TRUE;
}

VOID _icon_ChangeToSelectedIconColor ( register struct IconBase    *IconBase __asm("a6"),
                                       register struct ColorRegister *cr     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: ChangeToSelectedIconColor() called cr=0x%08lx\n", cr);

    if (!cr)
        return;

    cr->red = (UBYTE)((cr->red + 0x22 > 0xff) ? 0xff : cr->red + 0x22);
    cr->green = (UBYTE)((cr->green + 0x22 > 0xff) ? 0xff : cr->green + 0x22);
    cr->blue = (UBYTE)((cr->blue + 0x22 > 0xff) ? 0xff : cr->blue + 0x22);
}

/****************************************************************************/
/* ROMTag and library initialization                                         */
/****************************************************************************/

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_icon_FuncTab [];
extern struct MyDataInit __g_lxa_icon_DataTab;
extern struct InitTable  __g_lxa_icon_InitTab;
extern APTR              __g_lxa_icon_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                // UWORD rt_MatchWord
    &ROMTag,                      // struct Resident *rt_MatchTag
    &__g_lxa_icon_EndResident,    // APTR  rt_EndSkip
    RTF_AUTOINIT,                 // UBYTE rt_Flags
    VERSION,                      // UBYTE rt_Version
    NT_LIBRARY,                   // UBYTE rt_Type
    0,                            // BYTE  rt_Pri
    &_g_icon_ExLibName[0],        // char  *rt_Name
    &_g_icon_ExLibID[0],          // char  *rt_IdString
    &__g_lxa_icon_InitTab         // APTR  rt_Init
};

APTR __g_lxa_icon_EndResident;
struct Resident *__lxa_icon_ROMTag = &ROMTag;

struct InitTable __g_lxa_icon_InitTab =
{
    (ULONG)               sizeof(struct IconBase),
    (APTR              *) &__g_lxa_icon_FuncTab[0],
    (APTR)                &__g_lxa_icon_DataTab,
    (APTR)                __g_lxa_icon_InitLib
};

/* Reserved function stub */
static ULONG _icon_Reserved(void) { return 0; }

/* Function table - offsets match AmigaOS 3.x icon.library 
 * Offsets from pragmas: FreeFreeList=0x36, AddFreeList=0x48, etc.
 * Each entry is at position (offset/6 - 1) from Open.
 */
APTR __g_lxa_icon_FuncTab [] =
{
    __g_lxa_icon_OpenLib,           // -6   (0x06) pos 0
    __g_lxa_icon_CloseLib,          // -12  (0x0c) pos 1
    __g_lxa_icon_ExpungeLib,        // -18  (0x12) pos 2
    __g_lxa_icon_ExtFuncLib,        // -24  (0x18) pos 3
    _icon_Reserved,                 // -30  (0x1e) pos 4 - reserved (obsolete WBObject functions)
    _icon_Reserved,                 // -36  (0x24) pos 5 - reserved
    _icon_Reserved,                 // -42  (0x2a) pos 6 - reserved
    _icon_Reserved,                 // -48  (0x30) pos 7 - reserved
    _icon_FreeFreeList,             // -54  (0x36) pos 8 (obsolete)
    _icon_Reserved,                 // -60  (0x3c) pos 9 - reserved
    _icon_Reserved,                 // -66  (0x42) pos 10 - reserved
    _icon_AddFreeList,              // -72  (0x48) pos 11 (obsolete)
    _icon_GetDiskObject,            // -78  (0x4e) pos 12
    _icon_PutDiskObject,            // -84  (0x54) pos 13
    _icon_FreeDiskObject,           // -90  (0x5a) pos 14
    _icon_FindToolType,             // -96  (0x60) pos 15
    _icon_MatchToolValue,           // -102 (0x66) pos 16
    _icon_BumpRevision,             // -108 (0x6c) pos 17
    _icon_FreeAlloc,                // -114 (0x72) pos 18 (obsolete)
    _icon_GetDefDiskObject,         // -120 (0x78) pos 19 (V36)
    _icon_PutDefDiskObject,         // -126 (0x7e) pos 20 (V36)
    _icon_GetDiskObjectNew,         // -132 (0x84) pos 21 (V36)
    _icon_DeleteDiskObject,         // -138 (0x8a) pos 22 (V37)
    _icon_FreeFree,                 // -144 (0x90) pos 23 (V44)
    _icon_DupDiskObjectA,           // -150 (0x96) pos 24 (V44)
    _icon_IconControlA,             // -156 (0x9c) pos 25 (V44)
    _icon_DrawIconStateA,           // -162 (0xa2) pos 26 (V44)
    _icon_GetIconRectangleA,        // -168 (0xa8) pos 27 (V44)
    _icon_NewDiskObject,            // -174 (0xae) pos 28 (V44)
    _icon_GetIconTagList,           // -180 (0xb4) pos 29 (V44)
    _icon_PutIconTagList,           // -186 (0xba) pos 30 (V44)
    _icon_LayoutIconA,              // -192 (0xc0) pos 31 (V44)
    _icon_ChangeToSelectedIconColor,// -198 (0xc6) pos 32 (V44)
    _icon_BumpRevisionLength,       // -204 (0xcc) pos 33 (V44)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_icon_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_icon_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_icon_ExLibID[0],
    (ULONG) 0
};
