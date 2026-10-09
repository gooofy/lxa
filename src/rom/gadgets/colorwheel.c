/*
    Copyright (C) 1995-2015, The AROS Development Team. All rights reserved.

    AROS colorwheel gadget.

    Ported to lxa from AROS workbench/classes/gadgets/colorwheel
    (colorwheelclass.c, support.c, converthsbtorgb.c, convertrgbtohsb.c).
    lxa changes (see doc/third-party-code.md):
      - library frame in classlib.c, plain C BOOPSI helpers;
      - ConvertHSBToRGB()/ConvertRGBToHSB() compute exactly what AmigaOS 3.1
        does (16 bit components, sector width 0x2AAB, observed on the
        reference machine, tests/probes/colorwheel/class.c);
      - OM_SET applies the HSB attributes first, then the RGB attributes,
        keeps HSB as the master copy and returns which parts changed
        (bit 0: hue/saturation, bit 1: brightness), as 3.1;
      - an object can be created without WHEEL_Screen (3.1 does that too);
      - only the pen based (CLUT) wheel: no cybergraphics truecolor path;
      - no ghost pattern for GA_Disabled (3.1 draws it like an enabled one).
*/

#include <dos/dos.h>
#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <intuition/screens.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include "classlib.h"
#include "colorwheel_fixmath.h"

/***************************************************************************************************/

#define EG(o)                   ((struct ExtGadget *)o)

#define KNOBWIDTH               7
#define KNOBHEIGHT              7
#define KNOBCX                  3
#define KNOBCY                  3

#define BORDERWHEELSPACINGX     4
#define BORDERWHEELSPACINGY     4

#define USE_SYMMETRIC_SPEEDUP   1

struct ColorWheelData
{
    struct ColorWheelHSB        hsb;                    /* ISGNU        */
    struct ColorWheelRGB        rgb;                    /* ISGNU        */
    struct Screen               *scr;                   /* I            */
    Object                      *gradobj;               /* IS           */
    STRPTR                      abbrv;                  /* I            */
    UWORD                       *donation;              /* I            */
    UWORD                       maxpens;                /* I            */

    struct DrawInfo             *dri;
    struct BitMap               *bm;
    PLANEPTR                    mask;
    struct BitMap               *savebm;
    struct RastPort             trp;
    Object                      *frame;
    struct Hook                 *backfill;
    WORD                        bmwidth;
    WORD                        bmheight;
    WORD                        wheelcx;
    WORD                        wheelcy;
    WORD                        wheelrx;
    WORD                        wheelry;
    WORD                        knobsavex;
    WORD                        knobsavey;
    BOOL                        wheeldrawn;
    UWORD                       range;
    UWORD                       levels;
    WORD                        pens[6*6*6];
    BOOL                        gotpens;
};

const char  classlib_LibName[]    = "colorwheel.gadget";
const char  classlib_LibID[]      = "colorwheel 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct ColorWheelData);

/***************************************************************************************************/
/* ConvertHSBToRGB() / ConvertRGBToHSB()                                                           */
/***************************************************************************************************/

/*
 * AmigaOS 3.1 works with the upper 16 bits of each component and returns
 * them in both words of the result.  The hue circle has six sectors of
 * 0x2AAB; the position inside a sector is scaled to 0..0xFFFF.
 */
static void cw_hsb2rgb(const struct ColorWheelHSB *hsb, struct ColorWheelRGB *rgb)
{
    ULONG h = hsb->cw_Hue >> 16;
    ULONG s = hsb->cw_Saturation >> 16;
    ULONG i = hsb->cw_Brightness >> 16;
    ULONG r, g, b, sec, pos, f, w, q, t;

    if (s == 0)
    {
        r = g = b = i;
    }
    else
    {
        sec = h / 0x2AAB;
        pos = h - sec * 0x2AAB;
        f   = pos ? ((pos << 16) - 1) / 0x2AAA : 0;
        if (f > 0xFFFF)
            f = 0xFFFF;

        w = (i * (0xFFFF - s)) >> 16;
        q = (i * (0xFFFF - ((s * f) >> 16))) >> 16;
        t = (i * (0xFFFF - ((s * (0xFFFF - f)) >> 16))) >> 16;

        switch (sec)
        {
            case 0:  r = i; g = t; b = w; break;
            case 1:  r = q; g = i; b = w; break;
            case 2:  r = w; g = i; b = t; break;
            case 3:  r = w; g = q; b = i; break;
            case 4:  r = t; g = w; b = i; break;
            default: r = i; g = w; b = q; break;
        }
    }

    rgb->cw_Red   = r | (r << 16);
    rgb->cw_Green = g | (g << 16);
    rgb->cw_Blue  = b | (b << 16);
}

/* returns FALSE (hue left alone) for a grey: hue is undefined then */
static BOOL cw_rgb2hsb(const struct ColorWheelRGB *rgb, struct ColorWheelHSB *hsb)
{
    LONG r = rgb->cw_Red >> 16;
    LONG g = rgb->cw_Green >> 16;
    LONG b = rgb->cw_Blue >> 16;
    LONG max, min, delta, base, n, f, h;
    ULONG s;

    max = r > g ? r : g; if (b > max) max = b;
    min = r < g ? r : g; if (b < min) min = b;
    delta = max - min;

    s = max ? ((ULONG)delta * 0xFFFF) / (ULONG)max : 0;

    hsb->cw_Saturation = s | (s << 16);
    hsb->cw_Brightness = (ULONG)max | ((ULONG)max << 16);

    if (delta == 0)
        return FALSE;

    if (r == max)      { base = 0; n = g - b; }
    else if (g == max) { base = 2; n = b - r; }
    else               { base = 4; n = r - g; }

    /* |n| * 0xFFFF / delta, rounded away from zero */
    f = (LONG)((((ULONG)(n < 0 ? -n : n)) * 0xFFFF + (ULONG)delta - 1) / (ULONG)delta);
    if (n < 0)
        f = -f;

    h = base * 0xFFFF + f;
    if (h < 0)
        h += 6 * 0xFFFF;
    h = (h / 6) & 0xFFFF;

    hsb->cw_Hue = (ULONG)h | ((ULONG)h << 16);
    return TRUE;
}

