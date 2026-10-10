/*
 * lxa - colorwheel.gadget, the AmigaOS 3.1 HSB colour wheel BOOPSI class
 * and library (LIBS:gadgets/colorwheel.gadget).
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * Written from the NDK (gadgets/colorwheel.h, colorwheel_gc autodoc) and
 * the behaviour of the 3.1 class (tests/probes/colorwheel/class.c and
 * render.c); doc/gadget-classes.md lists the observed rules.  In short:
 *
 *   - ConvertHSBToRGB()/ConvertRGBToHSB() use 16 bit math and return the
 *     16 bit result in both words (colorwheel_math.h);
 *   - the object keeps hue, saturation and brightness as its master copy
 *     (16 bits each, read back widened to 32 bits) and derives RGB;
 *   - OM_SET applies all HSB attributes first, then the RGB attributes on
 *     top of the colour they give, and returns bit 0 when hue or
 *     saturation changed and bit 1 when the brightness changed;
 *   - an attached gradient slider holds the brightness (its GRAD_CurVal is
 *     0xFFFF - brightness) and is read back on every OM_GET/OM_SET;
 *   - objects can be created without WHEEL_Screen (they then draw the
 *     monochrome wheel); a disabled wheel looks like an enabled one.
 *
 * The wheel: red at the top, hue increasing clockwise, saturation growing
 * from the centre outwards.  On screens with enough free pens it is drawn
 * in colour (a cube of 2..6 levels per gun, of which only the surface -
 * colours with one gun at full intensity - is needed at full brightness),
 * ordered dithered; otherwise in monochrome with the six letters of
 * WHEEL_Abbrv at the primary and secondary colours.
 */

#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <gadgets/colorwheel.h>
#include <gadgets/gradientslider.h>

#include "classlib.h"
#include "colorwheel_math.h"

#define CUBE_MAX        6               /* most levels per gun */
#define CUBE_SIZE       (CUBE_MAX * CUBE_MAX * CUBE_MAX)
#define KNOB_SIZE       7               /* the knob is 7 x 7 pixels */
#define KNOB_HALF       3

struct Wheel
{
    ULONG          hue, sat, bri;       /* master copy, 0..0xFFFF each */
    ULONG          rgb[3];              /* derived from it, 0..0xFFFF each */

    struct Screen *screen;              /* WHEEL_Screen */
    Object        *slider;              /* WHEEL_GradientSlider */
    STRPTR         letters;             /* WHEEL_Abbrv */
    UWORD         *donated;             /* WHEEL_Donation */
    ULONG          max_pens;            /* WHEEL_MaxPens */
    Object        *bevel;               /* WHEEL_BevelBox: frameiclass image */
    struct Hook   *backfill;            /* GA_BackFill */

    /* colour cube pens: pen[(r * levels + g) * levels + b], -1 = unused */
    WORD           levels;              /* 0: monochrome */
    WORD           pen[CUBE_SIZE];
    UBYTE          obtained[CUBE_SIZE]; /* TRUE: ObtainBestPen()ed, release it */

    /* the wheel drawn off-screen, and the mask of its outline */
    struct BitMap *image;
    PLANEPTR       mask;
    ULONG          mask_size;
    WORD           image_w, image_h;

    /* what lies under the knob on the display */
    struct BitMap *under;
    WORD           under_x, under_y;    /* gadget relative */
    BOOL           under_valid;
    BOOL           shown;               /* rendered since the last full redraw */
};

/* the wheel's ellipse inside a gadget box (gadget relative) */
struct WheelShape
{
    LONG cx, cy, rx, ry;
    BOOL ok;                            /* FALSE: the box is too small */
};

const char  classlib_LibName[]    = "colorwheel.gadget";
const char  classlib_LibID[]      = "colorwheel 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct Wheel);

/* --------------------------------------------------------------------------
 * library functions
 */

static VOID cw_ConvertHSBToRGB(register struct ColorWheelHSB *hsb __asm("a0"),
                               register struct ColorWheelRGB *rgb __asm("a1"),
                               register struct Library *base __asm("a6"))
{
    ULONG out[3];

    cwm_hsb_to_rgb(hsb->cw_Hue >> 16, hsb->cw_Saturation >> 16, hsb->cw_Brightness >> 16, out);
    rgb->cw_Red   = CWM_WIDEN(out[0]);
    rgb->cw_Green = CWM_WIDEN(out[1]);
    rgb->cw_Blue  = CWM_WIDEN(out[2]);
}

