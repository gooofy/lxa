/*
 * lxa - tapedeck.gadget, the AmigaOS 3.1 BOOPSI tape deck / animation
 * controller class (LIBS:gadgets/tapedeck.gadget).
 *
 * (C) 2026 by G. Bartsch. Licensed under the MIT License.
 *
 * Written from the NDK (gadgets/tapedeck.h, tapedeck_gc autodoc) and the
 * behaviour of the 3.1 class (tests/probes/tapedeck/class.c,
 * tests/probes/colorwheel/render.c); doc/gadget-classes.md lists the
 * observed rules.  In short:
 *
 *   - a new object is always 202 x 16 pixels with GFLG_RELSPECIAL set;
 *     GA_Width/GA_Height given to OM_SET do change the size later;
 *   - TDECK_Mode defaults to BUT_STOP and only takes BUT_REWIND..BUT_STOP,
 *     TDECK_Paused is kept as a boolean, TDECK_Frames defaults to 10;
 *     TDECK_Tape cannot be read; OM_SET always returns 0;
 *   - the animation controls are four parts side by side - rewind, play,
 *     fast forward and the frame position - at 0, 27, 75 and 102 of 202
 *     pixels; the button of the current mode is drawn selected.
 *
 * The tape recorder look (TDECK_Tape) is not drawn yet.
 */

#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <intuition/screens.h>
#include <devices/inputevent.h>
#include <gadgets/tapedeck.h>

#include "classlib.h"

#define DECK_WIDTH      202         /* the size of every new object */
#define DECK_HEIGHT     16

/* the parts of the animation controls, left to right */
enum { SEG_REWIND, SEG_PLAY, SEG_FORWARD, SEG_FRAME, SEG_COUNT, SEG_NONE = -1 };

/* where each part starts, in pixels of a DECK_WIDTH wide deck */
static const UBYTE seg_start[SEG_COUNT + 1] = { 0, 27, 75, 102, DECK_WIDTH };

struct Deck
{
    Object *frame;              /* frameiclass image for the buttons */
    ULONG   mode;               /* TDECK_Mode */
    ULONG   held_mode;          /* the mode while rewind/forward is held down */
    ULONG   frames;             /* TDECK_Frames */
    ULONG   current;            /* TDECK_CurrentFrame */
    BYTE    pressed;            /* SEG_* the select button went down on */
    UBYTE   paused;             /* TDECK_Paused */
    UBYTE   tape;               /* TDECK_Tape */
};

const char  classlib_LibName[]    = "tapedeck.gadget";
const char  classlib_LibID[]      = "tapedeck 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct Deck);

APTR classlib_FuncTab[] =
{
    (APTR)classlib_Open,
    (APTR)classlib_Close,
    (APTR)classlib_Expunge,
    (APTR)classlib_ExtFunc,
    (APTR)-1
};

/* --------------------------------------------------------------------------
 * geometry
 */

struct DeckLayout
{
    struct IBox box;
    LONG        edge[SEG_COUNT + 1];    /* absolute x where each part starts */
};

static void deck_layout(Object *o, struct GadgetInfo *gi, struct DeckLayout *l)
{
    int s;

    classlib_GadgetBox((struct Gadget *)o, gi, &l->box);
    for (s = 0; s <= SEG_COUNT; s++)
        l->edge[s] = l->box.Left + seg_start[s] * l->box.Width / DECK_WIDTH;
}

/* rightmost pixel of a part: the frame position part ends on its edge */
static LONG seg_right(struct DeckLayout *l, int s)
{
    return (s == SEG_FRAME) ? l->edge[SEG_COUNT] : l->edge[s + 1] - 1;
}

/* the part under a gadget relative position */
static int deck_part_at(struct DeckLayout *l, LONG mx, LONG my)
{
    LONG x = l->box.Left + mx;
    int  s;

    if (mx < 0 || my < 0 || my >= l->box.Height - 1)
        return SEG_NONE;
    for (s = 0; s < SEG_COUNT; s++)
        if (x >= l->edge[s] && x <= seg_right(l, s))
            return s;
    return SEG_NONE;
}

/* the frame position knob (6 pixels wide) moves between these x positions */
static void frame_track(struct DeckLayout *l, LONG *first, LONG *range)
{
    *first = l->edge[SEG_FRAME] + 3;
    *range = l->edge[SEG_COUNT] - 2 - 5 - *first;
    if (*range < 0)
        *range = 0;
}

/* --------------------------------------------------------------------------
 * rendering
 */

