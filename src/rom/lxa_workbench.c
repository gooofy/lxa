/*
 * lxa workbench.library implementation
 *
 * Provides a hosted Workbench compatibility surface for the public APIs
 * exercised by the regression suite without requiring a full classic
 * Workbench task and backdrop manager.
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
#include <dos/dostags.h>
#include <dos/exall.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <workbench/workbench.h>

#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

#define VERSION    44
#define REVISION   1
#define EXLIBNAME  "workbench"
#define EXLIBVER   " 44.1 (2025/02/02)"

char __aligned _g_workbench_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_workbench_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_workbench_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_workbench_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct IntuitionBase *IntuitionBase;
extern struct UtilityBase *UtilityBase;

/* WorkbenchBase structure */
struct WorkbenchBase {
    struct Library lib;
    BPTR           SegList;
    struct List    app_objects;
    struct List    open_drawers;
    struct List    selected_icons;
    struct List    hidden_devices;
    struct List    program_list;
    struct List    setup_cleanup_hooks;
    ULONG          default_stack_size;
    ULONG          type_restart_time;
    ULONG          global_flags;
    struct Hook   *copy_hook;
    struct Hook   *delete_hook;
    struct Hook   *text_input_hook;
    struct Hook   *diskinfo_hook;
};

#define LXA_WORKBENCH_APP_MAGIC 0x57424150UL

enum lxa_workbench_app_type {
    LXA_WORKBENCH_APP_WINDOW = 1,
    LXA_WORKBENCH_APP_ICON = 2,
    LXA_WORKBENCH_APP_MENU = 3,
    LXA_WORKBENCH_APP_DROP_ZONE = 4
};

struct lxa_workbench_app_object {
    struct Node node;
    ULONG magic;
    ULONG type;
    ULONG id;
    ULONG userdata;
    APTR primary;
    APTR secondary;
    struct MsgPort *msgport;
    STRPTR name;
    struct IBox box;
    struct Hook *hook;
};

struct lxa_workbench_path_node {
    struct Node node;
    STRPTR path;
    ULONG flags;
    ULONG modes;
};

struct lxa_workbench_list_owner {
    struct Node node;
    struct Hook *hook;
};

static int workbench_ascii_casecmp(CONST_STRPTR a, CONST_STRPTR b)
{
    while (a && b && *a && *b)
    {
        char ca = (char)*a;
        char cb = (char)*b;

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + ('a' - 'A'));

        if (ca != cb)
            return (int)((unsigned char)ca - (unsigned char)cb);

        a++;
        b++;
    }

    if (!a)
        return b ? -1 : 0;
    if (!b)
        return 1;

    return (int)((unsigned char)*a - (unsigned char)*b);
}

static ULONG workbench_strlen(CONST_STRPTR s)
{
    ULONG len = 0;

    if (!s)
        return 0;

    while (s[len] != '\0')
        len++;

    return len;
}

static STRPTR workbench_strdup(CONST_STRPTR src)
{
    STRPTR copy;
    ULONG len;

    if (!src)
        return NULL;

    len = workbench_strlen(src) + 1;
    copy = (STRPTR)AllocMem(len, MEMF_CLEAR | MEMF_PUBLIC);
    if (!copy)
        return NULL;

    CopyMem((APTR)src, copy, len);
    return copy;
}

static VOID workbench_free_string(STRPTR value)
{
    if (!value)
        return;

    FreeMem(value, workbench_strlen(value) + 1);
}

static BOOL workbench_copy_string(CONST_STRPTR src, STRPTR dst, ULONG dst_size)
{
    ULONG len;

    if (!dst || dst_size == 0)
        return FALSE;

    if (!src)
    {
        dst[0] = '\0';
        return TRUE;
    }

    len = workbench_strlen(src);
    if (len >= dst_size)
        len = dst_size - 1;

    CopyMem((APTR)src, dst, len);
    dst[len] = '\0';
    return TRUE;
}

static BOOL workbench_path_join(CONST_STRPTR base,
                                CONST_STRPTR name,
                                STRPTR buffer,
                                ULONG buffer_size)
{
    ULONG base_len;
    ULONG name_len;
    BOOL needs_sep;

    if (!buffer || buffer_size == 0 || !base || !name)
        return FALSE;

    base_len = workbench_strlen(base);
    name_len = workbench_strlen(name);
    needs_sep = (base_len > 0 && base[base_len - 1] != ':' && base[base_len - 1] != '/');

    if (base_len + (needs_sep ? 1 : 0) + name_len + 1 > buffer_size)
        return FALSE;

    CopyMem((APTR)base, buffer, base_len);
    if (needs_sep)
        buffer[base_len++] = '/';
    CopyMem((APTR)name, buffer + base_len, name_len);
    buffer[base_len + name_len] = '\0';
    return TRUE;
}

static BOOL workbench_parent_from_path(CONST_STRPTR path,
                                       STRPTR parent,
                                       ULONG parent_size,
                                       STRPTR name,
                                       ULONG name_size)
{
    LONG split = -1;
    LONG i;
    ULONG parent_len;

    if (!path || !parent || !name || parent_size == 0 || name_size == 0)
        return FALSE;

    for (i = (LONG)workbench_strlen(path) - 1; i >= 0; i--)
    {
        if (path[i] == '/' || path[i] == ':')
        {
            split = i;
            break;
        }
    }

    if (split < 0)
        return FALSE;

    parent_len = (path[split] == ':') ? (ULONG)(split + 1) : (ULONG)split;

    if (parent_len + 1 > parent_size)
        return FALSE;

    CopyMem((APTR)path, parent, parent_len);
    parent[parent_len] = '\0';

    return workbench_copy_string(path + split + 1, name, name_size);
}

static BOOL workbench_path_matches(CONST_STRPTR a, CONST_STRPTR b)
{
    return workbench_ascii_casecmp(a, b) == 0;
}

static VOID workbench_free_path_node(struct lxa_workbench_path_node *node)
{
    if (!node)
        return;

    workbench_free_string(node->path);
    FreeMem(node, sizeof(*node));
}

static VOID workbench_free_path_list(struct List *list)
{
    struct lxa_workbench_path_node *node;
    struct lxa_workbench_path_node *next;

    if (!list)
        return;

    node = (struct lxa_workbench_path_node *)GETHEAD(list);
    while (node)
    {
        next = (struct lxa_workbench_path_node *)GETSUCC(&node->node);
        Remove(&node->node);
        workbench_free_path_node(node);
        node = next;
    }
}

static struct lxa_workbench_path_node *workbench_find_path_node(struct List *list,
                                                                CONST_STRPTR path)
{
    struct lxa_workbench_path_node *node;

    if (!list || !path)
        return NULL;

    node = (struct lxa_workbench_path_node *)GETHEAD(list);
    while (node)
    {
        if (workbench_path_matches(node->path, path))
            return node;
        node = (struct lxa_workbench_path_node *)GETSUCC(&node->node);
    }

    return NULL;
}

