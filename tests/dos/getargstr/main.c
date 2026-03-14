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

#define TEST_PORT_NAME "GetArgStr.TestPort"

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

struct ArgStrMessage
{
    struct Message msg;
    STRPTR argstr;
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

static void ArgStrProbeTask(void)
{
    struct MsgPort *port;
    struct ArgStrMessage *msg;

    Forbid();
    port = FindPort((STRPTR)TEST_PORT_NAME);
    Permit();

    if (port)
    {
        msg = (struct ArgStrMessage *)AllocMem(sizeof(*msg), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg)
        {
            msg->msg.mn_Node.ln_Type = NT_MESSAGE;
            msg->msg.mn_Length = sizeof(*msg);
            msg->argstr = GetArgStr();
            PutMsg(port, (struct Message *)msg);
        }
    }

    Forbid();
    RemTask(NULL);
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    STRPTR old_argstr;
    struct MsgPort *probe_port;
    struct Task *task;
    struct ArgStrMessage *msg;
    static char arg_a[] = "alpha beta";
    static char arg_b[] = "quoted \"value\"";

    print("GetArgStr Test\n");
    print("==============\n\n");

    if (!me || me->pr_Task.tc_Node.ln_Type != NT_PROCESS)
    {
        print("FAIL: Current task is not a process\n");
        return 20;
    }

    old_argstr = me->pr_Arguments;

    print("Test 1: Current process returns pr_Arguments pointer\n");
    me->pr_Arguments = arg_a;
    if (GetArgStr() == (STRPTR)arg_a)
        test_pass("Current process argument string");
    else
        test_fail("Current process argument string", "Returned wrong pointer");

    print("\nTest 2: NULL pr_Arguments returns NULL\n");
    me->pr_Arguments = NULL;
    if (GetArgStr() == NULL)
        test_pass("NULL argument string");
    else
        test_fail("NULL argument string", "Expected NULL");

    print("\nTest 3: Updated pr_Arguments value is observed immediately\n");
    me->pr_Arguments = arg_b;
    if (GetArgStr() == (STRPTR)arg_b)
        test_pass("Updated argument string");
    else
        test_fail("Updated argument string", "Did not return updated pointer");

    probe_port = CreateMsgPort();
    if (!probe_port)
    {
        test_fail("Task callers return NULL", "Could not allocate probe port");
        me->pr_Arguments = old_argstr;
        print("\nFailed: ");
        print_num(tests_failed);
        print("\n");
        return tests_failed ? 20 : 0;
    }

    probe_port->mp_Node.ln_Name = (char *)TEST_PORT_NAME;
    AddPort(probe_port);

    print("\nTest 4: Task callers return NULL\n");
    me->pr_Arguments = arg_a;
    task = CreateTask((CONST_STRPTR)"GetArgStr.Task", 0, ArgStrProbeTask, 4096);
    if (!task)
    {
        test_fail("Task callers return NULL", "CreateTask failed");
    }
    else
    {
        WaitPort(probe_port);
        msg = (struct ArgStrMessage *)GetMsg(probe_port);
        if (msg && msg->argstr == NULL)
            test_pass("Task callers return NULL");
        else
            test_fail("Task callers return NULL", "Task did not observe NULL");

        if (msg)
            FreeMem(msg, sizeof(*msg));
    }

    RemPort(probe_port);
    DeleteMsgPort(probe_port);
    me->pr_Arguments = old_argstr;

    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 20 : 0;
}
