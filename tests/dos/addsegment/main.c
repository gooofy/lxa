#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_failed = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char tmp[16];
    char *p = buf;
    int i = 0;

    if (n < 0)
    {
        *p++ = '-';
        n = -n;
    }

    do
    {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    while (i > 0)
        *p++ = tmp[--i];

    *p = '\0';
    print(buf);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

static BOOL bstr_equals_cstr(const UBYTE *bstr, const char *str)
{
    ULONG len = bstr ? bstr[0] : 0;
    ULONG i;

    if (!bstr || !str)
        return FALSE;

    for (i = 0; i < len; i++)
    {
        if (bstr[i + 1] != (UBYTE)str[i])
            return FALSE;
    }

    return str[len] == '\0';
}

static BPTR seg1, seg2, seg3;

static LONG which(const struct Segment *s)
{
    if (!s)
        return 0;
    if (s->seg_Seg == seg1)
        return 1;
    if (s->seg_Seg == seg2)
        return 2;
    if (s->seg_Seg == seg3)
        return 3;
    return 9;
}

static void check(BOOL cond, const char *name, const char *reason)
{
    if (cond)
        test_pass(name);
    else
        test_fail(name, reason);
}

int main(void)
{
    struct Segment *a;
    struct Segment *b;
    LONG ok;

    print("AddSegment Test\n");
    print("===============\n\n");

    seg1 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    seg2 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    seg3 = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/HelloWorld");
    if (!seg1 || !seg2 || !seg3)
    {
        print("FAIL: Could not load test seglists\n");
        if (seg1)
            UnLoadSeg(seg1);
        if (seg2)
            UnLoadSeg(seg2);
        if (seg3)
            UnLoadSeg(seg3);
        return 20;
    }

    print("Test 1: A user segment (use count 1) is found by FindSegment()\n");
    ok = AddSegment((CONST_STRPTR)"AddSegmentUser", seg1, 1);
    check(ok == DOSTRUE, "AddSegment succeeds", "AddSegment returned FALSE");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    check(which(a) == 1 && a->seg_UC == 1 && bstr_equals_cstr(a->seg_Name, "AddSegmentUser"),
          "FindSegment returns the new entry", "Entry not found or wrong fields");
    a = FindSegment((CONST_STRPTR)"addsegmentuser", NULL, FALSE);
    check(which(a) == 1, "FindSegment compares case-insensitively", "Lower-case lookup failed");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, TRUE);
    check(a == NULL, "User segments are not system segments", "Found as a system segment");

    print("\nTest 2: Duplicate names are allowed and prepended\n");
    ok = AddSegment((CONST_STRPTR)"addsegmentuser", seg2, 1);
    check(ok == DOSTRUE, "Duplicate AddSegment succeeds", "AddSegment returned FALSE");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    b = a ? FindSegment((CONST_STRPTR)"AddSegmentUser", a, FALSE) : NULL;
    check(which(a) == 2 && which(b) == 1, "Newest entry first, search continues after 'seg'",
          "Wrong search order");

    print("\nTest 3: Use count 0 entries are not found as user segments\n");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    a->seg_UC = 0;
    b = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    check(which(b) == 1, "seg_UC == 0 is skipped", "Use count 0 entry was returned");
    a->seg_UC = 1;

    print("\nTest 4: CMD_SYSTEM segments live in the system namespace\n");
    ok = AddSegment((CONST_STRPTR)"ADDSEGMENTUSER", seg3, CMD_SYSTEM);
    check(ok == DOSTRUE, "System AddSegment succeeds", "AddSegment returned FALSE");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, TRUE);
    check(which(a) == 3 && a->seg_UC == CMD_SYSTEM, "FindSegment(system) finds it",
          "System entry not found");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    check(which(a) == 2, "FindSegment(user) skips it", "User search returned the system entry");

    print("\nTest 5: RemSegment\n");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    a->seg_UC = 2;
    ok = RemSegment(a);
    check(ok == DOSFALSE && IoErr() == ERROR_OBJECT_IN_USE, "Segment in use is not removed",
          "Expected ERROR_OBJECT_IN_USE");
    a->seg_UC = 1;
    ok = RemSegment(a);
    check(ok == DOSTRUE, "Unused segment is removed", "RemSegment failed");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, FALSE);
    check(which(a) == 1, "Removed entry is gone", "Removed entry still found");
    a->seg_UC = 0;
    ok = RemSegment(a);
    check(ok == DOSTRUE, "Use count 0 segment is removed", "RemSegment failed");
    a = FindSegment((CONST_STRPTR)"AddSegmentUser", NULL, TRUE);
    ok = RemSegment(a);
    check(ok == DOSFALSE && IoErr() == ERROR_OBJECT_IN_USE, "System segment is not removed",
          "Expected ERROR_OBJECT_IN_USE");
    /* the CMD_SYSTEM entry (seg3) stays resident, as it would on AmigaOS */

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
