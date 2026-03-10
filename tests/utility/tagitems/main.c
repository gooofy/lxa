#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <utility/tagitem.h>
#include <utility/date.h>
#include <utility/pack.h>
#include <utility/hooks.h>
#include <utility/name.h>
#include <exec/semaphores.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/utility.h>
#include <string.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static struct UtilityBase *UtilityBase;
static LONG errors;

#define TEST_TAG_A (TAG_USER + 0x10)
#define TEST_TAG_B (TAG_USER + 0x11)
#define TEST_TAG_C (TAG_USER + 0x12)
#define TEST_TAG_D (TAG_USER + 0x13)
#define TEST_TAG_E (TAG_USER + 0x14)
#define TEST_PACK_BASE (TAG_USER + 0x0ff)

#define TEST_PACK_UBYTE  (TEST_PACK_BASE + 1)
#define TEST_PACK_BYTE   (TEST_PACK_BASE + 2)
#define TEST_PACK_UWORD  (TEST_PACK_BASE + 3)
#define TEST_PACK_WORD   (TEST_PACK_BASE + 4)
#define TEST_PACK_ULONG  (TEST_PACK_BASE + 5)
#define TEST_PACK_LONG   (TEST_PACK_BASE + 6)
#define TEST_PACK_BIT    (TEST_PACK_BASE + 7)
#define TEST_PACK_FLIP   (TEST_PACK_BASE + 8)
#define TEST_PACK_EXISTS (TEST_PACK_BASE + 9)

#define TEST_PACK_BASE2  (TAG_USER + 0x13f)
#define TEST_PACK_EXTRA  (TEST_PACK_BASE2 + 1)

struct TestPackStruct {
    UBYTE ubyte_value;
    BYTE byte_value;
    UWORD uword_value;
    WORD word_value;
    ULONG ulong_value;
    LONG long_value;
    UBYTE flags;
};

static const ULONG test_pack_table[] = {
    PACK_STARTTABLE(TEST_PACK_BASE),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_UBYTE, TestPackStruct, ubyte_value,
        PKCTRL_UBYTE | PKCTRL_PACKUNPACK),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_BYTE, TestPackStruct, byte_value,
        PKCTRL_BYTE | PKCTRL_PACKUNPACK),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_UWORD, TestPackStruct, uword_value,
        PKCTRL_UWORD | PKCTRL_PACKUNPACK),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_WORD, TestPackStruct, word_value,
        PKCTRL_WORD | PKCTRL_PACKUNPACK),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_ULONG, TestPackStruct, ulong_value,
        PKCTRL_ULONG | PKCTRL_PACKUNPACK),
    PACK_ENTRY(TEST_PACK_BASE, TEST_PACK_LONG, TestPackStruct, long_value,
        PKCTRL_LONG | PKCTRL_PACKUNPACK),
    PACK_BYTEBIT(TEST_PACK_BASE, TEST_PACK_BIT, TestPackStruct, flags,
        PKCTRL_BIT | PKCTRL_PACKUNPACK, 0x01),
    PACK_BYTEBIT(TEST_PACK_BASE, TEST_PACK_FLIP, TestPackStruct, flags,
        PKCTRL_FLIPBIT | PKCTRL_PACKUNPACK, 0x02),
    PACK_BYTEBIT(TEST_PACK_BASE, TEST_PACK_EXISTS, TestPackStruct, flags,
        PKCTRL_BIT | PKCTRL_PACKONLY | PSTF_EXISTS, 0x04),
    PACK_NEWOFFSET(TEST_PACK_BASE2),
    PACK_ENTRY(TEST_PACK_BASE2, TEST_PACK_EXTRA, TestPackStruct, ulong_value,
        PKCTRL_ULONG | PKCTRL_PACKUNPACK),
    PACK_ENDTABLE
};

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