static struct lxa_workbench_path_node *workbench_add_path_node(struct List *list,
                                                               CONST_STRPTR path)
{
    struct lxa_workbench_path_node *node;

    if (!list || !path)
        return NULL;

    node = workbench_find_path_node(list, path);
    if (node)
        return node;

    node = (struct lxa_workbench_path_node *)AllocMem(sizeof(*node), MEMF_CLEAR | MEMF_PUBLIC);
    if (!node)
        return NULL;

    node->path = workbench_strdup(path);
    if (!node->path)
    {
        FreeMem(node, sizeof(*node));
        return NULL;
    }

    node->node.ln_Name = (char *)node->path;
    AddTail(list, &node->node);
    return node;
}

static BOOL workbench_remove_path_node(struct List *list, CONST_STRPTR path)
{
    struct lxa_workbench_path_node *node = workbench_find_path_node(list, path);

    if (!node)
        return FALSE;

    Remove(&node->node);
    workbench_free_path_node(node);
    return TRUE;
}

static BOOL workbench_remove_selected_prefix(struct WorkbenchBase *wbb, CONST_STRPTR prefix)
{
    struct lxa_workbench_path_node *node;
    struct lxa_workbench_path_node *next;
    ULONG prefix_len;
    BOOL removed = FALSE;

    if (!wbb || !prefix)
        return FALSE;

    prefix_len = workbench_strlen(prefix);
    node = (struct lxa_workbench_path_node *)GETHEAD(&wbb->selected_icons);
    while (node)
    {
        BOOL matches = FALSE;

        next = (struct lxa_workbench_path_node *)GETSUCC(&node->node);
        if (node->path && prefix_len <= workbench_strlen(node->path))
        {
            ULONG i;

            matches = TRUE;
            for (i = 0; i < prefix_len; i++)
            {
                char a = node->path[i];
                char b = prefix[i];

                if (a >= 'A' && a <= 'Z')
                    a = (char)(a + ('a' - 'A'));
                if (b >= 'A' && b <= 'Z')
                    b = (char)(b + ('a' - 'A'));

                if (a != b)
                {
                    matches = FALSE;
                    break;
                }
            }

            if (matches && workbench_strlen(node->path) > prefix_len)
            {
                char separator = node->path[prefix_len];
                matches = (separator == ':' || separator == '/');
            }
        }

        if (matches)
        {
            Remove(&node->node);
            workbench_free_path_node(node);
            removed = TRUE;
        }
        node = next;
    }

    return removed;
}

static BOOL workbench_build_list_copy(struct List *source, struct List **out_list)
{
    struct List *copy;
    struct lxa_workbench_path_node *node;

    if (!out_list)
        return FALSE;

    *out_list = NULL;

    copy = (struct List *)AllocMem(sizeof(*copy), MEMF_CLEAR | MEMF_PUBLIC);
    if (!copy)
        return FALSE;

    NEWLIST(copy);

    node = (struct lxa_workbench_path_node *)GETHEAD(source);
    while (node)
    {
        struct lxa_workbench_path_node *copy_node = workbench_add_path_node(copy, node->path);
        if (!copy_node)
        {
            workbench_free_path_list(copy);
            FreeMem(copy, sizeof(*copy));
            return FALSE;
        }

        copy_node->flags = node->flags;
        copy_node->modes = node->modes;
        node = (struct lxa_workbench_path_node *)GETSUCC(&node->node);
    }

    *out_list = copy;
    return TRUE;
}

static VOID workbench_free_list_copy(struct List *list)
{
    if (!list)
        return;

    workbench_free_path_list(list);
    FreeMem(list, sizeof(*list));
}

static BPTR workbench_duplicate_search_path(BPTR original)
{
    BPTR head = 0;
    BPTR tail = 0;
    BPTR *entry = (BPTR *)BADDR(original);

    while (entry)
    {
        BPTR dup_lock = entry[1] ? DupLock(entry[1]) : 0;
        BPTR *copy_entry = (BPTR *)AllocVec(sizeof(BPTR) * 2, MEMF_CLEAR);

        if (!copy_entry || (entry[1] && !dup_lock))
        {
            if (dup_lock)
                UnLock(dup_lock);

            while (head)
            {
                BPTR *next = (BPTR *)BADDR(((BPTR *)BADDR(head))[0]);
                if (((BPTR *)BADDR(head))[1])
                    UnLock(((BPTR *)BADDR(head))[1]);
                FreeVec(BADDR(head));
                head = next ? MKBADDR(next) : 0;
            }

            return 0;
        }

        copy_entry[0] = 0;
        copy_entry[1] = dup_lock;

        if (!head)
        {
            head = MKBADDR(copy_entry);
            tail = head;
        }
        else
        {
            ((BPTR *)BADDR(tail))[0] = MKBADDR(copy_entry);
            tail = MKBADDR(copy_entry);
        }

        entry = (BPTR *)BADDR(entry[0]);
    }

    return head;
}

static VOID workbench_free_search_path(BPTR path)
{
    BPTR *entry = (BPTR *)BADDR(path);

    while (entry)
    {
        BPTR *next = (BPTR *)BADDR(entry[0]);
        if (entry[1])
            UnLock(entry[1]);
        FreeVec(entry);
        entry = next;
    }
}

static BOOL workbench_resolve_name(CONST_STRPTR name,
                                   STRPTR full_path,
                                   ULONG full_path_size,
                                   BOOL *is_drawer)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    BOOL result = FALSE;

    if (!name || !full_path || full_path_size == 0)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    lock = Lock(name, SHARED_LOCK);
    if (!lock)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib)
    {
        UnLock(lock);
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }

    if (NameFromLock(lock, full_path, full_path_size) && Examine(lock, fib))
    {
        if (is_drawer)
            *is_drawer = (fib->fib_DirEntryType > 0);
        result = TRUE;
        SetIoErr(0);
    }
    else
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return result;
}

static BOOL workbench_build_path_from_lock(BPTR lock,
                                           CONST_STRPTR name,
                                           STRPTR full_path,
                                           ULONG full_path_size)
{
    char parent[512];

    if (!full_path || full_path_size == 0 || !name)
        return FALSE;

    if (!lock)
        return workbench_copy_string(name, full_path, full_path_size);

    if (!NameFromLock(lock, (STRPTR)parent, sizeof(parent)))
        return FALSE;

    return workbench_path_join((CONST_STRPTR)parent, name, full_path, full_path_size);
}

static VOID workbench_remove_related_drop_zones(struct WorkbenchBase *wbb,
                                                struct lxa_workbench_app_object *app_window)
{
    struct lxa_workbench_app_object *obj;
    struct lxa_workbench_app_object *next;

