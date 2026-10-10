/*
 * lxa - gradientslider.gadget, the AmigaOS 3.1 BOOPSI gradient slider
 * class (LIBS:gadgets/gradientslider.gadget).
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * Written from the NDK (gadgets/gradientslider.h, gradientslider_gc
 * autodoc) and the behaviour of the 3.1 class
 * (tests/probes/gradientslider/class.c, tests/probes/colorwheel/render.c);
 * doc/gadget-classes.md lists the observed rules.  In short:
 *
 *   - OM_GET only knows GRAD_CurVal;
 *   - GRAD_CurVal is clamped to GRAD_MaxVal, a new GRAD_MaxVal clamps
 *     GRAD_CurVal (no rescaling);
 *   - OM_SET returns 1 when GRAD_CurVal changed or GRAD_MaxVal or
 *     GRAD_PenArray were given, else 0;
 *   - the look: a button frame, the knob track inside it (2 pixels from
 *     the sides, 1 from top and bottom), the gradient one pixel further
 *     in, dithered with 3.1's 4x4 threshold matrix; the knob is a black
 *     bar, with a gradient behind it surrounded by a background pixel;
 *     disabled sliders look like enabled ones.
 */

#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <intuition/screens.h>
#include <intuition/icclass.h>
#include <devices/inputevent.h>
#include <gadgets/gradientslider.h>

#include "classlib.h"

struct Slider
{
    ULONG          max;             /* GRAD_MaxVal */
    ULONG          cur;             /* GRAD_CurVal */
    ULONG          skip;            /* GRAD_SkipVal */
    ULONG          undo;            /* the value when the knob was grabbed */
    UWORD         *pens;            /* GRAD_PenArray */
    WORD           npens;
    WORD           knob;            /* GRAD_KnobPixels */
    BOOL           vertical;        /* PGA_Freedom == LORIENT_VERT */
    WORD           grab_pos;        /* knob offset in the track when grabbed */
    WORD           grab_mouse;      /* mouse position (along the track) then */
    WORD           last_x, last_y;  /* last mouse position seen while dragging */
    Object        *frame;           /* frameiclass image around the slider */
    struct BitMap *backdrop;        /* frame and gradient, without the knob */
    WORD           backdrop_w, backdrop_h;
};

const char  classlib_LibName[]    = "gradientslider.gadget";
const char  classlib_LibID[]      = "gradientslider 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct Slider);

APTR classlib_FuncTab[] =
{
    (APTR)classlib_Open,
    (APTR)classlib_Close,
    (APTR)classlib_Expunge,
    (APTR)classlib_ExtFunc,
    (APTR)-1
};

/* --------------------------------------------------------------------------
 * geometry (all boxes relative to the gadget's top left corner)
 */

/* the area the knob moves in */
static void track_box(struct IBox *gadget, struct IBox *t)
{
    t->Left   = 2;
    t->Top    = 1;
    t->Width  = gadget->Width - 4;
    t->Height = gadget->Height - 2;
}

/* track length along the slider's direction */
static LONG track_length(struct Slider *s, struct IBox *t)
{
    return s->vertical ? t->Height : t->Width;
}

/* how far the knob can travel */
static LONG knob_travel(struct Slider *s, struct IBox *t)
{
    LONG travel = track_length(s, t) - s->knob;

    return travel > 0 ? travel : 0;
}

/* the knob offset in the track: the value scaled to the travel, rounded */
static LONG knob_offset(struct Slider *s, struct IBox *t)
{
    ULONG max = s->max ? s->max : 1;
    ULONG val = s->cur < max ? s->cur : max;
    unsigned long long scaled = (unsigned long long)val * (ULONG)knob_travel(s, t) + max / 2;

    return (LONG)(scaled / max);
}

static void knob_box(struct Slider *s, struct IBox *t, struct IBox *k)
{
    LONG off = knob_offset(s, t);

    *k = *t;
    if (s->vertical)
    {
        k->Top   += off;
        k->Height = s->knob;
    }
    else
    {
        k->Left += off;
        k->Width = s->knob;
    }
}

/* --------------------------------------------------------------------------
 * rendering
 */