VOID cw_ConvertHSBToRGB(register struct ColorWheelHSB *hsb __asm("a0"),
                        register struct ColorWheelRGB *rgb __asm("a1"),
                        register struct Library *ColorWheelBase __asm("a6"))
{
    cw_hsb2rgb(hsb, rgb);
}

VOID cw_ConvertRGBToHSB(register struct ColorWheelRGB *rgb __asm("a0"),
                        register struct ColorWheelHSB *hsb __asm("a1"),
                        register struct Library *ColorWheelBase __asm("a6"))
{
    /* the hue of a grey is undefined (3.1 leaves a random value): 0 here */
    if (!cw_rgb2hsb(rgb, hsb))
        hsb->cw_Hue = 0;
}

APTR classlib_FuncTab[] =
{
    (APTR)classlib_Open,
    (APTR)classlib_Close,
    (APTR)classlib_Expunge,
    (APTR)classlib_ExtFunc,
    (APTR)cw_ConvertHSBToRGB,           /* -30 */
    (APTR)cw_ConvertRGBToHSB,           /* -36 */
    (APTR)-1
};

/***************************************************************************************************/
/* support.c                                                                                       */
/***************************************************************************************************/

static const UBYTE Bayer16[16][16] =
{
   {   1,235, 59,219, 15,231, 55,215,  2,232, 56,216, 12,228, 52,212},
   { 129, 65,187,123,143, 79,183,119,130, 66,184,120,140, 76,180,116},
   {  33,193, 17,251, 47,207, 31,247, 34,194, 18,248, 44,204, 28,244},
   { 161, 97,145, 81,175,111,159, 95,162, 98,146, 82,172,108,156, 92},
   {   9,225, 49,209,  5,239, 63,223, 10,226, 50,210,  6,236, 60,220},
   { 137, 73,177,113,133, 69,191,127,138, 74,178,114,134, 70,188,124},
   {  41,201, 25,241, 37,197, 21,255, 42,202, 26,242, 38,198, 22,252 },
   { 169,105,153, 89,165,101,149, 85,170,106,154, 90,166,102,150, 86},
   {   3,233, 57,217, 13,229, 53,213,  0,234, 58,218, 14,230, 54,214},
   { 131, 67,185,121,141, 77,181,117,128, 64,186,122,142, 78,182,118},
   {  35,195, 19,249, 45,205, 29,245, 32,192, 16,250, 46,206, 30,246},
   { 163, 99,147, 83,173,109,157, 93,160, 96,144, 80,174,110,158, 94},
   {  11,227, 51,211,  7,237, 61,221,  8,224, 48,208,  4,238, 62,222},
   { 139, 75,179,115,135, 71,189,125,136, 72,176,112,132, 68,190,126},
   {  43,203, 27,243, 39,199, 23,253, 40,200, 24,240, 36,196, 20,255 },
   { 171,107,155, 91,167,103,151, 87,168,104,152, 88,164,100,148, 84}
};

static BOOL CalcWheelColor(LONG x, LONG y, LONG cx, LONG cy, ULONG *hue, ULONG *sat)
{
    Fixed32     r, l, h, s, sinus;
    LONG        rx, ry;
    ULONG       sq1, sq2;

    rx = cx - x;
    ry = (y - cy) * cx / cy;

    r = FixSqrti((rx*rx) + (ry*ry));

    h = (r != 0) ? FixAtan2(FixDiv(INT_TO_FIXED(rx), r), FixDiv(INT_TO_FIXED(ry), r)) : 0;

    /* N.B. We convert to ints before adding the two squared values below to
       avoid an overflow with large wheel sizes. */
    sq1 = FixSqr(cx * FixSinCos(h + (FIXED_PI/2), &sinus));
    sq2 = FixSqr(cx * sinus);
    l = FixSqrti(FIXED_TO_INT(sq1) + FIXED_TO_INT(sq2));

    s = l ? FixDiv(r, l) : 0;

    h = FixMul(h + FIXED_PI, 10430); /* == FixDiv(h + FIXED_PI, FIXED_2PI) */

    if (s == 0) s = 1;
    else if (s >= FIXED_ONE)
        s = FIXED_ONE-1;

    h &= 0xffff;

    *hue = (h << 16) | h;
    *sat = (s << 16) | s;

    return (r <= INT_TO_FIXED(cx));
}

static VOID CalcKnobPos(struct ColorWheelData *data, WORD *x, WORD *y)
{
    Fixed32 alpha, sat, sinus;

    alpha = data->hsb.cw_Hue >> 16;
    alpha = FixMul(FIXED_2PI, alpha) - (FIXED_PI/2);

    sat = data->hsb.cw_Saturation >> 16;

    *x = data->wheelcx + (WORD)FIXED_TO_INT(FixMul(((LONG)data->wheelrx) * sat, FixSinCos(alpha, &sinus)));
    *y = data->wheelcy + (WORD)FIXED_TO_INT(FixMul(((LONG)data->wheelry) * sat, sinus));
}

