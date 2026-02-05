/*
 * fontreq.c - ASL Font Requester Sample
 *
 * This is an RKM-style sample demonstrating the ASL font requester.
 * It creates a font requester structure and tests the allocation.
 *
 * Key Functions Demonstrated:
 *   - AllocAslRequest() with ASL_FontRequest
 *   - FreeAslRequest() - Free a font requester
 *
 * Note: The interactive AslRequest() for fonts is not yet fully
 * implemented in lxa, so we test the allocation/deallocation only.
 *
 * Based on RKM ASL sample code.
 * Copyright (c) 1992 Commodore-Amiga, Inc.
 */

#include <exec/types.h>
#include <libraries/asl.h>
#include <utility/tagitem.h>

#include <clib/exec_protos.h>
#include <clib/asl_protos.h>

#include <stdio.h>

struct Library *AslBase = NULL;

/* Tag list for the font requester */
struct TagItem fotags[] =
{
    {ASL_FontName,    (ULONG)"topaz.font"},
    {ASL_FontHeight,  8},
    {ASL_MinHeight,   6},
    {ASL_MaxHeight,   24},
    {TAG_END,         0}
};

void main(void)
{
    struct FontRequester *fo;
    
    printf("FontReq: Starting ASL Font Requester demo\n");
    
    AslBase = OpenLibrary("asl.library", 37L);
    if (AslBase != NULL)
    {
        printf("FontReq: Opened asl.library v%d\n", AslBase->lib_Version);
        
        /* Allocate the font requester */
        fo = (struct FontRequester *)AllocAslRequest(ASL_FontRequest, fotags);
        if (fo != NULL)
        {
            printf("FontReq: AllocAslRequest(ASL_FontRequest) succeeded\n");
            
            /* Access font attributes if they're set */
            printf("FontReq: Font requester configured\n");
            printf("FontReq: (Skipping interactive AslRequest - not yet implemented for fonts)\n");
            
            /* Free the requester */
            FreeAslRequest(fo);
            printf("FontReq: FreeAslRequest() - requester freed\n");
        }
        else
        {
            printf("FontReq: AllocAslRequest(ASL_FontRequest) failed\n");
        }
        
        CloseLibrary(AslBase);
        printf("FontReq: Closed asl.library\n");
    }
    else
    {
        printf("FontReq: Failed to open asl.library v37\n");
    }
    
    printf("FontReq: Done\n");
}
