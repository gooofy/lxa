/* Allocate.c - Adapted from RKM sample
 * Demonstrates Exec memory allocation functions
 * Shows AllocMem, AllocVec, AllocEntry usage
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/dos.h>

#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

/* Structure for AllocEntry example */
struct MyMemData {
    UBYTE *buffer1;
    UBYTE *buffer2;
    UBYTE *buffer3;
};

int main(int argc, char **argv)
{
    UBYTE *mem1 = NULL;
    APTR mem3 = NULL;
    struct MemList *memlist = NULL;
    ULONG i;

    printf("Allocate: Demonstrating Exec memory allocation\n\n");

    /* === AllocMem example === */
    printf("=== AllocMem Example ===\n");
    printf("Allocating 1024 bytes with MEMF_PUBLIC | MEMF_CLEAR...\n");
    
    mem1 = (UBYTE *)AllocMem(1024, MEMF_PUBLIC | MEMF_CLEAR);
    if (!mem1) {
        printf("AllocMem failed!\n");
        return RETURN_FAIL;
    }
    printf("  Allocated at: 0x%lx\n", (ULONG)mem1);
    
    /* Verify memory is cleared */
    printf("  Checking if memory is cleared...\n");
    for (i = 0; i < 1024; i++) {
        if (mem1[i] != 0) {
            printf("  ERROR: Memory not cleared at offset %lu!\n", i);
            break;
        }
    }
    if (i == 1024) {
        printf("  OK: All 1024 bytes are zero\n");
    }
    
    /* Write some data */
    printf("  Writing test pattern...\n");
    for (i = 0; i < 1024; i++) {
        mem1[i] = (UBYTE)(i & 0xFF);
    }
    printf("  Wrote pattern to memory\n");
    
    /* Display memory usage */
    printf("  Available memory (MEMF_PUBLIC): %lu bytes\n", 
           AvailMem(MEMF_PUBLIC));
    printf("  Available memory (MEMF_CHIP): %lu bytes\n", 
           AvailMem(MEMF_CHIP));
    printf("  Available memory (MEMF_FAST): %lu bytes\n", 
           AvailMem(MEMF_FAST));

    /* === AllocVec example === */
    printf("\n=== AllocVec Example ===\n");
    printf("Allocating 2048 bytes with MEMF_PUBLIC | MEMF_CLEAR...\n");
    
    mem3 = AllocVec(2048, MEMF_PUBLIC | MEMF_CLEAR);
    if (!mem3) {
        printf("AllocVec failed!\n");
        FreeMem(mem1, 1024);
        return RETURN_FAIL;
    }
    printf("  Allocated at: 0x%lx\n", (ULONG)mem3);
    printf("  (AllocVec stores size internally, so FreeVec doesn't need size)\n");

    /* === AllocEntry example === */
    printf("\n=== AllocEntry Example ===\n");
    printf("Allocating multiple memory blocks in one call...\n");
    
    struct MemList ml;
    
    ml.ml_NumEntries = 3;
    ml.ml_ME[0].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
    ml.ml_ME[0].me_Length = 512;
    ml.ml_ME[1].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
    ml.ml_ME[1].me_Length = 1024;
    ml.ml_ME[2].me_Reqs = MEMF_PUBLIC | MEMF_CLEAR;
    ml.ml_ME[2].me_Length = 256;
    
    memlist = (struct MemList *)AllocEntry((struct MemList *)&ml);
    if ((ULONG)memlist & 0x80000000) {
        printf("  AllocEntry failed!\n");
        printf("  Failed on entry: %lu\n", (ULONG)memlist & 0x7FFFFFFF);
    } else {
        printf("  AllocEntry succeeded!\n");
        printf("  Allocated %u blocks:\n", memlist->ml_NumEntries);
        for (i = 0; i < memlist->ml_NumEntries; i++) {
            printf("    Block %lu: %lu bytes at 0x%lx\n",
                   i + 1,
                   memlist->ml_ME[i].me_Length,
                   (ULONG)memlist->ml_ME[i].me_Addr);
        }
    }

    /* === TypeOfMem example === */
    printf("\n=== TypeOfMem Example ===\n");
    printf("Checking memory types...\n");
    
    ULONG memtype;
    
    memtype = TypeOfMem(mem1);
    printf("  mem1 (AllocMem): 0x%08lx ", memtype);
    if (memtype & MEMF_PUBLIC) printf("PUBLIC ");
    if (memtype & MEMF_CHIP) printf("CHIP ");
    if (memtype & MEMF_FAST) printf("FAST ");
    printf("\n");
    
    memtype = TypeOfMem(mem3);
    printf("  mem3 (AllocVec): 0x%08lx ", memtype);
    if (memtype & MEMF_PUBLIC) printf("PUBLIC ");
    if (memtype & MEMF_CHIP) printf("CHIP ");
    if (memtype & MEMF_FAST) printf("FAST ");
    printf("\n");

    /* === Cleanup === */
    printf("\n=== Cleanup ===\n");
    printf("Freeing all allocated memory...\n");
    
    if (mem1) {
        printf("  Freeing mem1 (AllocMem)...\n");
        FreeMem(mem1, 1024);
    }
    
    if (mem3) {
        printf("  Freeing mem3 (AllocVec)...\n");
        FreeVec(mem3);
    }
    
    if (memlist && !((ULONG)memlist & 0x80000000)) {
        printf("  Freeing memlist (FreeEntry)...\n");
        FreeEntry(memlist);
    }
    
    printf("\nAllocate: Memory allocation demonstration complete.\n");
    return RETURN_OK;
}
