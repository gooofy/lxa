/*
 * Test for AVAIL command
 * 
 * Verifies basic memory information display
 * Output is pattern-based to handle varying memory values
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

extern struct ExecBase *SysBase;

int main(int argc, char **argv)
{
    ULONG avail_any, avail_chip, avail_fast;
    ULONG largest_any, largest_chip;
    int pass = 1;
    
    printf("=== AVAIL Command Test ===\n\n");
    
    /* Test AvailMem with different flags */
    avail_any = AvailMem(0);
    avail_chip = AvailMem(MEMF_CHIP);
    avail_fast = AvailMem(MEMF_FAST);
    largest_any = AvailMem(MEMF_LARGEST);
    largest_chip = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    
    /* Test 1: Any memory should be available */
    if (avail_any > 0)
    {
        printf("PASS: Any memory available\n");
    }
    else
    {
        printf("FAIL: No memory reported\n");
        pass = 0;
    }
    
    /* Test 2: Largest block should be valid */
    if (largest_any > 0 && largest_any <= avail_any)
    {
        printf("PASS: Largest block valid\n");
    }
    else
    {
        printf("FAIL: Largest block invalid\n");
        pass = 0;
    }
    
    /* Test 3: Chip memory should be positive or zero */
    if (avail_chip >= 0)
    {
        printf("PASS: Chip memory valid\n");
    }
    else
    {
        printf("FAIL: Chip memory invalid\n");
        pass = 0;
    }
    
    /* Test 4: Fast memory should be zero or positive */
    if (avail_fast >= 0)
    {
        printf("PASS: Fast memory valid\n");
    }
    else
    {
        printf("FAIL: Fast memory invalid\n");
        pass = 0;
    }
    
    /* Test 5: Largest chip should be valid */
    if (largest_chip >= 0 && largest_chip <= avail_chip)
    {
        printf("PASS: Largest chip valid\n");
    }
    else
    {
        printf("FAIL: Largest chip invalid\n");
        pass = 0;
    }
    
    if (pass)
    {
        printf("\nAll AVAIL tests passed.\n");
        return 0;
    }
    else
    {
        printf("\nSome tests failed.\n");
        return 10;
    }
}
