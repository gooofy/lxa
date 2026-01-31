/*
 * Integration Test: BREAK command
 *
 * Tests:
 *   - Signal sending to self
 *   - Signal mask building
 *   - SIGBREAK flag constants
 *   - SetSignal/CheckSignal functionality
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

int main(void)
{
    print("BREAK Command Test\n");
    print("==================\n\n");
    
    /* Test 1: SIGBREAKF constants are correct */
    print("Test 1: SIGBREAKF constants are correct\n");
    
    /* SIGBREAKF_CTRL_C should be bit 12 = 0x1000 */
    if (SIGBREAKF_CTRL_C == 0x1000 &&
        SIGBREAKF_CTRL_D == 0x2000 &&
        SIGBREAKF_CTRL_E == 0x4000 &&
        SIGBREAKF_CTRL_F == 0x8000) {
        test_pass("SIGBREAKF constants correct");
    } else {
        test_fail("SIGBREAKF constants", "Wrong values");
    }
    
    /* Test 2: Signal to self works */
    print("\nTest 2: Signal to self works\n");
    
    struct Task *self = FindTask(NULL);
    if (!self) {
        test_fail("Signal to self", "FindTask failed");
    } else {
        /* Clear any pending signals first */
        SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
        
        /* Signal ourselves with CTRL_D */
        Signal(self, SIGBREAKF_CTRL_D);
        
        /* Check if signal is set */
        ULONG sig = SetSignal(0, 0);  /* Read without clearing */
        if (sig & SIGBREAKF_CTRL_D) {
            test_pass("Signal to self works");
        } else {
            test_fail("Signal to self", "Signal not received");
        }
        
        /* Clear the signal */
        SetSignal(0, SIGBREAKF_CTRL_D);
    }
    
    /* Test 3: Signal mask building (multiple signals) */
    print("\nTest 3: Signal mask building\n");
    
    ULONG mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_E;
    if (mask == 0x5000) {
        test_pass("Signal mask building correct");
    } else {
        test_fail("Signal mask building", "Wrong combined value");
    }
    
    /* Test 4: ALL signals mask */
    print("\nTest 4: ALL signals mask\n");
    
    ULONG allMask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | 
                    SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
    if (allMask == 0xF000) {
        test_pass("ALL signals mask = 0xF000");
    } else {
        test_fail("ALL signals mask", "Wrong value");
    }
    
    /* Test 5: SetSignal can clear signals */
    print("\nTest 5: SetSignal can clear signals\n");
    
    if (self) {
        /* Set a signal */
        Signal(self, SIGBREAKF_CTRL_E);
        
        /* Verify it's set */
        ULONG before = SetSignal(0, 0);
        
        /* Clear it */
        SetSignal(0, SIGBREAKF_CTRL_E);
        
        /* Verify it's cleared */
        ULONG after = SetSignal(0, 0);
        
        if ((before & SIGBREAKF_CTRL_E) && !(after & SIGBREAKF_CTRL_E)) {
            test_pass("SetSignal clears signals");
        } else {
            test_fail("SetSignal clear", "Signal not cleared properly");
        }
    } else {
        test_fail("SetSignal clear", "No self task");
    }
    
    /* Test 6: CheckSignal equivalent (SetSignal with same mask) */
    print("\nTest 6: CheckSignal (SetSignal returns old value)\n");
    
    if (self) {
        /* Clear and set CTRL_F */
        SetSignal(0, SIGBREAKF_CTRL_F);
        Signal(self, SIGBREAKF_CTRL_F);
        
        /* SetSignal returns OLD value, so checking should return the signal */
        ULONG old = SetSignal(0, SIGBREAKF_CTRL_F);
        
        if (old & SIGBREAKF_CTRL_F) {
            test_pass("SetSignal returns old signal state");
        } else {
            test_fail("SetSignal return", "Old signal not returned");
        }
    } else {
        test_fail("SetSignal return", "No self task");
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
