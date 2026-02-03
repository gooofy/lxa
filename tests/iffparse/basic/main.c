/*
 * iffparse.library basic test
 *
 * Tests the core IFF parsing functionality:
 * - AllocIFF/FreeIFF
 * - InitIFFasDOS
 * - OpenIFF/CloseIFF
 * - PushChunk/PopChunk
 * - WriteChunkBytes/ReadChunkBytes
 * - ParseIFF
 * - CurrentChunk
 * - GoodID/GoodType/IDtoStr
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/iffparse.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/iffparse_protos.h>

#include <stdio.h>
#include <string.h>

struct Library *IFFParseBase;

/* Test data */
#define ID_TEST MAKE_ID('T','E','S','T')
#define ID_DATA MAKE_ID('D','A','T','A')
#define ID_NAME MAKE_ID('N','A','M','E')

static char testData[] = "Hello, IFF World!";
static char nameData[] = "Test IFF File";

int main(int argc, char **argv)
{
    struct IFFHandle *iff;
    BPTR file;
    LONG error;
    struct ContextNode *cn;
    UBYTE buf[64];
    UBYTE idStr[8];
    LONG bytesRead;
    
    printf("IFFParse library basic test\n");
    printf("===========================\n\n");
    
    /* Open iffparse.library */
    IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 0);
    if (!IFFParseBase)
    {
        printf("FAIL: Could not open iffparse.library\n");
        return 20;
    }
    printf("Opened iffparse.library\n");
    
    /* Test GoodID */
    printf("\n--- Testing GoodID/GoodType/IDtoStr ---\n");
    
    if (GoodID(ID_FORM))
        printf("GoodID(FORM): TRUE (correct)\n");
    else
        printf("GoodID(FORM): FALSE (FAIL)\n");
    
    if (GoodID(0x00000000))
        printf("GoodID(0x00000000): TRUE (FAIL)\n");
    else
        printf("GoodID(0x00000000): FALSE (correct - contains non-printable chars)\n");
    
    if (GoodType(ID_TEST))
        printf("GoodType(TEST): TRUE (correct)\n");
    else
        printf("GoodType(TEST): FALSE (FAIL)\n");
    
    if (GoodType(MAKE_ID(' ','A','B','C')))
        printf("GoodType(' ABC'): TRUE (FAIL - first char is space)\n");
    else
        printf("GoodType(' ABC'): FALSE (correct - first char is space)\n");
    
    IDtoStr(ID_FORM, idStr);
    printf("IDtoStr(ID_FORM): '%s'\n", (char *)idStr);
    
    /* Test AllocIFF */
    printf("\n--- Testing AllocIFF ---\n");
    iff = AllocIFF();
    if (!iff)
    {
        printf("FAIL: AllocIFF returned NULL\n");
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("AllocIFF: OK (handle allocated)\n");
    printf("  iff_Depth: %d\n", (int)iff->iff_Depth);
    printf("  iff_Flags: 0x%x\n", (unsigned int)iff->iff_Flags);
    
    /* Test write mode */
    printf("\n--- Testing IFF Write ---\n");
    
    /* Open output file */
    file = Open((CONST_STRPTR)"T:test.iff", MODE_NEWFILE);
    if (!file)
    {
        printf("FAIL: Could not create T:test.iff\n");
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("Created T:test.iff\n");
    
    /* Set up IFF handle for DOS I/O */
    iff->iff_Stream = (ULONG)file;
    InitIFFasDOS(iff);
    printf("InitIFFasDOS completed\n");
    
    /* Open for writing */
    error = OpenIFF(iff, IFFF_WRITE);
    if (error != 0)
    {
        printf("FAIL: OpenIFF(WRITE) returned %d\n", (int)error);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("OpenIFF(WRITE): success\n");
    
    /* Push FORM chunk */
    error = PushChunk(iff, ID_TEST, ID_FORM, IFFSIZE_UNKNOWN);
    if (error != 0)
    {
        printf("FAIL: PushChunk(FORM) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("PushChunk(FORM TEST): success, depth=%d\n", (int)iff->iff_Depth);
    
    /* Push NAME chunk */
    error = PushChunk(iff, 0, ID_NAME, IFFSIZE_UNKNOWN);
    if (error != 0)
    {
        printf("FAIL: PushChunk(NAME) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("PushChunk(NAME): success, depth=%d\n", (int)iff->iff_Depth);
    
    /* Write NAME data */
    error = WriteChunkBytes(iff, nameData, strlen(nameData));
    if (error < 0)
    {
        printf("FAIL: WriteChunkBytes(NAME) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("WriteChunkBytes(NAME): wrote %d bytes\n", (int)error);
    
    /* Pop NAME chunk */
    error = PopChunk(iff);
    if (error != 0)
    {
        printf("FAIL: PopChunk(NAME) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("PopChunk(NAME): success, depth=%d\n", (int)iff->iff_Depth);
    
    /* Push DATA chunk */
    error = PushChunk(iff, 0, ID_DATA, IFFSIZE_UNKNOWN);
    if (error != 0)
    {
        printf("FAIL: PushChunk(DATA) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("PushChunk(DATA): success, depth=%d\n", (int)iff->iff_Depth);
    
    /* Write DATA data */
    error = WriteChunkBytes(iff, testData, strlen(testData));
    if (error < 0)
    {
        printf("FAIL: WriteChunkBytes(DATA) returned %d\n", (int)error);
        CloseIFF(iff);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("WriteChunkBytes(DATA): wrote %d bytes\n", (int)error);
    
    /* Pop DATA chunk */
    error = PopChunk(iff);
    printf("PopChunk(DATA): result=%d, depth=%d\n", (int)error, (int)iff->iff_Depth);
    
    /* Pop FORM chunk */
    error = PopChunk(iff);
    printf("PopChunk(FORM): result=%d, depth=%d\n", (int)error, (int)iff->iff_Depth);
    
    /* Close IFF */
    CloseIFF(iff);
    printf("CloseIFF: done\n");
    
    /* Close file */
    Close(file);
    printf("Closed output file\n");
    
    /* Now test read mode */
    printf("\n--- Testing IFF Read ---\n");
    
    /* Reopen file for reading */
    file = Open((CONST_STRPTR)"T:test.iff", MODE_OLDFILE);
    if (!file)
    {
        printf("FAIL: Could not open T:test.iff for reading\n");
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("Opened T:test.iff for reading\n");
    
    /* Set up for reading */
    iff->iff_Stream = (ULONG)file;
    InitIFFasDOS(iff);
    
    /* Open for reading */
    error = OpenIFF(iff, IFFF_READ);
    if (error != 0)
    {
        printf("FAIL: OpenIFF(READ) returned %d\n", (int)error);
        Close(file);
        FreeIFF(iff);
        CloseLibrary(IFFParseBase);
        return 20;
    }
    printf("OpenIFF(READ): success\n");
    
    /* Get current chunk (should be FORM) */
    cn = CurrentChunk(iff);
    if (cn)
    {
        IDtoStr(cn->cn_ID, idStr);
        printf("CurrentChunk: ID='%s' Size=%d\n", (char *)idStr, (int)cn->cn_Size);
    }
    else
    {
        printf("CurrentChunk: NULL\n");
    }
    
    /* Parse to find chunks */
    printf("\nParsing IFF file...\n");
    while ((error = ParseIFF(iff, IFFPARSE_RAWSTEP)) == 0)
    {
        cn = CurrentChunk(iff);
        if (cn)
        {
            UBYTE typeStr[8];
            IDtoStr(cn->cn_ID, idStr);
            IDtoStr(cn->cn_Type, typeStr);
            printf("  Found chunk: ID='%s' Type='%s' Size=%d Scan=%d\n",
                   (char *)idStr, (char *)typeStr, (int)cn->cn_Size, (int)cn->cn_Scan);
            
            /* Read DATA chunk content */
            if (cn->cn_ID == ID_DATA)
            {
                memset(buf, 0, sizeof(buf));
                bytesRead = ReadChunkBytes(iff, buf, sizeof(buf) - 1);
                printf("    Read %d bytes: '%s'\n", (int)bytesRead, (char *)buf);
            }
            else if (cn->cn_ID == ID_NAME)
            {
                memset(buf, 0, sizeof(buf));
                bytesRead = ReadChunkBytes(iff, buf, sizeof(buf) - 1);
                printf("    Read %d bytes: '%s'\n", (int)bytesRead, (char *)buf);
            }
        }
    }
    
    if (error == IFFERR_EOF)
    {
        printf("ParseIFF: reached EOF (expected)\n");
    }
    else
    {
        printf("ParseIFF: ended with error %d\n", (int)error);
    }
    
    /* Close */
    CloseIFF(iff);
    Close(file);
    printf("Closed IFF file\n");
    
    /* Free IFF handle */
    FreeIFF(iff);
    printf("FreeIFF: done\n");
    
    /* Clean up */
    CloseLibrary(IFFParseBase);
    printf("\nTest completed successfully!\n");
    
    return 0;
}
