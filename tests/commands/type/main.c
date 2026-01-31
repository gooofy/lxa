/*
 * Integration Test: TYPE command
 *
 * Tests:
 *   - Text file output
 *   - Read file to verify content
 *   - Binary content handling
 *   - Non-existent file error
 *   - File size checking
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

/* Create a test file with binary data */
static BOOL create_binary_file(const char *name, const UBYTE *data, LONG len)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_NEWFILE);
    if (!fh) return FALSE;
    
    Write(fh, (CONST APTR)data, len);
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

/* Read file and compare with expected content */
static BOOL read_and_compare(const char *name, const char *expected)
{
    BPTR fh = Open((CONST_STRPTR)name, MODE_OLDFILE);
    if (!fh) return FALSE;
    
    char buffer[256];
    LONG bytes = Read(fh, buffer, 255);
    Close(fh);
    
    if (bytes < 0) return FALSE;
    buffer[bytes] = '\0';
    
    /* Compare strings */
    const char *a = buffer;
    const char *b = expected;
    while (*a && *b) {
        if (*a != *b) return FALSE;
        a++; b++;
    }
    return (*a == *b);
}

/* Get file size */
static LONG get_file_size(const char *name)
{
    BPTR lock = Lock((CONST_STRPTR)name, SHARED_LOCK);
    if (!lock) return -1;
    
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        UnLock(lock);
        return -1;
    }
    
    LONG size = -1;
    if (Examine(lock, fib)) {
        size = fib->fib_Size;
    }
    
    FreeDosObject(DOS_FIB, fib);
    UnLock(lock);
    return size;
}

/* Clean up test files */
static void cleanup_test_files(void)
{
    DeleteFile((CONST_STRPTR)"typetest1.txt");
    DeleteFile((CONST_STRPTR)"typetest2.txt");
    DeleteFile((CONST_STRPTR)"bintest.dat");
    DeleteFile((CONST_STRPTR)"multiline.txt");
}

int main(void)
{
    print("TYPE Command Test\n");
    print("=================\n\n");
    
    /* Cleanup any leftover test files */
    cleanup_test_files();
    
    /* Setup: Create test files */
    print("Setup: Creating test files\n");
    
    if (!create_test_file("typetest1.txt", "Hello, World!")) {
        print("ERROR: Could not create typetest1.txt\n");
        return 1;
    }
    print("  Created typetest1.txt\n");
    
    if (!create_test_file("typetest2.txt", "Line 1\nLine 2\nLine 3\n")) {
        print("ERROR: Could not create typetest2.txt\n");
        return 1;
    }
    print("  Created typetest2.txt\n");
    
    /* Binary file with various bytes */
    UBYTE bindata[] = { 0x00, 0x01, 0x02, 0xFF, 0xFE, 0x48, 0x65, 0x6C, 0x6C, 0x6F };
    if (!create_binary_file("bintest.dat", bindata, 10)) {
        print("ERROR: Could not create bintest.dat\n");
        return 1;
    }
    print("  Created bintest.dat\n");
    
    if (!create_test_file("multiline.txt", "First line\nSecond line\nThird line\nFourth line\nFifth line\n")) {
        print("ERROR: Could not create multiline.txt\n");
        return 1;
    }
    print("  Created multiline.txt\n");
    
    /* Test 1: Read simple text file */
    print("\nTest 1: Read simple text file\n");
    
    if (read_and_compare("typetest1.txt", "Hello, World!")) {
        test_pass("Read simple text file");
    } else {
        test_fail("Read simple text", "Content mismatch");
    }
    
    /* Test 2: Verify file size */
    print("\nTest 2: Verify file size\n");
    
    LONG size = get_file_size("typetest1.txt");
    if (size == 13) {  /* "Hello, World!" is 13 characters */
        test_pass("File size correct (13 bytes)");
    } else {
        test_fail("File size", "Wrong size");
        print("    Expected: 13, Got: ");
        print_num(size);
        print("\n");
    }
    
    /* Test 3: Read multiline file */
    print("\nTest 3: Read multiline file\n");
    
    if (file_exists("typetest2.txt")) {
        BPTR fh = Open((CONST_STRPTR)"typetest2.txt", MODE_OLDFILE);
        if (fh) {
            char buffer[256];
            LONG bytes = Read(fh, buffer, 255);
            Close(fh);
            
            if (bytes > 0) {
                buffer[bytes] = '\0';
                /* Count newlines */
                int newlines = 0;
                for (int i = 0; i < bytes; i++) {
                    if (buffer[i] == '\n') newlines++;
                }
                if (newlines == 3) {
                    test_pass("Multiline file (3 lines)");
                } else {
                    test_fail("Multiline file", "Wrong line count");
                }
            } else {
                test_fail("Multiline file", "Read failed");
            }
        } else {
            test_fail("Multiline file", "Open failed");
        }
    } else {
        test_fail("Multiline file", "File doesn't exist");
    }
    
    /* Test 4: Binary file handling */
    print("\nTest 4: Binary file handling\n");
    
    LONG binsize = get_file_size("bintest.dat");
    if (binsize == 10) {
        BPTR fh = Open((CONST_STRPTR)"bintest.dat", MODE_OLDFILE);
        if (fh) {
            UBYTE buffer[16];
            LONG bytes = Read(fh, buffer, 16);
            Close(fh);
            
            if (bytes == 10 && buffer[0] == 0x00 && buffer[3] == 0xFF) {
                test_pass("Binary file read correct");
            } else {
                test_fail("Binary file", "Content mismatch");
            }
        } else {
            test_fail("Binary file", "Open failed");
        }
    } else {
        test_fail("Binary file", "Wrong size");
    }
    
    /* Test 5: Open non-existent file */
    print("\nTest 5: Open non-existent file (error case)\n");
    
    BPTR fh = Open((CONST_STRPTR)"nonexistent_xyz.txt", MODE_OLDFILE);
    if (!fh) {
        LONG err = IoErr();
        if (err == ERROR_OBJECT_NOT_FOUND) {
            test_pass("Non-existent file error");
        } else {
            test_fail("Non-existent file", "Wrong error code");
        }
    } else {
        Close(fh);
        test_fail("Non-existent file", "Open succeeded");
    }
    
    /* Test 6: File with many lines */
    print("\nTest 6: Count lines in file\n");
    {
        BPTR fh = Open((CONST_STRPTR)"multiline.txt", MODE_OLDFILE);
        if (fh) {
            char buffer[256];
            LONG bytes = Read(fh, buffer, 255);
            Close(fh);
            
            if (bytes > 0) {
                buffer[bytes] = '\0';
                int lines = 0;
                for (int i = 0; i < bytes; i++) {
                    if (buffer[i] == '\n') lines++;
                }
                if (lines == 5) {
                    test_pass("Line count correct (5 lines)");
                } else {
                    test_fail("Line count", "Wrong count");
                }
            } else {
                test_fail("Line count", "Read failed");
            }
        } else {
            test_fail("Line count", "Open failed");
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
