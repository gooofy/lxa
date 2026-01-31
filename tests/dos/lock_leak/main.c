/*
 * Integration Test: Lock leak detection
 *
 * Tests:
 *   - Allocate many locks and verify proper cleanup
 *   - Check that unlocked locks don't cause leaks
 *   - Stress test with repeated lock/unlock cycles
 *   - Detect common lock leak patterns
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

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

/* Get approximate free memory (for leak detection) */
static ULONG get_free_mem(void)
{
    return AvailMem(MEMF_ANY);
}

int main(void)
{
    BPTR locks[32];
    ULONG mem_before, mem_after, mem_leaked;
    int i;
    
    print("Lock Leak Detection Test\n");
    print("========================\n\n");
    
    /* Test 1: Basic lock/unlock cycle - no leak */
    print("Test 1: Basic lock/unlock cycle\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < 10; i++) {
        BPTR lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            UnLock(lock);
        }
    }
    
    mem_after = get_free_mem();
    
    /* Allow small variance (system allocations may happen) */
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
    
    /* Accept up to 256 bytes variance from system activity */
    if (mem_leaked <= 256) {
        test_pass("No significant leak in basic cycle");
    } else {
        test_fail("Basic cycle", "Memory decreased significantly");
    }
    
    /* Test 2: Multiple simultaneous locks - all released */
    print("\nTest 2: Multiple simultaneous locks\n");
    
    mem_before = get_free_mem();
    
    /* Acquire 20 locks */
    for (i = 0; i < 20; i++) {
        locks[i] = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
    }
    
    /* Release all locks */
    for (i = 0; i < 20; i++) {
        if (locks[i]) {
            UnLock(locks[i]);
            locks[i] = 0;
        }
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Acquired 20 locks, released all\n");
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    if (mem_leaked <= 512) {
        test_pass("No significant leak with multiple locks");
    } else {
        test_fail("Multiple locks", "Memory decreased significantly");
    }
    
    /* Test 3: Stress test - 100 lock/unlock cycles */
    print("\nTest 3: Stress test (100 cycles)\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < 100; i++) {
        BPTR lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            UnLock(lock);
        }
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Completed 100 lock/unlock cycles\n");
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* 100 cycles should not leak more than 1KB */
    if (mem_leaked <= 1024) {
        test_pass("No significant leak in stress test");
    } else {
        test_fail("Stress test", "Memory decreased significantly");
    }
    
    /* Test 4: DupLock and UnLock */
    print("\nTest 4: DupLock and UnLock\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < 20; i++) {
        BPTR lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            BPTR dup = DupLock(lock);
            if (dup) {
                UnLock(dup);
            }
            UnLock(lock);
        }
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  20 DupLock/UnLock cycles\n");
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    if (mem_leaked <= 512) {
        test_pass("No significant leak with DupLock");
    } else {
        test_fail("DupLock test", "Memory decreased significantly");
    }
    
    /* Test 5: ParentDir lock chain */
    print("\nTest 5: ParentDir lock chain\n");
    
    mem_before = get_free_mem();
    
    for (i = 0; i < 10; i++) {
        BPTR lock = Lock((CONST_STRPTR)"SYS:", SHARED_LOCK);
        if (lock) {
            BPTR parent = ParentDir(lock);
            if (parent) {
                UnLock(parent);
            }
            UnLock(lock);
        }
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  10 ParentDir cycles\n");
    print("  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    if (mem_leaked <= 512) {
        test_pass("No significant leak with ParentDir");
    } else {
        test_fail("ParentDir test", "Memory decreased significantly");
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
