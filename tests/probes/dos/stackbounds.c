/*
 * Probe (Phase 238): the stack a command runs on.  The shell starts a
 * command with RunCommand(): tc_SPLower/tc_SPUpper describe the command's
 * own stack and SP starts at its top.  Programs rely on it: ADPro fills
 * tc_SPLower .. tc_SPUpper-1024 with $ff at startup to measure its stack
 * use, which overwrites live stack frames if SP is lower than that.
 * (lxa's runner started programs on the bootstrap process's stack, below
 * ~6 KB of the bootstrap's own locals.)
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct Task *me = FindTask(NULL);
    ULONG sp;

    __asm volatile ("move.l sp,%0" : "=r"(sp));

    P_SECTION("SP in main() relative to tc_SPLower/tc_SPUpper");
    P_BOOL("SP inside [tc_SPLower, tc_SPUpper]",
           sp > (ULONG)me->tc_SPLower && sp <= (ULONG)me->tc_SPUpper);
    P_BOOL("tc_SPUpper - SP < 1024", (ULONG)me->tc_SPUpper - sp < 1024);
    return 0;
}
