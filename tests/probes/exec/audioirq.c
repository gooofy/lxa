/*
 * Probe (Phase 237c): Paula audio DMA requests its interrupt.  Fred Fish
 * DeluxeBeep (disk 435) starts channels 0 and 1 by hitting the hardware
 * and busy-waits until INTREQR shows both audio interrupt bits.  Checks
 * that a channel requests its interrupt as soon as its DMA is switched
 * on, and again after it has played its whole buffer (AUDxLEN words of
 * two samples, AUDxPER colour clocks each): 20 words at period 500 are
 * 20000 colour clocks, about 88 raster lines.  Switching the DMA off
 * leaves the channel in manual mode, which requests once more when the
 * current word (two samples, 1000 colour clocks) has been played.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define CUSTOM ((volatile struct Custom *)0xdff000)

static ULONG lines_now(void)
{
    /* vertical beam position, 9 bits (V8 in VPOSR, V7-V0 in VHPOSR).  Read
     * both registers as one long and until two reads agree: reading them
     * one after the other straddles the end of a frame now and then (the
     * line then jumps by a whole frame - Phase 237d, the "flaky" probe
     * depended on where in the frame the probe started). */
    volatile ULONG *vpos = (volatile ULONG *)0xdff004;
    ULONG a, b;
    do {
        a = *vpos;
        b = *vpos;
    } while ((a ^ b) & 0x1ff00);
    return (a >> 8) & 0x1ff;
}

/* wait for the AUD0 request; returns raster lines elapsed or -1 */
static LONG wait_aud0(void)
{
    ULONG start = lines_now(), last = start, total = 0, n;
    for (n = 0; n < 2000000; n++) {
        ULONG l = lines_now();
        if (l != last) {
            total += (l > last) ? l - last : l + 313 - last;
            last = l;
        }
        if (CUSTOM->intreqr & INTF_AUD0)
            return (LONG)total;
    }
    return -1;
}

int main(int argc, char **argv)
{
    UBYTE *sample = AllocMem(64, MEMF_CHIP | MEMF_CLEAR);
    LONG first, second, off;
    UWORD intena;
    if (!sample)
        return 20;
    Disable();
    intena = CUSTOM->intenar;
    CUSTOM->intena = INTF_AUD0;                 /* no audio.device handler */
    CUSTOM->dmacon = DMAF_AUD0;
    CUSTOM->intreq = INTF_AUD0;
    CUSTOM->aud[0].ac_ptr = (UWORD *)sample;
    CUSTOM->aud[0].ac_len = 20;
    CUSTOM->aud[0].ac_per = 500;
    CUSTOM->aud[0].ac_vol = 0;
    CUSTOM->dmacon = DMAF_SETCLR | DMAF_MASTER | DMAF_AUD0;
    first = wait_aud0();
    CUSTOM->intreq = INTF_AUD0;
    second = wait_aud0();
    CUSTOM->dmacon = DMAF_AUD0;
    CUSTOM->intreq = INTF_AUD0;
    off = wait_aud0();
    CUSTOM->intreq = INTF_AUD0;
    CUSTOM->intena = INTF_SETCLR | (intena & INTF_AUD0);
    Enable();
    FreeMem(sample, 64);
    P_BOOL("request at DMA start", first >= 0 && first < 4);
    P_BOOL("request after the buffer (70..110 lines)", second >= 70 && second <= 110);
    P_BOOL("one more request after DMA off (2..8 lines)", off >= 2 && off <= 8);
    if (argc > 1) { P_LONG("first", first); P_LONG("second", second); P_LONG("off", off); }
    return 0;
}
