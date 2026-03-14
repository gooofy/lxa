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

static ULONG get_free_mem(void)
{
    return AvailMem(MEMF_PUBLIC);
}

int main(void)
{
    struct DosList *node;
    ULONG mem_before;
    ULONG mem_after;
    ULONG mem_leaked;
    LONG err;
    int i;

    print("FreeDosEntry Test\n");
    print("=================\n\n");

    print("Test 1: Frees entries returned by MakeDosEntry without leaking memory\n");
    mem_before = get_free_mem();
    for (i = 0; i < 32; i++)
    {
        node = MakeDosEntry((CONST_STRPTR)"FREEDOSENTRY", DLT_DEVICE);
        if (!node)
        {
            test_fail("Free MakeDosEntry result", "MakeDosEntry failed during the allocation/free cycle");
            print("\nFailed: ");
            print_num(tests_failed);
            print("\n");
            return 20;
        }

        FreeDosEntry(node);
    }
    mem_after = get_free_mem();
    mem_leaked = (mem_before >= mem_after) ? (mem_before - mem_after) : 0;
    if (mem_leaked <= 256)
        test_pass("Free MakeDosEntry result");
    else
        test_fail("Free MakeDosEntry result", "Repeated FreeDosEntry calls appear to leak memory");

    print("\nTest 2: Accepts NULL without disturbing IoErr\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    FreeDosEntry(NULL);
    err = IoErr();
    if (err == ERROR_OBJECT_NOT_FOUND)
        test_pass("NULL is a no-op");
    else
        test_fail("NULL is a no-op", "FreeDosEntry(NULL) changed IoErr");

    print("\nTest 3: Frees a single entry without disturbing IoErr\n");
    node = MakeDosEntry((CONST_STRPTR)"ONEENTRY", DLT_DIRECTORY);
    if (!node)
    {
        test_fail("Single entry free", "MakeDosEntry failed");
    }
    else
    {
        SetIoErr(ERROR_BAD_NUMBER);
        FreeDosEntry(node);
        err = IoErr();
        if (err == ERROR_BAD_NUMBER)
            test_pass("Single entry free");
        else
            test_fail("Single entry free", "FreeDosEntry changed IoErr while freeing an entry");
    }

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
