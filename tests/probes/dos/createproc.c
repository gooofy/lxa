/*
 * Probe (Phase 237): does a new process run before CreateNewProc()
 * returns?  Only a child with a higher priority than the caller preempts
 * it; at equal or lower priority the caller continues first.  SAS/C
 * programs that detach with CreateProc() on their own seglist depend on
 * this (Fish JbSpool, ColorSaver: parent and child share one data hunk).
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static volatile LONG ran, done;

static void child(void)
{
    ran = 1;
    done = 1;
}

static void try_pri(const char *label, LONG delta)
{
    struct Task *me = FindTask(NULL);
    struct Process *p;
    LONG seen, n;

    ran = done = 0;
    p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe child",
                          NP_Priority, me->tc_Node.ln_Pri + delta,
                          NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE,
                          TAG_DONE);
    seen = ran;
    for (n = 0; n < 100 && !done; n++)
        Delay(1);
    probe_s(label);
    probe_s(": created ");
    probe_s(p ? "yes" : "no");
    probe_s(", ran before return ");
    probe_s(seen ? "yes" : "no");
    probe_s(", finished ");
    probe_s(done ? "yes" : "no");
    probe_ch('\n');
    Delay(5);      /* let the child's process exit completely */
}

int main(void)
{
    P_SECTION("CreateNewProc NP_Entry");
    try_pri("priority +1", 1);
    try_pri("priority 0", 0);
    try_pri("priority -1", -1);
    return 0;
}
