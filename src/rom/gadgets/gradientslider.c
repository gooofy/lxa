/*
    Copyright (C) 1995-2011, The AROS Development Team. All rights reserved.

    AROS gradientslider gadget.

    Ported to lxa from AROS workbench/classes/gadgets/gradientslider
    (gradientsliderclass.c, support.c).
    lxa changes (see doc/third-party-code.md), all observed on AmigaOS 3.1
    (tests/probes/gradientslider/class.c):
      - library frame in classlib.c, plain C BOOPSI helpers;
      - OM_GET knows GRAD_CurVal only;
      - GRAD_MaxVal accepts any value and clamps GRAD_CurVal to it (no
        rescaling), GRAD_CurVal is clamped to GRAD_MaxVal;
      - OM_SET returns 1 when GRAD_CurVal changed or GRAD_MaxVal or
        GRAD_PenArray were set, 0 otherwise;
      - 3.1's look: its 4x4 ordered dither, the knob as a black bar
        with a background border (rounded position), no ghost pattern
        for GA_Disabled;
      - only the dithered (pen based) gradient: no cybergraphics path.
*/

#include <graphics/gfxmacros.h>
#include <graphics/gfxbase.h>
#include <intuition/screens.h>
#include <intuition/icclass.h>
#include <gadgets/gradientslider.h>

#include "classlib.h"

/***************************************************************************************************/

#define EG(o)                   ((struct ExtGadget *)o)

#define FRAMESLIDERSPACINGX     3
#define FRAMESLIDERSPACINGY     3

struct GradientSliderData
{
    struct BitMap               *savebm;
    struct BitMap               *knobbm;
    Object                      *frame;
    ULONG                       maxval;         /* ISU          */
    ULONG                       curval;         /* ISGNU        */
    ULONG                       saveval;
    ULONG                       skipval;        /* ISU          */
    UWORD                       knobpixels;     /* I            */
    UWORD                       *penarray;      /* ISU          */
    WORD                        freedom;        /* I            */
    WORD                        numpens;
    WORD                        clickoffsetx;
    WORD                        clickoffsety;
    WORD                        savefromx;
    WORD                        savefromy;
    WORD                        savebmwidth;
    WORD                        savebmheight;
    WORD                        x, y;
    struct RastPort             trp;
};

const char  classlib_LibName[]    = "gradientslider.gadget";
const char  classlib_LibID[]      = "gradientslider 40.1 (12.10.2026)\r\n";
const UWORD classlib_Version      = 40;
const UWORD classlib_Revision     = 1;
const char  classlib_SuperClass[] = "gadgetclass";
const UWORD classlib_InstSize     = sizeof(struct GradientSliderData);

APTR classlib_FuncTab[] =
{
    (APTR)classlib_Open,
    (APTR)classlib_Close,
    (APTR)classlib_Expunge,
    (APTR)classlib_ExtFunc,
    (APTR)-1
};

/***************************************************************************************************/
/* support.c                                                                                       */
/***************************************************************************************************/

/*
 * 3.1's ordered dither: a 4x4 threshold matrix, the level of a pixel row
 * (column) is (pos + 1) * 17 / length, pen2 where level >= threshold.
 * Observed on the reference (tests/probes/colorwheel/render.c captures).
 */
static const UBYTE Threshold[4][4] =
{
    {16,  4, 13,  1},
    { 8, 12,  5,  9},
    {14,  2, 15,  3},
    { 6, 10,  7, 11}
};

static VOID DitherV(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, LONG pen1, LONG pen2)
{
    LONG width = x2 - x1 + 1;
    LONG height = y2 - y1 + 1;
    LONG x, y, v, pixel, lastpixel = -1;

    for (y = 0; y < height; y++)
    {
        v = (y + 1) * 17 / height;

        for (x = 0; x < width; x++)
        {
            pixel = (v >= Threshold[y & 3][x & 3]) ? pen2 : pen1;

            if (pixel != lastpixel)
            {
                SetAPen(rp, pixel);
                lastpixel = pixel;
            }

            WritePixel(rp, x1 + x, y1 + y);
        }
    }
}

