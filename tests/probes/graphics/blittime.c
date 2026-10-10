/*
 * Probe (Phase 237b): how long BltBitMap() takes.  On AmigaOS 3.1 the
 * blitter does the work and BltBitMap() returns when it is done, so a
 * large blit costs the caller time (Fred Fish Marble-Slide blits the whole
 * screen in a loop and paces itself by it).  lxa copies on the host and
 * charges the blitter's time to the emulated clock.
 *
 * The time per blit is measured with ReadEClock() and printed as a
 * logarithmic bucket (exact values depend on DMA contention); run the probe
 * with any argument to see the raw microseconds.
 */
#include <devices/timer.h>
#include <clib/timer_protos.h>
#include <inline/timer.h>
#include "gfxprobe.h"

struct Device *TimerBase;

static ULONG eclock_us(struct EClockVal *a, struct EClockVal *b, ULONG freq)
{
    /* modular: correct while the difference stays below 2^32 ticks */
    ULONG ticks = b->ev_lo - a->ev_lo;
    return ticks * 1000 / (freq / 1000);
}

/* microseconds per blit -> bucket (powers of two) */
static void bucket(const char *label, ULONG us)
{
    ULONG lo = 16, k = 0;
    while (lo * 2 <= us && k < 20) {
        lo *= 2;
        k++;
    }
    probe_s(label);
    probe_s(": ");
    if (us < 16)
        probe_s("< 16 us");
    else {
        probe_dec((LONG)lo);
        probe_s(" .. ");
        probe_dec((LONG)lo * 2);
        probe_s(" us");
    }
    probe_ch('\n');
}

static void measure(const char *label, WORD w, WORD h, WORD depth, UBYTE minterm, int raw)
{
    struct gp_bm a, b;
    struct EClockVal t0, t1;
    ULONG freq, us;
    WORD i, n = 16;

    if (!gp_alloc(&a, w, h, depth) || !gp_alloc(&b, w, h, depth)) {
        P_STR(label, "no memory");
        return;
    }
    WaitTOF();
    freq = ReadEClock(&t0);
    for (i = 0; i < n; i++)
        BltBitMap(&a.bm, 0, 0, &b.bm, (WORD)(i & 1), 0, w, h, minterm, 0xff, NULL);
    WaitBlit();
    ReadEClock(&t1);
    us = eclock_us(&t0, &t1, freq) / n;
    bucket(label, us);
    if (raw) {
        probe_s("  raw us ");
        probe_dec((LONG)us);
        probe_ch('\n');
    }
    gp_free(&a);
    gp_free(&b);
}

int main(int argc, char **argv)
{
    struct timerequest tr;
    int raw = argc > 1;

    gp_args(argc, argv);
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;

    P_SECTION("BltBitMap duration");
    measure("320x188x5 copy", 320, 188, 5, 0xc0, raw);
    measure("640x256x4 copy", 640, 256, 4, 0xc0, raw);
    measure("320x200x2 invert", 320, 200, 2, 0x30, raw);
    measure("320x200x2 xor", 320, 200, 2, 0x60, raw);
    measure("320x200x2 clear", 320, 200, 2, 0x00, raw);
    measure("64x64x1 copy", 64, 64, 1, 0xc0, raw);

    CloseDevice((struct IORequest *)&tr);
    return 0;
}
