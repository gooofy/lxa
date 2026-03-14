#include <exec/types.h>
#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <exec/resident.h>
#include <exec/semaphores.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <utility/name.h>
#include <utility/utility.h>
#include <utility/pack.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

//#define ENABLE_DEBUG
#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "utility"
#define EXLIBVER   " 40.1 (2022/03/02)"

char __aligned _g_utility_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_utility_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_utility_Copyright [] = "(C)opyright 2022, 2023 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_utility_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

// libBase: UtilityBase
// baseType: struct UtilityBase *
// libname: utility.library

struct UtilityNameSpace
{
    struct MinList         list;
    struct SignalSemaphore lock;
    ULONG                  flags;
    struct UtilityNamedObject *cached_object;
};

struct UtilityNamedObject
{
    struct NamedObject            named_object;
    struct Node                   node;
    struct UtilityNameSpace      *parent_space;
    struct UtilityNameSpace      *child_space;
    struct Message               *free_message;
    UWORD                         use_count;
    BOOL                          free_object;
};

struct LXAUtilityRootBase
{
    struct UtilityBase      utility_base;
    ULONG                   last_id;
    struct UtilityNameSpace root_space;
};

static const ULONG utility_day_table[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335
};

union utility_memaccess
{
    UBYTE ub;
    UWORD uw;
    ULONG ul;
    BYTE  sb;
    WORD  sw;
    LONG  sl;
};

static VOID utility_new_min_list(struct MinList *list)
{
    list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
    list->mlh_Tail = NULL;
    list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
}

static struct UtilityNameSpace *utility_root_space(struct UtilityBase *UtilityBase)
{
    return &((struct LXAUtilityRootBase *)UtilityBase)->root_space;
}

static struct UtilityNamedObject *utility_named_object(struct NamedObject *object)
{
    return (struct UtilityNamedObject *)object;
}

static struct UtilityNamedObject *utility_named_object_from_node(struct Node *node)
{
    ULONG offset = (ULONG)&((struct UtilityNamedObject *)0)->node;

    if (!node || !node->ln_Succ)
        return NULL;

    return (struct UtilityNamedObject *)((UBYTE *)node - offset);
}

static struct UtilityNameSpace *utility_get_namespace(struct UtilityBase *UtilityBase,
                                                      struct NamedObject *name_space)
{
    if (name_space)
        return utility_named_object(name_space)->child_space;

    return utility_root_space(UtilityBase);
}

static BOOL utility_names_equal(struct UtilityBase *UtilityBase,
                                struct UtilityNameSpace *name_space,
                                CONST_STRPTR lhs,
                                CONST_STRPTR rhs)
{
    UBYTE c1;
    UBYTE c2;

    (void)UtilityBase;

    if (!lhs || !rhs)
        return FALSE;

    if (name_space->flags & NSF_CASE)
        return strcmp((const char *)lhs, (const char *)rhs) == 0;

    while (*lhs && *rhs)
    {
        c1 = ToLower(*lhs++);
        c2 = ToLower(*rhs++);

        if (c1 != c2)
            return FALSE;
    }

    return *lhs == *rhs;
}

static struct UtilityNamedObject *utility_find_named_object_locked(struct UtilityBase *UtilityBase,
                                                                   struct UtilityNameSpace *name_space,
                                                                   CONST_STRPTR name,
                                                                   struct UtilityNamedObject *last_object)
{
    struct Node *node;
    struct UtilityNamedObject *current;

    if (!name_space)
        return NULL;

    if (!last_object && name && name_space->cached_object &&
        name_space->cached_object->parent_space == name_space &&
        utility_names_equal(UtilityBase,
                            name_space,
                            (CONST_STRPTR)name_space->cached_object->node.ln_Name,
                            name))
    {
        return name_space->cached_object;
    }

    if (last_object)
        node = last_object->node.ln_Succ;
    else
        node = (struct Node *)name_space->list.mlh_Head;

    while ((current = utility_named_object_from_node(node)) != NULL)
    {
        if (!name || utility_names_equal(UtilityBase, name_space, (CONST_STRPTR)current->node.ln_Name, name))
        {
            if (name)
                name_space->cached_object = current;
            return current;
        }

        node = node->ln_Succ;
    }

    if (name)
        name_space->cached_object = NULL;

    return NULL;
}

static VOID utility_destroy_named_object(struct UtilityNamedObject *object)
{
    if (!object)
        return;

    if (object->named_object.no_Object && object->free_object)
        FreeVec(object->named_object.no_Object);

    if (object->child_space)
        FreeMem(object->child_space, sizeof(*object->child_space));

    FreeVec(object);
}

static VOID utility_finish_named_object_removal(struct UtilityNamedObject *object)
{
    struct UtilityNameSpace *name_space;
    struct Message *free_message;

    if (!object)
        return;

    name_space = object->parent_space;
    if (!name_space)
        return;

    ObtainSemaphore(&name_space->lock);

    if (object->parent_space == name_space)
    {
        if (name_space->cached_object == object)
            name_space->cached_object = NULL;
        Remove(&object->node);
        object->parent_space = NULL;
    }

    free_message = object->free_message;
    object->free_message = NULL;

    ReleaseSemaphore(&name_space->lock);

    if (free_message)
        ReplyMsg(free_message);
}

struct UtilityBase * __g_lxa_utility_InitLib    ( register struct UtilityBase *utilityb __asm("d0"),
                                                  register BPTR                seglist  __asm("a0"),
                                                  register struct ExecBase    *sysb     __asm("a6"))
{
    struct LXAUtilityRootBase *base = (struct LXAUtilityRootBase *)utilityb;

    (void)seglist;
    (void)sysb;

    base->last_id = 0;
    utility_new_min_list(&base->root_space.list);
    InitSemaphore(&base->root_space.lock);
    base->root_space.flags = NSF_NODUPS;
    base->root_space.cached_object = NULL;

    DPRINTF (LOG_DEBUG, "_utility: InitLib() called.\n");
    return utilityb;
}

