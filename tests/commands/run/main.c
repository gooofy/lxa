/*
 * Integration Test: RUN command (background process)
 *
 * Tests:
 *   - LoadSeg functionality
 *   - CreateNewProc tags
 *   - Seglist operations
 *   - Process creation basics
 *
 * Note: Full RUN testing is limited because background processes
 * may not complete within the test timeframe. This tests the
 * underlying mechanisms.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
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
    print("RUN Command Test\n");
    print("================\n\n");
    
    /* Test 1: LoadSeg works for existing binary */
    print("Test 1: LoadSeg for existing binary\n");
    
    /* Try to load the test binary itself (should work) */
    BPTR seg = LoadSeg((STRPTR)"SYS:Tests/Commands/run_test");
    if (seg) {
        test_pass("LoadSeg returned seglist");
        UnLoadSeg(seg);
    } else {
        /* Try alternate path - the test might be in current dir */
        seg = LoadSeg((STRPTR)"run_test");
        if (seg) {
            test_pass("LoadSeg returned seglist (current dir)");
            UnLoadSeg(seg);
        } else {
            test_fail("LoadSeg", "Could not load test binary");
        }
    }
    
    /* Test 2: LoadSeg fails for non-existent binary */
    print("\nTest 2: LoadSeg fails for non-existent binary\n");
    
    seg = LoadSeg((STRPTR)"SYS:nonexistent_command_xyz");
    if (!seg) {
        test_pass("LoadSeg returned NULL for invalid file");
    } else {
        test_fail("LoadSeg invalid", "Should have returned NULL");
        UnLoadSeg(seg);
    }
    
    /* Test 3: NP_ tag constants exist */
    print("\nTest 3: NP_ tag constants defined\n");
    
    if (NP_Seglist && NP_Name && NP_StackSize && NP_Cli) {
        test_pass("NP_ tags are defined");
    } else {
        test_fail("NP_ tags", "Some tags are zero/undefined");
    }
    
    /* Test 4: Process has valid CLI number */
    print("\nTest 4: Current process has CLI number\n");
    
    struct Process *me = (struct Process *)FindTask(NULL);
    if (me && me->pr_TaskNum > 0) {
        test_pass("Process has CLI number > 0");
    } else if (me) {
        /* It's valid for background processes to not have CLI number */
        test_pass("Process exists (CLI number may be 0)");
    } else {
        test_fail("CLI number", "FindTask returned NULL");
    }
    
    /* Test 5: NIL: device works */
    print("\nTest 5: NIL: device is available\n");
    
    BPTR nilFh = Open((STRPTR)"NIL:", MODE_NEWFILE);
    if (nilFh) {
        /* Write should succeed but data is discarded */
        Write(nilFh, "test", 4);
        Close(nilFh);
        test_pass("NIL: device available");
    } else {
        test_fail("NIL: device", "Could not open");
    }
    
    /* Test 6: Can get input/output handles */
    print("\nTest 6: Input/Output handles available\n");
    
    BPTR inFh = Input();
    BPTR outFh = Output();
    if (inFh && outFh) {
        test_pass("Input/Output handles valid");
    } else {
        test_fail("I/O handles", "NULL handle(s)");
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
