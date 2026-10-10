/*
 * Probe (Phase 237d): AllocAbs() of a free block in chip RAM, then the
 * block is written and read back, as Fred Fish AntiCicloVir (disk 842)
 * does at $7D000 (its failure on lxa turned out to come from reading past
 * its argument string, see MEMORY_DEPENDENT in tools/rdd/massrun.py).
 * Also: AllocAbs() of memory that is already allocated fails.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static const char text[] = "AntiCicloVir V1.8 by Matthias Gutt";

int main(void)
{
    UBYTE *p = (UBYTE *)0x7d000;
    APTR got;
    int i, ok = 1;

    got = AllocAbs(60, p);
    P_BOOL("AllocAbs($7D000) succeeds", got == p);
    if (got == p) {
        CopyMem((APTR)text, p, sizeof(text));
        for (i = 0; i < (int)sizeof(text); i++)
            if (p[i] != (UBYTE)text[i])
                ok = 0;
        P_BOOL("the copy reads back", ok);
        P_BOOL("AllocAbs() of the same block again fails", AllocAbs(60, p) == NULL);
        FreeMem(p, 60);
    }
    return 0;
}
