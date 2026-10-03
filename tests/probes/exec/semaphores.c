/*
 * Probe (Phase 222a): exec.library signal semaphores - InitSemaphore,
 * ObtainSemaphore[Shared], AttemptSemaphore[Shared], ReleaseSemaphore,
 * AddSemaphore, FindSemaphore, RemSemaphore, ObtainSemaphoreList,
 * ReleaseSemaphoreList, Procure, Vacate - single task and contended
 * by a subtask.  Prints nest/queue counts and ownership, never pointers.
 */
#include <exec/types.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"
#include "subtask.h"

extern struct ExecBase *SysBase;

static struct SignalSemaphore sem, sem2;
static struct Task *mainTask, *sub;
static volatile LONG r1, r2, step;

static void state(const char *label, struct SignalSemaphore *s)
{
    probe_s(label);
    probe_s(": nest ");
    probe_dec(s->ss_NestCount);
    probe_s(" queue ");
    probe_dec(s->ss_QueueCount);
    probe_s(" owner ");
    probe_s(s->ss_Owner == NULL ? "none" : s->ss_Owner == mainTask ? "main" : s->ss_Owner == sub ? "sub" : "other");
    probe_s(" waiters ");
    {
        struct Node *n;
        int k = 0;
        for (n = (struct Node *)s->ss_WaitQueue.mlh_Head; n->ln_Succ; n = n->ln_Succ)
            k++;
        probe_dec(k);
    }
    probe_ch('\n');
}

static void done(void)
{
    Forbid();
    Signal(mainTask, SIGBREAKF_CTRL_F);
}

/* tries the semaphore while main holds it */
static void sub_attempt(void)
{
    r1 = AttemptSemaphore(&sem);
    if (r1)
        ReleaseSemaphore(&sem);
    r2 = AttemptSemaphoreShared(&sem);
    if (r2)
        ReleaseSemaphore(&sem);
    done();
}

/* blocks on the semaphore */
static void sub_obtain(void)
{
    step = 1;
    ObtainSemaphore(&sem);
    step = 2;
    r1 = sem.ss_NestCount;
    ReleaseSemaphore(&sem);
    done();
}

static void sub_obtain_shared(void)
{
    step = 1;
    ObtainSemaphoreShared(&sem);
    step = 2;
    r1 = sem.ss_NestCount;
    Signal(mainTask, SIGBREAKF_CTRL_E);
    Wait(SIGBREAKF_CTRL_E);
    ReleaseSemaphore(&sem);
    done();
}

static void run(void (*fn)(void), BYTE pri)
{
    SetSignal(0, SIGBREAKF_CTRL_F | SIGBREAKF_CTRL_E);
    step = 0;
    sub = start_task("probesub", fn, pri);
}

static void finish(void)
{
    Wait(SIGBREAKF_CTRL_F);
    free_task(sub);
    sub = NULL;
}

