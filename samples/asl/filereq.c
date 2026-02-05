/*
 * filereq.c - ASL File Requester Sample
 *
 * This is an RKM-style sample demonstrating the ASL file requester.
 * It creates a file requester, displays it briefly for testing,
 * and shows the returned path and file.
 *
 * Key Functions Demonstrated:
 *   - AllocAslRequest() - Create a file requester
 *   - FreeAslRequest() - Free a file requester
 *   - AslRequest() - Display the requester (interactive)
 *
 * Based on RKM ASL sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <libraries/asl.h>
#include <utility/tagitem.h>

#include <clib/exec_protos.h>
#include <clib/asl_protos.h>

#include <stdio.h>

struct Library *AslBase = NULL;

/* Tag list for the file requester */
struct TagItem frtags[] =
{
    {ASL_Hail,       (ULONG)"LXA File Requester Test"},
    {ASL_Height,     200},
    {ASL_Width,      320},
    {ASL_LeftEdge,   0},
    {ASL_TopEdge,    0},
    {ASL_OKText,     (ULONG)"Open"},
    {ASL_CancelText, (ULONG)"Cancel"},
    {ASL_File,       (ULONG)"test.txt"},
    {ASL_Dir,        (ULONG)"SYS:"},
    {TAG_END,        0}
};

void main(void)
{
    struct FileRequester *fr;
    
    printf("FileReq: Starting ASL File Requester demo\n");
    
    AslBase = OpenLibrary("asl.library", 37L);
    if (AslBase != NULL)
    {
        printf("FileReq: Opened asl.library v%d\n", AslBase->lib_Version);
        
        /* Allocate the file requester */
        fr = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, frtags);
        if (fr != NULL)
        {
            printf("FileReq: AllocAslRequest() succeeded\n");
            printf("FileReq: Requester configured for dir='%s' file='%s'\n",
                   fr->fr_Drawer ? (char*)fr->fr_Drawer : "(null)",
                   fr->fr_File ? (char*)fr->fr_File : "(null)");
            
            printf("FileReq: Size: %dx%d at (%d,%d)\n",
                   fr->fr_Width, fr->fr_Height,
                   fr->fr_LeftEdge, fr->fr_TopEdge);
            
            /* For automated testing, we show info but don't wait for user */
            /* In real use, AslRequest() would be called here interactively */
            printf("FileReq: (Skipping interactive AslRequest for test)\n");
            
            /* Free the requester */
            FreeAslRequest(fr);
            printf("FileReq: FreeAslRequest() - requester freed\n");
        }
        else
        {
            printf("FileReq: AllocAslRequest() failed\n");
        }
        
        CloseLibrary(AslBase);
        printf("FileReq: Closed asl.library\n");
    }
    else
    {
        printf("FileReq: Failed to open asl.library v37\n");
    }
    
    printf("FileReq: Done\n");
}