/* an arrow head 8 pixels wide and 7 high, pointing left or right */
static void draw_arrow(struct RastPort *rp, LONG x, LONG y, BOOL to_left)
{
    LONG row, len;

    for (row = 0; row < 7; row++)
    {
        len = 8 - 2 * (row < 3 ? 3 - row : row - 3);
        if (to_left)
            RectFill(rp, x + 8 - len, y + row, x + 7, y + row);
        else
            RectFill(rp, x, y + row, x + len - 1, y + row);
    }
}

static BOOL seg_selected(struct Deck *d, int s)
{
    static const UBYTE seg_mode[3] = { BUT_REWIND, BUT_PLAY, BUT_FORWARD };

    return d->pressed == s || d->mode == seg_mode[s];
}

static void draw_part(struct Deck *d, struct RastPort *rp, struct DrawInfo *dri,
                      struct DeckLayout *l, int s)
{
    UWORD *pens = dri->dri_Pens;
    LONG   left = l->edge[s], right = seg_right(l, s), width = right - left + 1;
    LONG   top = l->box.Top, height = l->box.Height - 1;
    LONG   arrow_y = top + (height - 7) / 2;

    if (height < 3 || right <= left)
        return;

    if (d->frame)
    {
        SetAttrs(d->frame, IA_Width, width, IA_Height, height, TAG_DONE);
        DrawImageState(rp, (struct Image *)d->frame, left, top,
                       (s != SEG_FRAME && seg_selected(d, s)) ? IDS_SELECTED : IDS_NORMAL, dri);
    }

    SetDrMd(rp, JAM1);
    SetAPen(rp, pens[TEXTPEN]);

    if (s == SEG_REWIND || s == SEG_FORWARD)
    {
        /* two arrow heads, 9 pixels apart, a fifth of the part from the left */
        LONG x = left + width * 5 / 27;

        draw_arrow(rp, x, arrow_y, s == SEG_REWIND);
        draw_arrow(rp, x + 9, arrow_y, s == SEG_REWIND);
    }
    else if (s == SEG_PLAY)
    {
        draw_arrow(rp, left + (width - 8) / 2, arrow_y, FALSE);

        /* 3.1's play button: its bottom edge is shifted one pixel to the
           right and drawn in the shine pen (the next part covers the end) */
        SetAPen(rp, pens[SHADOWPEN]);
        WritePixel(rp, left, top + height - 1);
        SetAPen(rp, pens[SHINEPEN]);
        Move(rp, left + 1, top + height - 1);
        Draw(rp, right + 1, top + height - 1);
    }
    else
    {
        LONG first, range, x;
        ULONG pos = d->current < d->frames ? d->current : d->frames;

        frame_track(l, &first, &range);
        x = first + (d->frames ? (LONG)(pos * (ULONG)range / d->frames) : 0);
        if (height > 4)
            RectFill(rp, x, top + 2, x + 5, top + height - 3);
    }
}

static ULONG deck_render(Class *cl, Object *o, struct gpRender *msg)
{
    static UWORD ghost[] = { 0x8888, 0x2222 };
    struct Deck       *d = INST_DATA(cl, o);
    struct RastPort   *rp = msg->gpr_RPort;
    struct DrawInfo   *dri;
    struct DeckLayout  l;
    int                s;

    if (!rp || !msg->gpr_GInfo || !(dri = msg->gpr_GInfo->gi_DrInfo))
        return 0;

    if (d->tape)
        LXA_UNIMPLEMENTED("tapedeck.gadget", "GM_RENDER", "partial: TDECK_Tape drawn as animation controls");

    deck_layout(o, msg->gpr_GInfo, &l);
    for (s = 0; s < SEG_COUNT; s++)
        draw_part(d, rp, dri, &l, s);

    if (((struct Gadget *)o)->Flags & GFLG_DISABLED)
    {
        SetDrMd(rp, JAM1);
        SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
        SetAfPt(rp, ghost, 1);
        RectFill(rp, l.box.Left, l.box.Top, l.edge[SEG_COUNT], l.box.Top + l.box.Height - 2);
        SetAfPt(rp, NULL, 0);
    }
    return 1;
}

/* --------------------------------------------------------------------------
 * attributes
 */

