/*
 * Probe (Phase 222f): icon.library V40 string helpers - FindToolType,
 * MatchToolValue, BumpRevision - and the obsolete FreeList functions
 * AddFreeList/FreeFreeList.  Output compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <workbench/workbench.h>
#include <clib/exec_protos.h>
#include <clib/icon_protos.h>
#include <inline/exec.h>
#include <inline/icon.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IconBase;

static STRPTR tts[] = {
    (STRPTR)"FILETYPE=text|ascii",
    (STRPTR)"(HIDDEN=yes)",
    (STRPTR)"DONOTWAIT",
    (STRPTR)" SPACED=1",
    (STRPTR)"TAB\t=2",
    (STRPTR)"EMPTY=",
    (STRPTR)"=novalue",
    (STRPTR)"",
    (STRPTR)"DUP=first",
    (STRPTR)"dup=second",
    (STRPTR)"PUBSCREEN = Workbench",
    (STRPTR)"LONGNAMEWITHSUFFIX=x",
    (STRPTR)"A=B=C",
    (STRPTR)"\xe4" "UML=1",
    (STRPTR)"(DONOTWAIT2",
    (STRPTR)"DONOTWAIT2)",
    (STRPTR)"Startpri=5",
    NULL,
};

static const char *const names[] = {
    "FILETYPE", "filetype", "FileType", "FILE", "FILETYPE=", "HIDDEN", "(HIDDEN", "(HIDDEN=yes)",
    "DONOTWAIT", "donotwait", "DONOT", "SPACED", " SPACED", "TAB", "TAB\t", "EMPTY", "", "=", "DUP",
    "PUBSCREEN", "PUBSCREEN ", "LONGNAME", "LONGNAMEWITHSUFFIX", "A", "A=B", "\xe4" "UML",
    "\xc4" "UML", "DONOTWAIT2", "(DONOTWAIT2", "STARTPRI", "MISSING", "X", "=novalue", "=NOVALUE",
    "=nov", "DONOTWAIT2)", "TAB\t=", "EMPTY=",
};

static const char *const matches[][2] = {
    {"text|ascii", "text"}, {"text|ascii", "ASCII"}, {"text|ascii", "tex"}, {"text|ascii", "text|ascii"},
    {"text|ascii", "ascii|"}, {"text|ascii", ""}, {"text", "text"}, {"TEXT", "text"}, {"text", "texts"},
    {"", ""}, {"", "x"}, {"|", ""}, {"a||b", ""}, {"a|b|", ""}, {"|a", "a"}, {"a |b", "a"},
    {"a |b", "a "}, {"a| b", "b"}, {"a| b", " b"}, {"a=b", "a"}, {"a=b", "a=b"}, {"a=b|c", "c"},
    {"yes", "YES"}, {"\xe4pfel", "\xc4PFEL"}, {"x|y|z", "z"}, {"x|y|z", "y"}, {"x|y|z", "w"},
    {"long value here", "long value here"}, {"long|value", "long|value"}, {"ab", "a"},
    {"a", "ab"}, {"a\tb", "a\tb"}, {"text|", "text"}, {"a||b", "b"}, {"||", ""}, {"a|||", ""},
    {"a|", ""}, {"x||", ""}, {"|x", ""}, {"||x", "x"}, {"a||b", "a"}, {"text|ascii", "text|"},
};

static const char *const bumps[] = {
    "foo", "copy_of_foo", "Copy_of_foo", "COPY_OF_FOO", "copy_2_of_foo", "copy_9_of_foo",
    "copy_10_of_foo", "copy_99_of_foo", "copy_123456_of_foo", "copy_0_of_foo", "copy_1_of_foo",
    "copy_of_", "copy_of", "copy_", "copy_x_of_foo", "copy_2_foo", "copy_2_of_", "copy of foo",
    "Copy of foo", "copy 2 of foo", "copy_of_copy_of_foo", "", "a",
    "abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxyz0123", "abcdefghijklmnopqrstuvwxyz0123456789",
    "copy_of_abcdefghijklmnopqrstuvwxyz", "copy_9_of_abcdefghijklmnopqrstuv", "copy_99_of_abcdefghijklmnopqrstu",
    "copy__of_foo", "copy_02_of_foo", "copy_-1_of_foo", "copy_2_Of_foo", "Copy_3_OF_Foo",
    "copy_4294967295_of_x", "copy_2147483647_of_x", "foo.info", "dir/name", "copy-of-foo",
    "copy.of.foo", "copy\tof\tfoo", "copy_of foo", "copy of_foo", "copyof_foo", "copy  of foo",
    "copy_3 of_foo", "copy_3x_of_foo", " copy_of_foo", "Copy_Of_x", "copy_of_copy_2_of_x",
    "copy_2_of_copy_of_x", "copy_99999999999_of_x", "c", "copy", "copy_3_of",
};

int main(void)
{
    ULONG i;

    IconBase = OpenLibrary((STRPTR)"icon.library", 37);
    if (!IconBase)
        return 20;

    P_SECTION("FindToolType");
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        UBYTE *r = FindToolType(tts, (STRPTR)names[i]);
        probe_s("FindToolType(\"");
        probe_s(names[i]);
        probe_s("\") = ");
        if (r) {
            LONG k;
            probe_ch('"');
            probe_s((const char *)r);
            probe_s("\" entry ");
            /* which entry and offset it points into */
            for (k = 0; tts[k]; k++) {
                const UBYTE *e = (const UBYTE *)tts[k];
                ULONG len = 0;
                while (e[len])
                    len++;
                if (r >= e && r <= e + len) {
                    probe_dec(k);
                    probe_s(" offset ");
                    probe_dec(r - e);
                    break;
                }
            }
            if (!tts[k])
                probe_s("outside");
        } else
            probe_s("NULL");
        probe_ch('\n');
    }
    {
        static STRPTR none[] = {NULL};
        static STRPTR eqonly[] = {(STRPTR)"=x", (STRPTR)"Y", NULL};
        static STRPTR sp[] = {(STRPTR)"ONE TWO=3", (STRPTR)"FOO|BAR=4", NULL};
        UBYTE *r;
        P_NULL("FindToolType(empty array, \"X\")", FindToolType(none, (STRPTR)"X"));
        P_NULL("FindToolType(empty array, \"\")", FindToolType(none, (STRPTR)""));
        r = FindToolType(eqonly, (STRPTR)"");
        P_STR("FindToolType({\"=x\",\"Y\"}, \"\")", (char *)r);
        r = FindToolType(eqonly, (STRPTR)"=x");
        P_STR("FindToolType({\"=x\",\"Y\"}, \"=x\")", (char *)r);
        r = FindToolType(sp, (STRPTR)"ONE");
        P_STR("FindToolType(ONE TWO=3, \"ONE\")", (char *)r);
        r = FindToolType(sp, (STRPTR)"ONE TWO");
        P_STR("FindToolType(ONE TWO=3, \"ONE TWO\")", (char *)r);
        r = FindToolType(sp, (STRPTR)"FOO");
        P_STR("FindToolType(FOO|BAR=4, \"FOO\")", (char *)r);
        r = FindToolType(sp, (STRPTR)"BAR");
        P_STR("FindToolType(FOO|BAR=4, \"BAR\")", (char *)r);
    }

    P_SECTION("MatchToolValue");
    for (i = 0; i < sizeof(matches) / sizeof(matches[0]); i++) {
        BOOL r = MatchToolValue((STRPTR)matches[i][0], (STRPTR)matches[i][1]);
        probe_s("MatchToolValue(\"");
        probe_s(matches[i][0]);
        probe_s("\", \"");
        probe_s(matches[i][1]);
        probe_s("\") = ");
        probe_s(r ? "TRUE" : "FALSE");
        probe_ch('\n');
    }

    P_SECTION("BumpRevision");
    for (i = 0; i < sizeof(bumps) / sizeof(bumps[0]); i++) {
        UBYTE buf[64];
        UBYTE *r;
        LONG k;
        for (k = 0; k < 64; k++)
            buf[k] = 0xaa;
        r = BumpRevision(buf, (STRPTR)bumps[i]);
        probe_s("BumpRevision(\"");
        probe_s(bumps[i]);
        probe_s("\") = ");
        if (r == buf) {
            LONG len = 0;
            while (len < 63 && buf[len])
                len++;
            probe_ch('"');
            probe_s((const char *)buf);
            probe_s("\" len ");
            probe_dec(len);
            if (buf[31] != 0xaa)
                probe_s(" wrote beyond 31 bytes");
        } else
            probe_s(r ? "other" : "NULL");
        probe_ch('\n');
    }

    P_SECTION("FreeList");
    {
        ULONG before = AvailMem(MEMF_ANY);
        struct FreeList *fl = AllocMem(sizeof(struct FreeList), MEMF_CLEAR | MEMF_PUBLIC);
        if (fl) {
            static const ULONG sizes[] = {16, 100, 8, 1, 4096, 24, 32, 48, 64, 80, 96, 112};
            struct Node *n;
            LONG k = 0;
            fl->fl_MemList.lh_Head = (struct Node *)&fl->fl_MemList.lh_Tail;
            fl->fl_MemList.lh_Tail = NULL;
            fl->fl_MemList.lh_TailPred = (struct Node *)&fl->fl_MemList.lh_Head;
            for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
                APTR m = AllocMem(sizes[i], MEMF_PUBLIC);
                BOOL ok = AddFreeList(fl, m, sizes[i]);
                probe_s("AddFreeList(size ");
                probe_dec(sizes[i]);
                probe_s(") = ");
                probe_s(ok ? "TRUE" : "FALSE");
                probe_s(" NumFree ");
                probe_dec(fl->fl_NumFree);
                probe_ch('\n');
            }
            for (n = fl->fl_MemList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
                struct MemList *ml = (struct MemList *)n;
                LONG e;
                probe_s("memlist ");
                probe_dec(k++);
                probe_s(": type ");
                probe_dec(n->ln_Type);
                probe_s(" entries ");
                probe_dec(ml->ml_NumEntries);
                probe_s(" lengths");
                for (e = 0; e < ml->ml_NumEntries && e < 32; e++) {
                    probe_ch(' ');
                    probe_dec(ml->ml_ME[e].me_Length);
                }
                probe_ch('\n');
            }
            for (i = 0; i < 9; i++) {
                UBYTE *m = FreeAlloc(fl, 10 + i, MEMF_PUBLIC | MEMF_CLEAR);
                probe_s("FreeAlloc(");
                probe_dec(10 + i);
                probe_s(") = ");
                probe_s(m ? "ptr" : "NULL");
                if (m)
                    probe_s(m[0] == 0 && m[9] == 0 ? " cleared" : " not cleared");
                probe_s(" NumFree ");
                probe_dec(fl->fl_NumFree);
                probe_ch('\n');
            }
            k = 0;
            for (n = fl->fl_MemList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
                struct MemList *ml = (struct MemList *)n;
                probe_s("memlist ");
                probe_dec(k++);
                probe_s(": entries ");
                probe_dec(ml->ml_NumEntries);
                probe_ch('\n');
            }
            FreeFreeList(fl);
            probe_s("FreeFreeList done\n");
            {
                LONG delta = (LONG)(AvailMem(MEMF_ANY) - before);
                P_LONG("memory delta (FreeList itself stays allocated)", delta);
                if (delta == -(LONG)sizeof(struct FreeList))
                    FreeMem(fl, sizeof(struct FreeList));
            }
        }
    }

    CloseLibrary(IconBase);
    return 0;
}