/* pen based wheel (6x6x6 colour cube, ordered dither) */
static VOID ClutWheel(struct ColorWheelData *data, struct RastPort *rp, struct IBox *box)
{
    struct ColorWheelHSB        hsb;
    struct ColorWheelRGB        rgb;
    struct RastPort            *tRP = &data->trp;
    struct BitMap              *tBM;
    LONG                        x, y, left, top, width, height;
    LONG                        cx, cy;
    UBYTE                      *buf;

    left   = box->Left;
    top    = box->Top;
    width  = box->Width;
    height = box->Height;

    if (data->gotpens != TRUE)
    {
        /* no pens: monochrome wheel with the colour abbreviations */
        STRPTR          abbrv = data->abbrv;
        PLANEPTR        ras;
        LONG            wcx = data->wheelcx,
                        wcy = data->wheelcy,
                        rx = data->wheelrx - 4,
                        ry = data->wheelry - 4,
                        depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
        ULONG           rasSize;

        rasSize = RASSIZE(width*depth, height);

        if (data->scr && (ras = AllocVec(rasSize, MEMF_CHIP)))
        {
            struct AreaInfo     ai;
            struct TmpRas       tr;
            UWORD               pattern[] = {0xaaaa, 0x5555};
            LONG                black = FindColor(data->scr->ViewPort.ColorMap, 0,0,0, -1),
                                white = FindColor(data->scr->ViewPort.ColorMap, ~0,~0,~0, -1);
            WORD                abuf[10];
            LONG                endx, endy, TxOffset, i;

            for (i = 0; i < 10; i++) abuf[i] = 0;
            InitArea(&ai, abuf, sizeof(abuf) / 5);
            rp->AreaInfo = &ai;
            rp->TmpRas = &tr;
            InitTmpRas(&tr, ras, rasSize);

            SetAPen(rp, black);

            AreaEllipse(rp, wcx, wcy, rx, ry);
            AreaEnd(rp);

            SetAPen(rp, white);
            SetFont(rp, data->dri->dri_Font);

            TxOffset = rp->TxHeight - rp->TxBaseline;

            endx =  500*rx/2000; /* 30 degrees */
            endy = -865*ry/2000;

            Move(rp, wcx + rx/2, wcy);
            Draw(rp, wcx + rx, wcy);

            Move(rp, wcx - rx/2, wcy);
            Draw(rp, wcx - rx, wcy);

            Move(rp, wcx + endx, wcy + endy);
            Draw(rp, wcx + 2*endx, wcy + 2*endy);

            Move(rp, wcx + endx, wcy - endy);
            Draw(rp, wcx + 2*endx, wcy - 2*endy);

            Move(rp, wcx - endx, wcy + endy);
            Draw(rp, wcx - 2*endx, wcy + 2*endy);

            Move(rp, wcx - endx, wcy - endy);
            Draw(rp, wcx - 2*endx, wcy - 2*endy);

            endx =  866*rx/1500; /* 60 degrees */
            endy = -499*ry/1500;

            Move(rp, wcx + endx + (TextLength(rp, abbrv, 1L) / 2), wcy - (endy - TxOffset));
            Text(rp, abbrv++, 1L); /* G */

            Move(rp, wcx - (TextLength(rp, abbrv, 1L) / 2), wcy + (ry - ry/4) + TxOffset);
            Text(rp, abbrv++, 1L); /* C */

            Move(rp, wcx - endx - (TextLength(rp, abbrv, 1L)), wcy - (endy - TxOffset));
            Text(rp, abbrv++, 1L); /* B */

            Move(rp, wcx - endx - (TextLength(rp, abbrv, 1L)), wcy + (endy - TxOffset));
            Text(rp, abbrv++, 1L); /* M */

            Move(rp, wcx - (TextLength(rp, abbrv, 1L) / 2), wcy - (ry - ry/4) + TxOffset);
            Text(rp, abbrv++, 1L); /* R */

            Move(rp, wcx + endx + (TextLength(rp, abbrv, 1L) / 2), wcy + (endy + TxOffset));
            Text(rp, abbrv++, 1L); /* Y */

            SetAfPt(rp, pattern, 1L);

            AreaEllipse(rp, wcx, wcy, rx/2, ry/2);
            AreaEnd(rp);

            SetAfPt(rp, NULL, 0L);

            AreaEllipse(rp, wcx, wcy, rx/5, ry/5);
            AreaEnd(rp);

            WaitBlit();
            FreeVec(ras);

            rp->AreaInfo = NULL;
            rp->TmpRas = NULL;
        }

        return;
    }

    cx = width / 2;
    cy = height / 2;

    hsb.cw_Brightness = 0xFFFFFFFF;

    if ((tBM = AllocBitMap(width, 1, GetBitMapAttr(rp->BitMap, BMA_DEPTH), 0L, NULL)))
    {
        tRP->BitMap = tBM;

        if ((buf = AllocVec((((width+15)>>4)<<4), MEMF_ANY)))
        {
            LONG range = data->range,
                 levels = data->levels;

            for (y = 0; y < height; y++)
            {
                LONG    startX = 0, w = width;
                UBYTE  *p = buf;
                UBYTE  *p2 = &p[width];

                for (x = 0; x < width / 2; x++)
                {
                    if (CalcWheelColor(x, y, cx, cy, &hsb.cw_Hue, &hsb.cw_Saturation))
                    {
                        LONG    t, v, r, g, b, base;

                        cw_hsb2rgb(&hsb, &rgb);

                        t = Bayer16[y & 15][x & 15];

                        t = (t * range) / 255;

                        v = (rgb.cw_Red >> 24);
                        base = (v / range) * range;
                        r = (v - base > t) ? base + range : base;

                        v = (rgb.cw_Green >> 24);
                        base = (v / range) * range;
                        g = (v - base > t) ? base + range : base;

                        v = (rgb.cw_Blue >> 24);
                        base = (v / range) * range;
                        b = (v - base > t) ? base + range : base;

                        r /= range; if (r >= levels) r = levels - 1;
                        g /= range; if (g >= levels) g = levels - 1;
                        b /= range; if (b >= levels) b = levels - 1;

                        r *= levels*levels;

                        base = r + (g*levels) + b;
                        *p++ = data->pens[base];

                        base = r + (b*levels) + g;
                        *--p2 = data->pens[base];
                    }
                    else
                    {
                        startX++;
                        w--; p++;
                        w--; --p2;
                    }
                }

                if (w > 0)
                    WritePixelLine8(rp, startX + left, top + y, w, &buf[startX], tRP);
            }

            FreeVec(buf);
        }

        WaitBlit();
        FreeBitMap(tBM);
    }
}

