/*
 * Probe (Phase 237d): AllocMem() with unusual requirement flags.  Fred
 * Fish AskFirst and SofTrack (disks 753, 754) call AllocMem(134, ~0) and
 * quit when it fails.  Prints which flag combinations AmigaOS 3.1
 * satisfies.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void t(const char *name, ULONG size, ULONG flags)
{
    UBYTE *p = AllocMem(size, flags);
    ULONG i, clear = 1;
    probe_s(name);
    if (!p) {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": ok");
    {
        ULONG type = TypeOfMem(p);
        probe_s((type & MEMF_CHIP) ? " chip" : " fast");
    }
    if (flags & MEMF_CLEAR) {
        for (i = 0; i < size; i++)
            if (p[i])
                clear = 0;
        probe_s(clear ? " cleared" : " not cleared");
    }
    probe_ch('\n');
    FreeMem(p, size);
}

int main(void)
{
    t("~0", 134, 0xffffffff);
    t("~0 & ~MEMF_TOTAL", 134, 0xffffffff & ~MEMF_TOTAL);
    t("MEMF_CHIP|MEMF_FAST", 134, MEMF_CHIP | MEMF_FAST);
    t("MEMF_CHIP|MEMF_FAST|MEMF_CLEAR", 134, MEMF_CHIP | MEMF_FAST | MEMF_CLEAR);
    t("MEMF_LARGEST", 134, MEMF_LARGEST);
    t("MEMF_TOTAL", 134, MEMF_TOTAL);
    t("MEMF_REVERSE", 134, MEMF_REVERSE);
    t("bit 3 (unused)", 134, 1 << 3);
    t("bit 12 (unused)", 134, 1 << 12);
    t("0x10000 | bit 20", 134, MEMF_CLEAR | (1 << 20));
    t("MEMF_KICK", 134, MEMF_KICK);
    t("MEMF_24BITDMA", 134, MEMF_24BITDMA);
    t("MEMF_LOCAL", 134, MEMF_LOCAL);
    return 0;
}
