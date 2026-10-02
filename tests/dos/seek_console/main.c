/*
 * Integration Test: Seek on console handles
 *
 * Tests:
 *   - Seek on file handles (seekable)
 *   - Seek behavior verification
 *   
 * Note: This test cannot properly test true console Seek behavior when
 * output is redirected to a file (which is seekable). The test validates
 * that file I/O seek operations work correctly.
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
    BPTR fh;
    LONG result;
    LONG err;
    
    print("Seek on File Handles Test\n");
    print("=========================\n\n");
    
    /* Test 1: Create and seek in a file */
    print("Test 1: Create file and test Seek(BEGINNING)\n");
    
    fh = Open((CONST_STRPTR)"seek_test.tmp", MODE_NEWFILE);
    if (!fh) {
        test_fail("Create file", "Open failed");
        return 10;
    }
    
    /* Write some data */
    Write(fh, (CONST APTR)"0123456789", 10);
    
    /* Seek to beginning - returns previous position */
    result = Seek(fh, 0, OFFSET_BEGINNING);
    print("  Seek(BEGIN) returned: ");
    print_num(result);
    print("\n");
    
    if (result == 10) {
        test_pass("Seek(BEGIN) returns previous position");
    } else {
        test_fail("Seek(BEGIN)", "Expected previous position 10");
    }
    
    Close(fh);
    
    /* Test 2: Seek to end */
    print("\nTest 2: Seek to end of file\n");
    
    fh = Open((CONST_STRPTR)"seek_test.tmp", MODE_OLDFILE);
    if (!fh) {
        test_fail("Open file", "Failed");
        return 10;
    }
    
    result = Seek(fh, 0, OFFSET_END);
    print("  Seek(END) returned: ");
    print_num(result);
    print("\n");
    
    if (result == 0) {
        /* Position before seek was 0 */
        test_pass("Seek(END) from start returns 0");
    } else {
        test_fail("Seek(END)", "Expected previous position 0");
    }

    result = Seek(fh, 0, OFFSET_CURRENT);
    print("  Position after Seek(END): ");
    print_num(result);
    print("\n");
    if (result == 10) {
        test_pass("Seek(END) moved to end of file");
    } else {
        test_fail("Seek(END)", "Expected position 10");
    }
    
    Close(fh);
    
    /* Test 3: Seek with OFFSET_CURRENT */
    print("\nTest 3: Seek with OFFSET_CURRENT\n");
    
    fh = Open((CONST_STRPTR)"seek_test.tmp", MODE_OLDFILE);
    if (!fh) {
        test_fail("Open file", "Failed");
        return 10;
    }
    
    /* Read 5 bytes to move position */
    {
        char buf[10];
        Read(fh, buf, 5);
    }
    
    /* Seek back 2 bytes from current */
    result = Seek(fh, -2, OFFSET_CURRENT);
    print("  Seek(CURRENT, -2) returned: ");
    print_num(result);
    print("\n");
    
    if (result == 5) {
        test_pass("Seek(CURRENT) returns previous position");
    } else {
        test_fail("Seek(CURRENT)", "Expected previous position 5");
    }
    
    /* Get current position */
    result = Seek(fh, 0, OFFSET_CURRENT);
    print("  Current position: ");
    print_num(result);
    print("\n");
    
    if (result == 3) {
        test_pass("Position is 3 after seek back 2 from 5");
    } else {
        test_fail("Position query", "Expected position 3");
    }
    
    Close(fh);
    
    /* Test 4: Seek beyond EOF */
    print("\nTest 4: Seek beyond EOF\n");
    
    fh = Open((CONST_STRPTR)"seek_test.tmp", MODE_OLDFILE);
    if (!fh) {
        test_fail("Open file", "Failed");
        return 10;
    }
    
    result = Seek(fh, 100, OFFSET_BEGINNING);
    err = IoErr();      /* before print(): Write() resets IoErr() */
    print("  Seek(100) returned: ");
    print_num(result);
    print("\n");
    
    /* AmigaOS refuses to seek beyond EOF: -1, ERROR_SEEK_ERROR */
    print("  IoErr: ");
    print_num(err);
    print("\n");
    if (result == -1 && err == ERROR_SEEK_ERROR) {
        test_pass("Seek beyond EOF returned ERROR_SEEK_ERROR");
    } else {
        test_fail("Seek beyond EOF", "Expected -1 / ERROR_SEEK_ERROR");
    }

    result = Seek(fh, 0, OFFSET_CURRENT);
    if (result == 0) {
        test_pass("Failed seek left the position unchanged");
    } else {
        test_fail("Failed seek", "Position changed");
    }

    result = Seek(fh, -1, OFFSET_BEGINNING);
    err = IoErr();
    print("  Seek(-1) returned: ");
    print_num(result);
    print(", IoErr: ");
    print_num(err);
    print("\n");
    if (result == -1 && err == ERROR_SEEK_ERROR) {
        test_pass("Seek before start returned ERROR_SEEK_ERROR");
    } else {
        test_fail("Seek before start", "Expected -1 / ERROR_SEEK_ERROR");
    }
    
    Close(fh);
    
    /* Cleanup */
    DeleteFile((CONST_STRPTR)"seek_test.tmp");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