    obj = (struct lxa_workbench_app_object *)GETHEAD(&wbb->app_objects);
    while (obj)
    {
        next = (struct lxa_workbench_app_object *)GETSUCC(&obj->node);
        if (obj->type == LXA_WORKBENCH_APP_DROP_ZONE && obj->secondary == app_window)
        {
            Remove(&obj->node);
            workbench_free_string(obj->name);
            obj->magic = 0;
            FreeMem(obj, sizeof(*obj));
        }
        obj = next;
    }
}

static struct lxa_workbench_app_object *workbench_find_app_window_for_window(struct WorkbenchBase *wbb,
                                                                              struct Window *window)
{
    struct lxa_workbench_app_object *obj;

    if (!wbb || !window)
        return NULL;

    obj = (struct lxa_workbench_app_object *)GETHEAD(&wbb->app_objects);
    while (obj)
    {
        if (obj->type == LXA_WORKBENCH_APP_WINDOW && obj->primary == window)
            return obj;
        obj = (struct lxa_workbench_app_object *)GETSUCC(&obj->node);
    }

    return NULL;
}

static struct lxa_workbench_app_object *workbench_find_drop_zone_hit(struct WorkbenchBase *wbb,
                                                                      struct lxa_workbench_app_object *app_window,
                                                                      LONG x,
                                                                      LONG y)
{
    struct lxa_workbench_app_object *obj;

    obj = (struct lxa_workbench_app_object *)GETHEAD(&wbb->app_objects);
    while (obj)
    {
        if (obj->type == LXA_WORKBENCH_APP_DROP_ZONE && obj->secondary == app_window)
        {
            if (x >= obj->box.Left && y >= obj->box.Top &&
                x < obj->box.Left + obj->box.Width &&
                y < obj->box.Top + obj->box.Height)
                return obj;
        }

        obj = (struct lxa_workbench_app_object *)GETSUCC(&obj->node);
    }

    return NULL;
}

static VOID workbench_cleanup_base(struct WorkbenchBase *wbb)
{
    struct lxa_workbench_app_object *obj;
    struct lxa_workbench_app_object *next;
    struct lxa_workbench_list_owner *owner;
    struct lxa_workbench_list_owner *owner_next;

    if (!wbb)
        return;

    obj = (struct lxa_workbench_app_object *)GETHEAD(&wbb->app_objects);
    while (obj)
    {
        next = (struct lxa_workbench_app_object *)GETSUCC(&obj->node);
        Remove(&obj->node);
        workbench_free_string(obj->name);
        FreeMem(obj, sizeof(*obj));
        obj = next;
    }

    workbench_free_path_list(&wbb->open_drawers);
    workbench_free_path_list(&wbb->selected_icons);
    workbench_free_path_list(&wbb->hidden_devices);
    workbench_free_path_list(&wbb->program_list);

    owner = (struct lxa_workbench_list_owner *)wbb->setup_cleanup_hooks.lh_Head;
    while ((struct Node *)owner != (struct Node *)&wbb->setup_cleanup_hooks.lh_Tail)
    {
        owner_next = (struct lxa_workbench_list_owner *)owner->node.ln_Succ;
        Remove((struct Node *)owner);
        FreeMem(owner, sizeof(*owner));
        owner = owner_next;
    }
}

static VOID workbench_init_app_list(struct WorkbenchBase *wbb)
{
    if (!wbb)
        return;

    NEWLIST(&wbb->app_objects);
    NEWLIST(&wbb->open_drawers);
    NEWLIST(&wbb->selected_icons);
    NEWLIST(&wbb->hidden_devices);
    NEWLIST(&wbb->program_list);
    NEWLIST(&wbb->setup_cleanup_hooks);
    wbb->default_stack_size = 4096;
    wbb->type_restart_time = 3;
    wbb->global_flags = 0;
    wbb->copy_hook = NULL;
    wbb->delete_hook = NULL;
    wbb->text_input_hook = NULL;
    wbb->diskinfo_hook = NULL;
}

static struct lxa_workbench_app_object *workbench_validate_app_object(APTR handle,
                                                                      ULONG expected_type)
{
    struct lxa_workbench_app_object *obj = (struct lxa_workbench_app_object *)handle;

    if (!obj)
        return NULL;

    if (obj->magic != LXA_WORKBENCH_APP_MAGIC || obj->type != expected_type)
        return NULL;

    return obj;
}

static APTR workbench_alloc_app_object(struct WorkbenchBase *WorkbenchBase,
                                       ULONG type,
                                       ULONG id,
                                       ULONG userdata,
                                       APTR primary,
                                       APTR secondary,
                                       struct MsgPort *msgport,
                                       CONST_STRPTR name)
{
    struct lxa_workbench_app_object *obj;

    if (!WorkbenchBase)
        return NULL;

    obj = AllocMem(sizeof(*obj), MEMF_CLEAR | MEMF_PUBLIC);
    if (!obj)
        return NULL;

    obj->magic = LXA_WORKBENCH_APP_MAGIC;
    obj->type = type;
    obj->id = id;
    obj->userdata = userdata;
    obj->primary = primary;
    obj->secondary = secondary;
    obj->msgport = msgport;
    obj->name = workbench_strdup(name);

    if (name && !obj->name)
    {
        FreeMem(obj, sizeof(*obj));
        return NULL;
    }

    AddTail(&WorkbenchBase->app_objects, &obj->node);

    return obj;
}

static BOOL workbench_free_app_object(APTR handle, ULONG expected_type)
{
    struct lxa_workbench_app_object *obj = workbench_validate_app_object(handle, expected_type);

    if (!obj)
        return FALSE;

    Remove(&obj->node);
    workbench_free_string(obj->name);
    obj->magic = 0;
    FreeMem(obj, sizeof(*obj));
    return TRUE;
}

/*
 * Library init/open/close/expunge
 */

struct WorkbenchBase * __g_lxa_workbench_InitLib ( register struct WorkbenchBase *wbb __asm("d0"),
                                                   register BPTR                seglist __asm("a0"),
                                                   register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_workbench: InitLib() called\n");
    wbb->SegList = seglist;
    workbench_init_app_list(wbb);
    return wbb;
}

BPTR __g_lxa_workbench_ExpungeLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    workbench_cleanup_base(wbb);
    return 0;
}

struct WorkbenchBase * __g_lxa_workbench_OpenLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_workbench: OpenLib() called, wbb=0x%08lx\n", (ULONG)wbb);
    wbb->lib.lib_OpenCnt++;
    wbb->lib.lib_Flags &= ~LIBF_DELEXP;
    return wbb;
}

