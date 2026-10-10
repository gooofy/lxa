/*
 * Probe (Phase 237d): AllocRemember() with a NULL key.  Fred Fish show
 * (WBLander, disk 114) and LifeCycles (disk 325) call AllocRemember() with
 * a key pointer of NULL and go on only when it returns memory.  Prints the
 * result and whether the Remember node went to address 0 (the key), then
 * frees what was allocated and puts address 0 back.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;

/* address 0 (inline assembler: a C NULL dereference is undefined) */
static struct Remember *get0(void)
{
    struct Remember *r;
    __asm volatile ("move.l 0.w,%0" : "=d" (r));
    return r;
}

static void set0(struct Remember *r)
{
    __asm volatile ("move.l %0,0.w" : : "d" (r) : "memory");
}

int main(void)
{
    struct Remember *old, *rem;
    APTR mem;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase)
        return 20;
    old = get0();
    probe_s("calling AllocRemember(NULL)\n");
    mem = AllocRemember((struct Remember **)0, 30, MEMF_PUBLIC);
    rem = get0();
    set0(old);
    P_BOOL("AllocRemember(NULL, 30) returned memory", mem != NULL);
    P_BOOL("address 0 changed", rem != old);
    if (mem && rem != old) {
        P_BOOL("node at address 0 has the memory", rem->Memory == (UBYTE *)mem);
        P_LONG("node RememberSize", rem->RememberSize);
        P_BOOL("node NextRemember is the old address 0", rem->NextRemember == old);
        FreeMem(rem->Memory, rem->RememberSize);
        FreeMem(rem, sizeof(struct Remember));
    }
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
