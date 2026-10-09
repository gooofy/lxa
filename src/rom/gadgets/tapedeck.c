/*
    Copyright (C) 2016, The AROS Development Team. All rights reserved.

    AROS tapedeck gadget.

    Ported to lxa from AROS workbench/classes/gadgets/tapedeck
    (tapedeckclass.c).
    lxa changes (see doc/third-party-code.md), observed on AmigaOS 3.1
    (tests/probes/tapedeck/class.c, tests/probes/colorwheel/render.c):
      - library frame in classlib.c, plain C BOOPSI helpers;
      - a new object is 202 x 16 pixels whatever GA_Width/GA_Height say
        (they can be changed later with OM_SET), GFLG_RELSPECIAL is set;
      - defaults TDECK_Mode BUT_STOP, TDECK_Frames 10, TDECK_CurrentFrame 0;
      - TDECK_Paused is kept (as a BOOL) and readable, TDECK_Mode accepts
        BUT_REWIND .. BUT_STOP only, OM_SET returns 0;
      - the animation controls look like 3.1's: three framed buttons
        (rewind, play, fast forward; the button of the current mode drawn
        pressed) and a framed frame-position indicator with a knob, instead
        of AROS' prop gadget strip above centred symbols;
      - the tape recorder look (TDECK_Tape) is not drawn yet: the
        animation controls are used for it (LXA_UNIMPLEMENTED).
*/

#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <gadgets/tapedeck.h>

#include "classlib.h"

/***********************************************************************************/

#define IM(o) ((struct Image *)(o))
#define EG(o) ((struct Gadget *)(o))

#define TDECK_WIDTH      202
#define TDECK_HEIGHT     16

#define PART_REWIND      0
#define PART_PLAY        1
#define PART_FORWARD     2
#define PART_POSITION    3
#define PART_NONE        (-1)

struct TapeDeckData
{
    Object                      *tdd_Frame;
    ULONG                       tdd_Mode;
    ULONG                       tdd_ModeSaved;          /* while rewind/forward is held */
    ULONG                       tdd_FrameCount;
    ULONG                       tdd_FrameCurrent;
    BYTE                        tdd_Pressed;            /* PART_* under the held button */
    UBYTE                       tdd_Paused;
    UBYTE                       tdd_Tape;
};

const char  classlib_LibName[]    = "tapedeck.gadget";
const char  classlib_LibID[]      = "tapedeck 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct TapeDeckData);

APTR classlib_FuncTab[] =
{
    (APTR)classlib_Open,
    (APTR)classlib_Close,
    (APTR)classlib_Expunge,
    (APTR)classlib_ExtFunc,
    (APTR)-1
};

/***********************************************************************************/
/* geometry: 3.1 places the parts at 0, 27, 75 and 102 of 202 pixels                */
/***********************************************************************************/

struct DeckBox
{
    LONG left, top, width, height;
    LONG x[5];      /* part p spans x[p] .. x[p+1] - 1 (the last one x[4] inclusive) */
};

static VOID GetDeckBox(Object *o, struct GadgetInfo *gi, struct DeckBox *b)
{
    static const UBYTE split[5] = { 0, 27, 75, 102, 202 };
    LONG i;

    b->left   = EG(o)->LeftEdge;
    b->top    = EG(o)->TopEdge;
    b->width  = EG(o)->Width;
    b->height = EG(o)->Height;

    if (gi)
    {
        if (EG(o)->Flags & GFLG_RELRIGHT)  b->left   += gi->gi_Domain.Width - 1;
        if (EG(o)->Flags & GFLG_RELBOTTOM) b->top    += gi->gi_Domain.Height - 1;
        if (EG(o)->Flags & GFLG_RELWIDTH)  b->width  += gi->gi_Domain.Width;
        if (EG(o)->Flags & GFLG_RELHEIGHT) b->height += gi->gi_Domain.Height;
    }

    for (i = 0; i < 5; i++)
        b->x[i] = b->left + split[i] * b->width / TDECK_WIDTH;
}

