/*
 * ASL ScreenModeRequester Test
 */

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/asl.h>
#include <graphics/modeid.h>
#include <clib/exec_protos.h>
#include <clib/asl_protos.h>
#include <stdio.h>

struct Library *AslBase = NULL;

int main(void)
{
    struct ScreenModeRequester *sm;
    struct TagItem tags[6];

    printf("ASL ScreenModeRequester Test\n");
    printf("============================\n\n");

    AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 36);
    if (!AslBase) {
        printf("FAIL: Cannot open asl.library\n");
        return 1;
    }

    printf("Opened asl.library v%d\n", AslBase->lib_Version);

    sm = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, NULL);
    if (!sm) {
        printf("FAIL: AllocAslRequest returned NULL\n");
        CloseLibrary(AslBase);
        return 1;
    }

    printf("PASS: Basic screen mode allocation works\n");
    printf("  DisplayID: 0x%08lx\n", sm->sm_DisplayID);
    printf("  Width: %lu\n", sm->sm_DisplayWidth);
    printf("  Height: %lu\n", sm->sm_DisplayHeight);
    printf("  Depth: %u\n", sm->sm_DisplayDepth);
    FreeAslRequest(sm);

    tags[0].ti_Tag = ASLSM_TitleText;
    tags[0].ti_Data = (ULONG)"Pick a mode";
    tags[1].ti_Tag = ASLSM_InitialDisplayID;
    tags[1].ti_Data = PAL_MONITOR_ID | HIRES_KEY;
    tags[2].ti_Tag = ASLSM_MinWidth;
    tags[2].ti_Data = 640;
    tags[3].ti_Tag = ASLSM_MinHeight;
    tags[3].ti_Data = 200;
    tags[4].ti_Tag = ASLSM_MinDepth;
    tags[4].ti_Data = 4;
    tags[5].ti_Tag = TAG_DONE;

    sm = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, tags);
    if (!sm) {
        printf("FAIL: Tagged AllocAslRequest returned NULL\n");
        CloseLibrary(AslBase);
        return 1;
    }

    printf("PASS: Tagged screen mode allocation works\n");
    printf("  DisplayID: 0x%08lx\n", sm->sm_DisplayID);
    printf("  Width: %lu\n", sm->sm_DisplayWidth);
    printf("  Height: %lu\n", sm->sm_DisplayHeight);
    printf("  Depth: %u\n", sm->sm_DisplayDepth);
    printf("  BitMapWidth: %lu\n", sm->sm_BitMapWidth);
    printf("  BitMapHeight: %lu\n", sm->sm_BitMapHeight);
    FreeAslRequest(sm);

    CloseLibrary(AslBase);
    printf("\nPASS\n");
    return 0;
}
