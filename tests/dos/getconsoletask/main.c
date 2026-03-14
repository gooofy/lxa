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

#define TEST_PORT_NAME "GetConsoleTask.TestPort"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

struct ConsoleTaskMessage
{
    struct Message msg;
    struct MsgPort *console_task;
};

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

static void ConsoleTaskProbeTask(void)
{
    struct MsgPort *port;
    struct ConsoleTaskMessage *msg;

    Forbid();
    port = FindPort((STRPTR)TEST_PORT_NAME);
    Permit();

    if (port)
    {
        msg = (struct ConsoleTaskMessage *)AllocMem(sizeof(*msg), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->msg.mn_Node.ln_Type = NT_MESSAGE;
            msg->msg.mn_Length = sizeof(*msg);
            msg->console_task = GetConsoleTask();
            PutMsg(port, (struct Message *)msg);
        }
    }

    Forbid();
    RemTask(NULL);
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *old_console_task;
    struct MsgPort *probe_port;
    struct MsgPort *test_console_port;
    struct Task *task;
    struct ConsoleTaskMessage *msg;

    print("GetConsoleTask Test\n");
    print("===================\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    old_console_task = me->pr_ConsoleTask;

    test_console_port = CreateMsgPort();
    if (!test_console_port)
    {
        print("FAIL: Could not allocate console test port\n");
        return 20;
    }

    print("Test 1: Current process returns pr_ConsoleTask\n");
    me->pr_ConsoleTask = test_console_port;
    if (GetConsoleTask() == test_console_port)
        test_pass("Current process console task");
    else
        test_fail("Current process console task", "Returned wrong port");

    print("\nTest 2: NULL pr_ConsoleTask returns NULL\n");
    me->pr_ConsoleTask = NULL;
    if (GetConsoleTask() == NULL)
        test_pass("NULL console task");
    else
        test_fail("NULL console task", "Expected NULL");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        test_fail("Task caller returns NULL", "Could not allocate probe port");
        me->pr_ConsoleTask = old_console_task;
        DeleteMsgPort(test_console_port);
        print("\nFailed: ");
        print_num(tests_failed);
        print("\n");
        return tests_failed ? 20 : 0;
    }

    probe_port->mp_Node.ln_Name = (char *)TEST_PORT_NAME;
    AddPort(probe_port);

    print("\nTest 3: Task callers return NULL\n");
    me->pr_ConsoleTask = test_console_port;
    task = CreateTask((CONST_STRPTR)"GetConsoleTask.Task", 0, ConsoleTaskProbeTask, 4096);
    if (!task)
    {
        test_fail("Task callers return NULL", "CreateTask failed");
    }
    else
    {
        WaitPort(probe_port);
        msg = (struct ConsoleTaskMessage *)GetMsg(probe_port);
        if (msg && msg->console_task == NULL)
            test_pass("Task callers return NULL");
        else
            test_fail("Task callers return NULL", "Task did not observe NULL");

        if (msg)
            FreeMem(msg, sizeof(*msg));
    }

    RemPort(probe_port);
    DeleteMsgPort(probe_port);
    me->pr_ConsoleTask = old_console_task;
    DeleteMsgPort(test_console_port);

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
