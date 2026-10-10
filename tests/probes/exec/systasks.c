/*
 * Probe (Phase 244): the system tasks programs look up by name.  SnoopDos
 * finds "input.device" and "ramlib" with FindTask() and raises ramlib's
 * priority while it patches it; without them it takes another path.
 */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void show(const char *name)
{
    struct Task *t;

    Forbid();
    t = FindTask((STRPTR)name);
    probe_s(name);
    if (!t)
        probe_s(": not found\n");
    else
    {
        probe_s(": type ");
        probe_dec(t->tc_Node.ln_Type);
        probe_s(" pri ");
        probe_dec(t->tc_Node.ln_Pri);
        probe_s(" state ");
        probe_dec(t->tc_State);
        probe_ch('\n');
    }
    Permit();
}

int main(void)
{
    show("input.device");
    show("ramlib");
    return 0;
}
