/*
 * Probe (Phase 222a): utility.library tag-list functions - NextTagItem,
 * FindTagItem, GetTagData, PackBoolTags, FilterTagChanges, MapTags,
 * AllocateTagItems, CloneTagItems, RefreshTagItemClones, FreeTagItems,
 * TagInArray, FilterTagItems, ApplyTagChanges, PackStructureTags,
 * UnpackStructureTags, CallHookPkt.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <utility/pack.h>
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/utility.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *UtilityBase;

#define U(n) (TAG_USER + (n))

/* print a tag list as walked by NextTagItem (system tags handled) */
static void walk(const char *label, struct TagItem *list)
{
    struct TagItem *state = list, *ti;
    int n = 0;
    probe_s(label);
    probe_s(" =");
    while ((ti = NextTagItem(&state)) != NULL) {
        probe_ch(' ');
        if (ti->ti_Tag >= TAG_USER) {
            probe_ch('U');
            probe_dec((LONG)(ti->ti_Tag - TAG_USER));
        } else
            probe_dec((LONG)ti->ti_Tag);
        probe_ch(':');
        probe_dec((LONG)ti->ti_Data);
        if (++n > 40)
            break;
    }
    probe_ch('\n');
}

/* print raw array up to TAG_DONE (no system-tag interpretation) */
static void raw(const char *label, struct TagItem *list, int max)
{
    int i;
    probe_s(label);
    probe_s(" =");
    for (i = 0; i < max; i++) {
        probe_ch(' ');
        if (list[i].ti_Tag >= TAG_USER) {
            probe_ch('U');
            probe_dec((LONG)(list[i].ti_Tag - TAG_USER));
        } else
            probe_dec((LONG)list[i].ti_Tag);
        probe_ch(':');
        probe_dec((LONG)list[i].ti_Data);
        if (list[i].ti_Tag == TAG_DONE)
            break;
    }
    probe_ch('\n');
}

/* Hook entry: a0 = hook, a2 = object, a1 = message -> C function(hook, obj, msg) */
__asm__(
    "    .text\n"
    "    .globl _probe_hookentry\n"
    "_probe_hookentry:\n"
    "    move.l a1,-(sp)\n"
    "    move.l a2,-(sp)\n"
    "    move.l a0,-(sp)\n"
    "    move.l 12(a0),a0\n"
    "    jsr (a0)\n"
    "    lea 12(sp),sp\n"
    "    rts\n");
extern ULONG probe_hookentry(void);

static ULONG hookfunc(struct Hook *h, APTR obj, APTR msg)
{
    return (ULONG)h->h_Data + (ULONG)obj * 100 + *(ULONG *)msg * 10000;
}

struct PackTest {
    BYTE b;
    UBYTE ub;
    WORD w;
    UWORD uw;
    LONG l;
    ULONG flags;
    UWORD wflags;
    UBYTE bflags;
    UBYTE pad;
    LONG ponly;
    LONG uonly;
};

#define PB U(100)
static ULONG packtab[] = {
    PACK_STARTTABLE(PB),
    PACK_ENTRY(PB, PB + 0, PackTest, b, PKCTRL_BYTE | PKCTRL_PACKUNPACK),
    PACK_ENTRY(PB, PB + 1, PackTest, ub, PKCTRL_UBYTE | PKCTRL_PACKUNPACK),
    PACK_ENTRY(PB, PB + 2, PackTest, w, PKCTRL_WORD | PKCTRL_PACKUNPACK),
    PACK_ENTRY(PB, PB + 3, PackTest, uw, PKCTRL_UWORD | PKCTRL_PACKUNPACK),
    PACK_ENTRY(PB, PB + 4, PackTest, l, PKCTRL_LONG | PKCTRL_PACKUNPACK),
    PACK_LONGBIT(PB, PB + 5, PackTest, flags, PKCTRL_BIT | PKCTRL_PACKUNPACK, 0x00000001),
    PACK_LONGBIT(PB, PB + 6, PackTest, flags, PKCTRL_BIT | PKCTRL_PACKUNPACK, 0x00010000),
    PACK_LONGBIT(PB, PB + 7, PackTest, flags, PKCTRL_FLIPBIT | PKCTRL_PACKUNPACK, 0x00000080),
    PACK_WORDBIT(PB, PB + 8, PackTest, wflags, PKCTRL_BIT | PKCTRL_PACKUNPACK, 0x0200),
    PACK_BYTEBIT(PB, PB + 9, PackTest, bflags, PKCTRL_BIT | PKCTRL_PACKUNPACK, 0x04),
    PACK_ENTRY(PB, PB + 10, PackTest, ponly, PKCTRL_LONG | PKCTRL_PACKONLY),
    PACK_ENTRY(PB, PB + 11, PackTest, uonly, PKCTRL_LONG | PKCTRL_UNPACKONLY),
    PACK_NEWOFFSET(U(300)),
    PACK_ENTRY(U(300), U(301), PackTest, pad, PKCTRL_UBYTE | PKCTRL_PACKUNPACK),
    PACK_ENDTABLE
};