static VOID RenderWheel(struct ColorWheelData *data, struct RastPort *rp, struct IBox *box)
{
    struct IBox         wbox;
    struct RastPort     temprp;
    LONG                cx, cy, rx, ry;

    cx = data->frame ? BORDERWHEELSPACINGX * 4 : BORDERWHEELSPACINGX * 2;
    cy = data->frame ? BORDERWHEELSPACINGY * 4 : BORDERWHEELSPACINGY * 2;

    data->wheeldrawn = FALSE;

    if ((box->Width < cx) || (box->Height < cy)) return;

    if (!data->bm || (box->Width != data->bmwidth) || (box->Height != data->bmheight))
    {
        if (data->bm)
        {
            WaitBlit();

            if (data->mask)
            {
                FreeVec(data->mask);
                data->mask = NULL;
            }

            FreeBitMap(data->bm);
        }

        data->bm = AllocBitMap(box->Width,
                               box->Height,
                               GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                               BMF_MINPLANES,
                               rp->BitMap);

        if (data->bm)
        {
            data->bmwidth  = box->Width;
            data->bmheight = box->Height;

            wbox.Left   = data->frame ? BORDERWHEELSPACINGX : 2;
            wbox.Top    = data->frame ? BORDERWHEELSPACINGY : 2;
            wbox.Width  = (box->Width  - (data->frame ? BORDERWHEELSPACINGX * 2 : 4)) & ~1;
            wbox.Height = (box->Height - (data->frame ? BORDERWHEELSPACINGY * 2 : 4)) & ~1;

            if (wbox.Width > 440)
            {
                wbox.Left  += (wbox.Width-440)/2;
                wbox.Width  = 440;
            }

            if (wbox.Height > 440)
            {
                wbox.Top    += (wbox.Height-440)/2;
                wbox.Height  = 440;
            }

            InitRastPort(&temprp);
            temprp.BitMap = data->bm;

            SetDrMd(&temprp, JAM1);
            SetRast(&temprp, data->dri->dri_Pens[BACKGROUNDPEN]);

            rx = wbox.Width / 2;
            ry = wbox.Height / 2;

            cx = wbox.Left + rx;
            cy = wbox.Top + ry;

            data->wheelcx = cx;
            data->wheelcy = cy;
            data->wheelrx = rx;
            data->wheelry = ry;

            if (data->frame)
            {
                struct TagItem fitags[] =
                {
                    {IA_Width   , box->Width    },
                    {IA_Height  , box->Height   },
                    {TAG_DONE                   }
                };

                SetAttrsA(data->frame, fitags);
                DrawImageState(&temprp, (struct Image *)data->frame, 0, 0, IDS_NORMAL, data->dri);
            }
            else
            {
                struct BitMap   maskBM;
                ULONG           bmWidth, bmHeight, rasSize;
                PLANEPTR        ras;

                bmWidth = GetBitMapAttr(data->bm, BMA_WIDTH);
                bmHeight = GetBitMapAttr(data->bm, BMA_HEIGHT);
                InitBitMap(&maskBM, 1L, bmWidth, bmHeight);

                rasSize = RASSIZE(bmWidth, bmHeight);

                if ((ras = AllocVec(rasSize, MEMF_CHIP)))
                {
                    if ((data->mask = AllocVec(rasSize, MEMF_CHIP)))
                    {
                        struct AreaInfo  ai;
                        struct TmpRas    tr;
                        struct RastPort *maskRP = &data->trp;
                        WORD             abuf[10];
                        LONG             i;

                        for (i = 0; i < 10; i++) abuf[i] = 0;
                        maskRP->BitMap = &maskBM;
                        maskBM.Planes[0] = data->mask;
                        InitArea(&ai, abuf, sizeof(abuf) / 5);
                        maskRP->AreaInfo = &ai;
                        maskRP->TmpRas = &tr;
                        InitTmpRas(&tr, ras, rasSize);

                        SetRast(maskRP, 0L);
                        SetAPen(maskRP, 1L);
                        AreaEllipse(maskRP, cx, cy, rx, ry);
                        AreaEnd(maskRP);
                        WaitBlit();

                        if (!data->savebm)
                        {
                            data->savebm = AllocBitMap(
                                    KNOBWIDTH, KNOBHEIGHT,
                                    GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                                    BMF_MINPLANES, rp->BitMap);
                        }

                        maskRP->AreaInfo = NULL;
                        maskRP->TmpRas = NULL;
                    }

                    FreeVec(ras);
                }
            }

            ClutWheel(data, &temprp, &wbox);

            SetAPen(&temprp, data->dri->dri_Pens[SHADOWPEN]);
            DrawEllipse(&temprp, cx, cy, rx, ry);
            DrawEllipse(&temprp, cx, cy, rx - 1, ry);
            DrawEllipse(&temprp, cx, cy, rx, ry - 1);
            DrawEllipse(&temprp, cx, cy, rx - 1, ry - 1);
        }
    }

    if (data->bm)
    {
        if (data->mask)
        {
            EraseRect(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
            BltMaskBitMapRastPort(data->bm, 0, 0,
                                  rp, box->Left, box->Top, box->Width, box->Height,
                                  0xe0, data->mask);
        }
        else
            BltBitMapRastPort(data->bm, 0, 0, rp, box->Left, box->Top, box->Width, box->Height, 0xC0);

        data->wheeldrawn = TRUE;
    }
}

static VOID RenderKnob(struct ColorWheelData *data, struct RastPort *rp, struct IBox *gbox, BOOL update)
{
    WORD x, y;

    if (!data->wheeldrawn) return;

    if (update)
    {
        /* Restore */
        if (data->savebm)
            BltBitMapRastPort(data->savebm, 0, 0, rp,
                              data->knobsavex + gbox->Left, data->knobsavey + gbox->Top,
                              KNOBWIDTH, KNOBHEIGHT, 0xC0);
        else
            BltBitMapRastPort(data->bm, data->knobsavex, data->knobsavey, rp,
                              data->knobsavex + gbox->Left, data->knobsavey + gbox->Top,
                              KNOBWIDTH, KNOBHEIGHT, 0xC0);
    }

    CalcKnobPos(data, &x, &y);

    if (x < KNOBCX) x = KNOBCX; else if (x > gbox->Width  - 1 - KNOBCX) x = gbox->Width  - 1 - KNOBCX;
    if (y < KNOBCY) y = KNOBCY; else if (y > gbox->Height - 1 - KNOBCY) y = gbox->Height - 1 - KNOBCY;

    /* Backup */
    data->knobsavex = x - KNOBCX;
    data->knobsavey = y - KNOBCY;

    /* Render */
    x += gbox->Left;
    y += gbox->Top;

    if (data->savebm)
    {
        data->trp.BitMap = data->savebm;
        ClipBlit(rp, x-KNOBCX, y-KNOBCY, &data->trp, 0, 0, KNOBWIDTH, KNOBHEIGHT, 0xc0);
    }

    SetDrMd(rp, JAM1);

    SetAPen(rp, data->dri->dri_Pens[SHADOWPEN]);

    RectFill(rp, x - 3, y - 1, x - 3, y + 1);
    RectFill(rp, x - 2, y - 2, x - 2, y + 2);
    RectFill(rp, x - 1, y - 3, x + 1, y + 3);
    RectFill(rp, x + 2, y - 2, x + 2, y + 2);
    RectFill(rp, x + 3, y - 1, x + 3, y + 1);

    SetAPen(rp, data->dri->dri_Pens[SHINEPEN]);

    RectFill(rp, x - 1, y, x + 1, y);
    RectFill(rp, x, y - 1, x, y + 1);
}

static VOID GetGadgetIBox(Object *o, struct GadgetInfo *gi, struct IBox *ibox)
{
    ibox->Left   = EG(o)->LeftEdge;
    ibox->Top    = EG(o)->TopEdge;
    ibox->Width  = EG(o)->Width;
    ibox->Height = EG(o)->Height;

    if (gi)
    {
        if (EG(o)->Flags & GFLG_RELRIGHT)
            ibox->Left   += gi->gi_Domain.Width - 1;

        if (EG(o)->Flags & GFLG_RELBOTTOM)
            ibox->Top    += gi->gi_Domain.Height - 1;

        if (EG(o)->Flags & GFLG_RELWIDTH)
            ibox->Width  += gi->gi_Domain.Width;

        if (EG(o)->Flags & GFLG_RELHEIGHT)
            ibox->Height += gi->gi_Domain.Height;
    }
}

static BOOL getPens(struct ColorWheelData *data)
{
    struct ColorMap *cm = data->scr->ViewPort.ColorMap;
    LONG             r, g, b, levels = data->levels, range = data->range, i;
    WORD            *p;
    UWORD           *donation = data->donation;

    for (p = data->pens, i = levels-1, r = 0; r < levels; r++)
    {
        for (g = 0; g < levels; g++)
        {
            for (b = 0; b < levels; b++)
            {
                if (r == i || g == i || b == i)
                {
                    if (donation && *donation != 0xFFFF)
                    {
                        ULONG   tab[5];

                        tab[0] = (1 << 16) | (*donation);
                        tab[1] = (r*range)*0x01010101;
                        tab[2] = (g*range)*0x01010101;
                        tab[3] = (b*range)*0x01010101;
                        tab[4] = 0L;

                        LoadRGB32(&data->scr->ViewPort, tab);
                        *p++ = *donation++;
                    }
                    else
                    {
                        struct TagItem   tags[] =
                        {
                            {OBP_Precision, PRECISION_EXACT},
                            {OBP_FailIfBad, TRUE},
                            {TAG_DONE}
                        };

                        if ((*p++ = ObtainBestPenA(cm,
                                (r*range)*0x01010101,
                                (g*range)*0x01010101,
                                (b*range)*0x01010101, tags)) == -1)
                        {
                            WORD        *p2 = data->pens;

                            while (p2 != p)
                            {
                                UWORD *don;

                                if ((don = data->donation))
                                {
                                    do
                                    {
                                        if (*don == (UWORD)*p2)
                                        {
                                            *p2 = -1;
                                            break;
                                        }
                                    }
                                    while (*don++ != 0xFFFF);
                                }

                                if (*p2 != -1)
                                    ReleasePen(cm, *p2);
                                p2++;
                            }

                            return FALSE;
                        }
                    }
                }
                else *p++ = -1;
            }
        }
    }

    return data->gotpens = TRUE;
}

static void allocPens(struct ColorWheelData *data)
{
    LONG        depth = GetBitMapAttr(data->scr->RastPort.BitMap, BMA_DEPTH);
    LONG        donated = 0L, levels;

    if (data->donation)
        for (donated = 0L; data->donation[donated] != 0xffff; donated++);

    for (levels = 6; levels >= 2; levels--)
    {
        LONG         i;

        i = levels-1;
        i = levels*levels*levels-i*i*i;

        if ((i > (donated+data->maxpens)) || (depth < 31 && i > (1L<<depth)))
            continue;

        data->range = 255 / (levels-1);
        data->levels = levels;

        if (getPens(data)) break;
    }
}

static void freePens(struct ColorWheelData *data)
{
    if (data->gotpens)
    {
        struct ColorMap *cm = data->scr->ViewPort.ColorMap;
        LONG             i = data->levels;

        for (i = (i*i*i)-1; i >= 0; i--)
        {
            if (data->pens[i] != -1)
            {
                UWORD    *donation;

                if ((donation = data->donation))
                {
                    do
                    {
                        if ((UWORD)data->pens[i] == *donation)
                        {
                            data->pens[i] = -1;
                            break;
                        }
                    }
                    while (*donation++ != 0xFFFF);
                }

                if (data->pens[i] != -1)
                    ReleasePen(cm, data->pens[i]);
            }
        }
        data->gotpens = FALSE;
    }
}

/***************************************************************************************************/
/* colorwheelclass.c                                                                               */
/***************************************************************************************************/

static VOID notify_all(Class *cl, Object *o, struct GadgetInfo *gi, BOOL interim, BOOL userinput)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    struct opUpdate             opu;
    struct TagItem              tags[] =
    {
        {GA_ID                  , EG(o)->GadgetID               },
        {GA_UserInput           , userinput                     },
        {WHEEL_Hue              , data->hsb.cw_Hue              },
        {WHEEL_Saturation       , data->hsb.cw_Saturation       },
        {TAG_DONE                                               }
    };

    opu.MethodID     = OM_NOTIFY;
    opu.opu_AttrList = tags;
    opu.opu_GInfo    = gi;
    opu.opu_Flags    = interim ? OPUF_INTERIM : 0;

    CL_DoMethodA(o, (Msg)&opu);
}

