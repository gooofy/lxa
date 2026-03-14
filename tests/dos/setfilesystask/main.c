#include <exec/types.h>
#include <exec/ports.h>
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
    struct MsgPort *old_filesystem_task;
    struct MsgPort *port_a;
    struct MsgPort *port_b;
    struct MsgPort *previous;

    print("SetFileSysTask Test\n");
    print("=====================\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    port_a = CreateMsgPort();
    port_b = CreateMsgPort();
    if (!port_a || !port_b)
    {
        if (port_a)
            DeleteMsgPort(port_a);
        if (port_b)
            DeleteMsgPort(port_b);
        print("FAIL: Could not allocate filesystem test ports\n");
        return 20;
    }

    old_filesystem_task = me->pr_FileSystemTask;

    print("Test 1: Returns previous filesystem task and installs new port\n");
    previous = SetFileSysTask(port_a);
    if (previous == old_filesystem_task && me->pr_FileSystemTask == port_a)
        test_pass("Installs first filesystem task");
    else
        test_fail("Installs first filesystem task", "Unexpected old/new filesystem task state");

    print("\nTest 2: Replacing filesystem task returns prior port\n");
    previous = SetFileSysTask(port_b);
    if (previous == port_a && me->pr_FileSystemTask == port_b)
        test_pass("Replaces filesystem task");
    else
        test_fail("Replaces filesystem task", "Did not report the previous port");

    print("\nTest 3: Setting NULL clears filesystem task\n");
    previous = SetFileSysTask(NULL);
    if (previous == port_b && me->pr_FileSystemTask == NULL)
        test_pass("Clears filesystem task");
    else
        test_fail("Clears filesystem task", "Did not clear filesystem task correctly");

    print("\nTest 4: Restoring original filesystem task reports previous NULL\n");
    previous = SetFileSysTask(old_filesystem_task);
    if (previous == NULL && me->pr_FileSystemTask == old_filesystem_task)
        test_pass("Restores original filesystem task");
    else
        test_fail("Restores original filesystem task", "Did not restore original filesystem task");

    DeleteMsgPort(port_b);
    DeleteMsgPort(port_a);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