/* PART_* at a gadget relative mouse position */
static LONG HitPart(struct DeckBox *b, LONG mx, LONG my)
{
    LONG x = b->left + mx, p;

    if (my < 0 || my >= b->height - 1 || mx < 0)
        return PART_NONE;
    for (p = 0; p < 4; p++)
        if (x >= b->x[p] && x < b->x[p + 1] + (p == 3))
            return p;
    return PART_NONE;
}

static VOID KnobRange(struct DeckBox *b, LONG *inner, LONG *travel)
{
    *inner  = b->x[3] + 3;
    *travel = (b->x[4] - 2) - *inner - 5;
    if (*travel < 0)
        *travel = 0;
}

/* a 7 pixel high triangle (widths 2,4,6,8,6,4,2) */
static VOID Triangle(struct RastPort *rp, LONG x, LONG y, BOOL left)
{
    LONG r, w;

    for (r = 0; r < 7; r++)
    {
        w = 2 * (4 - (r < 3 ? 3 - r : r - 3));
        if (left)
            RectFill(rp, x + 8 - w, y + r, x + 7, y + r);
        else
            RectFill(rp, x, y + r, x + w - 1, y + r);
    }
}

static VOID DrawPart(struct TapeDeckData *data, struct RastPort *rp, struct DrawInfo *dri,
                     struct DeckBox *b, LONG part)
{
    LONG x0 = b->x[part], x1 = (part == 3) ? b->x[4] : b->x[part + 1] - 1;
    LONG h = b->height - 1, ty = b->top + (h - 7) / 2;
    BOOL selected = FALSE;

    if (h < 3 || x1 <= x0)
        return;

    switch (part)
    {
        case PART_REWIND:  selected = data->tdd_Pressed == part || data->tdd_Mode == BUT_REWIND;  break;
        case PART_PLAY:    selected = data->tdd_Pressed == part || data->tdd_Mode == BUT_PLAY;    break;
        case PART_FORWARD: selected = data->tdd_Pressed == part || data->tdd_Mode == BUT_FORWARD; break;
    }

    if (data->tdd_Frame)
    {
        struct TagItem ftags[] =
        {
            { IA_Width,  x1 - x0 + 1 },
            { IA_Height, h           },
            { TAG_DONE,  0           }
        };
        SetAttrsA(data->tdd_Frame, ftags);
        DrawImageState(rp, IM(data->tdd_Frame), x0, b->top, selected ? IDS_SELECTED : IDS_NORMAL, dri);
    }

    SetDrMd(rp, JAM1);
    SetAPen(rp, dri->dri_Pens[TEXTPEN]);

    switch (part)
    {
        case PART_REWIND:
        {
            LONG s = x0 + (x1 - x0 + 1) * 5 / 27;
            Triangle(rp, s, ty, TRUE);
            Triangle(rp, s + 9, ty, TRUE);
            break;
        }
        case PART_PLAY:
            Triangle(rp, x0 + (x1 - x0 + 1 - 8) / 2, ty, FALSE);
            /* 3.1 draws the bottom edge of the play button one pixel to the right, in white */
            SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
            RectFill(rp, x0, b->top + h - 1, x0, b->top + h - 1);
            SetAPen(rp, dri->dri_Pens[SHINEPEN]);
            RectFill(rp, x0 + 1, b->top + h - 1, x1 + 1, b->top + h - 1);
            break;
        case PART_FORWARD:
        {
            LONG s = x0 + (x1 - x0 + 1) * 5 / 27;
            Triangle(rp, s, ty, FALSE);
            Triangle(rp, s + 9, ty, FALSE);
            break;
        }
        case PART_POSITION:
        {
            LONG inner, travel, kx;
            ULONG cur = data->tdd_FrameCurrent;

            KnobRange(b, &inner, &travel);
            if (cur > data->tdd_FrameCount)
                cur = data->tdd_FrameCount;
            kx = inner + (data->tdd_FrameCount ? (LONG)(cur * (ULONG)travel / data->tdd_FrameCount) : 0);
            if (h > 4)
                RectFill(rp, kx, b->top + 2, kx + 5, b->top + h - 3);
            break;
        }
    }
}

