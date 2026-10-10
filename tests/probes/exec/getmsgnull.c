/*
 * Probe (Phase 237c): GetMsg(NULL) - exec takes the port as it is; the
 * "message list" of a NULL port is the vector table at $14 (vectors 5-7).
 * Fred Fish ParM/BrowserII's handler calls WaitPort(NULL)/GetMsg(NULL)
 * until that list is empty.  The low memory is saved and restored around
 * the calls (the vectors themselves live at the VBR on the reference).
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static volatile ULONG *volatile lowmem_base;

static struct Message *getmsg_null(void)
{
    register struct Message *d0 __asm("d0");
    register APTR a0 __asm("a0") = NULL;
    register struct ExecBase *a6 __asm("a6") = SysBase;
    __asm volatile ("jsr -372(a6)" : "=r"(d0), "+r"(a0) : "r"(a6) : "d1", "a1", "cc", "memory");
    return d0;
}

int main(void)
{
    volatile ULONG *lm = lowmem_base;
    ULONG save[16];
    int i, n = 0;
    struct Message *m;

    Disable();
    for (i = 0; i < 16; i++)
        save[i] = lm[i];
    for (i = 0; i < 8; i++) {
        m = getmsg_null();
        if (!m)
            break;
        n++;
    }
    {
        ULONG head_changed = lm[5] != save[5];
        for (i = 0; i < 16; i++)
            lm[i] = save[i];
        Enable();
        probe_s("GetMsg(NULL) returned messages: ");
        probe_dec(n);
        probe_s(n >= 8 ? " or more" : "");
        probe_ch('\n');
        P_BOOL("the longword at $14 changed", head_changed);
    }
    return 0;
}
