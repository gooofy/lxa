/*
 * allocentry.c - Test AllocEntry and FreeEntry functions
 *
 * Based on RKM Exec sample, adapted for automated testing.
 * Tests: AllocEntry, FreeEntry, multi-block memory allocation.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCERROR 0x80000000

struct MemList *memlist;

/* Define structure for multiple memory entries */
struct MemBlocks
{
    struct MemList  mn_head;     /* one entry in the header */
    struct MemEntry mn_body[3];  /* additional entries */
} memblocks;

int main(void)
{
    printf("AllocEntry: Multiple block memory allocation test\n\n");

    /* Test 1: Basic multi-block allocation */
    printf("=== Test 1: Multi-block Allocation ===\n");
    
    memblocks.mn_head.ml_NumEntries = 4;  /* 4 entries total */

    /* First entry - small block */
    memblocks.mn_head.ml_ME[0].me_Reqs = MEMF_CLEAR;
    memblocks.mn_head.ml_ME[0].me_Length = 1000;

    /* Second entry - chip memory */
    memblocks.mn_body[0].me_Reqs = MEMF_CHIP | MEMF_CLEAR;
    memblocks.mn_body[0].me_Length = 2000;

    /* Third entry - public memory */
    memblocks.mn_body[1].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
    memblocks.mn_body[1].me_Length = 3000;

    /* Fourth entry - uncleared public */
    memblocks.mn_body[2].me_Reqs = MEMF_PUBLIC;
    memblocks.mn_body[2].me_Length = 4000;

    printf("Requesting 4 memory blocks:\n");
    printf("  Block 0: %ld bytes (MEMF_CLEAR)\n", memblocks.mn_head.ml_ME[0].me_Length);
    printf("  Block 1: %ld bytes (MEMF_CHIP|MEMF_CLEAR)\n", memblocks.mn_body[0].me_Length);
    printf("  Block 2: %ld bytes (MEMF_PUBLIC|MEMF_CLEAR)\n", memblocks.mn_body[1].me_Length);
    printf("  Block 3: %ld bytes (MEMF_PUBLIC)\n", memblocks.mn_body[2].me_Length);

    memlist = (struct MemList *)AllocEntry((struct MemList *)&memblocks);

    if ((ULONG)memlist & ALLOCERROR)
    {
        printf("ERROR: AllocEntry failed\n");
        return 1;
    }

    printf("OK: AllocEntry succeeded\n");
    printf("MemList at 0x%08lx with %d entries\n", (ULONG)memlist, memlist->ml_NumEntries);

    /* Verify all blocks were allocated */
    printf("\nAllocated blocks:\n");
    int i;
    for (i = 0; i < memlist->ml_NumEntries; i++)
    {
        printf("  Block %d: 0x%08lx, %ld bytes\n", i,
               (ULONG)memlist->ml_ME[i].me_Addr,
               memlist->ml_ME[i].me_Length);
        
        if (memlist->ml_ME[i].me_Addr == NULL)
        {
            printf("ERROR: Block %d has NULL address\n", i);
            FreeEntry(memlist);
            return 1;
        }
    }

    /* Test 2: Verify cleared memory */
    printf("\n=== Test 2: Verify MEMF_CLEAR ===\n");
    {
        UBYTE *ptr = (UBYTE *)memlist->ml_ME[0].me_Addr;
        int cleared = 1;
        int j;
        for (j = 0; j < 100; j++)  /* Check first 100 bytes */
        {
            if (ptr[j] != 0)
            {
                cleared = 0;
                break;
            }
        }
        if (cleared)
            printf("OK: Block 0 is properly cleared\n");
        else
            printf("WARNING: Block 0 not cleared at byte %d\n", j);
    }

    /* Test 3: FreeEntry */
    printf("\n=== Test 3: FreeEntry ===\n");
    FreeEntry(memlist);
    printf("OK: FreeEntry completed\n");

    /* Test 4: Single block allocation */
    printf("\n=== Test 4: Single Block Allocation ===\n");
    {
        struct MemList singleblock;
        struct MemList *single_result;

        singleblock.ml_NumEntries = 1;
        singleblock.ml_ME[0].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
        singleblock.ml_ME[0].me_Length = 512;

        printf("Requesting single 512-byte block\n");
        single_result = (struct MemList *)AllocEntry(&singleblock);

        if ((ULONG)single_result & ALLOCERROR)
        {
            printf("ERROR: Single block AllocEntry failed\n");
            return 1;
        }

        printf("OK: Single block allocated at 0x%08lx\n",
               (ULONG)single_result->ml_ME[0].me_Addr);

        FreeEntry(single_result);
        printf("OK: Single block freed\n");
    }

    printf("\nPASS: AllocEntry tests completed successfully\n");
    return 0;
}