BPTR __g_lxa_workbench_CloseLib ( register struct WorkbenchBase *wbb __asm("a6") )
{
    DPRINTF (LOG_DEBUG, "_workbench: CloseLib() called, wbb=0x%08lx\n", (ULONG)wbb);
    wbb->lib.lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_workbench_ExtFuncLib ( void )
{
    PRIVATE_FUNCTION_ERROR("_workbench", "ExtFuncLib");
    return 0;
}

/*
 * Workbench Functions (V36+)
 */

/* UpdateWorkbench - Notify Workbench of changes to an object */
void _workbench_UpdateWorkbench ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register CONST_STRPTR name __asm("a0"),
                                  register BPTR lock __asm("a1"),
                                  register LONG action __asm("d0") )
{
    char full_path[512];

    action = (LONG)(WORD)action; /* sign-extend: GCC m68k move.w workaround */

    DPRINTF (LOG_DEBUG, "_workbench: UpdateWorkbench() name='%s', action=%ld\n",
             STRORNULL(name), action);

    if (!WorkbenchBase || !name ||
        (action != UPDATEWB_ObjectAdded && action != UPDATEWB_ObjectRemoved))
        return;

    if (!workbench_build_path_from_lock(lock, name, (STRPTR)full_path, sizeof(full_path)))
        return;

    if (action == UPDATEWB_ObjectRemoved)
    {
        workbench_remove_path_node(&WorkbenchBase->open_drawers, (CONST_STRPTR)full_path);
        workbench_remove_path_node(&WorkbenchBase->program_list, (CONST_STRPTR)full_path);
        workbench_remove_path_node(&WorkbenchBase->selected_icons, (CONST_STRPTR)full_path);
    }
}

/* AddAppWindowA - Add an AppWindow (app that receives dropped icons) */
APTR _workbench_AddAppWindowA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                register ULONG id __asm("d0"),
                                register ULONG userdata __asm("d1"),
                                register struct Window *window __asm("a0"),
                                register struct MsgPort *msgport __asm("a1"),
                                register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppWindowA() id=%ld, window=0x%08lx\n",
             id, (ULONG)window);

    if (!window || !msgport)
        return NULL;

    return workbench_alloc_app_object(WorkbenchBase,
                                      LXA_WORKBENCH_APP_WINDOW,
                                      id,
                                      userdata,
                                      window,
                                      taglist,
                                      msgport,
                                      NULL);
}

/* RemoveAppWindow - Remove an AppWindow */
BOOL _workbench_RemoveAppWindow ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register APTR appWindow __asm("a0") )
{
    struct lxa_workbench_app_object *obj;

    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppWindow() appWindow=0x%08lx\n", (ULONG)appWindow);

    obj = workbench_validate_app_object(appWindow, LXA_WORKBENCH_APP_WINDOW);
    if (!obj)
        return FALSE;

    workbench_remove_related_drop_zones(WorkbenchBase, obj);
    return workbench_free_app_object(appWindow, LXA_WORKBENCH_APP_WINDOW);
}

/* AddAppIconA - Add an AppIcon to Workbench */
APTR _workbench_AddAppIconA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                              register ULONG id __asm("d0"),
                              register ULONG userdata __asm("d1"),
                              register CONST_STRPTR text __asm("a0"),
                              register struct MsgPort *msgport __asm("a1"),
                              register BPTR lock __asm("a2"),
                              register APTR diskobj __asm("a3"),
                              register struct TagItem *taglist __asm("a4") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppIconA() id=%ld, text='%s'\n",
             id, text ? (char *)text : "(null)");

    if (!msgport || !diskobj)
        return NULL;

    return workbench_alloc_app_object(WorkbenchBase,
                                      LXA_WORKBENCH_APP_ICON,
                                      id,
                                      userdata,
                                      diskobj,
                                      (APTR)lock,
                                      msgport,
                                      text);
}

/* RemoveAppIcon - Remove an AppIcon */
BOOL _workbench_RemoveAppIcon ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                register APTR appIcon __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppIcon() appIcon=0x%08lx\n", (ULONG)appIcon);
    return workbench_free_app_object(appIcon, LXA_WORKBENCH_APP_ICON);
}

/* AddAppMenuItemA - Add an item to Workbench Tools menu */
APTR _workbench_AddAppMenuItemA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                  register ULONG id __asm("d0"),
                                  register ULONG userdata __asm("d1"),
                                  register CONST_STRPTR text __asm("a0"),
                                  register struct MsgPort *msgport __asm("a1"),
                                  register struct TagItem *taglist __asm("a2") )
{
    DPRINTF (LOG_DEBUG, "_workbench: AddAppMenuItemA() id=%ld, text='%s'\n",
             id, text ? (char *)text : "(null)");

    if (!text || !msgport)
        return NULL;

    return workbench_alloc_app_object(WorkbenchBase,
                                      LXA_WORKBENCH_APP_MENU,
                                      id,
                                      userdata,
                                      (APTR)text,
                                      taglist,
                                      msgport,
                                      text);
}

/* RemoveAppMenuItem - Remove a menu item */
BOOL _workbench_RemoveAppMenuItem ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                    register APTR appMenuItem __asm("a0") )
{
    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppMenuItem() appMenuItem=0x%08lx\n", (ULONG)appMenuItem);
    return workbench_free_app_object(appMenuItem, LXA_WORKBENCH_APP_MENU);
}

/*
 * Workbench Functions (V39+)
 */

/* WBInfo - Display info requester for an object */
ULONG _workbench_WBInfo ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                          register BPTR lock __asm("a0"),
                          register CONST_STRPTR name __asm("a1"),
                          register struct Screen *screen __asm("a2") )
{
    char full_path[512];
    struct IntuiText body_text;
    struct IntuiText ok_text;

    DPRINTF (LOG_DEBUG, "_workbench: WBInfo() name='%s'\n", STRORNULL(name));

    (void)WorkbenchBase;
    (void)screen;

    if (!screen || !name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!workbench_build_path_from_lock(lock, name, (STRPTR)full_path, sizeof(full_path)))
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    body_text.FrontPen = 1;
    body_text.BackPen = 0;
    body_text.DrawMode = JAM1;
    body_text.LeftEdge = 8;
    body_text.TopEdge = 4;
    body_text.ITextFont = NULL;
    body_text.IText = (UBYTE *)full_path;
    body_text.NextText = NULL;

    ok_text.FrontPen = 1;
    ok_text.BackPen = 0;
    ok_text.DrawMode = JAM1;
    ok_text.LeftEdge = 0;
    ok_text.TopEdge = 0;
    ok_text.ITextFont = NULL;
    ok_text.IText = (UBYTE *)"OK";
    ok_text.NextText = NULL;

    {
        struct Window *requester;

        requester = BuildSysRequest((struct Window *)screen->FirstWindow,
                                    &body_text,
                                    &ok_text,
                                    NULL,
                                    0,
                                    320,
                                    70);
        if (!requester)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }

        FreeSysRequest(requester);
    }

    SetIoErr(0);
    return TRUE;
}

/*
 * Workbench Functions (V44+)
 */

