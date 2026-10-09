/*
 * Probe (Phase 238): exec functions the autodocs guarantee to "preserve
 * all registers" - Disable, Enable, Forbid, Permit, ObtainSemaphore,
 * ObtainSemaphoreShared, ReleaseSemaphore - really do so for the scratch
 * registers D0/D1/A0/A1 too.  AmiBlitz3's linked-list library keeps its
 * list pointer in D0 across Disable() and its result in D0 across
 * Enable(); without this it fails with "Could not build index cache".
 */
#include <exec/types.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct SignalSemaphore sem;

#define CALL(LVO, SEM)                                                          \
    do {                                                                        \
        register ULONG d0 __asm("d0") = 0x11111111;                            \
        register ULONG d1 __asm("d1") = 0x22222222;                            \
        register APTR a0 __asm("a0") = (SEM) ? (APTR)(SEM) : (APTR)0x33333333; \
        register APTR a1 __asm("a1") = (APTR)0x44444444;                       \
        register struct ExecBase *a6 __asm("a6") = SysBase;                    \
        __asm volatile ("jsr " #LVO "(a6)"                                     \
                        : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1)                \
                        : "r"(a6) : "cc", "memory");                            \
        r[0] = d0; r[1] = d1; r[2] = (ULONG)a0; r[3] = (ULONG)a1;              \
        a0v = (SEM) ? (ULONG)(SEM) : 0x33333333;                               \
    } while (0)

static void report(const char *name, ULONG *r, ULONG a0v)
{
    probe_s(name);
    probe_s(": d0 ");
    probe_s(r[0] == 0x11111111 ? "kept" : "changed");
    probe_s(", d1 ");
    probe_s(r[1] == 0x22222222 ? "kept" : "changed");
    probe_s(", a0 ");
    probe_s(r[2] == a0v ? "kept" : "changed");
    probe_s(", a1 ");
    probe_s(r[3] == 0x44444444 ? "kept" : "changed");
    probe_ch('\n');
}

int main(void)
{
    ULONG r[4], a0v;
    struct SignalSemaphore *s = &sem;

    InitSemaphore(s);

    CALL(-120, 0);
    report("Disable", r, a0v);
    CALL(-126, 0);
    report("Enable", r, a0v);
    CALL(-132, 0);
    report("Forbid", r, a0v);
    CALL(-138, 0);
    report("Permit", r, a0v);
    CALL(-564, s);
    report("ObtainSemaphore", r, a0v);
    CALL(-570, s);
    report("ReleaseSemaphore", r, a0v);
    CALL(-678, s);
    report("ObtainSemaphoreShared", r, a0v);
    CALL(-570, s);
    report("ReleaseSemaphore (shared)", r, a0v);
    /* nested: Enable()/Permit() that end the last nesting level */
    Disable();
    Disable();
    CALL(-126, 0);
    report("Enable (nested)", r, a0v);
    CALL(-126, 0);
    report("Enable (last)", r, a0v);
    Forbid();
    CALL(-138, 0);
    report("Permit (last)", r, a0v);
    return 0;
}
