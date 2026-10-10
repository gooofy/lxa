/*
 * Probe (Phase 244): when Intuition marks a gadget GFLG_IMAGEDISABLE.
 * FinalWriter's buttons (GADGIMAGE with BOOPSI frame images) carry it on
 * AmigaOS 3.1 after the window opened.
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static UWORD plane[4];
static struct Image plain = { 0, 0, 16, 2, 1, plane, 1, 0, NULL };

static void setgad(struct Gadget *g, WORD top, UWORD flags, APTR render, APTR select)
{
    g->NextGadget = NULL;
    g->LeftEdge = 10;
    g->TopEdge = top;
    g->Width = 60;
    g->Height = 12;
    g->Flags = flags;
    g->Activation = GACT_RELVERIFY;
    g->GadgetType = GTYP_BOOLGADGET;
    g->GadgetRender = render;
    g->SelectRender = select;
    g->GadgetText = NULL;
    g->MutualExclude = 0;
    g->SpecialInfo = NULL;
    g->GadgetID = 0;
    g->UserData = NULL;
}

int main(void)
{
    struct Screen *scr;
    struct DrawInfo *dri;
    struct Image *frame, *frame2, *sys;
    struct Gadget g[5], a[2];
    struct Window *w;
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    scr = LockPubScreen(NULL);
    dri = scr ? GetScreenDrawInfo(scr) : NULL;
    if (!dri)
        return 0;
    frame = (struct Image *)NewObject(NULL, (STRPTR)"frameiclass", IA_Width, 60, IA_Height, 12, TAG_END);
    frame2 = (struct Image *)NewObject(NULL, (STRPTR)"frameiclass", IA_Width, 60, IA_Height, 12,
                                       IA_Recessed, TRUE, TAG_END);
    sys = (struct Image *)NewObject(NULL, (STRPTR)"sysiclass", SYSIA_DrawInfo, (ULONG)dri,
                                    SYSIA_Which, CHECKIMAGE, TAG_END);
    P_NULL("frame", frame);
    P_NULL("sys", sys);
    if (!frame || !frame2 || !sys)
        return 0;
    P_LONG("frame depth", frame->Depth);

    setgad(&g[0], 20, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, frame, frame2);
    setgad(&g[1], 35, GFLG_GADGIMAGE | GFLG_GADGHCOMP, frame, NULL);
    setgad(&g[2], 50, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, &plain, &plain);
    setgad(&g[3], 65, GFLG_GADGIMAGE | GFLG_GADGHCOMP, sys, NULL);
    setgad(&g[4], 80, GFLG_GADGHCOMP, NULL, NULL);
    for (i = 0; i < 4; i++)
        g[i].NextGadget = &g[i + 1];
    probe_s("before: ");
    for (i = 0; i < 5; i++) { probe_hex(g[i].Flags, 4); probe_ch(' '); }
    probe_ch('\n');
    w = OpenWindowTags(NULL, WA_PubScreen, (ULONG)scr, WA_Left, 10, WA_Top, 20, WA_Width, 200,
                       WA_Height, 120, WA_Title, (ULONG)"ImgDis", WA_DragBar, TRUE, WA_Gadgets, (ULONG)g,
                       TAG_END);
    if (w)
    {
        probe_s("open:   ");
        for (i = 0; i < 5; i++) { probe_hex(g[i].Flags, 4); probe_ch(' '); }
        probe_ch('\n');
        setgad(&a[0], 95, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, frame, frame2);
        setgad(&a[1], 100, GFLG_GADGIMAGE | GFLG_GADGHIMAGE, &plain, &plain);
        a[0].NextGadget = &a[1];
        AddGList(w, a, ~0, 2, NULL);
        probe_s("added:  ");
        for (i = 0; i < 2; i++) { probe_ch(' '); probe_hex(a[i].Flags, 4); }
        probe_ch('\n');
        RemoveGList(w, a, 2);
        RemoveGList(w, g, 5);
        CloseWindow(w);
    }
    DisposeObject(frame);
    DisposeObject(frame2);
    DisposeObject(sys);
    FreeScreenDrawInfo(scr, dri);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(IntuitionBase);
    return 0;
}
