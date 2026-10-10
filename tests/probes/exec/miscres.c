/*
 * Probe (Phase 237b): misc.resource - AllocMiscResource(unit d0, name a1)
 * returns NULL
 * when the unit was free and the current owner's name otherwise;
 * FreeMiscResource() frees it.  Fred Fish voice.library (VCLI) opens it
 * and used it without a check.  Run with an argument to list every
 * resource in SysBase->ResourceList.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <resources/misc.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static char *alloc_misc(struct Library *base, ULONG unit, char *name)
{
    register char *res __asm("d0");
    register ULONG d0 __asm("d0") = unit;
    register char *a1 __asm("a1") = name;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr -6(a6)" : "=r"(res) : "0"(d0), "r"(a1), "r"(a6) : "d1", "a0", "cc", "memory");
    return res;
}

static void free_misc(struct Library *base, ULONG unit)
{
    register ULONG d0 __asm("d0") = unit;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr -12(a6)" : "+r"(d0) : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
}

static void owner(const char *label, char *r)
{
    probe_s(label);
    probe_s(" -> ");
    if (!r)
        probe_s("NULL");
    else {
        probe_ch('"');
        probe_s(r);
        probe_ch('"');
    }
    probe_ch('\n');
}

static char me[] = "probe-a";
static char other[] = "probe-b";

int main(int argc, char **argv)
{
    struct Library *misc = (struct Library *)OpenResource((CONST_STRPTR)MISCNAME);

    if (argc > 1) {
        struct Node *n;
        Forbid();
        for (n = SysBase->ResourceList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
            probe_s("resource ");
            probe_s(n->ln_Name ? n->ln_Name : "(null)");
            probe_ch('\n');
        }
        Permit();
    }
    P_NULL("OpenResource(misc.resource)", misc);
    if (!misc)
        return 0;
    owner("alloc parallel port", alloc_misc(misc, MR_PARALLELPORT, me));
    owner("alloc parallel port again", alloc_misc(misc, MR_PARALLELPORT, other));
    owner("alloc parallel bits", alloc_misc(misc, MR_PARALLELBITS, other));
    free_misc(misc, MR_PARALLELPORT);
    owner("alloc parallel port after free", alloc_misc(misc, MR_PARALLELPORT, other));
    free_misc(misc, MR_PARALLELPORT);
    free_misc(misc, MR_PARALLELBITS);
    return 0;
}