/* OpenWorkbenchObjectA - Open an object as Workbench would */
BOOL _workbench_OpenWorkbenchObjectA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                       register CONST_STRPTR name __asm("a0"),
                                       register struct TagItem *tags __asm("a1") )
{
    char full_path[512];
    BOOL is_drawer;

    DPRINTF (LOG_DEBUG, "_workbench: OpenWorkbenchObjectA() name='%s'\n", STRORNULL(name));

    if (!WorkbenchBase || !name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!workbench_resolve_name(name, (STRPTR)full_path, sizeof(full_path), &is_drawer))
        return FALSE;

    if (is_drawer)
    {
        struct lxa_workbench_path_node *node = workbench_add_path_node(&WorkbenchBase->open_drawers,
                                                                       (CONST_STRPTR)full_path);
        ULONG show_mode;
        ULONG view_mode;

        if (!node)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }

        show_mode = GetTagData(WBOPENA_Show, DDFLAGS_SHOWDEFAULT, tags);
        view_mode = GetTagData(WBOPENA_ViewBy, DDVM_BYDEFAULT, tags);

        if (show_mode >= DDFLAGS_SHOWDEFAULT && show_mode <= DDFLAGS_SHOWALL)
            node->flags = show_mode;
        if (view_mode >= DDVM_BYDEFAULT && view_mode <= DDVM_BYTYPE)
            node->modes = view_mode;

        SetIoErr(0);
        return TRUE;
    }

    {
        struct TagItem sys_tags[] = {
            { SYS_Asynch, TRUE },
            { TAG_DONE, 0 }
        };

        if (SystemTagList(name, sys_tags) == -1)
        {
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
            return FALSE;
        }

        if (!workbench_add_path_node(&WorkbenchBase->program_list, (CONST_STRPTR)full_path))
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }
    }

    SetIoErr(0);
    return TRUE;
}

/* CloseWorkbenchObjectA - Close an opened Workbench object */
BOOL _workbench_CloseWorkbenchObjectA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                        register CONST_STRPTR name __asm("a0"),
                                        register struct TagItem *tags __asm("a1") )
{
    char full_path[512];
    BOOL is_drawer = FALSE;

    (void)tags;

    DPRINTF (LOG_DEBUG, "_workbench: CloseWorkbenchObjectA() name='%s'\n", STRORNULL(name));

    if (!WorkbenchBase || !name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!workbench_resolve_name(name, (STRPTR)full_path, sizeof(full_path), &is_drawer))
        return FALSE;

    if (workbench_remove_path_node(&WorkbenchBase->open_drawers, (CONST_STRPTR)full_path))
    {
        workbench_remove_selected_prefix(WorkbenchBase, (CONST_STRPTR)full_path);
        SetIoErr(0);
        return TRUE;
    }

    if (workbench_remove_path_node(&WorkbenchBase->program_list, (CONST_STRPTR)full_path))
    {
        SetIoErr(0);
        return TRUE;
    }

    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
}

BOOL _workbench_WorkbenchControlA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                    register CONST_STRPTR name __asm("a0"),
                                    register CONST struct TagItem *tags __asm("a1") )
{
    const struct TagItem *state = tags;

    DPRINTF (LOG_DEBUG, "_workbench: WorkbenchControlA() name='%s'\n", STRORNULL(name));

    if (!WorkbenchBase)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    while (state)
    {
        const struct TagItem *tag = NextTagItem((struct TagItem **)&state);
        if (!tag)
            break;

        switch (tag->ti_Tag)
        {
            case WBCTRLA_IsOpen:
            {
                LONG *value = (LONG *)tag->ti_Data;
                char full_path[512];
                BOOL is_drawer = FALSE;

                if (!value || !name || !workbench_resolve_name(name, (STRPTR)full_path, sizeof(full_path), &is_drawer))
                {
                    SetIoErr(ERROR_OBJECT_NOT_FOUND);
                    return FALSE;
                }

                *value = workbench_find_path_node(&WorkbenchBase->open_drawers,
                                                  (CONST_STRPTR)full_path) ? TRUE : FALSE;
                break;
            }

            case WBCTRLA_DuplicateSearchPath:
            {
                BPTR *out_path = (BPTR *)tag->ti_Data;
                struct Process *me = U_getCurrentProcess();
                struct CommandLineInterface *cli = me && me->pr_CLI
                    ? (struct CommandLineInterface *)BADDR(me->pr_CLI) : NULL;

                if (!out_path)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }

                *out_path = cli ? workbench_duplicate_search_path(cli->cli_CommandDir) : 0;
                if (cli && cli->cli_CommandDir && !*out_path)
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;
            }

            case WBCTRLA_FreeSearchPath:
                workbench_free_search_path((BPTR)tag->ti_Data);
                break;

            case WBCTRLA_GetDefaultStackSize:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(ULONG *)tag->ti_Data = WorkbenchBase->default_stack_size;
                break;

            case WBCTRLA_SetDefaultStackSize:
                WorkbenchBase->default_stack_size = tag->ti_Data < 4096 ? 4096 : tag->ti_Data;
                break;

            case WBCTRLA_RedrawAppIcon:
                if (!workbench_validate_app_object((APTR)tag->ti_Data, LXA_WORKBENCH_APP_ICON))
                {
                    SetIoErr(ERROR_OBJECT_NOT_FOUND);
                    return FALSE;
                }
                break;

            case WBCTRLA_GetProgramList:
                if (!workbench_build_list_copy(&WorkbenchBase->program_list, (struct List **)tag->ti_Data))
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;

            case WBCTRLA_FreeProgramList:
                workbench_free_list_copy((struct List *)tag->ti_Data);
                break;

            case WBCTRLA_GetSelectedIconList:
                if (!workbench_build_list_copy(&WorkbenchBase->selected_icons, (struct List **)tag->ti_Data))
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;

            case WBCTRLA_FreeSelectedIconList:
                workbench_free_list_copy((struct List *)tag->ti_Data);
                break;

            case WBCTRLA_GetOpenDrawerList:
                if (!workbench_build_list_copy(&WorkbenchBase->open_drawers, (struct List **)tag->ti_Data))
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;

            case WBCTRLA_FreeOpenDrawerList:
                workbench_free_list_copy((struct List *)tag->ti_Data);
                break;

            case WBCTRLA_GetHiddenDeviceList:
                if (!workbench_build_list_copy(&WorkbenchBase->hidden_devices, (struct List **)tag->ti_Data))
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;

            case WBCTRLA_FreeHiddenDeviceList:
                workbench_free_list_copy((struct List *)tag->ti_Data);
                break;

            case WBCTRLA_AddHiddenDeviceName:
            {
                CONST_STRPTR device_name = (CONST_STRPTR)tag->ti_Data;
                ULONG len = workbench_strlen(device_name);

                if (!device_name || len == 0 || device_name[len - 1] != ':')
                {
                    SetIoErr(ERROR_BAD_NUMBER);
                    return FALSE;
                }

                if (!workbench_add_path_node(&WorkbenchBase->hidden_devices, device_name))
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }
                break;
            }

            case WBCTRLA_RemoveHiddenDeviceName:
                if (tag->ti_Data)
                    workbench_remove_path_node(&WorkbenchBase->hidden_devices, (CONST_STRPTR)tag->ti_Data);
                break;

            case WBCTRLA_GetTypeRestartTime:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(ULONG *)tag->ti_Data = WorkbenchBase->type_restart_time;
                break;

            case WBCTRLA_SetTypeRestartTime:
                if (tag->ti_Data == 0)
                {
                    SetIoErr(ERROR_BAD_NUMBER);
                    return FALSE;
                }
                WorkbenchBase->type_restart_time = tag->ti_Data;
                break;

            case WBCTRLA_GetCopyHook:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(struct Hook **)tag->ti_Data = WorkbenchBase->copy_hook;
                break;

            case WBCTRLA_SetCopyHook:
                WorkbenchBase->copy_hook = (struct Hook *)tag->ti_Data;
                break;

            case WBCTRLA_GetDeleteHook:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(struct Hook **)tag->ti_Data = WorkbenchBase->delete_hook;
                break;

            case WBCTRLA_SetDeleteHook:
                WorkbenchBase->delete_hook = (struct Hook *)tag->ti_Data;
                break;

            case WBCTRLA_GetTextInputHook:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(struct Hook **)tag->ti_Data = WorkbenchBase->text_input_hook;
                break;

            case WBCTRLA_SetTextInputHook:
                WorkbenchBase->text_input_hook = (struct Hook *)tag->ti_Data;
                break;

            case WBCTRLA_AddSetupCleanupHook:
            {
                struct lxa_workbench_list_owner *owner;

                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }

                owner = (struct lxa_workbench_list_owner *)AllocMem(sizeof(*owner), MEMF_CLEAR | MEMF_PUBLIC);
                if (!owner)
                {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    return FALSE;
                }

                owner->hook = (struct Hook *)tag->ti_Data;
                AddTail((struct List *)&WorkbenchBase->setup_cleanup_hooks, (struct Node *)owner);
                break;
            }

            case WBCTRLA_RemSetupCleanupHook:
            {
                struct lxa_workbench_list_owner *owner = (struct lxa_workbench_list_owner *)WorkbenchBase->setup_cleanup_hooks.lh_Head;
                while ((struct Node *)owner != (struct Node *)&WorkbenchBase->setup_cleanup_hooks.lh_Tail)
                {
                    if (owner->hook == (struct Hook *)tag->ti_Data)
                    {
                        Remove((struct Node *)owner);
                        FreeMem(owner, sizeof(*owner));
                        owner = NULL;
                        break;
                    }
                    owner = (struct lxa_workbench_list_owner *)owner->node.ln_Succ;
                }

                if (owner)
                {
                    SetIoErr(ERROR_OBJECT_NOT_FOUND);
                    return FALSE;
                }
                break;
            }

            case WBCTRLA_SetGlobalFlags:
                WorkbenchBase->global_flags = tag->ti_Data;
                break;

            case WBCTRLA_GetGlobalFlags:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(ULONG *)tag->ti_Data = WorkbenchBase->global_flags;
                break;

            case WBCTRLA_GetDiskInfoHook:
                if (!tag->ti_Data)
                {
                    SetIoErr(ERROR_REQUIRED_ARG_MISSING);
                    return FALSE;
                }
                *(struct Hook **)tag->ti_Data = WorkbenchBase->diskinfo_hook;
                break;

            case WBCTRLA_SetDiskInfoHook:
                WorkbenchBase->diskinfo_hook = (struct Hook *)tag->ti_Data;
                break;

            default:
                SetIoErr(ERROR_BAD_NUMBER);
                return FALSE;
        }
    }

    SetIoErr(0);
    return TRUE;
}

