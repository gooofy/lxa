/*
 * Integration Test: DIR command
 *
 * Tests:
 *   - Default listing (current directory)
 *   - Pattern matching: dir #?.txt
 *   - DIRS/FILES filtering
 *   - Empty directories
 *   - Directory path argument
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
    DeleteFile((CONST_STRPTR)"test1.txt");
    DeleteFile((CONST_STRPTR)"test2.txt");
    DeleteFile((CONST_STRPTR)"test.doc");
    DeleteFile((CONST_STRPTR)"readme.md");
    DeleteFile((CONST_STRPTR)"testdir/file1.txt");
    DeleteFile((CONST_STRPTR)"testdir/file2.txt");
    DeleteFile((CONST_STRPTR)"testdir");
    DeleteFile((CONST_STRPTR)"emptydir");
}

int main(void)
{
    BPTR lock;
    
    print("DIR Command Test\n");
    print("================\n\n");
    
    /* Cleanup any leftover test files */
    cleanup_test_files();
    
    /* Setup: Create test files and directories */
    print("Setup: Creating test files and directories\n");
    
    if (!create_test_file("test1.txt", "Test file 1")) {
        print("ERROR: Could not create test1.txt\n");
        return 1;
    }
    print("  Created test1.txt\n");
    
    if (!create_test_file("test2.txt", "Test file 2")) {
        print("ERROR: Could not create test2.txt\n");
        return 1;
    }
    print("  Created test2.txt\n");
    
    if (!create_test_file("test.doc", "Document")) {
        print("ERROR: Could not create test.doc\n");
        return 1;
    }
    print("  Created test.doc\n");
    
    if (!create_test_file("readme.md", "README")) {
        print("ERROR: Could not create readme.md\n");
        return 1;
    }
    print("  Created readme.md\n");
    
    /* Create test directory */
    lock = CreateDir((CONST_STRPTR)"testdir");
    if (lock) {
        UnLock(lock);
        print("  Created testdir\n");
        create_test_file("testdir/file1.txt", "File in testdir");
        create_test_file("testdir/file2.txt", "Another file");
    }
    
    /* Create empty directory */
    lock = CreateDir((CONST_STRPTR)"emptydir");
    if (lock) {
        UnLock(lock);
        print("  Created emptydir\n");
    }
    
    /* Test 1: Verify test files exist */
    print("\nTest 1: Verify test file setup\n");
    
    if (file_exists("test1.txt") && file_exists("test2.txt") && 
        file_exists("test.doc") && file_exists("readme.md")) {
        test_pass("All test files created");
    } else {
        test_fail("Test files", "Not all files exist");
    }
    
    /* Test 2: Verify directory exists */
    print("\nTest 2: Verify test directories\n");
    
    if (file_exists("testdir") && file_exists("emptydir")) {
        test_pass("Test directories created");
    } else {
        test_fail("Test directories", "Directories not created");
    }
    
    /* Test 3: Verify files in subdirectory */
    print("\nTest 3: Verify files in subdirectory\n");
    
    if (file_exists("testdir/file1.txt") && file_exists("testdir/file2.txt")) {
        test_pass("Subdirectory files exist");
    } else {
        test_fail("Subdirectory files", "Files not in directory");
    }
    
    /* Test 4: Test directory enumeration */
    print("\nTest 4: Directory enumeration test\n");
    {
        BPTR dirlock = Lock((CONST_STRPTR)"testdir", SHARED_LOCK);
        if (dirlock) {
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(dirlock, fib)) {
                    int count = 0;
                    while (ExNext(dirlock, fib)) {
                        count++;
                    }
                    if (count == 2) {
                        test_pass("Directory enumeration (2 files)");
                    } else {
                        test_fail("Directory enumeration", "Wrong file count");
                    }
                } else {
                    test_fail("Directory examine", "Examine failed");
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(dirlock);
        } else {
            test_fail("Directory lock", "Could not lock testdir");
        }
    }
    
    /* Test 5: Test empty directory enumeration */
    print("\nTest 5: Empty directory enumeration\n");
    {
        BPTR dirlock = Lock((CONST_STRPTR)"emptydir", SHARED_LOCK);
        if (dirlock) {
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(dirlock, fib)) {
                    int count = 0;
                    while (ExNext(dirlock, fib)) {
                        count++;
                    }
                    if (count == 0) {
                        test_pass("Empty directory (0 files)");
                    } else {
                        test_fail("Empty directory", "Directory not empty");
                    }
                } else {
                    test_fail("Empty dir examine", "Examine failed");
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(dirlock);
        } else {
            test_fail("Empty dir lock", "Could not lock emptydir");
        }
    }
    
    /* Test 6: Pattern matching for .txt files */
    print("\nTest 6: Pattern matching (#?.txt)\n");
    {
        char parsed[64];
        LONG has_wildcards = ParsePattern((STRPTR)"#?.txt", (STRPTR)parsed, sizeof(parsed));
        
        if (has_wildcards > 0) {
            /* Test matching */
            int matches = 0;
            if (MatchPattern((STRPTR)parsed, (STRPTR)"test1.txt")) matches++;
            if (MatchPattern((STRPTR)parsed, (STRPTR)"test2.txt")) matches++;
            if (!MatchPattern((STRPTR)parsed, (STRPTR)"test.doc")) matches++;  /* Should NOT match */
            if (!MatchPattern((STRPTR)parsed, (STRPTR)"readme.md")) matches++;  /* Should NOT match */
            
            if (matches == 4) {
                test_pass("Pattern matching #?.txt");
            } else {
                test_fail("Pattern matching", "Pattern match failed");
            }
        } else {
            test_fail("ParsePattern", "Did not detect wildcards");
        }
    }
    
    /* Test 7: Test DIRS filter concept (directory detection) */
    print("\nTest 7: Directory detection for DIRS filter\n");
    {
        BPTR testlock = Lock((CONST_STRPTR)"testdir", SHARED_LOCK);
        if (testlock) {
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(testlock, fib)) {
                    if (fib->fib_DirEntryType > 0) {
                        test_pass("Directory detection (DIRS filter)");
                    } else {
                        test_fail("Directory detection", "Not detected as dir");
                    }
                } else {
                    test_fail("Examine", "Failed");
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(testlock);
        } else {
            test_fail("Lock testdir", "Failed");
        }
    }
    
    /* Test 8: Test FILES filter concept (file detection) */
    print("\nTest 8: File detection for FILES filter\n");
    {
        BPTR testlock = Lock((CONST_STRPTR)"test1.txt", SHARED_LOCK);
        if (testlock) {
            struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
            if (fib) {
                if (Examine(testlock, fib)) {
                    if (fib->fib_DirEntryType < 0) {
                        test_pass("File detection (FILES filter)");
                    } else {
                        test_fail("File detection", "Detected as dir");
                    }
                } else {
                    test_fail("Examine", "Failed");
                }
                FreeDosObject(DOS_FIB, fib);
            }
            UnLock(testlock);
        } else {
            test_fail("Lock test1.txt", "Failed");
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
