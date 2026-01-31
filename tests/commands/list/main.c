/*
 * Integration Test: LIST command / Directory Examine functions
 *
 * Tests:
 *   - Lock/Examine/ExNext for directory listing
 *   - File size, protection, date reading
 *   - Directory vs file detection
 *   - Pattern filtering (manual simulation)
 *
 * Note: This test uses DOS functions directly rather than the LIST command
 * to test the underlying directory enumeration functionality.
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
    char buf[16];
    char *p = buf + 15;
    *p = '\0';
    BOOL neg = n < 0;
    unsigned long num = neg ? -n : n;
    do {
        *--p = '0' + (num % 10);
        num /= 10;
    } while (num);
    if (neg) *--p = '-';
    print(p);
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

static void test_fail(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    tests_failed++;
}

/* Create a test file */
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

/* Simple string compare */
static int str_equal(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0');
}

/* Check if string ends with suffix */
static int str_ends_with(const char *str, const char *suffix)
{
    LONG slen = 0, suflen = 0;
    const char *p;
    
    for (p = str; *p; p++) slen++;
    for (p = suffix; *p; p++) suflen++;
    
    if (suflen > slen) return 0;
    
    return str_equal(str + slen - suflen, suffix);
}

int main(void)
{
    BPTR lock;
    struct FileInfoBlock *fib;
    int file_count, dir_count, txt_count;
    
    print("LIST/Directory Examine Test\n");
    print("===========================\n\n");
    
    /* Cleanup any leftover test structure */
    DeleteFile((CONST_STRPTR)"list_test/file1.txt");
    DeleteFile((CONST_STRPTR)"list_test/file2.txt");
    DeleteFile((CONST_STRPTR)"list_test/readme.doc");
    DeleteFile((CONST_STRPTR)"list_test/subdir");
    DeleteFile((CONST_STRPTR)"list_test");
    
    /* Create test directory structure */
    print("Setup: Creating test structure\n");
    
    lock = CreateDir((CONST_STRPTR)"list_test");
    if (lock) {
        UnLock(lock);
        print("  Created list_test/\n");
    } else {
        print("ERROR: Could not create test directory\n");
        return 20;
    }
    
    if (!create_test_file("list_test/file1.txt", "Content 1")) {
        print("ERROR: Could not create file1.txt\n");
        return 20;
    }
    print("  Created file1.txt (9 bytes)\n");
    
    if (!create_test_file("list_test/file2.txt", "Content 222222")) {
        print("ERROR: Could not create file2.txt\n");
        return 20;
    }
    print("  Created file2.txt (14 bytes)\n");
    
    if (!create_test_file("list_test/readme.doc", "Documentation text here")) {
        print("ERROR: Could not create readme.doc\n");
        return 20;
    }
    print("  Created readme.doc (23 bytes)\n");
    
    lock = CreateDir((CONST_STRPTR)"list_test/subdir");
    if (lock) {
        UnLock(lock);
        print("  Created subdir/\n");
    }
    
    /* Allocate FileInfoBlock */
    fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        print("ERROR: Could not allocate FIB\n");
        return 20;
    }
    
    /* Test 1: Lock and Examine directory */
    print("\nTest 1: Lock and Examine directory\n");
    lock = Lock((CONST_STRPTR)"list_test", SHARED_LOCK);
    if (lock) {
        if (Examine(lock, fib)) {
            if (fib->fib_DirEntryType > 0) {
                test_pass("Directory lock and examine");
            } else {
                test_fail("Not recognized as directory");
            }
        } else {
            test_fail("Examine failed");
            UnLock(lock);
            FreeDosObject(DOS_FIB, fib);
            return 20;
        }
    } else {
        test_fail("Lock failed");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    /* Test 2: Count entries with ExNext */
    print("\nTest 2: Count directory entries with ExNext\n");
    file_count = 0;
    dir_count = 0;
    txt_count = 0;
    
    while (ExNext(lock, fib)) {
        print("  Found: ");
        print(fib->fib_FileName);
        
        if (fib->fib_DirEntryType > 0) {
            print(" (DIR)\n");
            dir_count++;
        } else {
            print(" (");
            print_num(fib->fib_Size);
            print(" bytes)\n");
            file_count++;
            
            /* Check for .txt extension */
            if (str_ends_with(fib->fib_FileName, ".txt")) {
                txt_count++;
            }
        }
    }
    
    print("  Total files: ");
    print_num(file_count);
    print(", dirs: ");
    print_num(dir_count);
    print("\n");
    
    if (file_count == 3 && dir_count == 1) {
        test_pass("Correct entry count (3 files, 1 dir)");
    } else {
        test_fail("Wrong entry count");
    }
    
    UnLock(lock);
    
    /* Test 3: Test FILES filter (simulate by checking fib_DirEntryType) */
    print("\nTest 3: Count only files (like LIST FILES)\n");
    print("  Found ");
    print_num(file_count);
    print(" files\n");
    if (file_count == 3) {
        test_pass("FILES filter simulation");
    } else {
        test_fail("Wrong file count");
    }
    
    /* Test 4: Test DIRS filter (simulate by checking fib_DirEntryType) */
    print("\nTest 4: Count only directories (like LIST DIRS)\n");
    print("  Found ");
    print_num(dir_count);
    print(" directories\n");
    if (dir_count == 1) {
        test_pass("DIRS filter simulation");
    } else {
        test_fail("Wrong directory count");
    }
    
    /* Test 5: Pattern matching simulation (*.txt files) */
    print("\nTest 5: Pattern matching simulation (*.txt)\n");
    print("  Found ");
    print_num(txt_count);
    print(" .txt files\n");
    if (txt_count == 2) {
        test_pass("Pattern matching (2 .txt files)");
    } else {
        test_fail("Wrong .txt count");
    }
    
    /* Test 6: Check file sizes are correct */
    print("\nTest 6: Verify file size reading\n");
    lock = Lock((CONST_STRPTR)"list_test/file1.txt", SHARED_LOCK);
    if (lock) {
        if (Examine(lock, fib)) {
            if (fib->fib_Size == 9) {
                test_pass("file1.txt size correct (9 bytes)");
            } else {
                print("  Got size: ");
                print_num(fib->fib_Size);
                print("\n");
                test_fail("Wrong file size");
            }
        } else {
            test_fail("Examine failed");
        }
        UnLock(lock);
    } else {
        test_fail("Lock failed");
    }
    
    /* Test 7: Verify protection bits are readable */
    print("\nTest 7: Verify protection bits\n");
    lock = Lock((CONST_STRPTR)"list_test/file1.txt", SHARED_LOCK);
    if (lock) {
        if (Examine(lock, fib)) {
            /* Just verify we got some protection value */
            print("  Protection: ");
            print_num(fib->fib_Protection);
            print("\n");
            test_pass("Protection bits readable");
        } else {
            test_fail("Examine failed");
        }
        UnLock(lock);
    } else {
        test_fail("Lock failed");
    }
    
    /* Cleanup */
    FreeDosObject(DOS_FIB, fib);
    
    print("\nCleanup: Removing test structure\n");
    DeleteFile((CONST_STRPTR)"list_test/file1.txt");
    DeleteFile((CONST_STRPTR)"list_test/file2.txt");
    DeleteFile((CONST_STRPTR)"list_test/readme.doc");
    DeleteFile((CONST_STRPTR)"list_test/subdir");
    DeleteFile((CONST_STRPTR)"list_test");
    print("  Done\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
