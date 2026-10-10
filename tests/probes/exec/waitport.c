/*
 * Probe (Phase 237c): how WaitPort() decides that a port is empty - from
 * lh_Head->ln_Succ or from lh_TailPred.  Fred Fish ParM/BrowserII call
 * WaitPort(NULL) before their port exists; what the vector table at
 * address 0 looks like as a list decides whether they wait.  The port's
 * list is made inconsistent: lh_Head points at a node with a successor
 * while lh_TailPred says "empty".  A helper process repairs the list and
 * signals after a while; the probe prints whether WaitPort() returned at
 * once or waited for the helper.
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct MsgPort port;
static struct Message msg;
static struct Node fake;            /* a node with a successor */
static struct Task *me;
static volatile int helper_done;

static void helper(void)
{
    Delay(25);
    helper_done = 1;
    /* a consistent list with one message */
    port.mp_MsgList.lh_Head = (struct Node *)&msg;
    port.mp_MsgList.lh_TailPred = (struct Node *)&msg;
    msg.mn_Node.ln_Succ = (struct Node *)&port.mp_MsgList.lh_Tail;
    msg.mn_Node.ln_Pred = (struct Node *)&port.mp_MsgList.lh_Head;
    Signal(me, 1L << port.mp_SigBit);
}

int main(void)
{
    BYTE sig = AllocSignal(-1);
    struct Process *p;
    struct Message *m;

    me = FindTask(NULL);
    port.mp_Node.ln_Type = NT_MSGPORT;
    port.mp_Flags = PA_SIGNAL;
    port.mp_SigBit = sig;
    port.mp_SigTask = me;
    /* head says "non-empty", tail-pred says "empty" */
    fake.ln_Succ = (struct Node *)&msg;
    port.mp_MsgList.lh_Head = &fake;
    port.mp_MsgList.lh_Tail = NULL;
    port.mp_MsgList.lh_TailPred = (struct Node *)&port.mp_MsgList.lh_Head;
    SetSignal(0, 1L << sig);

    p = CreateNewProcTags(NP_Entry, (ULONG)helper, NP_Name, (ULONG)"waitport helper", TAG_DONE);
    if (!p)
        return 20;
    m = WaitPort(&port);
    probe_s("WaitPort with head->succ set, tail-pred empty: ");
    probe_s(helper_done ? "waited" : "returned at once");
    probe_ch('\n');
    P_BOOL("returned the head node", m == (struct Message *)&fake);
    while (!helper_done)
        Delay(5);
    Delay(5);
    FreeSignal(sig);
    return 0;
}
