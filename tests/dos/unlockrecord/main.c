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

    print("UnLockRecord Test\n");
    print("=================\n\n");

    fh1 = Open((CONST_STRPTR)"unlockrecord_test.dat", MODE_NEWFILE);
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

    fh1 = Open((CONST_STRPTR)"unlockrecord_test.dat", MODE_READWRITE);
    fh2 = Open((CONST_STRPTR)"unlockrecord_test.dat", MODE_READWRITE);
    if (!fh1 || !fh2)
    {
        test_fail("Open readwrite handles", "Open failed");
        if (fh1)
            Close(fh1);
        if (fh2)
            Close(fh2);
        return 1;
    }

    print("Test 1: Unlock succeeds for matching lock\n");
    if (!LockRecord(fh1, 2, 4, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup matching lock", "LockRecord failed");
    }
    else
    {
        ok = UnLockRecord(fh1, 2, 4);
        if (ok && IoErr() == 0)
            test_pass("Matching unlock succeeds");
        else
            test_fail("Matching unlock succeeds", "UnLockRecord failed");
    }

    print("\nTest 2: Unlock releases record for other handles\n");
    if (!LockRecord(fh1, 2, 4, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup release probe lock", "LockRecord failed");
    }
    else if (!UnLockRecord(fh1, 2, 4))
    {
        test_fail("Release probe unlock", "UnLockRecord failed");
    }
    else if (!LockRecord(fh2, 3, 2, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Unlock releases record", "Lock remained held");
    }
    else
    {
        test_pass("Unlock releases record");
        UnLockRecord(fh2, 3, 2);
    }

    print("\nTest 3: Wrong handle cannot unlock record\n");
    if (!LockRecord(fh1, 6, 2, REC_SHARED_IMMED, 0))
    {
        test_fail("Setup wrong handle lock", "LockRecord failed");
    }
    else
    {
        ok = UnLockRecord(fh2, 6, 2);
        if (!ok)
        {
            err = IoErr();
            if (err == ERROR_RECORD_NOT_LOCKED)
                test_pass("Wrong handle error");
            else
                test_fail("Wrong handle error", "Wrong IoErr");
        }
        else
        {
            test_fail("Wrong handle error", "Unexpected success");
        }

        UnLockRecord(fh1, 6, 2);
    }

    print("\nTest 4: Offset and length must match\n");
    if (!LockRecord(fh1, 8, 4, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup exact-match lock", "LockRecord failed");
    }
    else
    {
        ok = UnLockRecord(fh1, 8, 2);
        if (!ok)
        {
            err = IoErr();
            if (err == ERROR_RECORD_NOT_LOCKED)
                test_pass("Exact range required");
            else
                test_fail("Exact range required", "Wrong IoErr");
        }
        else
        {
            test_fail("Exact range required", "Unexpected success");
        }

        UnLockRecord(fh1, 8, 4);
    }

    print("\nTest 5: Unlocking missing record fails\n");
    ok = UnLockRecord(fh1, 12, 2);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_RECORD_NOT_LOCKED)
            test_pass("Missing record error");
        else
            test_fail("Missing record error", "Wrong IoErr");
    }
    else
    {
        test_fail("Missing record error", "Unexpected success");
    }

    print("\nTest 6: Null handle is rejected\n");
    ok = UnLockRecord(0, 0, 1);
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

    Close(fh1);
    Close(fh2);
    DeleteFile((CONST_STRPTR)"unlockrecord_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
