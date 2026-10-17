/*
 * lxa_rtg.c - host side of lxa's RTG display support (Phases 241/242).
 *
 * lxa models the `rtg` reference profile: Picasso96 2.0 with one uaegfx
 * board.  This file holds
 *   - the RTG part of the display database (mode IDs, DisplayInfo,
 *     DimensionInfo, MonitorInfo, NameInfo, monitors), whose values were
 *     captured on the reference machine (tests/probes/rtg/modes);
 *   - BestModeIDA() while RTG is enabled;
 *   - all rendering into RTG (chunky) bitmaps: graphics.library primitives,
 *     Picasso96 and CyberGraphX pixel transfers, BltBitMap() between RTG
 *     and planar bitmaps, and the conversion for the host display.
 *
 * The emulated side (src/rom, the Picasso96API/cybergraphics disk
 * libraries) calls it through EMU_CALL_GFX_RTG (see src/include/lxa_rtg.h).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "m68k.h"
#include "util.h"
#include "lxa_rtg.h"
#include "lxa_rtg_host.h"

#include "lxa_memmap.h"

#define INVALID_ID 0xffffffffu

static bool s_enabled_config = false;
static uint32_t s_monitors[19];
static uint32_t s_screen_cm;

/* ------------------------------------------------------------------ */
/* switch                                                              */

void rtg_set_enabled(bool enabled)
{
    s_enabled_config = enabled;
}

bool rtg_enabled(void)
{
    const char *e;

    if (s_enabled_config)
        return true;
    e = getenv("LXA_RTG");
    return e && *e == '1';
}

void rtg_reset(void)
{
    memset(s_monitors, 0, sizeof(s_monitors));
    s_screen_cm = 0;
}

/* ------------------------------------------------------------------ */
/* emulated memory                                                     */

/* host pointer to n bytes of emulated RAM at a (chip RAM or Zorro III
 * fast RAM), NULL if the range is not RAM */
static inline uint8_t *ram_ptr(uint32_t a, uint32_t n)
{
    return n ? lxa_ram_ptr(a, n) : NULL;
}

static inline bool ram_ok(uint32_t a, uint32_t n)
{
    return ram_ptr(a, n) != NULL;
}

static inline uint32_t rd8(uint32_t a)
{
    uint8_t *p = ram_ptr(a, 1);
    return p ? p[0] : 0;
}

static inline uint32_t rd16(uint32_t a)
{
    uint8_t *p = ram_ptr(a, 2);
    return p ? ((uint32_t)p[0] << 8) | p[1] : 0;
}

static inline uint32_t rd32(uint32_t a)
{
    uint8_t *p = ram_ptr(a, 4);
    return p ? ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3] : 0;
}

static inline void wr8(uint32_t a, uint32_t v)
{
    uint8_t *p = ram_ptr(a, 1);
    if (p)
        p[0] = (uint8_t)v;
}

static inline void wr16(uint32_t a, uint32_t v)
{
    uint8_t *p = ram_ptr(a, 2);
    if (p)
    {
        p[0] = (uint8_t)(v >> 8);
        p[1] = (uint8_t)v;
    }
}

static inline void wr32(uint32_t a, uint32_t v)
{
    uint8_t *p = ram_ptr(a, 4);
    if (p)
    {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    }
}

/* ------------------------------------------------------------------ */
/* display database (reference: Picasso96 2.0, uaegfx, `rtg` profile)  */

typedef struct
{
    uint16_t code;      /* bits 16..23 of the mode ID; 0xfff0: the P96Mode entry */
    uint16_t w, h;
    uint8_t rx, ry;     /* Resolution / ViewResolution / MouseTicks */
    uint16_t speed;     /* PixelSpeed */
    uint16_t rows;      /* MonitorInfo.TotalRows */
    uint16_t clocks;    /* MonitorInfo.TotalColorClocks */
} rtg_res_t;

static const rtg_res_t s_res[] = {
    {0x00, 320, 200, 44, 53, 312, 200, 357},
    {0x01, 320, 240, 44, 44, 260, 240, 297},
    {0x09, 320, 256, 44, 41, 244, 256, 279},
    {0xff, 640, 360, 22, 29, 86, 360, 198},
    {0x02, 640, 400, 22, 26, 78, 400, 178},
    {0x03, 640, 480, 22, 22, 65, 480, 148},
    {0x0a, 640, 512, 22, 21, 61, 512, 139},
    {0xfe, 672, 540, 21, 20, 55, 540, 132},
    {0xfd, 800, 450, 18, 23, 55, 450, 158},
    {0xfc, 800, 500, 18, 21, 50, 500, 142},
    {0x04, 800, 600, 18, 18, 41, 600, 119},
    {0xfb, 960, 540, 15, 20, 38, 540, 132},
    {0x8b, 1024, 576, 14, 18, 33, 576, 124},
    {0xfa, 1024, 600, 14, 18, 32, 600, 119},
    {0x05, 1024, 768, 14, 14, 25, 768, 93},
    {0x8e, 1280, 720, 11, 15, 21, 720, 99},
    {0x07, 1280, 1024, 11, 10, 15, 1024, 69},
    {0xf9, 1344, 1080, 10, 10, 13, 1080, 66},
    {0xfff0, 320, 200, 44, 53, 25, 230, 35},    /* uaegfx.card0-P96Mode */
};
#define NUM_RES ((int)(sizeof(s_res) / sizeof(s_res[0])))
#define RES_P96MODE (NUM_RES - 1)

typedef struct
{
    uint16_t suffix;    /* low word of the mode ID */
    uint8_t format;     /* RGBFTYPE */
    uint8_t bpp;
    uint8_t depth;      /* DimensionInfo.MaxDepth */
    uint8_t bits[3];
    uint16_t palette;   /* DisplayInfo.PaletteRange */
    uint16_t maxw, maxh;
    const char *tail;   /* NameInfo suffix */
    uint8_t ida_depth;  /* p96GetModeIDAttr(P96IDA_DEPTH) */
    uint8_t ida_bits;   /* P96IDA_BITSPERPIXEL */
} rtg_fmt_t;

static const rtg_fmt_t s_fmt[] = {
    {0x1000, 1, 1, 8, {8, 8, 8}, 512, 4096, 2048, " 8bit", 8, 8},
    {0x1100, 4, 2, 16, {5, 6, 5}, 150, 2048, 2048, "16bit PC", 16, 16},
    {0x1303, 9, 4, 24, {8, 8, 8}, 512, 1024, 2048, "32bit BGRA", 32, 24},
};
#define NUM_FMT ((int)(sizeof(s_fmt) / sizeof(s_fmt[0])))

#define RTG_PROPS       0x00104380u
#define RTG_NA_P96MODE  0x2000u
#define RTG_BOARD_MEM   (16u * 1024u * 1024u)
#define RTG_COMPATIBLE  13246u

static uint32_t mode_id(int r, int f)
{
    if (r == RES_P96MODE)
        return 0xfff00000u | s_fmt[f].suffix;
    return 0x50000000u | ((uint32_t)s_res[r].code << 16) | s_fmt[f].suffix;
}

/* mode ID -> resolution and format index; false: no RTG mode */
static bool mode_lookup(uint32_t id, int *r, int *f)
{
    int i, j;

    for (j = 0; j < NUM_FMT; j++)
        if ((id & 0xffff) == s_fmt[j].suffix)
            break;
    if (j == NUM_FMT)
        return false;
    for (i = 0; i < NUM_RES; i++)
    {
        if (mode_id(i, j) == id)
        {
            *r = i;
            *f = j;
            return true;
        }
    }
    return false;
}

