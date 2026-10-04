/*
 * Probe (Phase 222g): which IDCMP messages a freshly opened window gets
 * (IDCMP_REFRESHWINDOW for smart/simple refresh, IDCMP_NEWSIZE,
 * IDCMP_ACTIVEWINDOW, IDCMP_CHANGEWINDOW) and its layer's LAYERREFRESH
 * state, and whether drawing between BeginRefresh/EndRefresh with no
 * damage reaches the window.
 */
#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static void drain(const char *label, struct Window *w)
{
    struct IntuiMessage *m;

    probe_s(label);
    probe_s(":");
    while ((m = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL) {
        probe_ch(' ');
        probe_hex(m->Class, 8);
        ReplyMsg((struct Message *)m);
    }
    probe_ch('\n');
}

static void one(const char *name, ULONG refresh)
{
    struct Window *w;
    ULONG idcmp = IDCMP_REFRESHWINDOW | IDCMP_NEWSIZE | IDCMP_ACTIVEWINDOW |
                  IDCMP_CHANGEWINDOW;

    P_SECTION(name);
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                       WA_Title, (ULONG)"Refresh", WA_IDCMP, idcmp, WA_Activate, TRUE,
                       WA_DragBar, TRUE, WA_SizeGadget, TRUE, refresh, TRUE, TAG_END);
    if (!w)
        return;
    P_LONG("layerrefresh at open", (w->WLayer->Flags & LAYERREFRESH) ? 1 : 0);
    Delay(10);
    P_LONG("layerrefresh +0.2s", (w->WLayer->Flags & LAYERREFRESH) ? 1 : 0);
    drain("messages", w);

    /* drawing inside a refresh with no damage */
    SetAPen(w->RPort, 1);
    RectFill(w->RPort, 10, 15, 30, 25);
    BeginRefresh(w);
    SetAPen(w->RPort, 2);
    RectFill(w->RPort, 10, 15, 30, 25);
    EndRefresh(w, TRUE);
    P_LONG("pixel after refresh draw", ReadPixel(w->RPort, 20, 20));
    CloseWindow(w);
}

int main(void)
{
    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    one("smart refresh", WA_SmartRefresh);
    one("simple refresh", WA_SimpleRefresh);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
