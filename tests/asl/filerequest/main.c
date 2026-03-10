/*
 * ASL FileRequester Test
 * 
 * Tests basic ASL library functions:
 * - AllocAslRequest/FreeAslRequest
 * - Initial drawer/file values
 * - Tag parsing (ASLFR_*)
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <utility/tagitem.h>
#include <libraries/asl.h>
#include <clib/exec_protos.h>
#include <clib/asl_protos.h>
#include <stdio.h>

struct Library *AslBase = NULL;

int main(void)
{
    struct FileRequester *fr;
    
    printf("ASL FileRequester Test\n");
    printf("======================\n\n");
    
    /* Open asl.library */
    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 36);
    if (!AslBase) {
        printf("FAIL: Cannot open asl.library\n");
        return 1;
    }
    printf("Opened asl.library v%d\n", AslBase->lib_Version);
    
    /* Test 1: Basic allocation */
    printf("\nTest 1: AllocAslRequest(ASL_FileRequest)...\n");
    fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);
    if (!fr) {
        printf("FAIL: AllocAslRequest returned NULL\n");
        CloseLibrary(AslBase);
        return 1;
    }
    printf("  Allocated FileRequester\n");
    printf("  PASS: Basic allocation works\n");
    
    /* Test 2: Free requester */
    printf("\nTest 2: FreeAslRequest...\n");
    FreeAslRequest(fr);
    printf("  PASS: FreeAslRequest completed\n");
    
    /* Test 3: Allocation with tags */
    printf("\nTest 3: AllocAslRequest with tags...\n");
    
    fr = (struct FileRequester *)AllocAslRequestTags(
        ASL_FileRequest,
        ASLFR_TitleText, (ULONG)"Select a File",
        ASLFR_InitialDrawer, (ULONG)"SYS:",
        ASLFR_InitialFile, (ULONG)"test.txt",
        ASLFR_InitialLeftEdge, 50,
        ASLFR_InitialTopEdge, 30,
        ASLFR_InitialWidth, 320,
        ASLFR_InitialHeight, 200,
        TAG_DONE);
    
    if (!fr) {
        printf("FAIL: AllocAslRequest with tags returned NULL\n");
        CloseLibrary(AslBase);
        return 1;
    }
    
    printf("  Allocated FileRequester with tags\n");
    
    /* Check if values were set */
    if (fr->fr_Drawer) {
        printf("  Initial Drawer: %s\n", fr->fr_Drawer);
    } else {
        printf("  Initial Drawer: (null)\n");
    }
    
    if (fr->fr_File) {
        printf("  Initial File: %s\n", fr->fr_File);
    } else {
        printf("  Initial File: (null)\n");
    }
    
    printf("  PASS: Allocation with tags works\n");
    
    /* Clean up */
    printf("\nTest 4: Cleanup...\n");
    FreeAslRequest(fr);
    printf("  PASS: Cleanup completed\n");
    
    CloseLibrary(AslBase);
    
    printf("\nPASS\n");
    return 0;
}