int main(void)
{
    mainTask = FindTask(NULL);
    SetTaskPri(mainTask, 0);         /* the launcher's priority is not part of the probe */
    SetTaskPri(mainTask, 0);         /* the launcher's priority is not part of the probe */

    P_SECTION("InitSemaphore");
    {
        int i;
        for (i = 0; i < (int)sizeof(sem); i++)
            ((UBYTE *)&sem)[i] = 0x55;
    }
    InitSemaphore(&sem);
    state("init", &sem);
    P_LONG("ln_Type", sem.ss_Link.ln_Type);
    P_LONG("ln_Pri", sem.ss_Link.ln_Pri);
    P_BOOL("ss_MultipleLink zeroed", sem.ss_MultipleLink.sr_Waiter == NULL);
    P_BOOL("wait queue empty", sem.ss_WaitQueue.mlh_TailPred == (struct MinNode *)&sem.ss_WaitQueue);

    P_SECTION("single task");
    ObtainSemaphore(&sem);
    state("Obtain", &sem);
    ObtainSemaphore(&sem);
    state("Obtain nested", &sem);
    P_BOOL("AttemptSemaphore (owner)", AttemptSemaphore(&sem));
    state("after Attempt", &sem);
    P_BOOL("AttemptSemaphoreShared (exclusive owner)", AttemptSemaphoreShared(&sem));
    state("after AttemptShared", &sem);
    ReleaseSemaphore(&sem);
    ReleaseSemaphore(&sem);
    ReleaseSemaphore(&sem);
    state("3 releases", &sem);
    ReleaseSemaphore(&sem);
    state("4 releases", &sem);
    ObtainSemaphoreShared(&sem);
    state("ObtainShared", &sem);
    ObtainSemaphoreShared(&sem);
    state("ObtainShared nested", &sem);
    P_BOOL("AttemptSemaphore (shared owner)", AttemptSemaphore(&sem));
    state("after Attempt on shared", &sem);
    ReleaseSemaphore(&sem);
    state("release", &sem);
    ReleaseSemaphore(&sem);
    state("release", &sem);
    if (sem.ss_NestCount) {
        ReleaseSemaphore(&sem);
        state("release", &sem);
    }

    P_SECTION("contended");
    ObtainSemaphore(&sem);
    run(sub_attempt, 0);
    finish();
    P_BOOL("sub AttemptSemaphore while main exclusive", r1);
    P_BOOL("sub AttemptSemaphoreShared while main exclusive", r2);
    ReleaseSemaphore(&sem);

    ObtainSemaphoreShared(&sem);
    run(sub_attempt, 0);
    finish();
    P_BOOL("sub AttemptSemaphore while main shared", r1);
    P_BOOL("sub AttemptSemaphoreShared while main shared", r2);
    ReleaseSemaphore(&sem);

    ObtainSemaphore(&sem);
    run(sub_obtain, 1);              /* higher priority: runs until it blocks */
    P_LONG("sub step (blocked)", step);
    state("main exclusive, sub waiting", &sem);
    ReleaseSemaphore(&sem);
    P_LONG("sub step after release", step);
    finish();
    P_LONG("sub saw nest", r1);
    state("after sub", &sem);

    ObtainSemaphoreShared(&sem);
    run(sub_obtain_shared, 1);
    P_LONG("sub step (shared+shared)", step);
    if (step == 2) {
        Wait(SIGBREAKF_CTRL_E);
        state("two shared holders", &sem);
        P_LONG("sub saw nest", r1);
        r2 = AttemptSemaphore(&sem);
        P_BOOL("main AttemptSemaphore with two sharers", r2);
        if (r2)
            ReleaseSemaphore(&sem);
        Signal(sub, SIGBREAKF_CTRL_E);
    }
    finish();
    ReleaseSemaphore(&sem);
    state("all released", &sem);

    ObtainSemaphoreShared(&sem);
    run(sub_obtain, 1);
    P_LONG("sub exclusive vs main shared: step", step);
    state("main shared, sub waiting exclusive", &sem);
    ReleaseSemaphore(&sem);
    finish();
    state("after", &sem);

    P_SECTION("public semaphores");
    InitSemaphore(&sem2);
    sem2.ss_Link.ln_Name = (char *)"lxa.probe.sem";
    sem2.ss_Link.ln_Pri = 0;
    AddSemaphore(&sem2);
    P_BOOL("FindSemaphore", FindSemaphore((STRPTR)"lxa.probe.sem") == &sem2);
    P_NULL("FindSemaphore(lowercase variant)", FindSemaphore((STRPTR)"LXA.PROBE.SEM"));
    P_LONG("ln_Type after AddSemaphore", sem2.ss_Link.ln_Type);
    RemSemaphore(&sem2);
    P_NULL("FindSemaphore after RemSemaphore", FindSemaphore((STRPTR)"lxa.probe.sem"));
    state("sem2 after add/rem", &sem2);

    P_SECTION("ObtainSemaphoreList");
    {
        struct List l;
        l.lh_Head = (struct Node *)&l.lh_Tail;
        l.lh_Tail = NULL;
        l.lh_TailPred = (struct Node *)&l.lh_Head;
        InitSemaphore(&sem);
        InitSemaphore(&sem2);
        AddTail(&l, &sem.ss_Link);
        AddTail(&l, &sem2.ss_Link);
        ObtainSemaphoreList(&l);
        state("list sem", &sem);
        state("list sem2", &sem2);
        ReleaseSemaphoreList(&l);
        state("released sem", &sem);
        state("released sem2", &sem2);
    }

    P_SECTION("Procure/Vacate");
    {
        struct MsgPort *mp = CreateMsgPort();
        struct SemaphoreMessage sm, sm2;
        struct Message *m;
        InitSemaphore(&sem);
        sm.ssm_Message.mn_ReplyPort = mp;
        sm.ssm_Message.mn_Length = sizeof(sm);
        sm.ssm_Message.mn_Node.ln_Name = NULL;          /* exclusive */
        sm.ssm_Semaphore = NULL;
        Procure(&sem, &sm);
        m = GetMsg(mp);
        P_BOOL("Procure exclusive replied at once", m == &sm.ssm_Message);
        P_BOOL("ssm_Semaphore set", sm.ssm_Semaphore == &sem);
        state("procured", &sem);
        sm2.ssm_Message.mn_ReplyPort = mp;
        sm2.ssm_Message.mn_Length = sizeof(sm2);
        sm2.ssm_Message.mn_Node.ln_Name = (char *)SM_SHARED;
        sm2.ssm_Semaphore = NULL;
        Procure(&sem, &sm2);
        m = GetMsg(mp);
        P_BOOL("Procure shared by owner replied at once", m == &sm2.ssm_Message);
        state("procured twice", &sem);
        if (m)
            Vacate(&sem, &sm2);
        Vacate(&sem, &sm);
        state("vacated", &sem);
        P_NULL("no stray messages", GetMsg(mp));
        DeleteMsgPort(mp);
    }
    return 0;
}