static void print_pack(const char *label, struct PackTest *p)
{
    probe_s(label);
    probe_s(": b=");
    probe_dec(p->b);
    probe_s(" ub=");
    probe_dec(p->ub);
    probe_s(" w=");
    probe_dec(p->w);
    probe_s(" uw=");
    probe_dec(p->uw);
    probe_s(" l=");
    probe_dec(p->l);
    probe_s(" flags=");
    probe_hex(p->flags, 8);
    probe_s(" wflags=");
    probe_hex(p->wflags, 4);
    probe_s(" bflags=");
    probe_hex(p->bflags, 2);
    probe_s(" pad=");
    probe_dec(p->pad);
    probe_s(" ponly=");
    probe_dec(p->ponly);
    probe_s(" uonly=");
    probe_dec(p->uonly);
    probe_ch('\n');
}

int main(void)
{
    UtilityBase = OpenLibrary((STRPTR)"utility.library", 37);
    if (!UtilityBase)
        return 20;

    P_SECTION("NextTagItem");
    {
        struct TagItem more[] = {{U(20), 200}, {TAG_IGNORE, 0}, {U(21), 210}, {TAG_DONE, 0}};
        struct TagItem list[] = {
            {U(1), 1}, {TAG_IGNORE, 5}, {U(2), 2}, {TAG_SKIP, 2}, {U(3), 3}, {U(4), 4},
            {U(5), 5}, {TAG_MORE, (ULONG)more}, {U(99), 99}
        };
        struct TagItem empty[] = {{TAG_DONE, 0}};
        struct TagItem moreonly[] = {{TAG_MORE, 0}, {U(1), 1}, {TAG_DONE, 0}};
        struct TagItem skip0[] = {{TAG_SKIP, 0}, {U(1), 1}, {U(2), 2}, {TAG_DONE, 0}};
        struct TagItem *state = NULL;
        walk("walk list", list);
        walk("walk empty", empty);
        walk("walk TAG_MORE NULL", moreonly);
        walk("walk TAG_SKIP 0", skip0);
        P_NULL("NextTagItem(&NULL)", NextTagItem(&state));
        P_LONG("GetTagData via TAG_MORE", GetTagData(U(21), -1, list));
        P_LONG("GetTagData skipped U4", GetTagData(U(4), -1, list));
        P_LONG("GetTagData after TAG_MORE", GetTagData(U(99), -1, list));
        P_LONG("GetTagData TAG_IGNORE", GetTagData(TAG_IGNORE, -1, list));
        P_LONG("GetTagData NULL list", GetTagData(U(1), 77, NULL));
        P_NULL("FindTagItem NULL list", FindTagItem(U(1), NULL));
        {
            struct TagItem *ti = FindTagItem(U(20), list);
            P_BOOL("FindTagItem U20 in more[]", ti == &more[0]);
            ti = FindTagItem(TAG_SKIP, list);
            P_NULL("FindTagItem(TAG_SKIP)", ti);
            ti = FindTagItem(TAG_MORE, list);
            P_NULL("FindTagItem(TAG_MORE)", ti);
        }
    }

    P_SECTION("TagInArray");
    {
        static Tag arr[] = {U(1), U(2), TAG_IGNORE, U(3), TAG_DONE};
        static Tag none[] = {TAG_DONE};
        P_BOOL("TagInArray U1", TagInArray(U(1), arr));
        P_BOOL("TagInArray U3", TagInArray(U(3), arr));
        P_BOOL("TagInArray TAG_IGNORE", TagInArray(TAG_IGNORE, arr));
        P_BOOL("TagInArray TAG_DONE", TagInArray(TAG_DONE, arr));
        P_BOOL("TagInArray empty", TagInArray(U(1), none));
    }

    P_SECTION("PackBoolTags");
    {
        struct TagItem boolmap[] = {{U(1), 0x01}, {U(2), 0x02}, {U(3), 0x0c}, {U(4), 0x100}, {TAG_DONE, 0}};
        struct TagItem t1[] = {{U(1), TRUE}, {U(2), FALSE}, {U(3), 5}, {U(9), TRUE}, {TAG_DONE, 0}};
        struct TagItem t2[] = {{U(2), TRUE}, {U(1), FALSE}, {U(4), TRUE}, {U(1), TRUE}, {TAG_DONE, 0}};
        struct TagItem t3[] = {{TAG_DONE, 0}};
        P_HEX("PackBoolTags(0, t1)", PackBoolTags(0, t1, boolmap));
        P_HEX("PackBoolTags(0xff, t1)", PackBoolTags(0xff, t1, boolmap));
        P_HEX("PackBoolTags(0x0f, t2)", PackBoolTags(0x0f, t2, boolmap));
        P_HEX("PackBoolTags(0x1234, empty)", PackBoolTags(0x1234, t3, boolmap));
    }

    P_SECTION("FilterTagItems");
    {
        static Tag filt[] = {U(2), U(4), TAG_DONE};
        struct TagItem a[] = {{U(1), 1}, {U(2), 2}, {U(3), 3}, {U(4), 4}, {TAG_IGNORE, 9}, {U(2), 22}, {TAG_DONE, 0}};
        struct TagItem b[] = {{U(1), 1}, {U(2), 2}, {U(3), 3}, {U(4), 4}, {TAG_IGNORE, 9}, {U(2), 22}, {TAG_DONE, 0}};
        P_LONG("FilterTagItems AND count", FilterTagItems(a, filt, TAGFILTER_AND));
        raw("FilterTagItems AND result", a, 8);
        P_LONG("FilterTagItems NOT count", FilterTagItems(b, filt, TAGFILTER_NOT));
        raw("FilterTagItems NOT result", b, 8);
        P_LONG("FilterTagItems NULL list", FilterTagItems(NULL, filt, TAGFILTER_AND));
    }

    P_SECTION("FilterTagChanges/ApplyTagChanges");
    {
        struct TagItem orig[] = {{U(1), 10}, {U(2), 20}, {U(3), 30}, {TAG_DONE, 0}};
        struct TagItem chg[] = {{U(1), 10}, {U(2), 21}, {U(5), 50}, {U(3), 31}, {TAG_DONE, 0}};
        struct TagItem orig2[] = {{U(1), 10}, {U(2), 20}, {U(3), 30}, {TAG_DONE, 0}};
        struct TagItem chg2[] = {{U(1), 11}, {U(2), 20}, {TAG_DONE, 0}};
        struct TagItem orig3[] = {{U(1), 10}, {U(2), 20}, {U(3), 30}, {TAG_DONE, 0}};
        struct TagItem chg3[] = {{U(3), 33}, {U(7), 77}, {U(1), 11}, {TAG_DONE, 0}};
        FilterTagChanges(chg, orig, TRUE);
        raw("FilterTagChanges(apply) changes", chg, 6);
        raw("FilterTagChanges(apply) orig", orig, 5);
        FilterTagChanges(chg2, orig2, FALSE);
        raw("FilterTagChanges(noapply) changes", chg2, 5);
        raw("FilterTagChanges(noapply) orig", orig2, 5);
        ApplyTagChanges(orig3, chg3);
        raw("ApplyTagChanges orig", orig3, 5);
        raw("ApplyTagChanges changes", chg3, 5);
    }

    P_SECTION("MapTags");
    {
        struct TagItem map[] = {{U(1), U(11)}, {U(2), U(12)}, {U(3), TAG_IGNORE}, {TAG_DONE, 0}};
        struct TagItem a[] = {{U(1), 1}, {U(2), 2}, {U(3), 3}, {U(4), 4}, {TAG_DONE, 0}};
        struct TagItem b[] = {{U(1), 1}, {U(2), 2}, {U(3), 3}, {U(4), 4}, {TAG_DONE, 0}};
        MapTags(a, map, MAP_REMOVE_NOT_FOUND);
        raw("MapTags REMOVE_NOT_FOUND", a, 6);
        MapTags(b, map, MAP_KEEP_NOT_FOUND);
        raw("MapTags KEEP_NOT_FOUND", b, 6);
    }

    P_SECTION("AllocateTagItems/CloneTagItems/RefreshTagItemClones");
    {
        struct TagItem more[] = {{U(20), 200}, {TAG_DONE, 0}};
        struct TagItem src[] = {{U(1), 1}, {TAG_IGNORE, 0}, {U(2), 2}, {TAG_MORE, (ULONG)more}};
        struct TagItem *t = AllocateTagItems(4);
        struct TagItem *c;
        P_NULL("AllocateTagItems(4)", t);
        if (t) {
            raw("AllocateTagItems contents", t, 4);
            FreeTagItems(t);
        }
        c = CloneTagItems(src);
        P_NULL("CloneTagItems", c);
        if (c) {
            raw("CloneTagItems raw", c, 6);
            walk("CloneTagItems walk", c);
            c[0].ti_Data = 999;
            c[1].ti_Data = 888;
            src[2].ti_Data = 22;
            RefreshTagItemClones(c, src);
            walk("RefreshTagItemClones walk", c);
            FreeTagItems(c);
        }
        c = CloneTagItems(NULL);
        P_NULL("CloneTagItems(NULL)", c);
        if (c) {
            raw("CloneTagItems(NULL) raw", c, 2);
            FreeTagItems(c);
        }
        FreeTagItems(NULL);
        P_LONG("FreeTagItems(NULL) survived", 1);
    }

    P_SECTION("PackStructureTags/UnpackStructureTags");
    {
        struct PackTest p;
        struct TagItem in[] = {
            {PB + 0, -5}, {PB + 1, 0x1fe}, {PB + 2, -1000}, {PB + 3, 0x18000}, {PB + 4, 123456},
            {PB + 5, TRUE}, {PB + 6, TRUE}, {PB + 7, FALSE}, {PB + 8, TRUE}, {PB + 9, TRUE},
            {PB + 10, 42}, {PB + 11, 43}, {U(301), 7}, {TAG_DONE, 0}
        };
        struct TagItem in2[] = {{PB + 5, FALSE}, {PB + 7, TRUE}, {PB + 8, FALSE}, {TAG_DONE, 0}};
        struct TagItem out[] = {
            {PB + 0, 0}, {PB + 1, 0}, {PB + 2, 0}, {PB + 3, 0}, {PB + 4, 0}, {PB + 5, 77},
            {PB + 6, 77}, {PB + 7, 77}, {PB + 8, 77}, {PB + 9, 77}, {PB + 10, 77}, {PB + 11, 77},
            {U(301), 0}, {U(999), 55}, {TAG_DONE, 0}
        };
        int i;
        for (i = 0; i < (int)sizeof(p); i++)
            ((UBYTE *)&p)[i] = 0;
        P_LONG("PackStructureTags count", PackStructureTags(&p, packtab, in));
        print_pack("packed", &p);
        P_LONG("PackStructureTags count 2", PackStructureTags(&p, packtab, in2));
        print_pack("packed 2", &p);
        p.uonly = 9;
        p.ponly = 8;
        P_LONG("UnpackStructureTags count", UnpackStructureTags(&p, packtab, out));
        raw("unpacked", out, 16);
    }

    P_SECTION("CallHookPkt");
    {
        struct Hook h;
        ULONG msg = 3;
        h.h_Entry = (ULONG (*)())probe_hookentry;
        h.h_SubEntry = (ULONG (*)())hookfunc;
        h.h_Data = (APTR)7;
        P_LONG("CallHookPkt", CallHookPkt(&h, (APTR)5, &msg));
    }

    CloseLibrary(UtilityBase);
    return 0;
}
