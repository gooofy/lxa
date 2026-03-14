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

static void init_record(struct RecordLock *record, BPTR fh, ULONG offset,
                        ULONG length, ULONG mode)
{
    record->rec_FH = fh;
    record->rec_Offset = offset;
    record->rec_Length = length;
    record->rec_Mode = mode;
}

int main(void)
{
    BPTR fh1;
    BPTR fh2;
    BPTR fh3;
    BOOL ok;
    LONG err;
    struct RecordLock records[3];
    struct RecordLock blocked[3];
    struct RecordLock invalid[3];

    print("LockRecords Test\n");
    print("================\n\n");

    fh1 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_NEWFILE);
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

    fh1 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_READWRITE);
    fh2 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_READWRITE);
    fh3 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_READWRITE);
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

    init_record(&records[0], fh1, 0, 2, REC_EXCLUSIVE_IMMED);
    init_record(&records[1], fh1, 4, 2, REC_SHARED_IMMED);
    init_record(&records[2], 0, 0, 0, 0);

    print("Test 1: Multiple records lock successfully\n");
    ok = LockRecords(records, 77);
    if (ok)
        test_pass("LockRecords success");
    else
        test_fail("LockRecords success", "LockRecords failed");

    print("\nTest 2: Conflicting later entry rolls back earlier locks\n");
    if (!LockRecord(fh2, 8, 2, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup conflicting lock", "LockRecord failed");
    }
    else
    {
        init_record(&blocked[0], fh1, 12, 2, REC_EXCLUSIVE_IMMED);
        init_record(&blocked[1], fh1, 8, 2, REC_EXCLUSIVE_IMMED);
        init_record(&blocked[2], 0, 0, 0, 0);

        ok = LockRecords(blocked, 0);
        if (ok)
        {
            test_fail("Rollback on failure", "Unexpected success");
        }
        else
        {
            err = IoErr();
            if (err != ERROR_LOCK_COLLISION)
            {
                test_fail("Rollback on failure", "Wrong IoErr");
            }
            else if (!LockRecord(fh3, 12, 2, REC_EXCLUSIVE_IMMED, 0))
            {
                test_fail("Rollback on failure", "Earlier lock remained held");
            }
            else
            {
                test_pass("Rollback on failure");
                Close(fh3);
                fh3 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_READWRITE);
                if (!fh3)
                {
                    test_fail("Reopen fh3 after rollback probe", "Open failed");
                }
            }
        }

        Close(fh2);
        fh2 = Open((CONST_STRPTR)"lockrecords_test.dat", MODE_READWRITE);
        if (!fh2)
        {
            test_fail("Reopen fh2 after conflict probe", "Open failed");
        }
    }

    print("\nTest 3: Invalid mode aborts and rolls back earlier locks\n");
    init_record(&invalid[0], fh1, 14, 1, REC_SHARED_IMMED);
    init_record(&invalid[1], fh1, 15, 1, 99);
    init_record(&invalid[2], 0, 0, 0, 0);

    ok = LockRecords(invalid, 0);
    if (ok)
    {
        test_fail("Invalid mode rollback", "Unexpected success");
    }
    else
    {
        err = IoErr();
        if (err != ERROR_BAD_NUMBER)
        {
            test_fail("Invalid mode rollback", "Wrong IoErr");
        }
        else if (!LockRecord(fh2, 14, 1, REC_EXCLUSIVE_IMMED, 0))
        {
            test_fail("Invalid mode rollback", "Earlier lock remained held");
        }
        else
        {
            test_pass("Invalid mode rollback");
        }
    }

    print("\nTest 4: Null array is rejected\n");
    ok = LockRecords((struct RecordLock *)0, 0);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_INVALID_LOCK)
            test_pass("Null array error");
        else
            test_fail("Null array error", "Wrong IoErr");
    }
    else
    {
        test_fail("Null array error", "Unexpected success");
    }

    Close(fh1);
    Close(fh2);
    Close(fh3);
    DeleteFile((CONST_STRPTR)"lockrecords_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
