/*
 * Probe (Phase 222a): exec.library message ports - CreateMsgPort,
 * DeleteMsgPort, PutMsg, GetMsg, ReplyMsg, WaitPort, AddPort, FindPort,
 * RemPort, PA_SIGNAL/PA_IGNORE, node types along the message life cycle.
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct Message msgs[4];

static int idx(struct Message *m)
{
    int i;
    if (!m)
        return -1;
    for (i = 0; i < 4; i++)
        if (m == &msgs[i])
            return i;
    return -2;
}

static int count(struct MsgPort *p)
{
    struct Node *n;
    int k = 0;
    for (n = p->mp_MsgList.lh_Head; n->ln_Succ; n = n->ln_Succ)
        k++;
    return k;
}

int main(void)
{
    struct Task *me = FindTask(NULL);
    struct MsgPort *p, *r;
    ULONG sig;
    int i;

    P_SECTION("CreateMsgPort");
    p = CreateMsgPort();
    r = CreateMsgPort();
    if (!p || !r)
        return 20;
    P_LONG("ln_Type", p->mp_Node.ln_Type);
    P_LONG("ln_Pri", p->mp_Node.ln_Pri);
    P_NULL("ln_Name", p->mp_Node.ln_Name);
    P_LONG("mp_Flags", p->mp_Flags);
    P_BOOL("mp_SigTask == me", p->mp_SigTask == me);
    P_BOOL("mp_SigBit allocated", (me->tc_SigAlloc >> p->mp_SigBit) & 1);
    P_BOOL("mp_SigBit is a user bit (>= 16)", p->mp_SigBit >= 16);
    P_LONG("msg list empty count", count(p));
    P_LONG("mp_MsgList.lh_Type", p->mp_MsgList.lh_Type);
    sig = 1UL << p->mp_SigBit;

    P_SECTION("PutMsg/GetMsg");
    for (i = 0; i < 4; i++) {
        msgs[i].mn_Node.ln_Type = NT_UNKNOWN;
        msgs[i].mn_Node.ln_Pri = (BYTE)(i == 2 ? 10 : 0);
        msgs[i].mn_ReplyPort = r;
        msgs[i].mn_Length = sizeof(struct Message);
    }
    SetSignal(0, sig);
    PutMsg(p, &msgs[0]);
    P_LONG("ln_Type after PutMsg", msgs[0].mn_Node.ln_Type);
    P_BOOL("port signal set", (SetSignal(0, 0) & sig) != 0);
    PutMsg(p, &msgs[1]);
    PutMsg(p, &msgs[2]);              /* priority ignored: FIFO */
    PutMsg(p, &msgs[3]);
    P_LONG("count", count(p));
    P_LONG("WaitPort returns head", idx((struct Message *)WaitPort(p)));
    P_LONG("count after WaitPort", count(p));
    P_LONG("GetMsg", idx(GetMsg(p)));
    P_LONG("GetMsg", idx(GetMsg(p)));
    P_LONG("GetMsg", idx(GetMsg(p)));
    P_LONG("GetMsg", idx(GetMsg(p)));
    P_LONG("GetMsg empty", idx(GetMsg(p)));
    P_LONG("ln_Type after GetMsg", msgs[0].mn_Node.ln_Type);
    P_BOOL("signal still set (GetMsg does not clear)", (SetSignal(0, sig) & sig) != 0);

    P_SECTION("ReplyMsg");
    SetSignal(0, 1UL << r->mp_SigBit);
    ReplyMsg(&msgs[0]);
    P_LONG("ln_Type after ReplyMsg", msgs[0].mn_Node.ln_Type);
    P_BOOL("reply port signal", (SetSignal(0, 0) & (1UL << r->mp_SigBit)) != 0);
    P_LONG("reply port GetMsg", idx(GetMsg(r)));
    msgs[1].mn_ReplyPort = NULL;
    ReplyMsg(&msgs[1]);
    P_LONG("ln_Type after ReplyMsg(no reply port)", msgs[1].mn_Node.ln_Type);
    P_LONG("reply port empty", idx(GetMsg(r)));

    P_SECTION("PA_IGNORE");
    p->mp_Flags = PA_IGNORE;
    SetSignal(0, sig);
    PutMsg(p, &msgs[2]);
    P_BOOL("PA_IGNORE: no signal", (SetSignal(0, 0) & sig) == 0);
    P_LONG("PA_IGNORE: queued", count(p));
    P_LONG("GetMsg", idx(GetMsg(p)));
    p->mp_Flags = PA_SIGNAL;

    P_SECTION("public ports");
    {
        struct MsgPort *q = CreateMsgPort();
        q->mp_Node.ln_Name = (char *)"lxa.probe.port";
        q->mp_Node.ln_Pri = 5;
        AddPort(q);
        P_LONG("ln_Type after AddPort", q->mp_Node.ln_Type);
        P_BOOL("FindPort", FindPort((STRPTR)"lxa.probe.port") == q);
        P_NULL("FindPort (case differs)", FindPort((STRPTR)"LXA.PROBE.PORT"));
        P_NULL("FindPort (missing)", FindPort((STRPTR)"lxa.probe.none"));
        RemPort(q);
        P_NULL("FindPort after RemPort", FindPort((STRPTR)"lxa.probe.port"));
        q->mp_Node.ln_Name = NULL;
        DeleteMsgPort(q);
    }

    P_SECTION("DeleteMsgPort frees the signal");
    {
        BYTE bit = p->mp_SigBit;
        DeleteMsgPort(p);
        P_BOOL("signal bit free after DeleteMsgPort", !((me->tc_SigAlloc >> bit) & 1));
    }
    DeleteMsgPort(r);
    return 0;
}
