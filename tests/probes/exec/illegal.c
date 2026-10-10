/*
 * Probe (Phase 237b): a program's ILLEGAL instruction ($4AFC) takes the
 * illegal-instruction exception, whatever D0 holds.  lxa uses $4AFC with a
 * call number in D0 for its emulator calls; an unknown number used to end
 * the emulator silently (Fred Fish Scrambler executes ILLEGAL on purpose).
 * The task's tc_TrapCode records the exception number and skips the
 * instruction.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

volatile ULONG trap_num;
volatile ULONG trap_count;

/* tc_TrapCode: (sp) = exception number, then the CPU's exception frame */
void trap_handler(void);
__asm(
"_trap_handler:\n"
"    move.l  (sp)+,_trap_num\n"
"    addq.l  #1,_trap_count\n"
"    addq.l  #2,2(sp)\n"            /* skip the 2-byte ILLEGAL */
"    rte\n");

static void illegal_with(ULONG d0val)
{
    register ULONG d0 __asm("d0") = d0val;
    __asm volatile ("illegal" : "+r"(d0) : : "cc", "memory");
}

int main(void)
{
    static const ULONG values[] = {0, 0xdead0000, 99999};
    struct Task *me = FindTask(NULL);
    APTR old = me->tc_TrapCode;
    int i;

    me->tc_TrapCode = (APTR)trap_handler;
    for (i = 0; i < 3; i++) {
        trap_num = 0;
        trap_count = 0;
        illegal_with(values[i]);
        probe_s("ILLEGAL with d0 ");
        probe_hex(values[i], 8);
        probe_s(": trap ");
        probe_dec((LONG)trap_num);
        probe_s(" count ");
        probe_dec((LONG)trap_count);
        probe_ch('\n');
    }
    me->tc_TrapCode = old;
    return 0;
}
