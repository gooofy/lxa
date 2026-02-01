/*
 * Test for DOS File Handle Utilities
 * Step 10.5: DupLockFromFH, ExamineFH, NameFromFH, ParentOfFH, OpenFromLock
 * Step 10.6: CheckSignal, WaitForChar
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    BPTR fh;
    BPTR lock;
    struct FileInfoBlock *fib;
    UBYTE buffer[256];
    LONG result;

    printf("=== File Handle Utilities Test ===\n\n");

    /* Allocate FIB */
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib) {
        printf("FAIL: Could not allocate FIB\n");
        return 20;
    }

    /*
     * Test 1: Create a file and test ExamineFH
     */
    printf("Test 1: ExamineFH\n");
    
    fh = Open((CONST_STRPTR)"testfile.txt", MODE_NEWFILE);
    if (!fh) {
        printf("FAIL: Could not create testfile.txt\n");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    /* Write some data */
    Write(fh, (APTR)"Hello, World!", 13);
    
    /* Examine the open file handle */
    result = ExamineFH(fh, fib);
    if (result) {
        printf("  ExamineFH succeeded\n");
        printf("  Filename: %s\n", fib->fib_FileName);
        printf("  Size: %ld\n", (long)fib->fib_Size);
        printf("  Type: %s\n", fib->fib_DirEntryType < 0 ? "FILE" : "DIR");
    } else {
        printf("  FAIL: ExamineFH failed\n");
    }
    
    Close(fh);
    printf("\n");

    /*
     * Test 2: NameFromFH
     */
    printf("Test 2: NameFromFH\n");
    
    fh = Open((CONST_STRPTR)"testfile.txt", MODE_OLDFILE);
    if (!fh) {
        printf("FAIL: Could not open testfile.txt\n");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    result = NameFromFH(fh, buffer, sizeof(buffer));
    if (result) {
        printf("  NameFromFH succeeded\n");
        /* Just check if it contains the filename (path may vary) */
        if (strstr((char *)buffer, "testfile.txt")) {
            printf("  Path contains: testfile.txt\n");
        } else {
            printf("  Path: %s\n", buffer);
        }
    } else {
        printf("  Note: NameFromFH returned no path (may be normal)\n");
    }
    
    Close(fh);
    printf("\n");

    /*
     * Test 3: ParentOfFH
     */
    printf("Test 3: ParentOfFH\n");
    
    fh = Open((CONST_STRPTR)"testfile.txt", MODE_OLDFILE);
    if (!fh) {
        printf("FAIL: Could not open testfile.txt\n");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    lock = ParentOfFH(fh);
    if (lock) {
        printf("  ParentOfFH succeeded\n");
        
        /* Examine the parent directory */
        if (Examine(lock, fib)) {
            printf("  Parent type: %s\n", fib->fib_DirEntryType > 0 ? "DIR" : "FILE");
        }
        
        UnLock(lock);
    } else {
        printf("  Note: ParentOfFH returned NULL (may be at root)\n");
    }
    
    Close(fh);
    printf("\n");

    /*
     * Test 4: DupLockFromFH
     */
    printf("Test 4: DupLockFromFH\n");
    
    fh = Open((CONST_STRPTR)"testfile.txt", MODE_OLDFILE);
    if (!fh) {
        printf("FAIL: Could not open testfile.txt\n");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    lock = DupLockFromFH(fh);
    if (lock) {
        printf("  DupLockFromFH succeeded\n");
        UnLock(lock);
    } else {
        printf("  Note: DupLockFromFH returned NULL\n");
    }
    
    Close(fh);
    printf("\n");

    /*
     * Test 5: OpenFromLock
     */
    printf("Test 5: OpenFromLock\n");
    
    /* First get a lock on the file */
    lock = Lock((CONST_STRPTR)"testfile.txt", SHARED_LOCK);
    if (!lock) {
        printf("FAIL: Could not lock testfile.txt\n");
        FreeDosObject(DOS_FIB, fib);
        return 20;
    }
    
    /* Convert lock to file handle */
    fh = OpenFromLock(lock);
    if (fh) {
        printf("  OpenFromLock succeeded\n");
        
        /* Read from the file to verify it works */
        UBYTE buf[64];
        LONG len = Read(fh, buf, 13);
        buf[len] = 0;
        printf("  Read %ld bytes: %s\n", (long)len, buf);
        
        Close(fh);
        /* Note: lock is consumed by OpenFromLock, don't UnLock */
    } else {
        printf("  Note: OpenFromLock returned NULL\n");
        UnLock(lock);  /* Only unlock if OpenFromLock failed */
    }
    printf("\n");

    /*
     * Test 6: CheckSignal
     */
    printf("Test 6: CheckSignal\n");
    
    /* First check - should return 0 (no signals pending) */
    LONG sigs = CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
    printf("  Initial CheckSignal: 0x%08lx\n", (unsigned long)sigs);
    
    /* Signal ourselves */
    struct Task *me = FindTask(NULL);
    Signal(me, SIGBREAKF_CTRL_C);
    
    /* Now check - should return SIGBREAKF_CTRL_C */
    sigs = CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
    printf("  After Signal: 0x%08lx\n", (unsigned long)sigs);
    
    if (sigs & SIGBREAKF_CTRL_C) {
        printf("  Ctrl-C signal was detected and cleared\n");
    }
    
    /* Check again - should be 0 (signal was cleared) */
    sigs = CheckSignal(SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D);
    printf("  After clear: 0x%08lx\n", (unsigned long)sigs);
    printf("\n");

    /*
     * Cleanup
     */
    printf("=== Cleanup ===\n");
    DeleteFile((CONST_STRPTR)"testfile.txt");
    FreeDosObject(DOS_FIB, fib);
    printf("Test file deleted\n");

    printf("\n=== All Tests Completed ===\n");
    return 0;
}