struct AppWindowDropZone * _workbench_AddAppWindowDropZoneA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                                               register struct AppWindow *aw __asm("a0"),
                                                               register ULONG id __asm("d0"),
                                                               register ULONG userdata __asm("d1"),
                                                               register CONST struct TagItem *tags __asm("a1") )
{
    struct lxa_workbench_app_object *app_window;
    struct lxa_workbench_app_object *zone;
    struct Window *window;
    struct IBox *box;
    WORD left;
    WORD top;
    WORD width;
    WORD height;

    DPRINTF (LOG_DEBUG, "_workbench: AddAppWindowDropZoneA() aw=0x%08lx id=%ld\n",
             (ULONG)aw, id);

    app_window = workbench_validate_app_object((APTR)aw, LXA_WORKBENCH_APP_WINDOW);
    if (!WorkbenchBase || !app_window)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return NULL;
    }

    window = (struct Window *)app_window->primary;
    if (!window)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return NULL;
    }

    box = (struct IBox *)GetTagData(WBDZA_Box, 0, tags);
    if (box)
    {
        left = box->Left;
        top = box->Top;
        width = box->Width;
        height = box->Height;
    }
    else
    {
        BOOL have_left = FindTagItem(WBDZA_Left, (struct TagItem *)tags) != NULL ||
                         FindTagItem(WBDZA_RelRight, (struct TagItem *)tags) != NULL;
        BOOL have_top = FindTagItem(WBDZA_Top, (struct TagItem *)tags) != NULL ||
                        FindTagItem(WBDZA_RelBottom, (struct TagItem *)tags) != NULL;
        BOOL have_width = FindTagItem(WBDZA_Width, (struct TagItem *)tags) != NULL ||
                          FindTagItem(WBDZA_RelWidth, (struct TagItem *)tags) != NULL;
        BOOL have_height = FindTagItem(WBDZA_Height, (struct TagItem *)tags) != NULL ||
                           FindTagItem(WBDZA_RelHeight, (struct TagItem *)tags) != NULL;

        if (!have_left || !have_top || !have_width || !have_height)
        {
            SetIoErr(ERROR_REQUIRED_ARG_MISSING);
            return NULL;
        }

        left = (WORD)GetTagData(WBDZA_Left,
                                (ULONG)(window->Width + (WORD)GetTagData(WBDZA_RelRight, 0, tags)),
                                tags);
        top = (WORD)GetTagData(WBDZA_Top,
                               (ULONG)(window->Height + (WORD)GetTagData(WBDZA_RelBottom, 0, tags)),
                               tags);
        width = (WORD)GetTagData(WBDZA_Width,
                                 (ULONG)(window->Width + (WORD)GetTagData(WBDZA_RelWidth, 0, tags)),
                                 tags);
        height = (WORD)GetTagData(WBDZA_Height,
                                  (ULONG)(window->Height + (WORD)GetTagData(WBDZA_RelHeight, 0, tags)),
                                  tags);
    }

    if (width <= 0 || height <= 0 || left < 0 || top < 0 ||
        left + width > window->Width || top + height > window->Height)
    {
        SetIoErr(ERROR_BAD_NUMBER);
        return NULL;
    }

    zone = (struct lxa_workbench_app_object *)workbench_alloc_app_object(WorkbenchBase,
                                                                          LXA_WORKBENCH_APP_DROP_ZONE,
                                                                          id,
                                                                          userdata,
                                                                          app_window,
                                                                          app_window,
                                                                          app_window->msgport,
                                                                          NULL);
    if (!zone)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }

    zone->box.Left = left;
    zone->box.Top = top;
    zone->box.Width = width;
    zone->box.Height = height;
    zone->hook = (struct Hook *)GetTagData(WBDZA_Hook, 0, tags);
    SetIoErr(0);
    return (struct AppWindowDropZone *)zone;
}