static void put16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void put_rect(uint8_t *p, int x0, int y0, int x1, int y1)
{
    put16(p, (uint32_t)x0);
    put16(p + 2, (uint32_t)y0);
    put16(p + 4, (uint32_t)x1);
    put16(p + 6, (uint32_t)y1);
}

#define DTAG_DISP 0x80000000u
#define DTAG_DIMS 0x80001000u
#define DTAG_MNTR 0x80002000u
#define DTAG_NAME 0x80003000u
#define TAG_SKIP  3u

static void mode_name(int r, int f, char *out, size_t n)
{
    if (r == RES_P96MODE)
        snprintf(out, n, "uaegfx.card0-P96Mode %s", s_fmt[f].tail);
    else
        snprintf(out, n, "UAE:%4dx%4d %s", s_res[r].w, s_res[r].h, s_fmt[f].tail);
}

static void monitor_name(int r, char *out, size_t n)
{
    if (r == RES_P96MODE)
        snprintf(out, n, "P96-0:uaegfx.card0-P96Mode");
    else
        snprintf(out, n, "P96-0:UAE:%4dx%4d", s_res[r].w, s_res[r].h);
}

static uint32_t mode_info(uint32_t id, uint32_t tag, uint32_t buf, uint32_t size)
{
    uint8_t q[88];
    uint32_t n;
    int r, f;
    const rtg_res_t *rs;

    if (!mode_lookup(id, &r, &f) || !buf || !size)
        return 0;
    rs = &s_res[r];
    memset(q, 0, sizeof(q));
    put32(q, tag);
    put32(q + 4, id);
    put32(q + 8, TAG_SKIP);

    switch (tag)
    {
        case DTAG_DISP:
            n = 48;
            put32(q + 12, 4);
            put16(q + 16, r == RES_P96MODE ? RTG_NA_P96MODE : 0);
            put32(q + 18, RTG_PROPS);
            put16(q + 22, rs->rx);
            put16(q + 24, rs->ry);
            put16(q + 26, rs->speed);
            put16(q + 28, 0);
            put16(q + 30, s_fmt[f].palette);
            put16(q + 32, rs->rx);
            put16(q + 34, rs->ry);
            q[40] = s_fmt[f].bits[0];
            q[41] = s_fmt[f].bits[1];
            q[42] = s_fmt[f].bits[2];
            break;
        case DTAG_DIMS:
            n = 66;
            put32(q + 12, 8);
            put16(q + 16, s_fmt[f].depth);
            put16(q + 18, 16);
            put16(q + 20, 1);
            put16(q + 22, s_fmt[f].maxw);
            put16(q + 24, s_fmt[f].maxh);
            for (int i = 0; i < 5; i++)
                put_rect(q + 26 + i * 8, 0, 0, rs->w - 1, rs->h - 1);
            break;
        case DTAG_MNTR:
            n = 88;
            put32(q + 12, 9);
            put32(q + 16, s_monitors[r]);
            put16(q + 24, rs->rx);
            put16(q + 26, rs->ry);
            put16(q + 36, rs->rows);
            put16(q + 38, rs->clocks);
            put16(q + 42, 0xffff);
            put16(q + 76, rs->rx);
            put16(q + 78, rs->ry);
            put32(q + 84, mode_id(r, 0));
            break;
        case DTAG_NAME:
        {
            char name[40];
            n = 48;
            put32(q + 12, 6);
            mode_name(r, f, name, sizeof(name));
            memcpy(q + 16, name, strlen(name) < 31 ? strlen(name) : 31);
            break;
        }
        default:
            return 0;
    }
    if (n > size)
        n = size;
    if (!ram_ok(buf, n))
        return 0;
    memcpy(ram_ptr(buf, n), q, n);
    return n;
}

static uint32_t mode_next(uint32_t id)
{
    int r, f, i;

    if (id == INVALID_ID)
        return mode_id(0, 0);
    if (!mode_lookup(id, &r, &f))
        return INVALID_ID;
    i = r * NUM_FMT + f + 1;
    if (i >= NUM_RES * NUM_FMT)
        return INVALID_ID;
    return mode_id(i / NUM_FMT, i % NUM_FMT);
}

static uint32_t mode_attr(uint32_t id, uint32_t attr)
{
    int r, f;

    if (!mode_lookup(id, &r, &f))
        return 0;
    switch (attr)
    {
        case 0: return s_res[r].w;
        case 1: return s_res[r].h;
        case 2: return s_fmt[f].ida_depth;
        case 3: return s_fmt[f].bpp;
        case 4: return s_fmt[f].ida_bits;
        case 5: return s_fmt[f].format;
        case 6: return 1;                       /* ISP96 */
        case 7: return 0;                       /* board number */
        case 8: return (uint32_t)s_res[r].w * s_fmt[f].bpp;
        case 10: return RTG_COMPATIBLE;
        default: return 0;
    }
}

/* ------------------------------------------------------------------ */
/* BestModeIDA() with RTG                                               */

#define BIDTAG_DIPFMustHave    0x80000001u
#define BIDTAG_DIPFMustNotHave 0x80000002u
#define BIDTAG_ViewPort        0x80000003u
#define BIDTAG_NominalWidth    0x80000004u
#define BIDTAG_NominalHeight   0x80000005u
#define BIDTAG_DesiredWidth    0x80000006u
#define BIDTAG_DesiredHeight   0x80000007u
#define BIDTAG_Depth           0x80000008u
#define BIDTAG_MonitorID       0x80000009u
#define BIDTAG_SourceID        0x8000000au
#define BIDTAG_RedBits         0x8000000bu
#define BIDTAG_GreenBits       0x8000000cu
#define BIDTAG_BlueBits        0x8000000du

#define DIPF_SPECIAL_FLAGS     0x0000100eu   /* DUALPF | PF2PRI | HAM | EXTRAHALFBRITE */
#define DIPF_IS_FOREIGN        0x80000000u

/* the native modes a query can end on (PAL machine, default flags) */
static const struct
{
    uint32_t id;
    uint16_t w, h;
    uint32_t props;
} s_native[] = {
    {0x00021000, 320, 256, 0x001cebe0},
    {0x00029000, 640, 256, 0x001cebe0},
    {0x00029020, 1280, 256, 0x001cebf0},
    {0x00021004, 320, 512, 0x001cebe1},
    {0x00029004, 640, 512, 0x001cebe1},
    {0x00029024, 1280, 512, 0x001cebf1},
    {0x00021008, 320, 128, 0x001deae0},
};
#define NUM_NATIVE ((int)(sizeof(s_native) / sizeof(s_native[0])))