/*
 * 3.1's ordered dither between two pens: along the gradient the level
 * (pos + 1) * 17 / length runs from 0 to 17; a pixel takes the second pen
 * where the level reaches its threshold.
 */
static const UBYTE dither_threshold[4][4] =
{
    { 16,  4, 13,  1 },
    {  8, 12,  5,  9 },
    { 14,  2, 15,  3 },
    {  6, 10,  7, 11 },
};

/* one segment [from, to] (along the gradient) blending pen a into pen b */
static void dither_segment(struct Slider *s, struct RastPort *rp, struct IBox *g,
                           LONG from, LONG to, UWORD a, UWORD b)
{
    LONG len = to - from + 1, along, across, across_len = s->vertical ? g->Width : g->Height;

    SetAPen(rp, a);
    if (s->vertical)
        RectFill(rp, g->Left, g->Top + from, g->Left + g->Width - 1, g->Top + to);
    else
        RectFill(rp, g->Left + from, g->Top, g->Left + to, g->Top + g->Height - 1);

    SetAPen(rp, b);
    for (along = from; along <= to; along++)
    {
        LONG level = (along - from + 1) * 17 / len;

        for (across = 0; across < across_len; across++)
        {
            LONG x = s->vertical ? across : along;
            LONG y = s->vertical ? along : across;

            if (level >= dither_threshold[y & 3][x & 3])
                WritePixel(rp, g->Left + x, g->Top + y);
        }
    }
}

static void draw_gradient(struct Slider *s, struct RastPort *rp, struct DrawInfo *dri, struct IBox *g)
{
    LONG len = s->vertical ? g->Height : g->Width, seg, from, to;

    if (g->Width < 2 || g->Height < 2)
        return;

    SetDrMd(rp, JAM1);
    if (s->npens < 2)
    {
        /* no pens: the screen's background, a single pen: that pen */
        SetAPen(rp, s->npens == 1 ? s->pens[0] : dri->dri_Pens[BACKGROUNDPEN]);
        RectFill(rp, g->Left, g->Top, g->Left + g->Width - 1, g->Top + g->Height - 1);
        return;
    }

    /* the pens at evenly spaced points, dithered in between */
    for (seg = 0; seg < s->npens - 1; seg++)
    {
        from = len * seg / (s->npens - 1);
        to   = len * (seg + 1) / (s->npens - 1) - 1;
        if (to >= from)
            dither_segment(s, rp, g, from, to, s->pens[seg], s->pens[seg + 1]);
    }
}

static void draw_knob(struct Slider *s, struct RastPort *rp, struct DrawInfo *dri, struct IBox *k)
{
    LONG x1 = k->Left + k->Width - 1, y1 = k->Top + k->Height - 1;

    if (k->Width <= 2 || k->Height <= 2)
        return;

    SetDrMd(rp, JAM1);
    SetAfPt(rp, NULL, 0);
    if (s->npens >= 2)
    {
        /* over a gradient the bar is surrounded by a background pixel */
        SetAPen(rp, dri->dri_Pens[BACKGROUNDPEN]);
        RectFill(rp, k->Left, k->Top, x1, y1);
        SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
        RectFill(rp, k->Left + 1, k->Top + 1, x1 - 1, y1 - 1);
    }
    else
    {
        SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
        RectFill(rp, k->Left, k->Top, x1, y1);
    }
}

static void free_backdrop(struct Slider *s)
{
    if (s->backdrop)
    {
        WaitBlit();
        FreeBitMap(s->backdrop);
        s->backdrop = NULL;
    }
}

/* draw frame and gradient into the off-screen backdrop */
static BOOL build_backdrop(struct Slider *s, struct RastPort *rp, struct DrawInfo *dri, struct IBox *gb)
{
    struct RastPort brp;
    struct IBox     t, g;

    if (!s->backdrop || s->backdrop_w != gb->Width || s->backdrop_h != gb->Height)
    {
        free_backdrop(s);
        s->backdrop = AllocBitMap(gb->Width, gb->Height, GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                                  BMF_MINPLANES, rp->BitMap);
        if (!s->backdrop)
            return FALSE;
        s->backdrop_w = gb->Width;
        s->backdrop_h = gb->Height;
    }

    InitRastPort(&brp);
    brp.BitMap = s->backdrop;

    if (s->frame)
    {
        SetAttrs(s->frame, IA_Width, gb->Width, IA_Height, gb->Height, TAG_DONE);
        DrawImageState(&brp, (struct Image *)s->frame, 0, 0, IDS_NORMAL, dri);
    }

    track_box(gb, &t);
    g.Left   = t.Left + 1;
    g.Top    = t.Top + 1;
    g.Width  = t.Width - 2;
    g.Height = t.Height - 2;
    draw_gradient(s, &brp, dri, &g);
    return TRUE;
}

