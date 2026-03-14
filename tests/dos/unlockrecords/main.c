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
    struct RecordLock invalid[3];
    struct RecordLock missing[3];
    struct RecordLock short_list[2];

    print("UnLockRecords Test\n");
    print("==================\n\n");

    fh1 = Open((CONST_STRPTR)"unlockrecords_test.dat", MODE_NEWFILE);
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

    fh1 = Open((CONST_STRPTR)"unlockrecords_test.dat", MODE_READWRITE);
    fh2 = Open((CONST_STRPTR)"unlockrecords_test.dat", MODE_READWRITE);
    fh3 = Open((CONST_STRPTR)"unlockrecords_test.dat", MODE_READWRITE);
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

    print("Test 1: UnlockRecords releases every listed record\n");
    init_record(&records[0], fh1, 0, 2, REC_EXCLUSIVE_IMMED);
    init_record(&records[1], fh1, 4, 2, REC_SHARED_IMMED);
    init_record(&records[2], 0, 0, 0, 0);
    if (!LockRecords(records, 0))
    {
        test_fail("Setup lock list", "LockRecords failed");
    }
    else
    {
        ok = UnLockRecords(records);
        if (!ok)
        {
            test_fail("Unlock list succeeds", "UnLockRecords failed");
        }
        else if (IoErr() != 0)
        {
            test_fail("Unlock list succeeds", "IoErr not cleared");
        }
        else if (!LockRecord(fh2, 0, 2, REC_EXCLUSIVE_IMMED, 0))
        {
            test_fail("Unlock list succeeds", "First record remained held");
        }
        else if (!LockRecord(fh3, 4, 2, REC_EXCLUSIVE_IMMED, 0))
        {
            test_fail("Unlock list succeeds", "Second record remained held");
            UnLockRecord(fh2, 0, 2);
        }
        else
        {
            test_pass("Unlock list succeeds");
            UnLockRecord(fh2, 0, 2);
            UnLockRecord(fh3, 4, 2);
        }
    }

    print("\nTest 2: UnlockRecords stops at the terminator entry\n");
    init_record(&short_list[0], fh1, 8, 2, REC_EXCLUSIVE_IMMED);
    init_record(&short_list[1], 0, 12, 2, REC_EXCLUSIVE_IMMED);
    if (!LockRecord(fh1, 8, 2, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup terminator probe lock", "LockRecord failed");
    }
    else if (!LockRecord(fh1, 12, 2, REC_EXCLUSIVE_IMMED, 0))
    {
        test_fail("Setup post-terminator lock", "LockRecord failed");
        UnLockRecord(fh1, 8, 2);
    }
    else
    {
        ok = UnLockRecords(short_list);
        if (!ok)
        {
            test_fail("Terminator stops unlock loop", "UnLockRecords failed");
        }
        else if (!LockRecord(fh2, 8, 2, REC_EXCLUSIVE_IMMED, 0))
        {
            test_fail("Terminator stops unlock loop", "First record remained held");
        }
        else if (LockRecord(fh3, 12, 2, REC_EXCLUSIVE_IMMED, 0))
        {
            test_fail("Terminator stops unlock loop", "Post-terminator record was unlocked");
            UnLockRecord(fh3, 12, 2);
            UnLockRecord(fh2, 8, 2);
        }
        else if (IoErr() != ERROR_LOCK_COLLISION)
        {
            test_fail("Terminator stops unlock loop", "Wrong IoErr for held tail record");
            UnLockRecord(fh2, 8, 2);
        }
        else
        {
            test_pass("Terminator stops unlock loop");
            UnLockRecord(fh2, 8, 2);
        }

        UnLockRecord(fh1, 12, 2);
    }

    print("\nTest 3: UnlockRecords reports the first failing entry\n");
    init_record(&invalid[0], fh1, 2, 2, REC_SHARED_IMMED);
    init_record(&invalid[1], fh2, 2, 2, REC_SHARED_IMMED);
    init_record(&invalid[2], 0, 0, 0, 0);
    if (!LockRecord(fh1, 2, 2, REC_SHARED_IMMED, 0))
    {
        test_fail("Setup first failing entry", "LockRecord failed");
    }
    else
    {
        ok = UnLockRecords(invalid);
        if (ok)
        {
            test_fail("First failing entry error", "Unexpected success");
        }
        else
        {
            err = IoErr();
            if (err != ERROR_RECORD_NOT_LOCKED)
            {
                test_fail("First failing entry error", "Wrong IoErr");
            }
            else if (!LockRecord(fh3, 2, 2, REC_SHARED_IMMED, 0))
            {
                test_fail("First failing entry error", "Earlier entry was not unlocked");
            }
            else
            {
                test_pass("First failing entry error");
                UnLockRecord(fh3, 2, 2);
            }
        }
    }

    print("\nTest 4: Missing record entry fails\n");
    init_record(&missing[0], fh1, 14, 1, REC_SHARED_IMMED);
    init_record(&missing[1], 0, 0, 0, 0);
    ok = UnLockRecords(missing);
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

    print("\nTest 5: Null array is rejected\n");
    ok = UnLockRecords((CONST struct RecordLock *)0);
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
    DeleteFile((CONST_STRPTR)"unlockrecords_test.dat");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