/* the brightness lives in an attached gradient slider (GRAD_CurVal = 0xFFFF - brightness) */
static VOID sync_from_slider(struct ColorWheelData *data)
{
    if (data->gradobj)
    {
        ULONG gradval = 0;

        GetAttr(GRAD_CurVal, data->gradobj, &gradval);
        gradval = (0xFFFF - gradval) & 0xFFFF;

        data->hsb.cw_Brightness = gradval * 0x10000 + gradval;
        cw_hsb2rgb(&data->hsb, &data->rgb);
    }
}

static ULONG norm16(ULONG v)
{
    return (v & 0xFFFF0000) | (v >> 16);
}

static ULONG ColorWheel_SET(Class *cl, Object *o, struct opSet *msg, BOOL init)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    struct TagItem              *tag, *tstate;
    struct ColorWheelHSB        old, hsb;
    struct ColorWheelRGB        rgb;
    BOOL                        hsbset = FALSE, rgbset = FALSE, redraw = FALSE;
    BOOL                        disabled = (EG(o)->Flags & GFLG_DISABLED) != 0;
    ULONG                       retval = 0;

    if (!init)
        CL_DoSuperMethodA(cl, o, (Msg)msg);

    sync_from_slider(data);
    old = data->hsb;
    hsb = data->hsb;
    rgb.cw_Red = rgb.cw_Green = rgb.cw_Blue = 0;

    /* 1st pass: the HSB attributes */
    tstate = msg->ops_AttrList;
    while ((tag = NextTagItem(&tstate)))
    {
        ULONG tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case WHEEL_Hue:         hsb.cw_Hue = norm16(tidata);        hsbset = TRUE; break;
            case WHEEL_Saturation:  hsb.cw_Saturation = norm16(tidata); hsbset = TRUE; break;
            case WHEEL_Brightness:  hsb.cw_Brightness = norm16(tidata); hsbset = TRUE; break;
            case WHEEL_HSB:
                if (tidata)
                {
                    struct ColorWheelHSB *h = (struct ColorWheelHSB *)tidata;
                    hsb.cw_Hue        = norm16(h->cw_Hue);
                    hsb.cw_Saturation = norm16(h->cw_Saturation);
                    hsb.cw_Brightness = norm16(h->cw_Brightness);
                    hsbset = TRUE;
                }
                break;
            case WHEEL_GradientSlider:
                data->gradobj = (Object *)tidata;
                break;
            case GA_BackFill:
                data->backfill = (struct Hook *)tidata;
                break;
            case GA_Disabled:
                if (disabled != (tidata != 0))
                    redraw = TRUE;
                break;
            default:
                break;
        }
    }

    cw_hsb2rgb(&hsb, &rgb);

    /* 2nd pass: the RGB attributes, applied to the colour above */
    tstate = msg->ops_AttrList;
    while ((tag = NextTagItem(&tstate)))
    {
        ULONG tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case WHEEL_Red:   rgb.cw_Red = tidata;   rgbset = TRUE; break;
            case WHEEL_Green: rgb.cw_Green = tidata; rgbset = TRUE; break;
            case WHEEL_Blue:  rgb.cw_Blue = tidata;  rgbset = TRUE; break;
            case WHEEL_RGB:
                if (tidata)
                {
                    rgb = *(struct ColorWheelRGB *)tidata;
                    rgbset = TRUE;
                }
                break;
            default:
                break;
        }
    }

    if (rgbset)
    {
        struct ColorWheelHSB nhsb;

        if (!cw_rgb2hsb(&rgb, &nhsb))
            nhsb.cw_Hue = hsb.cw_Hue;       /* grey: keep the hue */
        hsb = nhsb;
    }

    if (hsbset || rgbset)
    {
        data->hsb = hsb;
        cw_hsb2rgb(&data->hsb, &data->rgb);
    }

    if (data->hsb.cw_Hue != old.cw_Hue || data->hsb.cw_Saturation != old.cw_Saturation)
        retval |= 1;
    if (data->hsb.cw_Brightness != old.cw_Brightness)
        retval |= 2;

    if (retval && !init)
        notify_all(cl, o, msg->ops_GInfo, FALSE, FALSE);

    if (data->gradobj && (init || (retval & 2)))
    {
        struct TagItem set_tags[] =
        {
            {GRAD_CurVal , 0    },
            {TAG_DONE           }
        };
        struct opSet ops;

        set_tags[0].ti_Data = 0xFFFF - (data->hsb.cw_Brightness >> 16);
        ops.MethodID     = OM_SET;
        ops.ops_AttrList = set_tags;
        ops.ops_GInfo    = msg->ops_GInfo;
        CL_DoMethodA(data->gradobj, (Msg)&ops);
    }

    if (!init && (retval || redraw) && msg->ops_GInfo)
    {
        struct RastPort *rp;

        if ((rp = ObtainGIRPort(msg->ops_GInfo)))
        {
            struct gpRender gpr;

            gpr.MethodID   = GM_RENDER;
            gpr.gpr_GInfo  = msg->ops_GInfo;
            gpr.gpr_RPort  = rp;
            gpr.gpr_Redraw = redraw ? GREDRAW_REDRAW : GREDRAW_UPDATE;
            CL_DoMethodA(o, (Msg)&gpr);
            ReleaseGIRPort(rp);
        }
    }

    return retval;
}