struct UtilityBase * __g_lxa_utility_OpenLib ( register struct UtilityBase  *UtilityBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_utility: OpenLib() called\n");
    UtilityBase->ub_LibNode.lib_OpenCnt++;
    UtilityBase->ub_LibNode.lib_Flags &= ~LIBF_DELEXP;
    return UtilityBase;
}

BPTR __g_lxa_utility_CloseLib ( register struct UtilityBase  *utilityb __asm("a6"))
{
    utilityb->ub_LibNode.lib_OpenCnt--;
    return NULL;
}

BPTR __g_lxa_utility_ExpungeLib ( register struct UtilityBase  *utilityb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_utility_ExtFuncLib(void)
{
    return NULL;
}

static struct TagItem * _utility_FindTagItem ( register struct UtilityBase   *UtilityBase __asm("a6"),
                                                        register Tag                   tagVal      __asm("d0"),
                                                        register const struct TagItem *tagList     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: FindTagItem() called.\n");

    struct TagItem *tstate = (struct TagItem *)tagList;
    struct TagItem *tag;

    while ((tag = NextTagItem (&tstate)))
    {
        if ((ULONG)tag->ti_Tag == (ULONG)tagVal)
            return tag;
    }

    return NULL;
}

static ULONG _utility_GetTagData ( register struct UtilityBase   *UtilityBase __asm("a6"),
                                            register Tag                   tagValue    __asm("d0"),
                                            register ULONG                 defaultVal  __asm("d1"),
                                            register const struct TagItem *tagList     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: GetTagData() called.\n");

    struct TagItem *tag = FindTagItem (tagValue, tagList);

    return tag ? tag->ti_Data : defaultVal;
}

static ULONG _utility_PackBoolTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register ULONG initialFlags __asm("d0"),
                                                        register const struct TagItem * tagList __asm("a0"),
                                                        register const struct TagItem * boolMap __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: PackBoolTags() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static struct TagItem * _utility_NextTagItem ( register struct UtilityBase *  UtilityBase __asm("a6"),
                                                        register struct TagItem     ** tagListPtr  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: NextTagItem() called.\n");

    if (!(*tagListPtr))
        return NULL;

    while (TRUE)
    {
        switch (((*tagListPtr)->ti_Tag))
        {
            case TAG_MORE:
                if (!((*tagListPtr) = (struct TagItem *)(*tagListPtr)->ti_Data))
                    return NULL;
                continue;

            case TAG_END:
                (*tagListPtr) = NULL;
                return NULL;

            case TAG_SKIP:
                (*tagListPtr) += (*tagListPtr)->ti_Data + 1;
                continue;

            case TAG_IGNORE:
                break;

            default:
                return (*tagListPtr)++;
        }

        (*tagListPtr)++;
    }
}

static VOID _utility_FilterTagChanges ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * changeList __asm("a0"),
                                                        register struct TagItem * originalList __asm("a1"),
                                                        register ULONG apply __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_utility: FilterTagChanges() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_MapTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * tagList __asm("a0"),
                                                        register const struct TagItem * mapList __asm("a1"),
                                                        register ULONG mapType __asm("d0"))
{
    struct TagItem *tag;
    struct TagItem *map;
    struct TagItem *state;

    DPRINTF (LOG_DEBUG, "_utility: MapTags() called.\n");

    (void)UtilityBase;

    state = tagList;

    while ((tag = NextTagItem(&state)))
    {
        if (mapList && (map = FindTagItem(tag->ti_Tag, mapList)))
        {
            if (map->ti_Data == TAG_DONE)
                tag->ti_Tag = TAG_IGNORE;
            else
                tag->ti_Tag = (ULONG)map->ti_Data;
        }
        else if (mapType == MAP_REMOVE_NOT_FOUND)
        {
            tag->ti_Tag = TAG_IGNORE;
        }
    }
}

/*
 * AllocateTagItems - Allocate an array of TagItem structures
 *
 * Allocates memory for numTags TagItem structures.
 * The array is initialized to TAG_DONE.
 */
static struct TagItem * _utility_AllocateTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                    register ULONG numTags __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: AllocateTagItems() called, numTags=%lu\n", numTags);

    if (numTags == 0)
        return NULL;

    /* Allocate memory for the tag array */
    struct TagItem *tags = AllocVec(numTags * sizeof(struct TagItem), MEMF_PUBLIC | MEMF_CLEAR);

    if (tags)
    {
        /* Initialize all tags to TAG_DONE (already 0 from MEMF_CLEAR, but be explicit) */
        for (ULONG i = 0; i < numTags; i++)
        {
            tags[i].ti_Tag = TAG_DONE;
            tags[i].ti_Data = 0;
        }
    }

    return tags;
}

/*
 * CloneTagItems - Create a copy of a tag list
 *
 * Creates a new tag array containing copies of all tags from the original list.
 * The clone follows TAG_MORE links and expands them into a flat array.
 */
static struct TagItem * _utility_CloneTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                 register const struct TagItem * tagList __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: CloneTagItems() called, tagList=0x%08lx\n", (ULONG)tagList);

    (void)UtilityBase;

    if (!tagList)
        return AllocateTagItems(1);

    /* First pass: count the tags */
    ULONG count = 0;
    struct TagItem *tstate = (struct TagItem *)tagList;
    struct TagItem *tag;

    while ((tag = NextTagItem(&tstate)))
    {
        count++;
    }

    if (count == 0)
        return NULL;

    /* Allocate memory for the clone (count + 1 for TAG_DONE terminator) */
    struct TagItem *clone = AllocateTagItems(count + 1);
    if (!clone)
        return NULL;

    /* Second pass: copy the tags */
    tstate = (struct TagItem *)tagList;
    ULONG i = 0;

    while ((tag = NextTagItem(&tstate)))
    {
        clone[i].ti_Tag = tag->ti_Tag;
        clone[i].ti_Data = tag->ti_Data;
        i++;
    }

    /* Terminate with TAG_DONE (already set by AllocateTagItems, but be explicit) */
    clone[i].ti_Tag = TAG_DONE;
    clone[i].ti_Data = 0;

    return clone;
}

