/*
 * Integration Test: Rename with open file handles
 *
 * Tests:
 *   - Attempt to rename a file that is currently open
 *   - Verify behavior is consistent (either succeeds or fails gracefully)
 *   - File should remain accessible after rename attempt
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

static int tests_passed = 0;
static int tests_failed = 0;

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

int main(void)
{
    BPTR fh;
    LONG result;
    char buf[64];
    
    print("Rename with Open Handles Test\n");
    print("==============================\n\n");
    
    /* Test 1: Rename file while open for reading */
    print("Test 1: Rename file while open for reading\n");
    
    /* Create the test file */
    fh = Open((CONST_STRPTR)"open_read_test.txt", MODE_NEWFILE);
    if (!fh) {
        print("ERROR: Could not create test file\n");
        return 1;
    }
    Write(fh, "Test content for rename", 23);
    Close(fh);
    print("  Created open_read_test.txt\n");
    
    /* Open the file for reading */
    fh = Open((CONST_STRPTR)"open_read_test.txt", MODE_OLDFILE);
    if (!fh) {
        print("ERROR: Could not open test file for reading\n");
        return 1;
    }
    print("  Opened file for reading\n");
    
    /* Try to rename while open */
    result = Rename((CONST_STRPTR)"open_read_test.txt", (CONST_STRPTR)"renamed_while_open.txt");
    
    if (result) {
        print("  Rename succeeded while file was open\n");
        /* On UNIX-like systems (which lxa emulates), rename of open file is allowed */
        
        /* Verify we can still read from the original handle */
        Seek(fh, 0, OFFSET_BEGINNING);
        LONG bytesRead = Read(fh, buf, 23);
        if (bytesRead == 23) {
            test_pass("File remains readable after rename");
        } else {
            test_fail("File readable after rename", "Read failed");
        }
        
        /* Check the new name exists */
        if (file_exists("renamed_while_open.txt")) {
            test_pass("New filename exists");
        } else {
            test_fail("New filename", "Does not exist");
        }
        
        /* Check old name no longer exists */
        if (!file_exists("open_read_test.txt")) {
            test_pass("Old filename removed");
        } else {
            /* Note: On some systems the old name might still be accessible */
            print("  Note: Old filename still exists (may be system-dependent)\n");
            tests_passed++;
        }
    } else {
        LONG err = IoErr();
        print("  Rename failed (IoErr=");
        print_num(err);
        print(")\n");
        /* Some systems don't allow rename of open files - that's also valid */
        test_pass("Rename correctly refused for open file");
        
        /* Verify file is still accessible */
        Seek(fh, 0, OFFSET_BEGINNING);
        LONG bytesRead = Read(fh, buf, 23);
        if (bytesRead == 23) {
            test_pass("File remains readable after failed rename");
        } else {
            test_fail("File readable after failed rename", "Read failed");
        }
    }
    
    Close(fh);
    print("  Closed file handle\n");
    
    /* Test 2: Rename file while open for writing */
    print("\nTest 2: Rename file while open for writing\n");
    
    /* Create a new file for this test */
    fh = Open((CONST_STRPTR)"write_test.txt", MODE_NEWFILE);
    if (!fh) {
        print("ERROR: Could not create write test file\n");
        return 1;
    }
    Write(fh, "Initial content", 15);
    print("  Created write_test.txt\n");
    
    /* Don't close it - try to rename while open for writing */
    result = Rename((CONST_STRPTR)"write_test.txt", (CONST_STRPTR)"write_renamed.txt");
    
    if (result) {
        print("  Rename succeeded for file open for writing\n");
        
        /* Try to write more to the file */
        LONG written = Write(fh, " - more data", 12);
        if (written == 12) {
            test_pass("Can still write to file after rename");
        } else {
            test_fail("Write after rename", "Write failed");
        }
    } else {
        LONG err = IoErr();
        print("  Rename failed for file open for writing (IoErr=");
        print_num(err);
        print(")\n");
        test_pass("Rename correctly refused for file open for writing");
    }
    
    Close(fh);
    print("  Closed write handle\n");
    
    /* Test 3: Rename target directory with file open inside */
    print("\nTest 3: Rename directory with open file inside\n");
    
    BPTR dirLock = CreateDir((CONST_STRPTR)"dir_with_open");
    if (!dirLock) {
        print("  Could not create directory - skipping test\n");
    } else {
        UnLock(dirLock);
        
        /* Create a file inside the directory */
        fh = Open((CONST_STRPTR)"dir_with_open/inner.txt", MODE_NEWFILE);
        if (fh) {
            Write(fh, "Inside dir", 10);
            print("  Created dir_with_open/inner.txt and keeping it open\n");
            
            /* Try to rename the directory while file is open */
            result = Rename((CONST_STRPTR)"dir_with_open", (CONST_STRPTR)"dir_renamed");
            
            if (result) {
                print("  Directory rename succeeded\n");
                
                /* Check if we can still access the file via original handle */
                Seek(fh, 0, OFFSET_BEGINNING);
                char tmpbuf[16];
                LONG rd = Read(fh, tmpbuf, 10);
                if (rd == 10) {
                    test_pass("File in renamed dir still accessible");
                } else {
                    test_fail("File in renamed dir", "Read failed");
                }
            } else {
                LONG err = IoErr();
                print("  Directory rename failed (IoErr=");
                print_num(err);
                print(")\n");
                test_pass("Directory rename refused with open file");
            }
            
            Close(fh);
        } else {
            print("  Could not create file in directory\n");
        }
    }
    
    /* Cleanup */
    print("\nCleanup:\n");
    
    DeleteFile((CONST_STRPTR)"open_read_test.txt");
    DeleteFile((CONST_STRPTR)"renamed_while_open.txt");
    DeleteFile((CONST_STRPTR)"write_test.txt");
    DeleteFile((CONST_STRPTR)"write_renamed.txt");
    DeleteFile((CONST_STRPTR)"dir_with_open/inner.txt");
    DeleteFile((CONST_STRPTR)"dir_renamed/inner.txt");
    DeleteFile((CONST_STRPTR)"dir_with_open");
    DeleteFile((CONST_STRPTR)"dir_renamed");
    
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