static ULONG ColorWheel_NEW(Class *cl, Object *o, struct opSet *msg)
{
    o = (Object *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
        struct ColorWheelData *data = INST_DATA(cl, o);
        UBYTE *p = (UBYTE *)data;
        ULONG i;

        for (i = 0; i < sizeof(*data); i++)
            p[i] = 0;
        for (i = 0; i < 6*6*6; i++)
            data->pens[i] = -1;

        data->scr = (struct Screen *)GetTagData(WHEEL_Screen, 0, msg->ops_AttrList);

        if (GetTagData(WHEEL_BevelBox, FALSE, msg->ops_AttrList))
        {
            struct TagItem fitags[] =
            {
               {IA_EdgesOnly    , TRUE          },
               {IA_FrameType    , FRAME_BUTTON  },
               {TAG_DONE                        }
            };

            data->frame = NewObjectA(NULL, (ClassID)FRAMEICLASS, fitags);
        }

        data->hsb.cw_Hue        = 0;
        data->hsb.cw_Saturation = 0xFFFFFFFF;
        data->hsb.cw_Brightness = 0xFFFFFFFF;

        data->rgb.cw_Red        = 0xFFFFFFFF;
        data->rgb.cw_Green      = 0;
        data->rgb.cw_Blue       = 0;

        data->abbrv    = (STRPTR)  GetTagData(WHEEL_Abbrv,    (ULONG)"GCBMRY", msg->ops_AttrList);
        data->donation = (UWORD *) GetTagData(WHEEL_Donation, (ULONG)NULL,     msg->ops_AttrList);
        data->maxpens  =           GetTagData(WHEEL_MaxPens,  256,             msg->ops_AttrList);

        ColorWheel_SET(cl, o, msg, TRUE);

        {
            struct Library *DOSBase;

            if ((DOSBase = OpenLibrary((CONST_STRPTR)"dos.library", 39L)))
            {
                TEXT    buf[64];

                if (GetVar((STRPTR)"classes/gadgets/cw_maxpens", buf, sizeof(buf), 0L) > 0L)
                {
                    LONG pens;

                    if (StrToLong(buf, &pens) > 0L)
                        data->maxpens = (pens < 7) ? 7 : pens;
                }

                CloseLibrary(DOSBase);
            }
        }

        if (data->scr)
            allocPens(data);

        InitRastPort(&data->trp);
    }
    return (ULONG)o;
}

