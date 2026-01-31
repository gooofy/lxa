/*
 * Integration Test: ReadArgs comprehensive testing
 *
 * Tests all ReadArgs features:
 *   - /A required argument
 *   - /K keyword argument
 *   - /S switch
 *   - /N numeric argument
 *   - /M multiple arguments
 *   - KEY=value syntax
 *   - Quoted strings with spaces
 *   - Missing required argument handling
 *   - Multiple arguments in template
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
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
    struct RDArgs *rda;
    LONG args[8];
    struct Process *me = (struct Process *)FindTask(NULL);
    int i;
    
    print("ReadArgs Comprehensive Test\n");
    print("===========================\n\n");
    
    /* Test 1: Simple required argument (/A) */
    print("Test 1: Required argument (/A)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"testfile.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILE/A", args, NULL);
    if (!rda) {
        test_fail("Required arg", "ReadArgs failed");
    } else {
        if (args[0] && ((char *)args[0])[0] == 't') {
            test_pass("Required arg parsed");
        } else {
            test_fail("Required arg", "Wrong value");
        }
        FreeArgs(rda);
    }
    
    /* Test 2: Switch argument (/S) - present */
    print("\nTest 2: Switch argument (/S) - present\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"QUIET\n";
    
    rda = ReadArgs((CONST_STRPTR)"QUIET/S", args, NULL);
    if (!rda) {
        test_fail("Switch present", "ReadArgs failed");
    } else {
        if (args[0]) {
            test_pass("Switch is TRUE when present");
        } else {
            test_fail("Switch present", "Should be TRUE");
        }
        FreeArgs(rda);
    }
    
    /* Test 3: Switch argument (/S) - absent */
    print("\nTest 3: Switch argument (/S) - absent\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\n";
    
    rda = ReadArgs((CONST_STRPTR)"QUIET/S", args, NULL);
    if (!rda) {
        test_fail("Switch absent", "ReadArgs failed");
    } else {
        if (!args[0]) {
            test_pass("Switch is FALSE when absent");
        } else {
            test_fail("Switch absent", "Should be FALSE");
        }
        FreeArgs(rda);
    }
    
    /* Test 4: Keyword argument with = syntax (/K) */
    print("\nTest 4: Keyword with = syntax (TO=value)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"TO=output.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"TO/K", args, NULL);
    if (!rda) {
        test_fail("Keyword =", "ReadArgs failed");
    } else {
        if (args[0]) {
            print("    Value: ");
            print((char *)args[0]);
            print("\n");
            if (((char *)args[0])[0] == 'o') {
                test_pass("Keyword = syntax works");
            } else {
                test_fail("Keyword =", "Wrong value");
            }
        } else {
            test_fail("Keyword =", "Value not parsed");
        }
        FreeArgs(rda);
    }
    
    /* Test 5: Keyword argument with space syntax (/K) */
    print("\nTest 5: Keyword with space syntax (TO value)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"TO output.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"TO/K", args, NULL);
    if (!rda) {
        test_fail("Keyword space", "ReadArgs failed");
    } else {
        if (args[0]) {
            print("    Value: ");
            print((char *)args[0]);
            print("\n");
            if (((char *)args[0])[0] == 'o') {
                test_pass("Keyword space syntax works");
            } else {
                test_fail("Keyword space", "Wrong value");
            }
        } else {
            test_fail("Keyword space", "Value not parsed");
        }
        FreeArgs(rda);
    }
    
    /* Test 6: Numeric argument (/N) */
    print("\nTest 6: Numeric argument (/N)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"SIZE=42\n";
    
    rda = ReadArgs((CONST_STRPTR)"SIZE/K/N", args, NULL);
    if (!rda) {
        test_fail("Numeric", "ReadArgs failed");
    } else {
        if (args[0]) {
            /* /N returns pointer to LONG */
            LONG val = *(LONG *)args[0];
            print("    Value: ");
            print_num(val);
            print("\n");
            if (val == 42) {
                test_pass("Numeric value correct");
            } else {
                test_fail("Numeric", "Wrong value");
            }
        } else {
            test_fail("Numeric", "Value not parsed");
        }
        FreeArgs(rda);
    }
    
    /* Test 7: Negative numeric argument */
    print("\nTest 7: Negative numeric argument\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"SIZE=-123\n";
    
    rda = ReadArgs((CONST_STRPTR)"SIZE/K/N", args, NULL);
    if (!rda) {
        test_fail("Negative numeric", "ReadArgs failed");
    } else {
        if (args[0]) {
            LONG val = *(LONG *)args[0];
            print("    Value: ");
            print_num(val);
            print("\n");
            if (val == -123) {
                test_pass("Negative numeric correct");
            } else {
                test_fail("Negative numeric", "Wrong value");
            }
        } else {
            test_fail("Negative numeric", "Value not parsed");
        }
        FreeArgs(rda);
    }
    
    /* Test 8: Missing required argument should fail */
    print("\nTest 8: Missing required argument\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILE/A", args, NULL);
    if (rda) {
        test_fail("Missing required", "Should have failed");
        FreeArgs(rda);
    } else {
        LONG err = IoErr();
        print("    IoErr: ");
        print_num(err);
        print("\n");
        test_pass("Correctly rejected missing required");
    }
    
    /* Test 9: Quoted string with spaces */
    print("\nTest 9: Quoted string with spaces\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\"file with spaces.txt\"\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILE/A", args, NULL);
    if (!rda) {
        test_fail("Quoted string", "ReadArgs failed");
    } else {
        if (args[0]) {
            print("    Value: '");
            print((char *)args[0]);
            print("'\n");
            /* Check it includes the space */
            char *s = (char *)args[0];
            BOOL has_space = FALSE;
            while (*s) {
                if (*s == ' ') has_space = TRUE;
                s++;
            }
            if (has_space) {
                test_pass("Quoted string with spaces parsed");
            } else {
                test_fail("Quoted string", "Space not preserved");
            }
        } else {
            test_fail("Quoted string", "Value not parsed");
        }
        FreeArgs(rda);
    }
    
    /* Test 10: Multiple template items */
    print("\nTest 10: Multiple template items\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"input.txt TO=output.txt ALL\n";
    
    rda = ReadArgs((CONST_STRPTR)"FROM/A,TO/K,ALL/S", args, NULL);
    if (!rda) {
        test_fail("Multiple items", "ReadArgs failed");
    } else {
        print("    FROM: ");
        if (args[0]) print((char *)args[0]); else print("(null)");
        print("\n    TO: ");
        if (args[1]) print((char *)args[1]); else print("(null)");
        print("\n    ALL: ");
        print(args[2] ? "TRUE" : "FALSE");
        print("\n");
        
        if (args[0] && args[1] && args[2]) {
            test_pass("Multiple items parsed");
        } else {
            test_fail("Multiple items", "Some values missing");
        }
        FreeArgs(rda);
    }
    
    /* Test 11: Optional argument not provided */
    print("\nTest 11: Optional argument not provided\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"input.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FROM/A,TO/K", args, NULL);
    if (!rda) {
        test_fail("Optional missing", "ReadArgs failed");
    } else {
        if (args[0] && !args[1]) {
            test_pass("Optional arg correctly NULL");
        } else {
            test_fail("Optional missing", "Wrong state");
        }
        FreeArgs(rda);
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