static VOID DitherH(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, LONG pen1, LONG pen2)
{
    LONG width = x2 - x1 + 1;
    LONG height = y2 - y1 + 1;
    LONG x, y, v, pixel, lastpixel = -1;

    for (x = 0; x < width; x++)
    {
        v = (x + 1) * 17 / width;

        for (y = 0; y < height; y++)
        {
            pixel = (v >= Threshold[y & 3][x & 3]) ? pen2 : pen1;

            if (pixel != lastpixel)
            {
                SetAPen(rp, pixel);
                lastpixel = pixel;
            }

            WritePixel(rp, x1 + x, y1 + y);
        }
    }
}

static VOID DrawGradient(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, UWORD *penarray,
                         LONG numpens, LONG orientation)
{
    UWORD *pen = penarray;
    ULONG step, pos = 0;
    LONG  x, y, width, height, oldx, oldy, endx, endy;
    LONG  pen1, pen2, i;

    pen1 = *pen++;

    switch (orientation)
    {
        case LORIENT_VERT:
            height = y2 - y1 + 1;
            step = height * 65536 / (numpens - 1);
            oldy = y1;
            for (i = 0; i < numpens - 1; i++)
            {
                pen2 = *pen++;
                pos += step;
                y = y1 + (pos / 65536);
                endy = y; if (endy != y2) endy--;
                if (endy >= oldy)
                {
                    DitherV(rp, x1, oldy, x2, endy, pen1, pen2);
                    pen1 = pen2;
                    oldy = y;
                }
            }
            break;

        case LORIENT_HORIZ:
            width = x2 - x1 + 1;
            step = width * 65536 / (numpens - 1);
            oldx = x1;
            for (i = 0; i < numpens - 1; i++)
            {
                pen2 = *pen++;
                pos += step;
                x = x1 + (pos / 65536);
                endx = x; if (endx != x2) endx--;
                if (endx >= oldx)
                {
                    DitherH(rp, oldx, y1, endx, y2, pen1, pen2);
                    pen1 = pen2;
                    oldx = x;
                }
            }
            break;
    }
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

/* the area the knob moves in: inside the frame (2 pixels left/right, 1 top/bottom) */
static VOID GetSliderBox(struct IBox *gadgetbox, struct IBox *sliderbox)
{
    sliderbox->Left   = gadgetbox->Left   + 2;
    sliderbox->Top    = gadgetbox->Top    + 1;
    sliderbox->Width  = gadgetbox->Width  - 4;
    sliderbox->Height = gadgetbox->Height - 2;
}

/* the gradient: one more pixel inside */
static VOID GetGradientBox(struct IBox *sliderbox, struct IBox *gradbox)
{
    gradbox->Left   = sliderbox->Left   + 1;
    gradbox->Top    = sliderbox->Top    + 1;
    gradbox->Width  = sliderbox->Width  - 2;
    gradbox->Height = sliderbox->Height - 2;
}

/* the knob position is rounded: 3.1 places it at round(cur * travel / max) */
static VOID GetKnobBox(struct GradientSliderData *data, struct IBox *sliderbox, struct IBox *knobbox)
{
    ULONG max = data->maxval ? data->maxval : 1;
    ULONG cur = data->curval > max ? max : data->curval;
    LONG  len = (data->freedom == LORIENT_HORIZ ? sliderbox->Width : sliderbox->Height) - data->knobpixels;
    LONG  pos = len > 0 ? (LONG)(((unsigned long long)cur * (ULONG)len * 2 + max) / ((unsigned long long)max * 2)) : 0;

    if (data->freedom == LORIENT_HORIZ)
    {
        knobbox->Left   = sliderbox->Left + pos;
        knobbox->Top    = sliderbox->Top;
        knobbox->Width  = data->knobpixels;
        knobbox->Height = sliderbox->Height;
    }
    else
    {
        knobbox->Left   = sliderbox->Left;
        knobbox->Top    = sliderbox->Top + pos;
        knobbox->Width  = sliderbox->Width;
        knobbox->Height = data->knobpixels;
    }
}

/* 3.1's knob: a black bar, with a gradient a pixel of background around it */
static VOID DrawKnob(struct GradientSliderData *data, struct RastPort *rp, struct DrawInfo *dri,
                     struct IBox *box)
{
    if ((box->Width > 2) && (box->Height > 2))
    {
        SetDrMd(rp, JAM1);
        if (data->numpens >= 2)
        {
            SetAPen(rp, dri->dri_Pens[BACKGROUNDPEN]);
            RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
            SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
            RectFill(rp, box->Left + 1, box->Top + 1, box->Left + box->Width - 2, box->Top + box->Height - 2);
        }
        else
        {
            SetAPen(rp, dri->dri_Pens[SHADOWPEN]);
            RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
        }
    }
}

/***************************************************************************************************/
/* gradientsliderclass.c                                                                           */
/***************************************************************************************************/

static VOID notify_curval(Class *cl, Object *o, struct GadgetInfo *gi, BOOL interim, BOOL userinput)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);
    struct opUpdate             opu;
    struct TagItem              tags[] =
    {
        {GA_ID                  , EG(o)->GadgetID       },
        {GA_UserInput           , userinput             },
        {GRAD_CurVal            , data->curval          },
        {TAG_DONE                                       }
    };

    opu.MethodID     = OM_NOTIFY;
    opu.opu_AttrList = tags;
    opu.opu_GInfo    = gi;
    opu.opu_Flags    = interim ? OPUF_INTERIM : 0;

    CL_DoMethodA(o, (Msg)&opu);
}