static VOID DrawDisabledPattern(struct RastPort *rp, struct DeckBox *b, UWORD pen)
{
    UWORD pattern[] = { 0x8888, 0x2222 };

    SetDrMd(rp, JAM1);
    SetAPen(rp, pen);
    SetAfPt(rp, pattern, 1);
    RectFill(rp, b->left, b->top, b->x[4], b->top + b->height - 2);
    SetAfPt(rp, NULL, 0);
}

/***********************************************************************************/

static VOID notify(Class *cl, Object *o, struct GadgetInfo *gi, BOOL interim)
{
    struct TapeDeckData *data = INST_DATA(cl, o);
    struct opUpdate      opu;
    struct TagItem       tags[] =
    {
        { GA_ID,              EG(o)->GadgetID         },
        { TDECK_Mode,         data->tdd_Mode          },
        { TDECK_Paused,       data->tdd_Paused        },
        { TDECK_CurrentFrame, data->tdd_FrameCurrent  },
        { TAG_DONE,           0                       }
    };

    opu.MethodID     = OM_NOTIFY;
    opu.opu_AttrList = tags;
    opu.opu_GInfo    = gi;
    opu.opu_Flags    = interim ? OPUF_INTERIM : 0;
    CL_DoMethodA(o, (Msg)&opu);
}

static VOID render(Object *o, struct GadgetInfo *gi, LONG mode)
{
    struct RastPort *rport;

    if (gi && (rport = ObtainGIRPort(gi)))
    {
        struct gpRender gpr;

        gpr.MethodID   = GM_RENDER;
        gpr.gpr_GInfo  = gi;
        gpr.gpr_RPort  = rport;
        gpr.gpr_Redraw = mode;
        CL_DoMethodA(o, (Msg)&gpr);
        ReleaseGIRPort(rport);
    }
}

static ULONG TapeDeck_GET(Class *cl, Object *o, struct opGet *msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);

    switch (msg->opg_AttrID)
    {
        case TDECK_Mode:
            *msg->opg_Storage = data->tdd_Mode;
            return 1;

        case TDECK_Paused:
            *msg->opg_Storage = data->tdd_Paused;
            return 1;

        case TDECK_Frames:
            *msg->opg_Storage = data->tdd_FrameCount;
            return 1;

        case TDECK_CurrentFrame:
            *msg->opg_Storage = data->tdd_FrameCurrent;
            return 1;

        default:
            return CL_DoSuperMethodA(cl, o, (Msg)msg);
    }
}

static ULONG TapeDeck_SET(Class *cl, Object *o, struct opSet *msg, BOOL init)
{
    struct TapeDeckData *data = INST_DATA(cl, o);
    struct TagItem      *tag, *taglist = msg->ops_AttrList;
    BOOL                rerender = FALSE;

    if (!init)
        CL_DoSuperMethodA(cl, o, (Msg)msg);

    while ((tag = NextTagItem(&taglist)))
    {
        switch (tag->ti_Tag)
        {
            case TDECK_Mode:
                if (tag->ti_Data <= BUT_STOP)
                {
                    data->tdd_Mode = tag->ti_Data;
                    rerender = TRUE;
                }
                break;

            case TDECK_Paused:
                data->tdd_Paused = tag->ti_Data ? 1 : 0;
                rerender = TRUE;
                break;

            case TDECK_Tape:
                if (init)
                    data->tdd_Tape = tag->ti_Data ? 1 : 0;
                break;

            case TDECK_Frames:
                data->tdd_FrameCount = tag->ti_Data;
                rerender = TRUE;
                break;

            case TDECK_CurrentFrame:
                data->tdd_FrameCurrent = tag->ti_Data;
                rerender = TRUE;
                break;

            case GA_Disabled:
            case GA_Left:
            case GA_Top:
            case GA_Width:
            case GA_Height:
                rerender = TRUE;
                break;
        }
    }

    if (!init && rerender)
        render(o, msg->ops_GInfo, GREDRAW_REDRAW);

    return 0;
}

