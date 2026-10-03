/*
 * Probe (Phase 238): intuition.library window activation and the
 * IDCMP_ACTIVEWINDOW / IDCMP_INACTIVEWINDOW messages - when the
 * activation of a WA_Activate window takes effect relative to
 * OpenWindowTagList() returning, whether a window whose IDCMP is set only
 * after OpenWindow() (ModifyIDCMP) still receives its ACTIVEWINDOW, and
 * what ActivateWindow() of an already active window sends.
 * (AmigaOberon's OEd sets its IDCMP from a second process after
 * OpenWindow() and waits for the first message before it draws.)
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

static struct Screen *g_scr;

static void drain(const char *label, struct Window *w)
{
    struct IntuiMessage *m;
    int n = 0;

    Delay(10);
    probe_s(label);
    probe_s(":");
    if (w->UserPort) {
        while ((m = (struct IntuiMessage *)GetMsg(w->UserPort))) {
            probe_s(" ");
            if (m->Class == IDCMP_ACTIVEWINDOW)
                probe_s("ACTIVE");
            else if (m->Class == IDCMP_INACTIVEWINDOW)
                probe_s("INACTIVE");
            else
                probe_hex(m->Class, 8);
            ReplyMsg((struct Message *)m);
            n++;
        }
    }
    if (!n)
        probe_s(" (none)");
    probe_ch('\n');
}

static void state(const char *label, struct Window *w)
{
    probe_s(label);
    probe_s(": WINDOWACTIVE=");
    probe_dec((w->Flags & WFLG_WINDOWACTIVE) ? 1 : 0);
    probe_s(" ActiveWindow==w ");
    probe_dec(IntuitionBase->ActiveWindow == w);
    probe_ch('\n');
}

static struct Window *open(ULONG idcmp, BOOL activate, WORD left)
{
    return OpenWindowTags(NULL, WA_PubScreen, (ULONG)g_scr, WA_Left, left, WA_Top, 30,
                          WA_Width, 160, WA_Height, 60, WA_Title, (ULONG)"Activate probe",
                          WA_DragBar, TRUE, WA_DepthGadget, TRUE,
                          WA_IDCMP, idcmp, WA_Activate, activate, TAG_END);
}

int main(void)
{
    struct Window *a, *b;
    const ULONG act = IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;
    g_scr = LockPubScreen(NULL);
    if (!g_scr)
        return 20;

    P_SECTION("WA_Activate with IDCMP from the start");
    a = open(act, TRUE, 20);
    if (!a)
        return 20;
    state("right after OpenWindow", a);
    drain("messages", a);
    state("after Delay", a);
    P_SECTION("ActivateWindow of the active window");
    ActivateWindow(a);
    drain("messages", a);
    CloseWindow(a);

    P_SECTION("WA_Activate, IDCMP set by ModifyIDCMP after OpenWindow");
    a = open(0, TRUE, 20);
    if (!a)
        return 20;
    ModifyIDCMP(a, act);
    drain("messages", a);
    state("after Delay", a);
    P_SECTION("... then ActivateWindow");
    ActivateWindow(a);
    drain("messages", a);
    CloseWindow(a);

    P_SECTION("no WA_Activate, ModifyIDCMP, ActivateWindow");
    a = open(0, FALSE, 20);
    if (!a)
        return 20;
    ModifyIDCMP(a, act);
    state("right after OpenWindow", a);
    ActivateWindow(a);
    state("right after ActivateWindow", a);
    drain("messages", a);
    state("after Delay", a);

    P_SECTION("second window activated");
    b = open(act, TRUE, 200);
    if (b) {
        drain("A", a);
        drain("B", b);
        state("A", a);
        state("B", b);
        ActivateWindow(a);
        drain("A after ActivateWindow(A)", a);
        drain("B after ActivateWindow(A)", b);
        CloseWindow(b);
        drain("A after CloseWindow(B)", a);
        state("A", a);
    }
    CloseWindow(a);

    UnlockPubScreen(NULL, g_scr);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
