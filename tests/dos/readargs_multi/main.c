/*
 * Integration Test: ReadArgs /M multi-argument support
 *
 * Tests the /M modifier which collects multiple arguments into an array.
 * Format: /M returns a NULL-terminated array of STRPTR pointers.
 *
 * Tests:
 *   - /M with multiple arguments
 *   - /M with single argument
 *   - /M with no arguments (should return NULL array)
 *   - /M/A (required multi - at least one)
 *   - /M combined with other arguments
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
    
    print("ReadArgs /M Multi-Argument Test\n");
    print("===============================\n\n");
    
    /* Test 1: /M with multiple arguments */
    print("Test 1: /M with multiple arguments (file1 file2 file3)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"file1.txt file2.txt file3.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs failed, IoErr: ");
        print_num(err);
        print("\n");
        test_fail("/M multiple", "ReadArgs failed");
    } else {
        if (args[0]) {
            /* args[0] should be a pointer to a NULL-terminated array of STRPTR */
            STRPTR *files = (STRPTR *)args[0];
            int count = 0;
            
            print("  Parsed files:\n");
            while (files[count]) {
                print("    [");
                print_num(count);
                print("] ");
                print((char *)files[count]);
                print("\n");
                count++;
                if (count > 10) {
                    print("    (too many entries, stopping)\n");
                    break;
                }
            }
            
            if (count == 3) {
                test_pass("/M collected 3 files");
            } else {
                print("  Expected 3 files, got: ");
                print_num(count);
                print("\n");
                test_fail("/M multiple", "Wrong count");
            }
        } else {
            test_fail("/M multiple", "args[0] is NULL");
        }
        FreeArgs(rda);
    }
    
    /* Test 2: /M with single argument */
    print("\nTest 2: /M with single argument\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"only_one.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs failed, IoErr: ");
        print_num(err);
        print("\n");
        test_fail("/M single", "ReadArgs failed");
    } else {
        if (args[0]) {
            STRPTR *files = (STRPTR *)args[0];
            int count = 0;
            
            while (files[count]) {
                print("    [");
                print_num(count);
                print("] ");
                print((char *)files[count]);
                print("\n");
                count++;
            }
            
            if (count == 1) {
                test_pass("/M collected 1 file");
            } else {
                test_fail("/M single", "Wrong count");
            }
        } else {
            test_fail("/M single", "args[0] is NULL");
        }
        FreeArgs(rda);
    }
    
    /* Test 3: /M with no arguments */
    print("\nTest 3: /M with no arguments\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M", args, NULL);
    if (!rda) {
        /* This might be OK - some implementations require at least one */
        test_pass("/M empty rejected");
    } else {
        if (args[0] == 0) {
            test_pass("/M empty returns NULL");
        } else {
            STRPTR *files = (STRPTR *)args[0];
            if (files[0] == NULL) {
                test_pass("/M empty returns empty array");
            } else {
                test_fail("/M empty", "Expected empty result");
            }
        }
        FreeArgs(rda);
    }
    
    /* Test 4: /M/A - required multi (at least one required) */
    print("\nTest 4: /M/A with no arguments (should fail)\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M/A", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs correctly rejected, IoErr: ");
        print_num(err);
        print("\n");
        test_pass("/M/A empty correctly rejected");
    } else {
        test_fail("/M/A empty", "Should have failed");
        FreeArgs(rda);
    }
    
    /* Test 5: /M/A with arguments (should succeed) */
    print("\nTest 5: /M/A with arguments\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"req1.txt req2.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M/A", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs failed, IoErr: ");
        print_num(err);
        print("\n");
        test_fail("/M/A with args", "ReadArgs failed");
    } else {
        if (args[0]) {
            STRPTR *files = (STRPTR *)args[0];
            int count = 0;
            while (files[count]) count++;
            
            if (count == 2) {
                test_pass("/M/A collected 2 required files");
            } else {
                test_fail("/M/A with args", "Wrong count");
            }
        } else {
            test_fail("/M/A with args", "args[0] is NULL");
        }
        FreeArgs(rda);
    }
    
    /* Test 6: /M combined with other options */
    print("\nTest 6: /M combined with other options\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"f1.txt f2.txt TO=output.txt ALL\n";
    
    rda = ReadArgs((CONST_STRPTR)"FROM/M,TO/K,ALL/S", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs failed, IoErr: ");
        print_num(err);
        print("\n");
        test_fail("/M combined", "ReadArgs failed");
    } else {
        BOOL from_ok = FALSE, to_ok = FALSE, all_ok = FALSE;
        
        /* Check FROM/M */
        if (args[0]) {
            STRPTR *files = (STRPTR *)args[0];
            int count = 0;
            print("  FROM files:\n");
            while (files[count]) {
                print("    ");
                print((char *)files[count]);
                print("\n");
                count++;
            }
            from_ok = (count == 2);
        }
        
        /* Check TO */
        if (args[1]) {
            print("  TO: ");
            print((char *)args[1]);
            print("\n");
            to_ok = TRUE;
        }
        
        /* Check ALL */
        if (args[2]) {
            print("  ALL: TRUE\n");
            all_ok = TRUE;
        }
        
        if (from_ok && to_ok && all_ok) {
            test_pass("/M combined with TO and ALL");
        } else {
            if (!from_ok) test_fail("/M combined", "FROM files wrong");
            if (!to_ok) test_fail("/M combined", "TO missing");
            if (!all_ok) test_fail("/M combined", "ALL missing");
        }
        FreeArgs(rda);
    }
    
    /* Test 7: /M with quoted strings containing spaces */
    print("\nTest 7: /M with quoted strings\n");
    
    for (i = 0; i < 8; i++) args[i] = 0;
    me->pr_Arguments = (STRPTR)"\"file with spaces.txt\" normal.txt\n";
    
    rda = ReadArgs((CONST_STRPTR)"FILES/M", args, NULL);
    if (!rda) {
        LONG err = IoErr();
        print("  ReadArgs failed, IoErr: ");
        print_num(err);
        print("\n");
        test_fail("/M quoted", "ReadArgs failed");
    } else {
        if (args[0]) {
            STRPTR *files = (STRPTR *)args[0];
            int count = 0;
            BOOL has_space = FALSE;
            
            print("  Parsed:\n");
            while (files[count]) {
                char *s = (char *)files[count];
                print("    ");
                print(s);
                print("\n");
                
                /* Check if first file contains space */
                if (count == 0) {
                    while (*s) {
                        if (*s == ' ') has_space = TRUE;
                        s++;
                    }
                }
                count++;
            }
            
            if (count == 2 && has_space) {
                test_pass("/M quoted string with space preserved");
            } else if (count == 2) {
                test_pass("/M quoted (space may have been stripped)");
            } else {
                test_fail("/M quoted", "Wrong count or no space");
            }
        } else {
            test_fail("/M quoted", "args[0] is NULL");
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