static VOID render(Object *o, struct GadgetInfo *gi, LONG mode)
{
    struct RastPort *rp;

    if (gi && (rp = ObtainGIRPort(gi)))
    {
        struct gpRender gpr;

        gpr.MethodID   = GM_RENDER;
        gpr.gpr_GInfo  = gi;
        gpr.gpr_RPort  = rp;
        gpr.gpr_Redraw = mode;
        CL_DoMethodA(o, (Msg)&gpr);
        ReleaseGIRPort(rp);
    }
}

static ULONG GradientSlider_SET(Class *cl, Object *o, struct opSet *msg, BOOL init)
{
    struct TagItem              *tstate;
    struct TagItem              *tag;
    ULONG                       retval = 0;
    struct GradientSliderData   *data = INST_DATA(cl, o);
    BOOL                        redraw_all = data->savebm == NULL;
    BOOL                        redraw = FALSE;
    ULONG                       oldcur = data->curval;
    LONG                        flags = EG(o)->Flags;

    if (!init)
        CL_DoSuperMethodA(cl, o, (Msg)msg);

    tstate = msg->ops_AttrList;
    while ((tag = NextTagItem(&tstate)))
    {
        ULONG tidata = tag->ti_Data;

        switch (tag->ti_Tag)
        {
            case GRAD_CurVal:
                data->curval = tidata > data->maxval ? data->maxval : tidata;
                break;

            case GA_Disabled:
                if ((EG(o)->Flags & GFLG_DISABLED) != (flags & GFLG_DISABLED))
                {
                    redraw = TRUE;
                    redraw_all = TRUE;
                }
                break;

            case GRAD_MaxVal:
                data->maxval = tidata;
                if (data->curval > data->maxval)
                    data->curval = data->maxval;
                retval = 1;
                redraw = TRUE;
                break;

            case GRAD_SkipVal:
                data->skipval = tidata;
                break;

            case GRAD_PenArray:
            {
                data->penarray = (UWORD *)tidata;
                data->numpens = 0;

                if (data->penarray)
                {
                    UWORD *pen = data->penarray;

                    while (*pen++ != (UWORD)~0) data->numpens++;
                }

                retval = 1;
                redraw = TRUE;
                redraw_all = TRUE;
            }
            break;

            default:
                break;
        }
    }

    if (data->curval != oldcur)
    {
        retval = 1;
        redraw = TRUE;
        if (!init)
            notify_curval(cl, o, msg->ops_GInfo, FALSE, FALSE);
    }

    if (!init && redraw)
        render(o, msg->ops_GInfo, redraw_all ? GREDRAW_REDRAW : GREDRAW_UPDATE);

    return retval;
}