static void print_ulong_value(ULONG value)
{
    char buf[16];
    int i = 0;

    if (value == 0)
    {
        print("0");
        return;
    }

    while (value != 0 && i < (int)sizeof(buf))
    {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i-- > 0)
        Write(Output(), &buf[i], 1);
}

static void print_long_value(LONG value)
{
    if (value < 0)
    {
        print("-");
        print_ulong_value((ULONG)(-value));
        return;
    }

    print_ulong_value((ULONG)value);
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
        print("  expected=");
        print_ulong_value(expected);
        print(" actual=");
        print_ulong_value(actual);
        print("\n");
        return FALSE;
    }

    pass(msg);
    return TRUE;
}

static BOOL expect_long(LONG actual, LONG expected, const char *msg)
{
    if (actual != expected)
    {
        fail(msg);
        print("  expected=");
        print_long_value(expected);
        print(" actual=");
        print_long_value(actual);
        print("\n");
        return FALSE;
    }

    pass(msg);
    return TRUE;
}

static LONG call_sdivmod32(LONG dividend, LONG divisor, LONG *remainder)
{
    register struct UtilityBase *base __asm("a6") = UtilityBase;
    register LONG quotient __asm("d0") = dividend;
    register LONG rem __asm("d1") = divisor;

    __asm volatile (
        "jsr -150(a6)"
        : "+d"(quotient), "+d"(rem)
        : "a"(base)
        : "a0", "a1", "cc", "memory");

    if (remainder)
        *remainder = rem;

    return quotient;
}

static ULONG call_udivmod32(ULONG dividend, ULONG divisor, ULONG *remainder)
{
    register struct UtilityBase *base __asm("a6") = UtilityBase;
    register ULONG quotient __asm("d0") = dividend;
    register ULONG rem __asm("d1") = divisor;

    __asm volatile (
        "jsr -156(a6)"
        : "+d"(quotient), "+d"(rem)
        : "a"(base)
        : "a0", "a1", "cc", "memory");

    if (remainder)
        *remainder = rem;

    return quotient;
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

    hook.h_Entry = (ULONG (*)())HookEntry;
    hook.h_SubEntry = (ULONG (*)())hook_subentry;
    hook.h_Data = &hook_state;

    ret = CallHookPkt(&hook, (APTR)0x1234UL, (APTR)0x5678UL);
    expect_ulong(ret, 0xCAFEBABEUL, "CallHookPkt returns hook result");
    expect_ulong(hook_state, 0xCAFEBABEUL, "CallHookPkt passes hook/object/message");
    expect_true(hook.h_Entry == (ULONG (*)())HookEntry,
        "HookEntry amiga.lib stub is installed as hook entry");

    id1 = GetUniqueID();
    id2 = GetUniqueID();
    expect_true(id2 == id1 + 1, "GetUniqueID returns monotonically increasing ids");
}