static void deck_notify(Object *o, struct Deck *d, struct GadgetInfo *gi, BOOL interim)
{
    struct TagItem attrs[5];

    attrs[0].ti_Tag = GA_ID;              attrs[0].ti_Data = ((struct Gadget *)o)->GadgetID;
    attrs[1].ti_Tag = TDECK_Mode;         attrs[1].ti_Data = d->mode;
    attrs[2].ti_Tag = TDECK_Paused;       attrs[2].ti_Data = d->paused;
    attrs[3].ti_Tag = TDECK_CurrentFrame; attrs[3].ti_Data = d->current;
    attrs[4].ti_Tag = TAG_DONE;           attrs[4].ti_Data = 0;
    classlib_Notify(o, gi, attrs, interim);
}

/* apply the attributes; TRUE if the look changed */
static BOOL deck_apply(struct Deck *d, struct TagItem *attrs, BOOL creating)
{
    struct TagItem *state = attrs, *t;
    BOOL            changed = FALSE;

    while ((t = NextTagItem(&state)))
    {
        switch (t->ti_Tag)
        {
            case TDECK_Mode:
                /* 3.1 ignores BUT_PAUSE and above */
                if (t->ti_Data <= BUT_STOP)
                {
                    d->mode = t->ti_Data;
                    changed = TRUE;
                }
                break;
            case TDECK_Paused:
                d->paused = t->ti_Data ? 1 : 0;
                changed = TRUE;
                break;
            case TDECK_Tape:
                if (creating)
                    d->tape = t->ti_Data ? 1 : 0;
                break;
            case TDECK_Frames:
                d->frames = t->ti_Data;
                changed = TRUE;
                break;
            case TDECK_CurrentFrame:
                d->current = t->ti_Data;
                changed = TRUE;
                break;
            case GA_Disabled:
            case GA_Left:
            case GA_Top:
            case GA_Width:
            case GA_Height:
                changed = TRUE;
                break;
        }
    }
    return changed;
}

