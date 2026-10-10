/*
 * Probe (Phase 237e): how long the RastPort blits take.  BltBitMap() costs
 * the caller its blitter time on AmigaOS 3.1 (probe graphics/blittime);
 * this probe measures ClipBlit(), BltBitMapRastPort(), BltMaskBitMapRastPort()
 * and ScrollRaster() on RastPorts without layers the same way: 16 calls,
 * ReadEClock() before and after, the time per call printed as a power-of-two
 * bucket.  Run the probe with any argument to see the raw microseconds.
 */
#include <devices/timer.h>
#include <clib/timer_protos.h>
#include <inline/timer.h>
#include "gfxprobe.h"

struct Device *TimerBase;
static int raw;

static ULONG eclock_us(struct EClockVal *a, struct EClockVal *b, ULONG freq)
{
    ULONG ticks = b->ev_lo - a->ev_lo;
    return ticks * 1000 / (freq / 1000);
}

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
    if (raw) {
        probe_s("  raw us ");
        probe_dec((LONG)us);
        probe_ch('\n');
    }
}

enum { CLIPBLIT, BLTBMRP, BLTMASK, SCROLL };

static void measure(const char *label, int what, WORD w, WORD h, WORD depth)
{
    struct gp_bm a, b;
    struct RastPort ra, rb;
    struct EClockVal t0, t1;
    PLANEPTR mask;
    ULONG freq;
    WORD i, n = 16;

    if (!gp_alloc(&a, w, h, depth) || !gp_alloc(&b, w, h, depth)) {
        P_STR(label, "no memory");
        return;
    }
    mask = AllocRaster(w, h);
    if (!mask) {
        P_STR(label, "no memory");
        return;
    }
    gp_rp(&ra, &a);
    gp_rp(&rb, &b);
    WaitTOF();
    freq = ReadEClock(&t0);
    for (i = 0; i < n; i++) {
        switch (what) {
        case CLIPBLIT:
            ClipBlit(&ra, 0, 0, &rb, (WORD)(i & 1), 0, w - 1, h, 0xc0);
            break;
        case BLTBMRP:
            BltBitMapRastPort(&a.bm, 0, 0, &rb, (WORD)(i & 1), 0, w - 1, h, 0xc0);
            break;
        case BLTMASK:
            BltMaskBitMapRastPort(&a.bm, 0, 0, &rb, (WORD)(i & 1), 0, w - 1, h, 0xe0, mask);
            break;
        case SCROLL:
            ScrollRaster(&rb, 0, 1, 0, 0, w - 1, h - 1);
            break;
        }
    }
    WaitBlit();
    ReadEClock(&t1);
    bucket(label, eclock_us(&t0, &t1, freq) / n);
    FreeRaster(mask, w, h);
    gp_free(&a);
    gp_free(&b);
}

int main(int argc, char **argv)
{
    struct timerequest tr;

    raw = argc > 1;
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_ECLOCK, (struct IORequest *)&tr, 0))
        return 20;
    TimerBase = tr.tr_node.io_Device;

    P_SECTION("RastPort blit duration");
    measure("ClipBlit 320x200x2", CLIPBLIT, 320, 200, 2);
    measure("ClipBlit 64x64x1", CLIPBLIT, 64, 64, 1);
    measure("BltBitMapRastPort 320x200x2", BLTBMRP, 320, 200, 2);
    measure("BltBitMapRastPort 64x64x1", BLTBMRP, 64, 64, 1);
    measure("BltMaskBitMapRastPort 320x200x2", BLTMASK, 320, 200, 2);
    measure("BltMaskBitMapRastPort 64x64x1", BLTMASK, 64, 64, 1);
    measure("ScrollRaster 320x200x2", SCROLL, 320, 200, 2);
    measure("ScrollRaster 64x64x1", SCROLL, 64, 64, 1);

    CloseDevice((struct IORequest *)&tr);
    return 0;
}