/*
 * FreeTagItems - Free a TagItem array allocated by AllocateTagItems or CloneTagItems
 */
static VOID _utility_FreeTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                    register struct TagItem * tagList __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: FreeTagItems() called, tagList=0x%08lx\n", (ULONG)tagList);

    if (tagList)
    {
        FreeVec(tagList);
    }
}

/*
 * RefreshTagItemClones - Update cloned tag values from original
 *
 * Scans the clone and original lists in parallel, updating clone values
 * to match the original. Both lists must have the same structure.
 */
static VOID _utility_RefreshTagItemClones ( register struct UtilityBase * UtilityBase __asm("a6"),
                                            register struct TagItem * clone __asm("a0"),
                                            register const struct TagItem * original __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_utility: RefreshTagItemClones() called\n");

    (void)UtilityBase;

    if (!clone)
        return;

    if (!original)
    {
        clone->ti_Tag = TAG_DONE;
        clone->ti_Data = 0;
        return;
    }

    struct TagItem *cstate = clone;
    struct TagItem *ostate = (struct TagItem *)original;
    struct TagItem *ctag, *otag;

    while ((ctag = NextTagItem(&cstate)) && (otag = NextTagItem(&ostate)))
    {
        /* Update the clone's data to match the original */
        ctag->ti_Data = otag->ti_Data;
    }
}

/*
 * TagInArray - Check if a tag value appears in an array
 *
 * The array is terminated by TAG_DONE (0).
 * Returns TRUE if the tag is found, FALSE otherwise.
 */
static BOOL _utility_TagInArray ( register struct UtilityBase * UtilityBase __asm("a6"),
                                  register Tag tagValue __asm("d0"),
                                  register const Tag * tagArray __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: TagInArray() called, tagValue=0x%08lx\n", tagValue);

    if (!tagArray)
        return FALSE;

    while (*tagArray != TAG_DONE)
    {
        if (*tagArray == tagValue)
            return TRUE;
        tagArray++;
    }

    return FALSE;
}

/*
 * FilterTagItems - Filter tags based on an array of tag values
 *
 * logic can be:
 *   TAGFILTER_AND (0): Keep only tags that ARE in filterArray
 *   TAGFILTER_NOT (1): Keep only tags that are NOT in filterArray
 *
 * Tags that don't pass the filter are changed to TAG_IGNORE.
 * Returns the count of remaining valid tags.
 */
static ULONG _utility_FilterTagItems ( register struct UtilityBase * UtilityBase __asm("a6"),
                                       register struct TagItem * tagList __asm("a0"),
                                       register const Tag * filterArray __asm("a1"),
                                       register ULONG logic __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: FilterTagItems() called, logic=%lu\n", logic);

    (void)UtilityBase;

    if (!tagList || !filterArray)
        return 0;

    ULONG count = 0;
    struct TagItem *tstate = tagList;

    /* We need to iterate manually to modify tags in place */
    while (tstate->ti_Tag != TAG_DONE)
    {
        /* Handle control tags */
        if (tstate->ti_Tag == TAG_MORE)
        {
            tstate = (struct TagItem *)tstate->ti_Data;
            if (!tstate)
                break;
            continue;
        }

        if (tstate->ti_Tag == TAG_SKIP)
        {
            tstate += tstate->ti_Data + 1;
            continue;
        }

        if (tstate->ti_Tag == TAG_IGNORE)
        {
            tstate++;
            continue;
        }

        /* Check if this tag should be kept */
        BOOL in_array = filterArray ? TagInArray(tstate->ti_Tag, filterArray) : FALSE;
        BOOL keep;

        if (logic == TAGFILTER_AND)
            keep = in_array;
        else if (logic == TAGFILTER_NOT)
            keep = !in_array;
        else
            keep = TRUE;

        if (keep)
        {
            count++;
        }
        else
        {
            /* Mark filtered tag as ignored */
            tstate->ti_Tag = TAG_IGNORE;
        }

        tstate++;
    }

    return count;
}

/*
 * CallHookPkt - Call a hook function with object and message
 *
 * The hook function is called with:
 *   A0 = pointer to Hook structure
 *   A1 = pointer to message (paramPacket)
 *   A2 = pointer to object
 *
 * Returns the value returned by the hook function.
 */
static ULONG _utility_CallHookPkt ( register struct UtilityBase * UtilityBase __asm("a6"),
                                    register struct Hook * hook __asm("a0"),
                                    register APTR object __asm("a2"),
                                    register APTR paramPacket __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_utility: CallHookPkt() called, hook=0x%08lx, object=0x%08lx, message=0x%08lx\n",
             (ULONG)hook, (ULONG)object, (ULONG)paramPacket);

    if (!hook || !hook->h_Entry)
        return 0;

    /*
     * Call the hook entry point with the standard register convention.
     * We define a function pointer type that uses the Amiga register convention.
     */
    typedef ULONG (*HookEntry)(register struct Hook *hook __asm("a0"),
                               register APTR object __asm("a2"),
                               register APTR message __asm("a1"));

    HookEntry entry = (HookEntry)hook->h_Entry;

    return entry(hook, object, paramPacket);
}

static VOID _utility_private0 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private0");
    assert(FALSE);
}

static VOID _utility_private1 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private1");
    assert(FALSE);
}

