/*
 * Exec Signal Tests
 *
 * Tests:
 *   - AllocSignal/FreeSignal
 *   - SetSignal for reading/clearing signals
 *   - Signal masks and multiple signals
 *   - SetExcept basics
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

static void print_hex(ULONG n)
{
    char buf[12];
    char *p = buf + 11;
    static const char hex[] = "0123456789ABCDEF";
    
    *p = '\0';
    
    do {
        *--p = hex[n & 0xF];
        n >>= 4;
    } while (n > 0);
    
    print("0x");
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
    struct Task *thisTask;
    BYTE sig1, sig2, sig3;
    ULONG oldSigs, sigs;
    ULONG savedAlloc;
    
    out = Output();
    
    print("Exec Signal Tests\n");
    print("=================\n\n");
    
    thisTask = FindTask(NULL);
    if (thisTask == NULL) {
        print("FATAL: FindTask(NULL) returned NULL\n");
        return 20;
    }
    
    /* Save original tc_SigAlloc */
    savedAlloc = thisTask->tc_SigAlloc;
    print("Initial tc_SigAlloc: ");
    print_hex(savedAlloc);
    print("\n\n");
    
    /* Test 1: AllocSignal with auto-select */
    print("Test 1: AllocSignal(-1) auto-select\n");
    sig1 = AllocSignal(-1);
    
    if (sig1 >= 0 && sig1 < 32) {
        print("    Allocated signal: ");
        print_num(sig1);
        print("\n");
        test_ok("AllocSignal(-1) returned valid signal");
        
        /* Verify bit is set in tc_SigAlloc */
        if (thisTask->tc_SigAlloc & (1 << sig1)) {
            test_ok("Signal bit set in tc_SigAlloc");
        } else {
            test_fail_msg("Signal bit not set in tc_SigAlloc");
        }
    } else {
        test_fail_msg("AllocSignal(-1) failed");
    }
    
    /* Test 2: AllocSignal - allocate more signals */
    print("\nTest 2: Multiple AllocSignal\n");
    sig2 = AllocSignal(-1);
    sig3 = AllocSignal(-1);
    
    if (sig2 >= 0 && sig3 >= 0) {
        test_ok("Allocated two more signals");
        
        if (sig1 != sig2 && sig2 != sig3 && sig1 != sig3) {
            test_ok("All signal numbers are unique");
        } else {
            test_fail_msg("Duplicate signal numbers allocated");
        }
    } else {
        test_fail_msg("Could not allocate additional signals");
    }
    
    /* Test 3: AllocSignal with specific number */
    print("\nTest 3: AllocSignal with specific number\n");
    {
        BYTE specSig;
        /* Find an unallocated signal */
        BYTE testNum = 20;
        
        /* Make sure it's not already allocated */
        if (!(thisTask->tc_SigAlloc & (1 << testNum))) {
            specSig = AllocSignal(testNum);
            
            if (specSig == testNum) {
                test_ok("AllocSignal(specific) returned requested number");
                FreeSignal(specSig);
            } else if (specSig == -1) {
                test_fail_msg("AllocSignal(specific) failed");
            } else {
                test_fail_msg("AllocSignal(specific) returned wrong number");
            }
        } else {
            print("  SKIP: Signal 20 already allocated\n");
        }
    }
    
    /* Test 4: AllocSignal - already allocated */
    print("\nTest 4: AllocSignal already-allocated signal\n");
    {
        BYTE dupSig = AllocSignal(sig1);  /* Try to allocate already-used signal */
        
        if (dupSig == -1) {
            test_ok("AllocSignal returns -1 for already-allocated signal");
        } else {
            test_fail_msg("AllocSignal should return -1 for duplicate");
            if (dupSig >= 0) FreeSignal(dupSig);
        }
    }
    
    /* Test 5: SetSignal - read signals */
    print("\nTest 5: SetSignal - read signals\n");
    oldSigs = SetSignal(0, 0);  /* Read without modifying */
    print("    Current tc_SigRecvd: ");
    print_hex(oldSigs);
    print("\n");
    test_ok("SetSignal(0,0) reads signals without modification");
    
    /* Test 6: SetSignal - set signals */
    print("\nTest 6: SetSignal - set signals\n");
    {
        ULONG mask = (1 << sig1);
        
        /* Clear the signal first */
        SetSignal(0, mask);
        
        /* Now set it */
        oldSigs = SetSignal(mask, mask);
        
        /* Verify it's set */
        sigs = SetSignal(0, 0);
        if (sigs & mask) {
            test_ok("SetSignal successfully set signal bit");
        } else {
            test_fail_msg("Signal bit not set after SetSignal");
        }
        
        /* Clear it again */
        SetSignal(0, mask);
        sigs = SetSignal(0, 0);
        if (!(sigs & mask)) {
            test_ok("SetSignal successfully cleared signal bit");
        } else {
            test_fail_msg("Signal bit not cleared after SetSignal");
        }
    }
    
    /* Test 7: SetSignal - selective modification */
    print("\nTest 7: SetSignal - selective modification\n");
    {
        ULONG mask1 = (1 << sig1);
        ULONG mask2 = (1 << sig2);
        
        /* Set both signals */
        SetSignal(mask1 | mask2, mask1 | mask2);
        
        /* Clear only sig1 */
        SetSignal(0, mask1);
        
        /* Verify sig2 is still set */
        sigs = SetSignal(0, 0);
        if ((sigs & mask2) && !(sigs & mask1)) {
            test_ok("SetSignal selectively clears only specified signals");
        } else {
            print("    sigs: ");
            print_hex(sigs);
            print("\n");
            test_fail_msg("Selective signal modification failed");
        }
        
        /* Clean up */
        SetSignal(0, mask2);
    }
    
    /* Test 8: FreeSignal */
    print("\nTest 8: FreeSignal\n");
    {
        FreeSignal(sig3);
        
        if (!(thisTask->tc_SigAlloc & (1 << sig3))) {
            test_ok("FreeSignal cleared bit in tc_SigAlloc");
        } else {
            test_fail_msg("FreeSignal did not clear bit");
        }
        
        /* Signal should be re-allocatable */
        BYTE reSig = AllocSignal(sig3);
        if (reSig == sig3) {
            test_ok("Freed signal can be re-allocated");
            FreeSignal(reSig);
        } else {
            test_fail_msg("Could not re-allocate freed signal");
            if (reSig >= 0) FreeSignal(reSig);
        }
    }
    
    /* Test 9: FreeSignal(-1) should not crash */
    print("\nTest 9: FreeSignal(-1) safety\n");
    FreeSignal(-1);
    test_ok("FreeSignal(-1) did not crash");
    
    /* Test 10: SetExcept - basic test */
    print("\nTest 10: SetExcept basics\n");
    {
        ULONG mask = (1 << sig1);
        ULONG oldExcept;
        
        /* Get current exception mask */
        oldExcept = thisTask->tc_SigExcept;
        print("    Initial tc_SigExcept: ");
        print_hex(oldExcept);
        print("\n");
        
        /* Enable exception for sig1 */
        oldExcept = SetExcept(mask, mask);
        
        if (thisTask->tc_SigExcept & mask) {
            test_ok("SetExcept enabled exception bit");
        } else {
            test_fail_msg("SetExcept did not enable exception");
        }
        
        /* Disable it */
        SetExcept(0, mask);
        
        if (!(thisTask->tc_SigExcept & mask)) {
            test_ok("SetExcept disabled exception bit");
        } else {
            test_fail_msg("SetExcept did not disable exception");
        }
    }
    
    /* Clean up allocated signals */
    FreeSignal(sig1);
    FreeSignal(sig2);
    /* sig3 already freed in test 8 */
    
    /* Summary */
    print("\n=============================\n");
    print("Tests passed: ");
    print_num(test_pass);
    print("\n");
    print("Tests failed: ");
    print_num(test_fail);
    print("\n");
    
    if (test_fail == 0) {
        print("\nPASS: All signal tests passed!\n");
        return 0;
    } else {
        print("\nFAIL: Some tests failed\n");
        return 20;
    }
}
