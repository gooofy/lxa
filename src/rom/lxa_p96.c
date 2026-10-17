/*
 * lxa Picasso96API.library (Phase 242)
 *
 * lxa's own implementation of the Picasso96 2.x application interface for
 * the emulated uaegfx board (lxa --rtg / LXA_RTG=1).  The bitmaps are the
 * chunky RTG bitmaps of graphics.library (lxa_rtg.h); pixel work runs on
 * the host (src/lxa/lxa_rtg.c).  Behaviour is checked against Picasso96
 * 2.0 on the `rtg` reference machine (tests/probes/rtg/).  Without RTG the
 * library does not open, as on a machine without a graphics board.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <graphics/gfx.h>
#include <graphics/displayinfo.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include <libraries/asl.h>
#include <clib/asl_protos.h>
#include <inline/asl.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>

#include "util.h"
#include "lxa_rtglib.h"
#include "lxa_p96.h"

#define VERSION    2
#define REVISION   300
#define EXLIBNAME  "Picasso96API"
#define EXLIBVER   " 2.300 (19.09.99)\r\n"

char __aligned _g_p96_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_p96_ExLibID   [] = EXLIBNAME ".library" EXLIBVER;
char __aligned _g_p96_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";
char __aligned _g_p96_VERSTRING [] = "\0$VER: " EXLIBNAME ".library" EXLIBVER;

static const char s_board_name[] = "uaegfx.card";
static const char s_chip_name[] = "not yet implemented";

#define P96_BOARD_MEM   (16UL * 1024UL * 1024UL)

/* the library base: struct Library plus private data (PosSize 68, reference) */
struct P96Base
{
    struct Library lib;
    UBYTE          priv[68 - sizeof(struct Library)];
};

struct Library *AslBase = NULL;

/* ------------------------------------------------------------------ */
/* library housekeeping                                                 */

struct Library * __g_lxa_p96_InitLib ( register struct Library   *libbase __asm("d0"),
                                       register BPTR              seglist __asm("a0"),
                                       register struct ExecBase  *sysb __asm("a6"))
{
    SysBase = sysb;
    if (!rtgl_enabled() || !rtgl_open_libs())
    {
        /* no graphics board: the library is not there */
        FreeMem((UBYTE *)libbase - libbase->lib_NegSize, libbase->lib_NegSize + libbase->lib_PosSize);
        return NULL;
    }
    return libbase;
}

BPTR __g_lxa_p96_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_p96_OpenLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_p96_CloseLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_p96_ExtFuncLib ( void )
{
    return 0;
}

/* private library slots */
ULONG _p96_Private ( void )
{
    PRIVATE_FUNCTION_ERROR("_p96", "Private");
    return 0;
}

/* ------------------------------------------------------------------ */
/* bitmaps                                                              */

/* -30 p96AllocBitMap */
struct BitMap * _p96_AllocBitMap ( register struct Library *P96Base __asm("a6"),
                                   register ULONG w __asm("d0"),
                                   register ULONG h __asm("d1"),
                                   register ULONG depth __asm("d2"),
                                   register ULONG flags __asm("d3"),
                                   register struct BitMap *friend_bm __asm("a0"),
                                   register ULONG fmt __asm("d7") )
{
    return rtgl_alloc_bitmap(w, h, depth, flags, friend_bm, fmt);
}

/* -36 p96FreeBitMap */
void _p96_FreeBitMap ( register struct Library *P96Base __asm("a6"),
                       register struct BitMap *bm __asm("a0") )
{
    WaitBlit();
    rtgl_free_bitmap(bm);
}

