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

int main(void)
{
    LONG ok;
    LONG err;

    print("Inhibit Test\n");
    print("============\n\n");

    print("Test 1: Rejects NULL device names\n");
    ok = Inhibit(NULL, DOSTRUE);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_REQUIRED_ARG_MISSING)
        test_pass("Reject NULL device");
    else
        test_fail("Reject NULL device", "NULL device handling behaved incorrectly");

    print("\nTest 2: Rejects device names without a trailing ':' device syntax\n");
    ok = Inhibit((CONST_STRPTR)"SYS", DOSTRUE);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_INVALID_COMPONENT_NAME)
        test_pass("Reject missing colon");
    else
        test_fail("Reject missing colon", "Device syntax validation behaved incorrectly");

    print("\nTest 3: Rejects assigns because Inhibit requires a real device\n");
    ok = Inhibit((CONST_STRPTR)"C:", DOSTRUE);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_DEVICE_NOT_MOUNTED)
        test_pass("Reject assign");
    else
        test_fail("Reject assign", "Assign inputs should fail as non-device targets");

    print("\nTest 4: Cleanly reports unsupported hosted inhibit packets instead of hitting the stub\n");
    ok = Inhibit((CONST_STRPTR)"HOME:", DOSTRUE);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_ACTION_NOT_KNOWN)
        test_pass("Report unsupported inhibit action");
    else
        test_fail("Report unsupported inhibit action", "Expected ACTION_INHIBIT to fail cleanly in the current hosted stack");

    print("\nTest 5: Uses the same public path for uninhibit requests\n");
    ok = Inhibit((CONST_STRPTR)"HOME:", DOSFALSE);
    err = IoErr();
    if (ok == DOSFALSE && err == ERROR_ACTION_NOT_KNOWN)
        test_pass("Report unsupported uninhibit action");
    else
        test_fail("Report unsupported uninhibit action", "Expected ACTION_INHIBIT clear requests to fail cleanly in the current hosted stack");

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