static uint32_t read_tag(uint32_t *tagp, uint32_t *data)
{
    for (int guard = 0; guard < 4096; guard++)
    {
        uint32_t t = rd32(*tagp);
        uint32_t d = rd32(*tagp + 4);

        switch (t)
        {
            case 0:                     /* TAG_DONE */
                return 0;
            case 1:                     /* TAG_IGNORE */
                *tagp += 8;
                break;
            case 2:                     /* TAG_MORE */
                if (!d)
                    return 0;
                *tagp = d;
                break;
            case 3:                     /* TAG_SKIP */
                *tagp += 8 * (d + 1);
                break;
            default:
                *tagp += 8;
                *data = d;
                return t;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* p96BestModeIDTagList()                                              */

#define P96BID_FORMATSALLOWED   0x80000061u
#define P96BID_FORMATSFORBIDDEN 0x80000062u
#define P96BID_NOMINALWIDTH     0x80000063u
#define P96BID_NOMINALHEIGHT    0x80000064u
#define P96BID_DEPTH            0x80000065u

/*
 * Picasso96 takes the shallowest pixel format that is allowed and holds
 * the depth, then the resolution with the smallest relative size error
 * |w - nw| * 256 / nw + |h - nh| * 256 / nh (integer divisions, the first
 * of equal scores wins); equal to the reference on the probe
 * rtg/p96best grids (Phase 246).
 */
static uint32_t p96_best_mode(uint32_t tags)
{
    uint32_t allowed = 0xffffffffu, forbidden = 0, depth = 8, nw = 1, nh = 1, t, d;
    uint32_t best = INVALID_ID, best_s = 0;
    int fbest = -1;

    while (tags && (t = read_tag(&tags, &d)) != 0)
    {
        switch (t)
        {
            case P96BID_FORMATSALLOWED: allowed = d; break;
            case P96BID_FORMATSFORBIDDEN: forbidden = d; break;
            case P96BID_NOMINALWIDTH: nw = d ? d : 1; break;
            case P96BID_NOMINALHEIGHT: nh = d ? d : 1; break;
            case P96BID_DEPTH: depth = d; break;
            default: break;
        }
    }
    for (int f = 0; f < NUM_FMT; f++)
    {
        uint32_t bit = 1u << s_fmt[f].format;
        if (!(allowed & bit) || (forbidden & bit) || s_fmt[f].ida_depth < depth)
            continue;
        if (fbest < 0 || s_fmt[f].ida_depth < s_fmt[fbest].ida_depth)
            fbest = f;
    }
    if (fbest < 0)
        return INVALID_ID;
    for (int r = 0; r < RES_P96MODE; r++)
    {
        uint32_t w = s_res[r].w, h = s_res[r].h, s;
        s = (w >= nw ? w - nw : nw - w) * 256u / nw + (h >= nh ? h - nh : nh - h) * 256u / nh;
        if (best == INVALID_ID || s < best_s)
        {
            best = mode_id(r, fbest);
            best_s = s;
        }
    }
    return best;
}

/* P96Mode.Description of a board mode */
static uint32_t p96_mode_desc(uint32_t id, uint32_t buf, uint32_t size)
{
    char tmp[64];
    const char *kind;
    int r, f, n;

    if (!mode_lookup(id, &r, &f) || r == RES_P96MODE || !size || !ram_ok(buf, size))
        return 0;
    switch (s_fmt[f].format)
    {
        case 1: kind = "PseudoColor"; break;
        case 9: kind = "TrueColor BGRA"; break;
        default: kind = "HiColor RGBPC"; break;
    }
    n = snprintf(tmp, sizeof(tmp), "uaegfx.card:%4dx%-4d%3d bit %s", s_res[r].w, s_res[r].h,
                 s_fmt[f].ida_depth, kind);
    if (n < 0)
        return 0;
    if ((uint32_t)n >= size)
        n = (int)size - 1;
    memcpy(ram_ptr(buf, (uint32_t)n + 1), tmp, (size_t)n);
    wr8(buf + (uint32_t)n, 0);
    return 1;
}

/* how badly a w x h mode fits the query (lower is better) */
static double best_score(double w, double h, double nw, double nh, double dw, double dh)
{
    double x = log(w / dw), y = log(h / dh);
    double a = fabs(log((nw / nh) / (w / h)));

    return 1.25 * (x < 0 ? -x : 0) + 0.75 * (y > 0 ? y : 0) + 0.5 * (y < 0 ? -y : 0) + 4.0 * a;
}

static uint32_t best_mode(uint32_t tags)
{
    uint32_t must = 0, mustnot = DIPF_SPECIAL_FLAGS, depth = 1, monitor = INVALID_ID;
    uint32_t source = INVALID_ID, nw = 0, nh = 0, dw = 0, dh = 0, t, d;
    uint32_t bits = 4, best = INVALID_ID;
    double best_s = 0;
    int r, f;

    if (!tags)
        return RTG_BEST_CLASSIC;
    while ((t = read_tag(&tags, &d)) != 0)
    {
        switch (t)
        {
            case BIDTAG_DIPFMustHave: must = d; break;
            case BIDTAG_DIPFMustNotHave: mustnot = d; break;
            case BIDTAG_ViewPort: return RTG_BEST_CLASSIC;
            case BIDTAG_NominalWidth: nw = d & 0xffff; break;
            case BIDTAG_NominalHeight: nh = d & 0xffff; break;
            case BIDTAG_DesiredWidth: dw = d & 0xffff; break;
            case BIDTAG_DesiredHeight: dh = d & 0xffff; break;
            case BIDTAG_Depth: depth = d; break;
            case BIDTAG_MonitorID: monitor = d; break;
            case BIDTAG_SourceID: source = d; break;
            case BIDTAG_RedBits:
            case BIDTAG_GreenBits:
            case BIDTAG_BlueBits:
                if (d > bits)
                    bits = d;
                break;
            default: break;
        }
    }

    if (must & DIPF_IS_FOREIGN)
        return INVALID_ID;
    if (depth > 24 || bits > 8)
        return INVALID_ID;
    if (depth == 0)
        depth = 1;

    /* the nominal size: the source mode's, else 640 x 200 */
    if (source != INVALID_ID)
    {
        if (mode_lookup(source, &r, &f))
        {
            if (!nw) nw = s_res[r].w;
            if (!nh) nh = s_res[r].h;
        }
        else
        {
            int i;
            for (i = 0; i < NUM_NATIVE; i++)
                if ((s_native[i].id & 0xffff) == (source & 0xffff))
                    break;
            if (i == NUM_NATIVE)
                return RTG_BEST_CLASSIC;
            if (!nw) nw = s_native[i].w;
            if (!nh) nh = s_native[i].h;
        }
    }
    if (!nw || !nh)
    {
        nw = 640;
        nh = 200;
    }
    if (!dw || !dh)
    {
        dw = nw;
        dh = nh;
    }

    /* a native monitor: the native modes decide (graphics.library) */
    if (monitor != INVALID_ID && !(monitor & 0xf0000000u))
        return RTG_BEST_CLASSIC;
    if (monitor != INVALID_ID && (monitor & ~0xffff1000u))
        return INVALID_ID;
    /* flags the RTG modes cannot have: only native modes qualify */
    if ((must & ~RTG_PROPS) || (mustnot & RTG_PROPS))
        return depth > 8 ? INVALID_ID : RTG_BEST_CLASSIC;

    for (r = 0; r < RES_P96MODE; r++)
    {
        int fi = depth <= 8 ? 0 : depth <= 16 ? 1 : 2;
        double s;

        if (s_res[r].w >= 1024)
            continue;
        if (monitor != INVALID_ID && (mode_id(r, 0) & 0xffff1000u) != monitor)
            continue;
        s = best_score(s_res[r].w, s_res[r].h, nw, nh, dw, dh);
        if (best == INVALID_ID || s < best_s - 1e-9)
        {
            best = mode_id(r, fi);
            best_s = s;
        }
    }
    if (depth <= 8 && monitor == INVALID_ID)
    {
        for (int i = 0; i < NUM_NATIVE; i++)
        {
            double s;

            if ((must & ~s_native[i].props) || (mustnot & s_native[i].props))
                continue;
            s = best_score(s_native[i].w, s_native[i].h, nw, nh, dw, dh);
            if (best == INVALID_ID || s < best_s - 1e-9)
            {
                best = s_native[i].id;
                best_s = s;
            }
        }
    }
    return best;
}

/* ------------------------------------------------------------------ */
/* colours                                                             */

static uint32_t encode_color(uint32_t fmt, uint32_t c)
{
    uint32_t r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff, v;

    switch (fmt)
    {
        case 0:
        case 1: return r;
        case 2:
        case 6: return b << 16 | g << 8 | r;
        case 3:
        case 7: return r << 16 | g << 8 | b;
        case 8: return r << 24 | g << 16 | b << 8;
        case 9: return b << 24 | g << 16 | r << 8;
        case 4:
        case 10:
            v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
            break;
        case 5:
        case 11:
            v = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
            break;
        case 12:
            v = (b >> 3) << 11 | (g >> 2) << 5 | (r >> 3);
            break;
        case 13:
            v = (b >> 3) << 10 | (g >> 3) << 5 | (r >> 3);
            break;
        case 14:
        case 15:
        {
            double y = 0.299 * r + 0.587 * g + 0.114 * b;
            double cr = 128 + 0.5 * r - 0.419 * g - 0.081 * b;
            double cb = 128 - 0.169 * r - 0.331 * g + 0.5 * b;
            uint32_t Y = y < 0 ? 0 : y > 255 ? 255 : (uint32_t)y;
            uint32_t CR = cr < 0 ? 0 : cr > 255 ? 255 : (uint32_t)cr;
            uint32_t CB = cb < 0 ? 0 : cb > 255 ? 255 : (uint32_t)cb;

            if (fmt == 14)
                return Y << 24 | CR << 16 | Y << 8 | CB;
            Y >>= 3;
            return Y << 27 | Y << 22 | Y << 17 | Y << 12 | (CB >> 2) << 6 | (CR >> 2);
        }
        default: return 0;
    }
    if (fmt == 4 || fmt == 5 || fmt == 12 || fmt == 13)
        v = ((v & 0xff) << 8) | (v >> 8);
    return v << 16 | v;
}

/* bytes per pixel of a pixel format (RGBFTYPE or RTG_FMT_CGX_*) */
static uint32_t fmt_bpp(uint32_t fmt)
{
    switch (fmt)
    {
        case 1: return 1;
        case 2: case 3: return 3;
        case 4: case 5: case 10: case 11: case 12: case 13: case 14: case 15: return 2;
        case 6: case 7: case 8: case 9: return 4;
        case RTG_FMT_CGX_RGB: return 3;
        case RTG_FMT_CGX_RGBA: case RTG_FMT_CGX_ARGB: return 4;
        case RTG_FMT_CGX_LUT8: case RTG_FMT_CGX_GREY8: return 1;
        default: return 0;
    }
}

static uint32_t load_raw(uint32_t a, uint32_t bpp)
{
    switch (bpp)
    {
        case 1: return rd8(a);
        case 2: return rd16(a);
        case 3: return (rd8(a) << 16) | (rd8(a + 1) << 8) | rd8(a + 2);
        default: return rd32(a);
    }
}

static void store_raw(uint32_t a, uint32_t bpp, uint32_t v)
{
    switch (bpp)
    {
        case 1: wr8(a, v); break;
        case 2: wr16(a, v); break;
        case 3: wr8(a, v >> 16); wr8(a + 1, v >> 8); wr8(a + 2, v); break;
        default: wr32(a, v); break;
    }
}

/* raw pixel (as loaded big-endian) -> 0xAARRGGBB */
static uint32_t raw_to_argb(uint32_t fmt, uint32_t v)
{
    uint32_t r, g, b, a = 0;

    switch (fmt)
    {
        case 2: case RTG_FMT_CGX_RGB: return v & 0xffffff;
        case 3: return ((v & 0xff) << 16) | (v & 0xff00) | ((v >> 16) & 0xff);
        case 6: case RTG_FMT_CGX_ARGB: return v;
        case 7: return (v & 0xff00ff00) | ((v & 0xff) << 16) | ((v >> 16) & 0xff);
        case 8: case RTG_FMT_CGX_RGBA: return (v >> 8) | (v << 24);
        case 9: return ((v & 0xff) << 24) | ((v >> 8) & 0xff) << 16 | ((v >> 16) & 0xff) << 8 | (v >> 24);
        case RTG_FMT_CGX_GREY8: return v * 0x010101;
        case 4: case 5: case 12: case 13:
            v = ((v & 0xff) << 8) | ((v >> 8) & 0xff);
            break;
        case 10: case 11:
            break;
        default:
            return 0;
    }
    if (fmt == 4 || fmt == 10)
    {
        r = (v >> 11) << 3; g = ((v >> 5) & 0x3f) << 2; b = (v & 0x1f) << 3;
    }
    else if (fmt == 5 || fmt == 11)
    {
        r = ((v >> 10) & 0x1f) << 3; g = ((v >> 5) & 0x1f) << 3; b = (v & 0x1f) << 3;
    }
    else if (fmt == 12)
    {
        b = (v >> 11) << 3; g = ((v >> 5) & 0x3f) << 2; r = (v & 0x1f) << 3;
    }
    else
    {
        b = ((v >> 10) & 0x1f) << 3; g = ((v >> 5) & 0x1f) << 3; r = (v & 0x1f) << 3;
    }
    return a << 24 | r << 16 | g << 8 | b;
}

/* 0xAARRGGBB -> raw pixel to store */
static uint32_t argb_to_raw(uint32_t fmt, uint32_t c)
{
    uint32_t a = c >> 24, r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff, v;

    switch (fmt)
    {
        case 2: case RTG_FMT_CGX_RGB: return c & 0xffffff;
        case 3: return b << 16 | g << 8 | r;
        case 6: case RTG_FMT_CGX_ARGB: return c;
        case 7: return a << 24 | b << 16 | g << 8 | r;
        case 8: case RTG_FMT_CGX_RGBA: return r << 24 | g << 16 | b << 8 | a;
        case 9: return b << 24 | g << 16 | r << 8 | a;
        case RTG_FMT_CGX_GREY8: return (r * 77 + g * 150 + b * 29) >> 8;
        case 4: case 10: v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3); break;
        case 5: case 11: v = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3); break;
        case 12: v = (b >> 3) << 11 | (g >> 2) << 5 | (r >> 3); break;
        case 13: v = (b >> 3) << 10 | (g >> 3) << 5 | (r >> 3); break;
        default: return 0;
    }
    if (fmt == 4 || fmt == 5 || fmt == 12 || fmt == 13)
        v = ((v & 0xff) << 8) | (v >> 8);
    return v;
}

/* a ColorMap's colours (8 bits per gun) */
typedef struct
{
    uint32_t count;
    uint32_t table;
    uint32_t low;
} rtg_cmap_t;

static void cmap_open(uint32_t cm, rtg_cmap_t *c)
{
    memset(c, 0, sizeof(*c));
    if (!cm)
        cm = s_screen_cm;
    if (!cm || !ram_ok(cm, 16))
        return;
    c->count = rd16(cm + 2);
    c->table = rd32(cm + 4);
    if (rd8(cm + 1) > 0)
        c->low = rd32(cm + 12);
}

static uint32_t cmap_rgb(const rtg_cmap_t *c, uint32_t pen)
{
    uint32_t hi, lo, r, g, b;

    if (!c->table || pen >= c->count)
        return 0;
    hi = rd16(c->table + pen * 2);
    r = (hi >> 4) & 0xf0;
    g = hi & 0xf0;
    b = (hi << 4) & 0xf0;
    if (c->low)
    {
        lo = rd16(c->low + pen * 2);
        r |= (lo >> 8) & 0xf;
        g |= (lo >> 4) & 0xf;
        b |= lo & 0xf;
    }
    else
    {
        r |= r >> 4;
        g |= g >> 4;
        b |= b >> 4;
    }
    return r << 16 | g << 8 | b;
}

/* the first pen with exactly this colour, -1 if there is none */
static int32_t cmap_find(const rtg_cmap_t *c, uint32_t rgb)
{
    for (uint32_t i = 0; i < c->count && i < 256; i++)
        if (cmap_rgb(c, i) == (rgb & 0xffffff))
            return (int32_t)i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* RTG bitmaps                                                         */

bool rtg_bitmap_get(uint32_t bm, rtg_bitmap_t *o)
{
    uint32_t pad, mem, ext;

    if (!bm || !ram_ok(bm, 40))
        return false;
    pad = rd16(bm + 6);
    if ((pad & LXA_RTG_PAD_MASK) != LXA_RTG_PAD_TAG)
        return false;
    mem = rd32(bm + 8);
    if (mem < LXA_RTG_EXT_SIZE || !ram_ok(mem - LXA_RTG_EXT_SIZE, LXA_RTG_EXT_SIZE))
        return false;
    ext = mem - LXA_RTG_EXT_SIZE;
    if (rd32(ext + LXA_RTG_EXT_MAGIC) != LXA_RTG_MAGIC)
        return false;
    o->bm = bm;
    o->memory = mem;
    o->bpr = rd16(bm);
    o->height = rd16(bm + 2);
    o->format = rd32(ext + LXA_RTG_EXT_FORMAT);
    o->bpp = rd32(ext + LXA_RTG_EXT_BPP);
    if (!o->bpp)
        return false;
    o->width = o->bpr / o->bpp;
    o->cmap = rd32(ext + LXA_RTG_EXT_CMAP);
    o->depth = rd8(bm + 5);
    if (!ram_ok(mem, o->bpr * o->height))
        return false;
    return true;
}

static inline uint32_t px_addr(const rtg_bitmap_t *b, int x, int y)
{
    return b->memory + (uint32_t)y * b->bpr + (uint32_t)x * b->bpp;
}

/* all bits of a pixel, the colour bits only */
static uint32_t px_allbits(const rtg_bitmap_t *b)
{
    if (b->format == 1)
        return b->depth >= 8 ? 0xff : (1u << b->depth) - 1;
    if (b->bpp >= 4)
        return 0xffffffffu;
    return (1u << (b->bpp * 8)) - 1;
}

static uint32_t px_colourbits(const rtg_bitmap_t *b)
{
    switch (b->format)
    {
        case 6: case 7: return 0x00ffffffu;
        case 8: case 9: return 0xffffff00u;
        default: return px_allbits(b);
    }
}

/* the raw value of pen in this bitmap */
static uint32_t pen_raw(const rtg_bitmap_t *b, const rtg_cmap_t *c, uint32_t pen)
{
    if (b->format == 1)
        return pen & 0xff;
    return argb_to_raw(b->format, cmap_rgb(c, pen & 0xff));
}

/* clip x,y,w,h to the bitmap; false if nothing is left */
static bool clip_rect(const rtg_bitmap_t *b, int32_t *x, int32_t *y, int32_t *w, int32_t *h,
                      int32_t *sx, int32_t *sy)
{
    if (*x < 0) { if (sx) *sx -= *x; *w += *x; *x = 0; }
    if (*y < 0) { if (sy) *sy -= *y; *h += *y; *y = 0; }
    if (*x + *w > (int32_t)b->width) *w = (int32_t)b->width - *x;
    if (*y + *h > (int32_t)b->height) *h = (int32_t)b->height - *y;
    return *w > 0 && *h > 0;
}

#define DM_JAM1       0
#define DM_JAM2       1
#define DM_COMPLEMENT 2
#define DM_INVERSVID  4

/* draw one pixel: on = foreground (pattern/template bit) */
typedef struct
{
    rtg_bitmap_t b;
    rtg_cmap_t cm;
    uint32_t fg_raw, bg_raw;
    uint32_t mode;
    uint32_t mask;      /* CLUT write mask */
    uint32_t xorbits;
} rtg_pen_t;

static void pen_setup(rtg_pen_t *p, uint32_t fg, uint32_t bg, uint32_t mode, uint32_t cm)
{
    cmap_open(cm ? cm : p->b.cmap, &p->cm);
    p->fg_raw = pen_raw(&p->b, &p->cm, fg);
    p->bg_raw = pen_raw(&p->b, &p->cm, bg);
    p->mode = mode;
    p->mask = p->b.format == 1 ? (mode >> 8) & 0xff : 0xffffffffu;
    p->xorbits = (mode & RTGM_PIXEL) ? px_colourbits(&p->b) : px_allbits(&p->b);
    if (p->b.format == 1)
        p->xorbits &= p->mask;
}

static inline void pen_put(const rtg_pen_t *p, uint32_t a, uint32_t raw)
{
    if (p->b.format == 1)
    {
        uint32_t old = rd8(a);
        wr8(a, (old & ~p->mask) | (raw & p->mask));
    }
    else
        store_raw(a, p->b.bpp, raw);
}

/* on: 1 foreground, 0 background (drawn by JAM2 only) */
static inline void pen_plot(const rtg_pen_t *p, uint32_t a, int on)
{
    uint32_t dm = p->mode & 0xff;

    if (dm & DM_INVERSVID)
        on = !on;
    if (dm & DM_COMPLEMENT)
    {
        if (on)
            store_raw(a, p->b.bpp, load_raw(a, p->b.bpp) ^ p->xorbits);
        return;
    }
    if (on)
        pen_put(p, a, p->fg_raw);
    else if (dm & DM_JAM2)
        pen_put(p, a, p->bg_raw);
}

typedef struct
{
    uint32_t bm;
    int32_t x, y, w, h;
    uint32_t fg, bg, mode, cm, src;
    int32_t srcmod, srcx, srcy;
    uint32_t fmt, extra;
} rtg_args_t;

static void args_read(uint32_t a, rtg_args_t *o)
{
    o->bm = rd32(a + RTGA_BM);
    o->x = (int32_t)rd32(a + RTGA_X);
    o->y = (int32_t)rd32(a + RTGA_Y);
    o->w = (int32_t)rd32(a + RTGA_W);
    o->h = (int32_t)rd32(a + RTGA_H);
    o->fg = rd32(a + RTGA_FG);
    o->bg = rd32(a + RTGA_BG);
    o->mode = rd32(a + RTGA_MODE);
    o->cm = rd32(a + RTGA_CMAP);
    o->src = rd32(a + RTGA_SRC);
    o->srcmod = (int32_t)rd32(a + RTGA_SRCMOD);
    o->srcx = (int32_t)rd32(a + RTGA_SRCX);
    o->srcy = (int32_t)rd32(a + RTGA_SRCY);
    o->fmt = rd32(a + RTGA_FMT);
    o->extra = rd32(a + RTGA_EXTRA);
}

/* pen of a pixel: CLUT value or the first pen with its colour */
static int32_t px_pen(const rtg_bitmap_t *b, const rtg_cmap_t *c, int x, int y)
{
    uint32_t raw = load_raw(px_addr(b, x, y), b->bpp);

    if (b->format == 1)
        return (int32_t)raw;
    return cmap_find(c, raw_to_argb(b->format, raw));
}

static uint32_t px_rgb(const rtg_bitmap_t *b, const rtg_cmap_t *c, int x, int y)
{
    uint32_t raw = load_raw(px_addr(b, x, y), b->bpp);

    if (b->format == 1)
        return cmap_rgb(c, raw);
    return raw_to_argb(b->format, raw) & 0xffffff;
}

static uint32_t render(uint32_t op, uint32_t argp)
{
    rtg_args_t a;
    rtg_pen_t p;
    int32_t sx, sy;

    args_read(argp, &a);
    if (!rtg_bitmap_get(a.bm, &p.b))
        return 0;
    sx = a.srcx;
    sy = a.srcy;

    switch (op)
    {
        case RTG_R_FILL:
        case RTG_R_WRITEPIXEL:
            if (op == RTG_R_WRITEPIXEL)
                a.w = a.h = 1;
            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, NULL, NULL))
                return 0;
            pen_setup(&p, a.fg, a.bg, a.mode | (op == RTG_R_WRITEPIXEL ? RTGM_PIXEL : 0), a.cm);
            p.mode &= ~(uint32_t)DM_INVERSVID;
            for (int32_t y = 0; y < a.h; y++)
            {
                uint32_t row = px_addr(&p.b, a.x, a.y + y);
                for (int32_t x = 0; x < a.w; x++)
                    pen_plot(&p, row + (uint32_t)x * p.b.bpp, 1);
            }
            return 1;

        case RTG_R_TEMPLATE:
            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, &sx, &sy))
                return 0;
            pen_setup(&p, a.fg, a.bg, a.mode, a.cm);
            for (int32_t y = 0; y < a.h; y++)
            {
                uint32_t row = px_addr(&p.b, a.x, a.y + y);
                uint32_t trow = a.src + (uint32_t)((sy - a.srcy + y) * a.srcmod);
                for (int32_t x = 0; x < a.w; x++)
                {
                    int32_t bit = sx + x;
                    int on = (rd8(trow + (uint32_t)(bit >> 3)) >> (7 - (bit & 7))) & 1;
                    pen_plot(&p, row + (uint32_t)x * p.b.bpp, on);
                }
            }
            return 1;

        case RTG_R_PATTERN:
        {
            /* src: AreaPtrn, srcmod: AreaPtSz, srcx/srcy: pattern origin */
            int multi = a.srcmod < 0;
            int32_t rows = 1 << ((multi ? -a.srcmod : a.srcmod) & 15);
            int planes = p.b.format == 1 ? (p.b.depth > 8 ? 8 : p.b.depth) : 8;

            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, NULL, NULL))
                return 0;
            pen_setup(&p, a.fg, a.bg, a.mode, a.cm);
            for (int32_t y = 0; y < a.h; y++)
            {
                int32_t prow = (a.y + y + a.srcy) & (rows - 1);
                uint32_t row = px_addr(&p.b, a.x, a.y + y);
                for (int32_t x = 0; x < a.w; x++)
                {
                    int bit = 15 - ((a.x + x + a.srcx) & 15);
                    uint32_t addr = row + (uint32_t)x * p.b.bpp;

                    if (multi && !(p.mode & DM_COMPLEMENT))
                    {
                        uint32_t pen = 0;
                        for (int pl = 0; pl < planes; pl++)
                            if ((rd16(a.src + (uint32_t)((pl * rows + prow) * 2)) >> bit) & 1)
                                pen |= 1u << pl;
                        pen_put(&p, addr, pen_raw(&p.b, &p.cm, pen));
                    }
                    else
                        pen_plot(&p, addr, (rd16(a.src + (uint32_t)prow * 2) >> bit) & 1);
                }
            }
            return 1;
        }

        case RTG_R_READPIXEL:
            if (a.x < 0 || a.y < 0 || a.x >= (int32_t)p.b.width || a.y >= (int32_t)p.b.height)
                return 0xffffffffu;
            cmap_open(a.cm ? a.cm : p.b.cmap, &p.cm);
            return (uint32_t)px_pen(&p.b, &p.cm, a.x, a.y);

        case RTG_R_READPIXELRGB:
            if (a.x < 0 || a.y < 0 || a.x >= (int32_t)p.b.width || a.y >= (int32_t)p.b.height)
                return 0xffffffffu;
            if ((a.mode & RTGM_ALPHA) && p.b.format >= 6 && p.b.format <= 9)
                return raw_to_argb(p.b.format, load_raw(px_addr(&p.b, a.x, a.y), p.b.bpp));
            cmap_open(a.cm ? a.cm : p.b.cmap, &p.cm);
            return px_rgb(&p.b, &p.cm, a.x, a.y);

        case RTG_R_WRITEPIXELRGB:
            if (a.x < 0 || a.y < 0 || a.x >= (int32_t)p.b.width || a.y >= (int32_t)p.b.height)
                return 0;
            store_raw(px_addr(&p.b, a.x, a.y), p.b.bpp,
                      p.b.format == 1 ? (a.fg & 0xff) : argb_to_raw(p.b.format, a.fg));
            return 1;

        case RTG_R_FILLRGB:
        case RTG_R_INVERT:
        {
            uint32_t raw = p.b.format == 1 ? (a.fg & 0xff) : argb_to_raw(p.b.format, a.fg);
            uint32_t all = p.b.bpp >= 4 ? 0xffffffffu : (1u << (p.b.bpp * 8)) - 1;

            if (a.mode & RTGM_PIXEL)    /* CGX InvertPixelArray: not the alpha byte */
                all = px_colourbits(&p.b);

            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, NULL, NULL))
                return 0;
            for (int32_t y = 0; y < a.h; y++)
            {
                uint32_t row = px_addr(&p.b, a.x, a.y + y);
                for (int32_t x = 0; x < a.w; x++)
                {
                    uint32_t ad = row + (uint32_t)x * p.b.bpp;
                    store_raw(ad, p.b.bpp, op == RTG_R_INVERT ? load_raw(ad, p.b.bpp) ^ all : raw);
                }
            }
            return 1;
        }

        case RTG_R_WRITE8:
        case RTG_R_READ8:
        {
            uint32_t m = (a.mode & RTGM_ARRAYMASK) && p.b.format == 1 ? px_allbits(&p.b) : 0xff;
            int32_t ox = a.x, oy = a.y;

            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, NULL, NULL))
                return 0;
            cmap_open(a.cm ? a.cm : p.b.cmap, &p.cm);
            for (int32_t y = 0; y < a.h; y++)
            {
                uint32_t arow = a.src + (uint32_t)((a.y - oy + y) * a.srcmod) + (uint32_t)(a.x - ox);
                for (int32_t x = 0; x < a.w; x++)
                {
                    if (op == RTG_R_WRITE8)
                        store_raw(px_addr(&p.b, a.x + x, a.y + y), p.b.bpp,
                                  pen_raw(&p.b, &p.cm, rd8(arow + (uint32_t)x)));
                    else
                    {
                        int32_t pen = px_pen(&p.b, &p.cm, a.x + x, a.y + y);
                        wr8(arow + (uint32_t)x, pen < 0 ? 0 : (uint32_t)pen & m);
                    }
                }
            }
            return (uint32_t)(a.w * a.h);
        }

        case RTG_R_WRITERGB:
        case RTG_R_READRGB:
        case RTG_R_WRITELUT:
        case RTG_R_ALPHA:
        {
            uint32_t sbpp = op == RTG_R_WRITELUT ? 1 : fmt_bpp(a.fmt);

            if (!sbpp)
                return 0;
            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, &sx, &sy))
                return 0;
            cmap_open(a.cm ? a.cm : p.b.cmap, &p.cm);
            for (int32_t y = 0; y < a.h; y++)
            {
                uint32_t srow = a.src + (uint32_t)((sy + y) * a.srcmod) + (uint32_t)sx * sbpp;
                uint32_t drow = px_addr(&p.b, a.x, a.y + y);
                for (int32_t x = 0; x < a.w; x++)
                {
                    uint32_t sa = srow + (uint32_t)x * sbpp, da = drow + (uint32_t)x * p.b.bpp;

                    if (op == RTG_R_READRGB)
                    {
                        uint32_t raw = load_raw(da, p.b.bpp), v;
                        if (a.fmt == RTG_FMT_CGX_LUT8 || (a.fmt == 1 && p.b.format == 1))
                            v = p.b.format == 1 ? raw : (uint32_t)(px_pen(&p.b, &p.cm, a.x + x, a.y + y) & 0xff);
                        else if (p.b.format == 1)
                            v = argb_to_raw(a.fmt, cmap_rgb(&p.cm, raw) | 0xff000000u);
                        else if (a.fmt == p.b.format)
                            v = raw;
                        else
                            v = argb_to_raw(a.fmt, raw_to_argb(p.b.format, raw) |
                                            ((p.b.format >= 6 && p.b.format <= 9) || (a.mode & RTGM_ALPHA) ?
                                             0 : 0xff000000u));
                        store_raw(sa, sbpp, v);
                    }
                    else
                    {
                        uint32_t sv = load_raw(sa, sbpp), c;

                        if (op == RTG_R_WRITELUT)
                            c = a.extra ? rd32(a.extra + sv * 4) : 0;
                        else if (a.fmt == RTG_FMT_CGX_LUT8 || a.fmt == 1)
                        {
                            if (p.b.format == 1)
                            {
                                store_raw(da, 1, sv);
                                continue;
                            }
                            c = cmap_rgb(&p.cm, sv);
                        }
                        else if (a.fmt == p.b.format)
                        {
                            store_raw(da, p.b.bpp, sv);
                            continue;
                        }
                        else
                            c = raw_to_argb(a.fmt, sv);
                        if (op == RTG_R_ALPHA)
                        {
                            uint32_t al = (c >> 24) * (a.extra >> 24) / 255, d = px_rgb(&p.b, &p.cm, a.x + x, a.y + y);
                            uint32_t r = (((c >> 16) & 0xff) * al + ((d >> 16) & 0xff) * (255 - al)) / 255;
                            uint32_t g = (((c >> 8) & 0xff) * al + ((d >> 8) & 0xff) * (255 - al)) / 255;
                            uint32_t bb = ((c & 0xff) * al + (d & 0xff) * (255 - al)) / 255;
                            c = r << 16 | g << 8 | bb;
                        }
                        if (p.b.format == 1)
                        {
                            int32_t pen = cmap_find(&p.cm, c);
                            store_raw(da, 1, pen < 0 ? 0 : (uint32_t)pen);
                        }
                        else
                            store_raw(da, p.b.bpp, argb_to_raw(p.b.format, op == RTG_R_ALPHA ? c & 0x00ffffffu : c));
                    }
                }
            }
            return (uint32_t)(a.w * a.h);
        }

        case RTG_R_WRITETC:
        case RTG_R_READTC:
        {
            /* struct TrueColorInfo: PixelDistance, BytesPerRow, Red/Green/BlueData */
            uint32_t dist = rd32(a.extra), bpr = rd32(a.extra + 4);
            uint32_t rp = rd32(a.extra + 8), gp = rd32(a.extra + 12), bp = rd32(a.extra + 16);

            if (!clip_rect(&p.b, &a.x, &a.y, &a.w, &a.h, &sx, &sy))
                return 0;
            cmap_open(a.cm ? a.cm : p.b.cmap, &p.cm);
            for (int32_t y = 0; y < a.h; y++)
            {
                for (int32_t x = 0; x < a.w; x++)
                {
                    uint32_t off = (uint32_t)(sy + y) * bpr + (uint32_t)(sx + x) * dist;
                    uint32_t da = px_addr(&p.b, a.x + x, a.y + y);

                    if (op == RTG_R_READTC)
                    {
                        uint32_t c = px_rgb(&p.b, &p.cm, a.x + x, a.y + y);
                        wr8(rp + off, c >> 16);
                        wr8(gp + off, c >> 8);
                        wr8(bp + off, c);
                    }
                    else
                    {
                        uint32_t c = rd8(rp + off) << 16 | rd8(gp + off) << 8 | rd8(bp + off);
                        if (p.b.format == 1)
                        {
                            int32_t pen = cmap_find(&p.cm, c);
                            store_raw(da, 1, pen < 0 ? 0 : (uint32_t)pen);
                        }
                        else
                            store_raw(da, p.b.bpp, argb_to_raw(p.b.format, c));
                    }
                }
            }
            return 1;
        }

        default:
            return 0;
    }
}

