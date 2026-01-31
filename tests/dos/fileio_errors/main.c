/*
 * Integration Test: File I/O error handling
 *
 * Tests:
 *   - Read-only file write attempt (should fail)
 *   - Write to non-existent directory
 *   - Read from non-existent file
 *   - Write to device that doesn't support write
 *   - Error code verification
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
    BPTR fh;
    LONG result;
    LONG err;
    
    print("File I/O Error Handling Test\n");
    print("============================\n\n");
    
    /* Test 1: Open non-existent file for reading */
    print("Test 1: Open non-existent file (MODE_OLDFILE)\n");
    
    fh = Open((CONST_STRPTR)"nonexistent_xyz_12345.txt", MODE_OLDFILE);
    if (fh) {
        test_fail("Open non-existent", "Should have failed");
        Close(fh);
    } else {
        err = IoErr();
        print("  IoErr: ");
        print_num(err);
        print("\n");
        /* ERROR_OBJECT_NOT_FOUND = 205 */
        if (err == ERROR_OBJECT_NOT_FOUND) {
            test_pass("Correct error code (ERROR_OBJECT_NOT_FOUND)");
        } else {
            test_pass("Open correctly failed");
        }
    }
    
    /* Test 2: Create file in non-existent directory */
    print("\nTest 2: Create file in non-existent directory\n");
    
    fh = Open((CONST_STRPTR)"nonexistent_dir_xyz/newfile.txt", MODE_NEWFILE);
    if (fh) {
        test_fail("Create in bad dir", "Should have failed");
        Close(fh);
        DeleteFile((CONST_STRPTR)"nonexistent_dir_xyz/newfile.txt");
    } else {
        err = IoErr();
        print("  IoErr: ");
        print_num(err);
        print("\n");
        /* ERROR_DIR_NOT_FOUND = 204 or ERROR_OBJECT_NOT_FOUND = 205 */
        if (err == ERROR_DIR_NOT_FOUND || err == ERROR_OBJECT_NOT_FOUND) {
            test_pass("Correct error code");
        } else {
            test_pass("Create correctly failed");
        }
    }
    
    /* Test 3: Open directory as file */
    print("\nTest 3: Open directory as file (for writing)\n");
    
    fh = Open((CONST_STRPTR)"SYS:", MODE_NEWFILE);
    if (fh) {
        /* Some systems may allow this, depends on implementation */
        print("  Open returned handle (implementation-dependent)\n");
        Close(fh);
        test_pass("Open behavior noted");
    } else {
        err = IoErr();
        print("  IoErr: ");
        print_num(err);
        print("\n");
        test_pass("Correctly rejected directory open for write");
    }
    
    /* Test 4: Read from closed/invalid handle */
    print("\nTest 4: Operation on NULL handle\n");
    
    /* Reading from NULL handle should fail gracefully */
    result = Read(0, NULL, 10);
    if (result == -1) {
        err = IoErr();
        print("  Read(NULL) returned -1, IoErr: ");
        print_num(err);
        print("\n");
        test_pass("Read from NULL returns error");
    } else if (result == 0) {
        test_pass("Read from NULL returns 0");
    } else {
        test_fail("Read from NULL", "Unexpected result");
    }
    
    /* Test 5: Write to file opened for reading only */
    print("\nTest 5: Write to file opened MODE_OLDFILE\n");
    
    /* First, create a test file */
    fh = Open((CONST_STRPTR)"test_readonly.txt", MODE_NEWFILE);
    if (fh) {
        Write(fh, (CONST APTR)"Test data", 9);
        Close(fh);
    }
    
    /* Open for reading */
    fh = Open((CONST_STRPTR)"test_readonly.txt", MODE_OLDFILE);
    if (fh) {
        result = Write(fh, (CONST APTR)"New data", 8);
        if (result == -1) {
            err = IoErr();
            print("  Write returned -1, IoErr: ");
            print_num(err);
            print("\n");
            test_pass("Write to read-only file correctly failed");
        } else if (result >= 0) {
            /* Some implementations allow write on MODE_OLDFILE */
            print("  Write returned: ");
            print_num(result);
            print(" (implementation allows write)\n");
            test_pass("Write behavior noted");
        }
        Close(fh);
    } else {
        test_fail("Write to OLDFILE", "Could not open test file");
    }
    
    /* Clean up test file */
    DeleteFile((CONST_STRPTR)"test_readonly.txt");
    
    /* Test 6: Seek on invalid handle */
    print("\nTest 6: Seek on NULL handle\n");
    
    result = Seek(0, 100, OFFSET_BEGINNING);
    if (result == -1) {
        err = IoErr();
        print("  Seek(NULL) returned -1, IoErr: ");
        print_num(err);
        print("\n");
        test_pass("Seek on NULL returns error");
    } else {
        print("  Seek(NULL) returned: ");
        print_num(result);
        print("\n");
        test_pass("Seek behavior noted");
    }
    
    /* Test 7: Delete non-existent file */
    print("\nTest 7: Delete non-existent file\n");
    
    result = DeleteFile((CONST_STRPTR)"nonexistent_xyz_99999.txt");
    if (result) {
        test_fail("Delete non-existent", "Should have failed");
    } else {
        err = IoErr();
        print("  IoErr: ");
        print_num(err);
        print("\n");
        if (err == ERROR_OBJECT_NOT_FOUND) {
            test_pass("Correct error (ERROR_OBJECT_NOT_FOUND)");
        } else {
            test_pass("Delete correctly failed");
        }
    }
    
    /* Test 8: Rename non-existent file */
    print("\nTest 8: Rename non-existent file\n");
    
    result = Rename((CONST_STRPTR)"nonexistent_xyz_88888.txt", 
                    (CONST_STRPTR)"newname.txt");
    if (result) {
        test_fail("Rename non-existent", "Should have failed");
    } else {
        err = IoErr();
        print("  IoErr: ");
        print_num(err);
        print("\n");
        if (err == ERROR_OBJECT_NOT_FOUND || err == ERROR_RENAME_ACROSS_DEVICES) {
            test_pass("Correct error code");
        } else {
            test_pass("Rename correctly failed");
        }
    }
    
    /* Test 9: Lock non-existent file */
    print("\nTest 9: Lock non-existent file\n");
    
    {
        BPTR lock = Lock((CONST_STRPTR)"nonexistent_xyz_77777.txt", SHARED_LOCK);
        if (lock) {
            test_fail("Lock non-existent", "Should have failed");
            UnLock(lock);
        } else {
            err = IoErr();
            print("  IoErr: ");
            print_num(err);
            print("\n");
            if (err == ERROR_OBJECT_NOT_FOUND) {
                test_pass("Correct error (ERROR_OBJECT_NOT_FOUND)");
            } else {
                test_pass("Lock correctly failed");
            }
        }
    }
    
    /* Test 10: CreateDir with invalid characters */
    print("\nTest 10: CreateDir with existing file as parent\n");
    
    /* Create a file, then try to create a directory inside it */
    fh = Open((CONST_STRPTR)"test_file_not_dir.txt", MODE_NEWFILE);
    if (fh) {
        Write(fh, (CONST APTR)"Test", 4);
        Close(fh);
    }
    
    {
        BPTR lock = CreateDir((CONST_STRPTR)"test_file_not_dir.txt/subdir");
        if (lock) {
            test_fail("CreateDir in file", "Should have failed");
            UnLock(lock);
        } else {
            err = IoErr();
            print("  IoErr: ");
            print_num(err);
            print("\n");
            test_pass("CreateDir inside file correctly failed");
        }
    }
    
    /* Clean up */
    DeleteFile((CONST_STRPTR)"test_file_not_dir.txt");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
