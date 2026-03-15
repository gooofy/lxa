#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

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

int main(void)
{
    BPTR sys_lock;
    BPTR dup_lock;
    BPTR sys_tests_lock;
    BPTR ram_lock;
    BOOL same;

    print("SameDevice Test\n");
    print("===============\n\n");

    sys_lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    dup_lock = sys_lock ? DupLock(sys_lock) : 0;
    sys_tests_lock = Lock((CONST_STRPTR)"SYS:Tests", SHARED_LOCK);
    ram_lock = Lock((CONST_STRPTR)"RAM:", SHARED_LOCK);

    if (!sys_lock || !dup_lock || !sys_tests_lock || !ram_lock)
    {
        test_fail("Open probe locks", "Could not open required lock set");
        if (ram_lock)
            UnLock(ram_lock);
        if (sys_tests_lock)
            UnLock(sys_tests_lock);
        if (dup_lock)
            UnLock(dup_lock);
        if (sys_lock)
            UnLock(sys_lock);

        print("\nFailed: ");
        print_num(tests_failed ? tests_failed : 1);
        print("\n");
        return 20;
    }

    print("Test 1: Duplicate locks on the same object report the same device\n");
    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    same = SameDevice(sys_lock, dup_lock);
    if (same == DOSTRUE && IoErr() == ERROR_OBJECT_NOT_FOUND)
        test_pass("Duplicate locks share a device");
    else
        test_fail("Duplicate locks share a device", "Expected DOSTRUE with unchanged IoErr");

    print("\nTest 2: Different objects on the same volume report the same device\n");
    SetIoErr(ERROR_BAD_NUMBER);
    same = SameDevice(sys_lock, sys_tests_lock);
    if (same == DOSTRUE && IoErr() == ERROR_BAD_NUMBER)
        test_pass("Same volume shares a device");
    else
        test_fail("Same volume shares a device", "Expected DOSTRUE with unchanged IoErr");

    print("\nTest 3: Locks on different devices report DOSFALSE\n");
    SetIoErr(ERROR_DEVICE_NOT_MOUNTED);
    same = SameDevice(sys_lock, ram_lock);
    if (same == DOSFALSE && IoErr() == ERROR_DEVICE_NOT_MOUNTED)
        test_pass("Different devices compare false");
    else
        test_fail("Different devices compare false", "Expected DOSFALSE with unchanged IoErr");

    print("\nTest 4: NULL first lock returns DOSFALSE\n");
    SetIoErr(ERROR_INVALID_LOCK);
    same = SameDevice(0, sys_lock);
    if (same == DOSFALSE && IoErr() == ERROR_INVALID_LOCK)
        test_pass("NULL first lock rejected");
    else
        test_fail("NULL first lock rejected", "Expected DOSFALSE with unchanged IoErr");

    print("\nTest 5: NULL second lock returns DOSFALSE\n");
    SetIoErr(ERROR_OBJECT_WRONG_TYPE);
    same = SameDevice(sys_lock, 0);
    if (same == DOSFALSE && IoErr() == ERROR_OBJECT_WRONG_TYPE)
        test_pass("NULL second lock rejected");
    else
        test_fail("NULL second lock rejected", "Expected DOSFALSE with unchanged IoErr");

    UnLock(ram_lock);
    UnLock(sys_tests_lock);
    UnLock(dup_lock);
    UnLock(sys_lock);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
