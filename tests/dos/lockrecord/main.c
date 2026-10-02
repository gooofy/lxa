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

/* Expect a failed lock call with the given IoErr() */
static void expect_fail(const char *name, BOOL ok, LONG want)
{
    LONG err = IoErr();

    if (ok)
        test_fail(name, "Unexpected success");
    else if (err != want)
    {
        test_fail(name, "Wrong IoErr");
        print("    IoErr: ");
        print_num(err);
        print("\n");
    }
    else
        test_pass(name);
}

static void expect_ok(const char *name, BOOL ok)
{
    if (ok)
        test_pass(name);
    else
        test_fail(name, "LockRecord failed");
}

/*
 * Record locking is implemented by the handler.  The test file lives on RAM:
 * so the reference run exercises AmigaOS' own ram-handler.  Only well-formed
 * requests are made (no NULL handles or invalid modes), each record is
 * locked once per handle, and handles are closed only after their records
 * were unlocked: those cases are handler-specific.
 */
#define TEST_FILE "RAM:lockrecord_test.dat"

int main(void)
{
    BPTR fh1;
    BPTR fh2;
    BPTR fh3;

    print("LockRecord Test\n");
    print("===============\n\n");

    fh1 = Open((CONST_STRPTR)TEST_FILE, MODE_NEWFILE);
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

    fh1 = Open((CONST_STRPTR)TEST_FILE, MODE_READWRITE);
    fh2 = Open((CONST_STRPTR)TEST_FILE, MODE_READWRITE);
    fh3 = Open((CONST_STRPTR)TEST_FILE, MODE_READWRITE);
    if (!fh1 || !fh2 || !fh3)
    {
        test_fail("Open readwrite handles", "Open failed");
        if (fh1)
            Close(fh1);
        if (fh2)
            Close(fh2);
        if (fh3)
            Close(fh3);
        return 1;
    }

    print("Test 1: Exclusive immediate lock succeeds\n");
    expect_ok("Exclusive immediate lock", LockRecord(fh1, 2, 4, REC_EXCLUSIVE_IMMED, 99));

    print("\nTest 2: Other handle gets collision for overlapping exclusive lock\n");
    expect_fail("Exclusive collision error", LockRecord(fh2, 3, 2, REC_EXCLUSIVE_IMMED, 0), ERROR_LOCK_COLLISION);

    print("\nTest 3: Blocking mode times out on conflict\n");
    expect_fail("Timeout error", LockRecord(fh2, 3, 2, REC_EXCLUSIVE, 5), ERROR_LOCK_TIMEOUT);

    print("\nTest 4: Shared lock collides with an exclusive record\n");
    expect_fail("Shared vs exclusive collision", LockRecord(fh2, 3, 2, REC_SHARED_IMMED, 0), ERROR_LOCK_COLLISION);

    print("\nTest 5: Shared locks coexist across handles\n");
    expect_ok("First shared lock", LockRecord(fh1, 10, 2, REC_SHARED_IMMED, 0));
    expect_ok("Second shared lock", LockRecord(fh2, 10, 2, REC_SHARED_IMMED, 0));
    expect_fail("Exclusive vs shared collision", LockRecord(fh3, 10, 2, REC_EXCLUSIVE_IMMED, 0), ERROR_LOCK_COLLISION);

    print("\nTest 6: Unlocking frees the record for other handles\n");
    expect_ok("UnLockRecord", UnLockRecord(fh1, 2, 4));
    expect_ok("Lock after unlock", LockRecord(fh2, 3, 2, REC_EXCLUSIVE_IMMED, 0));

    print("\nTest 7: Unlocking a record that is not held fails\n");
    expect_fail("Unlock not held", UnLockRecord(fh3, 3, 2), ERROR_RECORD_NOT_LOCKED);

    UnLockRecord(fh2, 3, 2);
    UnLockRecord(fh1, 10, 2);
    UnLockRecord(fh2, 10, 2);
    Close(fh1);
    Close(fh2);
    Close(fh3);
    DeleteFile((CONST_STRPTR)TEST_FILE);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    return tests_failed ? 20 : 0;
}
