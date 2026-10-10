/*
 * Probe (Phase 237c): AllocTrap()/FreeTrap() and tc_TrapAlloc - Fred Fish
 * Dk and Cursor ("Could not alloc trap 7") allocate trap numbers.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct Task *me = FindTask(NULL);
    LONG r;

    P_HEX("tc_TrapAlloc at start", me->tc_TrapAlloc);
    r = AllocTrap(7);
    P_LONG("AllocTrap(7)", r);
    r = AllocTrap(7);
    P_LONG("AllocTrap(7) again", r);
    r = AllocTrap(-1);
    P_LONG("AllocTrap(-1)", r);
    r = AllocTrap(-1);
    P_LONG("AllocTrap(-1) again", r);
    r = AllocTrap(15);
    P_LONG("AllocTrap(15)", r);
    P_HEX("tc_TrapAlloc", me->tc_TrapAlloc);
    FreeTrap(7);
    P_HEX("tc_TrapAlloc after FreeTrap(7)", me->tc_TrapAlloc);
    r = AllocTrap(-1);
    P_LONG("AllocTrap(-1) after", r);
    return 0;
}