static void test_date_helpers(void)
{
    struct ClockData epoch;
    struct ClockData leap;
    struct ClockData roundtrip;
    struct ClockData valid_with_weird_wday;
    struct ClockData invalid_non_leap;
    ULONG leap_seconds;

    print("Testing Amiga2Date/Date2Amiga/CheckDate...\n");

    Amiga2Date(0, &epoch);
    expect_ulong(epoch.year, 1978, "Amiga2Date epoch year");
    expect_ulong(epoch.month, 1, "Amiga2Date epoch month");
    expect_ulong(epoch.mday, 1, "Amiga2Date epoch day");
    expect_ulong(epoch.hour, 0, "Amiga2Date epoch hour");
    expect_ulong(epoch.min, 0, "Amiga2Date epoch minute");
    expect_ulong(epoch.sec, 0, "Amiga2Date epoch second");
    expect_ulong(Date2Amiga(&epoch), 0, "Date2Amiga epoch returns zero");

    leap.sec = 56;
    leap.min = 34;
    leap.hour = 12;
    leap.mday = 29;
    leap.month = 2;
    leap.year = 2000;
    leap.wday = 2;

    leap_seconds = Date2Amiga(&leap);
    Amiga2Date(leap_seconds, &roundtrip);
    expect_ulong(roundtrip.year, 2000, "Date round-trip keeps year");
    expect_ulong(roundtrip.month, 2, "Date round-trip keeps month");
    expect_ulong(roundtrip.mday, 29, "Date round-trip keeps day");
    expect_ulong(roundtrip.hour, 12, "Date round-trip keeps hour");
    expect_ulong(roundtrip.min, 34, "Date round-trip keeps minute");
    expect_ulong(roundtrip.sec, 56, "Date round-trip keeps second");
    expect_ulong(CheckDate(&leap), leap_seconds, "CheckDate returns seconds for valid leap date");

    valid_with_weird_wday = leap;
    valid_with_weird_wday.sec = 60;
    valid_with_weird_wday.wday = 99;
    expect_ulong(CheckDate(&valid_with_weird_wday), Date2Amiga(&valid_with_weird_wday),
        "CheckDate accepts leap second and ignores weekday");

    invalid_non_leap = leap;
    invalid_non_leap.year = 2001;
    expect_ulong(CheckDate(&invalid_non_leap), 0,
        "CheckDate rejects February 29 on non-leap year");
}

static void test_math_helpers(void)
{
    LONG srem;
    LONG squot;
    ULONG urem;
    ULONG uquot;

    print("Testing SMult32/UMult32/SDivMod32/UDivMod32...\n");

    expect_long(SMult32(-7, 9), -63, "SMult32 multiplies signed values");
    expect_ulong(UMult32(0xffffffffUL, 2), 0xfffffffeUL,
        "UMult32 returns low 32 bits");

    squot = call_sdivmod32(-17, 5, &srem);
    expect_long(squot, -3, "SDivMod32 returns signed quotient");
    expect_long(srem, -2, "SDivMod32 returns signed remainder in D1");

    uquot = call_udivmod32(17, 5, &urem);
    expect_ulong(uquot, 3, "UDivMod32 returns unsigned quotient");
    expect_ulong(urem, 2, "UDivMod32 returns unsigned remainder in D1");
}

static void test_pack_structure_tags(void)
{
    struct TestPackStruct packed;
    struct TagItem tags[] = {
        { TEST_PACK_UBYTE, 0x12 },
        { TEST_PACK_BYTE, (ULONG)-5 },
        { TEST_PACK_UWORD, 0x3456 },
        { TEST_PACK_WORD, (ULONG)-1234 },
        { TEST_PACK_ULONG, 0x89abcdefUL },
        { TEST_PACK_LONG, (ULONG)-1234567 },
        { TEST_PACK_BIT, TRUE },
        { TEST_PACK_FLIP, TRUE },
        { TEST_PACK_EXISTS, 0 },
        { TEST_PACK_EXTRA, 0x13579bdfUL },
        { TAG_DONE, 0 }
    };
    ULONG count;

    print("Testing PackStructureTags...\n");

    memset(&packed, 0, sizeof(packed));
    packed.flags = 0x02;

    count = PackStructureTags(&packed, test_pack_table, tags);
    expect_ulong(count, 10, "PackStructureTags counts packed tags");
    expect_ulong(packed.ubyte_value, 0x12, "PackStructureTags packs unsigned byte");
    expect_long(packed.byte_value, -5, "PackStructureTags packs signed byte");
    expect_ulong(packed.uword_value, 0x3456, "PackStructureTags packs unsigned word");
    expect_long(packed.word_value, -1234, "PackStructureTags packs signed word");
    expect_ulong(packed.ulong_value, 0x13579bdfUL,
        "PackStructureTags handles PACK_NEWOFFSET entries");
    expect_long(packed.long_value, -1234567, "PackStructureTags packs signed long");
    expect_true((packed.flags & 0x01) != 0, "PackStructureTags sets bit fields");
    expect_true((packed.flags & 0x02) == 0, "PackStructureTags flips inverted bit fields");
    expect_true((packed.flags & 0x04) != 0, "PackStructureTags applies TAG_EXISTS hack");
}

