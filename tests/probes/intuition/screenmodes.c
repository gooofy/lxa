/*
 * Probe (Phase 238): OpenScreenTagList() with display IDs that the machine
 * does not have.  AmigaOS 3.1 fails the open (SA_ErrorCode tells why) and
 * FindDisplayInfo()/ModeNotAvailable() report the mode as unavailable.
 * (ASM-One tries its default screen - an RTG mode ID - first and asks
 * for a screen mode only when that OpenScreenTagList() fails.)
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;
struct GfxBase *GfxBase;

static void try_mode(const char *label, ULONG id, ULONG w, ULONG h, ULONG depth)
{
    ULONG err = 0xdeadbeef;
    struct Screen *s;
    DisplayInfoHandle dh = FindDisplayInfo(id);
    struct DisplayInfo di;

    P_SECTION(label);
    P_NULL("FindDisplayInfo", dh);
    P_HEX("ModeNotAvailable", ModeNotAvailable(id));
    if (dh && GetDisplayInfoData(dh, (UBYTE *)&di, sizeof(di), DTAG_DISP, INVALID_ID) > 0)
        P_HEX("DisplayInfo.NotAvailable", di.NotAvailable);
    if (dh) {
        struct NameInfo ni;
        P_LONG("GetDisplayInfoData(DTAG_NAME)", GetDisplayInfoData(dh, (UBYTE *)&ni, sizeof(ni), DTAG_NAME, INVALID_ID));
    }
    s = OpenScreenTags(NULL, SA_Width, w, SA_Height, h, SA_Depth, depth, SA_DisplayID, id,
                       SA_Title, (ULONG)"Probe", SA_ErrorCode, (ULONG)&err, SA_Quiet, TRUE, TAG_END);
    P_NULL("OpenScreenTags", s);
    if (!s)
        P_HEX("SA_ErrorCode", err);
    else {
        P_LONG("Width", s->Width);
        P_LONG("Height", s->Height);
        CloseScreen(s);
    }
}

int main(void)
{
    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;

    P_SECTION("NextDisplayInfo order");
    {
        ULONG id = INVALID_ID;
        int n = 0;
        while ((id = NextDisplayInfo(id)) != INVALID_ID && n < 200) {
            probe_hex(id, 8);
            probe_ch(++n % 6 ? ' ' : '\n');
        }
        probe_ch('\n');
        P_LONG("modes", n);
    }

    try_mode("PAL HIRES", PAL_MONITOR_ID | HIRES_KEY, 640, 256, 2);
    try_mode("default HIRES", HIRES_KEY, 640, 256, 2);
    try_mode("PAL LORES", PAL_MONITOR_ID | LORES_KEY, 320, 256, 2);
    try_mode("NTSC HIRES (no ntsc.monitor)", NTSC_MONITOR_ID | HIRES_KEY, 640, 200, 2);
    try_mode("PAL SUPER72 key 0x00029024 (PAL monitor, no such mode)", PAL_MONITOR_ID | 0x9024, 640, 256, 2);
    try_mode("RTG mode ID 0x40120032 (UAE/P96 monitor)", 0x40120032UL, 800, 600, 2);
    try_mode("unknown monitor 0x00fa1000", 0x00fa1000UL, 640, 256, 2);
    try_mode("DBLPAL HIRES (no monitor driver)", 0x000a9024UL, 640, 512, 2);

    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
