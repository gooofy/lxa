/*
 * Integration Test: Delete and Rename operations
 *
 * Tests:
 *   - DeleteFile on regular file
 *   - DeleteFile on non-existent file (should fail)
 *   - DeleteFile on non-empty directory (should fail)
 *   - DeleteFile on empty directory
 *   - Rename within same directory
 *   - Rename across directories
 *   - Rename non-existent file (should fail)
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

/* Create a test file with some content */
static BOOL create_test_file(const char *name, const char *content)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh) return FALSE;
    
    LONG len = 0;
    const char *p = content;
    while (*p++) len++;
    
    Write(fh, (CONST APTR)content, len);
    Close(fh);
    return TRUE;
}

/* Check if a file/dir exists */
static BOOL file_exists(const char *name)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        return TRUE;
    }
    return FALSE;
}

int main(void)
{
    BPTR lock;
    LONG result;
    
    print("Delete/Rename Test\n");
    print("==================\n\n");
    
    /* Setup: Create test files and directories */
    print("Setup: Creating test files and directories\n");
    
    if (!create_test_file("testfile.txt", "Hello, World!")) {
        print("ERROR: Could not create testfile.txt\n");
        return 1;
    }
    print("  Created testfile.txt\n");
    
    lock = CreateDir((CONST_STRPTR)"testdir_empty");
    if (!lock) {
        print("ERROR: Could not create testdir_empty\n");
        return 1;
    }
    UnLock(lock);
    print("  Created testdir_empty\n");
    
    lock = CreateDir((CONST_STRPTR)"testdir_nonempty");
    if (!lock) {
        print("ERROR: Could not create testdir_nonempty\n");
        return 1;
    }
    UnLock(lock);
    print("  Created testdir_nonempty\n");
    
    if (!create_test_file("testdir_nonempty/child.txt", "Child file")) {
        print("ERROR: Could not create child file\n");
        return 1;
    }
    print("  Created testdir_nonempty/child.txt\n");
    
    lock = CreateDir((CONST_STRPTR)"rename_dir");
    if (!lock) {
        print("ERROR: Could not create rename_dir\n");
        return 1;
    }
    UnLock(lock);
    print("  Created rename_dir\n");
    
    /* Test 1: Delete regular file */
    print("\nTest 1: Delete regular file\n");
    
    result = DeleteFile((CONST_STRPTR)"testfile.txt");
    if (result) {
        if (!file_exists("testfile.txt")) {
            test_pass("DeleteFile on file");
        } else {
            test_fail("DeleteFile on file", "File still exists");
        }
    } else {
        test_fail("DeleteFile on file", "DeleteFile returned FALSE");
    }
    
    /* Test 2: Delete non-existent file */
    print("\nTest 2: Delete non-existent file (should fail)\n");
    
    result = DeleteFile((CONST_STRPTR)"nonexistent_xyz.txt");
    if (!result) {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        test_pass("DeleteFile non-existent (correctly failed)");
    } else {
        test_fail("DeleteFile non-existent", "Should have failed");
    }
    
    /* Test 3: Delete non-empty directory (should fail) */
    print("\nTest 3: Delete non-empty directory (should fail)\n");
    
    result = DeleteFile((CONST_STRPTR)"testdir_nonempty");
    if (!result) {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        /* Directory is not empty error is typically ERROR_DIRECTORY_NOT_EMPTY (216) */
        test_pass("DeleteFile non-empty dir (correctly failed)");
    } else {
        test_fail("DeleteFile non-empty dir", "Should have failed");
    }
    
    /* Test 4: Delete empty directory */
    print("\nTest 4: Delete empty directory\n");
    
    result = DeleteFile((CONST_STRPTR)"testdir_empty");
    if (result) {
        if (!file_exists("testdir_empty")) {
            test_pass("DeleteFile empty dir");
        } else {
            test_fail("DeleteFile empty dir", "Dir still exists");
        }
    } else {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print(" - ");
        test_fail("DeleteFile empty dir", "DeleteFile returned FALSE");
    }
    
    /* Test 5: Rename file within same directory */
    print("\nTest 5: Rename file (same directory)\n");
    
    /* Create a file to rename */
    if (!create_test_file("rename_me.txt", "Rename test")) {
        print("ERROR: Could not create rename_me.txt\n");
        return 1;
    }
    
    result = Rename((CONST_STRPTR)"rename_me.txt", (CONST_STRPTR)"renamed.txt");
    if (result) {
        if (!file_exists("rename_me.txt") && file_exists("renamed.txt")) {
            test_pass("Rename same dir");
        } else {
            test_fail("Rename same dir", "Files not correct");
        }
    } else {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        test_fail("Rename same dir", "Rename returned FALSE");
    }
    
    /* Test 6: Rename file to different directory */
    print("\nTest 6: Rename file to different directory\n");
    
    result = Rename((CONST_STRPTR)"renamed.txt", (CONST_STRPTR)"rename_dir/moved.txt");
    if (result) {
        if (!file_exists("renamed.txt") && file_exists("rename_dir/moved.txt")) {
            test_pass("Rename across dirs");
        } else {
            test_fail("Rename across dirs", "Files not correct");
        }
    } else {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        test_fail("Rename across dirs", "Rename returned FALSE");
    }
    
    /* Test 7: Rename non-existent file (should fail) */
    print("\nTest 7: Rename non-existent file (should fail)\n");
    
    result = Rename((CONST_STRPTR)"nonexistent_xyz.txt", (CONST_STRPTR)"newfname.txt");
    if (!result) {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        test_pass("Rename non-existent (correctly failed)");
    } else {
        test_fail("Rename non-existent", "Should have failed");
    }
    
    /* Test 8: Rename directory */
    print("\nTest 8: Rename directory\n");
    
    lock = CreateDir((CONST_STRPTR)"rename_this_dir");
    if (lock) {
        UnLock(lock);
        
        result = Rename((CONST_STRPTR)"rename_this_dir", (CONST_STRPTR)"renamed_dir");
        if (result) {
            if (!file_exists("rename_this_dir") && file_exists("renamed_dir")) {
                test_pass("Rename directory");
            } else {
                test_fail("Rename directory", "Dirs not correct");
            }
        } else {
            LONG err = IoErr();
            print("  IoErr = ");
            print_num(err);
            print("\n");
            test_fail("Rename directory", "Rename returned FALSE");
        }
    } else {
        test_fail("Rename directory", "Could not create test dir");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test files and directories\n");
    
    /* Delete files inside directories first */
    DeleteFile((CONST_STRPTR)"rename_dir/moved.txt");
    DeleteFile((CONST_STRPTR)"testdir_nonempty/child.txt");
    
    /* Delete directories */
    DeleteFile((CONST_STRPTR)"rename_dir");
    DeleteFile((CONST_STRPTR)"testdir_nonempty");
    DeleteFile((CONST_STRPTR)"renamed_dir");
    
    /* Delete any leftover files */
    DeleteFile((CONST_STRPTR)"testfile.txt");
    DeleteFile((CONST_STRPTR)"rename_me.txt");
    DeleteFile((CONST_STRPTR)"renamed.txt");
    DeleteFile((CONST_STRPTR)"testdir_empty");
    
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