/* -42 p96GetBitMapAttr */
ULONG _p96_GetBitMapAttr ( register struct Library *P96Base __asm("a6"),
                           register struct BitMap *bm __asm("a0"),
                           register ULONG attr __asm("d0") )
{
    ULONG bpp, fmt;

    if (!lxa_rtg_is(bm))
        return 0;   /* a planar bitmap is no Picasso96 bitmap (reference) */
    bpp = LXA_RTG_EXTL(bm, LXA_RTG_EXT_BPP);
    fmt = LXA_RTG_EXTL(bm, LXA_RTG_EXT_FORMAT);
    switch (attr)
    {
        case P96BMA_WIDTH:          return LXA_RTG_EXTL(bm, LXA_RTG_EXT_WIDTH);
        case P96BMA_HEIGHT:         return bm->Rows;
        case P96BMA_DEPTH:          return LXA_RTG_EXTL(bm, LXA_RTG_EXT_DEPTH);
        case P96BMA_MEMORY:         return (ULONG)bm->Planes[0];
        case P96BMA_BYTESPERROW:    return bm->BytesPerRow;
        case P96BMA_BYTESPERPIXEL:  return bpp;
        case P96BMA_BITSPERPIXEL:
            return (fmt == RGBFB_R5G5B5PC || fmt == RGBFB_R5G5B5 || fmt == RGBFB_B5G5R5PC) ? 15 : bpp * 8;
        case P96BMA_RGBFORMAT:      return fmt;
        case P96BMA_ISP96:          return 1;
        case P96BMA_ISONBOARD:      return (LXA_RTG_EXTL(bm, LXA_RTG_EXT_FLAGS) & LXA_RTGF_ONBOARD) ? 1 : 0;
        /* the emulated board has no memory window or registers of its own:
         * its memory is ordinary emulated RAM */
        case P96BMA_BOARDMEMBASE:
        case P96BMA_BOARDIOBASE:
        case P96BMA_BOARDMEMIOBASE:
        default:
            return 0;
    }
}

/* -48 p96LockBitMap: the RenderInfo of the pixels; the memory does not
 * move in lxa, the lock only counts */
LONG _p96_LockBitMap ( register struct Library *P96Base __asm("a6"),
                       register struct BitMap *bm __asm("a0"),
                       register UBYTE *buf __asm("a1"),
                       register ULONG size __asm("d0") )
{
    struct RenderInfo *ri = (struct RenderInfo *)buf;

    if (!lxa_rtg_is(bm))
        return 0;
    if (ri && size >= 4)
        ri->Memory = bm->Planes[0];
    if (ri && size >= 6)
        ri->BytesPerRow = (WORD)bm->BytesPerRow;
    if (ri && size >= 12)
        ri->RGBFormat = LXA_RTG_EXTL(bm, LXA_RTG_EXT_FORMAT);
    Forbid();
    LXA_RTG_EXTL(bm, LXA_RTG_EXT_LOCK)++;
    Permit();
    return (LONG)lxa_rtg_ext(bm);
}

/* -54 p96UnlockBitMap */
void _p96_UnlockBitMap ( register struct Library *P96Base __asm("a6"),
                         register struct BitMap *bm __asm("a0"),
                         register LONG lock __asm("d0") )
{
    if (!lxa_rtg_is(bm) || !lock)
        return;
    Forbid();
    if (LXA_RTG_EXTL(bm, LXA_RTG_EXT_LOCK))
        LXA_RTG_EXTL(bm, LXA_RTG_EXT_LOCK)--;
    Permit();
}

/* ------------------------------------------------------------------ */
/* modes                                                                */

/* -60 p96BestModeIDTagList */
ULONG _p96_BestModeIDTagList ( register struct Library *P96Base __asm("a6"),
                               register struct TagItem *tags __asm("a0") )
{
    return rtgl_call(RTG_OP_P96BEST, (ULONG)tags, 0, 0, 0);
}

/* one board mode of a mode query (p96AllocModeList, p96RequestModeID) */
typedef struct
{
    ULONG min_w, min_h, min_d, max_w, max_h, max_d;
    ULONG allowed, forbidden;
} p96_filter_t;

