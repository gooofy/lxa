/*
 * Probe (Phase 222a): exec.library CacheControl - the returned previous
 * state, which bits a call can change, and that action/unknown bits never
 * read back as set.  Only EnableI/EnableD are printed: the burst, freeze,
 * write-allocate and copyback bits that accompany them depend on the CPU
 * model (the reference is a 68040, lxa a 68030 until Phase 240).
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define SHOWN (CACRF_EnableI | CACRF_EnableD)
#define NEVER (CACRF_ClearI | CACRF_ClearD | 0x00000004UL | CACRF_EnableE | 0x40000000UL)

static void cc(const char *label, ULONG bits, ULONG mask)
{
    ULONG r = CacheControl(bits, mask);
    ULONG now = CacheControl(0, 0);
    probe_s(label);
    probe_s(": old ");
    probe_hex(r & SHOWN, 4);
    probe_s(" now ");
    probe_hex(now & SHOWN, 4);
    probe_s(now & NEVER ? " (action/unknown bits set)\n" : "\n");
}

int main(void)
{
    ULONG orig = CacheControl(0, 0);

    P_SECTION("CacheControl");
    P_HEX("initial enables", orig & SHOWN);
    P_BOOL("initial action/unknown bits clear", !(orig & NEVER));
    cc("(0,0)", 0, 0);
    cc("clear EnableD", 0, CACRF_EnableD);
    cc("set EnableD", CACRF_EnableD, CACRF_EnableD);
    cc("clear EnableI", 0, CACRF_EnableI);
    cc("set EnableI", CACRF_EnableI, CACRF_EnableI);
    cc("ClearI", CACRF_ClearI, CACRF_ClearI);
    cc("ClearD", CACRF_ClearD, CACRF_ClearD);
    cc("EnableE", CACRF_EnableE, CACRF_EnableE);
    cc("bit 2", 4, 4);
    cc("bit 30", 0x40000000, 0x40000000);
    cc("all bits, mask 0", 0xffffffff, 0);
    cc("no bits, mask without enables", 0, ~(ULONG)SHOWN);
    cc("clear both", 0, SHOWN);
    cc("set EnableD only", CACRF_EnableD | CACRF_EnableI, CACRF_EnableD);
    cc("restore", orig, 0xffffffff);
    P_BOOL("restored", (CacheControl(0, 0) & SHOWN) == (orig & SHOWN));
    return 0;
}
