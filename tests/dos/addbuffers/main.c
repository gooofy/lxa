#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/*
 * AddBuffers() on devices without a buffer cache
 *
 * Only volumes that cannot be damaged are used: on AmigaOS 3.1 these
 * calls on SYS: (or an assign into it) really act on the boot volume.
 * Expected results come from the AmigaOS 3.1 reference.
 */

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

static void expect(const char *name, LONG ok, LONG want_ok, LONG want_err)
{
    LONG err = IoErr();

    print("  ");
    print(name);
    print(": result=");
    print_num(ok);
    print(" IoErr=");
    print_num(err);
    if (ok == want_ok && err == want_err)
    {
        print("  PASS\n");
    }
    else
    {
        print("  FAIL\n");
        tests_failed++;
    }
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR old_window_ptr = me->pr_WindowPtr;

    me->pr_WindowPtr = (APTR)-1;   /* no "insert volume" requesters */

    print("AddBuffers Test\n");
    print("===============\n\n");

    print("Test 1: Unknown devices are not mounted\n");
    SetIoErr(0);
    expect("AddBuffers NODEV: 1", AddBuffers((CONST_STRPTR)"NODEV:", 1), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    SetIoErr(0);
    expect("AddBuffers NODEV:x 5", AddBuffers((CONST_STRPTR)"NODEV:x", 5), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    print("\nTest 2: NIL: has no handler\n");
    SetIoErr(0);
    expect("AddBuffers NIL: 1", AddBuffers((CONST_STRPTR)"NIL:", 1), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    print("\nTest 3: The RAM: handler does not know ACTION_MORE_CACHE\n");
    SetIoErr(0);
    expect("AddBuffers RAM: 1", AddBuffers((CONST_STRPTR)"RAM:", 1), DOSFALSE, ERROR_ACTION_NOT_KNOWN);
    SetIoErr(0);
    expect("AddBuffers RAM: 0", AddBuffers((CONST_STRPTR)"RAM:", 0), DOSFALSE, ERROR_ACTION_NOT_KNOWN);
    SetIoErr(0);
    expect("AddBuffers RAM: -1", AddBuffers((CONST_STRPTR)"RAM:", -1), DOSFALSE, ERROR_ACTION_NOT_KNOWN);

    me->pr_WindowPtr = old_window_ptr;

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
