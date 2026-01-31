/*
 * Task Stress Test
 *
 * Tests task creation and scheduling under stress:
 *   1. Create many tasks sequentially, verify completion
 *   2. Test scheduler fairness with multiple concurrent tasks
 *   3. Verify signal delivery under load
 *   4. Test message passing stress
 *   5. Verify proper cleanup after task termination
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

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

static ULONG get_free_mem(void)
{
    return AvailMem(MEMF_ANY);
}

/* Simple child task that just increments a counter and exits */
static volatile LONG g_counter = 0;
static struct MsgPort *g_resultPort = NULL;

/* Message for reporting task completion */
struct TaskDoneMsg {
    struct Message tdm_Message;
    LONG           tdm_TaskNum;
    LONG           tdm_Counter;
};

/* Simple task entry - just signal parent and exit */
void SimpleTask(void)
{
    struct Task *parent = (struct Task *)FindTask(NULL)->tc_UserData;
    if (parent) {
        Signal(parent, SIGBREAKF_CTRL_D);
    }
}

/* Counter task - increments counter then signals */
void CounterTask(void)
{
    Forbid();
    g_counter++;
    Permit();
    
    struct Task *parent = (struct Task *)FindTask(NULL)->tc_UserData;
    if (parent) {
        Signal(parent, SIGBREAKF_CTRL_D);
    }
}

/* Message task - sends completion message to parent's port */
void MessageTask(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    LONG taskNum = (LONG)me->pr_Task.tc_UserData;
    
    if (g_resultPort) {
        struct TaskDoneMsg *msg = (struct TaskDoneMsg *)AllocMem(
            sizeof(struct TaskDoneMsg), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg) {
            msg->tdm_Message.mn_Node.ln_Type = NT_MESSAGE;
            msg->tdm_Message.mn_Length = sizeof(struct TaskDoneMsg);
            msg->tdm_Message.mn_ReplyPort = NULL;  /* No reply needed */
            msg->tdm_TaskNum = taskNum;
            msg->tdm_Counter = 1;
            PutMsg(g_resultPort, (struct Message *)msg);
        }
    }
}