static VOID _utility_Amiga2Date ( register struct UtilityBase *UtilityBase __asm("a6"),
                                  register ULONG               seconds     __asm("d0"),
                                  register struct ClockData   *result      __asm("a0"))
{
    LONG days;
    LONG leap;
    LONG year;
    LONG month;

    if (!result)
        return;

    days = seconds / 86400;
    result->wday = days % 7;

    result->sec = seconds % 60;
    seconds /= 60;
    result->min = seconds % 60;
    seconds /= 60;
    result->hour = seconds % 24;

    leap = 1;

    if (days < 92 * 365 + 30 * 366)
    {
        days += 366 + 365;
        year = 4 * (days / (366 + 3 * 365)) + 1976;
        days %= (366 + 3 * 365);

        if (days >= 366)
        {
            leap = 0;
            days--;
            year += days / 365;
            days %= 365;
        }
    }
    else
    {
        days -= 17 * 365 + 5 * 366;
        year = 400 * (days / (97 * 366 + 303 * 365)) + 2000;
        days %= (97 * 366 + 303 * 365);

        if (days >= 366)
        {
            leap = 0;
            days--;
            year += 100 * (days / (24 * 366 + 76 * 365));
            days %= (24 * 366 + 76 * 365);

            if (days >= 365)
            {
                leap = 1;
                days++;
                year += 4 * (days / (366 + 3 * 365));
                days %= (366 + 3 * 365);

                if (days >= 366)
                {
                    leap = 0;
                    days--;
                    year += days / 365;
                    days %= 365;
                }
            }
        }
    }

    if (!leap && days >= 31 + 28)
        days++;

    for (month = 11; month >= 0; month--)
    {
        if (days >= (LONG)utility_day_table[month])
        {
            days -= utility_day_table[month];
            break;
        }
    }

    days++;
    month++;

    result->month = month;
    result->mday = days;
    result->year = year;
}

/*
 * Date2Amiga - Convert ClockData structure to Amiga seconds
 *
 * Returns seconds since January 1, 1978 (Amiga epoch)
 * Uses the algorithm from http://howardhinnant.github.io/date_algorithms.html
 */
static ULONG _utility_Date2Amiga ( register struct UtilityBase * UtilityBase __asm("a6"),
                                   register const struct ClockData * date __asm("a0"))
{
    static const UWORD days_per_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    ULONG time;
    UWORD year;
    UWORD leaps;

    if (!date)
        return 0;

    DPRINTF (LOG_DEBUG, "_utility: Date2Amiga() called, date=%04d-%02d-%02d %02d:%02d:%02d\n",
             date->year, date->month, date->mday, date->hour, date->min, date->sec);

    time = date->sec + (date->min * 60) + (date->hour * 3600);
    time += ((date->mday - 1) + days_per_month[date->month - 1]) * 86400;
    time += (date->year - 1978) * 31536000;

    year = date->year;

    if (((year % 400) == 0) || (((year % 4) == 0) && ((year % 100) != 0)))
    {
        if (date->month >= 3)
            time += 86400;
    }

    year--;
    leaps = ((year / 4) - (year / 100) + (year / 400) - (494 - 19 + 4));
    time += leaps * 86400;

    DPRINTF (LOG_DEBUG, "_utility: Date2Amiga() -> %lu seconds\n", time);

    return time;
}

/*
 * CheckDate - Validate a ClockData structure
 *
 * Returns 0 if the date is invalid, or the Amiga time value if valid.
 * This allows checking validity and getting the time in one call.
 */
static ULONG _utility_CheckDate ( register struct UtilityBase * UtilityBase __asm("a6"),
                                  register const struct ClockData * date __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_utility: CheckDate() called\n");

    if (!date)
        return 0;

    /* Validate time components */
    if (date->sec > 60 || date->min > 59 || date->hour > 23)
        return 0;

    /* Validate month */
    if (date->month < 1 || date->month > 12)
        return 0;

    /* Validate day */
    if (date->mday < 1 || date->mday > 31)
        return 0;

    /* Days per month (non-leap year) */
    static const UBYTE days_in_month[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    UWORD max_day = days_in_month[date->month];

    /* Check for February in leap year */
    if (date->month == 2)
    {
        UWORD year = date->year;
        BOOL is_leap = (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
        if (is_leap)
            max_day = 29;
    }

    if (date->mday > max_day)
        return 0;

    /* Validate year (Amiga epoch starts 1978) */
    if (date->year < 1978)
        return 0;

    /* Date is valid - return the Amiga time value */
    return Date2Amiga(date);
}

static LONG _utility_SMult32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register LONG arg1 __asm("d0"),
                                                        register LONG arg2 __asm("d1"))
{
    return arg1 * arg2;
}

static ULONG _utility_UMult32 ( register struct UtilityBase * UtilityBase __asm("a6"),
												register ULONG arg1 __asm("d0"),
												register ULONG arg2 __asm("d1"))
{
    return arg1 * arg2;
}

static LONG _utility_SDivMod32 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register LONG dividend __asm("d0"),
                                                        register LONG divisor __asm("d1"))
{
    LONG quotient;
    LONG remainder;

    (void)UtilityBase;

    /* SDivMod32 divides D0 by D1 and returns quotient in D0 and remainder in D1 */
    if (divisor == 0) {
        DPRINTF (LOG_ERROR, "_utility: SDivMod32() division by zero!\n");
        __asm volatile ("moveq #0, %%d1" : : : "d1");
        return 0;
    }

    quotient = dividend / divisor;
    remainder = dividend % divisor;

    __asm volatile ("move.l %0, %%d1" : : "d"(remainder) : "d1");

    return quotient;
}

static ULONG _utility_UDivMod32 ( register struct UtilityBase * UtilityBase __asm("a6"),
														register ULONG dividend __asm("d0"),
														register ULONG divisor __asm("d1"))
{
    ULONG quotient;
    ULONG remainder;

    (void)UtilityBase;

    /* UDivMod32 divides D0 by D1 and returns quotient in D0 and remainder in D1 */
    if (divisor == 0) {
        DPRINTF (LOG_ERROR, "_utility: UDivMod32() division by zero!\n");
        __asm volatile ("moveq #0, %%d1" : : : "d1");
        return 0;
    }

    quotient = dividend / divisor;
    remainder = dividend % divisor;

    __asm volatile ("move.l %0, %%d1" : : "d"(remainder) : "d1");

    return quotient;
}

