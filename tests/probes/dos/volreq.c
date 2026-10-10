/*
 * Probe (Phase 237e): Lock() of a volume that is not there, with the
 * process's pr_WindowPtr 0 (the default): AmigaOS 3.1 puts up a "Please
 * insert volume" system requester and waits.  Fred Fish hack (disk 62)
 * locks "hack_game:" - on the reference the requester is the window it
 * shows.  A helper process does the Lock(); the probe looks for the
 * requester on the Workbench screen, prints its title and gadget texts,
 * answers it with Cancel (a GADGETUP message for the gadget with ID 0) and
 * prints what Lock() returned.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/intuitionbase.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;

static volatile LONG done, lockres, lockerr;

static void helper(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR l;

    me->pr_WindowPtr = NULL;
    l = Lock((STRPTR)"p237evol:x", ACCESS_READ);
    lockerr = IoErr();
    lockres = l != 0;
    if (l)
        UnLock(l);
    Forbid();
    done = 1;
}

static struct Window *find_req(struct Window *before[], int nbefore)
{
    struct Screen *s = LockPubScreen((STRPTR)"Workbench");
    struct Window *w, *found = NULL;
    int i;

    if (!s)
        return NULL;
    Forbid();
    for (w = s->FirstWindow; w && !found; w = w->NextWindow) {
        for (i = 0; i < nbefore; i++)
            if (before[i] == w)
                break;
        if (i == nbefore)
            found = w;
    }
    Permit();
    UnlockPubScreen(NULL, s);
    return found;
}

int main(void)
{
    struct Window *before[32], *w = NULL;
    struct Screen *s;
    int n = 0, i;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    if (!IntuitionBase)
        return 20;
    s = LockPubScreen((STRPTR)"Workbench");
    if (s) {
        Forbid();
        for (w = s->FirstWindow; w && n < 32; w = w->NextWindow)
            before[n++] = w;
        Permit();
        UnlockPubScreen(NULL, s);
    }
    if (!CreateNewProcTags(NP_Entry, (ULONG)helper, NP_Name, (ULONG)"probe.volreq", NP_StackSize, 8192,
                           NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END))
        return 20;
    w = NULL;
    for (i = 0; i < 100 && !done && !(w = find_req(before, n)); i++)
        Delay(2);
    P_BOOL("Lock() returned without a requester", done && !w);
    if (w)
        Delay(25);              /* the requester is complete */
    if (w) {
        struct Gadget *g, *cancel = NULL;
        struct MsgPort *port = CreateMsgPort();
        struct IntuiMessage *im = AllocMem(sizeof(*im), MEMF_PUBLIC | MEMF_CLEAR);

        P_STR("requester title", w->Title ? (const char *)w->Title : "(none)");
        for (g = w->FirstGadget; g; g = g->NextGadget) {
            /* the answer gadgets (3.1 also has an image gadget for the body) */
            if ((g->GadgetType & GTYP_SYSGADGET) || !(g->Activation & GACT_RELVERIFY))
                continue;
            probe_s("gadget ");
            probe_dec(g->GadgetID);
            probe_s(": ");
            probe_s(g->GadgetText && g->GadgetText->IText ? (const char *)g->GadgetText->IText : "(no text)");
            probe_ch('\n');
            if (g->GadgetID == 0)
                cancel = g;
        }
        if (cancel && port && im && w->UserPort) {
            im->ExecMessage.mn_ReplyPort = port;
            im->ExecMessage.mn_Length = sizeof(*im);
            im->Class = IDCMP_GADGETUP;
            im->IAddress = cancel;
            im->IDCMPWindow = w;
            PutMsg(w->UserPort, &im->ExecMessage);
            for (i = 0; i < 100 && !done; i++)
                Delay(2);
        }
        P_BOOL("Lock() returned after Cancel", done);
    }
    if (done) {
        P_BOOL("Lock() succeeded", lockres);
        P_LONG("IoErr()", lockerr);
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