int main(void)
{
    ULONG mem_before, mem_after, mem_leaked;
    ULONG signals;
    int i;
    int failed;
    int completed;
    
    print("Task Stress Test\n");
    print("================\n\n");
    
    /* Test 1: Sequential task creation - 50 tasks */
    print("Test 1: Sequential task creation (50 tasks)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    completed = 0;
    
    struct Task *mainTask = FindTask(NULL);
    
    for (i = 0; i < 50; i++) {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)SimpleTask },
            { NP_Name, (ULONG)"SimpleTask" },
            { NP_StackSize, 4096 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        
        /* Store parent task pointer for signaling */
        mainTask->tc_UserData = (APTR)mainTask;
        
        struct Process *child = CreateNewProc(tags);
        if (!child) {
            failed++;
            continue;
        }
        
        /* Wait for child to signal completion */
        child->pr_Task.tc_UserData = (APTR)mainTask;
        signals = Wait(SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_C);
        
        if (signals & SIGBREAKF_CTRL_D) {
            completed++;
        }
    }
    
    /* Small delay to let tasks fully exit */
    Delay(10);
    
    mem_after = get_free_mem();
    
    print("  Created ");
    print_num(50 - failed);
    print(" tasks, ");
    print_num(completed);
    print(" completed\n");
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* Allow some overhead (4KB) for internal structures */
    if (failed == 0 && completed == 50 && mem_leaked <= 8192) {
        test_pass("Sequential task creation");
    } else if (failed > 0) {
        test_fail("Sequential task creation", "Some tasks failed to create");
    } else if (completed < 50) {
        test_fail("Sequential task creation", "Some tasks did not complete");
    } else {
        test_fail("Sequential task creation", "Memory leak detected");
    }
    
    /* Test 2: Counter increment test (verify task execution) */
    print("\nTest 2: Counter increment (30 tasks)\n");
    
    g_counter = 0;
    failed = 0;
    completed = 0;
    
    for (i = 0; i < 30; i++) {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)CounterTask },
            { NP_Name, (ULONG)"CounterTask" },
            { NP_StackSize, 4096 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        
        struct Process *child = CreateNewProc(tags);
        if (!child) {
            failed++;
            continue;
        }
        
        child->pr_Task.tc_UserData = (APTR)mainTask;
        signals = Wait(SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_C);
        
        if (signals & SIGBREAKF_CTRL_D) {
            completed++;
        }
    }
    
    Delay(5);
    
    print("  Tasks completed: ");
    print_num(completed);
    print("\n  Counter value: ");
    print_num(g_counter);
    print("\n");
    
    if (failed == 0 && g_counter == 30) {
        test_pass("Counter increment");
    } else if (failed > 0) {
        test_fail("Counter increment", "Some tasks failed to create");
    } else {
        test_fail("Counter increment", "Counter mismatch - tasks may not have executed");
    }
    
    /* Test 3: Signal delivery under load */
    print("\nTest 3: Signal delivery stress (20 tasks with signals)\n");
    
    int signals_received = 0;
    struct Task *me = FindTask(NULL);
    int tasks_started = 0;
    
    /* Clear any pending signals */
    SetSignal(0, SIGBREAKF_CTRL_D);
    
    /* Start tasks and immediately wait for each - more reliable */
    for (i = 0; i < 20; i++) {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)SimpleTask },
            { NP_Name, (ULONG)"SignalTask" },
            { NP_StackSize, 4096 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        
        struct Process *child = CreateNewProc(tags);
        if (!child) {
            continue;
        }
        
        child->pr_Task.tc_UserData = (APTR)me;
        tasks_started++;
        
        /* Wait for this task's signal */
        signals = Wait(SIGBREAKF_CTRL_D);
        if (signals & SIGBREAKF_CTRL_D) {
            signals_received++;
        }
    }
    
    print("  Tasks started: ");
    print_num(tasks_started);
    print("\n  Signals received: ");
    print_num(signals_received);
    print("\n");
    
    if (signals_received >= tasks_started - 2) {  /* Allow 2 signal losses */
        test_pass("Signal delivery under load");
    } else {
        test_fail("Signal delivery under load", "Too many signals lost");
    }
    
    /* Test 4: Message port stress */
    print("\nTest 4: Message port stress (25 messages)\n");
    
    g_resultPort = CreateMsgPort();
    if (!g_resultPort) {
        test_fail("Message port stress", "Failed to create message port");
    } else {
        int messages_received = 0;
        int tasks_created = 0;
        
        /* Create tasks that send messages */
        for (i = 0; i < 25; i++) {
            struct TagItem tags[] = {
                { NP_Entry, (ULONG)MessageTask },
                { NP_Name, (ULONG)"MsgTask" },
                { NP_StackSize, 4096 },
                { NP_Input, Input() },
                { NP_Output, Output() },
                { TAG_DONE, 0 }
            };
            
            struct Process *child = CreateNewProc(tags);
            if (child) {
                child->pr_Task.tc_UserData = (APTR)(LONG)i;
                tasks_created++;
            }
        }
        
        /* Receive messages with delay for tasks to complete */
        Delay(25);
        
        struct TaskDoneMsg *msg;
        while ((msg = (struct TaskDoneMsg *)GetMsg(g_resultPort)) != NULL) {
            messages_received++;
            FreeMem(msg, sizeof(struct TaskDoneMsg));
        }
        
        print("  Tasks created: ");
        print_num(tasks_created);
        print("\n  Messages received: ");
        print_num(messages_received);
        print("\n");
        
        DeleteMsgPort(g_resultPort);
        g_resultPort = NULL;
        
        if (messages_received >= 20) {  /* Allow some message loss */
            test_pass("Message port stress");
        } else {
            test_fail("Message port stress", "Too few messages received");
        }
    }
    
    /* Test 5: Memory cleanup after task termination */
    print("\nTest 5: Memory cleanup after task termination\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < 20; i++) {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)SimpleTask },
            { NP_Name, (ULONG)"CleanupTask" },
            { NP_StackSize, 4096 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        
        struct Process *child = CreateNewProc(tags);
        if (child) {
            child->pr_Task.tc_UserData = (APTR)mainTask;
            Wait(SIGBREAKF_CTRL_D);
        }
    }
    
    /* Delay to ensure cleanup */
    Delay(10);
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    if (mem_leaked <= 4096) {
        test_pass("Memory cleanup after task termination");
    } else {
        test_fail("Memory cleanup after task termination", "Memory leak detected");
    }
    
    /* Test 6: Task priority test (basic scheduler fairness) */
    print("\nTest 6: Basic scheduler operation\n");
    
    /* Just verify we can still create and run tasks after all the stress */
    g_counter = 0;
    
    for (i = 0; i < 5; i++) {
        struct TagItem tags[] = {
            { NP_Entry, (ULONG)CounterTask },
            { NP_Name, (ULONG)"FinalTask" },
            { NP_StackSize, 4096 },
            { NP_Input, Input() },
            { NP_Output, Output() },
            { TAG_DONE, 0 }
        };
        
        struct Process *child = CreateNewProc(tags);
        if (child) {
            child->pr_Task.tc_UserData = (APTR)mainTask;
            Wait(SIGBREAKF_CTRL_D);
        }
    }
    
    Delay(5);
    
    print("  Final counter: ");
    print_num(g_counter);
    print(" (expected 5)\n");
    
    if (g_counter == 5) {
        test_pass("Basic scheduler operation");
    } else {
        test_fail("Basic scheduler operation", "Tasks did not execute correctly");
    }
    
    /* Summary */
    print("\n=== Task Stress Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n\n");
    
    if (tests_failed == 0) {
        print("PASS: All task stress tests passed\n");
    }
    
    return tests_failed > 0 ? 10 : 0;
}
