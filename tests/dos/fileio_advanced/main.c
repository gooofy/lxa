/*
 * Integration Test: File I/O edge cases
 *
 * Tests:
 *   - MODE_READWRITE (append mode)
 *   - Seek beyond EOF
 *   - Large file write/read
 *   - Write position after open modes
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
    UBYTE buffer[128];
    const char *test_file = "fileio_advanced.txt";
    
    print("File I/O Edge Cases Test\n");
    print("========================\n\n");
    
    /* Test 1: Create initial file with MODE_NEWFILE */
    print("Test 1: Create initial file\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_NEWFILE);
    if (!fh) {
        test_fail("Create file", "Open failed");
        return 1;
    }
    
    result = Write(fh, (CONST APTR)"Hello", 5);
    if (result != 5) {
        test_fail("Create file", "Write failed");
        Close(fh);
        return 1;
    }
    Close(fh);
    test_pass("Create file with 5 bytes");
    
    /* Test 2: MODE_READWRITE - should open existing, position at start */
    print("\nTest 2: MODE_READWRITE (open existing)\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_READWRITE);
    if (!fh) {
        test_fail("MODE_READWRITE open", "Open failed");
        return 1;
    }
    
    /* Check position is at beginning */
    result = Seek(fh, 0, OFFSET_CURRENT);
    if (result == 0) {
        test_pass("MODE_READWRITE pos at start");
    } else {
        print("  Position: ");
        print_num(result);
        print("\n");
        test_fail("MODE_READWRITE pos at start", "Wrong position");
    }
    
    /* Read should get our data */
    result = Read(fh, buffer, 5);
    if (result == 5 && buffer[0] == 'H' && buffer[4] == 'o') {
        test_pass("MODE_READWRITE read existing data");
    } else {
        test_fail("MODE_READWRITE read", "Data mismatch");
    }
    
    /* Seek to end and write more */
    Seek(fh, 0, OFFSET_END);
    result = Write(fh, (CONST APTR)"World", 5);
    if (result == 5) {
        test_pass("MODE_READWRITE write at end");
    } else {
        test_fail("MODE_READWRITE write at end", "Write failed");
    }
    
    Close(fh);
    
    /* Test 3: Verify file has both parts */
    print("\nTest 3: Verify combined content\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        test_fail("Verify content", "Open failed");
        return 1;
    }
    
    result = Read(fh, buffer, 20);
    if (result == 10) {
        buffer[10] = '\0';
        print("  Content: '");
        print((char *)buffer);
        print("'\n");
        if (buffer[0] == 'H' && buffer[5] == 'W' && buffer[9] == 'd') {
            test_pass("Content is 'HelloWorld'");
        } else {
            test_fail("Content check", "Wrong content");
        }
    } else {
        print("  Read returned ");
        print_num(result);
        print(" bytes\n");
        test_fail("Content check", "Wrong size");
    }
    Close(fh);
    
    /* Test 4: Seek beyond EOF */
    print("\nTest 4: Seek beyond EOF\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        test_fail("Seek beyond EOF", "Open failed");
        return 1;
    }
    
    /* Try to seek way past EOF */
    result = Seek(fh, 1000, OFFSET_BEGINNING);
    print("  Seek(1000) returned: ");
    print_num(result);
    print("\n");
    
    /* Read should return 0 (at/past EOF) */
    result = Read(fh, buffer, 10);
    print("  Read returned: ");
    print_num(result);
    print(" bytes\n");
    
    if (result == 0) {
        test_pass("Read at EOF returns 0");
    } else if (result == -1) {
        test_pass("Read past EOF returns error");
    } else {
        test_fail("Read past EOF", "Unexpected result");
    }
    
    Close(fh);
    
    /* Test 5: Large write/read */
    print("\nTest 5: Large write/read (1KB)\n");
    
    fh = Open((CONST_STRPTR)"large_test.dat", MODE_NEWFILE);
    if (!fh) {
        test_fail("Large write", "Open failed");
        return 1;
    }
    
    /* Write 1KB of data (pattern: 0,1,2,...,255,0,1,...) */
    {
        UBYTE write_buf[256];
        LONG total_written = 0;
        int i, j;
        
        for (i = 0; i < 256; i++) {
            write_buf[i] = (UBYTE)i;
        }
        
        for (j = 0; j < 4; j++) {
            result = Write(fh, write_buf, 256);
            if (result != 256) {
                print("  Write failed at iteration ");
                print_num(j);
                print("\n");
                break;
            }
            total_written += result;
        }
        
        print("  Wrote ");
        print_num(total_written);
        print(" bytes\n");
        
        if (total_written == 1024) {
            test_pass("Large write (1024 bytes)");
        } else {
            test_fail("Large write", "Wrong size");
        }
    }
    Close(fh);
    
    /* Verify by reading back */
    fh = Open((CONST_STRPTR)"large_test.dat", MODE_OLDFILE);
    if (!fh) {
        test_fail("Large read", "Open failed");
    } else {
        LONG total_read = 0;
        BOOL data_ok = TRUE;
        int i;
        
        while (1) {
            result = Read(fh, buffer, 128);
            if (result <= 0) break;
            
            for (i = 0; i < result; i++) {
                UBYTE expected = (UBYTE)((total_read + i) & 0xFF);
                if (buffer[i] != expected) {
                    data_ok = FALSE;
                    print("  Mismatch at byte ");
                    print_num(total_read + i);
                    print(": got ");
                    print_num(buffer[i]);
                    print(" expected ");
                    print_num(expected);
                    print("\n");
                    break;
                }
            }
            
            total_read += result;
            if (!data_ok) break;
        }
        
        print("  Read ");
        print_num(total_read);
        print(" bytes\n");
        
        if (total_read == 1024 && data_ok) {
            test_pass("Large read with data verification");
        } else {
            test_fail("Large read", "Size or data mismatch");
        }
        Close(fh);
    }
    
    /* Test 6: MODE_NEWFILE truncates existing */
    print("\nTest 6: MODE_NEWFILE truncates existing\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_NEWFILE);
    if (!fh) {
        test_fail("Truncate test", "Open failed");
        return 1;
    }
    
    result = Write(fh, (CONST APTR)"AB", 2);
    Close(fh);
    
    /* Verify file is now only 2 bytes */
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (fh) {
        Seek(fh, 0, OFFSET_END);
        result = Seek(fh, 0, OFFSET_CURRENT);
        print("  File size now: ");
        print_num(result);
        print(" bytes\n");
        
        if (result == 2) {
            test_pass("MODE_NEWFILE truncates");
        } else {
            test_fail("MODE_NEWFILE truncates", "File not truncated");
        }
        Close(fh);
    } else {
        test_fail("Truncate verify", "Open failed");
    }
    
    /* Cleanup */
    print("\nCleanup\n");
    DeleteFile((CONST_STRPTR)test_file);
    DeleteFile((CONST_STRPTR)"large_test.dat");
    print("  Files deleted\n");
    
    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");
    
    return tests_failed > 0 ? 10 : 0;
}
