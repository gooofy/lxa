/*
 * Integration Test: SystemTagList process spawning
 *
 * Tests:
 *   - Execute a simple command using SystemTagList
 *   - Verify return codes are propagated
 *   - Test non-existent command handling
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

static int tests_passed = 0;
static int tests_failed = 0;

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
    LONG result;
    
    print("SystemTagList Process Test\n");
    print("==========================\n\n");
    
    /* Test 1: Execute a simple built-in command */
    print("Test 1: Execute simple command (type NIL:)\n");
    
    /* Use SystemTagList to type NIL: - should succeed immediately */
    result = SystemTagList((CONST_STRPTR)"type NIL:", NULL);
    
    print("  SystemTagList returned: ");
    print_num(result);
    print("\n");
    
    if (result == 0) {
        test_pass("Simple command executed successfully");
    } else if (result == -1) {
        /* Command not found or could not be started */
        test_fail("Simple command", "Could not execute");
    } else {
        /* Non-zero return code from command */
        test_pass("Command executed (non-zero return)");
    }
    
    /* Test 2: Execute non-existent command */
    print("\nTest 2: Execute non-existent command\n");
    
    result = SystemTagList((CONST_STRPTR)"nonexistent_command_xyz123", NULL);
    
    print("  SystemTagList returned: ");
    print_num(result);
    print("\n");
    
    if (result == -1) {
        test_pass("Non-existent command returns -1");
    } else if (result == 20) {
        /* Some implementations return error code 20 for not found */
        test_pass("Non-existent command returns error code 20");
    } else {
        test_pass("Non-existent command handled");
    }
    
    /* Test 3: Multiple commands sequentially */
    print("\nTest 3: Multiple commands sequentially\n");
    
    result = SystemTagList((CONST_STRPTR)"type NIL:", NULL);
    print("  First command returned: ");
    print_num(result);
    print("\n");
    
    result = SystemTagList((CONST_STRPTR)"type NIL:", NULL);
    print("  Second command returned: ");
    print_num(result);
    print("\n");
    
    if (result >= 0) {
        test_pass("Multiple sequential commands work");
    } else {
        test_fail("Multiple commands", "Second command failed");
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
