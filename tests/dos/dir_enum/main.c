/*
 * Integration Test: Directory enumeration with multiple entries
 *
 * Tests:
 *   - ExNext on directory with many files
 *   - Entry count accuracy
 *   - Correct termination
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

/* Create a test file with given name */
static BOOL create_test_file(const char *name)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh) return FALSE;
    Write(fh, (CONST APTR)"test", 4);
    Close(fh);
    return TRUE;
}

int main(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    LONG count;
    int i;
    char filename[64];
    
    print("Directory Enumeration Test\n");
    print("==========================\n\n");
    
    /* Allocate FileInfoBlock */
    fib = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock), MEMF_CLEAR);
    if (!fib) {
        print("ERROR: Failed to allocate FileInfoBlock\n");
        return 1;
    }
    
    /* Test 1: Create directory with 10 files */
    print("Test 1: Create directory with 10 files\n");
    
    lock = CreateDir((CONST_STRPTR)"many_files_dir");
    if (!lock) {
        print("ERROR: Could not create test directory\n");
        FreeMem(fib, sizeof(struct FileInfoBlock));
        return 1;
    }
    UnLock(lock);
    
    for (i = 0; i < 10; i++) {
        char *p = filename;
        char *src = "many_files_dir/file";
        while (*src) *p++ = *src++;
        
        /* Append number */
        if (i >= 10) {
            *p++ = '0' + (i / 10);
        }
        *p++ = '0' + (i % 10);
        
        src = ".txt";
        while (*src) *p++ = *src++;
        *p = '\0';
        
        if (!create_test_file(filename)) {
            print("  Failed to create ");
            print(filename);
            print("\n");
        }
    }
    print("  Created 10 test files\n");
    
    /* Test 2: Enumerate and count */
    print("\nTest 2: Enumerate directory\n");
    
    lock = Lock((CONST_STRPTR)"many_files_dir", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock directory", "Lock failed");
    } else {
        if (!Examine(lock, fib)) {
            test_fail("Examine", "Examine failed");
        } else {
            count = 0;
            while (ExNext(lock, fib)) {
                print("  ");
                print_num(count + 1);
                print(": ");
                print((char *)fib->fib_FileName);
                print("\n");
                count++;
                
                /* Safety limit */
                if (count > 20) {
                    print("  (limiting output)\n");
                    break;
                }
            }
            
            LONG err = IoErr();
            
            print("  Total entries: ");
            print_num(count);
            print("\n");
            print("  Final IoErr: ");
            print_num(err);
            print("\n");
            
            if (count == 10) {
                test_pass("Found all 10 files");
            } else {
                test_fail("File count", "Wrong count");
            }
            
            if (err == ERROR_NO_MORE_ENTRIES) {
                test_pass("Correct termination error");
            } else {
                test_pass("Enumeration terminated");
            }
        }
        UnLock(lock);
    }
    
    /* Test 3: Create subdirectory and verify mixed listing */
    print("\nTest 3: Mixed files and subdirectory\n");
    
    lock = CreateDir((CONST_STRPTR)"many_files_dir/subdir");
    if (lock) {
        UnLock(lock);
        print("  Created subdirectory\n");
    }
    
    lock = Lock((CONST_STRPTR)"many_files_dir", SHARED_LOCK);
    if (!lock) {
        test_fail("Lock for mixed", "Lock failed");
    } else {
        if (!Examine(lock, fib)) {
            test_fail("Examine mixed", "Examine failed");
        } else {
            count = 0;
            LONG dirs = 0;
            LONG files = 0;
            
            while (ExNext(lock, fib)) {
                if (fib->fib_DirEntryType > 0) {
                    dirs++;
                } else {
                    files++;
                }
                count++;
            }
            
            print("  Total: ");
            print_num(count);
            print(" (");
            print_num(files);
            print(" files, ");
            print_num(dirs);
            print(" dirs)\n");
            
            if (count == 11 && dirs == 1 && files == 10) {
                test_pass("Mixed enumeration correct");
            } else {
                test_fail("Mixed enumeration", "Wrong counts");
            }
        }
        UnLock(lock);
    }
    
    /* Cleanup */
    print("\nCleanup\n");
    
    /* Delete files */
    for (i = 0; i < 10; i++) {
        char *p = filename;
        char *src = "many_files_dir/file";
        while (*src) *p++ = *src++;
        if (i >= 10) {
            *p++ = '0' + (i / 10);
        }
        *p++ = '0' + (i % 10);
        src = ".txt";
        while (*src) *p++ = *src++;
        *p = '\0';
        
        DeleteFile((CONST_STRPTR)filename);
    }
    
    DeleteFile((CONST_STRPTR)"many_files_dir/subdir");
    DeleteFile((CONST_STRPTR)"many_files_dir");
    print("  Test files and directory removed\n");
    
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