static ULONG slider_render(Class *cl, Object *o, struct gpRender *msg)
{
    struct Slider   *s = INST_DATA(cl, o);
    struct RastPort *rp = msg->gpr_RPort;
    struct DrawInfo *dri;
    struct IBox      gb, t, k;

    if (!rp || !msg->gpr_GInfo || !(dri = msg->gpr_GInfo->gi_DrInfo))
        return 0;

    classlib_GadgetBox((struct Gadget *)o, msg->gpr_GInfo, &gb);
    if (gb.Width <= 0 || gb.Height <= 0)
        return 0;
    track_box(&gb, &t);
    knob_box(s, &t, &k);

    if (msg->gpr_Redraw == GREDRAW_UPDATE && s->backdrop &&
        s->backdrop_w == gb.Width && s->backdrop_h == gb.Height)
    {
        /* only the track changes: put the backdrop back, then the knob */
        BltBitMapRastPort(s->backdrop, t.Left, t.Top, rp, gb.Left + t.Left, gb.Top + t.Top,
                          t.Width, t.Height, 0xC0);
    }
    else
    {
        if (!build_backdrop(s, rp, dri, &gb))
            return 0;
        BltBitMapRastPort(s->backdrop, 0, 0, rp, gb.Left, gb.Top, gb.Width, gb.Height, 0xC0);
    }

    if (k.Width > 0 && k.Height > 0 && k.Width <= t.Width && k.Height <= t.Height)
    {
        k.Left += gb.Left;
        k.Top  += gb.Top;
        draw_knob(s, rp, dri, &k);
    }

    /* 3.1 draws a disabled gradient slider like an enabled one */
    return 1;
}

/* --------------------------------------------------------------------------
 * attributes
 */

static void slider_notify(Object *o, struct Slider *s, struct GadgetInfo *gi, BOOL interim, BOOL user)
{
    struct TagItem attrs[4];

    attrs[0].ti_Tag = GA_ID;        attrs[0].ti_Data = ((struct Gadget *)o)->GadgetID;
    attrs[1].ti_Tag = GA_UserInput; attrs[1].ti_Data = user;
    attrs[2].ti_Tag = GRAD_CurVal;  attrs[2].ti_Data = s->cur;
    attrs[3].ti_Tag = TAG_DONE;     attrs[3].ti_Data = 0;
    classlib_Notify(o, gi, attrs, interim);
}

static ULONG slider_set(Class *cl, Object *o, struct opSet *msg, BOOL creating)
{
    struct Slider  *s = INST_DATA(cl, o);
    struct TagItem *state = msg->ops_AttrList, *t;
    ULONG           before = s->cur, result = 0;
    UWORD           was_disabled = ((struct Gadget *)o)->Flags & GFLG_DISABLED;
    BOOL            moved = FALSE, restyled = FALSE;

    if (!creating)
        CL_DoSuperMethodA(cl, o, (Msg)msg);

    while ((t = NextTagItem(&state)))
    {
        switch (t->ti_Tag)
        {
            case GRAD_CurVal:
                s->cur = t->ti_Data < s->max ? t->ti_Data : s->max;
                break;

            case GRAD_MaxVal:
                s->max = t->ti_Data;
                if (s->cur > s->max)
                    s->cur = s->max;
                moved  = TRUE;
                result = 1;
                break;

            case GRAD_SkipVal:
                s->skip = t->ti_Data;
                break;

            case GRAD_PenArray:
                s->pens  = (UWORD *)t->ti_Data;
                s->npens = 0;
                while (s->pens && s->pens[s->npens] != (UWORD)~0)
                    s->npens++;
                restyled = TRUE;
                result   = 1;
                break;

            case GA_Disabled:
                if ((((struct Gadget *)o)->Flags & GFLG_DISABLED) != was_disabled)
                    restyled = TRUE;
                break;
        }
    }

    if (s->cur != before)
    {
        moved  = TRUE;
        result = 1;
        if (!creating)
            slider_notify(o, s, msg->ops_GInfo, FALSE, FALSE);
    }

    if (!creating && (moved || restyled))
        classlib_Render(o, msg->ops_GInfo, restyled ? GREDRAW_REDRAW : GREDRAW_UPDATE);

    return result;
}