static ULONG GradientSlider_NEW(Class *cl, Object *o, struct opSet *msg)
{
    o = (Object *)CL_DoSuperMethodA(cl, o, (Msg)msg);
    if (o)
    {
        struct GradientSliderData       *data = INST_DATA(cl, o);
        struct TagItem                  fitags[] =
        {
           {IA_FrameType        , FRAME_BUTTON  },
           {TAG_DONE                            }
        };
        UBYTE *p = (UBYTE *)data;
        ULONG i;

        for (i = 0; i < sizeof(*data); i++)
            p[i] = 0;

        if ((data->frame = NewObjectA(NULL, (ClassID)FRAMEICLASS, fitags)))
        {
            data->maxval     = 0xFFFF;
            data->curval     = 0;
            data->skipval    = 0x1111;
            data->knobpixels = GetTagData(GRAD_KnobPixels, 5, msg->ops_AttrList);
            data->freedom    = GetTagData(PGA_Freedom, LORIENT_HORIZ, msg->ops_AttrList);

            InitRastPort(&data->trp);

            GradientSlider_SET(cl, o, msg, TRUE);
        }
        else
        {
            CL_CoerceMethodA(cl, o, (Msg)&(ULONG){OM_DISPOSE});
            o = NULL;
        }
    }
    return (ULONG)o;
}

static ULONG GradientSlider_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);

    if (data->frame) DisposeObject(data->frame);

    if (data->savebm)
    {
        WaitBlit();
        FreeBitMap(data->savebm);
    }

    if (data->knobbm)
    {
        WaitBlit();
        FreeBitMap(data->knobbm);
    }

    return CL_DoSuperMethodA(cl, o, msg);
}

static ULONG GradientSlider_GET(Class *cl, Object *o, struct opGet *msg)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);

    if (msg->opg_AttrID == GRAD_CurVal)
    {
        *msg->opg_Storage = data->curval;
        return 1;
    }

    return CL_DoSuperMethodA(cl, o, (Msg)msg);
}

