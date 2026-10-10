/*
 * Probe (Phase 237d): SetTaskPri() with a task pointer into low memory.
 * Fred Fish Joey (disk 714) calls SetTaskPri(127, 1005) - its arguments
 * swapped - and runs on.  The "task" at 127 overlaps the exception vector
 * table: SetTaskPri() writes its ln_Pri byte (address 136) and returns the
 * old one.  The probe saves and restores the bytes it touches.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static UBYTE peekb(ULONG a)
{
    UBYTE v;
    __asm volatile ("move.b (%1),%0" : "=d" (v) : "a" (a));
    return v;
}

static void pokeb(ULONG a, UBYTE v)
{
    __asm volatile ("move.b %0,(%1)" : : "d" (v), "a" (a) : "memory");
}

int main(void)
{
    UBYTE save[24];
    ULONG i;
    LONG old;
    UBYTE pri;

    Disable();
    for (i = 0; i < 24; i++)
        save[i] = peekb(127 + i);
    old = SetTaskPri((struct Task *)127, 1005);
    pri = peekb(136);
    for (i = 0; i < 24; i++)
        pokeb(127 + i, save[i]);
    Enable();
    P_BOOL("returned the old byte at 136", old == (LONG)save[9]);
    P_HEX("byte at 136 afterwards", pri);
    return 0;
}
