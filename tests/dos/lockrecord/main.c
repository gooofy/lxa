#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/record.h>
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

int main(void)
{
    BPTR fh1;
    BPTR fh2;
    BOOL ok;
    LONG err;

    print("LockRecord Test\n");
    print("===============\n\n");

    fh1 = Open((CONST_STRPTR)"lockrecord_test.dat", MODE_NEWFILE);
    if (!fh1)
    {
        test_fail("Open writer handle", "Open failed");
        return 1;
    }

    if (Write(fh1, (CONST APTR)"0123456789abcdef", 16) != 16)
    {
        test_fail("Seed file", "Write failed");
        Close(fh1);
        return 1;
    }

    Close(fh1);

    fh1 = Open((CONST_STRPTR)"lockrecord_test.dat", MODE_READWRITE);
    fh2 = Open((CONST_STRPTR)"lockrecord_test.dat", MODE_READWRITE);
    if (!fh1 || !fh2)
    {
        test_fail("Open readwrite handles", "Open failed");
        if (fh1)
            Close(fh1);
        if (fh2)
            Close(fh2);
        return 1;
    }

    print("Test 1: Exclusive immediate lock succeeds\n");
    ok = LockRecord(fh1, 2, 4, REC_EXCLUSIVE_IMMED, 99);
    if (ok)
        test_pass("Exclusive immediate lock");
    else
        test_fail("Exclusive immediate lock", "LockRecord failed");

    print("\nTest 2: Same handle can relock same record\n");
    ok = LockRecord(fh1, 2, 4, REC_EXCLUSIVE_IMMED, 0);
    if (ok)
        test_pass("Same handle relock");
    else
        test_fail("Same handle relock", "LockRecord failed");

    print("\nTest 3: Other handle gets collision for overlapping exclusive lock\n");
    ok = LockRecord(fh2, 3, 2, REC_EXCLUSIVE_IMMED, 0);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_LOCK_COLLISION)
            test_pass("Exclusive collision error");
        else
            test_fail("Exclusive collision error", "Wrong IoErr");
    }
    else
    {
        test_fail("Exclusive collision error", "Unexpected success");
    }

    print("\nTest 4: Blocking mode times out on conflict\n");
    ok = LockRecord(fh2, 3, 2, REC_EXCLUSIVE, 0);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_LOCK_TIMEOUT)
            test_pass("Timeout error");
        else
            test_fail("Timeout error", "Wrong IoErr");
    }
    else
    {
        test_fail("Timeout error", "Unexpected success");
    }

    print("\nTest 5: Shared locks coexist across handles\n");
    ok = LockRecord(fh1, 10, 2, REC_SHARED_IMMED, 0);
    if (!ok)
    {
        test_fail("First shared lock", "LockRecord failed");
    }
    else if (!LockRecord(fh2, 10, 2, REC_SHARED_IMMED, 0))
    {
        test_fail("Second shared lock", "LockRecord failed");
    }
    else
    {
        test_pass("Shared locks coexist");
    }

    print("\nTest 6: Invalid mode is rejected\n");
    ok = LockRecord(fh2, 0, 1, 99, 0);
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

    print("\nTest 7: Null handle is rejected\n");
    ok = LockRecord(0, 0, 1, REC_SHARED_IMMED, 0);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_INVALID_LOCK)
            test_pass("Null handle error");
        else
            test_fail("Null handle error", "Wrong IoErr");
    }
    else
    {
        test_fail("Null handle error", "Unexpected success");
    }

    print("\nTest 8: Close releases held records\n");
    Close(fh1);
    ok = LockRecord(fh2, 2, 4, REC_EXCLUSIVE_IMMED, 0);
    if (ok)
        test_pass("Close releases records");
    else
        test_fail("Close releases records", "Lock remained held");

    Close(fh2);
    DeleteFile((CONST_STRPTR)"lockrecord_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