static ULONG GradientSlider_RENDER(Class *cl, Object *o, struct gpRender *msg)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);
    struct DrawInfo             *dri;
    struct RastPort             *rp = msg->gpr_RPort;
    struct IBox                 gbox, sbox, kbox, grbox;
    LONG                        redraw = msg->gpr_Redraw;

    if (!rp || !msg->gpr_GInfo || !(dri = msg->gpr_GInfo->gi_DrInfo))
        return 0;

    GetGadgetIBox(o, msg->gpr_GInfo, &gbox);
    GetSliderBox(&gbox, &sbox);
    sbox.Left -= gbox.Left;
    sbox.Top -= gbox.Top;
    GetKnobBox(data, &sbox, &kbox);

    switch (redraw)
    {
        case GREDRAW_UPDATE:
            if ((kbox.Width <= sbox.Width) && (kbox.Height <= sbox.Height) && (data->savebm) &&
                gbox.Width == data->savebmwidth && gbox.Height == data->savebmheight)
            {
                if ((kbox.Left != data->savefromx) || (kbox.Top != data->savefromy))
                {
                    /* Restore old area behind knob */
                    BltBitMap(data->knobbm, 0, 0, data->savebm, data->savefromx, data->savefromy,
                              kbox.Width, kbox.Height, 0xc0, 0xff, NULL);

                    data->savefromx = kbox.Left;
                    data->savefromy = kbox.Top;
                    BltBitMap(data->savebm, kbox.Left, kbox.Top, data->knobbm, 0, 0,
                              kbox.Width, kbox.Height, 0xc0, 0xff, NULL);
                    data->trp.BitMap = data->savebm;

                    DrawKnob(data, &data->trp, dri, &kbox);
                    BltBitMapRastPort(data->savebm, sbox.Left, sbox.Top, rp,
                                      gbox.Left + sbox.Left, gbox.Top + sbox.Top,
                                      sbox.Width, sbox.Height, 0xc0);
                }
                break;
            }
            /* fall through */

        case GREDRAW_REDRAW:
        {
            struct RastPort *trp = &data->trp;

            if (gbox.Width != data->savebmwidth || gbox.Height != data->savebmheight)
            {
                if (data->savebm)
                {
                    WaitBlit();
                    FreeBitMap(data->savebm);
                    data->savebm = NULL;
                }

                if (data->knobbm)
                {
                    WaitBlit();
                    FreeBitMap(data->knobbm);
                    data->knobbm = NULL;
                }
            }

            if (data->savebm == NULL)
            {
                struct TagItem fitags[] =
                {
                    {IA_Width       , gbox.Width    },
                    {IA_Height      , gbox.Height   },
                    {TAG_DONE                       }
                };

                if (gbox.Width <= 0 || gbox.Height <= 0)
                    break;

                SetAttrsA(data->frame, fitags);

                if (!(data->savebm = AllocBitMap(gbox.Width, gbox.Height,
                                                 GetBitMapAttr(rp->BitMap, BMA_DEPTH),
                                                 BMF_MINPLANES, rp->BitMap)))
                    break;

                data->savebmwidth = gbox.Width;
                data->savebmheight = gbox.Height;

                if (kbox.Width > 0 && kbox.Height > 0 &&
                    !(data->knobbm = AllocBitMap(kbox.Width, kbox.Height,
                                                 GetBitMapAttr(data->savebm, BMA_DEPTH),
                                                 BMF_MINPLANES, data->savebm)))
                {
                    FreeBitMap(data->savebm);
                    data->savebm = NULL;
                    break;
                }

                /* Draw frame */
                trp->BitMap = data->savebm;
                DrawImageState(trp, (struct Image *)data->frame, 0, 0, IDS_NORMAL, dri);
            }

            trp->BitMap = data->savebm;

            /* Draw slider background */
            GetGradientBox(&sbox, &grbox);
            if ((grbox.Width >= 2) && (grbox.Height >= 2))
            {
                if (data->numpens < 2)
                {
                    LONG pen = dri->dri_Pens[BACKGROUNDPEN];

                    if (data->penarray && (data->numpens == 1))
                        pen = data->penarray[0];

                    SetDrMd(trp, JAM1);
                    SetAPen(trp, pen);
                    RectFill(trp, grbox.Left, grbox.Top, grbox.Left + grbox.Width - 1, grbox.Top + grbox.Height - 1);
                }
                else
                {
                    DrawGradient(trp, grbox.Left, grbox.Top,
                                 grbox.Left + grbox.Width - 1, grbox.Top + grbox.Height - 1,
                                 data->penarray, data->numpens, data->freedom);
                }
            }

            /* Backup area over which knob will be drawn */
            if ((kbox.Width > 0) && (kbox.Height > 0) && data->knobbm &&
                (kbox.Width <= sbox.Width) && (kbox.Height <= sbox.Height))
            {
                BltBitMap(data->savebm, kbox.Left, kbox.Top, data->knobbm, 0, 0,
                          kbox.Width, kbox.Height, 0xc0, 0xff, NULL);

                data->savefromx    = kbox.Left;
                data->savefromy    = kbox.Top;

                /* Render knob */
                DrawKnob(data, trp, dri, &kbox);
            }

            BltBitMapRastPort(data->savebm, 0, 0, rp, gbox.Left, gbox.Top, gbox.Width, gbox.Height, 0xc0);
        }
        break;
    }

    /* 3.1 draws a disabled gradient slider like an enabled one */
    return 1;
}