static void p96_filter_init(p96_filter_t *f, struct TagItem *tags)
{
    struct TagItem *t, *state = tags;

    f->min_w = f->min_h = f->min_d = 0;
    f->max_w = f->max_h = f->max_d = 0xffffffffUL;
    f->allowed = 0xffffffffUL;
    f->forbidden = 0;
    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        switch (t->ti_Tag)
        {
            case P96MA_MinWidth:         f->min_w = t->ti_Data; break;
            case P96MA_MinHeight:        f->min_h = t->ti_Data; break;
            case P96MA_MinDepth:         f->min_d = t->ti_Data; break;
            case P96MA_MaxWidth:         f->max_w = t->ti_Data; break;
            case P96MA_MaxHeight:        f->max_h = t->ti_Data; break;
            case P96MA_MaxDepth:         f->max_d = t->ti_Data; break;
            case P96MA_FormatsAllowed:   f->allowed = t->ti_Data; break;
            case P96MA_FormatsForbidden: f->forbidden = t->ti_Data; break;
            default: break;
        }
    }
}

static BOOL p96_mode_matches(const p96_filter_t *f, ULONG id)
{
    ULONG w, h, d, fmt;

    if ((id & 0xffff0000UL) == 0xfff00000UL)
        return FALSE;   /* the board's P96Mode placeholder is no screen mode */
    if (!rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_ISP96, 0, 0))
        return FALSE;
    w = rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_WIDTH, 0, 0);
    h = rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_HEIGHT, 0, 0);
    d = rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_DEPTH, 0, 0);
    fmt = rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_RGBFORMAT, 0, 0);
    if (w < f->min_w || w > f->max_w || h < f->min_h || h > f->max_h || d < f->min_d || d > f->max_d)
        return FALSE;
    if (!(f->allowed & (1UL << fmt)) || (f->forbidden & (1UL << fmt)))
        return FALSE;
    return TRUE;
}

/* -72 p96AllocModeListTagList */
struct List * _p96_AllocModeListTagList ( register struct Library *P96Base __asm("a6"),
                                          register struct TagItem *tags __asm("a0") )
{
    p96_filter_t f;
    struct List *l;
    ULONG id = INVALID_ID;
    ULONG n = 0;

    p96_filter_init(&f, tags);
    l = (struct List *)AllocVec(sizeof(struct List), MEMF_PUBLIC | MEMF_CLEAR);
    if (!l)
        return NULL;
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
    while ((id = NextDisplayInfo(id)) != INVALID_ID)
    {
        struct P96Mode *m;

        if (!p96_mode_matches(&f, id))
            continue;
        m = (struct P96Mode *)AllocVec(sizeof(struct P96Mode), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m)
            break;
        rtgl_call(RTG_OP_P96DESC, id, (ULONG)m->Description, P96_MODENAMELENGTH, 0);
        m->Node.ln_Name = m->Description;
        m->Width = (UWORD)rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_WIDTH, 0, 0);
        m->Height = (UWORD)rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_HEIGHT, 0, 0);
        m->Depth = (UWORD)rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_DEPTH, 0, 0);
        m->DisplayID = id;
        AddTail(l, &m->Node);
        n++;
    }
    if (!n)
    {
        FreeVec(l);
        return NULL;    /* reference: no mode, no list */
    }
    return l;
}

/* -78 p96FreeModeList */
void _p96_FreeModeList ( register struct Library *P96Base __asm("a6"),
                         register struct List *l __asm("a0") )
{
    struct Node *n;

    if (!l)
        return;
    while ((n = RemHead(l)) != NULL)
        FreeVec(n);
    FreeVec(l);
}

/* -84 p96GetModeIDAttr */
ULONG _p96_GetModeIDAttr ( register struct Library *P96Base __asm("a6"),
                           register ULONG id __asm("d0"),
                           register ULONG attr __asm("d1") )
{
    if (!rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_ISP96, 0, 0))
        return 0;
    switch (attr)
    {
        case P96IDA_BOARDNAME:
            return (ULONG)s_board_name;
        case P96IDA_VIDEOCOMPATIBLE:
        case P96IDA_PABLOIVCOMPATIBLE:
        case P96IDA_PALOMAIVCOMPATIBLE:
            return 0;   /* uaegfx has no video module */
        default:
            return rtgl_call(RTG_OP_MODE_ATTR, id, attr, 0, 0);
    }
}

