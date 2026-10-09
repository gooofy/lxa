/*
 * Probe (Phase 238): the public fields of a ScreenModeRequester right after
 * AllocAslRequest() - defaults and the ASLSM_Initial* tags.
 */
#include <exec/types.h>
#include <libraries/asl.h>
#include <graphics/modeid.h>
#include <graphics/displayinfo.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/asl_protos.h>
#include "probe.h"

struct Library *AslBase;

static void dump(const char *name, struct ScreenModeRequester *sm)
{
    P_SECTION(name);
    if (!sm)
    {
        probe_s("(NULL)\n");
        return;
    }
    P_HEX("sm_DisplayID", sm->sm_DisplayID);
    P_LONG("sm_DisplayWidth", sm->sm_DisplayWidth);
    P_LONG("sm_DisplayHeight", sm->sm_DisplayHeight);
    P_LONG("sm_DisplayDepth", sm->sm_DisplayDepth);
    P_LONG("sm_OverscanType", sm->sm_OverscanType);
    P_LONG("sm_AutoScroll", sm->sm_AutoScroll);
    P_LONG("sm_BitMapWidth", sm->sm_BitMapWidth);
    P_LONG("sm_BitMapHeight", sm->sm_BitMapHeight);
    P_LONG("sm_LeftEdge", sm->sm_LeftEdge);
    P_LONG("sm_TopEdge", sm->sm_TopEdge);
    P_LONG("sm_Width", sm->sm_Width);
    P_LONG("sm_Height", sm->sm_Height);
    P_LONG("sm_InfoOpened", sm->sm_InfoOpened);
    P_LONG("sm_InfoLeftEdge", sm->sm_InfoLeftEdge);
    P_LONG("sm_InfoTopEdge", sm->sm_InfoTopEdge);
    P_LONG("sm_InfoWidth", sm->sm_InfoWidth);
    P_LONG("sm_InfoHeight", sm->sm_InfoHeight);
    P_HEX("sm_UserData", (ULONG)sm->sm_UserData);
}

int main(void)
{
    struct ScreenModeRequester *sm;

    AslBase = OpenLibrary((STRPTR)"asl.library", 38);
    if (!AslBase)
        return 20;

    sm = AllocAslRequestTags(ASL_ScreenModeRequest, TAG_END);
    dump("defaults", sm);
    FreeAslRequest(sm);

    sm = AllocAslRequestTags(ASL_ScreenModeRequest,
        ASLSM_InitialDisplayID, PAL_MONITOR_ID | HIRESLACE_KEY,
        ASLSM_InitialDisplayWidth, 704,
        ASLSM_InitialDisplayHeight, 566,
        ASLSM_InitialDisplayDepth, 4,
        ASLSM_InitialOverscanType, OSCAN_MAX,
        ASLSM_InitialAutoScroll, FALSE,
        ASLSM_InitialInfoOpened, TRUE,
        ASLSM_InitialInfoLeftEdge, 7,
        ASLSM_InitialInfoTopEdge, 9,
        ASLSM_InitialLeftEdge, 11,
        ASLSM_InitialTopEdge, 13,
        ASLSM_InitialWidth, 400,
        ASLSM_InitialHeight, 150,
        ASLSM_UserData, 0x12345678,
        TAG_END);
    dump("initial tags", sm);
    FreeAslRequest(sm);

    /* an unknown display ID and depth 0 are stored as given */
    sm = AllocAslRequestTags(ASL_ScreenModeRequest,
        ASLSM_InitialDisplayID, 0x00fa1000,
        ASLSM_InitialDisplayDepth, 0,
        ASLSM_InitialWidth, 10,
        ASLSM_InitialHeight, 10,
        TAG_END);
    dump("odd values", sm);
    FreeAslRequest(sm);

    /* the DEFAULT monitor's LORES */
    sm = AllocAslRequestTags(ASL_ScreenModeRequest,
        ASLSM_InitialDisplayID, LORES_KEY,
        TAG_END);
    dump("LORES_KEY", sm);
    FreeAslRequest(sm);

    CloseLibrary(AslBase);
    return 0;
}
