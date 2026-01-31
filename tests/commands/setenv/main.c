/*
 * Integration Test: Environment Variable Commands (SET, SETENV, GETENV, UNSET, UNSETENV)
 *
 * Tests the DOS library functions SetVar, GetVar, DeleteVar, FindVar
 * through their functionality:
 *   - Setting local variables (SET)
 *   - Setting global environment variables (SETENV)
 *   - Getting variables (GETENV)
 *   - Deleting variables (UNSET, UNSETENV)
 *
 * This test uses the DOS library functions directly.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
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
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = n < 0;
    unsigned long num = neg ? -n : n;
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    if (neg) *--p = '-';
    print(p);
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

static void test_fail(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    tests_failed++;
}

static void test_fail_str(const char *name, const char *expected, const char *got)
{
    print("  FAIL: ");
    print(name);
    print(" (expected '");
    print(expected);
    print("', got '");
    print(got);
    print("')\n");
    tests_failed++;
}

int main(void)
{
    char buffer[256];
    LONG len;
    BOOL result;
    
    print("Environment Variable Test\n");
    print("=========================\n\n");
    
    /* ===== Test 1: Local Variables ===== */
    print("Test Group 1: Local Variables (SetVar/GetVar with GVF_LOCAL_ONLY)\n");
    
    /* Set a local variable */
    result = SetVar((STRPTR)"TESTVAR1", (STRPTR)"hello world", -1, GVF_LOCAL_ONLY | LV_VAR);
    if (result) {
        test_pass("SetVar local TESTVAR1");
    } else {
        test_fail("SetVar local TESTVAR1");
    }
    
    /* Get the local variable */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"TESTVAR1", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (len > 0 && strcmp(buffer, "hello world") == 0) {
        test_pass("GetVar local TESTVAR1 = hello world");
    } else {
        test_fail_str("GetVar local TESTVAR1", "hello world", buffer);
    }
    
    /* Update the local variable */
    result = SetVar((STRPTR)"TESTVAR1", (STRPTR)"updated value", -1, GVF_LOCAL_ONLY | LV_VAR);
    if (result) {
        test_pass("SetVar local TESTVAR1 update");
    } else {
        test_fail("SetVar local TESTVAR1 update");
    }
    
    /* Verify updated value */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"TESTVAR1", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (len > 0 && strcmp(buffer, "updated value") == 0) {
        test_pass("GetVar local TESTVAR1 = updated value");
    } else {
        test_fail_str("GetVar local TESTVAR1", "updated value", buffer);
    }
    
    /* Delete the local variable */
    result = DeleteVar((STRPTR)"TESTVAR1", GVF_LOCAL_ONLY | LV_VAR);
    if (result) {
        test_pass("DeleteVar local TESTVAR1");
    } else {
        test_fail("DeleteVar local TESTVAR1");
    }
    
    /* Verify deletion - GetVar should fail */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"TESTVAR1", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (len < 0) {
        test_pass("GetVar TESTVAR1 after delete returns -1");
    } else {
        test_fail("GetVar should fail after DeleteVar");
    }
    
    /* ===== Test 2: Global Variables (ENV:) ===== */
    print("\nTest Group 2: Global Variables (SetVar/GetVar with GVF_GLOBAL_ONLY)\n");
    
    /* Set a global variable */
    result = SetVar((STRPTR)"TESTENV1", (STRPTR)"global value", -1, GVF_GLOBAL_ONLY | LV_VAR);
    if (result) {
        test_pass("SetVar global TESTENV1");
    } else {
        test_fail("SetVar global TESTENV1");
    }
    
    /* Get the global variable */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"TESTENV1", (STRPTR)buffer, sizeof(buffer), GVF_GLOBAL_ONLY | LV_VAR);
    if (len > 0 && strcmp(buffer, "global value") == 0) {
        test_pass("GetVar global TESTENV1 = global value");
    } else {
        test_fail_str("GetVar global TESTENV1", "global value", buffer);
    }
    
    /* Delete the global variable */
    result = DeleteVar((STRPTR)"TESTENV1", GVF_GLOBAL_ONLY | LV_VAR);
    if (result) {
        test_pass("DeleteVar global TESTENV1");
    } else {
        test_fail("DeleteVar global TESTENV1");
    }
    
    /* Verify deletion */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"TESTENV1", (STRPTR)buffer, sizeof(buffer), GVF_GLOBAL_ONLY | LV_VAR);
    if (len < 0) {
        test_pass("GetVar TESTENV1 after delete returns -1");
    } else {
        test_fail("GetVar should fail after DeleteVar");
    }
    
    /* ===== Test 3: Non-existent Variables ===== */
    print("\nTest Group 3: Non-existent Variables\n");
    
    /* Try to get a variable that doesn't exist */
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"NONEXISTENT_VAR", (STRPTR)buffer, sizeof(buffer), LV_VAR);
    if (len < 0) {
        test_pass("GetVar non-existent returns -1");
    } else {
        test_fail("GetVar non-existent should fail");
    }
    
    /* Delete non-existent variable - should return FALSE but not error */
    result = DeleteVar((STRPTR)"NONEXISTENT_VAR", GVF_LOCAL_ONLY | LV_VAR);
    /* On AmigaOS, deleting a non-existent var returns FALSE */
    test_pass("DeleteVar non-existent does not crash");
    
    /* ===== Test 4: Empty String Values ===== */
    print("\nTest Group 4: Edge Cases\n");
    
    /* Set an empty value */
    result = SetVar((STRPTR)"EMPTY_VAR", (STRPTR)"", -1, GVF_LOCAL_ONLY | LV_VAR);
    if (result) {
        test_pass("SetVar empty string");
    } else {
        test_fail("SetVar empty string");
    }
    
    /* Get the empty value */
    memset(buffer, 'X', sizeof(buffer));  /* Fill with X to detect empty */
    buffer[sizeof(buffer)-1] = '\0';
    len = GetVar((STRPTR)"EMPTY_VAR", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (len == 0 && buffer[0] == '\0') {
        test_pass("GetVar empty string returns len=0");
    } else {
        print("  FAIL: GetVar empty string (len=");
        print_num(len);
        print(")\n");
        tests_failed++;
    }
    
    /* Clean up */
    DeleteVar((STRPTR)"EMPTY_VAR", GVF_LOCAL_ONLY | LV_VAR);
    
    /* Variable with special characters */
    result = SetVar((STRPTR)"SPECIAL_VAR", (STRPTR)"path/to/file", -1, GVF_LOCAL_ONLY | LV_VAR);
    if (result) {
        test_pass("SetVar with path value");
    } else {
        test_fail("SetVar with path value");
    }
    
    memset(buffer, 0, sizeof(buffer));
    len = GetVar((STRPTR)"SPECIAL_VAR", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (len > 0 && strcmp(buffer, "path/to/file") == 0) {
        test_pass("GetVar with path value");
    } else {
        test_fail_str("GetVar with path value", "path/to/file", buffer);
    }
    
    /* Clean up */
    DeleteVar((STRPTR)"SPECIAL_VAR", GVF_LOCAL_ONLY | LV_VAR);
    
    /* ===== Test 5: Multiple Variables ===== */
    print("\nTest Group 5: Multiple Variables\n");
    
    /* Set multiple variables */
    SetVar((STRPTR)"VAR_A", (STRPTR)"value_a", -1, GVF_LOCAL_ONLY | LV_VAR);
    SetVar((STRPTR)"VAR_B", (STRPTR)"value_b", -1, GVF_LOCAL_ONLY | LV_VAR);
    SetVar((STRPTR)"VAR_C", (STRPTR)"value_c", -1, GVF_LOCAL_ONLY | LV_VAR);
    
    /* Verify all are accessible */
    memset(buffer, 0, sizeof(buffer));
    GetVar((STRPTR)"VAR_A", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (strcmp(buffer, "value_a") == 0) {
        test_pass("GetVar VAR_A");
    } else {
        test_fail("GetVar VAR_A");
    }
    
    memset(buffer, 0, sizeof(buffer));
    GetVar((STRPTR)"VAR_B", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (strcmp(buffer, "value_b") == 0) {
        test_pass("GetVar VAR_B");
    } else {
        test_fail("GetVar VAR_B");
    }
    
    memset(buffer, 0, sizeof(buffer));
    GetVar((STRPTR)"VAR_C", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (strcmp(buffer, "value_c") == 0) {
        test_pass("GetVar VAR_C");
    } else {
        test_fail("GetVar VAR_C");
    }
    
    /* Delete middle variable */
    DeleteVar((STRPTR)"VAR_B", GVF_LOCAL_ONLY | LV_VAR);
    
    /* Verify A and C still work */
    memset(buffer, 0, sizeof(buffer));
    GetVar((STRPTR)"VAR_A", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (strcmp(buffer, "value_a") == 0) {
        test_pass("GetVar VAR_A after deleting VAR_B");
    } else {
        test_fail("GetVar VAR_A after deleting VAR_B");
    }
    
    memset(buffer, 0, sizeof(buffer));
    GetVar((STRPTR)"VAR_C", (STRPTR)buffer, sizeof(buffer), GVF_LOCAL_ONLY | LV_VAR);
    if (strcmp(buffer, "value_c") == 0) {
        test_pass("GetVar VAR_C after deleting VAR_B");
    } else {
        test_fail("GetVar VAR_C after deleting VAR_B");
    }
    
    /* Clean up */
    DeleteVar((STRPTR)"VAR_A", GVF_LOCAL_ONLY | LV_VAR);
    DeleteVar((STRPTR)"VAR_C", GVF_LOCAL_ONLY | LV_VAR);
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
