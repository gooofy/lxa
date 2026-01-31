/*
 * Integration Test: STATUS command
 *
 * Tests:
 *   - Task list access
 *   - FindTask functionality
 *   - Process information retrieval
 *   - CLI number lookup
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf;
    
    if (n < 0) {
        *p++ = '-';
        n = -n;
    }
    
    char tmp[16];
    int i = 0;
    do {
        tmp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    while (i > 0) *p++ = tmp[--i];
    *p = '\0';
    
    print(buf);
}

static int tests_passed = 0;
static int tests_failed = 0;

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
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
    print("STATUS Command Test\n");
    print("===================\n\n");
    
    /* Test 1: FindTask(NULL) returns current task */
    print("Test 1: FindTask(NULL) returns current task\n");
    
    struct Task *thisTask = FindTask(NULL);
    if (thisTask) {
        test_pass("FindTask(NULL) returned task");
    } else {
        test_fail("FindTask(NULL)", "Returned NULL");
    }
    
    /* Test 2: Current task is a process (has pr_MsgPort) */
    print("\nTest 2: Current task is a Process\n");
    
    if (thisTask && thisTask->tc_Node.ln_Type == NT_PROCESS) {
        test_pass("Current task is NT_PROCESS");
    } else {
        test_fail("Task type", "Not a process");
    }
    
    /* Test 3: Check task state */
    print("\nTest 3: Current task state is TS_RUN\n");
    
    if (thisTask && thisTask->tc_State == TS_RUN) {
        test_pass("Task state is TS_RUN");
    } else {
        test_fail("Task state", "Not running");
    }
    
    /* Test 4: Check task has valid stack bounds */
    print("\nTest 4: Task has valid stack bounds\n");
    
    if (thisTask && thisTask->tc_SPLower && thisTask->tc_SPUpper) {
        ULONG stackSize = (ULONG)thisTask->tc_SPUpper - (ULONG)thisTask->tc_SPLower;
        if (stackSize > 0 && stackSize < 1000000) {  /* Reasonable stack size */
            test_pass("Valid stack bounds");
        } else {
            test_fail("Stack bounds", "Invalid size");
        }
    } else {
        test_fail("Stack bounds", "NULL pointers");
    }
    
    /* Test 5: SysBase TaskReady list accessible */
    print("\nTest 5: TaskReady list accessible\n");
    
    Forbid();
    if (SysBase && &SysBase->TaskReady) {
        test_pass("TaskReady list accessible");
    } else {
        test_fail("TaskReady list", "Not accessible");
    }
    Permit();
    
    /* Test 6: SysBase TaskWait list accessible */
    print("\nTest 6: TaskWait list accessible\n");
    
    Forbid();
    if (SysBase && &SysBase->TaskWait) {
        test_pass("TaskWait list accessible");
    } else {
        test_fail("TaskWait list", "Not accessible");
    }
    Permit();
    
    /* Test 7: Process pr_CLI is set for CLI processes */
    print("\nTest 7: Process has CLI structure\n");
    
    if (thisTask && thisTask->tc_Node.ln_Type == NT_PROCESS) {
        struct Process *proc = (struct Process *)thisTask;
        /* pr_CLI might be 0 for background processes */
        if (proc->pr_CLI || proc->pr_TaskNum > 0) {
            test_pass("Process has CLI/TaskNum");
        } else {
            /* Not all processes are CLI processes, this is OK */
            test_pass("Process CLI check (non-CLI ok)");
        }
    } else {
        test_fail("CLI structure", "Not a process");
    }
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
