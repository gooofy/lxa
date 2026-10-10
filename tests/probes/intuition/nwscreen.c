/*
 * Probe (Phase 237c): OpenWindow() with NewWindow.Type WBENCHSCREEN and a
 * Screen field that does not point to a screen.  GFA-BASIC programs (Fred
 * Fish ShowGuru, WBase, SetFont, FHS ...) pass the buffer they gave
 * GetScreenData() there.  Prints on which screen the window opens.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

static void try(const char *label, struct Screen *field, struct Screen *wb)
{
    struct NewWindow nw;
    struct Window *w;
    nw.LeftEdge = 20; nw.TopEdge = 20; nw.Width = 200; nw.Height = 60;
    nw.DetailPen = 0; nw.BlockPen = 1;
    nw.IDCMPFlags = 0;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET;
    nw.FirstGadget = NULL; nw.CheckMark = NULL;
    nw.Title = (UBYTE *)"probe";
    nw.Screen = field;
    nw.BitMap = NULL;
    nw.MinWidth = 50; nw.MinHeight = 20; nw.MaxWidth = 640; nw.MaxHeight = 256;
    nw.Type = WBENCHSCREEN;
    w = OpenWindow(&nw);
    probe_s(label);
    if (!w)
        probe_s(": no window\n");
    else {
        probe_s(w->WScreen == wb ? ": on the Workbench screen\n" : ": on another screen\n");
        CloseWindow(w);
    }
}

int main(void)
{
    struct Screen *wb, *buf;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase)
        return 20;
    wb = LockPubScreen((STRPTR)"Workbench");
    buf = AllocMem(sizeof(struct Screen), MEMF_CLEAR);
    if (!wb || !buf)
        return 20;
    try("Screen NULL", NULL, wb);
    try("Screen = a zeroed buffer", buf, wb);
    GetScreenData(buf, sizeof(struct Screen), WBENCHSCREEN, NULL);
    try("Screen = a GetScreenData() copy", buf, wb);
    FreeMem(buf, sizeof(struct Screen));
    UnlockPubScreen(NULL, wb);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
