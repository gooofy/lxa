/*
 * fontreq.c - ASL Font Requester Sample
 *
 * Based on RKM Libraries, Chapter 16: ASL Library
 * Demonstrates the ASL font requester with full interactive usage.
 *
 * Key Functions Demonstrated:
 *   - AllocAslRequest() with ASL_FontRequest
 *   - AslRequest() - Display font requester
 *   - FreeAslRequest() - Free font requester
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

void main(int argc, char **argv)
{
    struct FontRequester *fr;

    printf("FontReq: Starting ASL Font Requester demo\n");

    AslBase = OpenLibrary("asl.library", 37L);
    if (AslBase != NULL)
    {
        printf("FontReq: Opened asl.library v%d\n", AslBase->lib_Version);

        fr = (struct FontRequester *)
            AllocAslRequestTags(ASL_FontRequest,
                ASL_FontName,   (ULONG)"topaz.font",
                ASL_FontHeight, 8L,
                ASL_MinHeight,  6L,
                ASL_MaxHeight,  24L,
                TAG_DONE);

        if (fr != NULL)
        {
            printf("FontReq: AllocAslRequest succeeded\n");

            if (AslRequest(fr, NULL))
            {
                printf("FontReq: User selected font\n");
                printf("FontReq: Name=%s\n", fr->fo_Attr.ta_Name);
                printf("FontReq: YSize=%d\n", fr->fo_Attr.ta_YSize);
                printf("FontReq: Style=0x%x\n", fr->fo_Attr.ta_Style);
                printf("FontReq: Flags=0x%x\n", fr->fo_Attr.ta_Flags);
            }
            else
            {
                printf("FontReq: User cancelled\n");
            }

            FreeAslRequest(fr);
            printf("FontReq: Requester freed\n");
        }
        else
        {
            printf("FontReq: AllocAslRequest failed\n");
        }

        CloseLibrary(AslBase);
    }
    else
    {
        printf("FontReq: Failed to open asl.library v37\n");
    }

    printf("FontReq: Done\n");
}