/* ASLSM_FilterFunc of p96RequestModeIDTagList(): board modes of the query */
static ULONG p96_req_filter(register struct Hook *hook __asm("a0"),
                            register APTR req __asm("a2"),
                            register ULONG id __asm("a1"))
{
    return p96_mode_matches((const p96_filter_t *)hook->h_Data, id) ? 1 : 0;
}

/* -66 p96RequestModeIDTagList: the screen mode requester (asl.library's,
 * limited to the board modes the tags allow) */
ULONG _p96_RequestModeIDTagList ( register struct Library *P96Base __asm("a6"),
                                  register struct TagItem *tags __asm("a0") )
{
    struct ScreenModeRequester *smr;
    struct TagItem at[16], *t, *state = tags;
    struct Hook hook;
    p96_filter_t f;
    ULONG id = INVALID_ID;
    int n = 0;

    LXA_UNIMPLEMENTED("p96", "RequestModeIDTagList",
                      "partial: shows asl.library's screen mode requester, not Picasso96's own window");
    if (!AslBase)
        AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 38);
    if (!AslBase)
        return INVALID_ID;
    p96_filter_init(&f, tags);
    hook.h_Entry = (void *)p96_req_filter;
    hook.h_SubEntry = NULL;
    hook.h_Data = &f;
    while ((t = rtgl_next_tag(&state)) != NULL && n < 12)
    {
        switch (t->ti_Tag)
        {
            case P96MA_DisplayID:     at[n].ti_Tag = ASLSM_InitialDisplayID; break;
            case P96MA_WindowTitle:   at[n].ti_Tag = ASLSM_TitleText; break;
            case P96MA_OKText:        at[n].ti_Tag = ASLSM_PositiveText; break;
            case P96MA_CancelText:    at[n].ti_Tag = ASLSM_NegativeText; break;
            case P96MA_Window:        at[n].ti_Tag = ASLSM_Window; break;
            case P96MA_PubScreenName: at[n].ti_Tag = ASLSM_PubScreenName; break;
            case P96MA_Screen:        at[n].ti_Tag = ASLSM_Screen; break;
            default: continue;
        }
        at[n++].ti_Data = t->ti_Data;
    }
    at[n].ti_Tag = ASLSM_FilterFunc;
    at[n++].ti_Data = (ULONG)&hook;
    at[n].ti_Tag = ASLSM_PropertyMask;     /* every board mode, Workbench-able or not */
    at[n++].ti_Data = 0;
    at[n].ti_Tag = TAG_DONE;
    smr = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, at);
    if (!smr)
        return INVALID_ID;
    if (AslRequest(smr, NULL))
        id = smr->sm_DisplayID;
    FreeAslRequest(smr);
    return id;
}

/* ------------------------------------------------------------------ */
/* screens                                                              */

/* P96SA_* -> SA_* (0: no counterpart, handled or ignored) */
static ULONG p96_sa_tag(ULONG t)
{
    static const UBYTE map[] = {
        0,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, /* Left..ErrorCode */
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,       /* Font..DClip */
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x20, 0x21, 0x23, 0x24,       /* ShowTitle..VideoControl */
    };
    ULONG n = t - P96SA_Dummy;

    if (t == P96SA_Exclusive)
        return SA_Exclusive;
    if (n >= 1 && n < sizeof(map))
        return SA_Dummy + map[n];
    return 0;
}

