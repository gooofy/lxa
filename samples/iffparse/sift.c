/*
 * sift.c - IFF file structure viewer
 *
 * Based on RKM Libraries sample, adapted for automated testing.
 * Tests iffparse.library functions: AllocIFF, FreeIFF, OpenIFF, CloseIFF,
 * ParseIFF, CurrentChunk, IDtoStr, InitIFFasDOS.
 *
 * Usage: Sift <ifffile>
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/iffparse.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/iffparse_protos.h>

#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/iffparse.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

struct Library *IFFParseBase = NULL;

/* Error message table for IFFERR codes */
static char *errormsgs[] = {
    "End of file (not an error).",
    "End of context (not an error).",
    "No lexical scope.",
    "Insufficient memory.",
    "Stream read error.",
    "Stream write error.",
    "Stream seek error.",
    "File is corrupt.",
    "IFF syntax error.",
    "Not an IFF file.",
    "Required call-back hook missing.",
    "Return to client."
};

/* Print current chunk info with indentation */
static void PrintTopChunk(struct IFFHandle *iff)
{
    struct ContextNode *top;
    short i;
    char idbuf[5];

    /* Get pointer to context node for current chunk */
    if (!(top = CurrentChunk(iff)))
        return;

    /* Print dots for indentation based on nesting depth */
    for (i = iff->iff_Depth; i--; )
        printf(". ");

    /* Print chunk ID and size */
    printf("%s %ld ", IDtoStr(top->cn_ID, idbuf), top->cn_Size);

    /* Print chunk type (for FORM/LIST/CAT) */
    puts(IDtoStr(top->cn_Type, idbuf));
}

int main(int argc, char **argv)
{
    struct IFFHandle *iff = NULL;
    LONG error;
    BPTR file;
    int result = 1;
    int chunk_count = 0;

    printf("Sift: IFF file structure viewer\n\n");

    if (argc < 2) {
        printf("Usage: Sift <ifffile>\n");
        return 1;
    }

    printf("Opening iffparse.library...\n");
    if (!(IFFParseBase = OpenLibrary("iffparse.library", 0))) {
        printf("ERROR: Can't open iffparse.library\n");
        goto bye;
    }
    printf("OK: iffparse.library opened\n");

    /* Allocate IFF handle */
    printf("\nAllocating IFF handle...\n");
    if (!(iff = AllocIFF())) {
        printf("ERROR: AllocIFF() failed\n");
        goto bye;
    }
    printf("OK: IFF handle allocated at %p\n", (void *)iff);

    /* Open the file */
    printf("\nOpening file: %s\n", argv[1]);
    file = Open(argv[1], MODE_OLDFILE);
    if (!file) {
        printf("ERROR: Can't open file '%s'\n", argv[1]);
        goto bye;
    }
    printf("OK: File opened\n");

    /* Set up IFF handle for DOS I/O */
    iff->iff_Stream = (ULONG)file;
    InitIFFasDOS(iff);
    printf("OK: InitIFFasDOS complete\n");

    /* Start the IFF transaction */
    printf("\nOpening IFF stream for reading...\n");
    error = OpenIFF(iff, IFFF_READ);
    if (error) {
        printf("ERROR: OpenIFF failed, error %ld\n", error);
        Close(file);
        iff->iff_Stream = 0;
        goto bye;
    }
    printf("OK: OpenIFF succeeded\n");

    /* Parse the IFF file structure */
    printf("\n=== IFF Structure ===\n");
    
    while (1) {
        /*
         * IFFPARSE_RAWSTEP gives us precision monitoring of parsing:
         * Return 0       = Entered new context (chunk)
         * IFFERR_EOC     = About to leave a context
         * IFFERR_EOF     = End of file
         * Other          = Error
         */
        error = ParseIFF(iff, IFFPARSE_RAWSTEP);

        /* Skip end-of-context events - we only care about entering chunks */
        if (error == IFFERR_EOC)
            continue;
        else if (error)
            break;

        /* New chunk entered - print it */
        PrintTopChunk(iff);
        chunk_count++;
    }

    printf("=== End of Structure ===\n\n");

    /* Check final status */
    if (error == IFFERR_EOF) {
        printf("OK: File scan complete, found %d chunks\n", chunk_count);
        result = 0;
    } else {
        printf("ERROR: File scan aborted, error %ld: %s\n",
               error, errormsgs[-error - 1]);
    }

bye:
    if (iff) {
        /* Close the IFF transaction */
        CloseIFF(iff);
        printf("OK: CloseIFF done\n");

        /* Close the DOS file if open */
        if (iff->iff_Stream)
            Close((BPTR)iff->iff_Stream);

        /* Free the IFF handle */
        FreeIFF(iff);
        printf("OK: FreeIFF done\n");
    }

    if (IFFParseBase)
        CloseLibrary(IFFParseBase);

    if (result == 0)
        printf("\nPASS: Sift tests completed successfully\n");
    else
        printf("\nFAIL: Sift tests failed\n");

    return result;
}
