/*
 * Probe (Phase 222a): exec.library tasks - FindTask, AddTask, RemTask,
 * SetTaskPri, task states, Forbid/Permit and Disable/Enable nesting
 * counters, scheduling order by priority.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"
#include "subtask.h"

extern struct ExecBase *SysBase;

static struct Task *mainTask;
static volatile char order[16];
static volatile int norder;

static void sub_wait(void)
{
    Wait(SIGBREAKF_CTRL_C);
    order[norder++] = 'w';
    Forbid();
    Signal(mainTask, SIGBREAKF_CTRL_F);
}

static void sub_mark(void)
{
    struct Task *t = FindTask(NULL);
    order[norder++] = t->tc_Node.ln_Name[0];
    Forbid();
    Signal(mainTask, SIGBREAKF_CTRL_F);
}

static void sub_forever(void)
{
    Wait(0);
}

static const char *statename(UBYTE s)
{
    switch (s) {
    case TS_INVALID: return "INVALID";
    case TS_ADDED: return "ADDED";
    case TS_RUN: return "RUN";
    case TS_READY: return "READY";
    case TS_WAIT: return "WAIT";
    case TS_EXCEPT: return "EXCEPT";
    case TS_REMOVED: return "REMOVED";
    }
    return "?";
}

int main(void)
{
    struct Task *t, *a, *b, *c;
    BYTE oldpri;
    UBYTE sigstate;
    int sigorder;

    mainTask = FindTask(NULL);
    SetTaskPri(mainTask, 0);         /* the launcher's priority is not part of the probe */

    P_SECTION("FindTask");
    P_BOOL("FindTask(NULL) == ThisTask", mainTask == SysBase->ThisTask);
    P_LONG("ln_Type (process)", mainTask->tc_Node.ln_Type);
    P_STR("tc_State", statename(mainTask->tc_State));
    P_NULL("FindTask(missing)", FindTask((STRPTR)"lxa probe no such task"));

    P_SECTION("SetTaskPri");
    oldpri = SetTaskPri(mainTask, 3);
    P_LONG("SetTaskPri(me, 3) old", oldpri);
    P_LONG("ln_Pri now", mainTask->tc_Node.ln_Pri);
    P_LONG("SetTaskPri(me, -128) old", SetTaskPri(mainTask, -128));
    P_LONG("SetTaskPri(me, 127) old", SetTaskPri(mainTask, 127));
    SetTaskPri(mainTask, oldpri);

    P_SECTION("subtask life cycle");
    SetSignal(0, SIGBREAKF_CTRL_F);
    t = start_task("probewait", sub_wait, 1);
    P_LONG("subtask ln_Type", t->tc_Node.ln_Type);
    P_STR("subtask tc_State after AddTask (higher pri, now waiting)", statename(t->tc_State));
    P_HEX("subtask tc_SigWait", t->tc_SigWait);
    P_HEX("subtask tc_SigAlloc", t->tc_SigAlloc);
    P_BOOL("FindTask(name) finds it", FindTask((STRPTR)"probewait") == t);
    P_LONG("SetTaskPri(sub, -5) old", SetTaskPri(t, -5));
    P_STR("tc_State after SetTaskPri", statename(t->tc_State));
    Signal(t, SIGBREAKF_CTRL_C);
    /* read both before printing: Write() waits for the console, and the
     * lower-priority subtask may run meanwhile */
    sigstate = t->tc_State;
    sigorder = norder;
    P_STR("tc_State after Signal (lower pri)", statename(sigstate));
    P_LONG("subtask has not run yet", sigorder);
    Wait(SIGBREAKF_CTRL_F);
    P_LONG("subtask ran after main waited", norder);
    P_NULL("FindTask(name) after exit", FindTask((STRPTR)"probewait"));
    free_task(t);

    P_SECTION("RemTask of a waiting task");
    t = start_task("probeforever", sub_forever, 0);
    Forbid();
    P_BOOL("FindTask finds it", FindTask((STRPTR)"probeforever") == t);
    Permit();
    RemTask(t);
    P_NULL("FindTask after RemTask", FindTask((STRPTR)"probeforever"));
    free_task(t);

    P_SECTION("scheduling order");
    norder = 0;
    SetSignal(0, SIGBREAKF_CTRL_F);
    Forbid();
    a = start_task("a", sub_mark, -1);
    b = start_task("b", sub_mark, -1);
    c = start_task("c", sub_mark, 5);
    sigstate = a->tc_State;     /* printed after Permit: Write() inside Forbid() would let the subtasks run */
    Permit();
    order[norder++] = 'M';
    SetTaskPri(mainTask, -10);
    while (norder < 4)
        Wait(SIGBREAKF_CTRL_F);
    SetTaskPri(mainTask, oldpri);
    order[norder] = 0;
    P_STR("tc_State of a (Forbid)", statename(sigstate));
    P_STR("run order (c pri 5, main pri 0, a,b pri -1)", (char *)order);
    free_task(a);
    free_task(b);
    free_task(c);

    P_SECTION("Forbid/Disable nesting");
    {
        BYTE td0 = SysBase->TDNestCnt, id0 = SysBase->IDNestCnt, td1, td2, id1, id2;
        Forbid();
        td1 = SysBase->TDNestCnt;
        Forbid();
        td2 = SysBase->TDNestCnt;
        Permit();
        Permit();
        Disable();
        id1 = SysBase->IDNestCnt;
        Disable();
        id2 = SysBase->IDNestCnt;
        Enable();
        Enable();
        P_LONG("TDNestCnt normal", td0);
        P_LONG("TDNestCnt Forbid", td1);
        P_LONG("TDNestCnt Forbid x2", td2);
        P_LONG("TDNestCnt after Permit x2", SysBase->TDNestCnt);
        P_LONG("IDNestCnt normal", id0);
        P_LONG("IDNestCnt Disable", id1);
        P_LONG("IDNestCnt Disable x2", id2);
        P_LONG("IDNestCnt after Enable x2", SysBase->IDNestCnt);
    }
    return 0;
}
