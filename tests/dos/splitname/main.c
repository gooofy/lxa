#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

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

static int tests_failed = 0;

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

static BOOL streq(CONST_STRPTR a, CONST_STRPTR b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return FALSE;
        a++;
        b++;
    }

    return *a == *b;
}

int main(void)
{
    WORD pos;
    LONG err;
    char buffer[16];
    char tiny[4];

    print("SplitName Test\n");
    print("==============\n\n");

    print("Test 1: First component before separator\n");
    pos = SplitName((CONST_STRPTR)"SYS:Tools/Dir", ':', (STRPTR)buffer, 0, sizeof(buffer));
    if (pos == 4 && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"SYS"))
        test_pass("First component parsed");
    else
        test_fail("First component parsed", "Unexpected position or text");

    print("\nTest 2: Continue parsing from returned position\n");
    pos = SplitName((CONST_STRPTR)"SYS:Tools/Dir", '/', (STRPTR)buffer, 4, sizeof(buffer));
    if (pos == 10 && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"Tools"))
        test_pass("Continued component parsed");
    else
        test_fail("Continued component parsed", "Unexpected position or text");

    print("\nTest 3: Missing separator returns -1 and tail\n");
    pos = SplitName((CONST_STRPTR)"SYS:Tools/Dir", '/', (STRPTR)buffer, 10, sizeof(buffer));
    if (pos == -1 && streq((CONST_STRPTR)buffer, (CONST_STRPTR)"Dir"))
        test_pass("Tail without separator parsed");
    else
        test_fail("Tail without separator parsed", "Unexpected position or text");

    print("\nTest 4: Truncation still returns next separator position\n");
    pos = SplitName((CONST_STRPTR)"volume:component/rest", '/', (STRPTR)tiny, 7, sizeof(tiny));
    if (pos == 17 && streq((CONST_STRPTR)tiny, (CONST_STRPTR)"com"))
        test_pass("Truncation keeps next position");
    else
        test_fail("Truncation keeps next position", "Unexpected truncation result");

    print("\nTest 5: Size one buffer still null terminates\n");
    tiny[0] = 'X';
    pos = SplitName((CONST_STRPTR)"abc/def", '/', (STRPTR)tiny, 0, 1);
    if (pos == 4 && tiny[0] == '\0')
        test_pass("Size one buffer supported");
    else
        test_fail("Size one buffer supported", "Unexpected size-one behavior");

    print("\nTest 6: Separator at old position yields empty component\n");
    pos = SplitName((CONST_STRPTR)"alpha//omega", '/', (STRPTR)buffer, 6, sizeof(buffer));
    if (pos == 7 && streq((CONST_STRPTR)buffer, (CONST_STRPTR)""))
        test_pass("Empty component parsed");
    else
        test_fail("Empty component parsed", "Unexpected empty component result");

    print("\nTest 7: Position past end returns empty tail\n");
    pos = SplitName((CONST_STRPTR)"abc", '/', (STRPTR)buffer, 10, sizeof(buffer));
    if (pos == -1 && streq((CONST_STRPTR)buffer, (CONST_STRPTR)""))
        test_pass("Past-end position handled");
    else
        test_fail("Past-end position handled", "Unexpected past-end result");

    print("\nTest 8: Null name is rejected\n");
    pos = SplitName((CONST_STRPTR)0, '/', (STRPTR)buffer, 0, sizeof(buffer));
    if (pos == -1)
    {
        err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING)
            test_pass("Null name error");
        else
            test_fail("Null name error", "Wrong IoErr");
    }
    else
    {
        test_fail("Null name error", "Unexpected success");
    }

    print("\nTest 9: Null buffer is rejected\n");
    pos = SplitName((CONST_STRPTR)"abc", '/', (STRPTR)0, 0, sizeof(buffer));
    if (pos == -1)
    {
        err = IoErr();
        if (err == ERROR_REQUIRED_ARG_MISSING)
            test_pass("Null buffer error");
        else
            test_fail("Null buffer error", "Wrong IoErr");
    }
    else
    {
        test_fail("Null buffer error", "Unexpected success");
    }

    print("\nTest 10: Zero size is rejected\n");
    pos = SplitName((CONST_STRPTR)"abc", '/', (STRPTR)buffer, 0, 0);
    if (pos == -1)
    {
        err = IoErr();
        if (err == ERROR_BAD_NUMBER)
            test_pass("Zero size error");
        else
            test_fail("Zero size error", "Wrong IoErr");
    }
    else
    {
        test_fail("Zero size error", "Unexpected success");
    }

    print("\nTest 11: Negative position is rejected\n");
    pos = SplitName((CONST_STRPTR)"abc", '/', (STRPTR)buffer, -1, sizeof(buffer));
    if (pos == -1)
    {
        err = IoErr();
        if (err == ERROR_BAD_NUMBER)
            test_pass("Negative position error");
        else
            test_fail("Negative position error", "Wrong IoErr");
    }
    else
    {
        test_fail("Negative position error", "Unexpected success");
    }

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
