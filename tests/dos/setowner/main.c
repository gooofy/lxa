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
    BPTR fh;
    BPTR lock;
    LONG ok;
    LONG err;
    struct FileInfoBlock *fib;
    CONST_STRPTR path = (CONST_STRPTR)"T:setowner_test_file";

    print("SetOwner Test\n");
    print("=============\n\n");

    fh = Open(path, MODE_NEWFILE);
    if (!fh)
    {
        test_fail("Create test file", "Could not create probe file");
        print("\nFailed: ");
        print_num(tests_failed ? tests_failed : 1);
        print("\n");
        return 20;
    }
    Close(fh);

    print("Test 1: The standard filesystems do not support ACTION_SET_OWNER\n");
    SetIoErr(0);
    ok = SetOwner(path, 0x13572468);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_ACTION_NOT_KNOWN)
        test_pass("SetOwner reports ERROR_ACTION_NOT_KNOWN");
    else
        test_fail("SetOwner reports ERROR_ACTION_NOT_KNOWN", "Unexpected result or IoErr");

    print("\nTest 2: The packet is rejected before the object is looked up\n");
    SetIoErr(0);
    ok = SetOwner((CONST_STRPTR)"T:setowner_missing_file", 0x89abcdef);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_ACTION_NOT_KNOWN)
        test_pass("Missing object reports ERROR_ACTION_NOT_KNOWN");
    else
        test_fail("Missing object reports ERROR_ACTION_NOT_KNOWN", "Unexpected result or IoErr");

    print("\nTest 3: Examine() reports no owner\n");
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    lock = Lock(path, SHARED_LOCK);
    if (fib && lock && Examine(lock, fib) && fib->fib_OwnerUID == 0 && fib->fib_OwnerGID == 0)
        test_pass("Owner fields are zero");
    else
        test_fail("Owner fields are zero", "Examine failed or owner fields set");
    if (lock)
        UnLock(lock);
    if (fib)
        FreeDosObject(DOS_FIB, fib);

    DeleteFile(path);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
