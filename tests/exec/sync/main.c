/*
 * Exec Semaphore Tests
 *
 * Tests:
 *   - InitSemaphore
 *   - ObtainSemaphore/ReleaseSemaphore
 *   - AttemptSemaphore
 *   - Nested semaphore acquisition
 *   - Basic semaphore semantics
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

/* Helper to print a string */
static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Simple number to string for test output */
static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;
    
    *p = '\0';
    
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    if (neg) *--p = '-';
    
    print(p);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

int main(void)
{
    struct SignalSemaphore *sem;
    struct Task *thisTask;
    ULONG result;
    
    out = Output();
    
    print("Exec Semaphore Tests\n");
    print("====================\n\n");
    
    thisTask = FindTask(NULL);
    
    /* Allocate a semaphore */
    sem = (struct SignalSemaphore *)AllocMem(sizeof(struct SignalSemaphore), MEMF_PUBLIC | MEMF_CLEAR);
    if (sem == NULL) {
        print("FATAL: Could not allocate semaphore\n");
        return 20;
    }
    
    /* Test 1: InitSemaphore */
    print("Test 1: InitSemaphore\n");
    InitSemaphore(sem);
    
    /* Verify initialization */
    if (sem->ss_Link.ln_Type == NT_SIGNALSEM) {
        test_ok("Semaphore type set to NT_SIGNALSEM");
    } else {
        print("    Type is: ");
        print_num(sem->ss_Link.ln_Type);
        print(" (expected ");
        print_num(NT_SIGNALSEM);
        print(")\n");
        test_fail_msg("Semaphore type not set correctly");
    }
    
    if (sem->ss_NestCount == 0) {
        test_ok("NestCount initialized to 0");
    } else {
        test_fail_msg("NestCount not initialized to 0");
    }
    
    if (sem->ss_Owner == NULL) {
        test_ok("Owner initialized to NULL");
    } else {
        test_fail_msg("Owner not initialized to NULL");
    }
    
    if (sem->ss_QueueCount == -1) {
        test_ok("QueueCount initialized to -1");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print(" (expected -1)\n");
        test_fail_msg("QueueCount not initialized to -1");
    }
    
    /* Test 2: ObtainSemaphore */
    print("\nTest 2: ObtainSemaphore\n");
    ObtainSemaphore(sem);
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner set to current task after Obtain");
    } else {
        test_fail_msg("Owner not set correctly");
    }
    
    if (sem->ss_NestCount == 1) {
        test_ok("NestCount is 1 after first Obtain");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("NestCount not incremented correctly");
    }
    
    if (sem->ss_QueueCount == 0) {
        test_ok("QueueCount is 0 after first Obtain");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print("\n");
        test_fail_msg("QueueCount not updated correctly");
    }
    
    /* Test 3: Nested ObtainSemaphore (same task) */
    print("\nTest 3: Nested ObtainSemaphore\n");
    ObtainSemaphore(sem);
    
    if (sem->ss_NestCount == 2) {
        test_ok("NestCount is 2 after nested Obtain");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("Nested Obtain did not increment NestCount");
    }
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner still current task after nested Obtain");
    } else {
        test_fail_msg("Owner changed during nested Obtain");
    }
    
    /* Test 4: ReleaseSemaphore (nested) */
    print("\nTest 4: ReleaseSemaphore (nested)\n");
    ReleaseSemaphore(sem);
    
    if (sem->ss_NestCount == 1) {
        test_ok("NestCount decremented to 1");
    } else {
        print("    NestCount is: ");
        print_num(sem->ss_NestCount);
        print("\n");
        test_fail_msg("NestCount not decremented correctly");
    }
    
    if (sem->ss_Owner == thisTask) {
        test_ok("Owner still set (not fully released)");
    } else {
        test_fail_msg("Owner cleared too early");
    }
    
    /* Test 5: Final ReleaseSemaphore */
    print("\nTest 5: Final ReleaseSemaphore\n");
    ReleaseSemaphore(sem);
    
    if (sem->ss_NestCount == 0) {
        test_ok("NestCount decremented to 0");
    } else {
        test_fail_msg("NestCount not 0 after full release");
    }
    
    if (sem->ss_Owner == NULL) {
        test_ok("Owner cleared after full release");
    } else {
        test_fail_msg("Owner not cleared after full release");
    }
    
    if (sem->ss_QueueCount == -1) {
        test_ok("QueueCount back to -1");
    } else {
        print("    QueueCount is: ");
        print_num(sem->ss_QueueCount);
        print("\n");
        test_fail_msg("QueueCount not reset");
    }
    
    /* Test 6: AttemptSemaphore on free semaphore */
    print("\nTest 6: AttemptSemaphore (free semaphore)\n");
    result = AttemptSemaphore(sem);
    
    if (result != 0) {
        test_ok("AttemptSemaphore returned TRUE for free semaphore");
        
        if (sem->ss_Owner == thisTask) {
            test_ok("Semaphore now owned by current task");
        } else {
            test_fail_msg("Semaphore not owned after successful Attempt");
        }
        
        ReleaseSemaphore(sem);
        test_ok("Released semaphore after Attempt");
    } else {
        test_fail_msg("AttemptSemaphore returned FALSE for free semaphore");
    }
    
    /* Test 7: AttemptSemaphore on same-task-owned semaphore */
    print("\nTest 7: AttemptSemaphore (self-owned)\n");
    ObtainSemaphore(sem);
    
    result = AttemptSemaphore(sem);
    if (result != 0) {
        test_ok("AttemptSemaphore succeeds when same task owns it");
        
        if (sem->ss_NestCount == 2) {
            test_ok("NestCount incremented to 2");
        } else {
            test_fail_msg("NestCount not 2");
        }
        
        ReleaseSemaphore(sem);
    } else {
        test_fail_msg("AttemptSemaphore failed on self-owned semaphore");
    }
    
    ReleaseSemaphore(sem);
    
    /* Test 8: Multiple obtain/release cycles */
    print("\nTest 8: Multiple obtain/release cycles\n");
    {
        int i;
        BOOL success = TRUE;
        
        for (i = 0; i < 10; i++) {
            ObtainSemaphore(sem);
            if (sem->ss_Owner != thisTask || sem->ss_NestCount != 1) {
                success = FALSE;
                break;
            }
            ReleaseSemaphore(sem);
            if (sem->ss_Owner != NULL || sem->ss_NestCount != 0) {
                success = FALSE;
                break;
            }
        }
        
        if (success) {
            test_ok("10 obtain/release cycles completed correctly");
        } else {
            test_fail_msg("Obtain/release cycle failed");
        }
    }
    
    /* Test 9: InitSemaphore(NULL) should not crash */
    print("\nTest 9: InitSemaphore(NULL) safety\n");
    InitSemaphore(NULL);
    test_ok("InitSemaphore(NULL) did not crash");
    
    /* Test 10: ObtainSemaphore(NULL) should not crash */
    print("\nTest 10: ObtainSemaphore(NULL) safety\n");
    ObtainSemaphore(NULL);
    test_ok("ObtainSemaphore(NULL) did not crash");
    
    /* Test 11: ReleaseSemaphore(NULL) should not crash */
    print("\nTest 11: ReleaseSemaphore(NULL) safety\n");
    ReleaseSemaphore(NULL);
    test_ok("ReleaseSemaphore(NULL) did not crash");
    
    /* Test 12: AttemptSemaphore(NULL) should return FALSE */
    print("\nTest 12: AttemptSemaphore(NULL)\n");
    result = AttemptSemaphore(NULL);
    if (result == 0) {
        test_ok("AttemptSemaphore(NULL) returns FALSE");
    } else {
        test_fail_msg("AttemptSemaphore(NULL) should return FALSE");
    }
    
    /* Cleanup */
    FreeMem(sem, sizeof(struct SignalSemaphore));
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All semaphore tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