static ULONG deck_new(Class *cl, Object *o, struct opSet *msg)
{
    struct Gadget *g = (struct Gadget *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    struct Deck   *d;

    if (!g)
        return 0;

    d = INST_DATA(cl, (Object *)g);
    d->frame     = NewObject(NULL, (ClassID)FRAMEICLASS, IA_FrameType, FRAME_BUTTON,
                             IA_EdgesOnly, FALSE, TAG_DONE);
    d->mode      = BUT_STOP;
    d->held_mode = BUT_STOP;
    d->frames    = 10;
    d->current   = 0;
    d->pressed   = SEG_NONE;
    d->paused    = 0;
    d->tape      = 0;

    /* 3.1 makes every deck 202 x 16, whatever size was asked for */
    g->Width  = DECK_WIDTH;
    g->Height = DECK_HEIGHT;
    g->Flags |= GFLG_RELSPECIAL;

    deck_apply(d, msg->ops_AttrList, TRUE);
    return (ULONG)g;
}

static ULONG deck_set(Class *cl, Object *o, struct opSet *msg)
{
    struct Deck *d = INST_DATA(cl, o);

    CL_DoSuperMethodA(cl, o, (Msg)msg);
    if (deck_apply(d, msg->ops_AttrList, FALSE))
        classlib_Render(o, msg->ops_GInfo, GREDRAW_REDRAW);
    return 0;
}

static ULONG deck_get(Class *cl, Object *o, struct opGet *msg)
{
    struct Deck *d = INST_DATA(cl, o);
    ULONG       *store = msg->opg_Storage;

    switch (msg->opg_AttrID)
    {
        case TDECK_Mode:         *store = d->mode;    return 1;
        case TDECK_Paused:       *store = d->paused;  return 1;
        case TDECK_Frames:       *store = d->frames;  return 1;
        case TDECK_CurrentFrame: *store = d->current; return 1;
    }
    return CL_DoSuperMethodA(cl, o, (Msg)msg);
}

/* --------------------------------------------------------------------------
 * input
 */

/* move the frame position to a gadget relative x position */
static void deck_seek(struct Deck *d, struct DeckLayout *l, LONG mx)
{
    LONG first, range, x;

    frame_track(l, &first, &range);
    x = l->box.Left + mx - first - 3;           /* the knob's centre follows the mouse */
    if (x < 0)
        x = 0;
    if (x > range)
        x = range;
    d->current = range ? (ULONG)((x * d->frames + range / 2) / range) : 0;
}

static void deck_release(Object *o, struct Deck *d)
{
    if (d->pressed == SEG_REWIND || d->pressed == SEG_FORWARD)
        d->mode = d->held_mode;                 /* winding only lasts while held */
    d->pressed = SEG_NONE;
    ((struct Gadget *)o)->Flags &= ~GFLG_SELECTED;
}

static ULONG deck_goactive(Class *cl, Object *o, struct gpInput *msg)
{
    struct Deck       *d = INST_DATA(cl, o);
    struct DeckLayout  l;
    int                s;

    if (!msg->gpi_IEvent || (((struct Gadget *)o)->Flags & GFLG_DISABLED))
        return GMR_NOREUSE;

    deck_layout(o, msg->gpi_GInfo, &l);
    s = deck_part_at(&l, msg->gpi_Mouse.X, msg->gpi_Mouse.Y);
    if (s == SEG_NONE)
        return GMR_NOREUSE;

    d->pressed = s;
    ((struct Gadget *)o)->Flags |= GFLG_SELECTED;

    if (s == SEG_REWIND || s == SEG_FORWARD)
    {
        d->held_mode = d->mode;
        d->mode = (s == SEG_REWIND) ? BUT_REWIND : BUT_FORWARD;
        deck_notify(o, d, msg->gpi_GInfo, TRUE);
    }
    else if (s == SEG_FRAME)
    {
        deck_seek(d, &l, msg->gpi_Mouse.X);
        deck_notify(o, d, msg->gpi_GInfo, TRUE);
    }

    classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
    return GMR_MEACTIVE;
}

static ULONG deck_handleinput(Class *cl, Object *o, struct gpInput *msg)
{
    struct Deck       *d = INST_DATA(cl, o);
    struct InputEvent *ie = msg->gpi_IEvent;
    struct DeckLayout  l;

    if (!ie || ie->ie_Class != IECLASS_RAWMOUSE)
        return GMR_MEACTIVE;

    deck_layout(o, msg->gpi_GInfo, &l);

    if (ie->ie_Code == IECODE_NOBUTTON)
    {
        if (d->pressed == SEG_FRAME)
        {
            ULONG before = d->current;

            deck_seek(d, &l, msg->gpi_Mouse.X);
            if (d->current != before)
            {
                deck_notify(o, d, msg->gpi_GInfo, TRUE);
                classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
            }
        }
        return GMR_MEACTIVE;
    }

    if (ie->ie_Code == MENUDOWN)
        return GMR_NOREUSE;

    if (ie->ie_Code != SELECTUP)
        return GMR_MEACTIVE;

    /* play toggles between playing and stopped when released over it */
    if (d->pressed == SEG_PLAY &&
        deck_part_at(&l, msg->gpi_Mouse.X, msg->gpi_Mouse.Y) == SEG_PLAY)
    {
        d->mode   = (d->mode == BUT_PLAY) ? BUT_STOP : BUT_PLAY;
        d->paused = 0;
    }
    deck_release(o, d);
    deck_notify(o, d, msg->gpi_GInfo, FALSE);
    classlib_Render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
    *msg->gpi_Termination = d->mode;
    return GMR_NOREUSE | GMR_VERIFY;
}

static ULONG deck_goinactive(Class *cl, Object *o, struct gpGoInactive *msg)
{
    deck_release(o, INST_DATA(cl, o));
    classlib_Render(o, msg->gpgi_GInfo, GREDRAW_UPDATE);
    return 0;
}

static ULONG deck_hittest(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct DeckLayout l;

    deck_layout(o, msg->gpht_GInfo, &l);
    return deck_part_at(&l, msg->gpht_Mouse.X, msg->gpht_Mouse.Y) != SEG_NONE ? GMR_GADGETHIT : 0;
}

static ULONG deck_dispose(Class *cl, Object *o, Msg msg)
{
    struct Deck *d = INST_DATA(cl, o);

    if (d->frame)
        DisposeObject(d->frame);
    return CL_DoSuperMethodA(cl, o, msg);
}

ULONG classlib_Dispatcher(register Class *cl __asm("a0"), register Object *o __asm("a2"),
                          register Msg msg __asm("a1"))
{
    switch (msg->MethodID)
    {
        case OM_NEW:         return deck_new(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:     return deck_dispose(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:      return deck_set(cl, o, (struct opSet *)msg);
        case OM_GET:         return deck_get(cl, o, (struct opGet *)msg);
        case GM_RENDER:      return deck_render(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:     return deck_hittest(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:    return deck_goactive(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT: return deck_handleinput(cl, o, (struct gpInput *)msg);
        case GM_GOINACTIVE:  return deck_goinactive(cl, o, (struct gpGoInactive *)msg);
    }
    return CL_DoSuperMethodA(cl, o, msg);
}