static ULONG GradientSlider_HITTEST(Class *cl, Object *o, struct gpHitTest *msg)
{
    struct IBox                 gbox, sbox;
    LONG                        mousex, mousey;

    if (EG(o)->Flags & GFLG_DISABLED)
        return 0UL;

    GetGadgetIBox(o, msg->gpht_GInfo, &gbox);
    GetSliderBox(&gbox, &sbox);

    if ((sbox.Width > 2) && (sbox.Height > 2))
    {
        /* mouse coords relative to slider box */
        mousex = msg->gpht_Mouse.X - (sbox.Left - gbox.Left);
        mousey = msg->gpht_Mouse.Y - (sbox.Top  - gbox.Top);

        if ((mousex >= 0) && (mousey >= 0) &&
            (mousex < sbox.Width) && (mousey < sbox.Height))
            return GMR_GADGETHIT;
    }

    return 0;
}

static ULONG GradientSlider_GOACTIVE(Class *cl, Object *o, struct gpInput *msg)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);
    struct IBox                 gbox, sbox, kbox;
    LONG                        mousex, mousey;
    ULONG                       old_curval = data->curval;
    ULONG                       new_curval = data->curval;
    BOOL                        knobhit = TRUE;

    if (!msg->gpi_IEvent) return GMR_NOREUSE;

    GetGadgetIBox(o, msg->gpi_GInfo, &gbox);
    GetSliderBox(&gbox, &sbox);
    GetKnobBox(data, &sbox, &kbox);

    mousex = msg->gpi_Mouse.X + gbox.Left;
    mousey = msg->gpi_Mouse.Y + gbox.Top;

    data->x = msg->gpi_Mouse.X;
    data->y = msg->gpi_Mouse.Y;

    data->clickoffsetx = mousex - kbox.Left + (sbox.Left - gbox.Left);
    data->clickoffsety = mousey - kbox.Top  + (sbox.Top - gbox.Top);

    if (((data->freedom == LORIENT_HORIZ) && (mousex < kbox.Left)) ||
        ((data->freedom == LORIENT_VERT)  && (mousey < kbox.Top)))
    {
        new_curval = old_curval > data->skipval ? old_curval - data->skipval : 0;
        knobhit = FALSE;
    }
    else if (((data->freedom == LORIENT_HORIZ) && (mousex >= kbox.Left + kbox.Width)) ||
             ((data->freedom == LORIENT_VERT)  && (mousey >= kbox.Top + kbox.Height)))
    {
        new_curval = old_curval + data->skipval;
        if (new_curval > data->maxval || new_curval < old_curval) new_curval = data->maxval;
        knobhit = FALSE;
    }

    data->saveval = data->curval;

    if (!knobhit)
    {
        data->curval = new_curval;

        notify_curval(cl, o, msg->gpi_GInfo, FALSE, TRUE);
        render(o, msg->gpi_GInfo, GREDRAW_UPDATE);

        *msg->gpi_Termination = data->curval;
        return GMR_VERIFY | GMR_NOREUSE;
    }

    return GMR_MEACTIVE;
}

static ULONG GradientSlider_HANDLEINPUT(Class *cl, Object *o, struct gpInput *msg)
{
    struct GradientSliderData   *data = INST_DATA(cl, o);
    struct IBox                 gbox, sbox;
    struct InputEvent           *ie = msg->gpi_IEvent;
    LONG                        new_curval = 0;
    LONG                        mousex, mousey;
    ULONG                       retval = GMR_MEACTIVE;

    GetGadgetIBox(o, msg->gpi_GInfo, &gbox);
    GetSliderBox(&gbox, &sbox);

    if (ie->ie_Class == IECLASS_RAWMOUSE)
    {
        switch (ie->ie_Code)
        {
            case SELECTUP:
                retval = GMR_VERIFY | GMR_NOREUSE;
                *msg->gpi_Termination = data->curval;
                notify_curval(cl, o, msg->gpi_GInfo, FALSE, TRUE);
                break;

            case MENUDOWN:
                data->curval = data->saveval;
                notify_curval(cl, o, msg->gpi_GInfo, FALSE, TRUE);
                render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
                retval = GMR_NOREUSE;
                break;

            case IECODE_NOBUTTON:
                if (data->x == msg->gpi_Mouse.X && data->y == msg->gpi_Mouse.Y)
                    break;

                data->x = msg->gpi_Mouse.X;
                data->y = msg->gpi_Mouse.Y;

                mousex = msg->gpi_Mouse.X - data->clickoffsetx;
                mousey = msg->gpi_Mouse.Y - data->clickoffsety;

                if (data->freedom == LORIENT_HORIZ)
                {
                    if (sbox.Width != data->knobpixels)
                        new_curval = (LONG)(((long long)mousex * (LONG)data->maxval) /
                                            (sbox.Width - (LONG)data->knobpixels));
                }
                else
                {
                    if (sbox.Height != data->knobpixels)
                        new_curval = (LONG)(((long long)mousey * (LONG)data->maxval) /
                                            (sbox.Height - (LONG)data->knobpixels));
                }

                if (new_curval < 0)
                    new_curval = 0;
                else if ((ULONG)new_curval > data->maxval)
                    new_curval = (LONG)data->maxval;

                if ((ULONG)new_curval != data->curval)
                {
                    data->curval = (ULONG)new_curval;
                    render(o, msg->gpi_GInfo, GREDRAW_UPDATE);
                    notify_curval(cl, o, msg->gpi_GInfo, TRUE, TRUE);
                }
                break;
        }
    }

    return retval;
}