static LONG _utility_Stricmp ( register struct UtilityBase * UtilityBase __asm("a6"),
														register CONST_STRPTR string1 __asm("a0"),
														register CONST_STRPTR string2 __asm("a1"))
{
    /* Case-insensitive string comparison */
    while (*string1 && *string2) {
        char c1 = *string1;
        char c2 = *string2;
        
        /* Convert to lowercase for comparison */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
        
        if (c1 != c2) {
            return (LONG)((UBYTE)c1 - (UBYTE)c2);
        }
        
        string1++;
        string2++;
    }
    
    /* Return difference of terminating characters */
    return (LONG)((UBYTE)*string1 - (UBYTE)*string2);
}

static LONG _utility_Strnicmp ( register struct UtilityBase * UtilityBase __asm("a6"),
                                register CONST_STRPTR string1 __asm("a0"),
                                register CONST_STRPTR string2 __asm("a1"),
                                register LONG length __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: Strnicmp() called, length=%ld\n", length);

    if (length <= 0)
        return 0;

    while (length > 0 && *string1 && *string2)
    {
        char c1 = *string1;
        char c2 = *string2;

        /* Convert to lowercase for comparison */
        if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
        if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

        if (c1 != c2)
        {
            return (LONG)((UBYTE)c1 - (UBYTE)c2);
        }

        string1++;
        string2++;
        length--;
    }

    /* If we've compared 'length' characters, strings are equal up to that point */
    if (length == 0)
        return 0;

    /* Return difference of terminating characters */
    return (LONG)((UBYTE)*string1 - (UBYTE)*string2);
}

static UBYTE _utility_ToUpper ( register struct UtilityBase *UtilityBase __asm("a6"),
                                         register UBYTE               c           __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: ToUpper() called.\n");

    // FIXME: take locale into account

    if ((c>96) && (c<123)) return c ^ 0x20;
    if ((c >= 0xe0) && (c <= 0xee) && (c != 0xe7)) return c-0x20;

    return c;
}

static UBYTE _utility_ToLower ( register struct UtilityBase *UtilityBase __asm("a6"),
                                         register UBYTE               c           __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_utility: ToLower() called.\n");

    // FIXME: take locale into account

    if ((c>0x40) && (c<0x5b)) return c | 0x60;
    if ((c>=0xc0) && (c<=0xde) && (c!=0xd7)) return c + 0x20;
    return c;
}

static VOID _utility_ApplyTagChanges ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct TagItem * list __asm("a0"),
                                                        register const struct TagItem * changeList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_utility: ApplyTagChanges() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _utility_private2 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private2");
    assert(FALSE);
}

/*
 * SMult64 - Signed 32x32->64 bit multiplication
 *
 * Returns the low 32 bits in D0, high 32 bits in D1.
 * The C prototype only shows the D0 return value.
 *
 * We implement this manually to avoid needing __muldi3 from libgcc.
 */
static LONG _utility_SMult64 ( register struct UtilityBase * UtilityBase __asm("a6"),
                               register LONG arg1 __asm("d0"),
                               register LONG arg2 __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_utility: SMult64() called, arg1=%ld, arg2=%ld\n", arg1, arg2);

    /* Handle signs */
    BOOL negative = FALSE;
    ULONG a, b;

    if (arg1 < 0)
    {
        negative = !negative;
        a = (ULONG)(-arg1);
    }
    else
    {
        a = (ULONG)arg1;
    }

    if (arg2 < 0)
    {
        negative = !negative;
        b = (ULONG)(-arg2);
    }
    else
    {
        b = (ULONG)arg2;
    }

    /* Split into 16-bit halves: a = (a_hi << 16) + a_lo */
    ULONG a_lo = a & 0xFFFF;
    ULONG a_hi = a >> 16;
    ULONG b_lo = b & 0xFFFF;
    ULONG b_hi = b >> 16;

    /* Multiply parts:
     * a * b = (a_hi*2^16 + a_lo) * (b_hi*2^16 + b_lo)
     *       = a_hi*b_hi*2^32 + (a_hi*b_lo + a_lo*b_hi)*2^16 + a_lo*b_lo
     */
    ULONG lo_lo = a_lo * b_lo;                /* No overflow in 32 bits */
    ULONG hi_hi = a_hi * b_hi;                /* No overflow in 32 bits */
    ULONG cross1 = a_hi * b_lo;
    ULONG cross2 = a_lo * b_hi;

    /* Combine cross terms */
    ULONG cross = cross1 + cross2;
    ULONG cross_carry = (cross < cross1) ? 0x10000 : 0;  /* Carry to high word */

    /* Low result: lo_lo + (cross_lo << 16) */
    ULONG low = lo_lo + (cross << 16);
    ULONG low_carry = (low < lo_lo) ? 1 : 0;

    /* High result: hi_hi + (cross >> 16) + cross_carry + low_carry */
    ULONG high = hi_hi + (cross >> 16) + cross_carry + low_carry;

    /* Apply sign if needed (negate 64-bit result) */
    if (negative)
    {
        low = ~low + 1;
        high = ~high + (low == 0 ? 1 : 0);
    }

    /* The ABI expects high bits in D1 */
    __asm volatile ("move.l %0, %%d1" : : "d"(high) : "d1");

    return (LONG)low;
}

/*
 * UMult64 - Unsigned 32x32->64 bit multiplication
 *
 * Returns the low 32 bits in D0, high 32 bits in D1.
 * The C prototype only shows the D0 return value.
 *
 * We implement this manually to avoid needing __muldi3 from libgcc.
 */
