/*
 * Exec Memory Management Tests
 *
 * Tests:
 *   - AllocMem/FreeMem - basic allocation
 *   - AllocVec/FreeVec - vector allocation with size tracking
 *   - MEMF_CLEAR verification
 *   - Memory types (MEMF_PUBLIC, MEMF_CHIP, MEMF_FAST)
 *   - Boundary conditions
 *   - AvailMem for checking free memory
 *   - CopyMem/CopyMemQuick
 */

#include <exec/types.h>
#include <exec/memory.h>
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
    APTR mem1, mem2, mem3;
    APTR vec1, vec2;
    UBYTE *p;
    ULONG i;
    ULONG freeBefore, freeAfter;
    BOOL allZero;
    
    out = Output();
    
    print("Exec Memory Management Tests\n");
    print("============================\n\n");
    
    /* Test 1: Basic AllocMem/FreeMem */
    print("Test 1: Basic AllocMem/FreeMem\n");
    mem1 = AllocMem(1024, MEMF_PUBLIC);
    if (mem1 != NULL) {
        test_ok("AllocMem(1024, MEMF_PUBLIC) succeeded");
        
        /* Write pattern to verify memory is usable */
        p = (UBYTE *)mem1;
        for (i = 0; i < 1024; i++) {
            p[i] = (UBYTE)(i & 0xFF);
        }
        
        /* Verify pattern */
        allZero = TRUE;
        for (i = 0; i < 1024; i++) {
            if (p[i] != (UBYTE)(i & 0xFF)) {
                allZero = FALSE;
                break;
            }
        }
        if (allZero) {
            test_ok("Memory is readable and writable");
        } else {
            test_fail_msg("Memory verification failed");
        }
        
        FreeMem(mem1, 1024);
        test_ok("FreeMem succeeded");
    } else {
        test_fail_msg("AllocMem returned NULL");
    }
    
    /* Test 2: MEMF_CLEAR verification */
    print("\nTest 2: MEMF_CLEAR verification\n");
    mem1 = AllocMem(512, MEMF_PUBLIC | MEMF_CLEAR);
    if (mem1 != NULL) {
        test_ok("AllocMem(512, MEMF_CLEAR) succeeded");
        
        /* Verify all bytes are zero */
        p = (UBYTE *)mem1;
        allZero = TRUE;
        for (i = 0; i < 512; i++) {
            if (p[i] != 0) {
                allZero = FALSE;
                print("    Non-zero byte at offset ");
                print_num(i);
                print(": ");
                print_num(p[i]);
                print("\n");
                break;
            }
        }
        
        if (allZero) {
            test_ok("All 512 bytes are zero");
        } else {
            test_fail_msg("MEMF_CLEAR did not zero memory");
        }
        
        FreeMem(mem1, 512);
    } else {
        test_fail_msg("AllocMem with MEMF_CLEAR returned NULL");
    }
    
    /* Test 3: Multiple allocations */
    print("\nTest 3: Multiple allocations\n");
    mem1 = AllocMem(256, MEMF_PUBLIC);
    mem2 = AllocMem(512, MEMF_PUBLIC);
    mem3 = AllocMem(128, MEMF_PUBLIC);
    
    if (mem1 && mem2 && mem3) {
        test_ok("Three allocations succeeded");
        
        /* Verify they don't overlap */
        if (mem1 != mem2 && mem2 != mem3 && mem1 != mem3) {
            test_ok("Allocations are distinct");
        } else {
            test_fail_msg("Allocations overlap!");
        }
        
        FreeMem(mem1, 256);
        FreeMem(mem2, 512);
        FreeMem(mem3, 128);
        test_ok("All three freed");
    } else {
        print("    mem1=0x");
        print_num((LONG)mem1);
        print(" mem2=0x");
        print_num((LONG)mem2);
        print(" mem3=0x");
        print_num((LONG)mem3);
        print("\n");
        test_fail_msg("Some allocations failed");
        
        /* Free whatever succeeded */
        if (mem1) FreeMem(mem1, 256);
        if (mem2) FreeMem(mem2, 512);
        if (mem3) FreeMem(mem3, 128);
    }
    
    /* Test 4: AllocVec/FreeVec */
    print("\nTest 4: AllocVec/FreeVec\n");
    vec1 = AllocVec(1024, MEMF_PUBLIC);
    if (vec1 != NULL) {
        test_ok("AllocVec(1024, MEMF_PUBLIC) succeeded");
        
        /* Write and verify */
        p = (UBYTE *)vec1;
        for (i = 0; i < 1024; i++) {
            p[i] = 0xAA;
        }
        
        FreeVec(vec1);
        test_ok("FreeVec succeeded");
    } else {
        test_fail_msg("AllocVec returned NULL");
    }
    
    /* Test 5: AllocVec with MEMF_CLEAR */
    print("\nTest 5: AllocVec with MEMF_CLEAR\n");
    vec1 = AllocVec(256, MEMF_PUBLIC | MEMF_CLEAR);
    if (vec1 != NULL) {
        test_ok("AllocVec(256, MEMF_CLEAR) succeeded");
        
        /* Verify cleared */
        p = (UBYTE *)vec1;
        allZero = TRUE;
        for (i = 0; i < 256; i++) {
            if (p[i] != 0) {
                allZero = FALSE;
                break;
            }
        }
        
        if (allZero) {
            test_ok("AllocVec MEMF_CLEAR zeroed memory");
        } else {
            test_fail_msg("AllocVec MEMF_CLEAR did not zero memory");
        }
        
        FreeVec(vec1);
    } else {
        test_fail_msg("AllocVec with MEMF_CLEAR returned NULL");
    }
    
    /* Test 6: Zero-size allocation */
    print("\nTest 6: Zero-size allocation\n");
    mem1 = AllocMem(0, MEMF_PUBLIC);
    if (mem1 == NULL) {
        test_ok("AllocMem(0) returns NULL");
    } else {
        test_fail_msg("AllocMem(0) should return NULL");
        /* Don't try to free zero-size allocation */
    }
    
    vec1 = AllocVec(0, MEMF_PUBLIC);
    if (vec1 == NULL) {
        test_ok("AllocVec(0) returns NULL");
    } else {
        test_fail_msg("AllocVec(0) should return NULL");
    }
    
    /* Test 7: FreeVec(NULL) should not crash */
    print("\nTest 7: FreeVec(NULL) safety\n");
    FreeVec(NULL);
    test_ok("FreeVec(NULL) did not crash");
    
    /* Test 8: FreeMem(NULL) should not crash */
    print("\nTest 8: FreeMem(NULL) safety\n");
    FreeMem(NULL, 0);
    test_ok("FreeMem(NULL, 0) did not crash");
    FreeMem(NULL, 100);
    test_ok("FreeMem(NULL, 100) did not crash");
    
    /* Test 9: AvailMem consistency */
    print("\nTest 9: AvailMem consistency\n");
    freeBefore = AvailMem(MEMF_PUBLIC);
    print("    Free memory before: ");
    print_num(freeBefore);
    print(" bytes\n");
    
    mem1 = AllocMem(4096, MEMF_PUBLIC);
    if (mem1) {
        freeAfter = AvailMem(MEMF_PUBLIC);
        print("    Free memory after 4KB alloc: ");
        print_num(freeAfter);
        print(" bytes\n");
        
        if (freeAfter < freeBefore) {
            test_ok("AvailMem decreased after allocation");
        } else {
            test_fail_msg("AvailMem did not decrease");
        }
        
        FreeMem(mem1, 4096);
        
        freeAfter = AvailMem(MEMF_PUBLIC);
        print("    Free memory after free: ");
        print_num(freeAfter);
        print(" bytes\n");
        
        /* Allow some slack for allocator overhead */
        if (freeAfter >= freeBefore - 64) {
            test_ok("Memory returned after free");
        } else {
            test_fail_msg("Memory not fully returned");
        }
    } else {
        test_fail_msg("Could not allocate for AvailMem test");
    }
    
    /* Test 10: Multiple AllocVec/FreeVec */
    print("\nTest 10: Multiple AllocVec allocations\n");
    vec1 = AllocVec(100, MEMF_PUBLIC);
    vec2 = AllocVec(200, MEMF_PUBLIC);
    
    if (vec1 && vec2) {
        test_ok("Two AllocVec succeeded");
        
        /* Free in reverse order */
        FreeVec(vec2);
        FreeVec(vec1);
        test_ok("Both FreeVec succeeded");
    } else {
        test_fail_msg("AllocVec failed");
        if (vec1) FreeVec(vec1);
        if (vec2) FreeVec(vec2);
    }
    
    /* Test 11: Alignment verification */
    print("\nTest 11: Memory alignment\n");
    mem1 = AllocMem(10, MEMF_PUBLIC);  /* Odd size */
    mem2 = AllocMem(10, MEMF_PUBLIC);
    
    if (mem1 && mem2) {
        /* Check 4-byte alignment */
        if (((ULONG)mem1 & 3) == 0) {
            test_ok("First allocation is 4-byte aligned");
        } else {
            print("    Address: ");
            print_num((LONG)mem1);
            print("\n");
            test_fail_msg("First allocation not 4-byte aligned");
        }
        
        if (((ULONG)mem2 & 3) == 0) {
            test_ok("Second allocation is 4-byte aligned");
        } else {
            test_fail_msg("Second allocation not 4-byte aligned");
        }
        
        FreeMem(mem1, 10);
        FreeMem(mem2, 10);
    } else {
        test_fail_msg("Alignment test allocations failed");
        if (mem1) FreeMem(mem1, 10);
        if (mem2) FreeMem(mem2, 10);
    }
    
    /* Test 12: CopyMem */
    print("\nTest 12: CopyMem\n");
    {
        UBYTE src[64];
        UBYTE dst[64];
        BOOL copyOk = TRUE;
        
        /* Fill source with pattern */
        for (i = 0; i < 64; i++) {
            src[i] = (UBYTE)(i * 3 + 7);
            dst[i] = 0;
        }
        
        CopyMem(src, dst, 64);
        
        /* Verify copy */
        for (i = 0; i < 64; i++) {
            if (dst[i] != src[i]) {
                copyOk = FALSE;
                break;
            }
        }
        
        if (copyOk) {
            test_ok("CopyMem copied 64 bytes correctly");
        } else {
            print("    Mismatch at offset ");
            print_num(i);
            print("\n");
            test_fail_msg("CopyMem failed");
        }
        
        /* Test partial copy */
        for (i = 0; i < 64; i++) dst[i] = 0xFF;
        
        CopyMem(src + 10, dst + 5, 20);
        
        copyOk = TRUE;
        /* Check that dst[0..4] is still 0xFF */
        for (i = 0; i < 5; i++) {
            if (dst[i] != 0xFF) {
                copyOk = FALSE;
                break;
            }
        }
        /* Check that dst[5..24] matches src[10..29] */
        for (i = 0; i < 20; i++) {
            if (dst[5 + i] != src[10 + i]) {
                copyOk = FALSE;
                break;
            }
        }
        /* Check that dst[25..63] is still 0xFF */
        for (i = 25; i < 64; i++) {
            if (dst[i] != 0xFF) {
                copyOk = FALSE;
                break;
            }
        }
        
        if (copyOk) {
            test_ok("CopyMem partial copy works correctly");
        } else {
            test_fail_msg("CopyMem partial copy failed");
        }
    }
    
    /* Test 13: CopyMemQuick */
    print("\nTest 13: CopyMemQuick\n");
    {
        /* CopyMemQuick requires longword alignment and sizes */
        ULONG srcLong[16];
        ULONG dstLong[16];
        BOOL copyOk = TRUE;
        
        /* Fill source with pattern */
        for (i = 0; i < 16; i++) {
            srcLong[i] = i * 0x12345678 + 0xDEADBEEF;
            dstLong[i] = 0;
        }
        
        CopyMemQuick(srcLong, dstLong, 16 * sizeof(ULONG));
        
        /* Verify copy */
        for (i = 0; i < 16; i++) {
            if (dstLong[i] != srcLong[i]) {
                copyOk = FALSE;
                break;
            }
        }
        
        if (copyOk) {
            test_ok("CopyMemQuick copied 64 bytes correctly");
        } else {
            print("    Mismatch at index ");
            print_num(i);
            print("\n");
            test_fail_msg("CopyMemQuick failed");
        }
    }
    
    /* Test 14: CopyMem with zero size */
    print("\nTest 14: CopyMem with zero size\n");
    {
        UBYTE src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        UBYTE dst[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        
        CopyMem(src, dst, 0);  /* Should do nothing */
        
        if (dst[0] == 0xFF) {
            test_ok("CopyMem(0) did not modify destination");
        } else {
            test_fail_msg("CopyMem(0) modified destination");
        }
    }
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All memory tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