static ULONG GradientSlider_DOMAIN(Class *cl, Object *o, struct gpDomain *msg)
{
    struct GradientSliderData *data = INST_DATA(cl, o);
    struct DrawInfo           *dri = (struct DrawInfo *)GetTagData(GA_DrawInfo, 0, msg->gpd_Attrs);
    struct RastPort           *rp = msg->gpd_RPort;
    LONG                       width = 0, height = 0, x = 1, y = 1, txh = rp ? rp->TxHeight : 8;

    if (dri && dri->dri_Resolution.X && dri->dri_Resolution.Y)
    {
        y = dri->dri_Resolution.X;
        x = dri->dri_Resolution.Y;
    }

    switch (msg->gpd_Which)
    {
        case GDOMAIN_MINIMUM:
            if (data->freedom == LORIENT_VERT)
            {
                width  = 3 + ((txh * x) / y) + 3;
                height = 2 + (data->knobpixels*5) + 2;
            }
            else
            {
                width = 3 + (data->knobpixels*5) + 3;
                height = 2 + txh + 2;
            }
            break;

        case GDOMAIN_NOMINAL:
            if (data->freedom == LORIENT_VERT)
            {
                width = 3 + ((txh * x) / y) + 3;
                height = 2 + (10*data->knobpixels) + 2;
            }
            else
            {
                width = 3 + (10*data->knobpixels) + 3;
                height = 2 + txh + 2;
            }
            break;

        case GDOMAIN_MAXIMUM:
            if (data->freedom == LORIENT_VERT)
            {
                width = 3 + ((txh * x) / y) + 3;
                height = 0x7fff;
            }
            else
            {
                width = 0x7fff;
                height = 2 + txh + 2;
            }
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
        case OM_NEW:          return GradientSlider_NEW(cl, o, (struct opSet *)msg);
        case OM_DISPOSE:      return GradientSlider_DISPOSE(cl, o, msg);
        case OM_SET:
        case OM_UPDATE:       return GradientSlider_SET(cl, o, (struct opSet *)msg, FALSE);
        case OM_GET:          return GradientSlider_GET(cl, o, (struct opGet *)msg);
        case GM_RENDER:       return GradientSlider_RENDER(cl, o, (struct gpRender *)msg);
        case GM_HITTEST:      return GradientSlider_HITTEST(cl, o, (struct gpHitTest *)msg);
        case GM_GOACTIVE:     return GradientSlider_GOACTIVE(cl, o, (struct gpInput *)msg);
        case GM_HANDLEINPUT:  return GradientSlider_HANDLEINPUT(cl, o, (struct gpInput *)msg);
        case GM_DOMAIN:       return GradientSlider_DOMAIN(cl, o, (struct gpDomain *)msg);
        default:              return CL_DoSuperMethodA(cl, o, msg);
    }
}
