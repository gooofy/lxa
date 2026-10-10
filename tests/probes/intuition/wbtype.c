/*
 * Probe (Phase 237c): OpenScreen() with a NewScreen whose Type is
 * WBENCHSCREEN (1) - 1.x programs (Fred Fish Jed, JED, SmallJED) pass it
 * for a screen of their own.  Prints whether a screen opens, whether it
 * is the Workbench screen and its flags/size.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

static void try(const char *label, UWORD type, UWORD modes)
{
    struct NewScreen ns = { 0, 0, 640, 197, 2, 1, 2, modes, type, NULL, (UBYTE *)"probe", NULL, NULL };
    struct Screen *wb = LockPubScreen((STRPTR)"Workbench");
    struct Screen *s = OpenScreen(&ns);

    probe_s(label);
    if (!s) {
        probe_s(": NULL\n");
    } else {
        probe_s(": screen");
        probe_s(s == wb ? " (the Workbench)" : " (new)");
        probe_s(" type ");
        probe_hex(s->Flags & SCREENTYPE, 2);
        probe_s(" size ");
        probe_dec(s->Width);
        probe_ch('x');
        probe_dec(s->Height);
        probe_s(" depth ");
        probe_dec(s->BitMap.Depth);
        probe_ch('\n');
        if (s != wb)
            CloseScreen(s);
    }
    if (wb)
        UnlockPubScreen(NULL, wb);
}

int main(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 0);
    if (!IntuitionBase)
        return 20;
    try("Type WBENCHSCREEN hires", WBENCHSCREEN, HIRES);
    try("Type WBENCHSCREEN lores", WBENCHSCREEN, 0);
    try("Type CUSTOMSCREEN hires", CUSTOMSCREEN, HIRES);
    try("Type 0", 0, HIRES);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
