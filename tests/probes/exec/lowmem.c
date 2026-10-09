/*
 * Probe (Phase 234): the long word at address 0 after boot.  SysInfo walks
 * the DOS device list and, with an empty list, dereferences address 0 as a
 * DosList node (its dol_Next must then be 0 to end the walk).
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

/* volatile, so the compiler cannot treat the access as a NULL dereference */
static volatile ULONG g_zero_addr = 0;

int main(void)
{
    volatile ULONG *zero = (volatile ULONG *)g_zero_addr;

    P_SECTION("low memory");
    P_HEX("long at 0", zero[0]);
    P_BOOL("long at 4 is SysBase", zero[1] == (ULONG)SysBase);
    return 0;
}