static ULONG TapeDeck_NEW(Class *cl, Object *o, struct opSet *msg)
{
    struct TagItem      frametags[] =
    {
        { IA_EdgesOnly,         FALSE           },
        { IA_FrameType,         FRAME_BUTTON    },
        { TAG_DONE,             0               }
    };

    o = (Object *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
        struct TapeDeckData *data = INST_DATA(cl, o);
        UBYTE *p = (UBYTE *)data;
        ULONG i;

        for (i = 0; i < sizeof(*data); i++)
            p[i] = 0;

        EG(o)->Width  = TDECK_WIDTH;
        EG(o)->Height = TDECK_HEIGHT;
        EG(o)->Flags |= GFLG_RELSPECIAL;

        data->tdd_Frame        = NewObjectA(NULL, (ClassID)FRAMEICLASS, frametags);
        data->tdd_Mode         = BUT_STOP;
        data->tdd_ModeSaved    = BUT_STOP;
        data->tdd_FrameCount   = 10;
        data->tdd_FrameCurrent = 0;
        data->tdd_Pressed      = PART_NONE;

        TapeDeck_SET(cl, o, msg, TRUE);
    }

    return (ULONG)o;
}

static ULONG TapeDeck_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);

    if (data->tdd_Frame)
        DisposeObject(data->tdd_Frame);

    return CL_DoSuperMethodA(cl, o, msg);
}

static ULONG TapeDeck_RENDER(Class *cl, Object *o, struct gpRender *msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);
    struct DrawInfo     *dri;
    struct RastPort     *rp = msg->gpr_RPort;
    struct DeckBox       b;
    LONG                 part;

    if (!rp || !msg->gpr_GInfo || !(dri = msg->gpr_GInfo->gi_DrInfo))
        return 0;

    if (data->tdd_Tape)
        LXA_UNIMPLEMENTED("tapedeck.gadget", "GM_RENDER", "partial: TDECK_Tape drawn as animation controls");

    GetDeckBox(o, msg->gpr_GInfo, &b);

    for (part = 0; part < 4; part++)
        DrawPart(data, rp, dri, &b, part);

    if (EG(o)->Flags & GFLG_DISABLED)
        DrawDisabledPattern(rp, &b, dri->dri_Pens[SHADOWPEN]);

    return 1;
}

static VOID SetFromMouse(struct TapeDeckData *data, struct DeckBox *b, LONG mx)
{
    LONG inner, travel, pos;

    KnobRange(b, &inner, &travel);
    pos = b->left + mx - inner - 3;
    if (pos < 0)
        pos = 0;
    if (pos > travel)
        pos = travel;
    data->tdd_FrameCurrent = travel ? (ULONG)((pos * data->tdd_FrameCount + travel / 2) / travel) : 0;
}

static ULONG TapeDeck_GOACTIVE(Class *cl, Object *o, struct gpInput *msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);
    struct DeckBox       b;
    LONG                 part;

    if (!msg->gpi_IEvent || (EG(o)->Flags & GFLG_DISABLED))
        return GMR_NOREUSE;

    GetDeckBox(o, msg->gpi_GInfo, &b);
    part = HitPart(&b, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
    if (part == PART_NONE)
        return GMR_NOREUSE;

    data->tdd_Pressed = part;
    EG(o)->Flags |= GFLG_SELECTED;

    switch (part)
    {
        case PART_REWIND:
        case PART_FORWARD:
            /* winding lasts while the button is held */
            data->tdd_ModeSaved = data->tdd_Mode;
            data->tdd_Mode = (part == PART_REWIND) ? BUT_REWIND : BUT_FORWARD;
            notify(cl, o, msg->gpi_GInfo, TRUE);
            break;

        case PART_POSITION:
            SetFromMouse(data, &b, msg->gpi_Mouse.X);
            notify(cl, o, msg->gpi_GInfo, TRUE);
            break;
    }

    render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
    return GMR_MEACTIVE;
}