static ULONG slider_new(Class *cl, Object *o, struct opSet *msg)
{
    Object        *obj = (Object *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    struct Slider *s;

    if (!obj)
        return 0;

    s = INST_DATA(cl, obj);
    s->max        = 0xFFFF;
    s->cur        = 0;
    s->skip       = 0x1111;
    s->undo       = 0;
    s->pens       = NULL;
    s->npens      = 0;
    s->knob       = (WORD)GetTagData(GRAD_KnobPixels, 5, msg->ops_AttrList);
    s->vertical   = GetTagData(PGA_Freedom, LORIENT_HORIZ, msg->ops_AttrList) == LORIENT_VERT;
    s->backdrop   = NULL;
    s->backdrop_w = s->backdrop_h = 0;
    s->frame      = NewObject(NULL, (ClassID)FRAMEICLASS, IA_FrameType, FRAME_BUTTON, TAG_DONE);
    if (!s->frame)
    {
        CL_CoerceMethodA(cl, obj, (Msg)&(ULONG){ OM_DISPOSE });
        return 0;
    }

    slider_set(cl, obj, msg, TRUE);
    return (ULONG)obj;
}

static ULONG slider_get(Class *cl, Object *o, struct opGet *msg)
{
    if (msg->opg_AttrID == GRAD_CurVal)
    {
        *msg->opg_Storage = ((struct Slider *)INST_DATA(cl, o))->cur;
        return 1;
    }
    return CL_DoSuperMethodA(cl, o, (Msg)msg);
}

static ULONG slider_dispose(Class *cl, Object *o, Msg msg)
{
    struct Slider *s = INST_DATA(cl, o);

    free_backdrop(s);
    if (s->frame)
        DisposeObject(s->frame);
    return CL_DoSuperMethodA(cl, o, msg);
}

/* --------------------------------------------------------------------------
 * input
 */

static ULONG slider_hittest(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct IBox gb, t;
    LONG        x = msg->gpht_Mouse.X, y = msg->gpht_Mouse.Y;

    if (((struct Gadget *)o)->Flags & GFLG_DISABLED)
        return 0;

    classlib_GadgetBox((struct Gadget *)o, msg->gpht_GInfo, &gb);
    track_box(&gb, &t);
    if (t.Width <= 2 || t.Height <= 2)
        return 0;
    if (x >= t.Left && y >= t.Top && x < t.Left + t.Width && y < t.Top + t.Height)
        return GMR_GADGETHIT;
    return 0;
}

static ULONG slider_goactive(Class *cl, Object *o, struct gpInput *msg)
{
    struct Slider *s = INST_DATA(cl, o);
    struct IBox    gb, t, k;
    LONG           m, knob_from, knob_to;

    if (!msg->gpi_IEvent)
        return GMR_NOREUSE;

    classlib_GadgetBox((struct Gadget *)o, msg->gpi_GInfo, &gb);
    track_box(&gb, &t);
    knob_box(s, &t, &k);

    m         = s->vertical ? msg->gpi_Mouse.Y : msg->gpi_Mouse.X;
    knob_from = s->vertical ? k.Top : k.Left;
    knob_to   = knob_from + s->knob;

    s->undo   = s->cur;
    s->last_x = msg->gpi_Mouse.X;
    s->last_y = msg->gpi_Mouse.Y;

    if (m >= knob_from && m < knob_to)
    {
        /* on the knob: drag it */
        s->grab_pos   = knob_from - (s->vertical ? t.Top : t.Left);
        s->grab_mouse = m;
        return GMR_MEACTIVE;
    }

    /* beside the knob: one step towards the click */
    if (m < knob_from)
        s->cur = s->cur > s->skip ? s->cur - s->skip : 0;
    else
    {
        ULONG next = s->cur + s->skip;

        s->cur = (next > s->max || next < s->cur) ? s->max : next;
    }

    slider_notify(o, s, msg->gpi_GInfo, FALSE, TRUE);
    classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
    *msg->gpi_Termination = s->cur;
    return GMR_NOREUSE | GMR_VERIFY;
}

static ULONG slider_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    struct Slider     *s = INST_DATA(cl, o);
    struct InputEvent *ie = msg->gpi_IEvent;
    struct IBox        gb, t;
    LONG               pos, travel;
    LONG               val;

    if (ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    switch (ie->ie_Code)
    {
        case SELECTUP:
            *msg->gpi_Termination = s->cur;
            slider_notify(o, s, msg->gpi_GInfo, FALSE, TRUE);
            return GMR_NOREUSE | GMR_VERIFY;

        case MENUDOWN:
            /* cancelled: back to the value before the drag */
            s->cur = s->undo;
            slider_notify(o, s, msg->gpi_GInfo, FALSE, TRUE);
            classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
            return GMR_NOREUSE;

        case IECODE_NOBUTTON:
            if (msg->gpi_Mouse.X == s->last_x && msg->gpi_Mouse.Y == s->last_y)
                break;
            s->last_x = msg->gpi_Mouse.X;
            s->last_y = msg->gpi_Mouse.Y;

            classlib_GadgetBox((struct Gadget *)o, msg->gpi_GInfo, &gb);
            track_box(&gb, &t);
            travel = track_length(s, &t) - s->knob;
            pos    = s->grab_pos + (s->vertical ? msg->gpi_Mouse.Y : msg->gpi_Mouse.X) - s->grab_mouse;

            val = travel > 0 ? (LONG)((long long)pos * (LONG)s->max / travel) : 0;
            if (val < 0)
                val = 0;
            else if ((ULONG)val > s->max)
                val = (LONG)s->max;

            if ((ULONG)val != s->cur)
            {
                s->cur = (ULONG)val;
                classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
                slider_notify(o, s, msg->gpi_GInfo, TRUE, TRUE);
            }
            break;
    }
    return GMR_MEACTIVE;
}

static ULONG slider_domain(Class *cl, Object *o, struct gpDomain *msg)
{
    struct Slider   *s = INST_DATA(cl, o);
    struct DrawInfo *dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, 0, msg->gpd_Attrs);
    LONG             across = 4 + (msg->gpd_RPort ? msg->gpd_RPort->TxHeight : 8);
    LONG             along;

    /* a vertical slider is as wide as a horizontal one is high, in pixels
       of equal size */
    if (s->vertical && dri && dri->dri_Resolution.X && dri->dri_Resolution.Y)
        across = 6 + (across - 4) * dri->dri_Resolution.Y / dri->dri_Resolution.X;

    switch (msg->gpd_Which)
    {
        case GDOMAIN_MINIMUM: along = 4 + 5 * s->knob;  break;
        case GDOMAIN_NOMINAL: along = 4 + 10 * s->knob; break;
        default:              along = 0x7FFF;           break;
    }

    msg->gpd_Domain.Left   = 0;
    msg->gpd_Domain.Top    = 0;
    msg->gpd_Domain.Width  = s->vertical ? across : along;
    msg->gpd_Domain.Height = s->vertical ? along : across;
    return 1;
}

ULONG classlib_Dispatcher(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                          register Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
        case OM_NEW:         return slider_new(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:     return slider_dispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:      return slider_set(cl, o, (struct opSet *)msg, FALSE);
        case OM_GET:         return slider_get(cl, o, (struct opGet *)msg);
        case GM_RENDER:      return slider_render(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:     return slider_hittest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:    return slider_goactive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT: return slider_handleinput(cl, o, (struct gpInput *)msg);
        case GM_DOMAIN:      return slider_domain(cl, o, (struct gpDomain *)msg);
    }
    return CL_DoSuperMethodA(cl, o, msg);
}
