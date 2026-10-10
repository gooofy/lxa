/*
 * Probe (Phase 237b): GetCC() from user mode.  `move.w sr,<ea>` is
 * privileged on the 68010 and later, so on those CPUs exec must not use
 * it: Fred Fish CanDo decks (Division, LAZi, TrueEd, ...) call GetCC() and
 * took a privilege violation on lxa.  The whole D0 is printed, with known
 * values in the condition codes and in D0 before the call.
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static ULONG getcc_with(UWORD ccr, ULONG d0in)
{
    register ULONG d0 __asm("d0") = d0in;
    register ULONG d1 __asm("d1") = ccr;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile ("move.w %1,ccr\n\t"
                    "jsr -528(a6)"
                    : "+r"(d0), "+r"(d1) : "r"(a6) : "a0", "a1", "cc", "memory");
    return d0;
}

int main(void)
{
    static const UWORD ccrs[] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f};
    int i;

    for (i = 0; i < (int)(sizeof(ccrs) / sizeof(ccrs[0])); i++) {
        probe_s("ccr ");
        probe_hex(ccrs[i], 2);
        probe_s(" d0 12345678 -> ");
        probe_hex(getcc_with(ccrs[i], 0x12345678), 8);
        probe_s(" d0 0 -> ");
        probe_hex(getcc_with(ccrs[i], 0), 8);
        probe_ch('\n');
    }
    return 0;
}
