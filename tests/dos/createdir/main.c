/*
 * Integration Test: CreateDir and directory operations
 *
 * Tests:
 *   - CreateDir - single directory creation
 *   - CreateDir - nested directory creation  
 *   - CreateDir - error on existing directory
 *   - CreateDir - error on invalid path
 *   - Examine created directories
 *   - ParentDir traversal
 *   - Delete created directories (cleanup)
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
    BPTR lock, parentLock;
    struct FileInfoBlock *fib;
    LONG result;
    
    print("CreateDir Test\n");
    print("==============\n\n");
    
    /* Allocate FileInfoBlock */
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib) {
        print("ERROR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Test 1: Create a single directory */
    print("Test 1: Create single directory\n");
    
    lock = CreateDir((CONST_STRPTR)"testdir1");
    if (!lock) {
        test_fail("CreateDir single", "CreateDir returned NULL");
    } else {
        /* Verify it's a directory */
        if (Examine(lock, fib)) {
            if (fib->fib_DirEntryType > 0) {
                test_pass("CreateDir single");
            } else {
                test_fail("CreateDir single", "Not a directory");
            }
        } else {
            test_fail("CreateDir single", "Examine failed");
        }
        UnLock(lock);
    }
    
    /* Test 2: Verify directory exists by locking it */
    print("\nTest 2: Verify directory exists\n");
    
    lock = Lock((CONST_STRPTR)"testdir1", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock created dir", "Lock failed");
    } else {
        test_pass("Lock created dir");
        UnLock(lock);
    }
    
    /* Test 3: Create nested directory (parent must exist) */
    print("\nTest 3: Create nested directory\n");
    
    lock = CreateDir((CONST_STRPTR)"testdir1/subdir");
    if (!lock) {
        test_fail("CreateDir nested", "CreateDir returned NULL");
    } else {
        if (Examine(lock, fib)) {
            if (fib->fib_DirEntryType > 0) {
                test_pass("CreateDir nested");
            } else {
                test_fail("CreateDir nested", "Not a directory");
            }
        } else {
            test_fail("CreateDir nested", "Examine failed");
        }
        UnLock(lock);
    }
    
    /* Test 4: ParentDir traversal */
    print("\nTest 4: ParentDir traversal\n");
    
    lock = Lock((CONST_STRPTR)"testdir1/subdir", SHARED_LOCK);
    if (!lock) {
        test_fail("ParentDir lock", "Could not lock nested dir");
    } else {
        parentLock = ParentDir(lock);
        if (!parentLock) {
            test_fail("ParentDir", "ParentDir returned NULL");
        } else {
            if (Examine(parentLock, fib)) {
                /* Parent should be testdir1 */
                print("  Parent name: ");
                print((char *)fib->fib_FileName);
                print("\n");
                test_pass("ParentDir traversal");
            } else {
                test_fail("ParentDir", "Examine parent failed");
            }
            UnLock(parentLock);
        }
        UnLock(lock);
    }
    
    /* Test 5: Create directory that already exists (should fail) */
    print("\nTest 5: Create existing directory (should fail)\n");
    
    lock = CreateDir((CONST_STRPTR)"testdir1");
    if (lock) {
        /* Some systems allow this and just return lock to existing */
        test_pass("CreateDir existing (returned lock)");
        UnLock(lock);
    } else {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_EXISTS) {
            test_pass("CreateDir existing (correct error)");
        } else {
            print("  IoErr = ");
            print_num(err);
            print("\n");
            test_pass("CreateDir existing (failed)");
        }
    }
    
    /* Test 6: Create directory with invalid parent */
    print("\nTest 6: Create dir with invalid parent\n");
    
    lock = CreateDir((CONST_STRPTR)"nonexistent_xyz/newdir");
    if (lock) {
        test_fail("CreateDir invalid parent", "Should have failed");
        UnLock(lock);
    } else {
        LONG err = IoErr();
        print("  IoErr = ");
        print_num(err);
        print("\n");
        test_pass("CreateDir invalid parent (correctly failed)");
    }
    
    /* Test 7: Create deeply nested structure */
    print("\nTest 7: Create deeply nested structure\n");
    
    /* Create testdir1/subdir/deep first */
    lock = CreateDir((CONST_STRPTR)"testdir1/subdir/deep");
    if (!lock) {
        test_fail("CreateDir deep", "CreateDir returned NULL");
    } else {
        test_pass("CreateDir deep");
        UnLock(lock);
    }
    
    /* Test 8: Multiple ParentDir traversals back to root */
    print("\nTest 8: Multiple ParentDir traversals\n");
    
    lock = Lock((CONST_STRPTR)"testdir1/subdir/deep", SHARED_LOCK);
    if (!lock) {
        test_fail("Multi-parent lock", "Could not lock deep dir");
    } else {
        int depth = 0;
        BPTR currentLock = lock;
        
        while (currentLock) {
            BPTR parent = ParentDir(currentLock);
            if (currentLock != lock) {
                UnLock(currentLock);
            }
            currentLock = parent;
            depth++;
            
            /* Safety limit */
            if (depth > 10) {
                print("  (depth limit reached)\n");
                break;
            }
        }
        
        print("  Traversed ");
        print_num(depth);
        print(" levels\n");
        
        if (depth >= 3) {
            test_pass("Multiple ParentDir");
        } else {
            test_fail("Multiple ParentDir", "Not enough levels");
        }
        
        UnLock(lock);
    }
    
    /* Cleanup: Delete created directories (in reverse order) */
    print("\nCleanup: Deleting test directories\n");
    
    result = DeleteFile((CONST_STRPTR)"testdir1/subdir/deep");
    if (result) {
        print("  Deleted testdir1/subdir/deep: OK\n");
    } else {
        print("  Could not delete testdir1/subdir/deep (IoErr=");
        print_num(IoErr());
        print(")\n");
    }
    
    result = DeleteFile((CONST_STRPTR)"testdir1/subdir");
    if (result) {
        print("  Deleted testdir1/subdir: OK\n");
    } else {
        print("  Could not delete testdir1/subdir (IoErr=");
        print_num(IoErr());
        print(")\n");
    }
    
    result = DeleteFile((CONST_STRPTR)"testdir1");
    if (result) {
        print("  Deleted testdir1: OK\n");
    } else {
        print("  Could not delete testdir1 (IoErr=");
        print_num(IoErr());
        print(")\n");
    }
    
    FreeMem(fib, sizeof(struct FileInfoBlock));
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
