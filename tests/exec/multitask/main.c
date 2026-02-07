/*
 * Test: exec/multitask
 *
 * Verify that preemptive multitasking actually switches between tasks.
 * This test:
 * 1. Creates a background task that increments a counter in a loop
 * 2. Main task waits a bit then checks if counter advanced
 * 3. If counter advanced, preemptive scheduling is working
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

/* Shared counter between tasks */
static volatile LONG g_counter = 0;
static volatile BOOL g_stop = FALSE;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(const char *prefix, LONG val, const char *suffix)
{
    char buf[64];
    char *p = buf;
    const char *src = prefix;
    while (*src) *p++ = *src++;

    if (val < 0) {
        *p++ = '-';
        val = -val;
    }

    char digits[12];
    int i = 0;
    if (val == 0) {
        digits[i++] = '0';
    } else {
        while (val > 0) {
            digits[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    while (i > 0) *p++ = digits[--i];
    
    src = suffix;
    while (*src) *p++ = *src++;
    *p = '\0';
    print(buf);
}

/*
 * Background task that increments counter in a tight loop.
 * If preemptive scheduling works, this should NOT block the main task.
 */
static void __attribute__((noinline)) counter_task(void)
{
    while (!g_stop) {
        g_counter++;
        /* No Delay() or Wait() - pure CPU loop */
        /* If preemption works, main task will still get CPU time */
    }
    
    /* Signal completion - use Wait() to block until we're removed */
    Wait(0);
}

int main(void)
{
    int errors = 0;
    struct Task *bgTask = NULL;
    APTR stack = NULL;
    
    print("=== Multitasking Test ===\n\n");
    
    /* ========== Test 1: Create background task ========== */
    print("--- Test 1: Create background counter task ---\n");
    
    /* Allocate stack for background task */
    #define STACK_SIZE 4096
    stack = AllocMem(STACK_SIZE, MEMF_CLEAR);
    if (!stack) {
        print("FAIL: Cannot allocate stack\n");
        return 20;
    }
    
    /* Create task structure */
    bgTask = (struct Task *)AllocMem(sizeof(struct Task), MEMF_CLEAR | MEMF_PUBLIC);
    if (!bgTask) {
        print("FAIL: Cannot allocate Task\n");
        FreeMem(stack, STACK_SIZE);
        return 20;
    }
    
    /* Initialize task */
    bgTask->tc_Node.ln_Type = NT_TASK;
    bgTask->tc_Node.ln_Pri = 0;  /* Same priority as main task */
    bgTask->tc_Node.ln_Name = (char *)"CounterTask";
    bgTask->tc_SPLower = stack;
    bgTask->tc_SPUpper = (APTR)((UBYTE *)stack + STACK_SIZE);
    bgTask->tc_SPReg = bgTask->tc_SPUpper;
    
    /* Add task */
    AddTask(bgTask, (APTR)counter_task, NULL);
    print("OK: Background task created\n");
    
    /* ========== Test 2: Wait and check counter ========== */
    print("\n--- Test 2: Wait for preemption ---\n");
    
    /* Record initial counter value - don't print since it's always 0 */
    LONG initial = g_counter;
    (void)initial;  /* Suppress unused warning */
    
    /*
     * Wait using Delay(). During this time:
     * - VBlank interrupts should fire at 50Hz
     * - Preemptive scheduler should give CPU time to background task
     * - Background task should increment counter many times
     */
    print("  Waiting 25 ticks (0.5 seconds)...\n");
    Delay(25);
    
    /* Check if counter advanced */
    LONG final = g_counter;
    
    /* Stop background task before reporting */
    g_stop = TRUE;
    Delay(5);  /* Give it time to see the stop flag */
    
    /* Don't print exact values since they vary between runs */
    if (final > 1000) {
        print("  Counter advanced significantly\n");
        print("OK: Background task ran during Delay() - preemption works!\n");
    } else {
        print_num("  Final counter: ", final, " (too low)\n");
        print("FAIL: Counter did not advance - preemption not working\n");
        errors++;
    }
    
    /* ========== Test 3: Verify task switching count ========== */
    print("\n--- Test 3: Verify dispatch count ---\n");
    ULONG dispCount = SysBase->DispCount;
    
    if (dispCount > 2) {
        print("OK: Multiple task switches occurred\n");
    } else {
        print_num("  Dispatch count: ", (LONG)dispCount, " (too low)\n");
        print("FAIL: Insufficient task switching\n");
        errors++;
    }
    
    /* ========== Cleanup ========== */
    print("\n--- Cleanup ---\n");
    
    /* Stop the background task first */
    g_stop = TRUE;
    Delay(5);  /* Give it time to see the stop flag */
    
    /* Remove the task to let the emulator stop */
    if (bgTask) {
        RemTask(bgTask);
        print("OK: Background task removed\n");
    }
    
    print("OK: Test complete\n");
    
    /* ========== Final result ========== */
    print("\n=== Test Results ===\n");
    if (errors == 0) {
        print("PASS: Multitasking test passed\n");
        return 0;
    } else {
        print_num("FAIL: ", errors, " errors occurred\n");
        return 20;
    }
}
