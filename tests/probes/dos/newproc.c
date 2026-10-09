/*
 * Probe (Phase 238): dos.library CreateNewProc() scheduling - a child of
 * higher priority created inside Forbid() must not run before the
 * caller's Permit() (the AmigaOberon runtime creates its processes at
 * priority 127 inside Forbid() and fills in the child's tc_TrapData
 * afterwards); outside Forbid() it runs at once, an equal-priority child
 * does not preempt the caller.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static volatile LONG g_ran;
static volatile LONG g_saw_data;

static void child(void)
{
    struct Task *me = FindTask(NULL);
    g_saw_data = (me->tc_TrapData == (APTR)0x1234) ? 1 : 0;
    g_ran++;
}

static void wait_child(void)
{
    int i;
    for (i = 0; i < 50 && !g_ran; i++)
        Delay(1);
}

int main(void)
{
    struct Process *p;
    LONG mypri = FindTask(NULL)->tc_Node.ln_Pri;
    LONG before, after;

    P_SECTION("priority 127 child inside Forbid()");
    g_ran = 0;
    g_saw_data = -1;
    Forbid();
    p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe child",
                          NP_Priority, 127, NP_StackSize, 4096, TAG_END);
    before = g_ran;     /* no output inside Forbid(): Write() waits */
    if (p)
        p->pr_Task.tc_TrapData = (APTR)0x1234;
    Permit();
    after = g_ran;
    P_LONG("ran before Permit", before);
    P_LONG("ran right after Permit", after);
    wait_child();
    P_LONG("child saw tc_TrapData set after CreateNewProc", g_saw_data);

    P_SECTION("priority 127 child, no Forbid()");
    g_ran = 0;
    p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe child",
                          NP_Priority, 127, NP_StackSize, 4096, TAG_END);
    P_LONG("ran before CreateNewProc returned", g_ran);
    wait_child();

    P_SECTION("lower priority child, no Forbid()");
    g_ran = 0;
    p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe child",
                          NP_Priority, mypri - 1, NP_StackSize, 4096, TAG_END);
    P_LONG("ran before CreateNewProc returned", g_ran);
    wait_child();
    P_LONG("ran after Delay", g_ran);
    return 0;
}
