/*
 * screenmodereq.c - ASL Screen Mode Requester Sample
 */

#include <exec/types.h>
#include <libraries/asl.h>
#include <graphics/modeid.h>

#include <clib/exec_protos.h>
#include <clib/asl_protos.h>

#include <stdio.h>

struct Library *AslBase = NULL;

void main(int argc, char **argv)
{
    struct ScreenModeRequester *sm;
    struct TagItem tags[] = {
        { ASLSM_TitleText, (ULONG)"Select Screen Mode" },
        { ASLSM_InitialDisplayID, PAL_MONITOR_ID | HIRES_KEY },
        { ASLSM_MinWidth, 320 },
        { ASLSM_MinHeight, 200 },
        { TAG_DONE, 0 }
    };

    printf("ScreenModeReq: Starting ASL ScreenMode requester demo\n");

    AslBase = OpenLibrary("asl.library", 37L);
    if (AslBase == NULL)
    {
        printf("ScreenModeReq: Failed to open asl.library v37\n");
        return;
    }

    printf("ScreenModeReq: Opened asl.library v%d\n", AslBase->lib_Version);

    sm = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, tags);
    if (sm == NULL)
    {
        printf("ScreenModeReq: AllocAslRequest failed\n");
        CloseLibrary(AslBase);
        return;
    }

    printf("ScreenModeReq: AllocAslRequest succeeded\n");

    if (AslRequest(sm, NULL))
    {
        printf("ScreenModeReq: User selected mode\n");
        printf("ScreenModeReq: DisplayID=0x%08lx\n", sm->sm_DisplayID);
        printf("ScreenModeReq: Width=%lu\n", sm->sm_DisplayWidth);
        printf("ScreenModeReq: Height=%lu\n", sm->sm_DisplayHeight);
        printf("ScreenModeReq: Depth=%u\n", sm->sm_DisplayDepth);
        printf("ScreenModeReq: AutoScroll=%ld\n", sm->sm_AutoScroll);
        printf("ScreenModeReq: BitMapWidth=%lu\n", sm->sm_BitMapWidth);
        printf("ScreenModeReq: BitMapHeight=%lu\n", sm->sm_BitMapHeight);
    }
    else
    {
        printf("ScreenModeReq: User cancelled\n");
    }

    FreeAslRequest(sm);
    printf("ScreenModeReq: Requester freed\n");

    CloseLibrary(AslBase);
    printf("ScreenModeReq: Done\n");
}
