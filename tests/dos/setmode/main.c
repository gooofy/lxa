#include <exec/types.h>
#include <dos/dos.h>
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

static void fill_bytes(char *buf, LONG len, char value)
{
    LONG i;

    for (i = 0; i < len; i++)
        buf[i] = value;
}

int main(void)
{
    BPTR fh;
    LONG len;
    BOOL ok;
    LONG err;
    char buf[8];

    print("SetMode Test\n");
    print("============\n\n");

    fh = Open((CONST_STRPTR)"CON:0/0/320/80/SetMode Test", MODE_OLDFILE);
    if (fh == 0)
    {
        print("FAIL: Could not open CON: console\n");
        return 20;
    }

    print("Test 1: SetMode(1) switches to RAW mode\n");
    ok = SetMode(fh, 1);
    if (!ok)
    {
        test_fail("SetMode to RAW mode", "Call failed");
    }
    else
    {
        fill_bytes(buf, sizeof(buf), '?');
        print("Waiting for RAW input: a\n");
        len = Read(fh, (APTR)buf, 1);
        if (len == 1 && buf[0] == 'a')
            test_pass("Raw mode immediate read");
        else
            test_fail("Raw mode immediate read", "Unexpected initial RAW read result");
    }

    print("\nTest 2: SetMode(0) returns to CON mode\n");
    ok = SetMode(fh, 0);
    if (!ok)
    {
        test_fail("SetMode to CON mode", "Call failed");
    }
    else
    {
        test_pass("SetMode to CON mode");
    }

    print("\nTest 3: SetMode(1) can restore RAW mode after CON mode\n");
    ok = SetMode(fh, 1);
    if (!ok)
    {
        test_fail("SetMode restores RAW mode", "Call failed");
    }
    else
    {
        fill_bytes(buf, sizeof(buf), '?');
        print("Waiting for RAW input after CON mode: b\n");
        len = Read(fh, (APTR)buf, 1);
        if (len == 1 && buf[0] == 'b')
            test_pass("Raw mode restored after CON mode");
        else
            test_fail("Raw mode restored after CON mode", "Unexpected raw read after SetMode(0)");
    }

    print("\nTest 4: SetMode(2) is accepted\n");
    ok = SetMode(fh, 2);
    if (ok)
        test_pass("Medium mode accepted");
    else
        test_fail("Medium mode accepted", "Call failed");

    print("\nTest 5: Invalid mode is rejected\n");
    ok = SetMode(fh, 99);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_ACTION_NOT_KNOWN)
            test_pass("Invalid mode error");
        else
            test_fail("Invalid mode error", "Wrong IoErr");
    }
    else
    {
        test_fail("Invalid mode error", "Unexpected success");
    }

    print("\nTest 6: Regular files reject SetMode\n");
    {
        BPTR file_fh = Open((CONST_STRPTR)"T:setmode_regular_file.tmp", MODE_NEWFILE);
        if (!file_fh)
        {
            test_fail("Regular file SetMode rejection", "Could not open regular file");
        }
        else
        {
            ok = SetMode(file_fh, 1);
            if (!ok && IoErr() == ERROR_ACTION_NOT_KNOWN)
                test_pass("Regular file SetMode rejection");
            else
                test_fail("Regular file SetMode rejection", "Unexpected regular file result");

            Close(file_fh);
            DeleteFile((CONST_STRPTR)"T:setmode_regular_file.tmp");
        }
    }

    print("\nTest 7: Null filehandle is rejected\n");
    ok = SetMode(0, 0);
    if (!ok)
    {
        err = IoErr();
        if (err == ERROR_INVALID_LOCK)
            test_pass("Null filehandle error");
        else
            test_fail("Null filehandle error", "Wrong IoErr");
    }
    else
    {
        test_fail("Null filehandle error", "Unexpected success");
    }

    Close(fh);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