static void test_unpack_structure_tags(void)
{
    struct TestPackStruct packed;
    ULONG unpacked_ubyte = 0;
    LONG unpacked_byte = 0;
    ULONG unpacked_uword = 0;
    LONG unpacked_word = 0;
    ULONG unpacked_ulong = 0;
    LONG unpacked_long = 0;
    ULONG unpacked_bit = 0;
    ULONG unpacked_flip = 0;
    ULONG unpacked_extra = 0;
    struct TagItem tags[] = {
        { TEST_PACK_UBYTE, (ULONG)&unpacked_ubyte },
        { TEST_PACK_BYTE, (ULONG)&unpacked_byte },
        { TEST_PACK_UWORD, (ULONG)&unpacked_uword },
        { TEST_PACK_WORD, (ULONG)&unpacked_word },
        { TEST_PACK_ULONG, (ULONG)&unpacked_ulong },
        { TEST_PACK_LONG, (ULONG)&unpacked_long },
        { TEST_PACK_BIT, (ULONG)&unpacked_bit },
        { TEST_PACK_FLIP, (ULONG)&unpacked_flip },
        { TEST_PACK_EXTRA, (ULONG)&unpacked_extra },
        { TAG_DONE, 0 }
    };
    ULONG count;

    print("Testing UnpackStructureTags...\n");

    packed.ubyte_value = 0x22;
    packed.byte_value = -7;
    packed.uword_value = 0x4567;
    packed.word_value = -2345;
    packed.ulong_value = 0x2468ace0UL;
    packed.long_value = -7654321;
    packed.flags = 0x01;

    count = UnpackStructureTags(&packed, test_pack_table, tags);
    expect_ulong(count, 9, "UnpackStructureTags counts unpacked tags");
    expect_ulong(unpacked_ubyte, 0x22, "UnpackStructureTags reads unsigned byte");
    expect_long(unpacked_byte, -7, "UnpackStructureTags reads signed byte");
    expect_ulong(unpacked_uword, 0x4567, "UnpackStructureTags reads unsigned word");
    expect_long(unpacked_word, -2345, "UnpackStructureTags reads signed word");
    expect_ulong(unpacked_ulong, 0x2468ace0UL, "UnpackStructureTags reads unsigned long");
    expect_long(unpacked_long, -7654321, "UnpackStructureTags reads signed long");
    expect_ulong(unpacked_bit, TRUE, "UnpackStructureTags reads bit fields");
    expect_ulong(unpacked_flip, TRUE, "UnpackStructureTags reads inverted bit fields");
    expect_ulong(unpacked_extra, 0x2468ace0UL,
        "UnpackStructureTags handles PACK_NEWOFFSET entries");
}