static ULONG _utility_UMult64 ( register struct UtilityBase * UtilityBase __asm("a6"),
                                register ULONG arg1 __asm("d0"),
                                register ULONG arg2 __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_utility: UMult64() called, arg1=%lu, arg2=%lu\n", arg1, arg2);

    ULONG a = arg1;
    ULONG b = arg2;

    /* Split into 16-bit halves: a = (a_hi << 16) + a_lo */
    ULONG a_lo = a & 0xFFFF;
    ULONG a_hi = a >> 16;
    ULONG b_lo = b & 0xFFFF;
    ULONG b_hi = b >> 16;

    /* Multiply parts:
     * a * b = (a_hi*2^16 + a_lo) * (b_hi*2^16 + b_lo)
     *       = a_hi*b_hi*2^32 + (a_hi*b_lo + a_lo*b_hi)*2^16 + a_lo*b_lo
     */
    ULONG lo_lo = a_lo * b_lo;                /* No overflow in 32 bits */
    ULONG hi_hi = a_hi * b_hi;                /* No overflow in 32 bits */
    ULONG cross1 = a_hi * b_lo;
    ULONG cross2 = a_lo * b_hi;

    /* Combine cross terms */
    ULONG cross = cross1 + cross2;
    ULONG cross_carry = (cross < cross1) ? 0x10000 : 0;  /* Carry to high word */

    /* Low result: lo_lo + (cross_lo << 16) */
    ULONG low = lo_lo + (cross << 16);
    ULONG low_carry = (low < lo_lo) ? 1 : 0;

    /* High result: hi_hi + (cross >> 16) + cross_carry + low_carry */
    ULONG high = hi_hi + (cross >> 16) + cross_carry + low_carry;

    /* The ABI expects high bits in D1 */
    __asm volatile ("move.l %0, %%d1" : : "d"(high) : "d1");

    return low;
}

static ULONG _utility_PackStructureTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register APTR pack __asm("a0"),
                                                        register const ULONG * packTable __asm("a1"),
                                                        register const struct TagItem * tagList __asm("a2"))
{
    Tag tag_base;
    ULONG entry;
    ULONG count = 0;

    DPRINTF (LOG_DEBUG, "_utility: PackStructureTags() called.\n");

    (void)UtilityBase;

    if (!pack || !packTable || !tagList)
        return 0;

    tag_base = *packTable++;

    while ((entry = *packTable++) != 0)
    {
        UWORD mem_offset;
        UWORD tag_offset;
        UBYTE bit_offset;
        struct TagItem *tag;
        union utility_memaccess *mem_ptr;

        if ((LONG)entry == -1)
        {
            tag_base = *packTable++;
            continue;
        }

        if (entry & PSTF_PACK)
            continue;

        tag_offset = (entry >> 16) & 0x03ff;
        tag = FindTagItem(tag_base + tag_offset, tagList);
        if (!tag)
            continue;

        mem_offset = entry & 0x1fff;
        bit_offset = (entry >> 13) & 0x07;
        mem_ptr = (union utility_memaccess *)((UBYTE *)pack + mem_offset);

        if ((entry & (PKCTRL_BIT | PSTF_EXISTS)) == (PKCTRL_BIT | PSTF_EXISTS))
        {
            if (entry & PSTF_SIGNED)
                mem_ptr->ub &= ~(1U << bit_offset);
            else
                mem_ptr->ub |= (1U << bit_offset);

            count++;
            continue;
        }

        switch (entry & 0x98000000UL)
        {
            case PKCTRL_ULONG:
                mem_ptr->ul = tag->ti_Data;
                break;

            case PKCTRL_UWORD:
                mem_ptr->uw = tag->ti_Data;
                break;

            case PKCTRL_UBYTE:
                mem_ptr->ub = tag->ti_Data;
                break;

            case PKCTRL_LONG:
                mem_ptr->sl = tag->ti_Data;
                break;

            case PKCTRL_WORD:
                mem_ptr->sw = tag->ti_Data;
                break;

            case PKCTRL_BYTE:
                mem_ptr->sb = tag->ti_Data;
                break;

            case PKCTRL_BIT:
                if (tag->ti_Data)
                    mem_ptr->ub |= (1U << bit_offset);
                else
                    mem_ptr->ub &= ~(1U << bit_offset);
                break;

            case PKCTRL_FLIPBIT:
                if (tag->ti_Data)
                    mem_ptr->ub &= ~(1U << bit_offset);
                else
                    mem_ptr->ub |= (1U << bit_offset);
                break;

            default:
                continue;
        }

        count++;
    }

    return count;
}

static ULONG _utility_UnpackStructureTags ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register const APTR pack __asm("a0"),
                                                        register const ULONG * packTable __asm("a1"),
                                                        register struct TagItem * tagList __asm("a2"))
{
    Tag tag_base;
    ULONG entry;
    ULONG count = 0;

    DPRINTF (LOG_DEBUG, "_utility: UnpackStructureTags() called.\n");

    (void)UtilityBase;

    if (!pack || !packTable || !tagList)
        return 0;

    tag_base = *packTable++;

    while ((entry = *packTable++) != 0)
    {
        UWORD mem_offset;
        UWORD tag_offset;
        UBYTE bit_offset;
        struct TagItem *tag;
        union utility_memaccess *mem_ptr;

        if ((LONG)entry == -1)
        {
            tag_base = *packTable++;
            continue;
        }

        if (entry & PSTF_UNPACK)
            continue;

        tag_offset = (entry >> 16) & 0x03ff;
        tag = FindTagItem(tag_base + tag_offset, tagList);
        if (!tag)
            continue;

        mem_offset = entry & 0x1fff;
        bit_offset = (entry >> 13) & 0x07;
        mem_ptr = (union utility_memaccess *)((UBYTE *)pack + mem_offset);

        switch (entry & 0x98000000UL)
        {
            case PKCTRL_ULONG:
                *(ULONG *)tag->ti_Data = mem_ptr->ul;
                break;

            case PKCTRL_UWORD:
                *(ULONG *)tag->ti_Data = mem_ptr->uw;
                break;

            case PKCTRL_UBYTE:
                *(ULONG *)tag->ti_Data = mem_ptr->ub;
                break;

            case PKCTRL_LONG:
                *(ULONG *)tag->ti_Data = mem_ptr->sl;
                break;

            case PKCTRL_WORD:
                *(ULONG *)tag->ti_Data = mem_ptr->sw;
                break;

            case PKCTRL_BYTE:
                *(ULONG *)tag->ti_Data = mem_ptr->sb;
                break;

            case PKCTRL_BIT:
                *(ULONG *)tag->ti_Data = (mem_ptr->ub & (1U << bit_offset)) ? TRUE : FALSE;
                break;

            case PKCTRL_FLIPBIT:
                *(ULONG *)tag->ti_Data = (mem_ptr->ub & (1U << bit_offset)) ? FALSE : TRUE;
                break;

            default:
                continue;
        }

        count++;
    }

    return count;
}

