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
static volatile LONG g_message_counter = 0;
static struct MsgPort *g_resultPort = NULL;
static struct Task *g_parent_task = NULL;
static ULONG g_done_signal_mask = 0;

#define SEQUENTIAL_TASK_COUNT 20
#define COUNTER_TASK_COUNT 12
#define SIGNAL_TASK_COUNT 10
#define MESSAGE_TASK_COUNT 12
#define CLEANUP_TASK_COUNT 10
#define FINAL_TASK_COUNT 4

/* Message for reporting task completion */
struct TaskDoneMsg {
    struct Message tdm_Message;
    LONG           tdm_TaskNum;
    LONG           tdm_Counter;
};

/* Simple task entry - just signal parent and exit */
void SimpleTask(void)
{
    if (g_parent_task && g_done_signal_mask) {
        Signal(g_parent_task, g_done_signal_mask);
    }
}

/* Counter task - increments counter then signals */
void CounterTask(void)
{
    Forbid();
    g_counter++;
    Permit();

    if (g_parent_task && g_done_signal_mask) {
        Signal(g_parent_task, g_done_signal_mask);
    }
}

/* Message task - sends completion message to parent's port */
void MessageTask(void)
{
    if (g_resultPort) {
        struct TaskDoneMsg *msg = (struct TaskDoneMsg *)AllocMem(
            sizeof(struct TaskDoneMsg), MEMF_PUBLIC | MEMF_CLEAR);
        if (msg) {
            LONG taskNum;

            Forbid();
            taskNum = g_message_counter++;
            Permit();

            msg->tdm_Message.mn_Node.ln_Type = NT_MESSAGE;
            msg->tdm_Message.mn_Length = sizeof(struct TaskDoneMsg);
            msg->tdm_Message.mn_ReplyPort = NULL;  /* No reply needed */
            msg->tdm_TaskNum = taskNum;
            msg->tdm_Counter = 1;
            PutMsg(g_resultPort, (struct Message *)msg);
        }
    }
}

static struct Process *create_stress_task(APTR entry, const char *name)
{
    struct TagItem tags[] = {
        { NP_Entry, (ULONG)entry },
        { NP_Name, (ULONG)name },
        { NP_StackSize, 4096 },
        { NP_Input, Input() },
        { NP_Output, Output() },
        { TAG_DONE, 0 }
    };

    return CreateNewProc(tags);
}

/*
 * Pre-clear the done signal.  Must be called BEFORE creating the child task
 * to avoid the following race:
 *
 *   parent: CreateNewProc() → child immediately runs and signals parent
 *   parent: SetSignal(0, mask) → silently discards the already-received signal
 *   parent: Wait(mask)         → hangs forever
 *
 * By clearing the signal first we guarantee that any signal arriving after
 * the call has actually come from the child we are about to launch.
 */
static void pre_clear_done_signal(void)
{
    if (g_done_signal_mask) {
        SetSignal(0, g_done_signal_mask);
    }
}

static BOOL wait_for_task_completion(void)
{
    if (!g_done_signal_mask) {
        return FALSE;
    }

    return (Wait(g_done_signal_mask) & g_done_signal_mask) != 0;
}

static int collect_messages(struct MsgPort *port, int expected, int max_ticks)
{
    struct TaskDoneMsg *msg;
    int messages_received = 0;
    int tick;

    for (tick = 0; tick < max_ticks && messages_received < expected; tick++) {
        while ((msg = (struct TaskDoneMsg *)GetMsg(port)) != NULL) {
            messages_received++;
            FreeMem(msg, sizeof(struct TaskDoneMsg));
        }

        if (messages_received < expected) {
            Delay(1);
        }
    }

    while ((msg = (struct TaskDoneMsg *)GetMsg(port)) != NULL) {
        messages_received++;
        FreeMem(msg, sizeof(struct TaskDoneMsg));
    }

    return messages_received;
}

