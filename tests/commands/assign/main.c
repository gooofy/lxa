/*
 * Integration Test: ASSIGN command
 *
 * Tests:
 *   - Verify system assigns exist (C:, S:)
 *   - Create new assign
 *   - AssignLock functionality
 *   - Remove assign
 */

#include <exec/types.h>
#include <exec/memory.h>
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

/* Check if a path can be locked */
static BOOL path_accessible(const char *path)
{
    BPTR lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

/* Clean up test assigns */
static void cleanup_test_assigns(void)
{
    /* Try to remove test assign - may not exist */
    RemAssignList((STRPTR)"TESTASSIGN", (BPTR)0);
    DeleteFile((CONST_STRPTR)"assigntestdir");
}

int main(void)
{
    BPTR lock;
    
    print("ASSIGN Command Test\n");
    print("===================\n\n");
    
    /* Cleanup any leftover test data */
    cleanup_test_assigns();
    
    /* Test 1: Verify SYS: exists */
    print("Test 1: Verify SYS: assign exists\n");
    
    if (path_accessible("SYS:")) {
        test_pass("SYS: accessible");
    } else {
        test_fail("SYS:", "Not accessible");
    }
    
    /* Test 2: Verify C: exists */
    print("\nTest 2: Verify C: assign exists\n");
    
    if (path_accessible("C:")) {
        test_pass("C: accessible");
    } else {
        test_fail("C:", "Not accessible");
    }
    
    /* Test 3: Create test directory for assign */
    print("\nTest 3: Create test directory for assign\n");
    
    lock = CreateDir((CONST_STRPTR)"assigntestdir");
    if (lock) {
        UnLock(lock);
        test_pass("Test directory created");
    } else {
        test_fail("Test directory", "CreateDir failed");
    }
    
    /* Test 4: Create an assign using AssignLock */
    print("\nTest 4: Create assign using AssignLock\n");
    
    lock = Lock((CONST_STRPTR)"assigntestdir", SHARED_LOCK);
    if (lock) {
        BOOL result = AssignLock((STRPTR)"TESTASSIGN", lock);
        if (result) {
            test_pass("AssignLock succeeded");
            /* Note: lock is consumed by AssignLock on success */
        } else {
            test_fail("AssignLock", "Failed");
            UnLock(lock);  /* Free lock on failure */
        }
    } else {
        test_fail("Lock for assign", "Failed");
    }
    
    /* Test 5: Verify the new assign works */
    print("\nTest 5: Verify new assign accessible\n");
    
    if (path_accessible("TESTASSIGN:")) {
        test_pass("TESTASSIGN: accessible");
    } else {
        test_fail("TESTASSIGN:", "Not accessible");
    }
    
    /* Test 6: Remove the assign */
    print("\nTest 6: Remove assign\n");
    
    if (RemAssignList((STRPTR)"TESTASSIGN", (BPTR)0)) {
        if (!path_accessible("TESTASSIGN:")) {
            test_pass("Assign removed");
        } else {
            test_fail("Remove assign", "Still accessible");
        }
    } else {
        test_fail("RemAssignList", "Failed");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test data\n");
    cleanup_test_assigns();
    print("  Cleanup complete\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