/* ------------------------------------------------------------------ */
/* BltBitMap()                                                          */

static inline uint32_t minterm_apply(uint8_t m, uint32_t s, uint32_t d)
{
    uint32_t r = 0;

    if (m & 0x80) r |= s & d;
    if (m & 0x40) r |= s & ~d;
    if (m & 0x20) r |= ~s & d;
    if (m & 0x10) r |= ~s & ~d;
    return r;
}

/* BltMaskBitMapRastPort() as 3.1 renders it (see the planar blitter in
 * lxa_dispatch.c): T = S ^ D; M=1: ~((m80 & ~T) | (m40 & T)), M=0: the same
 * with m20/m10; the result XOR S */
static inline uint32_t blt_masked(uint8_t m, uint32_t mb, uint32_t s, uint32_t d)
{
    uint32_t t = s ^ d;
    uint32_t hi = (m & (mb ? 0x80 : 0x20)) ? 0xffffffffu : 0;
    uint32_t lo = (m & (mb ? 0x40 : 0x10)) ? 0xffffffffu : 0;

    return ~((hi & ~t) | (lo & t)) ^ s;
}

static uint32_t planar_pen(uint32_t bm, int depth, int x, int y)
{
    uint32_t bpr = rd16(bm), pen = 0;

    for (int p = 0; p < depth && p < 8; p++)
    {
        uint32_t plane = rd32(bm + 8 + (uint32_t)p * 4);
        if (plane == 0xffffffffu)
            pen |= 1u << p;
        else if (plane && ((rd8(plane + (uint32_t)y * bpr + (uint32_t)(x >> 3)) >> (7 - (x & 7))) & 1))
            pen |= 1u << p;
    }
    return pen;
}

