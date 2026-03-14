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
    BPTR lock1;
    BPTR lock2;
    BPTR fh;
    BOOL ok;
    LONG err;

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
    lock2 = Lock((CONST_STRPTR)"changemode_test.dat", SHARED_LOCK);
    fh = Open((CONST_STRPTR)"changemode_test.dat", MODE_READWRITE);

    if (!lock1 || !lock2 || !fh)
    {
        test_fail("Open objects", "Could not open required lock/filehandle");
        if (lock1)
            UnLock(lock1);
        if (lock2)
            UnLock(lock2);
        if (fh)
            Close(fh);
        DeleteFile((CONST_STRPTR)"changemode_test.dat");
        return 1;
    }

    print("Test 1: Shared lock can become exclusive when alone\n");
    UnLock(lock2);
    lock2 = 0;
    ok = ChangeMode(CHANGE_LOCK, lock1, EXCLUSIVE_LOCK);
    if (ok)
        test_pass("Shared->exclusive lock");
    else
        test_fail("Shared->exclusive lock", "Call failed");

    print("\nTest 2: Lock can return to shared mode\n");
    ok = ChangeMode(CHANGE_LOCK, lock1, SHARED_LOCK);
    if (ok)
        test_pass("Exclusive->shared lock");
    else
        test_fail("Exclusive->shared lock", "Call failed");

    print("\nTest 3: Exclusive upgrade is rejected while another shared lock exists\n");
    lock2 = Lock((CONST_STRPTR)"changemode_test.dat", SHARED_LOCK);
    if (!lock2)
    {
        test_fail("Reopen second lock", "Lock failed");
    }
    else
    {
        ok = ChangeMode(CHANGE_LOCK, lock1, EXCLUSIVE_LOCK);
        if (!ok)
        {
            err = IoErr();
            if (err == ERROR_OBJECT_IN_USE)
                test_pass("Exclusive upgrade conflict");
            else
                test_fail("Exclusive upgrade conflict", "Wrong IoErr");
        }
        else
        {
            test_fail("Exclusive upgrade conflict", "Unexpected success");
        }
    }

    print("\nTest 4: Filehandle ChangeMode accepts shared mode\n");
    if (lock2)
    {
        UnLock(lock2);
        lock2 = 0;
    }
    ok = ChangeMode(CHANGE_FH, fh, SHARED_LOCK);
    if (ok)
        test_pass("Filehandle shared mode");
    else
        test_fail("Filehandle shared mode", "Call failed");

    print("\nTest 5: Filehandle exclusive mode conflicts with outstanding shared lock\n");
    ok = ChangeMode(CHANGE_FH, fh, EXCLUSIVE_LOCK);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_OBJECT_IN_USE)
            test_pass("Filehandle exclusive conflict");
        else
            test_fail("Filehandle exclusive conflict", "Wrong IoErr");
    }
    else
    {
        test_fail("Filehandle exclusive conflict", "Unexpected success");
    }

    print("\nTest 6: Invalid type is rejected\n");
    ok = ChangeMode(99, lock1, SHARED_LOCK);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_BAD_NUMBER)
            test_pass("Invalid type error");
        else
            test_fail("Invalid type error", "Wrong IoErr");
    }
    else
    {
        test_fail("Invalid type error", "Unexpected success");
    }

    print("\nTest 7: Invalid new mode is rejected\n");
    ok = ChangeMode(CHANGE_LOCK, lock1, 99);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_BAD_NUMBER)
            test_pass("Invalid mode error");
        else
            test_fail("Invalid mode error", "Wrong IoErr");
    }
    else
    {
        test_fail("Invalid mode error", "Unexpected success");
    }

    print("\nTest 8: Invalid lock is rejected\n");
    ok = ChangeMode(CHANGE_LOCK, 0, SHARED_LOCK);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_INVALID_LOCK)
            test_pass("Invalid lock error");
        else
            test_fail("Invalid lock error", "Wrong IoErr");
    }
    else
    {
        test_fail("Invalid lock error", "Unexpected success");
    }

    print("\nTest 9: Wrong filehandle type is rejected\n");
    ok = ChangeMode(CHANGE_FH, lock1, SHARED_LOCK);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_OBJECT_WRONG_TYPE)
            test_pass("Wrong FH object type");
        else
            test_fail("Wrong FH object type", "Wrong IoErr");
    }
    else
    {
        test_fail("Wrong FH object type", "Unexpected success");
    }

    if (lock2)
        UnLock(lock2);
    UnLock(lock1);
    Close(fh);
    DeleteFile((CONST_STRPTR)"changemode_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
