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

/* Expect ChangeMode()/Lock() to fail with ERROR_OBJECT_IN_USE */
static void expect_in_use(const char *name, BOOL ok)
{
    if (ok)
        test_fail(name, "Unexpected success");
    else if (IoErr() != ERROR_OBJECT_IN_USE)
        test_fail(name, "Wrong IoErr");
    else
        test_pass(name);
}

/*
 * Only well-formed requests are exercised: ChangeMode() with an invalid type,
 * mode or object hands garbage to the handler on AmigaOS (an invalid type
 * hangs the reference machine).  Exclusive file-handle modes differ between
 * handlers and are not tested either.
 */
int main(void)
{
    BPTR lock1;
    BPTR lock2;
    BPTR fh;
    BOOL ok;

    print("ChangeMode Test\n");
    print("===============\n\n");

    fh = Open((CONST_STRPTR)"changemode_test.dat", MODE_NEWFILE);
    if (!fh)
    {
        test_fail("Create test file", "Open failed");
        return 1;
    }
    if (Write(fh, (CONST APTR)"mode", 4) != 4)
    {
        test_fail("Seed test file", "Write failed");
        Close(fh);
        return 1;
    }
    Close(fh);

    lock1 = Lock((CONST_STRPTR)"changemode_test.dat", SHARED_LOCK);
    if (!lock1)
    {
        test_fail("Lock test file", "Lock failed");
        DeleteFile((CONST_STRPTR)"changemode_test.dat");
        return 1;
    }

    print("Test 1: Shared lock can become exclusive when alone\n");
    ok = ChangeMode(CHANGE_LOCK, lock1, EXCLUSIVE_LOCK);
    if (ok)
        test_pass("Shared->exclusive lock");
    else
        test_fail("Shared->exclusive lock", "Call failed");

    print("\nTest 2: An exclusive lock keeps other lockers out\n");
    lock2 = Lock((CONST_STRPTR)"changemode_test.dat", SHARED_LOCK);
    expect_in_use("Shared lock refused", lock2 != 0);
    if (lock2)
        UnLock(lock2);

    print("\nTest 3: Lock can return to shared mode\n");
    ok = ChangeMode(CHANGE_LOCK, lock1, SHARED_LOCK);
    if (ok)
        test_pass("Exclusive->shared lock");
    else
        test_fail("Exclusive->shared lock", "Call failed");

    lock2 = Lock((CONST_STRPTR)"changemode_test.dat", SHARED_LOCK);
    if (lock2)
        test_pass("Second shared lock after downgrade");
    else
        test_fail("Second shared lock after downgrade", "Lock failed");

    print("\nTest 4: Exclusive upgrade is rejected while another shared lock exists\n");
    expect_in_use("Exclusive upgrade conflict", ChangeMode(CHANGE_LOCK, lock1, EXCLUSIVE_LOCK));
    if (lock2)
        UnLock(lock2);

    print("\nTest 5: Exclusive upgrade is rejected while the file is open\n");
    fh = Open((CONST_STRPTR)"changemode_test.dat", MODE_OLDFILE);
    if (!fh)
    {
        test_fail("Open with shared lock held", "Open failed");
    }
    else
    {
        test_pass("Open with shared lock held");
        expect_in_use("Exclusive upgrade with open file", ChangeMode(CHANGE_LOCK, lock1, EXCLUSIVE_LOCK));

        print("\nTest 6: Filehandle ChangeMode accepts shared mode\n");
        ok = ChangeMode(CHANGE_FH, fh, SHARED_LOCK);
        if (ok)
            test_pass("Filehandle shared mode");
        else
            test_fail("Filehandle shared mode", "Call failed");
        Close(fh);
    }

    UnLock(lock1);
    DeleteFile((CONST_STRPTR)"changemode_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    return tests_failed ? 20 : 0;
}
