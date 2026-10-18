/*
 * Probe (Phase 243): the Workbench screen of the `rtg` reference profile
 * (Picasso96, UAE 800x600 8 bit) as programs see it - the screen and its
 * ViewPort, its ViewPortExtra, the View and the GfxBase display fields.
 * Final Writer's swshell.library centres its dialogs in the DisplayClip.
 *
 * rdd: profile rtg
 */
#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <graphics/videocontrol.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include "probe.h"

extern struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

int main(void)
{
    struct Screen *s;
    struct ViewPort *vp;
    struct View *v;
    struct Rectangle r;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    s = LockPubScreen(NULL);
    if (!s)
    {
        probe_s("LockPubScreen: NULL\n");
        return 0;
    }
    vp = &s->ViewPort;
    P_SECTION("screen");
    P_LONG("Width", s->Width);
    P_LONG("Height", s->Height);
    P_LONG("LeftEdge", s->LeftEdge);
    P_LONG("TopEdge", s->TopEdge);
    P_HEX("GetVPModeID", GetVPModeID(vp));
    P_SECTION("viewport");
    P_LONG("DWidth", vp->DWidth);
    P_LONG("DHeight", vp->DHeight);
    P_LONG("DxOffset", vp->DxOffset);
    P_LONG("DyOffset", vp->DyOffset);
    P_SECTION("viewportextra");
    {
        struct TagItem vt[2];
        struct ViewPortExtra *vpe = NULL;

        vt[0].ti_Tag = VTAG_VIEWPORTEXTRA_GET;
        vt[0].ti_Data = 0;
        vt[1].ti_Tag = TAG_END;
        if (vp->ColorMap && VideoControl(vp->ColorMap, vt) == 0)
            vpe = (struct ViewPortExtra *)vt[0].ti_Data;
        P_NULL("vpe", vpe);
        if (vpe)
        {
            P_LONG("DisplayClip.MinX", vpe->DisplayClip.MinX);
            P_LONG("DisplayClip.MinY", vpe->DisplayClip.MinY);
            P_LONG("DisplayClip.MaxX", vpe->DisplayClip.MaxX);
            P_LONG("DisplayClip.MaxY", vpe->DisplayClip.MaxY);
        }
    }
    P_SECTION("view");
    v = GfxBase->ActiView;
    P_NULL("ActiView", v);
    if (v)
    {
        P_LONG("View.DxOffset", v->DxOffset);
        P_LONG("View.DyOffset", v->DyOffset);
    }
    P_SECTION("intuition");
    P_LONG("ViewLord.DxOffset", IntuitionBase->ViewLord.DxOffset);
    P_LONG("ViewLord.DyOffset", IntuitionBase->ViewLord.DyOffset);
    P_SECTION("gfxbase");
    P_LONG("NormalDisplayRows", GfxBase->NormalDisplayRows);
    P_LONG("NormalDisplayColumns", GfxBase->NormalDisplayColumns);
    P_LONG("MaxDisplayRow", GfxBase->MaxDisplayRow);
    P_LONG("MaxDisplayColumn", GfxBase->MaxDisplayColumn);
    P_LONG("NormalDPMX", GfxBase->NormalDPMX);
    P_LONG("NormalDPMY", GfxBase->NormalDPMY);
    P_SECTION("overscan");
    if (QueryOverscan(GetVPModeID(vp), &r, OSCAN_TEXT))
    {
        P_LONG("txt.MinX", r.MinX);
        P_LONG("txt.MinY", r.MinY);
        P_LONG("txt.MaxX", r.MaxX);
        P_LONG("txt.MaxY", r.MaxY);
    }
    else
        probe_s("QueryOverscan = 0\n");
    UnlockPubScreen(NULL, s);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