/* -90 p96OpenScreenTagList */
struct Screen * _p96_OpenScreenTagList ( register struct Library *P96Base __asm("a6"),
                                         register struct TagItem *tags __asm("a0") )
{
    struct TagItem *t, *state = tags, *st;
    struct TagItem bt[5];
    ULONG w = 640, h = 480, depth = 8, fmt = 0, id = INVALID_ID, n = 0, cnt = 0;
    LONG *errp = NULL;
    struct Screen *s;

    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        switch (t->ti_Tag)
        {
            case P96SA_Width:     w = t->ti_Data; break;
            case P96SA_Height:    h = t->ti_Data; break;
            case P96SA_Depth:     depth = t->ti_Data; break;
            case P96SA_RGBFormat: fmt = t->ti_Data; break;
            case P96SA_DisplayID: id = t->ti_Data; break;
            case P96SA_ErrorCode: errp = (LONG *)t->ti_Data; break;
            default: break;
        }
        cnt++;
    }
    if (id == INVALID_ID)
    {
        bt[0].ti_Tag = P96BIDTAG_NominalWidth;  bt[0].ti_Data = w;
        bt[1].ti_Tag = P96BIDTAG_NominalHeight; bt[1].ti_Data = h;
        bt[2].ti_Tag = P96BIDTAG_Depth;         bt[2].ti_Data = depth;
        bt[3].ti_Tag = fmt ? P96BIDTAG_FormatsAllowed : TAG_IGNORE;
        bt[3].ti_Data = 1UL << (fmt & 31);
        bt[4].ti_Tag = TAG_DONE;
        id = rtgl_call(RTG_OP_P96BEST, (ULONG)bt, 0, 0, 0);
        if (id == INVALID_ID)
        {
            if (errp)
                *errp = OSERR_UNKNOWNMODE;
            return NULL;
        }
    }
    st = (struct TagItem *)AllocVec((cnt + 4) * sizeof(struct TagItem), MEMF_PUBLIC | MEMF_CLEAR);
    if (!st)
    {
        if (errp)
            *errp = OSERR_NOMEM;
        return NULL;
    }
    state = tags;
    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        ULONG sa = p96_sa_tag(t->ti_Tag);

        if (!sa || sa == SA_DisplayID)
            continue;
        st[n].ti_Tag = sa;
        st[n++].ti_Data = t->ti_Data;
    }
    st[n].ti_Tag = SA_DisplayID;
    st[n++].ti_Data = id;
    if (fmt == RGBFB_CLUT || (!fmt && rtgl_call(RTG_OP_MODE_ATTR, id, P96IDA_RGBFORMAT, 0, 0) == RGBFB_CLUT))
    {
        st[n].ti_Tag = SA_Depth;
        st[n++].ti_Data = depth > 8 ? 8 : depth;
    }
    st[n].ti_Tag = TAG_DONE;
    s = OpenScreenTagList(NULL, st);
    FreeVec(st);
    return s;
}

/* -96 p96CloseScreen */
LONG _p96_CloseScreen ( register struct Library *P96Base __asm("a6"),
                        register struct Screen *s __asm("a0") )
{
    return CloseScreen(s) ? TRUE : FALSE;
}

/* ------------------------------------------------------------------ */
/* pixels                                                               */

/* -102 p96WritePixelArray */
void _p96_WritePixelArray ( register struct Library *P96Base __asm("a6"),
                            register struct RenderInfo *ri __asm("a0"),
                            register ULONG sx __asm("d0"),
                            register ULONG sy __asm("d1"),
                            register struct RastPort *rp __asm("a1"),
                            register ULONG dx __asm("d2"),
                            register ULONG dy __asm("d3"),
                            register ULONG w __asm("d4"),
                            register ULONG h __asm("d5") )
{
    rtgl_req_t r;

    if (!ri || !ri->Memory)
        return;
    r.op = RTG_R_WRITERGB;
    r.fg = r.bg = r.mode = r.cmap = r.extra = 0;
    r.src = ri->Memory;
    r.srcmod = ri->BytesPerRow;
    r.srcx = (WORD)sx;
    r.srcy = (WORD)sy;
    r.fmt = ri->RGBFormat;
    rtgl_render_rp(&r, rp, (WORD)dx, (WORD)dy, (WORD)w, (WORD)h, FALSE, TRUE);
}

/* -108 p96ReadPixelArray */
void _p96_ReadPixelArray ( register struct Library *P96Base __asm("a6"),
                           register struct RenderInfo *ri __asm("a0"),
                           register ULONG dx __asm("d0"),
                           register ULONG dy __asm("d1"),
                           register struct RastPort *rp __asm("a1"),
                           register ULONG sx __asm("d2"),
                           register ULONG sy __asm("d3"),
                           register ULONG w __asm("d4"),
                           register ULONG h __asm("d5") )
{
    rtgl_req_t r;

    if (!ri || !ri->Memory)
        return;
    r.op = RTG_R_READRGB;
    r.fg = r.bg = r.mode = r.cmap = r.extra = 0;
    r.src = ri->Memory;
    r.srcmod = ri->BytesPerRow;
    r.srcx = (WORD)dx;
    r.srcy = (WORD)dy;
    r.fmt = ri->RGBFormat;
    rtgl_render_rp(&r, rp, (WORD)sx, (WORD)sy, (WORD)w, (WORD)h, TRUE, FALSE);
}

