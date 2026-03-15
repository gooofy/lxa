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
    LONG owner_info;
    struct FileInfoBlock fib;
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

    print("Test 1: Rejects NULL names\n");
    ok = SetOwner(NULL, 0x12345678);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_REQUIRED_ARG_MISSING)
        test_pass("Reject NULL name");
    else
        test_fail("Reject NULL name", "Expected ERROR_REQUIRED_ARG_MISSING");

    print("\nTest 2: Rejects missing objects\n");
    ok = SetOwner((CONST_STRPTR)"T:setowner_missing_file", 0x89abcdef);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_OBJECT_NOT_FOUND)
        test_pass("Reject missing file");
    else
        test_fail("Reject missing file", "Expected ERROR_OBJECT_NOT_FOUND");

    print("\nTest 3: Updates owner information returned by Examine()\n");
    owner_info = 0x13572468;
    ok = SetOwner(path, owner_info);
    if (!ok)
    {
        test_fail("Set owner metadata", "SetOwner returned FALSE");
    }
    else
    {
        lock = Lock(path, SHARED_LOCK);
        if (!lock)
        {
            test_fail("Set owner metadata", "Lock failed after SetOwner");
        }
        else if (!Examine(lock, &fib))
        {
            test_fail("Set owner metadata", "Examine failed after SetOwner");
            UnLock(lock);
        }
        else if ((((LONG)fib.fib_OwnerUID << 16) | fib.fib_OwnerGID) == owner_info)
        {
            test_pass("Set owner metadata");
            UnLock(lock);
        }
        else
        {
            test_fail("Set owner metadata", "Examine owner fields did not match SetOwner value");
            UnLock(lock);
        }
    }

    DeleteFile(path);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
