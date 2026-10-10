/*
 * Probe (Phase 244): how OpenWindow sizes the window borders around
 * application border gadgets (GACT_*BORDER), and which gadgets get
 * GACT_BORDERSNIFF.  SIGMAth's graphics window has a 10 pixel bottom border
 * on AmigaOS 3.1 (its scroller arrows live there), BECKERtext II a 20 pixel
 * top border (a status gadget below the title bar).  Windows on a private
 * custom screen, so the Workbench screen's font does not matter.
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static struct Screen *g_scr;
static struct Gadget g_gad[3];
static UWORD g_pens[] = { (UWORD)~0 };

static void setgad(int i, WORD l, WORD t, WORD w, WORD h, UWORD flags, UWORD act)
{
    struct Gadget *g = &g_gad[i];
    g->NextGadget = NULL;
    g->LeftEdge = l;
    g->TopEdge = t;
    g->Width = w;
    g->Height = h;
    g->Flags = flags | GFLG_GADGHCOMP;
    g->Activation = act;
    g->GadgetType = GTYP_BOOLGADGET;
    g->GadgetRender = NULL;
    g->SelectRender = NULL;
    g->GadgetText = NULL;
    g->MutualExclude = 0;
    g->SpecialInfo = NULL;
    g->GadgetID = i + 1;
    g->UserData = NULL;
}

static void show(struct Window *w, int ngad)
{
    struct Gadget *g;
    int i;

    if (!w)
    {
        probe_s("  window = NULL\n");
        return;
    }
    probe_s("  border = ");
    probe_dec(w->BorderLeft); probe_ch(' ');
    probe_dec(w->BorderTop); probe_ch(' ');
    probe_dec(w->BorderRight); probe_ch(' ');
    probe_dec(w->BorderBottom); probe_ch('\n');
    probe_s("  size = ");
    probe_dec(w->Width); probe_ch(' ');
    probe_dec(w->Height); probe_ch('\n');
    if (w->Flags & WFLG_GIMMEZEROZERO)
    {
        probe_s("  gzz = ");
        probe_dec(w->GZZWidth); probe_ch(' ');
        probe_dec(w->GZZHeight); probe_ch('\n');
    }
    for (g = w->FirstGadget; g; g = g->NextGadget)
    {
        UWORD st = g->GadgetType & GTYP_SYSTYPEMASK;
        if (!(g->GadgetType & GTYP_SYSGADGET))
            continue;
        if (st == GTYP_WDRAGGING || st == GTYP_SIZING || st == GTYP_CLOSE || st == GTYP_WDEPTH)
        {
            probe_s("  sys ");
            probe_hex(st, 2);
            probe_s(" = ");
            probe_dec(g->LeftEdge); probe_ch(' ');
            probe_dec(g->TopEdge); probe_ch(' ');
            probe_dec(g->Width); probe_ch(' ');
            probe_dec(g->Height); probe_s(" act ");
            probe_hex(g->Activation, 4);
            probe_ch('\n');
        }
    }
    for (i = 0; i < ngad; i++)
    {
        probe_s("  gad ");
        probe_dec(i + 1);
        probe_s(" act = ");
        probe_hex(g_gad[i].Activation, 4);
        probe_s(" flags = ");
        probe_hex(g_gad[i].Flags, 4);
        probe_ch('\n');
    }
}

static struct Window *open_tags(ULONG flags, struct Gadget *first)
{
    return OpenWindowTags(NULL, WA_CustomScreen, (ULONG)g_scr, WA_Left, 20, WA_Top, 20,
                          WA_Width, 300, WA_Height, 150, WA_Title, (ULONG)"Sniff",
                          WA_Flags, flags, WA_Gadgets, (ULONG)first, TAG_END);
}

#define STD (WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET)

static void run(const char *name, ULONG flags, int ngad)
{
    struct Window *w;
    int i;

    for (i = 0; i + 1 < ngad; i++)
        g_gad[i].NextGadget = &g_gad[i + 1];
    P_SECTION(name);
    w = open_tags(flags, ngad ? &g_gad[0] : NULL);
    show(w, ngad);
    if (w)
        CloseWindow(w);
}

int main(void)
{
    struct Window *w;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    g_scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2,
                           SA_DisplayID, 0x8000, SA_Title, (ULONG)"Probe",
                           SA_Pens, (ULONG)g_pens, TAG_END);
    if (!g_scr)
    {
        probe_s("OpenScreen failed\n");
        return 0;
    }

    run("plain", STD, 0);
    run("plain + size", STD | WFLG_SIZEGADGET, 0);
    run("plain + size bbottom", STD | WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM, 0);

    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -9/10 + size", STD | WFLG_SIZEGADGET, 1);
    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -9/10", STD, 1);
    setgad(0, 0, -10, 40, 11, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -10/11", STD, 1);
    setgad(0, 0, -7, 40, 6, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -7/6", STD, 1);
    setgad(0, 0, -20, 40, 5, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -20/5", STD, 1);
    setgad(0, 0, -4, 40, 14, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom -4/14", STD, 1);
    setgad(0, 0, 140, 40, 10, 0, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom abs 140/10", STD, 1);
    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    setgad(1, 50, -12, 40, 12, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("bottom two", STD | WFLG_SIZEGADGET, 2);

    setgad(0, 2, 10, 200, 10, 0, GACT_TOPBORDER | GACT_RELVERIFY);
    run("top 10/10", STD | WFLG_SIZEGADGET, 1);
    setgad(0, 2, 0, 200, 15, 0, GACT_TOPBORDER | GACT_RELVERIFY);
    run("top 0/15", STD, 1);
    setgad(0, 2, 12, 200, 4, 0, GACT_TOPBORDER | GACT_RELVERIFY);
    run("top 12/4", STD, 1);
    setgad(0, 2, 3, 200, 4, 0, GACT_TOPBORDER | GACT_RELVERIFY);
    run("top 3/4", STD, 1);

    setgad(0, -29, 20, 30, 20, GFLG_RELRIGHT, GACT_RIGHTBORDER | GACT_RELVERIFY);
    run("right -29/30", STD, 1);
    setgad(0, -40, 20, 10, 20, GFLG_RELRIGHT, GACT_RIGHTBORDER | GACT_RELVERIFY);
    run("right -40/10", STD, 1);
    setgad(0, -14, 20, 14, 20, GFLG_RELRIGHT, GACT_RIGHTBORDER | GACT_RELVERIFY);
    run("right -14/14 + size", STD | WFLG_SIZEGADGET, 1);
    setgad(0, 250, 20, 50, 20, 0, GACT_RIGHTBORDER | GACT_RELVERIFY);
    run("right abs 250/50", STD, 1);

    setgad(0, 0, 20, 12, 20, 0, GACT_LEFTBORDER | GACT_RELVERIFY);
    run("left 0/12", STD, 1);
    setgad(0, 10, 20, 5, 20, 0, GACT_LEFTBORDER | GACT_RELVERIFY);
    run("left 10/5", STD, 1);

    setgad(0, 0, 0, 20, 20, 0, GACT_RELVERIFY);
    run("inner overlap", STD, 1);
    setgad(0, 30, 30, 20, 20, 0, GACT_RELVERIFY);
    run("inner", STD, 1);
    setgad(0, 0, 30, 6, 20, 0, GACT_RELVERIFY);
    run("inner overlap left", STD, 1);
    setgad(0, 294, 30, 6, 20, 0, GACT_RELVERIFY);
    run("inner overlap right", STD, 1);
    setgad(0, 30, 5, 20, 20, 0, GACT_RELVERIFY);
    run("inner overlap top", STD, 1);
    setgad(0, 0, 10, 6, 20, 0, GACT_RELVERIFY);
    run("inner overlap left, top - 1", STD, 1);
    setgad(0, 0, 40, 8, 10, 0, GACT_RELVERIFY);
    run("center x 4", STD, 1);
    setgad(0, 0, 40, 7, 10, 0, GACT_RELVERIFY);
    run("center x 3", STD, 1);
    setgad(0, 30, 136, 20, 14, 0, GACT_RELVERIFY);
    run("bottom overlap center y 143", STD, 1);
    setgad(0, 30, 140, 20, 10, 0, GACT_RELVERIFY);
    run("bottom overlap center y 145", STD, 1);
    setgad(0, 30, 145, 20, 4, 0, GACT_RELVERIFY);
    run("bottom overlap center y 147", STD, 1);
    setgad(0, 30, 146, 20, 4, 0, GACT_RELVERIFY);
    run("bottom overlap center y 148", STD, 1);
    setgad(0, 30, 140, 20, 16, 0, GACT_RELVERIFY);
    run("beyond bottom center y 148", STD, 1);
    setgad(0, 290, 40, 10, 10, 0, GACT_RELVERIFY);
    run("right overlap center x 295", STD, 1);
    setgad(0, 291, 40, 9, 10, 0, GACT_RELVERIFY);
    run("right overlap center x 295.5", STD, 1);
    setgad(0, 292, 40, 8, 10, 0, GACT_RELVERIFY);
    run("right overlap center x 296", STD, 1);
    setgad(0, 30, 144, 20, 4, 0, GACT_RELVERIFY);
    run("bottom overlap center y 146", STD, 1);
    setgad(0, 30, 142, 20, 8, 0, GACT_RELVERIFY);
    run("bottom overlap center y 146 h 8", STD, 1);
    setgad(0, 30, 0, 20, 8, 0, GACT_RELVERIFY);
    run("title bar center y 4", STD, 1);
    setgad(0, 30, 0, 20, 14, 0, GACT_RELVERIFY);
    run("title bar bottom 14", STD, 1);
    setgad(0, 30, 0, 20, 15, 0, GACT_RELVERIFY);
    run("title bar bottom 15", STD, 1);
    setgad(0, 30, 2, 20, 16, 0, GACT_RELVERIFY);
    run("title bar bottom 18", STD, 1);
    setgad(0, 292, 40, 10, 10, 0, GACT_RELVERIFY);
    run("beyond right center x 297", STD, 1);
    setgad(0, -4, 40, 10, 10, 0, GACT_RELVERIFY);
    run("beyond left center x 1", STD, 1);
    setgad(0, 2, 0, 6, 20, 0, GACT_RELVERIFY);
    run("center x 5 y 10", STD, 1);

    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    run("gzz bottom -9/10", STD | WFLG_GIMMEZEROZERO, 1);

    /* border gadgets added after OpenWindow */
    P_SECTION("addglist bottom -9/10 + right");
    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    setgad(1, -14, 20, 14, 20, GFLG_RELRIGHT, GACT_RIGHTBORDER | GACT_RELVERIFY);
    setgad(2, 30, 30, 20, 20, 0, GACT_RELVERIFY);
    g_gad[0].NextGadget = &g_gad[1];
    g_gad[1].NextGadget = &g_gad[2];
    w = open_tags(STD | WFLG_SIZEGADGET, NULL);
    if (w)
    {
        probe_s("  AddGList = ");
        probe_dec((WORD)AddGList(w, &g_gad[0], 0, -1, NULL));
        probe_ch('\n');
        show(w, 3);
        RemoveGList(w, &g_gad[0], 3);
        probe_s("  after RemoveGList\n");
        show(w, 3);
        CloseWindow(w);
    }

    P_SECTION("addglist overlap");
    setgad(0, 0, 0, 20, 20, 0, GACT_RELVERIFY);
    setgad(1, 0, 30, 6, 20, 0, GACT_RELVERIFY);
    setgad(2, 270, 147, 30, 3, 0, GACT_RELVERIFY);
    g_gad[0].NextGadget = &g_gad[1];
    g_gad[1].NextGadget = &g_gad[2];
    w = open_tags(STD, NULL);
    if (w)
    {
        AddGList(w, &g_gad[0], ~0, -1, NULL);
        show(w, 3);
        RemoveGList(w, &g_gad[0], 3);
        CloseWindow(w);
    }

    P_SECTION("addgadget bottom");
    setgad(0, 0, -9, 40, 10, GFLG_RELBOTTOM, GACT_BOTTOMBORDER | GACT_RELVERIFY);
    w = open_tags(STD, NULL);
    if (w)
    {
        probe_s("  AddGadget = ");
        probe_dec((WORD)AddGadget(w, &g_gad[0], ~0));
        probe_ch('\n');
        show(w, 1);
        RemoveGadget(w, &g_gad[0]);
        CloseWindow(w);
    }

    CloseScreen(g_scr);
    CloseLibrary(IntuitionBase);
    return 0;
}