static ULONG TapeDeck_HANDLEINPUT(Class *cl, Object *o, struct gpInput *msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);
    struct InputEvent   *ie = msg->gpi_IEvent;
    struct DeckBox       b;
    LONG                 part;

    if (!ie || ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    GetDeckBox(o, msg->gpi_GInfo, &b);

    switch (ie->ie_Code)
    {
        case IECODE_NOBUTTON:
            if (data->tdd_Pressed == PART_POSITION)
            {
                ULONG old = data->tdd_FrameCurrent;
                SetFromMouse(data, &b, msg->gpi_Mouse.X);
                if (old != data->tdd_FrameCurrent)
                {
                    notify(cl, o, msg->gpi_GInfo, TRUE);
                    render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
                }
            }
            return GMR_MEACTIVE;

        case SELECTUP:
            part = HitPart(&b, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
            switch (data->tdd_Pressed)
            {
                case PART_REWIND:
                case PART_FORWARD:
                    data->tdd_Mode = data->tdd_ModeSaved;
                    break;

                case PART_PLAY:
                    if (part == PART_PLAY)
                    {
                        data->tdd_Mode = (data->tdd_Mode == BUT_PLAY) ? BUT_STOP : BUT_PLAY;
                        data->tdd_Paused = 0;
                    }
                    break;
            }
            data->tdd_Pressed = PART_NONE;
            EG(o)->Flags &= ~GFLG_SELECTED;
            notify(cl, o, msg->gpi_GInfo, FALSE);
            render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
            *msg->gpi_Termination = data->tdd_Mode;
            return GMR_NOREUSE | GMR_VERIFY;

        case MENUDOWN:
            return GMR_NOREUSE;
    }

    return GMR_MEACTIVE;
}

static ULONG TapeDeck_GOINACTIVE(Class *cl, Object *o, struct gpGoInactive *msg)
{
    struct TapeDeckData *data = INST_DATA(cl, o);

    if (data->tdd_Pressed == PART_REWIND || data->tdd_Pressed == PART_FORWARD)
        data->tdd_Mode = data->tdd_ModeSaved;
    data->tdd_Pressed = PART_NONE;
    EG(o)->Flags &= ~GFLG_SELECTED;
    render(o, msg->gpgi_GInfo, GREDRAW_UPDATE);
    return 0;
}

static ULONG TapeDeck_HITTEST(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct DeckBox b;

    GetDeckBox(o, msg->gpht_GInfo, &b);
    return HitPart(&b, msg->gpht_Mouse.X, msg->gpht_Mouse.Y) != PART_NONE ? GMR_GADGETHIT : 0;
}

ULONG classlib_Dispatcher(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                          register Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
        case OM_NEW:          return TapeDeck_NEW(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:      return TapeDeck_DISPOSE(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:       return TapeDeck_SET(cl, o, (struct opSet *)msg, FALSE);
        case OM_GET:          return TapeDeck_GET(cl, o, (struct opGet *)msg);
        case GM_RENDER:       return TapeDeck_RENDER(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:      return TapeDeck_HITTEST(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:     return TapeDeck_GOACTIVE(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT:  return TapeDeck_HANDLEINPUT(cl, o, (struct gpInput *)msg);
        case GM_GOINACTIVE:   return TapeDeck_GOINACTIVE(cl, o, (struct gpGoInactive *)msg);
        default:              return CL_DoSuperMethodA(cl, o, msg);
    }
}
