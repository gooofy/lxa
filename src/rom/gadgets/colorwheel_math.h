/*
 * lxa - integer colour and geometry math of colorwheel.gadget
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * Everything here works on 16 bit quantities, like the AmigaOS 3.1 class
 * ("color calculations are currently done using 16-bit math", colorwheel
 * autodoc).  The HSB <-> RGB conversions reproduce the values 3.1 returns
 * bit for bit; doc/gadget-classes.md describes the observed rules and
 * tests/probes/colorwheel/class.c checks them against the reference.
 *
 * The header has no dependencies besides the Amiga integer types, so the
 * same code can be exercised on the host.
 */
#ifndef LXA_GADGETS_COLORWHEEL_MATH_H
#define LXA_GADGETS_COLORWHEEL_MATH_H

/* a 16 bit component as 3.1 hands it out: the value in both words */
#define CWM_WIDEN(v16)      ((ULONG)(v16) | ((ULONG)(v16) << 16))

/* hue units per sector of the hue circle (six sectors: R, Y, G, C, B, M) */
#define CWM_SECTOR          0x2AABUL

/* a * b / 65536 for two 16 bit fractions */
static inline ULONG cwm_frac(ULONG a, ULONG b)
{
    return (a * b) >> 16;
}

/*
 * HSB -> RGB, all values 0..0xFFFF.
 *
 * Inside a sector one channel is at full brightness, one at the floor
 * (brightness scaled by 1 - saturation) and the third moves between the
 * two; which channel does what depends on the sector.  The position in a
 * sector is stretched to 0..0xFFFF with a divisor of one less than the
 * sector width, rounded down from just below the exact value.
 */
enum { CWM_TOP, CWM_UP, CWM_DOWN, CWM_FLOOR };

static const UBYTE cwm_sector_roles[6][3] =
{
    /*  red        green      blue     */
    { CWM_TOP,   CWM_UP,    CWM_FLOOR },    /* red -> yellow    */
    { CWM_DOWN,  CWM_TOP,   CWM_FLOOR },    /* yellow -> green  */
    { CWM_FLOOR, CWM_TOP,   CWM_UP    },    /* green -> cyan    */
    { CWM_FLOOR, CWM_DOWN,  CWM_TOP   },    /* cyan -> blue     */
    { CWM_UP,    CWM_FLOOR, CWM_TOP   },    /* blue -> magenta  */
    { CWM_TOP,   CWM_FLOOR, CWM_DOWN  },    /* magenta -> red   */
};

static inline void cwm_hsb_to_rgb(ULONG hue, ULONG sat, ULONG bri, ULONG out[3])
{
    ULONG level[4];
    ULONG sector, offset, ramp;
    int   c;

    if (!sat)
    {
        out[0] = out[1] = out[2] = bri;
        return;
    }

    sector = hue / CWM_SECTOR;
    offset = hue % CWM_SECTOR;
    ramp   = 0;
    if (offset)
    {
        ramp = ((offset << 16) - 1) / (CWM_SECTOR - 1);
        if (ramp > 0xFFFF)
            ramp = 0xFFFF;
    }

    level[CWM_TOP]   = bri;
    level[CWM_FLOOR] = cwm_frac(bri, 0xFFFF - sat);
    level[CWM_DOWN]  = cwm_frac(bri, 0xFFFF - cwm_frac(sat, ramp));
    level[CWM_UP]    = cwm_frac(bri, 0xFFFF - cwm_frac(sat, 0xFFFF - ramp));

    for (c = 0; c < 3; c++)
        out[c] = level[cwm_sector_roles[sector][c]];
}

/*
 * RGB -> HSB, all values 0..0xFFFF.  Returns FALSE for a grey: its hue is
 * undefined and *hue is not written.
 *
 * Brightness is the largest channel, saturation the spread relative to
 * it.  The hue is measured in sixths of 0xFFFF: the sector starts at the
 * dominant channel (red 0, green 2, blue 4 - red wins ties, then green)
 * and the distance to the neighbouring primary is the difference of the
 * two other channels over the spread, rounded away from zero.
 */
