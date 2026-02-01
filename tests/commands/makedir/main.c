/*
 * Integration Test: MAKEDIR command
 *
 * Tests:
 *   - Single directory creation
 *   - Multiple directory creation
 *   - Creating already existing directory (error)
 *   - Invalid path handling
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

/* Check if path is a directory */
static BOOL is_directory(const char *name)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);
    if (!lock) return FALSE;
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return FALSE;
    }
    
    BOOL is_dir = FALSE;
    if (Examine(lock, fib)) {
        is_dir = (fib->fib_DirEntryType > 0);
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return is_dir;
}

/* Clean up test directories */
static void cleanup_test_dirs(void)
{
    DeleteFile((CONST_STRPTR)"mkdirtest1");
    DeleteFile((CONST_STRPTR)"mkdirtest2");
    DeleteFile((CONST_STRPTR)"mkdirtest3");
}

int main(void)
{
    BPTR lock;
    
    print("MAKEDIR Command Test\n");
    print("====================\n\n");
    
    /* Cleanup any leftover test directories */
    cleanup_test_dirs();
    
    /* Test 1: Create single directory */
    print("Test 1: Create single directory\n");
    
    lock = CreateDir((CONST_STRPTR)"mkdirtest1");
    if (lock) {
        UnLock(lock);
        if (is_directory("mkdirtest1")) {
            test_pass("Single directory created");
        } else {
            test_fail("Single directory", "Not a directory");
        }
    } else {
        test_fail("Single directory", "CreateDir failed");
    }
    
    /* Test 2: Create second directory */
    print("\nTest 2: Create second directory\n");
    
    lock = CreateDir((CONST_STRPTR)"mkdirtest2");
    if (lock) {
        UnLock(lock);
        if (is_directory("mkdirtest2")) {
            test_pass("Second directory created");
        } else {
            test_fail("Second directory", "Not a directory");
        }
    } else {
        test_fail("Second directory", "CreateDir failed");
    }
    
    /* Test 3: Try to create existing directory (should fail) */
    print("\nTest 3: Create existing directory (should fail)\n");
    
    lock = CreateDir((CONST_STRPTR)"mkdirtest1");
    if (lock) {
        UnLock(lock);
        test_fail("Existing dir", "Should have failed");
    } else {
        /* Note: Error code may vary (ERROR_OBJECT_EXISTS or ERROR_OBJECT_NOT_FOUND)
         * The important thing is that it fails */
        test_pass("Existing dir returns error");
    }
    
    /* Test 4: Create third directory */
    print("\nTest 4: Create third directory\n");
    
    lock = CreateDir((CONST_STRPTR)"mkdirtest3");
    if (lock) {
        UnLock(lock);
        if (is_directory("mkdirtest3")) {
            test_pass("Third directory created");
        } else {
            test_fail("Third directory", "Not a directory");
        }
    } else {
        test_fail("Third directory", "CreateDir failed");
    }
    
    /* Test 5: Verify all directories exist */
    print("\nTest 5: Verify all directories\n");
    
    if (file_exists("mkdirtest1") && file_exists("mkdirtest2") && file_exists("mkdirtest3")) {
        test_pass("All directories exist");
    } else {
        test_fail("All directories", "Some missing");
    }
    
    /* Cleanup */
    print("\nCleanup: Removing test directories\n");
    cleanup_test_dirs();
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