static BOOL _utility_AddNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * nameSpace __asm("a0"),
                                                        register struct NamedObject * object __asm("a1"))
{
    struct UtilityNameSpace *name_space;
    struct UtilityNamedObject *named_object;
    BOOL ret = FALSE;

    DPRINTF (LOG_DEBUG, "_utility: AddNamedObject() called, nameSpace=0x%08lx, object=0x%08lx\n",
             (ULONG)nameSpace, (ULONG)object);

    if (!object)
        return FALSE;

    name_space = utility_get_namespace(UtilityBase, nameSpace);
    named_object = utility_named_object(object);

    if (!name_space || named_object->parent_space)
        return FALSE;

    ObtainSemaphore(&name_space->lock);

    if ((name_space->flags & NSF_NODUPS) &&
        utility_find_named_object_locked(UtilityBase, name_space, (CONST_STRPTR)named_object->node.ln_Name, NULL))
    {
        ret = FALSE;
    }
    else
    {
        Enqueue((struct List *)&name_space->list, &named_object->node);
        named_object->parent_space = name_space;
        name_space->cached_object = named_object;
        ret = TRUE;
    }

    ReleaseSemaphore(&name_space->lock);

    return ret;
}

static struct NamedObject * _utility_AllocNamedObjectA ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register CONST_STRPTR name __asm("a0"),
                                                        register const struct TagItem * tagList __asm("a1"))
{
    struct UtilityNamedObject *object;
    ULONG name_len;
    ULONG user_space_size;

    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: AllocNamedObjectA() called, name='%s'\n", STRORNULL(name));

    if (!name)
        return NULL;

    name_len = strlen((const char *)name);
    object = AllocVec(sizeof(*object) + name_len + 1, MEMF_PUBLIC | MEMF_CLEAR);
    if (!object)
        return NULL;

    object->node.ln_Name = (char *)(object + 1);
    strcpy((char *)object->node.ln_Name, (const char *)name);
    object->node.ln_Pri = (BYTE)GetTagData(ANO_Priority, 0, tagList);
    object->use_count = 1;

    if (GetTagData(ANO_NameSpace, FALSE, tagList))
    {
        object->child_space = AllocMem(sizeof(*object->child_space), MEMF_PUBLIC | MEMF_CLEAR);
        if (!object->child_space)
        {
            utility_destroy_named_object(object);
            return NULL;
        }

        utility_new_min_list(&object->child_space->list);
        InitSemaphore(&object->child_space->lock);
        object->child_space->flags = GetTagData(ANO_Flags, 0, tagList);
        object->child_space->cached_object = NULL;
    }

    user_space_size = GetTagData(ANO_UserSpace, 0, tagList);
    if (user_space_size)
    {
        object->named_object.no_Object = AllocVec(user_space_size, MEMF_PUBLIC | MEMF_CLEAR);
        if (!object->named_object.no_Object)
        {
            utility_destroy_named_object(object);
            return NULL;
        }

        object->free_object = TRUE;
    }

    return &object->named_object;
}

static LONG _utility_AttemptRemNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    struct UtilityNamedObject *named_object;

    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: AttemptRemNamedObject() called, object=0x%08lx\n", (ULONG)object);

    if (!object)
        return FALSE;

    named_object = utility_named_object(object);
    if (named_object->free_message || named_object->use_count > 1)
        return FALSE;

    RemNamedObject(object, NULL);
    return TRUE;
}

static struct NamedObject * _utility_FindNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * nameSpace __asm("a0"),
                                                        register CONST_STRPTR name __asm("a1"),
                                                        register struct NamedObject * lastObject __asm("a2"))
{
    struct UtilityNameSpace *name_space;
    struct UtilityNamedObject *found;
    struct UtilityNamedObject *last_named_object = NULL;

    DPRINTF (LOG_DEBUG, "_utility: FindNamedObject() called, nameSpace=0x%08lx, name='%s', lastObject=0x%08lx\n",
             (ULONG)nameSpace, STRORNULL(name), (ULONG)lastObject);

    name_space = utility_get_namespace(UtilityBase, nameSpace);
    if (!name_space)
        return NULL;

    if (lastObject)
        last_named_object = utility_named_object(lastObject);

    ObtainSemaphore(&name_space->lock);

    if (name)
    {
        found = utility_find_named_object_locked(UtilityBase, name_space, name, last_named_object);
    }
    else
    {
        struct Node *node;

        if (last_named_object)
            node = last_named_object->node.ln_Succ;
        else
            node = (struct Node *)name_space->list.mlh_Head;

        found = utility_named_object_from_node(node);
    }

    if (found)
        found->use_count++;
    ReleaseSemaphore(&name_space->lock);

    return found ? &found->named_object : NULL;
}

static VOID _utility_FreeNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    struct UtilityNamedObject *named_object;

    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: FreeNamedObject() called, object=0x%08lx\n", (ULONG)object);

    if (!object)
        return;

    named_object = utility_named_object(object);
    if (named_object->parent_space)
        return;

    if (named_object->child_space && named_object->child_space->list.mlh_TailPred != (struct MinNode *)&named_object->child_space->list.mlh_Head)
        return;

    utility_destroy_named_object(named_object);
}

