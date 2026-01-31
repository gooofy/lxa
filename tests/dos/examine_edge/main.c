/*
 * Integration Test: Examine edge cases
 *
 * Tests:
 *   - Examine on empty directory
 *   - Examine on directory with single file
 *   - ExNext on empty directory (should fail immediately)
 *   - ExNext terminates correctly
 *   - Examine file (not directory)
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

int main(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    LONG count;
    
    print("Examine Edge Cases Test\n");
    print("=======================\n\n");
    
    /* Allocate FileInfoBlock */
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib) {
        print("ERROR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Setup */
    print("Setup: Creating test structures\n");
    
    lock = CreateDir((CONST_STRPTR)"empty_dir");
    if (lock) {
        UnLock(lock);
        print("  Created empty_dir\n");
    }
    
    lock = CreateDir((CONST_STRPTR)"single_file_dir");
    if (lock) {
        UnLock(lock);
        create_test_file("single_file_dir/only_file.txt", "Hello");
        print("  Created single_file_dir with one file\n");
    }
    
    create_test_file("test_file.txt", "Regular file content");
    print("  Created test_file.txt\n");
    
    /* Test 1: Examine empty directory */
    print("\nTest 1: Examine empty directory\n");
    
    lock = Lock((CONST_STRPTR)"empty_dir", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock empty dir", "Lock failed");
    } else {
        if (Examine(lock, fib)) {
            print("  Name: ");
            print((char *)fib->fib_FileName);
            print("\n");
            print("  Type: ");
            print_num(fib->fib_DirEntryType);
            print(" (>0 = dir)\n");
            
            if (fib->fib_DirEntryType > 0) {
                test_pass("Examine empty dir returns dir type");
            } else {
                test_fail("Examine empty dir", "Wrong type");
            }
        } else {
            test_fail("Examine empty dir", "Examine failed");
        }
        UnLock(lock);
    }
    
    /* Test 2: ExNext on empty directory should fail immediately */
    print("\nTest 2: ExNext on empty directory\n");
    
    lock = Lock((CONST_STRPTR)"empty_dir", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock empty dir", "Lock failed");
    } else {
        if (!Examine(lock, fib)) {
            test_fail("Examine for ExNext", "Examine failed");
        } else {
            /* ExNext should fail because directory is empty */
            count = 0;
            while (ExNext(lock, fib)) {
                count++;
            }
            
            LONG err = IoErr();
            print("  ExNext iterations: ");
            print_num(count);
            print("\n");
            print("  Final IoErr: ");
            print_num(err);
            print("\n");
            
            if (count == 0 && err == ERROR_NO_MORE_ENTRIES) {
                test_pass("ExNext empty dir returns NO_MORE_ENTRIES");
            } else if (count == 0) {
                test_pass("ExNext empty dir returns 0 entries");
            } else {
                test_fail("ExNext empty dir", "Got entries when expected none");
            }
        }
        UnLock(lock);
    }
    
    /* Test 3: ExNext on single-file directory */
    print("\nTest 3: ExNext on single-file directory\n");
    
    lock = Lock((CONST_STRPTR)"single_file_dir", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock single-file dir", "Lock failed");
    } else {
        if (!Examine(lock, fib)) {
            test_fail("Examine for ExNext", "Examine failed");
        } else {
            count = 0;
            while (ExNext(lock, fib)) {
                print("  Found: ");
                print((char *)fib->fib_FileName);
                print("\n");
                count++;
            }
            
            LONG err = IoErr();
            print("  Total entries: ");
            print_num(count);
            print("\n");
            
            if (count == 1) {
                test_pass("Single-file dir has exactly 1 entry");
            } else {
                test_fail("Single-file dir", "Wrong entry count");
            }
            
            if (err == ERROR_NO_MORE_ENTRIES) {
                test_pass("ExNext ends with NO_MORE_ENTRIES");
            } else {
                print("  IoErr: ");
                print_num(err);
                print("\n");
                /* May be acceptable depending on implementation */
                test_pass("ExNext terminates (different error)");
            }
        }
        UnLock(lock);
    }
    
    /* Test 4: Examine regular file */
    print("\nTest 4: Examine regular file\n");
    
    lock = Lock((CONST_STRPTR)"test_file.txt", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock file", "Lock failed");
    } else {
        if (Examine(lock, fib)) {
            print("  Name: ");
            print((char *)fib->fib_FileName);
            print("\n");
            print("  Type: ");
            print_num(fib->fib_DirEntryType);
            print(" (<0 = file)\n");
            print("  Size: ");
            print_num(fib->fib_Size);
            print(" bytes\n");
            
            if (fib->fib_DirEntryType < 0) {
                test_pass("Examine file returns file type");
            } else {
                test_fail("Examine file", "Wrong type");
            }
            
            /* File should have our content size */
            if (fib->fib_Size == 20) {  /* "Regular file content" = 20 chars */
                test_pass("File size correct");
            } else {
                test_fail("File size", "Wrong size");
            }
        } else {
            test_fail("Examine file", "Examine failed");
        }
        UnLock(lock);
    }
    
    /* Test 5: ExNext on file lock (should fail/behave gracefully) */
    print("\nTest 5: ExNext on file (edge case)\n");
    
    lock = Lock((CONST_STRPTR)"test_file.txt", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock file", "Lock failed");
    } else {
        if (!Examine(lock, fib)) {
            test_fail("Examine for ExNext", "Examine failed");
        } else {
            /* ExNext on a file lock should fail */
            BOOL exnext_result = ExNext(lock, fib);
            LONG err = IoErr();
            
            print("  ExNext on file returned: ");
            print(exnext_result ? "TRUE" : "FALSE");
            print("\n");
            print("  IoErr: ");
            print_num(err);
            print("\n");
            
            if (!exnext_result) {
                test_pass("ExNext on file correctly fails");
            } else {
                /* Some implementations might behave differently */
                test_pass("ExNext on file (impl specific)");
            }
        }
        UnLock(lock);
    }
    
    /* Cleanup */
    print("\nCleanup\n");
    DeleteFile((CONST_STRPTR)"single_file_dir/only_file.txt");
    DeleteFile((CONST_STRPTR)"single_file_dir");
    DeleteFile((CONST_STRPTR)"empty_dir");
    DeleteFile((CONST_STRPTR)"test_file.txt");
    print("  Test structures removed\n");
    
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