static ULONG ColorWheel_GET(Class *cl, Object *o, struct opGet *msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    ULONG                       retval = 1UL;

    sync_from_slider(data);

    switch (msg->opg_AttrID)
    {
        case WHEEL_Hue:
            *msg->opg_Storage = data->hsb.cw_Hue;
            break;

        case WHEEL_Saturation:
            *msg->opg_Storage = data->hsb.cw_Saturation;
            break;

        case WHEEL_Brightness:
            *msg->opg_Storage = data->hsb.cw_Brightness;
            break;

        case WHEEL_HSB:
            *(struct ColorWheelHSB *)msg->opg_Storage = data->hsb;
            break;

        case WHEEL_Red:
            *msg->opg_Storage = data->rgb.cw_Red;
            break;

        case WHEEL_Green:
            *msg->opg_Storage = data->rgb.cw_Green;
            break;

        case WHEEL_Blue:
            *msg->opg_Storage = data->rgb.cw_Blue;
            break;

        case WHEEL_RGB:
            *(struct ColorWheelRGB *)msg->opg_Storage = data->rgb;
            break;

        default:
            retval = CL_DoSuperMethodA(cl, o, (Msg)msg);
            break;
    }

    return retval;
}

static ULONG ColorWheel_RENDER(Class *cl, Object *o, struct gpRender *msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    struct RastPort             *rp = msg->gpr_RPort;
    struct Hook                 *hook = NULL;
    struct IBox                 gbox;
    LONG                        redraw = msg->gpr_Redraw;

    if (!rp || !msg->gpr_GInfo)
        return 0;

    GetGadgetIBox(o, msg->gpr_GInfo, &gbox);
    data->dri = msg->gpr_GInfo->gi_DrInfo;
    if (!data->dri)
        return 0;

    if (!data->bm || (data->bmwidth != gbox.Width) || (data->bmheight != gbox.Height))
        redraw = GREDRAW_REDRAW;

    if (data->backfill && rp->Layer) hook = InstallLayerHook(rp->Layer, data->backfill);

    switch (redraw)
    {
        case GREDRAW_REDRAW:
            RenderWheel(data, rp, &gbox);
            RenderKnob(data, rp, &gbox, FALSE);
            break;

        case GREDRAW_UPDATE:
            RenderKnob(data, rp, &gbox, TRUE);
            break;
    }

    /* 3.1 draws a disabled colour wheel like an enabled one */

    if (data->backfill && rp->Layer) InstallLayerHook(rp->Layer, hook);

    return 1;
}

static ULONG ColorWheel_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);

    if (data->bm)
    {
        WaitBlit();

        if (data->mask)
            FreeVec(data->mask);

        FreeBitMap(data->bm);
    }

    if (data->savebm)
        FreeBitMap(data->savebm);

    if (data->frame)
        DisposeObject(data->frame);

    if (data->scr)
        freePens(data);

    return CL_DoSuperMethodA(cl, o, msg);
}