static VOID cw_ConvertRGBToHSB(register struct ColorWheelRGB *rgb __asm("a0"),
                               register struct ColorWheelHSB *hsb __asm("a1"),
                               register struct Library *base __asm("a6"))
{
    ULONG in[3], hue = 0, sat, bri;

    in[0] = rgb->cw_Red >> 16;
    in[1] = rgb->cw_Green >> 16;
    in[2] = rgb->cw_Blue >> 16;
    /* a grey has no hue (3.1 returns whatever happens to be left): 0 */
    cwm_rgb_to_hsb(in, &hue, &sat, &bri);
    hsb->cw_Hue        = CWM_WIDEN(hue);
    hsb->cw_Saturation = CWM_WIDEN(sat);
    hsb->cw_Brightness = CWM_WIDEN(bri);
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

/* --------------------------------------------------------------------------
 * colour state
 */

static void wheel_derive_rgb(struct Wheel *w)
{
    cwm_hsb_to_rgb(w->hue, w->sat, w->bri, w->rgb);
}

/* the attached slider holds the brightness: GRAD_CurVal = 0xFFFF - brightness */
static void wheel_pull_slider(struct Wheel *w)
{
    ULONG v = 0;

    if (!w->slider)
        return;
    GetAttr(GRAD_CurVal, w->slider, &v);
    w->bri = (0xFFFF - v) & 0xFFFF;
    wheel_derive_rgb(w);
}

static void wheel_push_slider(struct Wheel *w, struct GadgetInfo *gi)
{
    struct TagItem attrs[2];
    struct opSet   msg;

    attrs[0].ti_Tag  = GRAD_CurVal;
    attrs[0].ti_Data = 0xFFFF - w->bri;
    attrs[1].ti_Tag  = TAG_DONE;
    attrs[1].ti_Data = 0;
    msg.MethodID     = OM_SET;
    msg.ops_AttrList = attrs;
    msg.ops_GInfo    = gi;
    CL_DoMethodA(w->slider, (Msg)&msg);
}

static void wheel_notify(Object *o, struct Wheel *w, struct GadgetInfo *gi, BOOL interim, BOOL user)
{
    struct TagItem attrs[5];

    attrs[0].ti_Tag = GA_ID;            attrs[0].ti_Data = ((struct Gadget *)o)->GadgetID;
    attrs[1].ti_Tag = GA_UserInput;     attrs[1].ti_Data = user;
    attrs[2].ti_Tag = WHEEL_Hue;        attrs[2].ti_Data = CWM_WIDEN(w->hue);
    attrs[3].ti_Tag = WHEEL_Saturation; attrs[3].ti_Data = CWM_WIDEN(w->sat);
    attrs[4].ti_Tag = TAG_DONE;         attrs[4].ti_Data = 0;
    classlib_Notify(o, gi, attrs, interim);
}

/* --------------------------------------------------------------------------
 * pens
 */

static void wheel_release_pens(struct Wheel *w)
{
    struct ColorMap *cm = w->screen ? w->screen->ViewPort.ColorMap : NULL;
    LONG             i;

    for (i = 0; i < CUBE_SIZE; i++)
    {
        if (w->obtained[i] && cm)
            ReleasePen(cm, w->pen[i]);
        w->obtained[i] = FALSE;
        w->pen[i]      = -1;
    }
    w->levels = 0;
}

/* one gun level as a 32 bit colour component */
static ULONG cube_gun(LONG level, LONG levels)
{
    return (ULONG)(level * 255 / (levels - 1)) * 0x01010101UL;
}

/* get every pen of the cube's surface; FALSE (nothing kept) if one fails */
static BOOL wheel_try_cube(struct Wheel *w, LONG levels)
{
    static const struct TagItem exact[] =
    {
        { OBP_Precision, PRECISION_EXACT },
        { OBP_FailIfBad, TRUE            },
        { TAG_DONE,      0               }
    };
    struct ViewPort *vp = &w->screen->ViewPort;
    UWORD           *gift = w->donated;
    LONG             r, g, b, i;

    w->levels = levels;
    for (r = 0; r < levels; r++)
    {
        for (g = 0; g < levels; g++)
        {
            for (b = 0; b < levels; b++)
            {
                ULONG R = cube_gun(r, levels), G = cube_gun(g, levels), B = cube_gun(b, levels);
                LONG  pen;

                i = (r * levels + g) * levels + b;
                if (r != levels - 1 && g != levels - 1 && b != levels - 1)
                    continue;           /* inside the cube: never needed */

                if (gift && *gift != (UWORD)~0)
                {
                    /* a donated pen: ours to set */
                    SetRGB32(vp, *gift, R, G, B);
                    w->pen[i] = *gift++;
                    continue;
                }

                pen = ObtainBestPenA(vp->ColorMap, R, G, B, (struct TagItem *)exact);
                if (pen == -1)
                {
                    wheel_release_pens(w);
                    return FALSE;
                }
                w->pen[i]      = pen;
                w->obtained[i] = TRUE;
            }
        }
    }
    return TRUE;
}

/* the most levels the screen and WHEEL_MaxPens allow */
static void wheel_get_pens(struct Wheel *w)
{
    LONG depth = GetBitMapAttr(w->screen->RastPort.BitMap, BMA_DEPTH);
    LONG gifts = 0, levels, needed;

    while (w->donated && w->donated[gifts] != (UWORD)~0)
        gifts++;

    for (levels = CUBE_MAX; levels >= 2; levels--)
    {
        needed = levels * levels * levels - (levels - 1) * (levels - 1) * (levels - 1);
        if (needed > gifts + (LONG)w->max_pens)
            continue;
        if (depth < 31 && needed > (1L << depth))
            continue;
        if (wheel_try_cube(w, levels))
            return;
    }
    w->levels = 0;
}

/* --------------------------------------------------------------------------
 * geometry
 */

static void wheel_shape(struct Wheel *w, struct IBox *gb, struct WheelShape *sh)
{
    LONG inset = w->bevel ? 4 : 2;
    LONG width = (gb->Width - 2 * inset) & ~1, height = (gb->Height - 2 * inset) & ~1;

    sh->ok = width >= 4 && height >= 4;
    sh->rx = width / 2;
    sh->ry = height / 2;
    sh->cx = inset + sh->rx;
    sh->cy = inset + sh->ry;
}

/* half the width of the ellipse (rx, ry) in row dy, -1 outside */
static LONG ellipse_span(LONG rx, LONG ry, LONG dy)
{
    if (dy < 0)
        dy = -dy;
    if (dy > ry)
        return -1;
    return rx * (LONG)cwm_isqrt((ULONG)(ry * ry - dy * dy)) / ry;
}

/* hue and saturation at a position relative to the wheel's centre */
static void wheel_colour_at(struct WheelShape *sh, LONG dx, LONG dy, ULONG *hue, ULONG *sat)
{
    LONG  ny = dy * sh->rx / sh->ry;    /* squeeze the ellipse into a circle */
    ULONG dist = cwm_isqrt((ULONG)(dx * dx + ny * ny));

    *hue = cwm_direction(dx, ny);
    *sat = dist >= (ULONG)sh->rx ? 0xFFFF : dist * 0xFFFF / (ULONG)sh->rx;
}

/* the knob's centre, gadget relative, kept inside the gadget box */
static void wheel_knob_at(struct Wheel *w, struct WheelShape *sh, struct IBox *gb, LONG *x, LONG *y)
{
    LONG ox = (LONG)((ULONG)sh->rx * w->sat >> 16), oy = (LONG)((ULONG)sh->ry * w->sat >> 16);

    *x = sh->cx + ox * cwm_sin(w->hue) / 4096;
    *y = sh->cy - oy * cwm_cos(w->hue) / 4096;

    if (*x < KNOB_HALF)
        *x = KNOB_HALF;
    if (*x > gb->Width - 1 - KNOB_HALF)
        *x = gb->Width - 1 - KNOB_HALF;
    if (*y < KNOB_HALF)
        *y = KNOB_HALF;
    if (*y > gb->Height - 1 - KNOB_HALF)
        *y = gb->Height - 1 - KNOB_HALF;
}

/* --------------------------------------------------------------------------
 * drawing the wheel
 */

/* 4 x 4 ordered dither threshold, 0..15 (recursive Bayer pattern) */
static LONG bayer4(LONG x, LONG y)
{
    LONG lo = (((x ^ y) & 1) << 1) | (y & 1);
    LONG hi = ((((x ^ y) >> 1) & 1) << 1) | ((y >> 1) & 1);

    return (lo << 2) | hi;
}

/* the pen for a full brightness colour at pixel (x, y) */
static UBYTE wheel_pen(struct Wheel *w, ULONG hue, ULONG sat, LONG x, LONG y)
{
    ULONG rgb[3], idx = 0;
    LONG  t = bayer4(x, y), steps = w->levels - 1, c;

    cwm_hsb_to_rgb(hue, sat, 0xFFFF, rgb);
    for (c = 0; c < 3; c++)
    {
        ULONG scaled = rgb[c] * (ULONG)steps;
        ULONG level = scaled / 0xFFFF, rest = scaled % 0xFFFF;

        if (rest * 16 > (ULONG)t * 0xFFFF + 0x7FFF && level < (ULONG)steps)
            level++;
        idx = idx * w->levels + level;
    }
    return (UBYTE)w->pen[idx];
}

/* the monochrome wheel's decorations: spokes between the colours, letters */
static void wheel_draw_mono(struct Wheel *w, struct RastPort *rp, struct DrawInfo *dri,
                            struct WheelShape *sh)
{
    LONG i, n = 0;

    SetDrMd(rp, JAM1);
    SetAPen(rp, dri->dri_Pens[SHINEPEN]);

    /* spokes at the borders between the six colours, from half the radius out */
    for (i = 0; i < 6; i++)
    {
        ULONG a = (ULONG)(2 * i + 1) * 0x10000 / 12;
        LONG  s = cwm_sin(a), c = cwm_cos(a);

        Move(rp, sh->cx + sh->rx * s / 8192, sh->cy - sh->ry * c / 8192);
        Draw(rp, sh->cx + (sh->rx - 2) * s / 4096, sh->cy - (sh->ry - 2) * c / 4096);
    }

    if (!w->letters)
        return;
    while (n < 6 && w->letters[n])
        n++;
    if (dri->dri_Font)
        SetFont(rp, dri->dri_Font);

    /* the letters (green, cyan, blue, magenta, red, yellow) at two thirds of
       the radius; green sits at a third of a turn */
    for (i = 0; i < n; i++)
    {
        ULONG a = (ULONG)((i + 2) % 6) * 0x10000 / 6;
        LONG  x = sh->cx + 2 * sh->rx * cwm_sin(a) / (3 * 4096);
        LONG  y = sh->cy - 2 * sh->ry * cwm_cos(a) / (3 * 4096);

        Move(rp, x - TextLength(rp, (STRPTR)&w->letters[i], 1) / 2,
             y - rp->TxHeight / 2 + rp->TxBaseline);
        Text(rp, (STRPTR)&w->letters[i], 1);
    }
}

static void wheel_free_image(struct Wheel *w)
{
    WaitBlit();
    if (w->mask)
        FreeVec(w->mask);
    if (w->image)
        FreeBitMap(w->image);
    w->mask    = NULL;
    w->image   = NULL;
    w->image_w = w->image_h = 0;
}

/* draw the wheel (with the bevel box, or its mask) into the image */
static BOOL wheel_build_image(struct Wheel *w, struct RastPort *rp, struct DrawInfo *dri,
                              struct IBox *gb, struct WheelShape *sh)
{
    struct RastPort irp, lrp;
    struct BitMap  *line = NULL;
    UBYTE          *row = NULL;
    LONG            x, y, bpr;

    wheel_free_image(w);
    w->image = AllocBitMap(gb->Width, gb->Height, GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                           BMF_MINPLANES, rp->BitMap);
    if (!w->image)
        return FALSE;
    w->image_w = gb->Width;
    w->image_h = gb->Height;
    bpr = w->image->BytesPerRow;

    if (!w->bevel)
    {
        /* without a box only the wheel is copied to the display */
        w->mask_size = (ULONG)bpr * gb->Height;
        w->mask = AllocVec(w->mask_size, MEMF_CHIP | MEMF_CLEAR);
        if (!w->mask)
        {
            wheel_free_image(w);
            return FALSE;
        }
    }

    InitRastPort(&irp);
    irp.BitMap = w->image;
    SetRast(&irp, dri->dri_Pens[BACKGROUNDPEN]);
    if (w->bevel)
    {
        SetAttrs(w->bevel, IA_Width, gb->Width, IA_Height, gb->Height, TAG_DONE);
        DrawImageState(&irp, (struct Image *)w->bevel, 0, 0, IDS_NORMAL, dri);
    }

    if (!sh->ok)
        return TRUE;

    if (w->levels)
    {
        /* colour wheel: one row at a time through WritePixelLine8() */
        line = AllocBitMap(gb->Width, 1, GetBitMapAttr(w->image, BMA_DEPTH), BMF_MINPLANES, w->image);
        row  = AllocVec((gb->Width + 15) & ~15, MEMF_ANY);
        InitRastPort(&lrp);
        lrp.BitMap = line;
    }

    SetDrMd(&irp, JAM1);
    SetAPen(&irp, dri->dri_Pens[SHADOWPEN]);
    for (y = sh->cy - sh->ry; y <= sh->cy + sh->ry; y++)
    {
        LONG dy = y - sh->cy, span = ellipse_span(sh->rx, sh->ry, dy);

        /* the mask covers the outline too (an ellipse half a pixel larger) */
        if (w->mask)
        {
            LONG outer = (2 * sh->rx + 1) * (LONG)cwm_isqrt((ULONG)((2 * sh->ry + 1) * (2 * sh->ry + 1) -
                                                             4 * dy * dy)) / (2 * (2 * sh->ry + 1));
            for (x = sh->cx - outer; x <= sh->cx + outer; x++)
                w->mask[y * bpr + (x >> 3)] |= 0x80 >> (x & 7);
        }

        if (span < 0 || dy == -sh->ry || dy == sh->ry)
            continue;

        if (line && row)
        {
            for (x = 0; x <= 2 * span; x++)
            {
                ULONG hue, sat;

                wheel_colour_at(sh, x - span, dy, &hue, &sat);
                row[x] = wheel_pen(w, hue, sat, sh->cx - span + x, y);
            }
            WritePixelLine8(&irp, sh->cx - span, y, 2 * span + 1, row, &lrp);
        }
        else
            RectFill(&irp, sh->cx - span, y, sh->cx + span, y);    /* monochrome disc */
    }

    if (line)
        FreeBitMap(line);
    if (row)
        FreeVec(row);

    if (!w->levels)
        wheel_draw_mono(w, &irp, dri, sh);

    /* a two pixel outline */
    SetAPen(&irp, dri->dri_Pens[SHADOWPEN]);
    DrawEllipse(&irp, sh->cx, sh->cy, sh->rx, sh->ry);
    DrawEllipse(&irp, sh->cx, sh->cy, sh->rx - 1, sh->ry - 1);
    return TRUE;
}

/* --------------------------------------------------------------------------
 * the knob
 */

static const char knob_shape[KNOB_SIZE][KNOB_SIZE + 1] =
{
    "  ###  ",
    " #   # ",
    "#  o  #",
    "# ooo #",
    "#  o  #",
    " #   # ",
    "  ###  ",
};

static void knob_paint(struct RastPort *rp, struct DrawInfo *dri, LONG left, LONG top)
{
    LONG x, y;

    SetDrMd(rp, JAM1);
    for (y = 0; y < KNOB_SIZE; y++)
    {
        for (x = 0; x < KNOB_SIZE; x++)
        {
            char c = knob_shape[y][x];

            if (c == ' ')
                continue;
            SetAPen(rp, dri->dri_Pens[c == '#' ? SHADOWPEN : SHINEPEN]);
            WritePixel(rp, left + x, top + y);
        }
    }
}

/* save what is under the knob's new place, then draw it */
static void knob_show(struct Wheel *w, struct RastPort *rp, struct DrawInfo *dri,
                      struct IBox *gb, struct WheelShape *sh)
{
    LONG x, y;

    wheel_knob_at(w, sh, gb, &x, &y);
    w->under_x = x - KNOB_HALF;
    w->under_y = y - KNOB_HALF;

    if (!w->under)
        w->under = AllocBitMap(KNOB_SIZE, KNOB_SIZE, GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                               BMF_MINPLANES, rp->BitMap);
    w->under_valid = FALSE;
    if (w->under)
    {
        struct RastPort urp;

        InitRastPort(&urp);
        urp.BitMap = w->under;
        ClipBlit(rp, gb->Left + w->under_x, gb->Top + w->under_y, &urp, 0, 0,
                 KNOB_SIZE, KNOB_SIZE, 0xC0);
        w->under_valid = TRUE;
    }
    knob_paint(rp, dri, gb->Left + w->under_x, gb->Top + w->under_y);
}

static void knob_hide(struct Wheel *w, struct RastPort *rp, struct IBox *gb)
{
    if (w->under_valid)
        BltBitMapRastPort(w->under, 0, 0, rp, gb->Left + w->under_x, gb->Top + w->under_y,
                          KNOB_SIZE, KNOB_SIZE, 0xC0);
    w->under_valid = FALSE;
}

static ULONG wheel_render(Class *cl, Object *o, struct gpRender *msg)
{
    struct Wheel      *w = INST_DATA(cl, o);
    struct RastPort   *rp = msg->gpr_RPort;
    struct DrawInfo   *dri;
    struct IBox        gb;
    struct WheelShape  sh;

    if (!rp || !msg->gpr_GInfo || !(dri = msg->gpr_GInfo->gi_DrInfo))
        return 0;

    classlib_GadgetBox((struct Gadget *)o, msg->gpr_GInfo, &gb);
    if (gb.Width <= 0 || gb.Height <= 0)
        return 0;
    wheel_shape(w, &gb, &sh);

    if (msg->gpr_Redraw == GREDRAW_UPDATE && w->shown &&
        w->image && w->image_w == gb.Width && w->image_h == gb.Height)
    {
        /* only the knob moved */
        knob_hide(w, rp, &gb);
        knob_show(w, rp, dri, &gb, &sh);
        return 1;
    }

    w->shown       = FALSE;
    w->under_valid = FALSE;
    if (msg->gpr_Redraw == GREDRAW_REDRAW || !w->image || w->image_w != gb.Width ||
        w->image_h != gb.Height)
    {
        if (!wheel_build_image(w, rp, dri, &gb, &sh))
            return 0;
    }

    if (w->mask)
    {
        /* clear the box (through the GA_BackFill hook), then copy the wheel */
        struct Hook *old = NULL;

        if (w->backfill && rp->Layer)
            old = InstallLayerHook(rp->Layer, w->backfill);
        EraseRect(rp, gb.Left, gb.Top, gb.Left + gb.Width - 1, gb.Top + gb.Height - 1);
        if (w->backfill && rp->Layer)
            InstallLayerHook(rp->Layer, old);
        BltMaskBitMapRastPort(w->image, 0, 0, rp, gb.Left, gb.Top, gb.Width, gb.Height,
                              0xE0, w->mask);
    }
    else
        BltBitMapRastPort(w->image, 0, 0, rp, gb.Left, gb.Top, gb.Width, gb.Height, 0xC0);

    if (sh.ok)
    {
        knob_show(w, rp, dri, &gb, &sh);
        w->shown = TRUE;
    }

    /* 3.1 draws a disabled colour wheel like an enabled one */
    return 1;
}

/* --------------------------------------------------------------------------
 * attributes
 */

static ULONG wheel_set(Class *cl, Object *o, struct opSet *msg, BOOL creating)
{
    struct Wheel   *w = INST_DATA(cl, o);
    struct TagItem *state, *t;
    ULONG           hue, sat, bri, given[3], result = 0;
    UBYTE           rgb_given = 0;
    BOOL            hsb_given = FALSE, restyled = FALSE;
    UWORD           was_disabled = ((struct Gadget *)o)->Flags & GFLG_DISABLED;

    if (!creating)
        CL_DoSuperMethodA(cl, o, (Msg)msg);

    wheel_pull_slider(w);
    hue = w->hue;
    sat = w->sat;
    bri = w->bri;
    given[0] = given[1] = given[2] = 0;

    /* the HSB attributes act at once; the RGB ones are collected and laid
       over the resulting colour afterwards (3.1's order) */
    state = msg->ops_AttrList;
    while ((t = NextTagItem(&state)))
    {
        ULONG v = t->ti_Data;

        switch (t->ti_Tag)
        {
            case WHEEL_Hue:        hue = v >> 16; hsb_given = TRUE; break;
            case WHEEL_Saturation: sat = v >> 16; hsb_given = TRUE; break;
            case WHEEL_Brightness: bri = v >> 16; hsb_given = TRUE; break;
            case WHEEL_HSB:
                if (v)
                {
                    struct ColorWheelHSB *h = (struct ColorWheelHSB *)v;

                    hue = h->cw_Hue >> 16;
                    sat = h->cw_Saturation >> 16;
                    bri = h->cw_Brightness >> 16;
                    hsb_given = TRUE;
                }
                break;
            case WHEEL_Red:   given[0] = v >> 16; rgb_given |= 1; break;
            case WHEEL_Green: given[1] = v >> 16; rgb_given |= 2; break;
            case WHEEL_Blue:  given[2] = v >> 16; rgb_given |= 4; break;
            case WHEEL_RGB:
                if (v)
                {
                    struct ColorWheelRGB *c = (struct ColorWheelRGB *)v;

                    given[0] = c->cw_Red >> 16;
                    given[1] = c->cw_Green >> 16;
                    given[2] = c->cw_Blue >> 16;
                    rgb_given = 7;
                }
                break;
            case WHEEL_GradientSlider:
                w->slider = (Object *)v;
                break;
            case GA_BackFill:
                w->backfill = (struct Hook *)v;
                break;
            case GA_Disabled:
                if ((((struct Gadget *)o)->Flags & GFLG_DISABLED) != was_disabled)
                    restyled = TRUE;
                break;
        }
    }

    if (rgb_given)
    {
        ULONG c[3];
        int   i;

        cwm_hsb_to_rgb(hue, sat, bri, c);
        for (i = 0; i < 3; i++)
            if (rgb_given & (1 << i))
                c[i] = given[i];
        cwm_rgb_to_hsb(c, &hue, &sat, &bri);   /* a grey keeps the hue */
    }

    if (hue != w->hue || sat != w->sat)
        result |= 1;
    if (bri != w->bri)
        result |= 2;
    if (hsb_given || rgb_given)
    {
        w->hue = hue;
        w->sat = sat;
        w->bri = bri;
        wheel_derive_rgb(w);
    }

    if (result && !creating)
        wheel_notify(o, w, msg->ops_GInfo, FALSE, FALSE);
    if (w->slider && (creating || (result & 2)))
        wheel_push_slider(w, msg->ops_GInfo);
    if (!creating && (result || restyled))
        classlib_Render(o, msg->ops_GInfo, restyled ? GREDRAW_REDRAW : GREDRAW_UPDATE);

    return result;
}

static ULONG wheel_new(Class *cl, Object *o, struct opSet *msg)
{
    Object       *obj = (Object *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    struct Wheel *w;
    LONG          i;

    if (!obj)
        return 0;

    w = INST_DATA(cl, obj);
    w->hue = 0;
    w->sat = 0xFFFF;
    w->bri = 0xFFFF;
    wheel_derive_rgb(w);

    w->screen   = (struct Screen *)GetTagData(WHEEL_Screen, 0, msg->ops_AttrList);
    w->slider   = NULL;
    w->letters  = (STRPTR)GetTagData(WHEEL_Abbrv, (ULONG)"GCBMRY", msg->ops_AttrList);
    w->donated  = (UWORD *)GetTagData(WHEEL_Donation, 0, msg->ops_AttrList);
    w->max_pens = GetTagData(WHEEL_MaxPens, 256, msg->ops_AttrList);
    w->backfill = NULL;
    w->bevel    = NULL;
    if (GetTagData(WHEEL_BevelBox, FALSE, msg->ops_AttrList))
        w->bevel = NewObject(NULL, (ClassID)FRAMEICLASS, IA_FrameType, FRAME_BUTTON,
                             IA_EdgesOnly, TRUE, TAG_DONE);

    w->levels = 0;
    for (i = 0; i < CUBE_SIZE; i++)
    {
        w->pen[i]      = -1;
        w->obtained[i] = FALSE;
    }
    w->image       = NULL;
    w->mask        = NULL;
    w->image_w     = w->image_h = 0;
    w->under       = NULL;
    w->under_valid = FALSE;
    w->shown       = FALSE;

    wheel_set(cl, obj, msg, TRUE);

    if (w->screen)
        wheel_get_pens(w);
    return (ULONG)obj;
}

static ULONG wheel_get(Class *cl, Object *o, struct opGet *msg)
{
    struct Wheel *w = INST_DATA(cl, o);
    ULONG        *store = msg->opg_Storage;

    wheel_pull_slider(w);

    switch (msg->opg_AttrID)
    {
        case WHEEL_Hue:        *store = CWM_WIDEN(w->hue);    return 1;
        case WHEEL_Saturation: *store = CWM_WIDEN(w->sat);    return 1;
        case WHEEL_Brightness: *store = CWM_WIDEN(w->bri);    return 1;
        case WHEEL_Red:        *store = CWM_WIDEN(w->rgb[0]); return 1;
        case WHEEL_Green:      *store = CWM_WIDEN(w->rgb[1]); return 1;
        case WHEEL_Blue:       *store = CWM_WIDEN(w->rgb[2]); return 1;
        case WHEEL_HSB:
        {
            struct ColorWheelHSB *h = (struct ColorWheelHSB *)store;

            h->cw_Hue        = CWM_WIDEN(w->hue);
            h->cw_Saturation = CWM_WIDEN(w->sat);
            h->cw_Brightness = CWM_WIDEN(w->bri);
            return 1;
        }
        case WHEEL_RGB:
        {
            struct ColorWheelRGB *c = (struct ColorWheelRGB *)store;

            c->cw_Red   = CWM_WIDEN(w->rgb[0]);
            c->cw_Green = CWM_WIDEN(w->rgb[1]);
            c->cw_Blue  = CWM_WIDEN(w->rgb[2]);
            return 1;
        }
    }
    return CL_DoSuperMethodA(cl, o, (Msg)msg);
}

static ULONG wheel_dispose(Class *cl, Object *o, Msg msg)
{
    struct Wheel *w = INST_DATA(cl, o);

    wheel_free_image(w);
    if (w->under)
        FreeBitMap(w->under);
    if (w->bevel)
        DisposeObject(w->bevel);
    wheel_release_pens(w);
    return CL_DoSuperMethodA(cl, o, msg);
}

/* --------------------------------------------------------------------------
 * input
 */

/* gadget relative mouse position inside the wheel? */
static BOOL wheel_hit(struct Wheel *w, struct GadgetInfo *gi, Object *o, LONG mx, LONG my,
                      struct WheelShape *sh)
{
    struct IBox gb;
    LONG        span;

    classlib_GadgetBox((struct Gadget *)o, gi, &gb);
    wheel_shape(w, &gb, sh);
    if (!sh->ok)
        return FALSE;
    span = ellipse_span(sh->rx, sh->ry, my - sh->cy);
    return span >= 0 && mx >= sh->cx - span && mx <= sh->cx + span;
}

/* hue and saturation from the mouse; TRUE if they changed */
static BOOL wheel_track(struct Wheel *w, struct WheelShape *sh, LONG mx, LONG my)
{
    ULONG hue, sat;

    wheel_colour_at(sh, mx - sh->cx, my - sh->cy, &hue, &sat);
    if (hue == w->hue && sat == w->sat)
        return FALSE;
    w->hue = hue;
    w->sat = sat;
    wheel_derive_rgb(w);
    return TRUE;
}

static ULONG wheel_hittest(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct WheelShape sh;

    if (((struct Gadget *)o)->Flags & GFLG_DISABLED)
        return 0;
    return wheel_hit(INST_DATA(cl, o), msg->gpht_GInfo, o, msg->gpht_Mouse.X, msg->gpht_Mouse.Y, &sh)
           ? GMR_GADGETHIT : 0;
}

static ULONG wheel_goactive(Class *cl, Object *o, struct gpInput *msg)
{
    struct Wheel      *w = INST_DATA(cl, o);
    struct WheelShape  sh;

    if (!msg->gpi_IEvent ||
        !wheel_hit(w, msg->gpi_GInfo, o, msg->gpi_Mouse.X, msg->gpi_Mouse.Y, &sh))
        return GMR_NOREUSE;

    wheel_pull_slider(w);
    wheel_track(w, &sh, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
    wheel_notify(o, w, msg->gpi_GInfo, TRUE, TRUE);
    classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
    return GMR_MEACTIVE;
}

static ULONG wheel_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    struct Wheel      *w = INST_DATA(cl, o);
    struct InputEvent *ie = msg->gpi_IEvent;
    struct WheelShape  sh;
    struct IBox        gb;

    if (ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    switch (ie->ie_Code)
    {
        case SELECTUP:
            wheel_notify(o, w, msg->gpi_GInfo, FALSE, TRUE);
            *msg->gpi_Termination = ((struct Gadget *)o)->GadgetID;
            return GMR_NOREUSE | GMR_VERIFY;

        case MENUDOWN:
            return GMR_NOREUSE;

        case IECODE_NOBUTTON:
            classlib_GadgetBox((struct Gadget *)o, msg->gpi_GInfo, &gb);
            wheel_shape(w, &gb, &sh);
            if (sh.ok && wheel_track(w, &sh, msg->gpi_Mouse.X, msg->gpi_Mouse.Y))
            {
                wheel_notify(o, w, msg->gpi_GInfo, TRUE, TRUE);
                classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
            }
            break;
    }
    return GMR_MEACTIVE;
}

static ULONG wheel_domain(Class *cl, Object *o, struct gpDomain *msg)
{
    struct Wheel    *w = INST_DATA(cl, o);
    struct DrawInfo *dri = msg->gpd_GInfo ? msg->gpd_GInfo->gi_DrInfo : NULL;
    LONG             edge = w->bevel ? 8 : 4, width, height;

    switch (msg->gpd_Which)
    {
        case GDOMAIN_MINIMUM:
            width = height = edge + 8;
            break;
        case GDOMAIN_NOMINAL:
            /* round on the screen: correct the height for the pixel aspect */
            width = height = edge + 40;
            if (dri && dri->dri_Resolution.X && dri->dri_Resolution.Y)
                height = edge + 40 * dri->dri_Resolution.X / dri->dri_Resolution.Y;
            break;
        default:
            width = height = 0x7FFF;
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
        case OM_NEW:         return wheel_new(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:     return wheel_dispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:      return wheel_set(cl, o, (struct opSet *)msg, FALSE);
        case OM_GET:         return wheel_get(cl, o, (struct opGet *)msg);
        case GM_RENDER:      return wheel_render(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:     return wheel_hittest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:    return wheel_goactive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT: return wheel_handleinput(cl, o, (struct gpInput *)msg);
        case GM_DOMAIN:      return wheel_domain(cl, o, (struct gpDomain *)msg);
    }
    return CL_DoSuperMethodA(cl, o, msg);
}
