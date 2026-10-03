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
 * Inhibit() on devices that cannot be damaged
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

    print("Inhibit Test\n");
    print("============\n\n");

    print("Test 1: Unknown devices are not mounted\n");
    SetIoErr(0);
    expect("Inhibit NODEV: on", Inhibit((CONST_STRPTR)"NODEV:", DOSTRUE), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    SetIoErr(0);
    expect("Inhibit NODEV:x on", Inhibit((CONST_STRPTR)"NODEV:x", DOSTRUE), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    print("\nTest 2: NIL: has no handler\n");
    SetIoErr(0);
    expect("Inhibit NIL: on", Inhibit((CONST_STRPTR)"NIL:", DOSTRUE), DOSFALSE, ERROR_DEVICE_NOT_MOUNTED);
    print("\nTest 3: The RAM: handler does not know ACTION_INHIBIT\n");
    SetIoErr(0);
    expect("Inhibit RAM: on", Inhibit((CONST_STRPTR)"RAM:", DOSTRUE), DOSFALSE, ERROR_ACTION_NOT_KNOWN);
    SetIoErr(0);
    expect("Inhibit RAM: off", Inhibit((CONST_STRPTR)"RAM:", DOSFALSE), DOSFALSE, ERROR_ACTION_NOT_KNOWN);
    SetIoErr(0);
    expect("Inhibit RAM:x/y on", Inhibit((CONST_STRPTR)"RAM:x/y", DOSTRUE), DOSFALSE, ERROR_ACTION_NOT_KNOWN);

    me->pr_WindowPtr = old_window_ptr;

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
