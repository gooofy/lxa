/*
 * stub_probe - exercises stub telemetry (Phase 203)
 *
 * Calls a known stub (commodities.library's reserved slot Private1) and an empty
 * exec.library vector (ObtainQuickVector, LVO -786), printing a marker
 * after each call.  Under --strict-unimplemented the run must stop at the
 * first call.
 */

#include <exec/types.h>
#include <exec/libraries.h>
#include <libraries/commodities.h>
#include <clib/exec_protos.h>
#include <clib/commodities_protos.h>
#include <inline/exec.h>
#include <inline/commodities.h>
#include <stdio.h>

extern struct ExecBase *SysBase;
struct Library *CxBase;

/* commodities.library's reserved slot commoditiesPrivate1 (LVO -108) */
static ULONG call_cx_lvo_108(void)
{
    register struct Library *a6 __asm("a6") = CxBase;
    register ULONG d0 __asm("d0") = 0;
    __asm volatile ("jsr -108(a6)" : "+r"(d0) : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

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

    CxBase = OpenLibrary((STRPTR)"commodities.library", 37);
    if (!CxBase)
    {
        printf("stub_probe: no commodities.library\n");
        return 20;
    }

    call_cx_lvo_108();
    printf("stub_probe: after Private1\n");
    CloseLibrary(CxBase);

    call_exec_lvo_786();
    printf("stub_probe: after empty exec vector\n");
    printf("stub_probe: done\n");
    return 0;
}
