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

static void free_makedosentry_result(struct DosList *node)
{
    if (!node)
        return;

    if (node->dol_Name)
        FreeVec((APTR)BADDR(node->dol_Name));
    FreeVec(node);
}

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

static LONG bstr_equals(struct DosList *node, const char *expected)
{
    UBYTE *buffer;
    ULONG len = 0;
    ULONG i;

    if (!node)
        return FALSE;

    buffer = (UBYTE *)BADDR(node->dol_Name);
    if (!buffer)
        return FALSE;

    while (expected[len])
        len++;

    if (len > 255)
        len = 255;

    if (buffer[0] != (UBYTE)len)
        return FALSE;

    for (i = 0; i < len; i++)
    {
        if (buffer[i + 1] != (UBYTE)expected[i])
            return FALSE;
    }

    return buffer[len + 1] == '\0';
}

int main(void)
{
    struct DosList *node;
    LONG err;
    char long_name[301];
    int i;

    print("MakeDosEntry Test\n");
    print("=================\n\n");

    print("Test 1: Allocates a cleared DosList with the requested type and name\n");
    node = MakeDosEntry((CONST_STRPTR)"SYS", DLT_DEVICE);
    err = IoErr();
    if (node && err == 0 && node->dol_Type == DLT_DEVICE && node->dol_Next == 0 &&
        node->dol_Task == NULL && node->dol_Lock == 0 && bstr_equals(node, "SYS"))
        test_pass("Create device entry");
    else
        test_fail("Create device entry", "Returned entry state did not match the documented contract");
    free_makedosentry_result(node);

    print("\nTest 2: Preserves assign types and null-terminates the BSTR payload\n");
    node = MakeDosEntry((CONST_STRPTR)"Work", DLT_DIRECTORY);
    err = IoErr();
    if (node && err == 0 && node->dol_Type == DLT_DIRECTORY && bstr_equals(node, "Work"))
        test_pass("Create assign entry");
    else
        test_fail("Create assign entry", "Assign entry was not initialized correctly");
    free_makedosentry_result(node);

    print("\nTest 3: Truncates names longer than 255 characters to BCPL length\n");
    for (i = 0; i < 300; i++)
        long_name[i] = 'A' + (i % 26);
    long_name[300] = '\0';

    node = MakeDosEntry((CONST_STRPTR)long_name, DLT_VOLUME);
    err = IoErr();
    if (node && err == 0 && node->dol_Type == DLT_VOLUME && bstr_equals(node, long_name))
        test_pass("Truncate long name");
    else
        test_fail("Truncate long name", "Long-name handling did not produce a valid BCPL string");
    free_makedosentry_result(node);

    print("\nTest 4: Rejects NULL names\n");
    node = MakeDosEntry(NULL, DLT_DEVICE);
    err = IoErr();
    if (!node && err == ERROR_REQUIRED_ARG_MISSING)
        test_pass("Reject NULL name");
    else
        test_fail("Reject NULL name", "NULL name handling behaved incorrectly");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
