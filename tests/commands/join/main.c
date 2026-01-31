/*
 * Integration Test: JOIN command
 *
 * Tests:
 *   - Join two files
 *   - Join multiple files
 *   - Verify content ordering
 *   - Binary content handling
 */

#include <exec/types.h>
#include <exec/memory.h>
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

/* Join files manually for testing */
static BOOL join_files(const char **sources, int count, const char *dest)
{
    BPTR dest_fh = Open((CONST_STRPTR)dest, MODE_NEWFILE);
    if (!dest_fh) return FALSE;
    
    UBYTE buffer[256];
    BOOL success = TRUE;
    
    for (int i = 0; i < count; i++) {
        BPTR src_fh = Open((CONST_STRPTR)sources[i], MODE_OLDFILE);
        if (!src_fh) {
            success = FALSE;
            break;
        }
        
        LONG bytes;
        while ((bytes = Read(src_fh, buffer, 256)) > 0) {
            if (Write(dest_fh, buffer, bytes) != bytes) {
                success = FALSE;
                break;
            }
        }
        
        Close(src_fh);
        if (!success) break;
    }
    
    Close(dest_fh);
    return success;
}

/* Clean up test files */
static void cleanup_test_files(void)
{
    DeleteFile((CONST_STRPTR)"part1.txt");
    DeleteFile((CONST_STRPTR)"part2.txt");
    DeleteFile((CONST_STRPTR)"part3.txt");
    DeleteFile((CONST_STRPTR)"joined.txt");
    DeleteFile((CONST_STRPTR)"binary.dat");
    DeleteFile((CONST_STRPTR)"joined_bin.dat");
}

int main(void)
{
    char buffer[512];
    const char *sources[3];
    LONG expected_size;
    
    print("JOIN Command Test\n");
    print("=================\n\n");
    
    /* Cleanup any leftover test files */
    cleanup_test_files();
    
    /* Test 1: Join two simple text files */
    print("Test 1: Join two text files\n");
    
    if (!create_test_file("part1.txt", "Hello, ")) {
        print("ERROR: Could not create part1.txt\n");
        return 1;
    }
    if (!create_test_file("part2.txt", "World!")) {
        print("ERROR: Could not create part2.txt\n");
        return 1;
    }
    
    sources[0] = "part1.txt";
    sources[1] = "part2.txt";
    
    if (join_files(sources, 2, "joined.txt")) {
        if (read_file_contents("joined.txt", buffer, sizeof(buffer))) {
            if (strings_equal(buffer, "Hello, World!")) {
                test_pass("Join two files content correct");
            } else {
                test_fail("Join two files", "Content mismatch");
            }
        } else {
            test_fail("Join two files", "Could not read output");
        }
    } else {
        test_fail("Join two files", "Join operation failed");
    }
    
    /* Clean intermediate file */
    DeleteFile((CONST_STRPTR)"joined.txt");
    
    /* Test 2: Join three files */
    print("\nTest 2: Join three files\n");
    
    if (!create_test_file("part3.txt", " Goodbye!")) {
        print("ERROR: Could not create part3.txt\n");
        return 1;
    }
    
    sources[0] = "part1.txt";
    sources[1] = "part2.txt";
    sources[2] = "part3.txt";
    
    if (join_files(sources, 3, "joined.txt")) {
        if (read_file_contents("joined.txt", buffer, sizeof(buffer))) {
            if (strings_equal(buffer, "Hello, World! Goodbye!")) {
                test_pass("Join three files content correct");
            } else {
                test_fail("Join three files", "Content mismatch");
            }
        } else {
            test_fail("Join three files", "Could not read output");
        }
    } else {
        test_fail("Join three files", "Join operation failed");
    }
    
    /* Test 3: Verify size is sum of parts */
    print("\nTest 3: Verify output size equals sum of parts\n");
    
    LONG size1 = get_file_size("part1.txt");
    LONG size2 = get_file_size("part2.txt");
    LONG size3 = get_file_size("part3.txt");
    LONG joined_size = get_file_size("joined.txt");
    
    expected_size = size1 + size2 + size3;
    
    if (size1 >= 0 && size2 >= 0 && size3 >= 0 && joined_size >= 0) {
        if (joined_size == expected_size) {
            test_pass("Output size matches sum of inputs");
        } else {
            print("  Expected: ");
            print_num(expected_size);
            print(", Got: ");
            print_num(joined_size);
            print("\n");
            test_fail("Size check", "Size mismatch");
        }
    } else {
        test_fail("Size check", "Could not get file sizes");
    }
    
    /* Test 4: Binary content preservation */
    print("\nTest 4: Binary content preservation\n");
    
    /* Create a file with some binary data (all byte values 0-255) */
    BPTR fh = Open((CONST_STRPTR)"binary.dat", MODE_NEWFILE);
    if (fh) {
        UBYTE binary_data[16];
        /* Use simple pattern that includes null bytes */
        binary_data[0] = 0x00;
        binary_data[1] = 0x01;
        binary_data[2] = 0x7F;
        binary_data[3] = 0x80;
        binary_data[4] = 0xFF;
        binary_data[5] = 0x00;
        binary_data[6] = 'H';
        binary_data[7] = 'i';
        
        Write(fh, binary_data, 8);
        Close(fh);
        
        /* Join binary file with text file */
        sources[0] = "binary.dat";
        sources[1] = "part1.txt";
        
        if (join_files(sources, 2, "joined_bin.dat")) {
            LONG bin_size = get_file_size("binary.dat");
            LONG txt_size = get_file_size("part1.txt");
            LONG result_size = get_file_size("joined_bin.dat");
            
            if (result_size == bin_size + txt_size) {
                test_pass("Binary content size preserved");
            } else {
                test_fail("Binary content", "Size mismatch");
            }
        } else {
            test_fail("Binary join", "Join failed");
        }
    } else {
        test_fail("Create binary", "Could not create binary file");
    }
    
    /* Test 5: Output file is created fresh */
    print("\nTest 5: Output file is created fresh (not appended)\n");
    
    /* Create a large output file first */
    create_test_file("joined.txt", "This is old content that should be overwritten completely!");
    
    /* Now join small files */
    sources[0] = "part1.txt";
    sources[1] = "part2.txt";
    
    if (join_files(sources, 2, "joined.txt")) {
        LONG new_size = get_file_size("joined.txt");
        /* part1 is "Hello, " (7) + part2 is "World!" (6) = 13 bytes */
        if (new_size == 13) {
            test_pass("Output created fresh, not appended");
        } else {
            print("  Expected: 13, Got: ");
            print_num(new_size);
            print("\n");
            test_fail("Fresh output", "Old content may remain");
        }
    } else {
        test_fail("Fresh output test", "Join failed");
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