/* -114 p96WritePixel */
ULONG _p96_WritePixel ( register struct Library *P96Base __asm("a6"),
                        register struct RastPort *rp __asm("a1"),
                        register ULONG x __asm("d0"),
                        register ULONG y __asm("d1"),
                        register ULONG color __asm("d2") )
{
    rtgl_req_t r = {RTG_R_FILLRGB, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    r.fg = color;
    return rtgl_render_rp(&r, rp, (WORD)x, (WORD)y, 1, 1, FALSE, TRUE) ? 0 : (ULONG)-1;
}

/* -120 p96ReadPixel: 0xAARRGGBB */
ULONG _p96_ReadPixel ( register struct Library *P96Base __asm("a6"),
                       register struct RastPort *rp __asm("a1"),
                       register ULONG x __asm("d0"),
                       register ULONG y __asm("d1") )
{
    rtgl_req_t r = {RTG_R_READPIXELRGB, 0, 0, RTGM_ALPHA, 0, NULL, 0, 0, 0, 0, 0};
    ULONG c;

    if (rp && rp->BitMap && !rp->Layer && lxa_rtg_is(rp->BitMap))
        c = rtgl_render_bm(&r, rp->BitMap, (WORD)x, (WORD)y, 1, 1);
    else
    {
        /* through a 1x1 copy; the copy is not drawn back */
        struct BitMap *tmp;
        struct RastPort trp;

        if (!rp || !rp->BitMap)
            return 0;
        tmp = rtgl_alloc_bitmap(1, 1, 8, 0, lxa_rtg_is(rp->BitMap) ? rp->BitMap : NULL, 0);
        if (!tmp)
            return 0;
        InitRastPort(&trp);
        trp.BitMap = tmp;
        if (rp->Layer)
            ClipBlit(rp, (WORD)x, (WORD)y, &trp, 0, 0, 1, 1, 0xc0);
        else
            BltBitMap(rp->BitMap, (WORD)x, (WORD)y, tmp, 0, 0, 1, 1, 0xc0, 0xff, NULL);
        c = rtgl_render_bm(&r, tmp, 0, 0, 1, 1);
        lxa_rtg_free_bitmap(tmp);
    }
    return c == 0xffffffffUL ? 0 : c;
}

/* -126 p96RectFill */
void _p96_RectFill ( register struct Library *P96Base __asm("a6"),
                     register struct RastPort *rp __asm("a1"),
                     register ULONG x0 __asm("d0"),
                     register ULONG y0 __asm("d1"),
                     register ULONG x1 __asm("d2"),
                     register ULONG y1 __asm("d3"),
                     register ULONG color __asm("d4") )
{
    rtgl_req_t r = {RTG_R_FILLRGB, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};
    LONG xa = (WORD)x0, ya = (WORD)y0, xb = (WORD)x1, yb = (WORD)y1;

    if (xb < xa || yb < ya)
        return;
    r.fg = color;
    rtgl_render_rp(&r, rp, xa, ya, xb - xa + 1, yb - ya + 1, FALSE, TRUE);
}

/* -132 p96WriteTrueColorData */
void _p96_WriteTrueColorData ( register struct Library *P96Base __asm("a6"),
                               register struct TrueColorInfo *tci __asm("a0"),
                               register ULONG sx __asm("d0"),
                               register ULONG sy __asm("d1"),
                               register struct RastPort *rp __asm("a1"),
                               register ULONG dx __asm("d2"),
                               register ULONG dy __asm("d3"),
                               register ULONG w __asm("d4"),
                               register ULONG h __asm("d5") )
{
    rtgl_req_t r = {RTG_R_WRITETC, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    if (!tci)
        return;
    r.srcx = (WORD)sx;
    r.srcy = (WORD)sy;
    r.extra = (ULONG)tci;
    rtgl_render_rp(&r, rp, (WORD)dx, (WORD)dy, (WORD)w, (WORD)h, FALSE, TRUE);
}

/* -138 p96ReadTrueColorData */
void _p96_ReadTrueColorData ( register struct Library *P96Base __asm("a6"),
                              register struct TrueColorInfo *tci __asm("a0"),
                              register ULONG dx __asm("d0"),
                              register ULONG dy __asm("d1"),
                              register struct RastPort *rp __asm("a1"),
                              register ULONG sx __asm("d2"),
                              register ULONG sy __asm("d3"),
                              register ULONG w __asm("d4"),
                              register ULONG h __asm("d5") )
{
    rtgl_req_t r = {RTG_R_READTC, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    if (!tci)
        return;
    r.srcx = (WORD)dx;
    r.srcy = (WORD)dy;
    r.extra = (ULONG)tci;
    rtgl_render_rp(&r, rp, (WORD)sx, (WORD)sy, (WORD)w, (WORD)h, TRUE, FALSE);
}

/* ------------------------------------------------------------------ */
/* picture in picture: uaegfx has no video overlay                      */

/* -144 p96PIP_OpenTagList */
struct Window * _p96_PIP_OpenTagList ( register struct Library *P96Base __asm("a6"),
                                       register struct TagItem *tags __asm("a0") )
{
    struct TagItem *t, *state = tags;

    /* reference (uaegfx.card): the board cannot show a PIP window */
    while ((t = rtgl_next_tag(&state)) != NULL)
        if (t->ti_Tag == P96PIP_ErrorCode && t->ti_Data)
            *(LONG *)t->ti_Data = OSERR_NOCHIPS;
    return NULL;
}

/* -150 p96PIP_Close */
LONG _p96_PIP_Close ( register struct Library *P96Base __asm("a6"),
                      register struct Window *w __asm("a0") )
{
    return 0;
}

/* -156 p96PIP_SetTagList */
LONG _p96_PIP_SetTagList ( register struct Library *P96Base __asm("a6"),
                           register struct Window *w __asm("a0"),
                           register struct TagItem *tags __asm("a1") )
{
    return 0;
}

/* -162 p96PIP_GetTagList */
LONG _p96_PIP_GetTagList ( register struct Library *P96Base __asm("a6"),
                           register struct Window *w __asm("a0"),
                           register struct TagItem *tags __asm("a1") )
{
    return 0;
}

/* ------------------------------------------------------------------ */
/* board information                                                    */

/* -180 p96GetRTGDataTagList */
LONG _p96_GetRTGDataTagList ( register struct Library *P96Base __asm("a6"),
                              register struct TagItem *tags __asm("a0") )
{
    struct TagItem *t, *state = tags;
    LONG n = 0;

    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        if (t->ti_Tag == P96RD_NumberOfBoards)
        {
            if (t->ti_Data)
                *(ULONG *)t->ti_Data = 1;
            n++;
        }
    }
    return n;
}

/* board memory in use: the Workbench screen when it is on the board */
static ULONG p96_board_free(void)
{
    struct Screen *wb = LockPubScreen(NULL);
    ULONG used = 0;

    if (wb)
    {
        struct BitMap *bm = wb->RastPort.BitMap;
        if (lxa_rtg_is(bm))
            used = (ULONG)bm->BytesPerRow * bm->Rows + 16;
        UnlockPubScreen(NULL, wb);
    }
    return P96_BOARD_MEM - used;
}

/* -186 p96GetBoardDataTagList */
LONG _p96_GetBoardDataTagList ( register struct Library *P96Base __asm("a6"),
                                register ULONG board __asm("d0"),
                                register struct TagItem *tags __asm("a0") )
{
    struct TagItem *t, *state = tags;
    LONG n = 0;

    if (board != 0)
        return -1;
    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        ULONG v;

        switch (t->ti_Tag)
        {
            case P96BD_BoardName:         v = (ULONG)s_board_name; break;
            case P96BD_ChipName:          v = (ULONG)s_chip_name; break;
            case P96BD_TotalMemory:       v = P96_BOARD_MEM; break;
            case P96BD_FreeMemory:
            case P96BD_LargestFreeMemory: v = p96_board_free(); break;
            case P96BD_MonitorSwitch:     v = 0; break;
            case P96BD_RGBFormats:        v = RGBFF_CLUT | RGBFF_R5G6B5PC | RGBFF_B8G8R8A8; break;
            case P96BD_MemoryClock:       v = 0; break;
            default: continue;
        }
        if (t->ti_Data)
            *(ULONG *)t->ti_Data = v;
        n++;
    }
    return n;
}

/* -192 p96EncodeColor */
ULONG _p96_EncodeColor ( register struct Library *P96Base __asm("a6"),
                         register ULONG fmt __asm("d0"),
                         register ULONG color __asm("d1") )
{
    return rtgl_call(RTG_OP_ENCODE, fmt, color, 0, 0);
}

/* ------------------------------------------------------------------ */

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_p96_FuncTab [];
extern struct MyDataInit __g_lxa_p96_DataTab;
extern struct InitTable  __g_lxa_p96_InitTab;
extern APTR              __g_lxa_p96_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_p96_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    &_g_p96_ExLibName[0],
    &_g_p96_ExLibID[0],
    &__g_lxa_p96_InitTab
};