bool rtg_blt_bitmap(uint32_t src_bm, int sx, int sy, uint32_t dst_bm, int dx, int dy,
                    int w, int h, uint8_t minterm, uint8_t mask, uint32_t pmask,
                    uint32_t pmask_bpr, int mask_mode, uint32_t *planes_affected)
{
    rtg_bitmap_t s, d;
    bool s_rtg = rtg_bitmap_get(src_bm, &s);
    bool d_rtg = rtg_bitmap_get(dst_bm, &d);
    int sw, shh, dw, dh, sdepth, ddepth;
    rtg_cmap_t scm, dcm;
    uint32_t *tmp;

    if (!s_rtg && !d_rtg)
        return false;
    *planes_affected = 0;

    sw = s_rtg ? (int)s.width : (int)rd16(src_bm) * 8;
    shh = s_rtg ? (int)s.height : (int)rd16(src_bm + 2);
    dw = d_rtg ? (int)d.width : (int)rd16(dst_bm) * 8;
    dh = d_rtg ? (int)d.height : (int)rd16(dst_bm + 2);
    sdepth = rd8(src_bm + 5);
    ddepth = rd8(dst_bm + 5);

    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (sx + w > sw) w = sw - sx;
    if (sy + h > shh) h = shh - sy;
    if (dx + w > dw) w = dw - dx;
    if (dy + h > dh) h = dh - dy;
    if (w <= 0 || h <= 0)
        return true;

    cmap_open(s_rtg ? s.cmap : 0, &scm);
    cmap_open(d_rtg ? d.cmap : 0, &dcm);
    if (d_rtg && !d.cmap && s_rtg)
        dcm = scm;

    /* read the source first: source and destination may overlap */
    tmp = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t) * 2);
    if (!tmp)
        return true;
    /* the pixel mask (BltMaskBitMapRastPort), aligned with the source */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            uint32_t mb = 1;
            if (pmask)
            {
                int bx = sx + x;
                mb = (rd8(pmask + (uint32_t)(sy + y) * pmask_bpr + (uint32_t)(bx >> 3)) >> (7 - (bx & 7))) & 1;
            }
            tmp[(size_t)w * (size_t)h + (size_t)y * (size_t)w + (size_t)x] = mb;
        }
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            uint32_t v;

            if (!s_rtg)
            {
                uint32_t pen = planar_pen(src_bm, sdepth, sx + x, sy + y);
                v = d.format == 1 ? pen : argb_to_raw(d.format, cmap_rgb(&dcm, pen));
            }
            else
            {
                uint32_t raw = load_raw(px_addr(&s, sx + x, sy + y), s.bpp);
                if (!d_rtg)
                    v = s.format == 1 ? raw : 0;
                else if (s.format == d.format)
                    v = raw;
                else if (d.format == 1)
                {
                    int32_t pen = cmap_find(&dcm, s.format == 1 ? cmap_rgb(&scm, raw)
                                                              : raw_to_argb(s.format, raw));
                    v = pen < 0 ? 0 : (uint32_t)pen;
                }
                else
                    v = argb_to_raw(d.format, (s.format == 1 ? cmap_rgb(&scm, raw)
                                                             : raw_to_argb(s.format, raw)) & 0xffffff);
            }
            tmp[(size_t)y * (size_t)w + (size_t)x] = v;
        }
    }

    if (d_rtg)
    {
        uint32_t m = d.format == 1 ? mask : 0xffffffffu;
        uint32_t all = d.bpp >= 4 ? 0xffffffffu : (1u << (d.bpp * 8)) - 1;

        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                uint32_t a = px_addr(&d, dx + x, dy + y);
                uint32_t old = load_raw(a, d.bpp);
                uint32_t sv = tmp[(size_t)y * (size_t)w + (size_t)x];
                uint32_t mb = tmp[(size_t)w * (size_t)h + (size_t)y * (size_t)w + (size_t)x];
                uint32_t r;

                if (!mb && !mask_mode)
                    continue;
                r = mask_mode ? blt_masked(minterm, mb, sv, old) : minterm_apply(minterm, sv, old);
                r &= all;
                store_raw(a, d.bpp, (old & ~m) | (r & m));
            }
        }
        *planes_affected = d.format == 1 ? d.depth : 8;
    }
    else
    {
        uint32_t bpr = rd16(dst_bm);

        for (int p = 0; p < ddepth && p < 8; p++)
        {
            uint32_t plane = rd32(dst_bm + 8 + (uint32_t)p * 4);

            if (!(mask & (1u << p)) || !plane || plane == 0xffffffffu)
                continue;
            (*planes_affected)++;
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    uint32_t a = plane + (uint32_t)(dy + y) * bpr + (uint32_t)((dx + x) >> 3);
                    uint32_t bit = 0x80u >> ((dx + x) & 7);
                    uint32_t sb = (tmp[(size_t)y * (size_t)w + (size_t)x] >> p) & 1;
                    uint32_t db = (rd8(a) & bit) ? 1 : 0;
                    uint32_t mb = tmp[(size_t)w * (size_t)h + (size_t)y * (size_t)w + (size_t)x];
                    uint32_t r;

                    if (!mb && !mask_mode)
                        continue;
                    r = (mask_mode ? blt_masked(minterm, mb, sb, db) : minterm_apply(minterm, sb, db)) & 1;
                    wr8(a, r ? (rd8(a) | bit) : (rd8(a) & ~bit));
                }
            }
        }
    }
    free(tmp);
    return true;
}

