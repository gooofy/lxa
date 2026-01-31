/*
 * Integration Test: File I/O operations
 *
 * Tests:
 *   - Open/Close with MODE_NEWFILE
 *   - Write data
 *   - Open/Close with MODE_OLDFILE
 *   - Read data
 *   - Seek operations
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

int main(void)
{
    BPTR fh;
    LONG result;
    UBYTE buffer[64];
    const char *test_data = "Hello, Amiga!";
    const char *test_file = "test_fileio_tmp.txt";
    
    print("File I/O Test\n");
    print("=============\n\n");
    
    /* Test 1: Create and write to file */
    print("Test 1: Create file (MODE_NEWFILE)\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_NEWFILE);
    if (!fh) {
        print("  ERROR: Open(MODE_NEWFILE) failed\n");
        return 1;
    }
    print("  File opened for writing: OK\n");
    
    result = Write(fh, (CONST APTR)test_data, 13);
    if (result != 13) {
        print("  ERROR: Write returned ");
        print_num(result);
        print("\n");
        Close(fh);
        return 1;
    }
    print("  Wrote 13 bytes: OK\n");
    
    Close(fh);
    print("  File closed: OK\n");
    
    /* Test 2: Open for reading */
    print("\nTest 2: Read file (MODE_OLDFILE)\n");
    
    fh = Open((CONST_STRPTR)test_file, MODE_OLDFILE);
    if (!fh) {
        print("  ERROR: Open(MODE_OLDFILE) failed\n");
        return 1;
    }
    print("  File opened for reading: OK\n");
    
    /* Clear buffer */
    for (int i = 0; i < 64; i++) buffer[i] = 0;
    
    result = Read(fh, buffer, 13);
    if (result != 13) {
        print("  ERROR: Read returned ");
        print_num(result);
        print("\n");
        Close(fh);
        return 1;
    }
    print("  Read 13 bytes: OK\n");
    
    /* Verify data */
    int match = 1;
    for (int i = 0; i < 13; i++) {
        if (buffer[i] != (UBYTE)test_data[i]) {
            match = 0;
            break;
        }
    }
    if (match) {
        print("  Data matches: OK\n");
    } else {
        print("  ERROR: Data mismatch\n");
        Close(fh);
        return 1;
    }
    
    /* Test 3: Seek operations */
    print("\nTest 3: Seek operations\n");
    
    /* Seek to beginning */
    result = Seek(fh, 0, OFFSET_BEGINNING);
    if (result == -1) {
        print("  ERROR: Seek(BEGINNING) failed\n");
        Close(fh);
        return 1;
    }
    print("  Seek to beginning: OK\n");
    
    /* Read first 5 bytes */
    result = Read(fh, buffer, 5);
    if (result != 5) {
        print("  ERROR: Read after seek failed\n");
        Close(fh);
        return 1;
    }
    buffer[5] = '\0';
    print("  Read '");
    print((char *)buffer);
    print("': OK\n");
    
    /* Seek to end */
    result = Seek(fh, 0, OFFSET_END);
    if (result == -1) {
        print("  ERROR: Seek(END) failed\n");
        Close(fh);
        return 1;
    }
    print("  Seek to end: OK\n");
    
    /* Read should return 0 at end */
    result = Read(fh, buffer, 5);
    if (result != 0) {
        print("  ERROR: Read at EOF returned ");
        print_num(result);
        print("\n");
        Close(fh);
        return 1;
    }
    print("  Read at EOF returns 0: OK\n");
    
    Close(fh);
    
    /* Test 4: Open non-existent file */
    print("\nTest 4: Open non-existent file (MODE_OLDFILE)\n");
    
    fh = Open((CONST_STRPTR)"nonexistent_xyz123.txt", MODE_OLDFILE);
    if (fh) {
        print("  ERROR: Open succeeded on non-existent file\n");
        Close(fh);
        return 1;
    }
    print("  Open correctly failed: OK\n");
    
    /* Test 5: Clean up - delete test file */
    print("\nTest 5: Delete test file\n");
    
    result = DeleteFile((CONST_STRPTR)test_file);
    if (!result) {
        print("  WARNING: DeleteFile failed (IoErr=");
        print_num(IoErr());
        print(")\n");
    } else {
        print("  File deleted: OK\n");
    }
    
    print("\nAll tests passed!\n");
    return 0;
}