static void test_named_objects(void)
{
    struct TagItem root_tags[] = {
        { ANO_NameSpace, TRUE },
        { ANO_Flags, NSF_NODUPS },
        { TAG_DONE, 0 }
    };
    struct TagItem child_tags[] = {
        { ANO_UserSpace, 32 },
        { ANO_Priority, 7 },
        { TAG_DONE, 0 }
    };
    struct NamedObject *root;
    struct NamedObject *alpha;
    struct NamedObject *dup;
    struct NamedObject *beta;
    struct NamedObject *found;
    struct NamedObject *found_next;
    struct NamedObject *root_scan;
    struct MsgPort *reply_port;
    struct Message remove_message;
    BOOL removed;

    print("Testing NamedObject namespace helpers...\n");

    root = AllocNamedObjectA((CONST_STRPTR)"root", root_tags);
    expect_true(root != NULL, "AllocNamedObjectA allocates namespace root");
    if (!root)
        return;

    alpha = AllocNamedObjectA((CONST_STRPTR)"Alpha", child_tags);
    expect_true(alpha != NULL, "AllocNamedObjectA allocates object with user space");
    if (!alpha)
    {
        FreeNamedObject(root);
        return;
    }

    expect_true(alpha->no_Object != NULL, "AllocNamedObjectA allocates requested user space");
    expect_true(NamedObjectName(alpha) != NULL && !strcmp((char *)NamedObjectName(alpha), "Alpha"),
        "NamedObjectName returns stored object name");

    expect_true(AddNamedObject(root, alpha), "AddNamedObject inserts child into namespace");
    expect_true(!AddNamedObject(root, alpha), "AddNamedObject rejects reinserting same object");

    dup = AllocNamedObjectA((CONST_STRPTR)"Alpha", NULL);
    expect_true(dup != NULL, "AllocNamedObjectA allocates duplicate-name candidate");
    if (dup)
    {
        expect_true(!AddNamedObject(root, dup), "AddNamedObject rejects duplicate names in NSF_NODUPS namespace");
        FreeNamedObject(dup);
    }

    found = FindNamedObject(root, (CONST_STRPTR)"alpha", NULL);
    expect_true(found == alpha, "FindNamedObject matches case-insensitively by default");
    if (found)
        ReleaseNamedObject(found);

    found = FindNamedObject(root, NULL, NULL);
    expect_true(found == alpha, "FindNamedObject(NULL name) returns first object");
    found_next = FindNamedObject(root, NULL, found);
    expect_true(found_next == NULL, "FindNamedObject(NULL name,last) advances past last object");
    if (found)
        ReleaseNamedObject(found);

    found = FindNamedObject(root, (CONST_STRPTR)"Alpha", NULL);
    expect_true(found == alpha, "FindNamedObject returns object for removal-hold test");

    removed = AttemptRemNamedObject(alpha);
    expect_true(!removed, "AttemptRemNamedObject fails while find hold remains");

    if (found)
        ReleaseNamedObject(found);

    removed = AttemptRemNamedObject(alpha);
    expect_true(removed, "AttemptRemNamedObject removes object after release");
    expect_true(FindNamedObject(root, (CONST_STRPTR)"Alpha", NULL) == NULL,
        "Removed object is no longer findable");
    FreeNamedObject(alpha);

    beta = AllocNamedObjectA((CONST_STRPTR)"Beta", NULL);
    expect_true(beta != NULL, "AllocNamedObjectA allocates removable object");
    if (beta)
    {
        expect_true(AddNamedObject(root, beta), "AddNamedObject inserts second child");
        reply_port = CreateMsgPort();
        expect_true(reply_port != NULL, "CreateMsgPort allocates reply port for RemNamedObject");
        if (reply_port)
        {
            memset(&remove_message, 0, sizeof(remove_message));
            remove_message.mn_ReplyPort = reply_port;
            RemNamedObject(beta, &remove_message);
            expect_true(WaitPort(reply_port) == &remove_message, "RemNamedObject replies removal message");
            expect_true((APTR)remove_message.mn_Node.ln_Name == (APTR)beta,
                "RemNamedObject reply names removed object");
            DeleteMsgPort(reply_port);
        }
        FreeNamedObject(beta);
    }

    root_scan = FindNamedObject(NULL, (CONST_STRPTR)"root", NULL);
    expect_true(root_scan == NULL, "Root namespace stays empty until object is added there");

    FreeNamedObject(root);
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
    test_date_helpers();
    test_math_helpers();
    test_pack_structure_tags();
    test_unpack_structure_tags();
    test_named_objects();

    CloseLibrary((struct Library *)UtilityBase);

    if (errors != 0)
    {
        print("FAIL: utility/tagitems had errors\n");
        return 20;
    }

    print("PASS: utility/tagitems all tests passed\n");
    return 0;
}
