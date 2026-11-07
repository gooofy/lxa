/*
 * stub_probe - exercises stub telemetry (Phase 203)
 *
 * Calls a known stub (exec.library's Debug(), lxa has no ROM debugger) and
 * an empty exec.library vector (ObtainQuickVector, LVO -786), printing a
 * marker after each call.  Under --strict-unimplemented the run must stop
 * at the first call.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <stdio.h>

extern struct ExecBase *SysBase;

static ULONG call_exec_lvo_786(void)
{
    register struct ExecBase *a6 __asm("a6") = SysBase;
    register ULONG d0 __asm("d0") = 0;
    register APTR a0 __asm("a0") = NULL;
    __asm volatile ("jsr -786(a6)" : "+r"(d0), "+r"(a0) : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}

int main(void)
{
    printf("stub_probe: start\n");

    Debug(0);
    printf("stub_probe: after Debug\n");

    call_exec_lvo_786();
    printf("stub_probe: after empty exec vector\n");
    printf("stub_probe: done\n");
    return 0;
}
