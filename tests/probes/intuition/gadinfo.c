/*
 * Probe (Phase 222g): the GadgetInfo a BOOPSI gadget receives - gi_Domain
 * for GM_LAYOUT, GM_RENDER, OM_SET (SetGadgetAttrsA) and DoGadgetMethodA
 * in a normal and a GIMMEZEROZERO window, which gadgets get GM_LAYOUT
 * (GA_RelWidth/GA_RelHeight, GA_RelRight/GA_RelBottom, GA_RelSpecial) and
 * when (AddGList, OpenWindow with WA_Gadgets, window resize), and the
 * gadget flags the GA_Rel* tags set; which gadgets SetWindowTitles() and
 * RefreshWindowFrame() redraw.
 */
#include <exec/memory.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>
#include "idump.h"

struct Library *IntuitionBase;

#define PROBE_METHOD 0x7777

struct logent {
    ULONG method;
    UWORD id;
    WORD l, t, w, h;
    LONG initial;
    BYTE has_gi;
};

static struct logent g_log[64];
static int g_nlog;

static void log_gi(ULONG method, Object *obj, struct GadgetInfo *gi, LONG initial)
{
    struct logent *e;

    if (g_nlog >= 64)
        return;
    e = &g_log[g_nlog++];
    e->method = method;
    e->id = ((struct Gadget *)obj)->GadgetID;
    e->initial = initial;
    e->has_gi = gi != NULL;
    if (gi) {
        e->l = gi->gi_Domain.Left;
        e->t = gi->gi_Domain.Top;
        e->w = gi->gi_Domain.Width;
        e->h = gi->gi_Domain.Height;
    }
}

static ULONG call_super(struct IClass *cl, Object *obj, Msg msg)
{
    typedef ULONG (*DispatchEntry)(register struct IClass *cl __asm("a0"),
                                   register Object *obj __asm("a2"),
                                   register Msg msg __asm("a1"));
    struct IClass *super = cl->cl_Super;

    return ((DispatchEntry)super->cl_Dispatcher.h_Entry)(super, obj, msg);
}

static ULONG dispatch(register struct IClass *cl __asm("a0"),
                      register Object *obj __asm("a2"),
                      register Msg msg __asm("a1"))
{
    switch (msg->MethodID) {
    case GM_LAYOUT:
        log_gi(GM_LAYOUT, obj, ((struct gpLayout *)msg)->gpl_GInfo,
               (LONG)((struct gpLayout *)msg)->gpl_Initial);
        break;
    case GM_RENDER:
        log_gi(GM_RENDER, obj, ((struct gpRender *)msg)->gpr_GInfo, -1);
        return 0;
    case OM_SET:
        log_gi(OM_SET, obj, ((struct opSet *)msg)->ops_GInfo, -1);
        break;
    case PROBE_METHOD:
        log_gi(PROBE_METHOD, obj, ((struct gpRender *)msg)->gpr_GInfo, -1);
        return 0;
    }
    return call_super(cl, obj, msg);
}

static const char *mname(ULONG m)
{
    switch (m) {
    case GM_LAYOUT: return "GM_LAYOUT";
    case GM_RENDER: return "GM_RENDER";
    case OM_SET: return "OM_SET";
    case PROBE_METHOD: return "DoGadgetMethod";
    }
    return "?";
}

static void flush_log(const char *label)
{
    int i;

    probe_s(label);
    probe_s(":\n");
    for (i = 0; i < g_nlog; i++) {
        struct logent *e = &g_log[i];
        probe_s("  ");
        probe_s(mname(e->method));
        ps_kv("id", e->id);
        if (e->has_gi)
            ps_box(e->l, e->t, e->w, e->h);
        else
            probe_s(" gi=NULL");
        if (e->initial >= 0)
            ps_kv("initial", e->initial);
        probe_ch('\n');
    }
    g_nlog = 0;
}

static struct IClass *g_cl;

static struct Gadget *make(struct Window *w, int kind, struct Gadget *prev)
{
    WORD bl = w ? w->BorderLeft : 4, bt = w ? w->BorderTop : 11;
    WORD br = w ? w->BorderRight : 18, bb = w ? w->BorderBottom : 10;
    struct Gadget *g = NULL;

    switch (kind) {
    case 1:
        g = (struct Gadget *)NewObject(g_cl, NULL, GA_Left, 10, GA_Top, 20, GA_Width, 50,
                                       GA_Height, 20, GA_ID, 1, GA_Previous, (ULONG)prev, TAG_END);
        break;
    case 2:
        g = (struct Gadget *)NewObject(g_cl, NULL, GA_Left, bl, GA_Top, bt,
                                       GA_RelWidth, -(bl + br), GA_RelHeight, -(bt + bb),
                                       GA_ID, 2, GA_Previous, (ULONG)prev, TAG_END);
        break;
    case 3:
        g = (struct Gadget *)NewObject(g_cl, NULL, GA_Left, 70, GA_Top, 20, GA_Width, 30,
                                       GA_Height, 10, GA_RelSpecial, TRUE, GA_ID, 3,
                                       GA_Previous, (ULONG)prev, TAG_END);
        break;
    case 4:
        g = (struct Gadget *)NewObject(g_cl, NULL, GA_RelRight, -40, GA_RelBottom, -30,
                                       GA_Width, 20, GA_Height, 10, GA_ID, 4,
                                       GA_Previous, (ULONG)prev, TAG_END);
        break;
    case 5:
        g = (struct Gadget *)NewObject(g_cl, NULL, GA_RelRight, -15, GA_Top, 20, GA_Width, 12,
                                       GA_Height, 10, GA_RightBorder, TRUE, GA_ID, 5,
                                       GA_Previous, (ULONG)prev, TAG_END);
        break;
    }
    return g;
}

