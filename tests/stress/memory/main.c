/*
 * Memory Stress Test
 *
 * Tests memory allocation and deallocation under stress:
 *   1. Allocate/free thousands of blocks in sequence
 *   2. Allocate many blocks, then free in various patterns
 *   3. Check for memory fragmentation
 *   4. Verify MEMF_CLEAR works correctly
 *   5. Test various allocation sizes
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
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

/* Get free memory */
static ULONG get_free_mem(void)
{
    return AvailMem(MEMF_ANY);
}

#define NUM_BLOCKS 500
#define SMALL_SIZE 64
#define MEDIUM_SIZE 256
#define LARGE_SIZE 1024

int main(void)
{
    void *blocks[NUM_BLOCKS];
    ULONG mem_before, mem_after, mem_leaked;
    int i, j;
    int failed;
    
    print("Memory Stress Test\n");
    print("==================\n\n");
    
    /* Test 1: Sequential alloc/free - 1000 iterations */
    print("Test 1: Sequential alloc/free (1000 iterations)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    for (i = 0; i < 1000; i++) {
        void *mem = AllocMem(SMALL_SIZE, MEMF_PUBLIC);
        if (!mem) {
            failed++;
            continue;
        }
        FreeMem(mem, SMALL_SIZE);
    }
    
    mem_after = get_free_mem();
    
    print("  Completed 1000 alloc/free cycles\n");
    print("  Allocation failures: ");
    print_num(failed);
    print("\n");
    
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
    
    if (failed == 0 && mem_leaked <= 1024) {
        test_pass("Sequential alloc/free");
    } else if (failed > 0) {
        test_fail("Sequential alloc/free", "Some allocations failed");
    } else {
        test_fail("Sequential alloc/free", "Memory leak detected");
    }
    
    /* Test 2: Allocate 500 blocks, free in order */
    print("\nTest 2: Batch alloc then free in order (500 blocks)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    /* Allocate all blocks */
    for (i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = AllocMem(MEDIUM_SIZE, MEMF_PUBLIC);
        if (!blocks[i]) {
            failed++;
        }
    }
    
    print("  Allocated ");
    print_num(NUM_BLOCKS - failed);
    print(" of ");
    print_num(NUM_BLOCKS);
    print(" blocks\n");
    
    /* Free all blocks in forward order */
    for (i = 0; i < NUM_BLOCKS; i++) {
        if (blocks[i]) {
            FreeMem(blocks[i], MEDIUM_SIZE);
            blocks[i] = NULL;
        }
    }
    
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
    
    if (failed == 0 && mem_leaked <= 1024) {
        test_pass("Batch alloc/free forward");
    } else if (failed > 0) {
        test_fail("Batch alloc/free forward", "Some allocations failed");
    } else {
        test_fail("Batch alloc/free forward", "Memory leak detected");
    }
    
    /* Test 3: Allocate 500 blocks, free in reverse order */
    print("\nTest 3: Batch alloc then free in reverse (500 blocks)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    /* Allocate all blocks */
    for (i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = AllocMem(MEDIUM_SIZE, MEMF_PUBLIC);
        if (!blocks[i]) {
            failed++;
        }
    }
    
    print("  Allocated ");
    print_num(NUM_BLOCKS - failed);
    print(" of ");
    print_num(NUM_BLOCKS);
    print(" blocks\n");
    
    /* Free all blocks in reverse order (LIFO - best for memory allocators) */
    for (i = NUM_BLOCKS - 1; i >= 0; i--) {
        if (blocks[i]) {
            FreeMem(blocks[i], MEDIUM_SIZE);
            blocks[i] = NULL;
        }
    }
    
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
    
    if (failed == 0 && mem_leaked <= 1024) {
        test_pass("Batch alloc/free reverse");
    } else if (failed > 0) {
        test_fail("Batch alloc/free reverse", "Some allocations failed");
    } else {
        test_fail("Batch alloc/free reverse", "Memory leak detected");
    }
    
    /* Test 4: Allocate then free alternate blocks (fragmentation test) */
    print("\nTest 4: Fragmentation test (alternate free pattern)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    /* Allocate all blocks */
    for (i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = AllocMem(MEDIUM_SIZE, MEMF_PUBLIC);
        if (!blocks[i]) {
            failed++;
        }
    }
    
    /* Free every other block - creates fragmentation */
    for (i = 0; i < NUM_BLOCKS; i += 2) {
        if (blocks[i]) {
            FreeMem(blocks[i], MEDIUM_SIZE);
            blocks[i] = NULL;
        }
    }
    
    /* Try to allocate larger blocks in the gaps */
    int large_allocs = 0;
    void *large_blocks[100];
    for (i = 0; i < 100; i++) {
        large_blocks[i] = AllocMem(LARGE_SIZE, MEMF_PUBLIC);
        if (large_blocks[i]) {
            large_allocs++;
        }
    }
    
    print("  Freed alternate blocks, then allocated ");
    print_num(large_allocs);
    print(" large blocks\n");
    
    /* Free large blocks */
    for (i = 0; i < 100; i++) {
        if (large_blocks[i]) {
            FreeMem(large_blocks[i], LARGE_SIZE);
        }
    }
    
    /* Free remaining original blocks */
    for (i = 1; i < NUM_BLOCKS; i += 2) {
        if (blocks[i]) {
            FreeMem(blocks[i], MEDIUM_SIZE);
            blocks[i] = NULL;
        }
    }
    
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
    
    /* Accept some fragmentation-induced wastage */
    if (large_allocs > 50 && mem_leaked <= 2048) {
        test_pass("Fragmentation handling");
    } else if (large_allocs <= 50) {
        test_fail("Fragmentation handling", "Too few large allocations succeeded");
    } else {
        test_fail("Fragmentation handling", "Memory leak detected");
    }
    
    /* Test 5: MEMF_CLEAR verification */
    print("\nTest 5: MEMF_CLEAR verification\n");
    
    mem_before = get_free_mem();
    failed = 0;
    int clear_failed = 0;
    
    for (i = 0; i < 100; i++) {
        UBYTE *mem = (UBYTE *)AllocMem(MEDIUM_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
        if (!mem) {
            failed++;
            continue;
        }
        
        /* Verify memory is zeroed */
        for (j = 0; j < MEDIUM_SIZE; j++) {
            if (mem[j] != 0) {
                clear_failed++;
                break;
            }
        }
        
        /* Fill with pattern before freeing */
        for (j = 0; j < MEDIUM_SIZE; j++) {
            mem[j] = 0xAA;
        }
        
        FreeMem(mem, MEDIUM_SIZE);
    }
    
    mem_after = get_free_mem();
    
    print("  Tested 100 MEMF_CLEAR allocations\n");
    print("  Allocation failures: ");
    print_num(failed);
    print("\n  Clear verification failures: ");
    print_num(clear_failed);
    print("\n");
    
    if (failed == 0 && clear_failed == 0) {
        test_pass("MEMF_CLEAR verification");
    } else if (failed > 0) {
        test_fail("MEMF_CLEAR verification", "Some allocations failed");
    } else {
        test_fail("MEMF_CLEAR verification", "Memory not properly cleared");
    }
    
    /* Test 6: Mixed sizes allocation */
    print("\nTest 6: Mixed size allocations\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    /* Various sizes to test */
    ULONG sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    /* Allocate blocks of various sizes */
    for (i = 0; i < NUM_BLOCKS; i++) {
        ULONG size = sizes[i % num_sizes];
        blocks[i] = AllocMem(size, MEMF_PUBLIC);
        if (!blocks[i]) {
            failed++;
        }
    }
    
    print("  Allocated ");
    print_num(NUM_BLOCKS - failed);
    print(" blocks of mixed sizes\n");
    
    /* Free in random-ish order (every 3rd, then every 2nd, then rest) */
    for (i = 0; i < NUM_BLOCKS; i += 3) {
        if (blocks[i]) {
            ULONG size = sizes[i % num_sizes];
            FreeMem(blocks[i], size);
            blocks[i] = NULL;
        }
    }
    
    for (i = 1; i < NUM_BLOCKS; i += 2) {
        if (blocks[i]) {
            ULONG size = sizes[i % num_sizes];
            FreeMem(blocks[i], size);
            blocks[i] = NULL;
        }
    }
    
    for (i = 0; i < NUM_BLOCKS; i++) {
        if (blocks[i]) {
            ULONG size = sizes[i % num_sizes];
            FreeMem(blocks[i], size);
            blocks[i] = NULL;
        }
    }
    
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
    
    if (failed == 0 && mem_leaked <= 2048) {
        test_pass("Mixed size allocations");
    } else if (failed > 0) {
        test_fail("Mixed size allocations", "Some allocations failed");
    } else {
        test_fail("Mixed size allocations", "Memory leak detected");
    }
    
    /* Test 7: AllocVec/FreeVec stress */
    print("\nTest 7: AllocVec/FreeVec stress (500 iterations)\n");
    
    mem_before = get_free_mem();
    failed = 0;
    
    for (i = 0; i < 500; i++) {
        ULONG size = 64 + (i % 256);  /* Varying sizes */
        void *mem = AllocVec(size, MEMF_PUBLIC);
        if (!mem) {
            failed++;
            continue;
        }
        FreeVec(mem);
    }
    
    mem_after = get_free_mem();
    
    if (mem_before >= mem_after) {
        mem_leaked = mem_before - mem_after;
    } else {
        mem_leaked = 0;
    }
    
    print("  Completed 500 AllocVec/FreeVec cycles\n");
    print("  Allocation failures: ");
    print_num(failed);
    print("\n  Memory before: ");
    print_num(mem_before);
    print("\n  Memory after: ");
    print_num(mem_after);
    print("\n  Potential leak: ");
    print_num(mem_leaked);
    print(" bytes\n");
    
    /* Note: AllocVec may have some internal overhead per allocation */
    if (failed == 0 && mem_leaked <= 4096) {
        test_pass("AllocVec/FreeVec stress");
    } else if (failed > 0) {
        test_fail("AllocVec/FreeVec stress", "Some allocations failed");
    } else {
        test_fail("AllocVec/FreeVec stress", "Significant memory leak detected");
    }
    
    /* Test 8: Memory exhaustion recovery */
    print("\nTest 8: Large allocation handling\n");
    
    /* Try to allocate progressively larger blocks */
    ULONG test_sizes[] = {4096, 8192, 16384, 32768, 65536};
    int num_test_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);
    int alloc_success = 0;
    
    for (i = 0; i < num_test_sizes; i++) {
        void *mem = AllocMem(test_sizes[i], MEMF_PUBLIC);
        if (mem) {
            alloc_success++;
            FreeMem(mem, test_sizes[i]);
        }
    }
    
    print("  Successfully allocated/freed ");
    print_num(alloc_success);
    print(" of ");
    print_num(num_test_sizes);
    print(" large blocks\n");
    
    if (alloc_success >= 3) {
        test_pass("Large allocation handling");
    } else {
        test_fail("Large allocation handling", "Not enough large allocations succeeded");
    }
    
    /* Summary */
    print("\n=== Memory Stress Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n\n");
    
    if (tests_failed == 0) {
        print("PASS: All memory stress tests passed\n");
    }
    
    return tests_failed > 0 ? 10 : 0;
}