BOOL _workbench_RemoveAppWindowDropZone ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                          register struct AppWindow *aw __asm("a0"),
                                          register struct AppWindowDropZone *dropZone __asm("a1") )
{
    struct lxa_workbench_app_object *app_window;
    struct lxa_workbench_app_object *zone;

    DPRINTF (LOG_DEBUG, "_workbench: RemoveAppWindowDropZone() aw=0x%08lx dropZone=0x%08lx\n",
             (ULONG)aw, (ULONG)dropZone);

    app_window = workbench_validate_app_object((APTR)aw, LXA_WORKBENCH_APP_WINDOW);
    zone = workbench_validate_app_object((APTR)dropZone, LXA_WORKBENCH_APP_DROP_ZONE);
    if (!app_window || !zone || zone->secondary != app_window)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    Remove(&zone->node);
    workbench_free_string(zone->name);
    zone->magic = 0;
    FreeMem(zone, sizeof(*zone));
    SetIoErr(0);
    return TRUE;
}

BOOL _workbench_ChangeWorkbenchSelectionA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                            register CONST_STRPTR name __asm("a0"),
                                            register struct Hook *hook __asm("a1"),
                                            register CONST struct TagItem *tags __asm("a2") )
{
    char drawer_path[512];
    BPTR lock;
    struct FileInfoBlock *fib;

    DPRINTF (LOG_DEBUG, "_workbench: ChangeWorkbenchSelectionA() name='%s'\n", STRORNULL(name));

    if (!WorkbenchBase || !name || !hook)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!workbench_resolve_name(name, (STRPTR)drawer_path, sizeof(drawer_path), NULL) ||
        !workbench_find_path_node(&WorkbenchBase->open_drawers, (CONST_STRPTR)drawer_path))
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    lock = Lock(name, SHARED_LOCK);
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!lock || !fib)
    {
        if (lock)
            UnLock(lock);
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }

    if (!Examine(lock, fib) || fib->fib_DirEntryType <= 0)
    {
        FreeDosObject(DOS_FIB, fib);
        UnLock(lock);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }

    while (ExNext(lock, fib))
    {
        struct IconSelectMsg ism;
        ULONG action;
        char object_path[512];
        BOOL selected;

        if (!workbench_path_join((CONST_STRPTR)drawer_path,
                                 (CONST_STRPTR)fib->fib_FileName,
                                 (STRPTR)object_path,
                                 sizeof(object_path)))
            continue;

        selected = workbench_find_path_node(&WorkbenchBase->selected_icons,
                                            (CONST_STRPTR)object_path) != NULL;

        memset(&ism, 0, sizeof(ism));
        ism.ism_Length = sizeof(ism);
        ism.ism_Drawer = lock;
        ism.ism_Name = fib->fib_FileName;
        ism.ism_Type = (fib->fib_DirEntryType > 0) ? WBDRAWER : WBTOOL;
        ism.ism_Selected = selected;
        ism.ism_Tags = (struct TagItem *)tags;

        action = CallHookPkt(hook, NULL, &ism);

        if (action == ISMACTION_Select)
        {
            if (!workbench_add_path_node(&WorkbenchBase->selected_icons, (CONST_STRPTR)object_path))
            {
                FreeDosObject(DOS_FIB, fib);
                UnLock(lock);
                SetIoErr(ERROR_NO_FREE_STORE);
                return FALSE;
            }
        }
        else if (action == ISMACTION_Unselect)
        {
            workbench_remove_path_node(&WorkbenchBase->selected_icons, (CONST_STRPTR)object_path);
        }
        else if (action == ISMACTION_Stop)
        {
            break;
        }
    }

    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    SetIoErr(0);
    return TRUE;
}

BOOL _workbench_MakeWorkbenchObjectVisibleA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                              register CONST_STRPTR name __asm("a0"),
                                              register CONST struct TagItem *tags __asm("a1") )
{
    char full_path[512];
    char parent_path[512];
    char leaf_name[256];
    BOOL is_drawer = FALSE;

    (void)tags;
    (void)leaf_name;

    DPRINTF (LOG_DEBUG, "_workbench: MakeWorkbenchObjectVisibleA() name='%s'\n", STRORNULL(name));

    if (!WorkbenchBase || !name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return FALSE;
    }

    if (!workbench_resolve_name(name, (STRPTR)full_path, sizeof(full_path), &is_drawer))
        return FALSE;

    if (is_drawer)
    {
        if (workbench_find_path_node(&WorkbenchBase->open_drawers, (CONST_STRPTR)full_path))
        {
            SetIoErr(0);
            return TRUE;
        }
    }
    else if (workbench_parent_from_path((CONST_STRPTR)full_path,
                                        (STRPTR)parent_path,
                                        sizeof(parent_path),
                                        (STRPTR)leaf_name,
                                        sizeof(leaf_name)) &&
             workbench_find_path_node(&WorkbenchBase->open_drawers, (CONST_STRPTR)parent_path))
    {
        SetIoErr(0);
        return TRUE;
    }

    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return FALSE;
}

