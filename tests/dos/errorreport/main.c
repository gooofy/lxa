#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

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
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old_window_ptr;
    BOOL ok;
    LONG err;

    print("ErrorReport Test\n");
    print("================\n\n");

    print("Test 1: Unknown codes return immediately and preserve ErrorReport contract\n");
    SetIoErr(7);
    ok = ErrorReport(9999, REPORT_STREAM, 0, NULL);
    err = IoErr();
    if (ok == DOSTRUE && err == 9999)
        test_pass("Unknown code immediate return");
    else
        test_fail("Unknown code immediate return", "Wrong result or IoErr");

    print("\nTest 2: pr_WindowPtr == -1 disables requester display\n");
    old_window_ptr = me->pr_WindowPtr;
    me->pr_WindowPtr = (APTR)-1;
    ok = ErrorReport(ERROR_NO_DISK, REPORT_INSERT, (ULONG)"DH0:", NULL);
    err = IoErr();
    me->pr_WindowPtr = old_window_ptr;
    if (ok == DOSTRUE && err == ERROR_NO_DISK)
        test_pass("Disabled requester path");
    else
        test_fail("Disabled requester path", "Wrong result or IoErr");

    print("\nTest 3: Recognized error honors disabled-requester contract\n");
    me->pr_WindowPtr = (APTR)-1;
    ok = ErrorReport(ERROR_DEVICE_NOT_MOUNTED, REPORT_INSERT, (ULONG)"DH0:Tools", NULL);
    err = IoErr();
    if (ok == DOSTRUE && err == ERROR_DEVICE_NOT_MOUNTED)
        test_pass("Recognized disabled-requester path");
    else
        test_fail("Recognized disabled-requester path", "Wrong result or IoErr");

    print("\nTest 4: Invalid report payload still returns safely when requester is disabled\n");
    me->pr_WindowPtr = (APTR)-1;
    ok = ErrorReport(ERROR_NO_DISK, REPORT_LOCK, 0, NULL);
    err = IoErr();
    if (ok == DOSTRUE && err == ERROR_NO_DISK)
        test_pass("Invalid payload disabled-requester path");
    else
        test_fail("Invalid payload disabled-requester path", "Wrong result or IoErr");

    me->pr_WindowPtr = old_window_ptr;

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
