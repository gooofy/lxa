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
#include "lxa_icon_defaults.h"

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

/*
 * Default icon names (ENV:Sys/def_<name>.info), indexed by do_Type.
 * AmigaOS 3.1 (reference-verified): WBGARBAGE uses "def_trashcan", WBDEVICE
 * has an empty name ("def_.info") and no built-in image, WBAPPICON has no
 * default icon at all.
 */
static BOOL icon_has_default(LONG type)
{
    return (type >= WBDISK && type <= WBKICK);
}

static CONST_STRPTR icon_default_type_name(LONG type)
{
    switch (type)
    {
        case WBDISK:    return (CONST_STRPTR)"disk";
        case WBDRAWER:  return (CONST_STRPTR)"drawer";
        case WBTOOL:    return (CONST_STRPTR)"tool";
        case WBPROJECT: return (CONST_STRPTR)"project";
        case WBGARBAGE: return (CONST_STRPTR)"trashcan";
        case WBDEVICE:  return (CONST_STRPTR)"";
        case WBKICK:    return (CONST_STRPTR)"kick";
    }

    return NULL;
}

/* "ENV:Sys/def_<name>" (without the .info suffix) */
static BOOL icon_build_default_path(LONG type, UBYTE *buffer, ULONG size)
{
    CONST_STRPTR type_name = icon_default_type_name(type);

    if (!type_name || !buffer || size < 32)
        return FALSE;

    strcpy((char *)buffer, "ENV:Sys/def_");
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

    lock = CreateDir((CONST_STRPTR)"ENV:Sys");
    if (lock)
        UnLock(lock);   /* CreateDir() returns an exclusive lock */
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
    PRIVATE_FUNCTION_ERROR("_icon", "ExtFuncLib");
    return 0;
}

/****************************************************************************/
/* FreeList functions                                                        */
/****************************************************************************/

/*
 * A FreeList is a list of MemLists with ICON_FREELIST_ENTRIES entries each.
 * fl_NumFree counts the free entries of the last MemList; entries are
 * filled from the top down and entry 0 stays unused, so a new MemList is
 * appended once fl_NumFree drops to 1 (AmigaOS 3.1, reference-verified).
 * FreeFreeList() frees every recorded block and the MemLists, but not the
 * FreeList itself.
 */
#define ICON_FREELIST_ENTRIES 10
#define ICON_MEMLIST_SIZE (sizeof(struct MemList) + (ICON_FREELIST_ENTRIES - 1) * sizeof(struct MemEntry))

static BOOL icon_add_free_list(struct FreeList *freelist, APTR mem, ULONG size)
{
    struct MemList *ml;

    if (freelist->fl_NumFree <= 1)
    {
        ml = AllocMem(ICON_MEMLIST_SIZE, MEMF_CLEAR | MEMF_PUBLIC);
        if (!ml)
            return FALSE;
        ml->ml_NumEntries = ICON_FREELIST_ENTRIES;
        AddTail(&freelist->fl_MemList, &ml->ml_Node);
        freelist->fl_NumFree = ICON_FREELIST_ENTRIES;
    }

    ml = (struct MemList *)freelist->fl_MemList.lh_TailPred;
    freelist->fl_NumFree--;
    ml->ml_ME[freelist->fl_NumFree].me_Addr = mem;
    ml->ml_ME[freelist->fl_NumFree].me_Length = size;
    return TRUE;
}

static void icon_free_free_list(struct FreeList *freelist)
{
    struct MemList *ml;

    while ((ml = (struct MemList *)RemHead(&freelist->fl_MemList)) != NULL)
    {
        UWORD n = ml->ml_NumEntries;
        UWORD i;

        for (i = 0; i < n; i++)
        {
            if (ml->ml_ME[i].me_Addr && ml->ml_ME[i].me_Length)
                FreeMem(ml->ml_ME[i].me_Addr, ml->ml_ME[i].me_Length);
        }
        FreeMem(ml, sizeof(struct MemList) + (n ? n - 1 : 0) * sizeof(struct MemEntry));
    }
    freelist->fl_NumFree = 0;
}

VOID _icon_FreeFreeList ( register struct IconBase *IconBase __asm("a6"),
                          register struct FreeList *freelist __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_icon: FreeFreeList() called freelist=0x%08lx\n", freelist);

    if (!freelist)
        return;

    icon_free_free_list(freelist);
}

BOOL _icon_AddFreeList ( register struct IconBase *IconBase __asm("a6"),
                         register struct FreeList *freelist __asm("a0"),
                         register CONST_APTR       mem      __asm("a1"),
                         register ULONG            size     __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_icon: AddFreeList() called freelist=0x%08lx mem=0x%08lx size=%ld\n",
             freelist, mem, size);

    if (!freelist)
        return FALSE;

    return icon_add_free_list(freelist, (APTR)mem, size);
}