static ULONG ColorWheel_HITTEST(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    ULONG                       hue, sat;

    if (EG(o)->Flags & GFLG_DISABLED)
        return 0UL;

    if (data->wheeldrawn)
    {
        LONG mousex = msg->gpht_Mouse.X - (data->wheelcx - data->wheelrx);
        LONG mousey = msg->gpht_Mouse.Y - (data->wheelcy - data->wheelry);

        if (CalcWheelColor(mousex, mousey, data->wheelrx, data->wheelry, &hue, &sat))
            return GMR_GADGETHIT;
    }

    return 0;
}

static VOID render_update(Object *o, struct GadgetInfo *gi)
{
    struct RastPort *rp;

    if (gi && (rp = ObtainGIRPort(gi)))
    {
        struct gpRender gpr;

        gpr.MethodID   = GM_RENDER;
        gpr.gpr_GInfo  = gi;
        gpr.gpr_RPort  = rp;
        gpr.gpr_Redraw = GREDRAW_UPDATE;
        CL_DoMethodA(o, (Msg)&gpr);
        ReleaseGIRPort(rp);
    }
}

static ULONG ColorWheel_GOACTIVE(Class *cl, Object *o, struct gpInput *msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);

    if (data->wheeldrawn && msg->gpi_IEvent)
    {
        LONG mousex = msg->gpi_Mouse.X - (data->wheelcx - data->wheelrx);
        LONG mousey = msg->gpi_Mouse.Y - (data->wheelcy - data->wheelry);

        sync_from_slider(data);
        CalcWheelColor(mousex, mousey, data->wheelrx, data->wheelry,
                       &data->hsb.cw_Hue, &data->hsb.cw_Saturation);

        cw_hsb2rgb(&data->hsb, &data->rgb);

        notify_all(cl, o, msg->gpi_GInfo, TRUE, TRUE);
        render_update(o, msg->gpi_GInfo);

        return GMR_MEACTIVE;
    }

    return GMR_NOREUSE;
}

static ULONG ColorWheel_HANDLEINPUT(Class *cl, Object *o, struct gpInput *msg)
{
    struct ColorWheelData       *data = INST_DATA(cl, o);
    struct InputEvent           *ie = msg->gpi_IEvent;
    ULONG                       retval = GMR_MEACTIVE;

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        switch (ie->ie_Code)
        {
            case SELECTUP:
                notify_all(cl, o, msg->gpi_GInfo, FALSE, TRUE);
                *msg->gpi_Termination = EG(o)->GadgetID;
                retval = GMR_NOREUSE | GMR_VERIFY;
                break;

            case MENUDOWN:
                retval = GMR_NOREUSE;
                break;

            case IECODE_NOBUTTON:
            {
                struct ColorWheelHSB    hsb = data->hsb;
                LONG                    mousex = msg->gpi_Mouse.X - (data->wheelcx - data->wheelrx);
                LONG                    mousey = msg->gpi_Mouse.Y - (data->wheelcy - data->wheelry);

                CalcWheelColor(mousex, mousey, data->wheelrx, data->wheelry,
                               &data->hsb.cw_Hue, &data->hsb.cw_Saturation);

                cw_hsb2rgb(&data->hsb, &data->rgb);

                if ((data->hsb.cw_Hue        != hsb.cw_Hue       ) ||
                    (data->hsb.cw_Saturation != hsb.cw_Saturation))
                {
                    notify_all(cl, o, msg->gpi_GInfo, TRUE, TRUE);
                    render_update(o, msg->gpi_GInfo);
                }
                break;
            }
        }
    }

    return retval;
}

static ULONG ColorWheel_DOMAIN(Class *cl, Object *o, struct gpDomain *msg)
{
    struct ColorWheelData *data = INST_DATA(cl, o);
    LONG                   width = 0x7fff, height = 0x7fff;

    switch (msg->gpd_Which)
    {
        case GDOMAIN_MINIMUM:
            width  = data->frame ? BORDERWHEELSPACINGX * 4 : BORDERWHEELSPACINGX * 2;
            height = data->frame ? BORDERWHEELSPACINGY * 4 : BORDERWHEELSPACINGY * 2;
            break;

        case GDOMAIN_NOMINAL:
            width  = data->frame ? BORDERWHEELSPACINGX * 12 : BORDERWHEELSPACINGX * 10;
            height = width;
            if (msg->gpd_GInfo && msg->gpd_GInfo->gi_DrInfo &&
                msg->gpd_GInfo->gi_DrInfo->dri_Resolution.Y)
                height = (width * msg->gpd_GInfo->gi_DrInfo->dri_Resolution.X) /
                         msg->gpd_GInfo->gi_DrInfo->dri_Resolution.Y;
            break;

        case GDOMAIN_MAXIMUM:
            width  = 0x7fff;
            height = 0x7fff;
            break;
    }

    msg->gpd_Domain.Left   = 0;
    msg->gpd_Domain.Top    = 0;
    msg->gpd_Domain.Width  = width;
    msg->gpd_Domain.Height = height;

    return 1;
}

ULONG classlib_Dispatcher(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                          register Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
        case OM_NEW:          return ColorWheel_NEW(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:      return ColorWheel_DISPOSE(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:       return ColorWheel_SET(cl, o, (struct opSet *)msg, FALSE);
        case OM_GET:          return ColorWheel_GET(cl, o, (struct opGet *)msg);
        case GM_RENDER:       return ColorWheel_RENDER(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:      return ColorWheel_HITTEST(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:     return ColorWheel_GOACTIVE(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT:  return ColorWheel_HANDLEINPUT(cl, o, (struct gpInput *)msg);
        case GM_DOMAIN:       return ColorWheel_DOMAIN(cl, o, (struct gpDomain *)msg);
        default:              return CL_DoSuperMethodA(cl, o, msg);
    }
}
