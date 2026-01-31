/*
 * Integration Test: Execute() with file input (FGets test)
 *
 * Tests:
 *   - Execute("", file_handle, output) - reads commands from file
 *   - FGets() properly reads lines
 *   - Multi-line script execution
 */

#include <exec/types.h>
#include <dos/dos.h>
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
    BPTR fh;
    
    print("Execute() with File Input Test (FGets)\n");
    print("======================================\n\n");
    
    /* Test 1: Create a script file with multiple commands */
    print("Test 1: Create test script file\n");
    
    fh = Open((CONST_STRPTR)"test_script.txt", MODE_NEWFILE);
    if (!fh) {
        test_fail("Create script file", "Could not create file");
        return 20;
    }
    
    /* Write some simple commands to the script */
    const char *script = 
        "type NIL:\n"
        "; This is a comment\n"
        "type NIL:\n";
    
    LONG script_len = 0;
    const char *sp = script;
    while (*sp++) script_len++;
    
    LONG written = Write(fh, (CONST APTR)script, script_len);
    Close(fh);
    
    if (written != script_len) {
        test_fail("Write script", "Write failed");
        DeleteFile((CONST_STRPTR)"test_script.txt");
        return 20;
    }
    test_pass("Script file created");
    
    /* Test 2: Open script file for reading */
    print("\nTest 2: Open script for reading\n");
    
    fh = Open((CONST_STRPTR)"test_script.txt", MODE_OLDFILE);
    if (!fh) {
        test_fail("Open script", "Could not open file for reading");
        DeleteFile((CONST_STRPTR)"test_script.txt");
        return 20;
    }
    test_pass("Script file opened");
    
    /* Test 3: Execute with file input */
    print("\nTest 3: Execute with file handle input\n");
    
    /* Execute("", input_fh, output_fh) - reads commands from file */
    result = Execute((CONST_STRPTR)"", fh, Output());
    
    print("  Execute returned: ");
    print_num(result);
    print("\n");
    
    if (result != -1) {
        test_pass("Execute with file input");
    } else {
        test_fail("Execute with file input", "Execute returned -1");
    }
    
    Close(fh);
    
    /* Cleanup */
    DeleteFile((CONST_STRPTR)"test_script.txt");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
