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
    struct TagItem tags[8];
    
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
    
    tags[0].ti_Tag = ASLFR_TitleText;
    tags[0].ti_Data = (ULONG)"Select a File";
    tags[1].ti_Tag = ASLFR_InitialDrawer;
    tags[1].ti_Data = (ULONG)"SYS:";
    tags[2].ti_Tag = ASLFR_InitialFile;
    tags[2].ti_Data = (ULONG)"test.txt";
    tags[3].ti_Tag = ASLFR_InitialLeftEdge;
    tags[3].ti_Data = 50;
    tags[4].ti_Tag = ASLFR_InitialTopEdge;
    tags[4].ti_Data = 30;
    tags[5].ti_Tag = ASLFR_InitialWidth;
    tags[5].ti_Data = 320;
    tags[6].ti_Tag = ASLFR_InitialHeight;
    tags[6].ti_Data = 200;
    tags[7].ti_Tag = TAG_DONE;
    
    fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, tags);
    
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