/* ------------------------------------------------------------------ */
/* host display                                                        */

void rtg_read_row_pens(const rtg_bitmap_t *b, int y, int x0, int count, uint8_t *dst)
{
    rtg_cmap_t c;

    if (y < 0 || y >= (int)b->height)
    {
        memset(dst, 0, (size_t)count);
        return;
    }
    if (b->format == 1)
    {
        for (int i = 0; i < count; i++)
            dst[i] = (x0 + i) < (int)b->width ? (uint8_t)rd8(px_addr(b, x0 + i, y)) : 0;
        return;
    }
    cmap_open(b->cmap, &c);
    {
        uint32_t last_rgb = 0xffffffffu;
        int32_t last_pen = 0;
        for (int i = 0; i < count; i++)
        {
            uint32_t rgb;

            if (x0 + i >= (int)b->width)
            {
                dst[i] = 0;
                continue;
            }
            rgb = raw_to_argb(b->format, load_raw(px_addr(b, x0 + i, y), b->bpp)) & 0xffffff;
            if (rgb != last_rgb)
            {
                last_rgb = rgb;
                last_pen = cmap_find(&c, rgb);
            }
            dst[i] = last_pen < 0 ? 0xff : (uint8_t)last_pen;
        }
    }
}

void rtg_read_row_rgb(const rtg_bitmap_t *b, int y, int x0, int count, uint32_t *dst)
{
    rtg_cmap_t c;

    cmap_open(b->cmap, &c);
    for (int i = 0; i < count; i++)
    {
        if (y < 0 || y >= (int)b->height || x0 + i >= (int)b->width)
            dst[i] = 0;
        else
            dst[i] = px_rgb(b, &c, x0 + i, y);
    }
}

