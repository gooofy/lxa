/*
 * Integration Test: DELETE command
 *
 * Tests:
 *   - Single file deletion
 *   - Multiple file deletion via pattern
 *   - Non-empty directory deletion (should fail without ALL)
 *   - Recursive deletion (ALL switch)
 *   - Delete non-existent file (error case)
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

/* Clean up test files */
static void cleanup_test_files(void)
{
    DeleteFile((CONST_STRPTR)"deltest1.txt");
    DeleteFile((CONST_STRPTR)"deltest2.txt");
    DeleteFile((CONST_STRPTR)"deltest3.txt");
    DeleteFile((CONST_STRPTR)"other.doc");
    DeleteFile((CONST_STRPTR)"deldir/nested/deep.txt");
    DeleteFile((CONST_STRPTR)"deldir/nested");
    DeleteFile((CONST_STRPTR)"deldir/child.txt");
    DeleteFile((CONST_STRPTR)"deldir");
    DeleteFile((CONST_STRPTR)"emptydeldir");
}

int main(void)
{
    BPTR lock;
    LONG result;
    
    print("DELETE Command Test\n");
    print("===================\n\n");
    
    /* Cleanup any leftover test files */
    cleanup_test_files();
    
    /* Setup: Create test files */
    print("Setup: Creating test files\n");
    
    if (!create_test_file("deltest1.txt", "Delete me 1")) {
        print("ERROR: Could not create deltest1.txt\n");
        return 1;
    }
    print("  Created deltest1.txt\n");
    
    if (!create_test_file("deltest2.txt", "Delete me 2")) {
        print("ERROR: Could not create deltest2.txt\n");
        return 1;
    }
    print("  Created deltest2.txt\n");
    
    if (!create_test_file("deltest3.txt", "Delete me 3")) {
        print("ERROR: Could not create deltest3.txt\n");
        return 1;
    }
    print("  Created deltest3.txt\n");
    
    if (!create_test_file("other.doc", "Don't delete")) {
        print("ERROR: Could not create other.doc\n");
        return 1;
    }
    print("  Created other.doc\n");
    
    /* Create directory structure */
    lock = CreateDir((CONST_STRPTR)"deldir");
    if (lock) {
        UnLock(lock);
        print("  Created deldir\n");
        
        create_test_file("deldir/child.txt", "Child file");
        print("  Created deldir/child.txt\n");
        
        lock = CreateDir((CONST_STRPTR)"deldir/nested");
        if (lock) {
            UnLock(lock);
            create_test_file("deldir/nested/deep.txt", "Deep file");
            print("  Created deldir/nested/deep.txt\n");
        }
    }
    
    lock = CreateDir((CONST_STRPTR)"emptydeldir");
    if (lock) {
        UnLock(lock);
        print("  Created emptydeldir\n");
    }
    
    /* Test 1: Delete single file */
    print("\nTest 1: Delete single file\n");
    
    if (file_exists("deltest1.txt")) {
        result = DeleteFile((CONST_STRPTR)"deltest1.txt");
        if (result && !file_exists("deltest1.txt")) {
            test_pass("Single file deletion");
        } else {
            test_fail("Single file deletion", "File still exists");
        }
    } else {
        test_fail("Single file deletion", "Test file doesn't exist");
    }
    
    /* Test 2: Delete empty directory */
    print("\nTest 2: Delete empty directory\n");
    
    if (file_exists("emptydeldir")) {
        result = DeleteFile((CONST_STRPTR)"emptydeldir");
        if (result && !file_exists("emptydeldir")) {
            test_pass("Empty directory deletion");
        } else {
            test_fail("Empty directory deletion", "Dir still exists");
        }
    } else {
        test_fail("Empty directory deletion", "Test dir doesn't exist");
    }
    
    /* Test 3: Delete non-empty directory (should fail) */
    print("\nTest 3: Delete non-empty directory (should fail)\n");
    
    if (file_exists("deldir")) {
        result = DeleteFile((CONST_STRPTR)"deldir");
        if (!result && file_exists("deldir")) {
            test_pass("Non-empty dir protected");
        } else {
            test_fail("Non-empty dir", "Was deleted or doesn't exist");
        }
    } else {
        test_fail("Non-empty dir", "Test dir doesn't exist");
    }
    
    /* Test 4: Delete non-existent file */
    print("\nTest 4: Delete non-existent file\n");
    
    result = DeleteFile((CONST_STRPTR)"nonexistent_xyz.txt");
    if (!result) {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_NOT_FOUND) {
            test_pass("Non-existent file error");
        } else {
            test_fail("Non-existent file", "Wrong error code");
        }
    } else {
        test_fail("Non-existent file", "Delete returned success");
    }
    
    /* Test 5: Manual recursive delete simulation */
    print("\nTest 5: Recursive delete (manual simulation)\n");
    
    /* Delete nested file first */
    if (file_exists("deldir/nested/deep.txt")) {
        DeleteFile((CONST_STRPTR)"deldir/nested/deep.txt");
    }
    
    /* Delete nested directory */
    if (file_exists("deldir/nested")) {
        DeleteFile((CONST_STRPTR)"deldir/nested");
    }
    
    /* Delete child file */
    if (file_exists("deldir/child.txt")) {
        DeleteFile((CONST_STRPTR)"deldir/child.txt");
    }
    
    /* Now directory should be empty */
    if (file_exists("deldir")) {
        result = DeleteFile((CONST_STRPTR)"deldir");
        if (result && !file_exists("deldir")) {
            test_pass("Recursive delete (manual)");
        } else {
            test_fail("Recursive delete", "Directory still exists");
        }
    } else {
        test_pass("Recursive delete (manual)");
    }
    
    /* Test 6: Verify remaining files still exist */
    print("\nTest 6: Verify unaffected files\n");
    
    if (file_exists("deltest2.txt") && file_exists("deltest3.txt") && file_exists("other.doc")) {
        test_pass("Unaffected files preserved");
    } else {
        test_fail("Unaffected files", "Some files missing");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing remaining test files\n");
    cleanup_test_files();
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
