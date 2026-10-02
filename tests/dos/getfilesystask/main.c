#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/alib_protos.h>
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
    struct MsgPort *test_filesystem_port;

    print("GetFileSysTask Test\n");
    print("===================\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    old_filesystem_task = (struct MsgPort *)me->pr_FileSystemTask;

    test_filesystem_port = CreateMsgPort();
    if (!test_filesystem_port)
    {
        print("FAIL: Could not allocate filesystem test port\n");
        return 20;
    }

    print("Test 1: Current process returns pr_FileSystemTask\n");
    me->pr_FileSystemTask = test_filesystem_port;
    if (GetFileSysTask() == test_filesystem_port)
        test_pass("Current process filesystem task");
    else
        test_fail("Current process filesystem task", "Returned wrong port");

    print("\nTest 2: NULL pr_FileSystemTask returns NULL\n");
    me->pr_FileSystemTask = NULL;
    if (GetFileSysTask() == NULL)
        test_pass("NULL filesystem task");
    else
        test_fail("NULL filesystem task", "Expected NULL");

    me->pr_FileSystemTask = old_filesystem_task;
    DeleteMsgPort(test_filesystem_port);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
