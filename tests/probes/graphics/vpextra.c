/*
 * Probe (Phase 238): the ViewPortExtra of an Intuition screen - what
 * VideoControl(VTAG_VIEWPORTEXTRA_GET) returns for the Workbench screen and
 * a custom screen (its DisplayClip), and QueryOverscan() of the screens'
 * modes.  reqtools' rtGetVScreenSize() reads DisplayClip to size its
 * requesters (ASM-One's screen-mode requester).
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/videocontrol.h>
#include <graphics/view.h>
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

static void rect(const char *label, struct Rectangle *r)
{
    probe_s(label);
    probe_s(" = ");
    probe_dec(r->MinX); probe_ch(','); probe_dec(r->MinY); probe_s(" - ");
    probe_dec(r->MaxX); probe_ch(','); probe_dec(r->MaxY);
    probe_ch('\n');
}

static void show(const char *label, struct Screen *s)
{
    struct ViewPortExtra *vpe = NULL;
    struct TagItem tags[2];
    struct Rectangle r;
    ULONG id = GetVPModeID(&s->ViewPort);
    LONG rc;

    P_SECTION(label);
    P_HEX("GetVPModeID", id);
    tags[0].ti_Tag = VTAG_VIEWPORTEXTRA_GET;
    tags[0].ti_Data = 0;
    tags[1].ti_Tag = TAG_END;
    rc = VideoControl(s->ViewPort.ColorMap, tags);
    P_LONG("VideoControl(VIEWPORTEXTRA_GET)", rc);
    P_HEX("returned tag", tags[0].ti_Tag);
    vpe = (struct ViewPortExtra *)tags[0].ti_Data;
    P_NULL("ViewPortExtra", vpe);
    if (vpe) {
        rect("DisplayClip", &vpe->DisplayClip);
        P_BOOL("ViewPort points back", vpe->ViewPort == &s->ViewPort);
    }
    if (QueryOverscan(id, &r, OSCAN_TEXT))
        rect("QueryOverscan(OSCAN_TEXT)", &r);
    if (QueryOverscan(id, &r, OSCAN_STANDARD))
        rect("QueryOverscan(OSCAN_STANDARD)", &r);
}

int main(void)
{
    struct Screen *wb, *s;

    IntuitionBase = OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        return 20;

    wb = LockPubScreen(NULL);
    if (wb) {
        show("Workbench screen", wb);
        UnlockPubScreen(NULL, wb);
    }
    s = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2,
                       SA_DisplayID, PAL_MONITOR_ID | HIRES_KEY, SA_Quiet, TRUE, SA_Behind, TRUE, TAG_END);
    if (s) {
        show("custom PAL HIRES screen", s);
        CloseScreen(s);
    }
    s = OpenScreenTags(NULL, SA_Width, 320, SA_Height, 200, SA_Depth, 2,
                       SA_DisplayID, PAL_MONITOR_ID | LORES_KEY, SA_Quiet, TRUE, SA_Behind, TRUE, TAG_END);
    if (s) {
        show("custom PAL LORES 320x200 screen", s);
        CloseScreen(s);
    }

    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