APTR _icon_FreeAlloc ( register struct IconBase *IconBase __asm("a6"),
                       register struct FreeList *freelist __asm("a0"),
                       register ULONG            len      __asm("a1"),
                       register ULONG            type     __asm("a2"))
{
    APTR mem;

    DPRINTF (LOG_DEBUG, "_icon: FreeAlloc() called len=%ld type=0x%08lx\n", len, type);

    if (!freelist || !len)
        return NULL;

    mem = AllocMem(len, type);
    if (!mem)
        return NULL;

    if (!icon_add_free_list(freelist, mem, len))
    {
        FreeMem(mem, len);
        return NULL;
    }

    return mem;
}

VOID _icon_FreeFree ( register struct IconBase *IconBase __asm("a6"),
                      register struct FreeList *fl       __asm("a0"),
                      register APTR             address  __asm("a1"))
{
    struct Node *node;

    DPRINTF (LOG_DEBUG, "_icon: FreeFree() called\n");

    if (!fl || !address)
        return;

    for (node = fl->fl_MemList.lh_Head; node->ln_Succ; node = node->ln_Succ)
    {
        struct MemList *ml = (struct MemList *)node;
        UWORD i;

        for (i = 0; i < ml->ml_NumEntries; i++)
        {
            if (ml->ml_ME[i].me_Addr == address)
            {
                if (ml->ml_ME[i].me_Length)
                    FreeMem(address, ml->ml_ME[i].me_Length);
                ml->ml_ME[i].me_Addr = NULL;
                ml->ml_ME[i].me_Length = 0;
                return;
            }
        }
    }
}

/****************************************************************************/
/* DiskObject allocation                                                     */
/****************************************************************************/

/*
 * Every DiskObject icon.library hands out carries a FreeList that records
 * all of its memory, so FreeDiskObject() releases exactly what was
 * allocated even if the application replaced pointers (e.g. do_ToolTypes)
 * in the meantime.
 */
struct IconDiskObject
{
    struct DiskObject dobj;
    struct FreeList   fl;
};

static struct IconDiskObject *icon_new_diskobject(void)
{
    struct IconDiskObject *ido = AllocMem(sizeof(*ido), MEMF_CLEAR | MEMF_PUBLIC);

    if (!ido)
        return NULL;

    NEWLIST(&ido->fl.fl_MemList);
    return ido;
}

