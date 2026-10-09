/*
 * Probe (Phase 238): the Workbench screen's first window as a program sees
 * it.  The reference runs LoadWB, whose backdrop window is the Workbench
 * screen's FirstWindow; Fish ISAM opens its "Request" window through it
 * (GetScreenData(WBENCHSCREEN)->FirstWindow).  lxa's harnesses open the
 * same window (LXA_WB_WINDOW=1).
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

int main(void)
{
    struct Screen s;
    struct Window *w;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase || !GetScreenData(&s, sizeof(s), WBENCHSCREEN, NULL))
    {
        probe_s("GetScreenData failed\n");
        return 0;
    }
    P_LONG("screen width", s.Width);
    P_LONG("screen height", s.Height);
    w = s.FirstWindow;
    P_NULL("FirstWindow", w);
    if (w)
    {
        P_NULL("title", w->Title);
        P_LONG("left", w->LeftEdge);
        P_LONG("top", w->TopEdge);
        P_LONG("width", w->Width);
        P_LONG("height", w->Height);
        P_HEX("flags & (BACKDROP|BORDERLESS|SIMPLE_REFRESH)",
              w->Flags & (WFLG_BACKDROP | WFLG_BORDERLESS | WFLG_SIMPLE_REFRESH));
        P_NULL("FirstGadget", w->FirstGadget);
    }
    return 0;
}
