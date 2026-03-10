#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/utility.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static struct UtilityBase *UtilityBase;
static LONG errors;

#define TEST_TAG_A (TAG_USER + 0x10)
#define TEST_TAG_B (TAG_USER + 0x11)
#define TEST_TAG_C (TAG_USER + 0x12)
#define TEST_TAG_D (TAG_USER + 0x13)
#define TEST_TAG_E (TAG_USER + 0x14)

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
    errors++;
}

static void pass(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static BOOL expect_true(BOOL cond, const char *msg)
{
    if (!cond)
    {
        fail(msg);
        return FALSE;
    }

    pass(msg);
    return TRUE;
}

static BOOL expect_ulong(ULONG actual, ULONG expected, const char *msg)
{
    if (actual != expected)
    {
        fail(msg);
        return FALSE;
    }

    pass(msg);
    return TRUE;
}

static ULONG hook_entry(register struct Hook *hook __asm("a0"),
                        register APTR object __asm("a2"),
                        register APTR message __asm("a1"))
{
    typedef ULONG (*HookFunc)(struct Hook *, APTR, APTR);
    HookFunc func = (HookFunc)hook->h_SubEntry;
    return func(hook, object, message);
}

static ULONG hook_subentry(struct Hook *hook, APTR object, APTR message)
{
    ULONG *state = (ULONG *)hook->h_Data;

    if ((ULONG)object == 0x1234UL && (ULONG)message == 0x5678UL)
        *state = 0xCAFEBABEUL;

    return *state;
}

static void test_find_and_get_tag_data(void)
{
    struct TagItem more_tags[] = {
        { TEST_TAG_C, 33 },
        { TAG_DONE, 0 }
    };
    struct TagItem tags[] = {
        { TEST_TAG_A, 11 },
        { TAG_IGNORE, 0 },
        { TAG_SKIP, 1 },
        { TEST_TAG_E, 55 },
        { TAG_MORE, (ULONG)more_tags },
        { TEST_TAG_D, 44 },
        { TAG_DONE, 0 }
    };
    struct TagItem *tag;
    struct TagItem *state;

    print("Testing FindTagItem/GetTagData/NextTagItem...\n");

    tag = FindTagItem(TEST_TAG_A, tags);
    expect_true(tag != NULL, "FindTagItem finds first tag");
    if (tag)
        expect_ulong(tag->ti_Data, 11, "FindTagItem returns matching data");

    expect_ulong(GetTagData(TEST_TAG_C, 99, tags), 33,
        "GetTagData follows TAG_MORE chain");
    expect_ulong(GetTagData(TEST_TAG_D, 99, tags), 99,
        "GetTagData stops at TAG_MORE terminator");
    expect_ulong(GetTagData(TEST_TAG_B, 77, tags), 77,
        "GetTagData returns default for missing tag");

    state = tags;
    tag = NextTagItem(&state);
    expect_true(tag != NULL && tag->ti_Tag == TEST_TAG_A,
        "NextTagItem returns first real tag");

    tag = NextTagItem(&state);
    expect_true(tag != NULL && tag->ti_Tag == TEST_TAG_C,
        "NextTagItem skips ignore/skip and follows TAG_MORE");

    tag = NextTagItem(&state);
    expect_true(tag == NULL, "NextTagItem ends on TAG_DONE in chained list");
}

static void test_allocate_clone_refresh(void)
{
    struct TagItem tail[] = {
        { TEST_TAG_C, 30 },
        { TAG_DONE, 0 }
    };
    struct TagItem original[] = {
        { TEST_TAG_A, 10 },
        { TEST_TAG_B, 20 },
        { TAG_MORE, (ULONG)tail },
        { TAG_DONE, 0 }
    };
    struct TagItem *allocated;
    struct TagItem *clone;
    struct TagItem *empty_clone;

    print("Testing AllocateTagItems/CloneTagItems/RefreshTagItemClones...\n");

    allocated = AllocateTagItems(3);
    expect_true(allocated != NULL, "AllocateTagItems returns memory");
    if (allocated)
    {
        expect_ulong(allocated[0].ti_Tag, TAG_DONE,
            "AllocateTagItems clears first tag");
        expect_ulong(allocated[2].ti_Tag, TAG_DONE,
            "AllocateTagItems clears all tags");
        FreeTagItems(allocated);
    }

    empty_clone = CloneTagItems(NULL);
    expect_true(empty_clone != NULL, "CloneTagItems(NULL) returns empty list");
    if (empty_clone)
    {
        expect_ulong(empty_clone[0].ti_Tag, TAG_DONE,
            "CloneTagItems(NULL) returns TAG_DONE terminator");
        FreeTagItems(empty_clone);
    }

    clone = CloneTagItems(original);
    expect_true(clone != NULL, "CloneTagItems clones chained list");
    if (!clone)
        return;

    expect_ulong(clone[0].ti_Tag, TEST_TAG_A, "CloneTagItems preserves first tag");
    expect_ulong(clone[1].ti_Data, 20, "CloneTagItems preserves second data");
    expect_ulong(clone[2].ti_Tag, TEST_TAG_C, "CloneTagItems flattens TAG_MORE chain");
    expect_ulong(clone[3].ti_Tag, TAG_DONE, "CloneTagItems terminates clone");

    clone[0].ti_Data = 999;
    clone[1].ti_Data = 888;
    RefreshTagItemClones(clone, original);
    expect_ulong(clone[0].ti_Data, 10, "RefreshTagItemClones restores first data");
    expect_ulong(clone[1].ti_Data, 20, "RefreshTagItemClones restores second data");

    RefreshTagItemClones(clone, NULL);
    expect_ulong(clone[0].ti_Tag, TAG_DONE,
        "RefreshTagItemClones(NULL original) resets clone terminator");

    FreeTagItems(clone);
}

static void test_tag_in_array_and_filter(void)
{
    Tag filter[] = { TEST_TAG_A, TEST_TAG_C, TAG_DONE };
    struct TagItem tags_and[] = {
        { TEST_TAG_A, 1 },
        { TEST_TAG_B, 2 },
        { TEST_TAG_C, 3 },
        { TAG_DONE, 0 }
    };
    struct TagItem tags_not[] = {
        { TEST_TAG_A, 1 },
        { TEST_TAG_B, 2 },
        { TEST_TAG_C, 3 },
        { TAG_DONE, 0 }
    };

    print("Testing TagInArray/FilterTagItems...\n");

    expect_true(TagInArray(TEST_TAG_A, filter), "TagInArray finds tag");
    expect_true(!TagInArray(TEST_TAG_D, filter), "TagInArray misses absent tag");

    expect_ulong(FilterTagItems(tags_and, filter, TAGFILTER_AND), 2,
        "FilterTagItems(TAGFILTER_AND) returns kept count");
    expect_ulong(tags_and[0].ti_Tag, TEST_TAG_A,
        "FilterTagItems(TAGFILTER_AND) keeps allowed tag");
    expect_ulong(tags_and[1].ti_Tag, TAG_IGNORE,
        "FilterTagItems(TAGFILTER_AND) ignores disallowed tag");

    expect_ulong(FilterTagItems(tags_not, filter, TAGFILTER_NOT), 1,
        "FilterTagItems(TAGFILTER_NOT) returns kept count");
    expect_ulong(tags_not[0].ti_Tag, TAG_IGNORE,
        "FilterTagItems(TAGFILTER_NOT) removes matched tag");
    expect_ulong(tags_not[1].ti_Tag, TEST_TAG_B,
        "FilterTagItems(TAGFILTER_NOT) keeps unmatched tag");
}

static void test_map_tags(void)
{
    struct TagItem map_list[] = {
        { TEST_TAG_A, TEST_TAG_D },
        { TEST_TAG_B, TAG_DONE },
        { TAG_DONE, 0 }
    };
    struct TagItem keep_tags[] = {
        { TEST_TAG_A, 1 },
        { TEST_TAG_B, 2 },
        { TEST_TAG_C, 3 },
        { TAG_DONE, 0 }
    };
    struct TagItem remove_tags[] = {
        { TEST_TAG_A, 1 },
        { TEST_TAG_B, 2 },
        { TEST_TAG_C, 3 },
        { TAG_DONE, 0 }
    };

    print("Testing MapTags...\n");

    MapTags(keep_tags, map_list, MAP_KEEP_NOT_FOUND);
    expect_ulong(keep_tags[0].ti_Tag, TEST_TAG_D,
        "MapTags remaps matching tag");
    expect_ulong(keep_tags[1].ti_Tag, TAG_IGNORE,
        "MapTags converts TAG_DONE mapping to TAG_IGNORE");
    expect_ulong(keep_tags[2].ti_Tag, TEST_TAG_C,
        "MapTags keeps unmatched tag when requested");

    MapTags(remove_tags, map_list, MAP_REMOVE_NOT_FOUND);
    expect_ulong(remove_tags[0].ti_Tag, TEST_TAG_D,
        "MapTags remaps matching tag with remove-not-found");
    expect_ulong(remove_tags[1].ti_Tag, TAG_IGNORE,
        "MapTags ignores TAG_DONE-mapped tag with remove-not-found");
    expect_ulong(remove_tags[2].ti_Tag, TAG_IGNORE,
        "MapTags removes unmatched tag when requested");
}

static void test_call_hook_and_unique_id(void)
{
    struct Hook hook;
    ULONG hook_state = 0x12345678UL;
    ULONG ret;
    ULONG id1;
    ULONG id2;

    print("Testing CallHookPkt/GetUniqueID...\n");

    hook.h_Entry = (ULONG (*)())hook_entry;
    hook.h_SubEntry = (ULONG (*)())hook_subentry;
    hook.h_Data = &hook_state;

    ret = CallHookPkt(&hook, (APTR)0x1234UL, (APTR)0x5678UL);
    expect_ulong(ret, 0xCAFEBABEUL, "CallHookPkt returns hook result");
    expect_ulong(hook_state, 0xCAFEBABEUL, "CallHookPkt passes hook/object/message");

    id1 = GetUniqueID();
    id2 = GetUniqueID();
    expect_true(id2 == id1 + 1, "GetUniqueID returns monotonically increasing ids");
}

int main(void)
{
    print("utility/tagitems test\n");

    UtilityBase = (struct UtilityBase *)OpenLibrary((CONST_STRPTR)"utility.library", 36);
    if (!UtilityBase)
    {
        print("FAIL: Could not open utility.library\n");
        return 20;
    }

    test_find_and_get_tag_data();
    test_allocate_clone_refresh();
    test_tag_in_array_and_filter();
    test_map_tags();
    test_call_hook_and_unique_id();

    CloseLibrary((struct Library *)UtilityBase);

    if (errors != 0)
    {
        print("FAIL: utility/tagitems had errors\n");
        return 20;
    }

    print("PASS: utility/tagitems all tests passed\n");
    return 0;
}
