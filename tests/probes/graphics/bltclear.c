/*
 * Probe (Phase 237c): BltClear() - byte counts, the row format (flags
 * bit 1), and the fill (flags bit 2: fill with the upper word of flags).
 * Fred Fish CPUClr's SpeedTest (disks 684, 709, 886) calls it 5000 times
 * with all of these.  Each case clears part of a 48 byte chip buffer
 * preset to $55 and dumps the buffer.
 */
#include <graphics/gfx.h>
#include <clib/graphics_protos.h>
#include "gfxprobe.h"

#define N 48

static void t(const char *name, UBYTE *buf, ULONG off, ULONG count, ULONG flags)
{
    int i;
    for (i = 0; i < N; i++)
        buf[i] = 0x55;
    BltClear(buf + off, count, flags);
    WaitBlit();
    probe_s(name);
    probe_s(":");
    for (i = 0; i < N; i++) {
        if (!(i & 15))
            probe_s("\n ");
        probe_ch(' ');
        probe_hex(buf[i], 2);
    }
    probe_ch('\n');
}

int main(void)
{
    UBYTE *buf = AllocMem(N, MEMF_CHIP);
    if (!buf)
        return 20;
    t("bytes 16", buf, 0, 16, 0);
    t("bytes 16 async", buf, 0, 16, 1);
    t("bytes 7 (odd)", buf, 0, 7, 0);
    t("bytes 0", buf, 0, 0, 0);
    t("rows 3 x 4", buf, 0, (3 << 16) | 4, 2);
    t("rows 2 x 5 (odd)", buf, 0, (2 << 16) | 5, 2);
    t("fill $1234 bytes 10", buf, 0, 10, 4 | (0x1234UL << 16));
    t("fill $aaaa async bytes 12", buf, 0, 12, 5 | (0xaaaaUL << 16));
    t("fill $abcd rows 2 x 6", buf, 0, (2 << 16) | 6, 6 | (0xabcdUL << 16));
    t("fill $ffff bytes 8", buf, 0, 8, 4 | (0xffffUL << 16));
    t("flags bit 2, no pattern", buf, 0, 8, 4);
    FreeMem(buf, N);
    return 0;
}