/* ------------------------------------------------------------------ */

uint32_t rtg_emucall(uint32_t op, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    switch (op)
    {
        case RTG_OP_ENABLED:
            return rtg_enabled() ? 1 : 0;
        case RTG_OP_NEXT_ID:
            return rtg_enabled() ? mode_next(a1) : INVALID_ID;
        case RTG_OP_AVAIL:
        {
            int r, f;
            if (!rtg_enabled() || !mode_lookup(a1, &r, &f))
                return INVALID_ID;
            return r == RES_P96MODE ? RTG_NA_P96MODE : 0;
        }
        case RTG_OP_INFO:
            return rtg_enabled() ? mode_info(a1, a2, a3, a4) : 0;
        case RTG_OP_MONITOR:
        {
            char name[32];
            if (!rtg_enabled() || a1 >= (uint32_t)NUM_RES)
                return 0;
            monitor_name((int)a1, name, sizeof(name));
            if (a2 && ram_ok(a2, 32))
                memcpy(ram_ptr(a2, 32), name, strlen(name) + 1);
            return (uint32_t)s_res[a1].rows << 16 | s_res[a1].clocks;
        }
        case RTG_OP_SET_MONITOR:
            if (a1 < (uint32_t)NUM_RES)
                s_monitors[a1] = a2;
            return 0;
        case RTG_OP_MONITOR_OF:
        {
            int r, f;
            if (!rtg_enabled() || !mode_lookup(a1, &r, &f))
                return 0;
            return s_monitors[r];
        }
        case RTG_OP_BESTMODE:
            return rtg_enabled() ? best_mode(a1) : RTG_BEST_CLASSIC;
        case RTG_OP_MODE_ATTR:
            return rtg_enabled() ? mode_attr(a1, a2) : 0;
        case RTG_OP_RENDER:
            return render(a1, a2);
        case RTG_OP_ENCODE:
            return encode_color(a1, a2);
        case RTG_OP_SET_SCREEN_CM:
            s_screen_cm = a1;
            return 0;
        case RTG_OP_BOARD_MEM:
            return RTG_BOARD_MEM;
        case RTG_OP_P96BEST:
            return rtg_enabled() ? p96_best_mode(a1) : INVALID_ID;
        case RTG_OP_P96DESC:
            return rtg_enabled() ? p96_mode_desc(a1, a2, a3) : 0;
        default:
            return 0;
    }
}
