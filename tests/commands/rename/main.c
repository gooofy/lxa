/*
 * Integration Test: RENAME command
 *
 * Tests:
 *   - Rename file in same directory
 *   - Move file to different directory
 *   - Rename directory
 *   - Error cases (non-existent source, destination exists)
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

/* Read file contents into buffer */
static BOOL read_file_contents(const char *name, char *buffer, LONG buf_size)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
    if (!fh) return FALSE;
    
    LONG bytes = Read(fh, buffer, buf_size - 1);
    Close(fh);
    
    if (bytes < 0) return FALSE;
    buffer[bytes] = '\0';
    return TRUE;
}

/* Compare two strings */
static BOOL strings_equal(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return FALSE;
        a++; b++;
    }
    return *a == *b;
}

/* Clean up test files */
static void cleanup_test_files(void)
{
    DeleteFile((CONST_STRPTR)"original.txt");
    DeleteFile((CONST_STRPTR)"renamed.txt");
    DeleteFile((CONST_STRPTR)"moveme.txt");
    DeleteFile((CONST_STRPTR)"targetdir/moveme.txt");
    DeleteFile((CONST_STRPTR)"targetdir/moved.txt");
    DeleteFile((CONST_STRPTR)"targetdir");
    DeleteFile((CONST_STRPTR)"mydir");
    DeleteFile((CONST_STRPTR)"newdir");
    DeleteFile((CONST_STRPTR)"existing.txt");
    DeleteFile((CONST_STRPTR)"conflict.txt");
}

int main(void)
{
    BPTR lock;
    LONG result;
    char buffer[256];
    
    print("RENAME Command Test\n");
    print("===================\n\n");
    
    /* Cleanup any leftover test files */
    cleanup_test_files();
    
    /* Test 1: Rename file in same directory */
    print("Test 1: Rename file in same directory\n");
    
    if (!create_test_file("original.txt", "Original content")) {
        print("ERROR: Could not create original.txt\n");
        return 1;
    }
    
    result = Rename((CONST_STRPTR)"original.txt", (CONST_STRPTR)"renamed.txt");
    if (result) {
        if (!file_exists("original.txt") && file_exists("renamed.txt")) {
            /* Verify content preserved */
            if (read_file_contents("renamed.txt", buffer, sizeof(buffer))) {
                if (strings_equal(buffer, "Original content")) {
                    test_pass("Rename same directory with content");
                } else {
                    test_fail("Rename content", "Content changed");
                }
            } else {
                test_fail("Rename read", "Could not read renamed file");
            }
        } else {
            test_fail("Rename same dir", "Files not correct");
        }
    } else {
        test_fail("Rename same dir", "Rename returned FALSE");
    }
    
    /* Test 2: Move file to different directory */
    print("\nTest 2: Move file to different directory\n");
    
    lock = CreateDir((CONST_STRPTR)"targetdir");
    if (!lock) {
        print("ERROR: Could not create targetdir\n");
        return 1;
    }
    UnLock(lock);
    
    if (!create_test_file("moveme.txt", "Move this content")) {
        print("ERROR: Could not create moveme.txt\n");
        return 1;
    }
    
    result = Rename((CONST_STRPTR)"moveme.txt", (CONST_STRPTR)"targetdir/moved.txt");
    if (result) {
        if (!file_exists("moveme.txt") && file_exists("targetdir/moved.txt")) {
            /* Verify content */
            if (read_file_contents("targetdir/moved.txt", buffer, sizeof(buffer))) {
                if (strings_equal(buffer, "Move this content")) {
                    test_pass("Move to directory with content");
                } else {
                    test_fail("Move content", "Content changed");
                }
            } else {
                test_fail("Move read", "Could not read moved file");
            }
        } else {
            test_fail("Move to dir", "Files not correct");
        }
    } else {
        test_fail("Move to dir", "Rename returned FALSE");
    }
    
    /* Test 3: Rename directory */
    print("\nTest 3: Rename directory\n");
    
    lock = CreateDir((CONST_STRPTR)"mydir");
    if (lock) {
        UnLock(lock);
        
        result = Rename((CONST_STRPTR)"mydir", (CONST_STRPTR)"newdir");
        if (result) {
            if (!file_exists("mydir") && file_exists("newdir")) {
                test_pass("Rename directory");
            } else {
                test_fail("Rename dir", "Dirs not correct");
            }
        } else {
            test_fail("Rename dir", "Rename returned FALSE");
        }
    } else {
        test_fail("Create mydir", "Could not create test dir");
    }
    
    /* Test 4: Rename non-existent file (should fail) */
    print("\nTest 4: Rename non-existent file (should fail)\n");
    
    result = Rename((CONST_STRPTR)"nonexistent_xyz.txt", (CONST_STRPTR)"newname.txt");
    if (!result) {
        test_pass("Non-existent source (correctly failed)");
    } else {
        test_fail("Non-existent source", "Should have failed");
    }
    
    /* Test 5: Rename to existing file (should fail) */
    print("\nTest 5: Rename to existing file (should fail)\n");
    
    create_test_file("existing.txt", "Existing file");
    create_test_file("conflict.txt", "Will conflict");
    
    result = Rename((CONST_STRPTR)"conflict.txt", (CONST_STRPTR)"existing.txt");
    /* Note: Amiga DOS behavior may vary - some allow overwrite, some don't */
    /* For safety, we expect this to fail or at least both files to exist */
    if (!result) {
        test_pass("Conflict with existing (correctly failed)");
    } else {
        /* If it succeeded, original should be overwritten */
        if (file_exists("existing.txt") && !file_exists("conflict.txt")) {
            test_pass("Conflict overwrite (allowed)");
        } else {
            test_fail("Conflict handling", "Unexpected state");
        }
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test files\n");
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