static STRPTR _utility_NamedObjectName ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: NamedObjectName() called, object=0x%08lx\n", (ULONG)object);

    if (!object)
        return NULL;

    return (STRPTR)utility_named_object(object)->node.ln_Name;
}

static VOID _utility_ReleaseNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"))
{
    struct UtilityNamedObject *named_object;
    BOOL finish_removal = FALSE;

    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: ReleaseNamedObject() called, object=0x%08lx\n", (ULONG)object);

    if (!object)
        return;

    named_object = utility_named_object(object);

    Forbid();
    if (named_object->use_count > 0)
        named_object->use_count--;
    if (named_object->use_count == 0 && named_object->free_message)
        finish_removal = TRUE;
    Permit();

    if (finish_removal)
        utility_finish_named_object_removal(named_object);
}

static VOID _utility_RemNamedObject ( register struct UtilityBase * UtilityBase __asm("a6"),
                                                        register struct NamedObject * object __asm("a0"),
                                                        register struct Message * message __asm("a1"))
{
    struct UtilityNamedObject *named_object;
    BOOL finish_removal = FALSE;

    (void)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: RemNamedObject() called, object=0x%08lx, message=0x%08lx\n",
             (ULONG)object, (ULONG)message);

    if (!object)
        return;

    named_object = utility_named_object(object);
    if (!named_object->parent_space)
        return;

    Forbid();

    if (message)
    {
        if (!named_object->free_message)
        {
            message->mn_Node.ln_Name = (char *)object;
            named_object->free_message = message;
        }
        else
        {
            message->mn_Node.ln_Name = NULL;
            ReplyMsg(message);
            Permit();
            return;
        }
    }

    if (named_object->use_count > 0)
        named_object->use_count--;

    if (named_object->use_count == 0)
        finish_removal = TRUE;

    Permit();

    if (finish_removal)
        utility_finish_named_object_removal(named_object);
}

static ULONG _utility_GetUniqueID ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    ULONG ret;
    struct LXAUtilityRootBase *base = (struct LXAUtilityRootBase *)UtilityBase;

    DPRINTF (LOG_DEBUG, "_utility: GetUniqueID() called.\n");

    Disable();
    ret = ++base->last_id;
    Enable();

    return ret;
}

static VOID _utility_private3 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private3");
    assert(FALSE);
}

static VOID _utility_private4 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private4");
    assert(FALSE);
}

static VOID _utility_private5 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private5");
    assert(FALSE);
}

static VOID _utility_private6 ( register struct UtilityBase * UtilityBase __asm("a6"))
{
    PRIVATE_FUNCTION_ERROR("_utility", "private6");
    assert(FALSE);
}

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

extern APTR              __g_lxa_utility_FuncTab [];
extern struct MyDataInit __g_lxa_utility_DataTab;
extern struct InitTable  __g_lxa_utility_InitTab;
extern APTR              __g_lxa_utility_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_utility_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_utility_ExLibName[0],     // char  *rt_Name;                   14
    &_g_utility_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_utility_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_utility_EndResident;
struct Resident *__lxa_utility_ROMTag = &ROMTag;

struct InitTable __g_lxa_utility_InitTab =
{
    (ULONG)               sizeof(struct LXAUtilityRootBase), // LibBaseSize
    (APTR              *) &__g_lxa_utility_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_utility_DataTab,     // DataTable
    (APTR)                __g_lxa_utility_InitLib       // InitLibFn
};

APTR __g_lxa_utility_FuncTab [] =
{
    __g_lxa_utility_OpenLib,
    __g_lxa_utility_CloseLib,
    __g_lxa_utility_ExpungeLib,
    __g_lxa_utility_ExtFuncLib,
    _utility_FindTagItem, // offset = -30
    _utility_GetTagData, // offset = -36
    _utility_PackBoolTags, // offset = -42
    _utility_NextTagItem, // offset = -48
    _utility_FilterTagChanges, // offset = -54
    _utility_MapTags, // offset = -60
    _utility_AllocateTagItems, // offset = -66
    _utility_CloneTagItems, // offset = -72
    _utility_FreeTagItems, // offset = -78
    _utility_RefreshTagItemClones, // offset = -84
    _utility_TagInArray, // offset = -90
    _utility_FilterTagItems, // offset = -96
    _utility_CallHookPkt, // offset = -102
    _utility_private0, // offset = -108
    _utility_private1, // offset = -114
    _utility_Amiga2Date, // offset = -120
    _utility_Date2Amiga, // offset = -126
    _utility_CheckDate, // offset = -132
    _utility_SMult32, // offset = -138
    _utility_UMult32, // offset = -144
    _utility_SDivMod32, // offset = -150
    _utility_UDivMod32, // offset = -156
    _utility_Stricmp, // offset = -162
    _utility_Strnicmp, // offset = -168
    _utility_ToUpper, // offset = -174
    _utility_ToLower, // offset = -180
    _utility_ApplyTagChanges, // offset = -186
    _utility_private2, // offset = -192
    _utility_SMult64, // offset = -198
    _utility_UMult64, // offset = -204
    _utility_PackStructureTags, // offset = -210
    _utility_UnpackStructureTags, // offset = -216
    _utility_AddNamedObject, // offset = -222
    _utility_AllocNamedObjectA, // offset = -228
    _utility_AttemptRemNamedObject, // offset = -234
    _utility_FindNamedObject, // offset = -240
    _utility_FreeNamedObject, // offset = -246
    _utility_NamedObjectName, // offset = -252
    _utility_ReleaseNamedObject, // offset = -258
    _utility_RemNamedObject, // offset = -264
    _utility_GetUniqueID, // offset = -270
    _utility_private3, // offset = -276
    _utility_private4, // offset = -282
    _utility_private5, // offset = -288
    _utility_private6, // offset = -294
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_utility_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_utility_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_utility_ExLibID[0],
    (ULONG) 0
};