ULONG _workbench_WhichWorkbenchObjectA ( register struct WorkbenchBase *WorkbenchBase __asm("a6"),
                                         register struct Window *window __asm("a0"),
                                         register LONG mousex __asm("d0"),
                                         register LONG mousey __asm("d1"),
                                         register CONST struct TagItem *tags __asm("a1") )
{
    struct lxa_workbench_app_object *app_window;
    struct lxa_workbench_app_object *zone;
    ULONG name_size = GetTagData(WBOBJA_NameSize, 64, tags);
    ULONG path_size = GetTagData(WBOBJA_FullPathSize, 512, tags);
    ULONG drawer_path_size = GetTagData(WBOBJA_DrawerPathSize, 64, tags);
    STRPTR name_buf = (STRPTR)GetTagData(WBOBJA_Name, 0, tags);
    STRPTR full_path_buf = (STRPTR)GetTagData(WBOBJA_FullPath, 0, tags);
    STRPTR drawer_path_buf = (STRPTR)GetTagData(WBOBJA_DrawerPath, 0, tags);
    ULONG *type_out = (ULONG *)GetTagData(WBOBJA_Type, 0, tags);
    LONG *left_out = (LONG *)GetTagData(WBOBJA_Left, 0, tags);
    LONG *top_out = (LONG *)GetTagData(WBOBJA_Top, 0, tags);
    ULONG *width_out = (ULONG *)GetTagData(WBOBJA_Width, 0, tags);
    ULONG *height_out = (ULONG *)GetTagData(WBOBJA_Height, 0, tags);
    ULONG *state_out = (ULONG *)GetTagData(WBOBJA_State, 0, tags);
    ULONG *is_fake_out = (ULONG *)GetTagData(WBOBJA_IsFake, 0, tags);
    ULONG *is_link_out = (ULONG *)GetTagData(WBOBJA_IsLink, 0, tags);
    ULONG *drawer_flags_out = (ULONG *)GetTagData(WBOBJA_DrawerFlags, 0, tags);
    ULONG *drawer_modes_out = (ULONG *)GetTagData(WBOBJA_DrawerModes, 0, tags);

    mousex = (LONG)(WORD)mousex; /* sign-extend: GCC m68k move.w workaround */
    mousey = (LONG)(WORD)mousey;

    DPRINTF (LOG_DEBUG, "_workbench: WhichWorkbenchObjectA() window=0x%08lx mouse=%ld,%ld\n",
             (ULONG)window, mousex, mousey);

    if (!WorkbenchBase || !window || mousex < 0 || mousey < 0 ||
        mousex >= window->Width || mousey >= window->Height)
        return WBO_NONE;

    app_window = workbench_find_app_window_for_window(WorkbenchBase, window);
    if (!app_window)
        return WBO_NONE;

    zone = workbench_find_drop_zone_hit(WorkbenchBase, app_window, mousex, mousey);
    if (name_buf && name_size > 0)
        name_buf[0] = '\0';
    if (full_path_buf && path_size > 0)
        full_path_buf[0] = '\0';
    if (drawer_path_buf && drawer_path_size > 0)
        drawer_path_buf[0] = '\0';
    if (drawer_flags_out)
        *drawer_flags_out = 0;
    if (drawer_modes_out)
        *drawer_modes_out = 0;

    if (zone)
    {
        if (type_out)
            *type_out = WBAPPICON;
        if (left_out)
            *left_out = zone->box.Left;
        if (top_out)
            *top_out = zone->box.Top;
        if (width_out)
            *width_out = zone->box.Width;
        if (height_out)
            *height_out = zone->box.Height;
        if (state_out)
            *state_out = IDS_NORMAL;
        if (is_fake_out)
            *is_fake_out = TRUE;
        if (is_link_out)
            *is_link_out = FALSE;
        return WBO_ICON;
    }

    if (type_out)
        *type_out = WBDRAWER;
    if (left_out)
        *left_out = 0;
    if (top_out)
        *top_out = 0;
    if (width_out)
        *width_out = window->Width;
    if (height_out)
        *height_out = window->Height;
    if (state_out)
        *state_out = IDS_NORMAL;
    if (is_fake_out)
        *is_fake_out = TRUE;
    if (is_link_out)
        *is_link_out = FALSE;
    return WBO_DRAWER;
}

/* Reserved/Private functions */
static ULONG _workbench_Private1 ( void ) { PRIVATE_FUNCTION_ERROR("_workbench", "Private1"); return 0; }
static ULONG _workbench_Private2 ( void ) { PRIVATE_FUNCTION_ERROR("_workbench", "Private2"); return 0; }
static ULONG _workbench_Private3 ( void ) { PRIVATE_FUNCTION_ERROR("_workbench", "Private3"); return 0; }

/*
 * Library structure definitions
 */

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

extern APTR              __g_lxa_workbench_FuncTab [];
extern struct MyDataInit __g_lxa_workbench_DataTab;
extern struct InitTable  __g_lxa_workbench_InitTab;
extern APTR              __g_lxa_workbench_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                        // UWORD rt_MatchWord
    &ROMTag,                              // struct Resident *rt_MatchTag
    &__g_lxa_workbench_EndResident,       // APTR  rt_EndSkip
    RTF_AUTOINIT,                         // UBYTE rt_Flags
    VERSION,                              // UBYTE rt_Version
    NT_LIBRARY,                           // UBYTE rt_Type
    0,                                    // BYTE  rt_Pri
    &_g_workbench_ExLibName[0],           // char  *rt_Name
    &_g_workbench_ExLibID[0],             // char  *rt_IdString
    &__g_lxa_workbench_InitTab            // APTR  rt_Init
};

APTR __g_lxa_workbench_EndResident;
struct Resident *__lxa_workbench_ROMTag = &ROMTag;

struct InitTable __g_lxa_workbench_InitTab =
{
    (ULONG)               sizeof(struct WorkbenchBase),
    (APTR              *) &__g_lxa_workbench_FuncTab[0],
    (APTR)                &__g_lxa_workbench_DataTab,
    (APTR)                __g_lxa_workbench_InitLib
};

/* Function table - based on wb_protos.h
 * Standard library functions at -6 through -24
 * First real function at -30
 */
APTR __g_lxa_workbench_FuncTab [] =
{
    __g_lxa_workbench_OpenLib,           // -6   Standard
    __g_lxa_workbench_CloseLib,          // -12  Standard
    __g_lxa_workbench_ExpungeLib,        // -18  Standard
    __g_lxa_workbench_ExtFuncLib,        // -24  Standard (reserved)
    _workbench_UpdateWorkbench,          // -30  UpdateWorkbench (V36)
    _workbench_Private1,                 // -36  Private (V36)
    _workbench_Private2,                 // -42  Private (V36)
    _workbench_AddAppWindowA,            // -48  AddAppWindowA (V36)
    _workbench_RemoveAppWindow,          // -54  RemoveAppWindow (V36)
    _workbench_AddAppIconA,              // -60  AddAppIconA (V36)
    _workbench_RemoveAppIcon,            // -66  RemoveAppIcon (V36)
    _workbench_AddAppMenuItemA,          // -72  AddAppMenuItemA (V36)
    _workbench_RemoveAppMenuItem,        // -78  RemoveAppMenuItem (V36)
    _workbench_Private3,                 // -84  Private (V39)
    _workbench_WBInfo,                   // -90  WBInfo (V39)
    _workbench_OpenWorkbenchObjectA,     // -96  OpenWorkbenchObjectA (V44)
    _workbench_CloseWorkbenchObjectA,    // -102 CloseWorkbenchObjectA (V44)
    _workbench_WorkbenchControlA,        // -108 WorkbenchControlA (V44)
    _workbench_AddAppWindowDropZoneA,    // -114 AddAppWindowDropZoneA (V44)
    _workbench_RemoveAppWindowDropZone,  // -120 RemoveAppWindowDropZone (V44)
    _workbench_ChangeWorkbenchSelectionA,// -126 ChangeWorkbenchSelectionA (V44)
    _workbench_MakeWorkbenchObjectVisibleA, // -132 MakeWorkbenchObjectVisibleA (V44)
    _workbench_WhichWorkbenchObjectA,    // -138 WhichWorkbenchObjectA (V47)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_workbench_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_workbench_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_workbench_ExLibID[0],
    (ULONG) 0
};