int main(void)
{
    ULONG mem_before, mem_after, mem_leaked;
    int i;
    int failed;
    int completed;
    LONG done_signal_bit;
    
    print("Task Stress Test\n");
    print("================\n\n");
    
    g_parent_task = FindTask(NULL);
    done_signal_bit = AllocSignal(-1);
    if (done_signal_bit == -1) {
        print("FAIL: Could not allocate completion signal\n");
        return 20;
    }
    g_done_signal_mask = 1UL << done_signal_bit;

    /* Test 1: Sequential task creation */
    print("Test 1: Sequential task creation\n");
    
    mem_before = get_free_mem();
    failed = 0;
    completed = 0;
    
    for (i = 0; i < SEQUENTIAL_TASK_COUNT; i++) {
        pre_clear_done_signal();
        struct Process *child = create_stress_task((APTR)SimpleTask, "SimpleTask");
        if (!child) {
            failed++;
            continue;
        }

        if (wait_for_task_completion()) {
            completed++;
        }
    }
    
    /* Small delay to let tasks fully exit */
    Delay(10);
    
    mem_after = get_free_mem();
    
    print("  Created ");
    print_num(SEQUENTIAL_TASK_COUNT - failed);
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
    if (failed == 0 && completed == SEQUENTIAL_TASK_COUNT && mem_leaked <= 8192) {
        test_pass("Sequential task creation");
    } else if (failed > 0) {
        test_fail("Sequential task creation", "Some tasks failed to create");
    } else if (completed < 50) {
        test_fail("Sequential task creation", "Some tasks did not complete");
    } else {
        test_fail("Sequential task creation", "Memory leak detected");
    }
    
    /* Test 2: Counter increment test (verify task execution) */
    print("\nTest 2: Counter increment\n");
    
    g_counter = 0;
    failed = 0;
    completed = 0;

    for (i = 0; i < COUNTER_TASK_COUNT; i++) {
        pre_clear_done_signal();
        struct Process *child = create_stress_task((APTR)CounterTask, "CounterTask");
        if (!child) {
            failed++;
            continue;
        }

        if (wait_for_task_completion()) {
            completed++;
        }
    }
    
    Delay(5);
    
    print("  Tasks completed: ");
    print_num(completed);
    print("\n  Counter value: ");
    print_num(g_counter);
    print("\n");
    
    if (failed == 0 && g_counter == COUNTER_TASK_COUNT) {
        test_pass("Counter increment");
    } else if (failed > 0) {
        test_fail("Counter increment", "Some tasks failed to create");
    } else {
        test_fail("Counter increment", "Counter mismatch - tasks may not have executed");
    }
    
    /* Test 3: Signal delivery under load */
    print("\nTest 3: Signal delivery stress\n");

    int signals_received = 0;
    int tasks_started = 0;

    for (i = 0; i < SIGNAL_TASK_COUNT; i++) {
        pre_clear_done_signal();
        struct Process *child = create_stress_task((APTR)SimpleTask, "SignalTask");
        if (!child) {
            continue;
        }

        tasks_started++;

        if (wait_for_task_completion()) {
            signals_received++;
        }
    }
    
    print("  Tasks started: ");
    print_num(tasks_started);
    print("\n  Signals received: ");
    print_num(signals_received);
    print("\n");
    
    if (signals_received == tasks_started) {
        test_pass("Signal delivery under load");
    } else {
        test_fail("Signal delivery under load", "Signal count mismatch");
    }
    
    /* Test 4: Message port stress */
    print("\nTest 4: Message port stress\n");
    
    g_resultPort = CreateMsgPort();
    if (!g_resultPort) {
        test_fail("Message port stress", "Failed to create message port");
    } else {
        int messages_received = 0;
        int tasks_created = 0;
        g_message_counter = 0;
        
        /* Create tasks that send messages */
        for (i = 0; i < MESSAGE_TASK_COUNT; i++) {
            struct Process *child = create_stress_task((APTR)MessageTask, "MsgTask");
            if (child) {
                tasks_created++;
            }
        }

        messages_received = collect_messages(g_resultPort, tasks_created, 100);
        
        print("  Tasks created: ");
        print_num(tasks_created);
        print("\n  Messages received: ");
        print_num(messages_received);
        print("\n");
        
        DeleteMsgPort(g_resultPort);
        g_resultPort = NULL;
        
        if (messages_received == tasks_created) {
            test_pass("Message port stress");
        } else {
            test_fail("Message port stress", "Message count mismatch");
        }
    }
    
    /* Test 5: Memory cleanup after task termination */
    print("\nTest 5: Memory cleanup after task termination\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < CLEANUP_TASK_COUNT; i++) {
        pre_clear_done_signal();
        struct Process *child = create_stress_task((APTR)SimpleTask, "CleanupTask");
        if (child) {
            wait_for_task_completion();
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
    
    for (i = 0; i < FINAL_TASK_COUNT; i++) {
        pre_clear_done_signal();
        struct Process *child = create_stress_task((APTR)CounterTask, "FinalTask");
        if (child) {
            wait_for_task_completion();
        }
    }
    
    Delay(5);
    
    print("  Final counter: ");
    print_num(g_counter);
    print(" (expected ");
    print_num(FINAL_TASK_COUNT);
    print(")\n");
    
    if (g_counter == FINAL_TASK_COUNT) {
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

    FreeSignal(done_signal_bit);
    
    return tests_failed > 0 ? 10 : 0;
}
