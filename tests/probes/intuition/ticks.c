/*
 * Probe (Phase 222g): IDCMP_INTUITICKS delivery and WFLG_WINDOWTICKED -
 * which windows receive ticks, whether ticks queue up while one is
 * outstanding, and when Intuition sets and clears WFLG_WINDOWTICKED.
 */
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/dos_protos.h>
#include "idump.h"

struct Library *IntuitionBase;

static LONG count_class(struct Window *w, ULONG cls)
{
    struct Node *n;
    LONG c = 0;

    Forbid();
    for (n = w->UserPort->mp_MsgList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if (((struct IntuiMessage *)n)->Class == cls)
            c++;
    Permit();
    return c;
}

static void reply_all(struct Window *w)
{
    struct IntuiMessage *m;

    while ((m = (struct IntuiMessage *)GetMsg(w->UserPort)) != NULL)
        ReplyMsg((struct Message *)m);
}

static void state(const char *label, struct Window *a, struct Window *b)
{
    probe_s(label);
    ps_kv("a.ticked", (a->Flags & WFLG_WINDOWTICKED) ? 1 : 0);
    ps_kv("a.ticks", count_class(a, IDCMP_INTUITICKS));
    ps_kv("a.active", (a->Flags & WFLG_WINDOWACTIVE) ? 1 : 0);
    ps_kv("b.ticked", (b->Flags & WFLG_WINDOWTICKED) ? 1 : 0);
    ps_kv("b.ticks", count_class(b, IDCMP_INTUITICKS));
    ps_kv("b.active", (b->Flags & WFLG_WINDOWACTIVE) ? 1 : 0);
    probe_ch('\n');
}

int main(void)
{
    struct Window *a, *b;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 20;

    a = OpenWindowTags(NULL, WA_Left, 20, WA_Top, 20, WA_Width, 200, WA_Height, 60,
                       WA_Title, (ULONG)"TickA", WA_IDCMP, IDCMP_INTUITICKS,
                       WA_Activate, TRUE, TAG_END);
    b = OpenWindowTags(NULL, WA_Left, 260, WA_Top, 20, WA_Width, 200, WA_Height, 60,
                       WA_Title, (ULONG)"TickB", WA_IDCMP, IDCMP_INTUITICKS, TAG_END);
    if (!a || !b)
        return 20;
    Delay(10);
    reply_all(a);
    reply_all(b);
    Delay(5);
    state("open", a, b);

    Delay(50);
    state("idle 1s", a, b);

    reply_all(a);
    state("a replied", a, b);
    Delay(25);
    state("a replied +0.5s", a, b);

    ActivateWindow(b);
    Delay(25);
    state("b activated", a, b);
    reply_all(a);
    reply_all(b);
    Delay(25);
    state("both replied +0.5s", a, b);

    /* a tick outstanding while the window stops asking for ticks */
    ModifyIDCMP(b, IDCMP_CLOSEWINDOW);
    state("b idcmp off", a, b);
    reply_all(b);
    Delay(25);
    state("b idcmp off replied", a, b);
    ModifyIDCMP(b, IDCMP_INTUITICKS);
    Delay(25);
    state("b idcmp on", a, b);

    /* another message to the inactive window with a replied tick */
    ModifyIDCMP(a, IDCMP_INTUITICKS | IDCMP_CHANGEWINDOW);
    MoveWindow(a, 4, 0);
    Delay(10);
    state("a moved", a, b);
    reply_all(a);
    MoveWindow(a, -4, 0);
    Delay(10);
    state("a moved again", a, b);

    CloseWindow(b);
    CloseWindow(a);
    CloseLibrary(IntuitionBase);
    return 0;
}
