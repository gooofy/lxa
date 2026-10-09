/*
 * Probe (Phase 237): Intuition's View (ViewAddress()) - its ViewPort list
 * holds the screens' ViewPorts, front screen first.  Fish EOMS copies
 * ViewAddress()->ViewPort->ColorMap into its own ViewPort.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/view.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include "idump.h"

struct Library *IntuitionBase;

static struct Screen *g_wb, *g_custom;

static const char *who(struct ViewPort *vp)
{
    if (!vp)
        return "NULL";
    if (g_wb && vp == &g_wb->ViewPort)
        return "Workbench";
    if (g_custom && vp == &g_custom->ViewPort)
        return "custom";
    return "other";
}

static void show(const char *label)
{
    struct View *v = ViewAddress();
    struct ViewPort *vp = v ? v->ViewPort : NULL;
    int n = 0;

    probe_s(label);
    probe_ch('\n');
    P_NULL("  ViewAddress()", v);
    if (!v)
        return;
    for (; vp && n < 4; vp = vp->Next, n++) {
        probe_s("  ViewPort ");
        probe_dec(n);
        probe_s(" = ");
        probe_s(who(vp));
        probe_s(vp->ColorMap ? ", ColorMap set" : ", no ColorMap");
        probe_ch('\n');
    }
    P_LONG("  ViewPorts", n);
}

int main(void)
{
    struct Screen *wb, *s;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    wb = g_wb = LockPubScreen(NULL);
    if (!wb)
        return 20;
    P_SECTION("ViewAddress");
    show("Workbench only");
    s = g_custom = OpenScreenTags(NULL, SA_Width, 320, SA_Height, 200, SA_Depth, 2,
                       SA_Title, (ULONG)"Probe", SA_ShowTitle, TRUE, TAG_END);
    if (s) {
        show("custom screen in front");
        ScreenToBack(s);
        show("custom screen behind (ScreenToBack)");
        ScreenToFront(s);
        show("custom screen in front again (ScreenToFront)");
        CloseScreen(s);
        g_custom = NULL;
    }
    show("closed again");
    UnlockPubScreen(NULL, wb);
    CloseLibrary(IntuitionBase);
    return 0;
}
