/*
 * Integration Test: Special characters in filenames
 *
 * Tests:
 *   - Files with spaces in names
 *   - Files with numbers
 *   - Files with underscores and dashes
 *   - Files starting with dot
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

/* Test creating, reading, and deleting a file with given name */
static BOOL test_filename(const char *filename, const char *test_name)
{
    BPTR fh;
    BPTR lock;
    UBYTE buffer[32];
    const char *content = "test content";
    LONG result;
    BOOL success = TRUE;
    
    print("  Testing '");
    print(filename);
    print("'\n");
    
    /* Create file */
    fh = Open((CONST_STRPTR)filename, MODE_NEWFILE);
    if (!fh) {
        print("    Create failed\n");
        test_fail(test_name, "Create failed");
        return FALSE;
    }
    
    Write(fh, (CONST APTR)content, 12);
    Close(fh);
    print("    Created: OK\n");
    
    /* Lock and verify it exists */
    lock = Lock((CONST_STRPTR)filename, SHARED_LOCK);
    if (!lock) {
        print("    Lock failed\n");
        test_fail(test_name, "Lock failed");
        success = FALSE;
    } else {
        print("    Locked: OK\n");
        UnLock(lock);
    }
    
    /* Read back */
    fh = Open((CONST_STRPTR)filename, MODE_OLDFILE);
    if (!fh) {
        print("    Open for read failed\n");
        test_fail(test_name, "Open for read failed");
        success = FALSE;
    } else {
        result = Read(fh, buffer, 12);
        if (result == 12) {
            print("    Read: OK\n");
        } else {
            print("    Read failed\n");
            test_fail(test_name, "Read failed");
            success = FALSE;
        }
        Close(fh);
    }
    
    /* Delete */
    result = DeleteFile((CONST_STRPTR)filename);
    if (result) {
        print("    Deleted: OK\n");
    } else {
        print("    Delete failed\n");
        test_fail(test_name, "Delete failed");
        success = FALSE;
    }
    
    if (success) {
        test_pass(test_name);
    }
    
    return success;
}

int main(void)
{
    print("Special Characters Test\n");
    print("=======================\n\n");
    
    /* Test 1: File with spaces */
    print("Test 1: Filename with spaces\n");
    test_filename("file with spaces.txt", "Spaces in name");
    
    /* Test 2: File with numbers */
    print("\nTest 2: Filename with numbers\n");
    test_filename("file123.txt", "Numbers in name");
    
    /* Test 3: File with underscores */
    print("\nTest 3: Filename with underscores\n");
    test_filename("my_test_file.txt", "Underscores");
    
    /* Test 4: File with dashes */
    print("\nTest 4: Filename with dashes\n");
    test_filename("my-test-file.txt", "Dashes");
    
    /* Test 5: File starting with dot */
    print("\nTest 5: Filename starting with dot\n");
    test_filename(".hidden_file", "Dot prefix");
    
    /* Test 6: Long filename */
    print("\nTest 6: Long filename\n");
    test_filename("this_is_a_reasonably_long_filename.txt", "Long name");
    
    /* Test 7: Mixed special chars */
    print("\nTest 7: Mixed special characters\n");
    test_filename("test-file_123 (copy).txt", "Mixed chars");
    
    /* Test 8: Directory with spaces */
    print("\nTest 8: Directory with spaces\n");
    {
        BPTR lock;
        BOOL success = TRUE;
        
        lock = CreateDir((CONST_STRPTR)"dir with spaces");
        if (lock) {
            print("  Created 'dir with spaces': OK\n");
            UnLock(lock);
            
            /* Try to lock it */
            lock = Lock((CONST_STRPTR)"dir with spaces", SHARED_LOCK);
            if (lock) {
                print("  Locked: OK\n");
                UnLock(lock);
            } else {
                print("  Lock failed\n");
                success = FALSE;
            }
            
            /* Delete it */
            if (DeleteFile((CONST_STRPTR)"dir with spaces")) {
                print("  Deleted: OK\n");
            } else {
                print("  Delete failed\n");
                success = FALSE;
            }
            
            if (success) {
                test_pass("Directory with spaces");
            } else {
                test_fail("Directory with spaces", "Operation failed");
            }
        } else {
            print("  Create failed\n");
            test_fail("Directory with spaces", "Create failed");
        }
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