APTR __g_lxa_p96_EndResident;
struct Resident *__lxa_p96_ROMTag = &ROMTag;

struct InitTable __g_lxa_p96_InitTab =
{
    (ULONG)               sizeof(struct P96Base),
    (APTR              *) &__g_lxa_p96_FuncTab[0],
    (APTR)                &__g_lxa_p96_DataTab,
    (APTR)                __g_lxa_p96_InitLib
};

/* 4 standard + 39 vectors: NegSize 260 (reference) */
APTR __g_lxa_p96_FuncTab [] =
{
    __g_lxa_p96_OpenLib,             // -6
    __g_lxa_p96_CloseLib,            // -12
    __g_lxa_p96_ExpungeLib,          // -18
    __g_lxa_p96_ExtFuncLib,          // -24
    _p96_AllocBitMap,                // -30
    _p96_FreeBitMap,                 // -36
    _p96_GetBitMapAttr,              // -42
    _p96_LockBitMap,                 // -48
    _p96_UnlockBitMap,               // -54
    _p96_BestModeIDTagList,          // -60
    _p96_RequestModeIDTagList,       // -66
    _p96_AllocModeListTagList,       // -72
    _p96_FreeModeList,               // -78
    _p96_GetModeIDAttr,              // -84
    _p96_OpenScreenTagList,          // -90
    _p96_CloseScreen,                // -96
    _p96_WritePixelArray,            // -102
    _p96_ReadPixelArray,             // -108
    _p96_WritePixel,                 // -114
    _p96_ReadPixel,                  // -120
    _p96_RectFill,                   // -126
    _p96_WriteTrueColorData,         // -132
    _p96_ReadTrueColorData,          // -138
    _p96_PIP_OpenTagList,            // -144
    _p96_PIP_Close,                  // -150
    _p96_PIP_SetTagList,             // -156
    _p96_PIP_GetTagList,             // -162
    _p96_Private,                    // -168 private
    _p96_Private,                    // -174 private
    _p96_GetRTGDataTagList,          // -180
    _p96_GetBoardDataTagList,        // -186
    _p96_EncodeColor,                // -192
    _p96_Private,                    // -198 private
    _p96_Private,                    // -204
    _p96_Private,                    // -210
    _p96_Private,                    // -216
    _p96_Private,                    // -222
    _p96_Private,                    // -228
    _p96_Private,                    // -234
    _p96_Private,                    // -240
    _p96_Private,                    // -246
    _p96_Private,                    // -252
    _p96_Private,                    // -258
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_p96_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_p96_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_p96_ExLibID[0],
    (ULONG) 0
};
