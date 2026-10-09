/*
 * Probe (Phase 237): how Intuition calls a GTYP_CUSTOMGADGET.  The
 * gadget's MutualExclude field holds the hook Intuition calls (a0 = hook,
 * a2 = gadget, a1 = message); a BOOPSI gadget has its class there.  Fish
 * JukeBox builds its custom gadgets by hand with its own hook and no
 * BOOPSI object header.
 */
#include <exec/memory.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>
#include "idump.h"

struct Library *IntuitionBase;

static ULONG g_methods[32];
static int g_n;
static struct Hook g_hook;
static struct Gadget g_gad;

static ULONG hookfunc(register struct Hook *h __asm("a0"),
                      register struct Gadget *obj __asm("a2"),
                      register Msg msg __asm("a1"))
{
    if (h == &g_hook && obj == &g_gad && g_n < 32)
        g_methods[g_n++] = msg->MethodID;
    if (msg->MethodID == GM_HITTEST)
        return 0;
    return 0;
}

static const char *mname(ULONG m)
{
    switch (m) {
    case GM_HITTEST: return "GM_HITTEST";
    case GM_RENDER: return "GM_RENDER";
    case GM_GOACTIVE: return "GM_GOACTIVE";
    case GM_HANDLEINPUT: return "GM_HANDLEINPUT";
    case GM_GOINACTIVE: return "GM_GOINACTIVE";
    case GM_HELPTEST: return "GM_HELPTEST";
    case GM_LAYOUT: return "GM_LAYOUT";
    }
    return "other";
}

static void flush(const char *label)
{
    int i;
    probe_s(label);
    probe_s(":");
    for (i = 0; i < g_n; i++) {
        probe_s(" ");
        probe_s(mname(g_methods[i]));
    }
    probe_ch('\n');
    g_n = 0;
}

int main(void)
{
    struct Window *w;
    Object *b;

    IntuitionBase = OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 20;

    P_SECTION("BOOPSI gadget");
    b = NewObject(NULL, (UBYTE *)"buttongclass", GA_Left, 10, GA_Top, 20, GA_Width, 40,
                  GA_Height, 12, TAG_END);
    if (b) {
        P_NULL("MutualExclude", (APTR)((struct Gadget *)b)->MutualExclude);
        P_BOOL("MutualExclude == OCLASS(gadget)",
               ((struct Gadget *)b)->MutualExclude == (ULONG)OCLASS(b));
        DisposeObject(b);
    }

    P_SECTION("hand-made custom gadget");
    g_hook.h_Entry = (ULONG (*)())hookfunc;
    g_gad.LeftEdge = 20;
    g_gad.TopEdge = 20;
    g_gad.Width = 40;
    g_gad.Height = 12;
    g_gad.Flags = GFLG_GADGHNONE;
    g_gad.Activation = GACT_RELVERIFY;
    g_gad.GadgetType = GTYP_CUSTOMGADGET;
    g_gad.MutualExclude = (ULONG)&g_hook;
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                       WA_Title, (ULONG)"Hook", TAG_END);
    if (!w)
        return 0;
    flush("OpenWindow");
    AddGList(w, &g_gad, -1, 1, NULL);
    flush("AddGList");
    RefreshGList(&g_gad, w, NULL, 1);
    flush("RefreshGList");
    RemoveGList(w, &g_gad, 1);
    flush("RemoveGList");
    CloseWindow(w);
    CloseLibrary(IntuitionBase);
    return 0;
}