static inline BOOL cwm_rgb_to_hsb(const ULONG in[3], ULONG *hue, ULONG *sat, ULONG *bri)
{
    ULONG hi = in[0], lo = in[0], spread, mag;
    LONG  diff, pos;
    int   dom = 0, c;

    for (c = 1; c < 3; c++)
    {
        if (in[c] > hi)
        {
            hi  = in[c];
            dom = c;
        }
        if (in[c] < lo)
            lo = in[c];
    }
    spread = hi - lo;

    *bri = hi;
    *sat = hi ? spread * 0xFFFF / hi : 0;
    if (!spread)
        return FALSE;

    /* channel after the dominant one minus the channel before it */
    diff = (LONG)in[(dom + 1) % 3] - (LONG)in[(dom + 2) % 3];
    mag  = (ULONG)(diff < 0 ? -diff : diff);
    mag  = (mag * 0xFFFF + spread - 1) / spread;

    pos = (LONG)(dom * 2) * 0xFFFF + (diff < 0 ? -(LONG)mag : (LONG)mag);
    if (pos < 0)
        pos += 6 * 0xFFFF;
    *hue = (ULONG)pos / 6;
    return TRUE;
}

/* floor(sqrt(v)) */
static inline ULONG cwm_isqrt(ULONG v)
{
    ULONG root = 0, bit = 1UL << 30;

    while (bit > v)
        bit >>= 2;
    while (bit)
    {
        if (v >= root + bit)
        {
            v   -= root + bit;
            root = (root >> 1) + bit;
        }
        else
            root >>= 1;
        bit >>= 2;
    }
    return root;
}

/*
 * The hue of a direction on the wheel: 0 straight up (red), increasing
 * clockwise on the screen (x to the right, y down), 0x10000 per turn.
 * atan of the ratio t = small/large in 0..1 is approximated by
 * t/8 + 0.0434 t (1 - t) turns (max. error about 0.1 degree).
 */
static inline ULONG cwm_direction(LONG dx, LONG dy)
{
    ULONG ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    ULONG t, a;

    if (!ax && !ay)
        return 0;

    /* angle from the nearer axis, 0..0x2000 (= 1/8 turn) */
    if (ax <= ay)
        t = (ULONG)(((ULONG)ax << 15) / ay) << 1;
    else
        t = (ULONG)(((ULONG)ay << 15) / ax) << 1;
    if (t > 0x10000)
        t = 0x10000;
    a = (t >> 3) + ((((t >> 4) * (0x10000 - t)) >> 12) * 2846 >> 16);

    if (ax > ay)
        a = 0x4000 - a;             /* measured from the horizontal axis */

    /* a is now the angle from the vertical axis, 0..0x4000: pick the quadrant */
    if (dx >= 0)
        return (dy <= 0) ? a : (0x8000 - a);
    return ((dy > 0) ? (0x8000 + a) : (0x10000 - a)) & 0xFFFF;
}

/*
 * sin of a hue angle (0x10000 per turn), scaled by 4096.  Uses the
 * rational approximation sin(2 pi u) = 64 q / (5 - 16 q), q = u (1/2 - u)
 * for half a turn u in 0..1/2 (max. error about 0.002).
 */
static inline LONG cwm_sin(ULONG angle)
{
    ULONG u = angle & 0x7FFF, q;
    LONG  v;

    q = (u * (0x8000 - u)) >> 12;   /* u (1/2 - u), scaled by 2^20 */
    v = (LONG)((q << 14) / (5UL * 0x10000 - q));
    return (angle & 0x8000) ? -v : v;
}

static inline LONG cwm_cos(ULONG angle)
{
    return cwm_sin(angle + 0x4000);
}

#endif