static APTR icon_alloc(struct IconDiskObject *ido, ULONG size, ULONG flags)
{
    APTR mem;

    if (!size)
        size = 1;

    mem = AllocMem(size, flags);
    if (!mem)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    if (!icon_add_free_list(&ido->fl, mem, size))
    {
        FreeMem(mem, size);
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    return mem;
}

static void icon_dispose_diskobject(struct IconDiskObject *ido)
{
    icon_free_free_list(&ido->fl);
    FreeMem(ido, sizeof(*ido));
}

static LONG icon_image_bytes(CONST struct Image *img)
{
    if (img->Width <= 0 || img->Height <= 0 || img->Depth <= 0)
        return 0;

    return ((LONG)(img->Width + 15) >> 4) * 2 * img->Height * img->Depth;
}

static STRPTR icon_dup_string(struct IconDiskObject *ido, CONST_STRPTR s)
{
    ULONG len = strlen((const char *)s) + 1;
    STRPTR copy = icon_alloc(ido, len, MEMF_PUBLIC);

    if (copy)
        CopyMem((APTR)s, copy, len);
    return copy;
}

static struct Image *icon_dup_image(struct IconDiskObject *ido, CONST struct Image *src,
                                    CONST UWORD *data)
{
    struct Image *img = icon_alloc(ido, sizeof(struct Image), MEMF_PUBLIC);
    LONG bytes;

    if (!img)
        return NULL;

    *img = *src;
    img->ImageData = NULL;
    bytes = icon_image_bytes(src);
    if (bytes > 0)
    {
        img->ImageData = icon_alloc(ido, bytes, MEMF_CHIP | MEMF_CLEAR);
        if (!img->ImageData)
            return NULL;
        if (data)
            CopyMem((APTR)data, img->ImageData, bytes);
    }

    return img;
}

/****************************************************************************/
/* Icon reading functions                                                    */
/****************************************************************************/

/*
 * .info file layout (all big-endian, pointer fields only flag presence):
 *   struct DiskObject (78 bytes)
 *   struct OldDrawerData (56 bytes)           if do_DrawerData
 *   struct Image (20 bytes) + plane data      if do_Gadget.GadgetRender
 *   struct Image (20 bytes) + plane data      if do_Gadget.SelectRender
 *   ULONG len + len bytes (incl. NUL)         if do_DefaultTool
 *   ULONG (n+1)*4, then n strings as above    if do_ToolTypes
 *   string                                    if do_ToolWindow
 *   ULONG dd_Flags, UWORD dd_ViewModes        if do_DrawerData (read only
 *                                             for gadget revision >= 1)
 */

/* a short read is reported with IoErr() 0, like a successful DOS Read() */
static BOOL icon_read(BPTR fh, APTR buf, LONG len)
{
    LONG got = Read(fh, buf, len);

    if (got == len)
        return TRUE;
    if (got >= 0)
        SetIoErr(0);
    return FALSE;
}

static STRPTR icon_read_string(struct IconDiskObject *ido, BPTR fh)
{
    ULONG len;
    STRPTR str;

    if (!icon_read(fh, &len, 4))
        return NULL;

    str = icon_alloc(ido, len + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!str)
        return NULL;

    if (len && !icon_read(fh, str, len))
        return NULL;

    str[len] = '\0';
    return str;
}

static struct Image *icon_read_image(struct IconDiskObject *ido, BPTR fh)
{
    struct Image *img = icon_alloc(ido, sizeof(struct Image), MEMF_PUBLIC);
    LONG bytes;

    if (!img)
        return NULL;

    if (!icon_read(fh, img, sizeof(struct Image)))
        return NULL;

    /* AmigaOS 3.1 (reference-verified): position and plane mapping are reset */
    img->LeftEdge = 0;
    img->TopEdge = 0;
    img->PlanePick = (UBYTE)((1 << img->Depth) - 1);
    img->PlaneOnOff = 0;
    img->ImageData = NULL;
    bytes = icon_image_bytes(img);
    if (bytes > 0)
    {
        img->ImageData = icon_alloc(ido, bytes, MEMF_CHIP);
        if (!img->ImageData)
            return NULL;
        if (!icon_read(fh, img->ImageData, bytes))
            return NULL;
    }

    return img;
}

static BOOL icon_read_body(struct IconDiskObject *ido, BPTR fh)
{
    struct DiskObject *dobj = &ido->dobj;
    BOOL has_dd = dobj->do_DrawerData != NULL;
    BOOL has_img1 = dobj->do_Gadget.GadgetRender != NULL;
    BOOL has_img2 = dobj->do_Gadget.SelectRender != NULL;
    BOOL has_tool = dobj->do_DefaultTool != NULL;
    BOOL has_tt = dobj->do_ToolTypes != NULL;
    BOOL has_tw = dobj->do_ToolWindow != NULL;

    dobj->do_Gadget.NextGadget = NULL;
    dobj->do_Gadget.GadgetText = NULL;
    dobj->do_Gadget.GadgetRender = NULL;
    dobj->do_Gadget.SelectRender = NULL;
    dobj->do_DrawerData = NULL;
    dobj->do_DefaultTool = NULL;
    dobj->do_ToolTypes = NULL;
    dobj->do_ToolWindow = NULL;

    if (has_dd)
    {
        dobj->do_DrawerData = icon_alloc(ido, sizeof(struct DrawerData), MEMF_PUBLIC | MEMF_CLEAR);
        if (!dobj->do_DrawerData)
            return FALSE;
        if (!icon_read(fh, dobj->do_DrawerData, OLDDRAWERDATAFILESIZE))
            return FALSE;
    }

    if (has_img1)
    {
        dobj->do_Gadget.GadgetRender = icon_read_image(ido, fh);
        if (!dobj->do_Gadget.GadgetRender)
            return FALSE;
    }

    if (has_img2)
    {
        dobj->do_Gadget.SelectRender = icon_read_image(ido, fh);
        if (!dobj->do_Gadget.SelectRender)
            return FALSE;
    }

    if (has_tool)
    {
        dobj->do_DefaultTool = icon_read_string(ido, fh);
        if (!dobj->do_DefaultTool)
            return FALSE;
    }

    if (has_tt)
    {
        ULONG size, count, i;
        STRPTR *tt;

        if (!icon_read(fh, &size, 4))
            return FALSE;

        count = size / sizeof(STRPTR);
        if (count == 0)
            count = 1;

        tt = icon_alloc(ido, count * sizeof(STRPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (!tt)
            return FALSE;

        for (i = 0; i + 1 < count; i++)
        {
            tt[i] = icon_read_string(ido, fh);
            if (!tt[i])
                return FALSE;
        }
        tt[count - 1] = NULL;
        dobj->do_ToolTypes = tt;
    }

    if (has_tw)
    {
        dobj->do_ToolWindow = icon_read_string(ido, fh);
        if (!dobj->do_ToolWindow)
            return FALSE;
    }

    if (has_dd && ((ULONG)dobj->do_Gadget.UserData & WB_DISKREVISIONMASK) >= 1)
    {
        UBYTE ext[6];

        /* optional: files written before V36 end here */
        if (Read(fh, ext, sizeof(ext)) == sizeof(ext))
        {
            dobj->do_DrawerData->dd_Flags = ((ULONG)ext[0] << 24) | ((ULONG)ext[1] << 16) |
                                            ((ULONG)ext[2] << 8) | ext[3];
            dobj->do_DrawerData->dd_ViewModes = (UWORD)((ext[4] << 8) | ext[5]);
        }
    }

    return TRUE;
}

static STRPTR icon_info_name(CONST_STRPTR name, ULONG *size)
{
    ULONG len = strlen((const char *)name) + 6;
    STRPTR info = AllocMem(len, MEMF_PUBLIC);

    if (!info)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    strcpy((char *)info, (const char *)name);
    strcat((char *)info, ".info");
    *size = len;
    return info;
}

struct DiskObject * _icon_GetDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                          register CONST_STRPTR     name     __asm("a0"))
{
    struct IconDiskObject *ido;
    STRPTR info;
    ULONG info_size;
    BPTR fh;
    BOOL ok;

    DPRINTF (LOG_DEBUG, "_icon: GetDiskObject() called name='%s'\n", STRORNULL(name));

    /* AmigaOS 3.1 (reference-verified): NULL yields an empty DiskObject */
    if (!name)
    {
        ido = icon_new_diskobject();
        if (!ido)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return NULL;
        }
        return &ido->dobj;
    }

    info = icon_info_name(name, &info_size);
    if (!info)
        return NULL;

    fh = Open(info, MODE_OLDFILE);
    FreeMem(info, info_size);
    if (!fh)
        return NULL;

    ido = icon_new_diskobject();
    if (!ido)
    {
        Close(fh);
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    ok = icon_read(fh, &ido->dobj, sizeof(struct DiskObject));
    if (ok && (ido->dobj.do_Magic != WB_DISKMAGIC || ido->dobj.do_Version != WB_DISKVERSION))
    {
        SetIoErr(0);
        ok = FALSE;
    }
    if (ok && !(ido->dobj.do_Gadget.Flags & GFLG_GADGIMAGE))
    {
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        ok = FALSE;
    }
    if (ok)
        ok = icon_read_body(ido, fh);

    Close(fh);

    if (!ok)
    {
        /* the header pointers are file garbage until icon_read_body() replaced them */
        icon_dispose_diskobject(ido);
        return NULL;
    }

    return &ido->dobj;
}

/*
 * Built-in default icons (AmigaOS 3.1, reference-verified).  WBKICK shows
 * the disk image; WBDEVICE and WBAPPICON have none.
 */
struct IconDefault
{
    UBYTE        type;
    WORD         gad_width, gad_height;
    UWORD        flags, activation;
    WORD         img_width, img_height;
    CONST UWORD *render;
    CONST UWORD *select;
    BOOL         drawer;
    CONST char  *default_tool;
};

static const struct IconDefault g_icon_defaults[] =
{
    { WBDISK,    35, 18, GFLG_GADGIMAGE,                  GACT_RELVERIFY | GACT_IMMEDIATE, 35, 17,
      g_icon_img_disk,     NULL,                   TRUE,  "SYS:System/DiskCopy" },
    { WBDRAWER,  57, 14, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, GACT_RELVERIFY | GACT_IMMEDIATE, 57, 14,
      g_icon_img_drawer,   g_icon_img_drawer_sel,   TRUE,  NULL },
    { WBTOOL,    54, 23, GFLG_GADGIMAGE,                  GACT_RELVERIFY,                  54, 22,
      g_icon_img_tool,     NULL,                   FALSE, NULL },
    { WBPROJECT, 54, 23, GFLG_GADGIMAGE | GFLG_GADGHBOX,  GACT_RELVERIFY,                  54, 22,
      g_icon_img_project,  NULL,                   FALSE, NULL },
    { WBGARBAGE, 51, 31, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, GACT_RELVERIFY | GACT_IMMEDIATE, 51, 31,
      g_icon_img_trashcan, g_icon_img_trashcan_sel, TRUE,  NULL },
    { WBKICK,    35, 18, GFLG_GADGIMAGE,                  GACT_RELVERIFY | GACT_IMMEDIATE, 35, 17,
      g_icon_img_disk,     NULL,                   FALSE, NULL },
};

static struct DiskObject *icon_builtin_default(LONG type)
{
    const struct IconDefault *def = NULL;
    struct IconDiskObject *ido;
    struct DiskObject *dobj;
    struct Image img;
    ULONG i;

    for (i = 0; i < sizeof(g_icon_defaults) / sizeof(g_icon_defaults[0]); i++)
    {
        if (g_icon_defaults[i].type == type)
        {
            def = &g_icon_defaults[i];
            break;
        }
    }
    if (!def)
        return NULL;

    ido = icon_new_diskobject();
    if (!ido)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    dobj = &ido->dobj;

    dobj->do_Magic = WB_DISKMAGIC;
    dobj->do_Version = WB_DISKVERSION;
    dobj->do_Type = def->type;
    dobj->do_CurrentX = NO_ICON_POSITION;
    dobj->do_CurrentY = NO_ICON_POSITION;
    dobj->do_Gadget.Width = def->gad_width;
    dobj->do_Gadget.Height = def->gad_height;
    dobj->do_Gadget.Flags = def->flags;
    dobj->do_Gadget.Activation = def->activation;
    dobj->do_Gadget.GadgetType = GTYP_BOOLGADGET;

    memset(&img, 0, sizeof(img));
    img.Width = def->img_width;
    img.Height = def->img_height;
    img.Depth = 2;
    img.PlanePick = 3;

    dobj->do_Gadget.GadgetRender = icon_dup_image(ido, &img, def->render);
    if (!dobj->do_Gadget.GadgetRender)
        goto fail;

    if (def->select)
    {
        dobj->do_Gadget.SelectRender = icon_dup_image(ido, &img, def->select);
        if (!dobj->do_Gadget.SelectRender)
            goto fail;
    }

    if (def->drawer)
    {
        struct DrawerData *dd = icon_alloc(ido, sizeof(struct DrawerData), MEMF_PUBLIC | MEMF_CLEAR);

        if (!dd)
            goto fail;
        dd->dd_NewWindow.LeftEdge = 50;
        dd->dd_NewWindow.TopEdge = 50;
        dd->dd_NewWindow.Width = 400;
        dd->dd_NewWindow.Height = 100;
        dd->dd_NewWindow.DetailPen = 255;
        dd->dd_NewWindow.BlockPen = 255;
        dd->dd_NewWindow.Flags = 0x0240027f;    /* as AmigaOS 3.1 */
        dd->dd_NewWindow.MinWidth = 90;
        dd->dd_NewWindow.MinHeight = 40;
        dd->dd_NewWindow.MaxWidth = 0xffff;
        dd->dd_NewWindow.MaxHeight = 0xffff;
        dd->dd_NewWindow.Type = WBENCHSCREEN;
        dobj->do_DrawerData = dd;
    }

    if (def->default_tool)
    {
        dobj->do_DefaultTool = icon_dup_string(ido, (CONST_STRPTR)def->default_tool);
        if (!dobj->do_DefaultTool)
            goto fail;
    }

    return dobj;

fail:
    icon_dispose_diskobject(ido);
    return NULL;
}

struct DiskObject * _icon_GetDefDiskObject ( register struct IconBase *IconBase __asm("a6"),
                                             register LONG              type    __asm("d0"))
{
    UBYTE default_name[32];
    struct DiskObject *dobj;

    DPRINTF (LOG_DEBUG, "_icon: GetDefDiskObject() called type=%ld\n", type);

    /* invalid types (and WBAPPICON) fail without touching IoErr() */
    if (!icon_has_default(type) || !icon_build_default_path(type, default_name, sizeof(default_name)))
        return NULL;

    dobj = _icon_GetDiskObject(IconBase, (CONST_STRPTR)default_name);
    if (dobj)
        return dobj;

    return icon_builtin_default(type);
}

struct DiskObject * _icon_GetDiskObjectNew ( register struct IconBase *IconBase __asm("a6"),
                                             register CONST_STRPTR     name     __asm("a0"))
{
    struct FileInfoBlock *fib;
    struct DiskObject *dobj;
    BPTR lock;
    LONG type;

    DPRINTF (LOG_DEBUG, "_icon: GetDiskObjectNew() called name='%s'\n", STRORNULL(name));

    dobj = _icon_GetDiskObject(IconBase, name);   /* NULL name: an empty DiskObject */
    if (dobj || !name)
        return dobj;

    /* no icon: a default one for what the object is; nothing if it does not exist */
    lock = Lock(name, ACCESS_READ);
    if (!lock)
        return NULL;

    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib)
    {
        UnLock(lock);
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    if (!Examine(lock, fib))
    {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        return NULL;
    }
    UnLock(lock);

    if (fib->fib_DirEntryType == ST_ROOT)
        type = WBDISK;
    else if (fib->fib_DirEntryType > 0)
        type = WBDRAWER;
    else if (fib->fib_Protection & FIBF_EXECUTE)
        type = WBPROJECT;
    else
        type = WBTOOL;
    FreeDosObject(DOS_FIB, fib);

    return _icon_GetDefDiskObject(IconBase, type);
}

/****************************************************************************/
/* Icon writing functions                                                    */
/****************************************************************************/

static BOOL icon_write(BPTR fh, CONST_APTR buf, LONG len)
{
    return Write(fh, (APTR)buf, len) == len;
}

static BOOL icon_write_string(BPTR fh, CONST_STRPTR str)
{
    ULONG len = strlen((const char *)str) + 1;

    return icon_write(fh, &len, 4) && icon_write(fh, str, len);
}

static BOOL icon_write_image(BPTR fh, struct Image *img)
{
    LONG bytes = icon_image_bytes(img);

    /* AmigaOS 3.1 (reference-verified): every plane is stored; the caller's
     * image is updated to match what was written */
    img->PlanePick = (UBYTE)((1 << img->Depth) - 1);
    img->PlaneOnOff = 0;

    if (!icon_write(fh, img, sizeof(struct Image)))
        return FALSE;

    if (bytes <= 0)
        return TRUE;

    if (img->ImageData)
        return icon_write(fh, img->ImageData, bytes);

    /* no plane data in memory: keep the file consistent with zeros */
    while (bytes > 0)
    {
        static const UBYTE zero[64];
        LONG n = bytes > (LONG)sizeof(zero) ? (LONG)sizeof(zero) : bytes;

        if (!icon_write(fh, zero, n))
            return FALSE;
        bytes -= n;
    }
    return TRUE;
}

static BOOL icon_write_diskobject(BPTR fh, CONST struct DiskObject *dobj)
{
    if (!icon_write(fh, dobj, sizeof(struct DiskObject)))
        return FALSE;

    if (dobj->do_DrawerData && !icon_write(fh, dobj->do_DrawerData, OLDDRAWERDATAFILESIZE))
        return FALSE;

    if (dobj->do_Gadget.GadgetRender &&
        !icon_write_image(fh, (struct Image *)dobj->do_Gadget.GadgetRender))
        return FALSE;

    if (dobj->do_Gadget.SelectRender &&
        !icon_write_image(fh, (struct Image *)dobj->do_Gadget.SelectRender))
        return FALSE;

    if (dobj->do_DefaultTool && !icon_write_string(fh, dobj->do_DefaultTool))
        return FALSE;

    if (dobj->do_ToolTypes)
    {
        STRPTR *tt = dobj->do_ToolTypes;
        ULONG size = sizeof(STRPTR);

        while (*tt++)
            size += sizeof(STRPTR);

        if (!icon_write(fh, &size, 4))
            return FALSE;

        for (tt = dobj->do_ToolTypes; *tt; tt++)
        {
            if (!icon_write_string(fh, *tt))
                return FALSE;
        }
    }

    if (dobj->do_ToolWindow && !icon_write_string(fh, dobj->do_ToolWindow))
        return FALSE;

    if (dobj->do_DrawerData)
    {
        if (!icon_write(fh, &dobj->do_DrawerData->dd_Flags, 4) ||
            !icon_write(fh, &dobj->do_DrawerData->dd_ViewModes, 2))
            return FALSE;
    }

    return TRUE;
}

BOOL _icon_PutDiskObject ( register struct IconBase       *IconBase __asm("a6"),
                           register CONST_STRPTR           name     __asm("a0"),
                           register CONST struct DiskObject *dobj   __asm("a1"))
{
    STRPTR info;
    ULONG info_size;
    BPTR fh;
    BOOL ok;
    LONG err;

    DPRINTF (LOG_DEBUG, "_icon: PutDiskObject() called name='%s' dobj=0x%08lx\n",
             STRORNULL(name), dobj);

    if (!name || !dobj)
        return FALSE;

    info = icon_info_name(name, &info_size);
    if (!info)
        return FALSE;

    fh = Open(info, MODE_NEWFILE);
    if (!fh)
    {
        FreeMem(info, info_size);
        return FALSE;
    }

    ok = icon_write_diskobject(fh, dobj);
    err = IoErr();
    if (!Close(fh))
    {
        if (ok)
            err = IoErr();
        ok = FALSE;
    }

    if (!ok)
    {
        DeleteFile(info);   /* never leave a truncated icon behind */
        FreeMem(info, info_size);
        SetIoErr(err);
        return FALSE;
    }

    FreeMem(info, info_size);
    SetIoErr(0);
    return TRUE;
}

BOOL _icon_PutDefDiskObject ( register struct IconBase       *IconBase  __asm("a6"),
                              register CONST struct DiskObject *dobj    __asm("a0"))
{
    UBYTE default_name[32];

    DPRINTF (LOG_DEBUG, "_icon: PutDefDiskObject() called dobj=0x%08lx\n", dobj);

    if (!dobj)
        return FALSE;

    /* AmigaOS 3.1: WBAPPICON fails silently, other unknown types clear IoErr() */
    if (dobj->do_Type == WBAPPICON)
        return FALSE;

    if (!icon_has_default(dobj->do_Type) ||
        !icon_build_default_path(dobj->do_Type, default_name, sizeof(default_name)))
    {
        SetIoErr(0);
        return FALSE;
    }

    icon_ensure_default_directory();

    return _icon_PutDiskObject(IconBase, (CONST_STRPTR)default_name, dobj);
}

BOOL _icon_DeleteDiskObject ( register struct IconBase *IconBase __asm("a6"),
                              register CONST_STRPTR     name     __asm("a0"))
{
    STRPTR info;
    ULONG info_size;
    BOOL result;

    DPRINTF (LOG_DEBUG, "_icon: DeleteDiskObject() called name='%s'\n", STRORNULL(name));

    if (!name)
        return FALSE;

    info = icon_info_name(name, &info_size);
    if (!info)
        return FALSE;

    result = DeleteFile(info);
    FreeMem(info, info_size);

    /* AmigaOS 3.1 (reference-verified): IoErr() is 0 afterwards, also on failure */
    SetIoErr(0);
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

    icon_remove_private_state(IconBase, dobj);
    icon_dispose_diskobject((struct IconDiskObject *)dobj);
}

/****************************************************************************/
/* ToolType functions                                                        */
/****************************************************************************/

/*
 * AmigaOS 3.1 (reference-verified): the name must match the start of an
 * entry exactly (ASCII case-insensitive, no blank skipping, comments in
 * parentheses are ordinary entries) and be followed by '=' (result: the
 * value) or the end of the entry (result: the empty string at its end).
 * An empty name only matches an empty entry.
 */
UBYTE * _icon_FindToolType ( register struct IconBase *IconBase      __asm("a6"),
                             register CONST_STRPTR    *toolTypeArray __asm("a0"),
                             register CONST_STRPTR     typeName      __asm("a1"))
{
    ULONG typeLen;

    DPRINTF (LOG_DEBUG, "_icon: FindToolType() called typeName='%s'\n", STRORNULL(typeName));

    if (!toolTypeArray || !typeName)
        return NULL;

    typeLen = strlen((const char *)typeName);

    for (; *toolTypeArray; toolTypeArray++)
    {
        const char *tt = (const char *)*toolTypeArray;

        if (strncasecmp_simple(tt, (const char *)typeName, typeLen) != 0)
            continue;

        if (tt[typeLen] == '\0')
            return (UBYTE *)&tt[typeLen];
        if (tt[typeLen] == '=' && typeLen > 0)
            return (UBYTE *)&tt[typeLen + 1];
    }

    return NULL;
}

/*
 * AmigaOS 3.1 (reference-verified): typeString is a '|'-separated list;
 * value must equal one element (ASCII case-insensitive, blanks are
 * significant).  An empty value only matches an empty last element.
 */
BOOL _icon_MatchToolValue ( register struct IconBase *IconBase   __asm("a6"),
                            register CONST_STRPTR     typeString __asm("a0"),
                            register CONST_STRPTR     value      __asm("a1"))
{
    const char *ts;
    ULONG valueLen;

    DPRINTF (LOG_DEBUG, "_icon: MatchToolValue() called typeString='%s' value='%s'\n",
             STRORNULL(typeString), STRORNULL(value));

    if (!typeString || !value)
        return FALSE;

    ts = (const char *)typeString;
    valueLen = strlen((const char *)value);

    if (valueLen == 0)
    {
        const char *last = ts;

        for (; *ts; ts++)
        {
            if (*ts == '|')
                last = ts + 1;
        }
        return *last == '\0';
    }

    for (;;)
    {
        const char *start = ts;

        while (*ts && *ts != '|')
            ts++;

        if ((ULONG)(ts - start) == valueLen &&
            strncasecmp_simple(start, (const char *)value, valueLen) == 0)
            return TRUE;

        if (!*ts)
            return FALSE;
        ts++;
    }
}

static BOOL bump_is_sep(char c)
{
    return c == '_' || c == ' ';
}

/* "of" followed by a separator */
static BOOL bump_is_of(const char *p)
{
    return tolower_simple(p[0]) == 'o' && tolower_simple(p[1]) == 'f' && bump_is_sep(p[2]);
}

STRPTR _icon_BumpRevision ( register struct IconBase *IconBase __asm("a6"),
                            register STRPTR           newname  __asm("a0"),
                            register CONST_STRPTR     oldname  __asm("a1"))
{
    /*
     * AmigaOS 3.1 (reference-verified): "foo" -> "Copy_of_foo",
     * "copy_of_foo" -> "Copy_2_of_foo", "copy_<n>_of_foo" -> "Copy_<n+1>_of_foo".
     * "copy", "of" match case-insensitively, each separator is one '_' or
     * ' ', <n> is accumulated as a 32-bit value and printed signed.  The
     * result is truncated to 30 characters.
     */
    const char *old;
    const char *rest = NULL;
    char tmp[64];
    char *dst;
    ULONG num = 0;
    int len = 0;

    DPRINTF (LOG_DEBUG, "_icon: BumpRevision() called oldname='%s'\n", STRORNULL(oldname));

    if (!newname || !oldname)
        return NULL;

    old = (const char *)oldname;

    if (strncasecmp_simple(old, "copy", 4) == 0 && bump_is_sep(old[4]))
    {
        const char *p = old + 5;

        if (bump_is_of(p))
        {
            num = 2;
            rest = p + 3;
        }
        else if (*p >= '0' && *p <= '9')
        {
            ULONG n = 0;

            while (*p >= '0' && *p <= '9')
                n = n * 10 + (ULONG)(*p++ - '0');

            if (bump_is_sep(*p) && bump_is_of(p + 1))
            {
                num = n + 1;
                rest = p + 4;
            }
        }
    }

    dst = tmp;
    strcpy(dst, "Copy_");
    dst += 5;
    if (rest)
    {
        char digits[12];
        LONG v = (LONG)num;
        ULONG u = v < 0 ? (ULONG)(-(v + 1)) + 1 : (ULONG)v;
        int nd = 0;

        if (v < 0)
            *dst++ = '-';
        do
        {
            digits[nd++] = (char)('0' + u % 10);
            u /= 10;
        } while (u);
        while (nd)
            *dst++ = digits[--nd];
        *dst++ = '_';
    }
    else
        rest = old;
    strcpy(dst, "of_");
    dst += 3;
    len = dst - tmp;

    while (len < 30 && *rest)
        tmp[len++] = *rest++;
    tmp[len] = '\0';

    CopyMem(tmp, newname, len + 1);
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

    if (result && maxLength > 0 && strlen((const char *)result) >= maxLength)
    {
        result[maxLength - 1] = '\0';
    }

    return result;
}

/****************************************************************************/
/* V44+ functions                                                            */
/****************************************************************************/

struct DiskObject * _icon_DupDiskObjectA ( register struct IconBase       *IconBase __asm("a6"),
                                           register CONST struct DiskObject *dobj   __asm("a0"),
                                           register CONST struct TagItem  *tags     __asm("a1"))
{
    struct IconDiskObject *ido;
    struct DiskObject *copy;

    DPRINTF (LOG_DEBUG, "_icon: DupDiskObjectA() called\n");

    if (!dobj)
        return NULL;

    ido = icon_new_diskobject();
    if (!ido)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    copy = &ido->dobj;
    *copy = *dobj;
    copy->do_Gadget.GadgetRender = NULL;
    copy->do_Gadget.SelectRender = NULL;
    copy->do_DrawerData = NULL;
    copy->do_DefaultTool = NULL;
    copy->do_ToolTypes = NULL;
    copy->do_ToolWindow = NULL;

    if (dobj->do_Gadget.GadgetRender)
    {
        CONST struct Image *src = (CONST struct Image *)dobj->do_Gadget.GadgetRender;
        copy->do_Gadget.GadgetRender = icon_dup_image(ido, src, src->ImageData);
        if (!copy->do_Gadget.GadgetRender)
            goto fail;
    }

    if (dobj->do_Gadget.SelectRender)
    {
        CONST struct Image *src = (CONST struct Image *)dobj->do_Gadget.SelectRender;
        copy->do_Gadget.SelectRender = icon_dup_image(ido, src, src->ImageData);
        if (!copy->do_Gadget.SelectRender)
            goto fail;
    }

    if (dobj->do_DrawerData)
    {
        copy->do_DrawerData = icon_alloc(ido, sizeof(struct DrawerData), MEMF_PUBLIC);
        if (!copy->do_DrawerData)
            goto fail;
        *copy->do_DrawerData = *dobj->do_DrawerData;
    }

    if (dobj->do_DefaultTool)
    {
        copy->do_DefaultTool = icon_dup_string(ido, dobj->do_DefaultTool);
        if (!copy->do_DefaultTool)
            goto fail;
    }

    if (dobj->do_ToolWindow)
    {
        copy->do_ToolWindow = icon_dup_string(ido, dobj->do_ToolWindow);
        if (!copy->do_ToolWindow)
            goto fail;
    }

    if (dobj->do_ToolTypes)
    {
        ULONG count = 0;
        ULONG i;

        while (dobj->do_ToolTypes[count])
            count++;

        copy->do_ToolTypes = icon_alloc(ido, (count + 1) * sizeof(STRPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (!copy->do_ToolTypes)
            goto fail;

        for (i = 0; i < count; i++)
        {
            copy->do_ToolTypes[i] = icon_dup_string(ido, dobj->do_ToolTypes[i]);
            if (!copy->do_ToolTypes[i])
                goto fail;
        }
    }

    return copy;

fail:
    icon_dispose_diskobject(ido);
    return NULL;
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

    leftOffset = (LONG)(WORD)leftOffset; /* sign-extend: GCC m68k move.w workaround */
    topOffset = (LONG)(WORD)topOffset;

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
    LXA_UNIMPLEMENTED("icon", "GetIconTagList", "partial: ignores all ICONGETA_* tags, behaves like GetDiskObjectNew");

    DPRINTF (LOG_DEBUG, "_icon: GetIconTagList() called name='%s'\n", STRORNULL(name));
    /* For now, just call GetDiskObjectNew */
    return _icon_GetDiskObjectNew(IconBase, name);
}

BOOL _icon_PutIconTagList ( register struct IconBase       *IconBase __asm("a6"),
                            register CONST_STRPTR           name     __asm("a0"),
                            register CONST struct DiskObject *icon   __asm("a1"),
                            register CONST struct TagItem   *tags    __asm("a2"))
{
    LXA_UNIMPLEMENTED("icon", "PutIconTagList", "partial: ignores all ICONPUTA_* tags, behaves like PutDiskObject");

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
static ULONG _icon_Reserved(void) { PRIVATE_FUNCTION_ERROR("_icon", "Reserved"); return 0; }

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