static void flags(struct Gadget *g)
{
    probe_s("gadget");
    ps_kv("id", g->GadgetID);
    ps_kx("flags", g->Flags, 4);
    ps_kx("type", g->GadgetType, 4);
    ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height);
    probe_ch('\n');
}

static void run(const char *name, ULONG gzz)
{
    struct Window *w;
    struct Gadget *g[6];
    int i;

    P_SECTION(name);
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 30, WA_Width, 300, WA_Height, 100,
                       WA_Title, (ULONG)"GInfo", WA_CloseGadget, TRUE, WA_DepthGadget, TRUE,
                       WA_DragBar, TRUE, WA_SizeGadget, TRUE, WA_MinWidth, 50, WA_MinHeight, 40,
                       WA_MaxWidth, 600, WA_MaxHeight, 200, WA_GimmeZeroZero, gzz, TAG_END);
    if (!w)
        return;
    g[0] = NULL;
    for (i = 1; i <= 5; i++)
        g[i] = make(w, i, g[i - 1]);
    for (i = 1; i <= 5; i++)
        flags(g[i]);
    flush_log("NewObject");
    AddGList(w, g[1], -1, 5, NULL);
    flush_log("AddGList");
    RefreshGList(g[1], w, NULL, 5);
    flush_log("RefreshGList");
    SetWindowTitles(w, (STRPTR)"Other", (STRPTR)-1);
    flush_log("SetWindowTitles");
    RefreshWindowFrame(w);
    flush_log("RefreshWindowFrame");
    /* (not for the GZZ window: on 3.1 another window moving off its border
     * redraws all its gadgets, which lxa does not emulate yet) */
    if (!gzz) {
        struct Window *o = OpenWindowTags(NULL, WA_Left, 400, WA_Top, 30, WA_Width, 100,
                                          WA_Height, 50, WA_Title, (ULONG)"Other",
                                          WA_Activate, TRUE, TAG_END);
        Delay(10);
        flush_log("other window activated");
        ActivateWindow(w);
        Delay(10);
        flush_log("window reactivated");
        if (o) {
            MoveWindow(o, -390, 0);
            Delay(10);
            flush_log("other window moved over it");
            CloseWindow(o);
            Delay(10);
            flush_log("other window closed");
        }
    }
    SetGadgetAttrs(g[1], w, NULL, GA_ID, 1, TAG_END);
    SetGadgetAttrs(g[2], w, NULL, GA_ID, 2, TAG_END);
    flush_log("SetGadgetAttrs");
    DoGadgetMethod(g[1], w, NULL, PROBE_METHOD, NULL);
    DoGadgetMethod(g[2], w, NULL, PROBE_METHOD, NULL);
    flush_log("DoGadgetMethod");
    ChangeWindowBox(w, 40, 30, 340, 120);
    Delay(25);
    flush_log("ChangeWindowBox");
    RemoveGList(w, g[1], 5);
    flush_log("RemoveGList");
    CloseWindow(w);
    for (i = 1; i <= 5; i++)
        DisposeObject(g[i]);
    flush_log("CloseWindow");
}

int main(void)
{
    struct Window *w;
    struct Gadget *g1, *g2;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 39);
    if (!IntuitionBase)
        return 20;
    g_cl = MakeClass(NULL, (ClassID)"gadgetclass", NULL, 0, 0);
    if (!g_cl)
        return 20;
    g_cl->cl_Dispatcher.h_Entry = (ULONG (*)())dispatch;

    run("window", FALSE);
    run("gzz window", TRUE);

    P_SECTION("WA_Gadgets");
    g1 = make(NULL, 2, NULL);
    g2 = make(NULL, 3, g1);
    w = OpenWindowTags(NULL, WA_Left, 40, WA_Top, 30, WA_Width, 300, WA_Height, 100,
                       WA_Title, (ULONG)"GInfo", WA_SizeGadget, TRUE, WA_Gadgets, (ULONG)g1,
                       TAG_END);
    flush_log("OpenWindow");
    if (w)
        CloseWindow(w);
    flush_log("CloseWindow");
    DisposeObject(g1);
    DisposeObject(g2);

    FreeClass(g_cl);
    CloseLibrary(IntuitionBase);
    return 0;
}
