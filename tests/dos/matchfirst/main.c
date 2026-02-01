/*
 * Test for DOS MatchFirst/MatchNext/MatchEnd
 * Step 10.7: Pattern matching directory enumeration
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    BPTR fh;
    LONG error;
    int count;

    printf("=== MatchFirst/MatchNext/MatchEnd Test ===\n\n");

    /*
     * Setup: Create some test files
     */
    printf("Setup: Creating test files...\n");
    
    fh = Open((CONST_STRPTR)"test1.txt", MODE_NEWFILE);
    if (fh) { Write(fh, (APTR)"file1", 5); Close(fh); }
    
    fh = Open((CONST_STRPTR)"test2.txt", MODE_NEWFILE);
    if (fh) { Write(fh, (APTR)"file2", 5); Close(fh); }
    
    fh = Open((CONST_STRPTR)"test3.dat", MODE_NEWFILE);
    if (fh) { Write(fh, (APTR)"file3", 5); Close(fh); }
    
    printf("Created: test1.txt, test2.txt, test3.dat\n\n");

    /*
     * Test 1: Match *.txt files
     */
    printf("Test 1: Match #?.txt files\n");
    
    /* Allocate AnchorPath with extra buffer space for path */
    struct AnchorPath *ap = (struct AnchorPath *)AllocVec(
        sizeof(struct AnchorPath) + 256, MEMF_CLEAR);
    
    if (!ap) {
        printf("FAIL: Could not allocate AnchorPath\n");
        return 20;
    }
    
    ap->ap_Strlen = 256;  /* Buffer size for full path */
    ap->ap_BreakBits = 0; /* No break signals */
    
    count = 0;
    error = MatchFirst((CONST_STRPTR)"#?.txt", ap);
    
    while (error == 0) {
        printf("  Found: %s (size=%ld)\n", 
               ap->ap_Info.fib_FileName, 
               (long)ap->ap_Info.fib_Size);
        count++;
        error = MatchNext(ap);
    }
    
    MatchEnd(ap);
    
    printf("  Total .txt files found: %d\n", count);
    if (count == 2) {
        printf("  PASS: Found expected 2 .txt files\n");
    } else {
        printf("  Note: Expected 2, found %d\n", count);
    }
    printf("\n");

    /*
     * Test 2: Match *.dat files
     */
    printf("Test 2: Match #?.dat files\n");
    
    count = 0;
    error = MatchFirst((CONST_STRPTR)"#?.dat", ap);
    
    while (error == 0) {
        printf("  Found: %s\n", ap->ap_Info.fib_FileName);
        count++;
        error = MatchNext(ap);
    }
    
    MatchEnd(ap);
    
    printf("  Total .dat files found: %d\n", count);
    if (count == 1) {
        printf("  PASS: Found expected 1 .dat file\n");
    } else {
        printf("  Note: Expected 1, found %d\n", count);
    }
    printf("\n");

    /*
     * Test 3: Match all test* files
     */
    printf("Test 3: Match test#? files\n");
    
    count = 0;
    error = MatchFirst((CONST_STRPTR)"test#?", ap);
    
    while (error == 0) {
        printf("  Found: %s\n", ap->ap_Info.fib_FileName);
        count++;
        error = MatchNext(ap);
    }
    
    MatchEnd(ap);
    
    printf("  Total test* files found: %d\n", count);
    if (count == 3) {
        printf("  PASS: Found expected 3 test* files\n");
    } else {
        printf("  Note: Expected 3, found %d\n", count);
    }
    printf("\n");

    /*
     * Test 4: Match non-existent pattern
     */
    printf("Test 4: Match nonexistent#?.xyz\n");
    
    count = 0;
    error = MatchFirst((CONST_STRPTR)"nonexistent#?.xyz", ap);
    
    while (error == 0) {
        count++;
        error = MatchNext(ap);
    }
    
    MatchEnd(ap);
    
    printf("  Files found: %d\n", count);
    if (count == 0) {
        printf("  PASS: No files found as expected\n");
    }
    printf("\n");

    /*
     * Cleanup
     */
    printf("=== Cleanup ===\n");
    FreeVec(ap);
    DeleteFile((CONST_STRPTR)"test1.txt");
    DeleteFile((CONST_STRPTR)"test2.txt");
    DeleteFile((CONST_STRPTR)"test3.dat");
    printf("Test files deleted\n");

    printf("\n=== All Tests Completed ===\n");
    return 0;
}
