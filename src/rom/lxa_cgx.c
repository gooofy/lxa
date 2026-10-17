/*
 * lxa cybergraphics.library (Phase 242)
 *
 * lxa's own implementation of the CyberGraphX v41 application interface
 * for the emulated uaegfx board (lxa --rtg / LXA_RTG=1), as Picasso96
 * offers it: a separate disk library that works on the same chunky RTG
 * bitmaps as Picasso96API.library (lxa_rtg.h, pixel work on the host in
 * src/lxa/lxa_rtg.c).  Behaviour is checked against the cybergraphics
 * emulation of Picasso96 2.0 on the `rtg` reference machine
 * (tests/probes/rtg/cgx.c).  Without RTG the library does not open.
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
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <clib/layers_protos.h>
#include <inline/layers.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <clib/asl_protos.h>
#include <inline/asl.h>
#include <utility/hooks.h>
#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>
#include <cybergraphx/cybergraphics.h>

#include "util.h"
#include "lxa_rtglib.h"
#include "lxa_p96.h"

#define VERSION    41
#define REVISION   4
#define EXLIBNAME  "cybergraphics"
#define EXLIBVER   " 41.4 (27.7.98)"

char __aligned _g_cgx_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_cgx_ExLibID   [] = EXLIBNAME ".library" EXLIBVER;
char __aligned _g_cgx_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";
char __aligned _g_cgx_VERSTRING [] = "\0$VER: " EXLIBNAME ".library" EXLIBVER;

static const char s_board_name[] = "uaegfx.card";

struct Library *AslBase = NULL;

/* ------------------------------------------------------------------ */
/* library housekeeping                                                 */

struct Library * __g_lxa_cgx_InitLib ( register struct Library   *libbase __asm("d0"),
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

BPTR __g_lxa_cgx_ExpungeLib ( register struct Library *libbase __asm("a6") )
{
    return 0;
}

struct Library * __g_lxa_cgx_OpenLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt++;
    libbase->lib_Flags &= ~LIBF_DELEXP;
    return libbase;
}

BPTR __g_lxa_cgx_CloseLib ( register struct Library *libbase __asm("a6") )
{
    libbase->lib_OpenCnt--;
    return 0;
}

ULONG __g_lxa_cgx_ExtFuncLib ( void )
{
    return 0;
}

/* private library slots */
ULONG _cgx_Private ( void )
{
    PRIVATE_FUNCTION_ERROR("_cgx", "Private");
    return 0;
}

/* ------------------------------------------------------------------ */
/* helpers                                                              */

/* RGBFTYPE -> CyberGraphX PIXFMT */
static ULONG cgx_pixfmt(ULONG fmt)
{
    switch (fmt)
    {
        case RGBFB_R8G8B8:   return PIXFMT_RGB24;
        case RGBFB_B8G8R8:   return PIXFMT_BGR24;
        case RGBFB_R5G6B5PC: return PIXFMT_RGB16PC;
        case RGBFB_R5G5B5PC: return PIXFMT_RGB15PC;
        case RGBFB_A8R8G8B8: return PIXFMT_ARGB32;
        case RGBFB_A8B8G8R8: return PIXFMT_ARGB32;  /* CyberGraphX has no ABGR format */
        case RGBFB_R8G8B8A8: return PIXFMT_RGBA32;
        case RGBFB_B8G8R8A8: return PIXFMT_BGRA32;
        case RGBFB_R5G6B5:   return PIXFMT_RGB16;
        case RGBFB_R5G5B5:   return PIXFMT_RGB15;
        case RGBFB_B5G6R5PC: return PIXFMT_BGR16PC;
        case RGBFB_B5G5R5PC: return PIXFMT_BGR15PC;
        default:             return PIXFMT_LUT8;
    }
}

static ULONG mode_attr(ULONG id, ULONG attr)
{
    return rtgl_call(RTG_OP_MODE_ATTR, id, attr, 0, 0);
}

/* a board mode of the display database (not the P96Mode placeholder) */
static BOOL cgx_is_mode(ULONG id)
{
    return (id & 0xffff0000UL) != 0xfff00000UL && mode_attr(id, P96IDA_ISP96) != 0;
}

/* the RTG bitmap of a RastPort, NULL for planar ones */
static struct BitMap *cgx_rtg_bm(struct RastPort *rp)
{
    if (!rp || !rp->BitMap || !lxa_rtg_is(rp->BitMap))
        return NULL;
    return rp->BitMap;
}

static ULONG cgx_bm_fmt(struct BitMap *bm)
{
    return LXA_RTG_EXTL(bm, LXA_RTG_EXT_FORMAT);
}

static char *put_dec(char *p, ULONG v, int width, BOOL left)
{
    char tmp[12];
    int n = 0;

    do
    {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < (int)sizeof(tmp));
    if (!left)
        while (width-- > n)
            *p++ = ' ';
    else
        width -= n;
    while (n)
        *p++ = tmp[--n];
    if (left)
        while (width-- > 0)
            *p++ = ' ';
    return p;
}

/* CyberModeNode.ModeText: "uaegfx.card:%4dx%-4dx%2d %s" */
static void cgx_mode_text(char *out, ULONG w, ULONG h, ULONG d, ULONG fmt)
{
    const char *s;
    char *p = out;

    for (s = s_board_name; *s; s++)
        *p++ = *s;
    *p++ = ':';
    p = put_dec(p, w, 4, FALSE);
    *p++ = 'x';
    p = put_dec(p, h, 4, TRUE);
    *p++ = 'x';
    p = put_dec(p, d, 2, FALSE);
    *p++ = ' ';
    switch (fmt)
    {
        case RGBFB_CLUT:     s = "CLUT"; break;
        case RGBFB_R5G6B5PC: s = "RGBPC"; break;
        case RGBFB_B8G8R8A8: s = "BGRA"; break;
        case RGBFB_R8G8B8:   s = "RGB"; break;
        case RGBFB_A8R8G8B8: s = "ARGB"; break;
        default:             s = "RGB"; break;
    }
    while (*s && p < out + DISPLAYNAMELEN - 1)
        *p++ = *s++;
    *p = 0;
}

/* ------------------------------------------------------------------ */
/* modes                                                                */

/* -54 IsCyberModeID */
LONG _cgx_IsCyberModeID ( register struct Library *CyberGfxBase __asm("a6"),
                          register ULONG id __asm("d0") )
{
    return cgx_is_mode(id) ? -1 : 0;
}

/* -60 BestCModeIDTagList: the shallowest pixel format that holds the
 * depth, then the smallest mode that holds the nominal size (else the
 * format's first mode) */
ULONG _cgx_BestCModeIDTagList ( register struct Library *CyberGfxBase __asm("a6"),
                                register struct TagItem *tags __asm("a0") )
{
    struct TagItem *t, *state = tags;
    ULONG depth = 8, nw = 640, nh = 480;
    ULONG id, fbest = 0, dbest = 0, best = INVALID_ID, first = INVALID_ID, abest = 0;

    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        switch (t->ti_Tag)
        {
            case CYBRBIDTG_Depth:         depth = t->ti_Data; break;
            case CYBRBIDTG_NominalWidth:  nw = (UWORD)t->ti_Data; break;
            case CYBRBIDTG_NominalHeight: nh = (UWORD)t->ti_Data; break;
            case CYBRBIDTG_BoardName:
            {
                const char *a = (const char *)t->ti_Data, *b = s_board_name;

                if (!a)
                    return INVALID_ID;
                while (*a && *a == *b)
                {
                    a++;
                    b++;
                }
                /* "uaegfx" or "uaegfx.card" */
                if (*a || (*b && *b != '.'))
                    return INVALID_ID;
                break;
            }
            default:    /* CYBRBIDTG_MonitorID: there is one board */
                break;
        }
    }
    /* the pixel format: the smallest P96 depth that holds the request */
    for (id = rtgl_call(RTG_OP_NEXT_ID, INVALID_ID, 0, 0, 0); id != INVALID_ID;
         id = rtgl_call(RTG_OP_NEXT_ID, id, 0, 0, 0))
    {
        ULONG d;

        if (!cgx_is_mode(id))
            continue;
        d = mode_attr(id, P96IDA_DEPTH);
        if (d >= depth && (!dbest || d < dbest))
        {
            dbest = d;
            fbest = mode_attr(id, P96IDA_RGBFORMAT);
        }
    }
    if (!dbest)
        return INVALID_ID;
    for (id = rtgl_call(RTG_OP_NEXT_ID, INVALID_ID, 0, 0, 0); id != INVALID_ID;
         id = rtgl_call(RTG_OP_NEXT_ID, id, 0, 0, 0))
    {
        ULONG w, h;

        if (!cgx_is_mode(id) || mode_attr(id, P96IDA_RGBFORMAT) != fbest)
            continue;
        if (first == INVALID_ID)
            first = id;
        w = mode_attr(id, P96IDA_WIDTH);
        h = mode_attr(id, P96IDA_HEIGHT);
        if (w >= nw && h >= nh && (best == INVALID_ID || w * h < abest))
        {
            best = id;
            abest = w * h;
        }
    }
    return best != INVALID_ID ? best : first;
}

typedef struct
{
    ULONG min_w, max_w, min_h, max_h, min_d, max_d;
    UWORD *models;
} cgx_filter_t;

static void cgx_filter_init(cgx_filter_t *f, struct TagItem *tags)
{
    struct TagItem *t, *state = tags;

    /* the defaults of the CyberGraphX autodoc */
    f->min_w = 320;
    f->max_w = 1600;
    f->min_h = 240;
    f->max_h = 1200;
    f->min_d = 8;
    f->max_d = 32;
    f->models = NULL;
    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        switch (t->ti_Tag)
        {
            case CYBRMREQ_MinWidth:    f->min_w = t->ti_Data; break;
            case CYBRMREQ_MaxWidth:    f->max_w = t->ti_Data; break;
            case CYBRMREQ_MinHeight:   f->min_h = t->ti_Data; break;
            case CYBRMREQ_MaxHeight:   f->max_h = t->ti_Data; break;
            case CYBRMREQ_MinDepth:    f->min_d = t->ti_Data; break;
            case CYBRMREQ_MaxDepth:    f->max_d = t->ti_Data; break;
            case CYBRMREQ_CModelArray: f->models = (UWORD *)t->ti_Data; break;
            default: break;
        }
    }
}

static BOOL cgx_filter_match(const cgx_filter_t *f, ULONG id)
{
    ULONG w, h, d;

    if (!cgx_is_mode(id))
        return FALSE;
    w = mode_attr(id, P96IDA_WIDTH);
    h = mode_attr(id, P96IDA_HEIGHT);
    d = mode_attr(id, P96IDA_DEPTH);
    if (w < f->min_w || w > f->max_w || h < f->min_h || h > f->max_h || d < f->min_d || d > f->max_d)
        return FALSE;
    if (f->models)
    {
        ULONG pf = cgx_pixfmt(mode_attr(id, P96IDA_RGBFORMAT));
        UWORD *m;

        for (m = f->models; *m != (UWORD)~0; m++)
            if (*m == pf)
                return TRUE;
        return FALSE;
    }
    return TRUE;
}

/* -72 AllocCModeListTagList */
struct List * _cgx_AllocCModeListTagList ( register struct Library *CyberGfxBase __asm("a6"),
                                           register struct TagItem *tags __asm("a1") )
{
    cgx_filter_t f;
    struct List *l;
    ULONG id, n = 0;

    cgx_filter_init(&f, tags);
    l = (struct List *)AllocVec(sizeof(struct List), MEMF_PUBLIC | MEMF_CLEAR);
    if (!l)
        return NULL;
    l->lh_Head = (struct Node *)&l->lh_Tail;
    l->lh_Tail = NULL;
    l->lh_TailPred = (struct Node *)&l->lh_Head;
    for (id = rtgl_call(RTG_OP_NEXT_ID, INVALID_ID, 0, 0, 0); id != INVALID_ID;
         id = rtgl_call(RTG_OP_NEXT_ID, id, 0, 0, 0))
    {
        struct CyberModeNode *m;

        if (!cgx_filter_match(&f, id))
            continue;
        m = (struct CyberModeNode *)AllocVec(sizeof(struct CyberModeNode), MEMF_PUBLIC | MEMF_CLEAR);
        if (!m)
            break;
        m->DisplayID = id;
        m->Width = (UWORD)mode_attr(id, P96IDA_WIDTH);
        m->Height = (UWORD)mode_attr(id, P96IDA_HEIGHT);
        m->Depth = (UWORD)mode_attr(id, P96IDA_DEPTH);
        cgx_mode_text(m->ModeText, m->Width, m->Height, m->Depth, mode_attr(id, P96IDA_RGBFORMAT));
        m->Node.ln_Name = m->ModeText;
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

/* -78 FreeCModeList */
void _cgx_FreeCModeList ( register struct Library *CyberGfxBase __asm("a6"),
                          register struct List *l __asm("a0") )
{
    struct Node *n;

    if (!l)
        return;
    while ((n = RemHead(l)) != NULL)
        FreeVec(n);
    FreeVec(l);
}

/* ASLSM_FilterFunc of CModeRequestTagList() */
static ULONG cgx_req_filter(register struct Hook *hook __asm("a0"),
                            register APTR req __asm("a2"),
                            register ULONG id __asm("a1"))
{
    return cgx_filter_match((const cgx_filter_t *)hook->h_Data, id) ? 1 : 0;
}

/* -66 CModeRequestTagList: the screen mode requester (asl.library's,
 * limited to the board modes the tags allow); 0 when cancelled */
ULONG _cgx_CModeRequestTagList ( register struct Library *CyberGfxBase __asm("a6"),
                                 register APTR request __asm("a0"),
                                 register struct TagItem *tags __asm("a1") )
{
    struct ScreenModeRequester *smr;
    struct TagItem at[10], *t, *state = tags;
    struct Hook hook;
    cgx_filter_t f;
    ULONG id = 0;
    int n = 0;

    LXA_UNIMPLEMENTED("cgx", "CModeRequestTagList",
                      "partial: shows asl.library's screen mode requester, not CyberGraphX's own window (Phase 246)");
    if (!AslBase)
        AslBase = OpenLibrary((CONST_STRPTR)"asl.library", 38);
    if (!AslBase)
        return 0;
    cgx_filter_init(&f, tags);
    hook.h_Entry = (void *)cgx_req_filter;
    hook.h_SubEntry = NULL;
    hook.h_Data = &f;
    while ((t = rtgl_next_tag(&state)) != NULL && n < 6)
    {
        switch (t->ti_Tag)
        {
            case CYBRMREQ_WinTitle:   at[n].ti_Tag = ASLSM_TitleText; break;
            case CYBRMREQ_OKText:     at[n].ti_Tag = ASLSM_PositiveText; break;
            case CYBRMREQ_CancelText: at[n].ti_Tag = ASLSM_NegativeText; break;
            case CYBRMREQ_Screen:     at[n].ti_Tag = ASLSM_Screen; break;
            default: continue;
        }
        at[n++].ti_Data = t->ti_Data;
    }
    at[n].ti_Tag = ASLSM_FilterFunc;
    at[n++].ti_Data = (ULONG)&hook;
    at[n].ti_Tag = ASLSM_PropertyMask;
    at[n++].ti_Data = 0;
    at[n].ti_Tag = TAG_DONE;
    smr = (struct ScreenModeRequester *)AllocAslRequest(ASL_ScreenModeRequest, at);
    if (!smr)
        return 0;
    if (AslRequest(smr, NULL))
        id = smr->sm_DisplayID;
    FreeAslRequest(smr);
    return id;
}

/* -102 GetCyberIDAttr */
ULONG _cgx_GetCyberIDAttr ( register struct Library *CyberGfxBase __asm("a6"),
                            register ULONG attr __asm("d0"),
                            register ULONG id __asm("d1") )
{
    /* every mode of the board, the P96Mode placeholder included */
    if (!mode_attr(id, P96IDA_ISP96))
        return 0xffffffffUL;
    switch (attr)
    {
        case CYBRIDATTR_PIXFMT: return cgx_pixfmt(mode_attr(id, P96IDA_RGBFORMAT));
        case CYBRIDATTR_WIDTH:  return mode_attr(id, P96IDA_WIDTH);
        case CYBRIDATTR_HEIGHT: return mode_attr(id, P96IDA_HEIGHT);
        case CYBRIDATTR_DEPTH:  return mode_attr(id, P96IDA_BITSPERPIXEL);
        case CYBRIDATTR_BPPIX:  return mode_attr(id, P96IDA_BYTESPERPIXEL);
        default:                return 0xffffffffUL;
    }
}

/* -162 CVideoCtrlTagList: the emulated board has no power management;
 * SETVC_DPMSLevel is accepted and changes nothing */
void _cgx_CVideoCtrlTagList ( register struct Library *CyberGfxBase __asm("a6"),
                              register struct ViewPort *vp __asm("a0"),
                              register struct TagItem *tags __asm("a1") )
{
}

/* ------------------------------------------------------------------ */
/* bitmaps                                                              */

/* -96 GetCyberMapAttr */
ULONG _cgx_GetCyberMapAttr ( register struct Library *CyberGfxBase __asm("a6"),
                             register struct BitMap *bm __asm("a0"),
                             register ULONG attr __asm("d0") )
{
    ULONG bpp;

    if (attr < CYBRMATTR_XMOD || attr > CYBRMATTR_ISLINEARMEM)
        return 0xffffffffUL;
    if (!bm)
        return 0;
    if (!lxa_rtg_is(bm))
        /* a planar bitmap: no CyberGraphX bitmap (reference: all 0, the
         * private display address some non-pixel value) */
        return attr == CYBRMATTR_DISPADR ? (ULONG)bm : 0;
    bpp = LXA_RTG_EXTL(bm, LXA_RTG_EXT_BPP);
    switch (attr)
    {
        case CYBRMATTR_XMOD:        return bm->BytesPerRow;
        case CYBRMATTR_BPPIX:       return bpp;
        case CYBRMATTR_DISPADR:     return (ULONG)bm->Planes[0];
        case CYBRMATTR_PIXFMT:      return cgx_pixfmt(cgx_bm_fmt(bm));
        case CYBRMATTR_WIDTH:       return LXA_RTG_EXTL(bm, LXA_RTG_EXT_WIDTH);
        case CYBRMATTR_HEIGHT:      return bm->Rows;
        case CYBRMATTR_DEPTH:       return bpp * 8;
        default:                    return 0xffffffffUL;    /* ISCYBERGFX, ISLINEARMEM */
    }
}

/* -168 LockBitMapTagList: the pixels do not move in lxa, the lock only
 * counts; the handle is the bitmap's RTG header */
APTR _cgx_LockBitMapTagList ( register struct Library *CyberGfxBase __asm("a6"),
                              register struct BitMap *bm __asm("a0"),
                              register struct TagItem *tags __asm("a1") )
{
    struct TagItem *t, *state = tags;
    ULONG bpp, fmt;

    if (!bm || !lxa_rtg_is(bm))
        return NULL;
    bpp = LXA_RTG_EXTL(bm, LXA_RTG_EXT_BPP);
    fmt = cgx_bm_fmt(bm);
    while ((t = rtgl_next_tag(&state)) != NULL)
    {
        ULONG v;

        switch (t->ti_Tag)
        {
            case LBMI_WIDTH:       v = LXA_RTG_EXTL(bm, LXA_RTG_EXT_WIDTH); break;
            case LBMI_HEIGHT:      v = bm->Rows; break;
            case LBMI_DEPTH:
                v = bpp == 4 ? 24 :
                    (fmt == RGBFB_R5G5B5PC || fmt == RGBFB_R5G5B5 || fmt == RGBFB_B5G5R5PC) ? 15 : bpp * 8;
                break;
            case LBMI_PIXFMT:      v = cgx_pixfmt(fmt); break;
            case LBMI_BYTESPERPIX: v = bpp; break;
            case LBMI_BYTESPERROW: v = bm->BytesPerRow; break;
            case LBMI_BASEADDRESS: v = (ULONG)bm->Planes[0]; break;
            default: continue;
        }
        if (t->ti_Data)
            *(ULONG *)t->ti_Data = v;
    }
    Forbid();
    LXA_RTG_EXTL(bm, LXA_RTG_EXT_LOCK)++;
    Permit();
    return lxa_rtg_ext(bm);
}

static void cgx_unlock(APTR handle)
{
    ULONG *ext = (ULONG *)handle;

    if (!ext || ext[LXA_RTG_EXT_MAGIC / 4] != LXA_RTG_MAGIC)
        return;
    Forbid();
    if (ext[LXA_RTG_EXT_LOCK / 4])
        ext[LXA_RTG_EXT_LOCK / 4]--;
    Permit();
}

/* -174 UnLockBitMap */
void _cgx_UnLockBitMap ( register struct Library *CyberGfxBase __asm("a6"),
                         register APTR handle __asm("a0") )
{
    cgx_unlock(handle);
}

/* -180 UnLockBitMapTagList: UBMI_UPDATERECTS needs no work (the pixel
 * memory is what the display shows); UBMI_REALLYUNLOCK FALSE keeps the lock */
void _cgx_UnLockBitMapTagList ( register struct Library *CyberGfxBase __asm("a6"),
                                register APTR handle __asm("a0"),
                                register struct TagItem *tags __asm("a1") )
{
    struct TagItem *t, *state = tags;
    BOOL really = TRUE;

    while ((t = rtgl_next_tag(&state)) != NULL)
        if (t->ti_Tag == UBMI_REALLYUNLOCK)
            really = t->ti_Data ? TRUE : FALSE;
    if (really)
        cgx_unlock(handle);
}

/* ------------------------------------------------------------------ */
/* pixels                                                               */

static ULONG cgx_rect(ULONG op, ULONG fg, ULONG mode, struct RastPort *rp, LONG x, LONG y, LONG w, LONG h,
                      BOOL read_first, BOOL write_back)
{
    rtgl_req_t r = {0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    r.op = op;
    r.fg = fg;
    r.mode = mode;
    return rtgl_render_rp(&r, rp, x, y, w, h, read_first, write_back);
}

/* -108 ReadRGBPixel: 0xAARRGGBB, the pen number on CLUT bitmaps */
ULONG _cgx_ReadRGBPixel ( register struct Library *CyberGfxBase __asm("a6"),
                          register struct RastPort *rp __asm("a1"),
                          register UWORD x __asm("d0"),
                          register UWORD y __asm("d1") )
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    ULONG c;

    if (!bm)
        return 0;
    if (cgx_bm_fmt(bm) == RGBFB_CLUT)
        c = cgx_rect(RTG_R_READPIXEL, 0, 0, rp, x, y, 1, 1, TRUE, FALSE);
    else
        c = cgx_rect(RTG_R_READPIXELRGB, 0, RTGM_ALPHA, rp, x, y, 1, 1, TRUE, FALSE);
    return c == 0xffffffffUL ? 0 : c;
}

/* -114 WriteRGBPixel: 0, -1 outside the RastPort; no alpha is stored */
LONG _cgx_WriteRGBPixel ( register struct Library *CyberGfxBase __asm("a6"),
                          register struct RastPort *rp __asm("a1"),
                          register UWORD x __asm("d0"),
                          register UWORD y __asm("d1"),
                          register ULONG argb __asm("d2") )
{
    if (!cgx_rtg_bm(rp))
        return 0;
    return cgx_rect(RTG_R_WRITEPIXELRGB, argb & 0x00ffffffUL, 0, rp, x, y, 1, 1, FALSE, TRUE) ? 0 : -1;
}

/* -150 FillPixelArray */
ULONG _cgx_FillPixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                            register struct RastPort *rp __asm("a1"),
                            register UWORD x __asm("d0"),
                            register UWORD y __asm("d1"),
                            register UWORD w __asm("d2"),
                            register UWORD h __asm("d3"),
                            register ULONG argb __asm("d4") )
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    ULONG c = argb & 0x00ffffffUL;

    if (bm)
    {
        /* reference: on A8R8G8B8 bitmaps red and blue trade places */
        if (cgx_bm_fmt(bm) == RGBFB_A8R8G8B8)
            c = ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff);
        cgx_rect(RTG_R_FILLRGB, c, 0, rp, x, y, w, h, FALSE, TRUE);
    }
    return (ULONG)w * h;
}

/* -144 InvertPixelArray: the colour bits, not the alpha byte */
ULONG _cgx_InvertPixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                              register struct RastPort *rp __asm("a1"),
                              register UWORD x __asm("d0"),
                              register UWORD y __asm("d1"),
                              register UWORD w __asm("d2"),
                              register UWORD h __asm("d3") )
{
    if (cgx_rtg_bm(rp))
        cgx_rect(RTG_R_INVERT, 0, RTGM_PIXEL, rp, x, y, w, h, TRUE, TRUE);
    return (ULONG)w * h;
}

/* WritePixelArray()/ScalePixelArray() of a RECTFMT array */
static void cgx_write_array(APTR src, LONG sx, LONG sy, LONG smod, struct RastPort *rp, LONG dx, LONG dy,
                            LONG w, LONG h, ULONG rectfmt)
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    rtgl_req_t r = {RTG_R_WRITERGB, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    if (!bm || !src || rectfmt > RECTFMT_GREY8)
        return;
    /* reference: only pen arrays reach CLUT bitmaps */
    if (cgx_bm_fmt(bm) == RGBFB_CLUT && rectfmt != RECTFMT_LUT8)
        return;
    r.src = src;
    r.srcmod = smod;
    r.srcx = sx;
    r.srcy = sy;
    r.fmt = RTG_FMT_CGX_BASE + rectfmt;
    rtgl_render_rp(&r, rp, dx, dy, w, h, FALSE, TRUE);
}

/* -126 WritePixelArray */
ULONG _cgx_WritePixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                             register APTR src __asm("a0"),
                             register UWORD sx __asm("d0"),
                             register UWORD sy __asm("d1"),
                             register UWORD smod __asm("d2"),
                             register struct RastPort *rp __asm("a1"),
                             register UWORD dx __asm("d3"),
                             register UWORD dy __asm("d4"),
                             register UWORD w __asm("d5"),
                             register UWORD h __asm("d6"),
                             register UBYTE fmt __asm("d7") )
{
    ULONG n = (ULONG)w * h;

    cgx_write_array(src, sx, sy, smod, rp, dx, dy, w, h, fmt);
    return n;
}

/* -198 WriteLUTPixelArray: pens through a 0x00RRGGBB colour table
 * (CTABFMT_XRGB8) to a true colour bitmap */
ULONG _cgx_WriteLUTPixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                                register APTR src __asm("a0"),
                                register UWORD sx __asm("d0"),
                                register UWORD sy __asm("d1"),
                                register UWORD smod __asm("d2"),
                                register struct RastPort *rp __asm("a1"),
                                register APTR ctab __asm("a2"),
                                register UWORD dx __asm("d3"),
                                register UWORD dy __asm("d4"),
                                register UWORD w __asm("d5"),
                                register UWORD h __asm("d6"),
                                register UBYTE ctfmt __asm("d7") )
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    rtgl_req_t r = {RTG_R_WRITELUT, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

    if (bm && src && ctab && ctfmt == CTABFMT_XRGB8 && cgx_bm_fmt(bm) != RGBFB_CLUT)
    {
        r.src = src;
        r.srcmod = smod;
        r.srcx = sx;
        r.srcy = sy;
        r.extra = (ULONG)ctab;
        rtgl_render_rp(&r, rp, dx, dy, w, h, FALSE, TRUE);
    }
    return (ULONG)w * h;
}

/* -120 ReadPixelArray */
ULONG _cgx_ReadPixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                            register APTR dst __asm("a0"),
                            register UWORD dx __asm("d0"),
                            register UWORD dy __asm("d1"),
                            register UWORD dmod __asm("d2"),
                            register struct RastPort *rp __asm("a1"),
                            register UWORD sx __asm("d3"),
                            register UWORD sy __asm("d4"),
                            register UWORD w __asm("d5"),
                            register UWORD h __asm("d6"),
                            register UBYTE fmt __asm("d7") )
{
    rtgl_req_t r = {RTG_R_READRGB, 0, 0, RTGM_ALPHA, 0, NULL, 0, 0, 0, 0, 0};

    /* reference: RECTFMT_LUT8 and RECTFMT_GREY8 are not read */
    if (cgx_rtg_bm(rp) && dst && fmt <= RECTFMT_ARGB)
    {
        r.src = dst;
        r.srcmod = dmod;
        r.srcx = dx;
        r.srcy = dy;
        r.fmt = RTG_FMT_CGX_BASE + fmt;
        rtgl_render_rp(&r, rp, sx, sy, w, h, TRUE, FALSE);
    }
    return (ULONG)w * h;
}

/* -132 MovePixelArray */
ULONG _cgx_MovePixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                            register UWORD sx __asm("d0"),
                            register UWORD sy __asm("d1"),
                            register struct RastPort *rp __asm("a1"),
                            register UWORD dx __asm("d2"),
                            register UWORD dy __asm("d3"),
                            register UWORD w __asm("d4"),
                            register UWORD h __asm("d5") )
{
    if (cgx_rtg_bm(rp) && w && h)
        ClipBlit(rp, (WORD)sx, (WORD)sy, rp, (WORD)dx, (WORD)dy, (WORD)w, (WORD)h, 0xc0);
    return (ULONG)w * h;
}

/* -90 ScalePixelArray: nearest neighbour */
LONG _cgx_ScalePixelArray ( register struct Library *CyberGfxBase __asm("a6"),
                            register APTR src __asm("a0"),
                            register UWORD sw __asm("d0"),
                            register UWORD sh __asm("d1"),
                            register UWORD smod __asm("d2"),
                            register struct RastPort *rp __asm("a1"),
                            register UWORD dx __asm("d3"),
                            register UWORD dy __asm("d4"),
                            register UWORD dw __asm("d5"),
                            register UWORD dh __asm("d6"),
                            register UBYTE fmt __asm("d7") )
{
    static const UBYTE bpps[] = {3, 4, 4, 1, 1};
    ULONG bpp, x, y, i;
    UBYTE *buf;

    if (!cgx_rtg_bm(rp) || !src || !sw || !sh || !dw || !dh || fmt > RECTFMT_GREY8)
        return (LONG)dw * dh;
    bpp = bpps[fmt];
    buf = (UBYTE *)AllocVec((ULONG)dw * dh * bpp, MEMF_ANY);
    if (!buf)
        return 0;
    for (y = 0; y < dh; y++)
    {
        const UBYTE *srow = (const UBYTE *)src + (y * sh / dh) * smod;
        UBYTE *drow = buf + y * dw * bpp;

        for (x = 0; x < dw; x++)
        {
            const UBYTE *s = srow + (x * sw / dw) * bpp;

            for (i = 0; i < bpp; i++)
                *drow++ = s[i];
        }
    }
    cgx_write_array(buf, 0, 0, (LONG)dw * bpp, rp, dx, dy, dw, dh, fmt);
    FreeVec(buf);
    return (LONG)dw * dh;
}

/* -186 ExtractColor: a mask of the pixels of one colour (CLUT: one pen)
 * into the first plane of a planar bitmap, from its top left corner */
ULONG _cgx_ExtractColor ( register struct Library *CyberGfxBase __asm("a6"),
                          register struct RastPort *rp __asm("a0"),
                          register struct BitMap *mask __asm("a1"),
                          register ULONG colour __asm("d0"),
                          register ULONG sx __asm("d1"),
                          register ULONG sy __asm("d2"),
                          register ULONG w __asm("d3"),
                          register ULONG h __asm("d4") )
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    rtgl_req_t r = {RTG_R_READRGB, 0, 0, RTGM_ALPHA, 0, NULL, 0, 0, 0, 0, 0};
    BOOL clut;
    ULONG x, y, bpp;
    UBYTE *buf;

    if (!bm || !mask || !mask->Planes[0] || !w || !h)
        return 0;
    clut = cgx_bm_fmt(bm) == RGBFB_CLUT;
    bpp = clut ? 1 : 4;
    buf = (UBYTE *)AllocVec(w * h * bpp, MEMF_ANY | MEMF_CLEAR);
    if (!buf)
        return 0;
    r.src = buf;
    r.srcmod = (LONG)(w * bpp);
    r.fmt = clut ? RTG_FMT_CGX_LUT8 : RTG_FMT_CGX_ARGB;
    rtgl_render_rp(&r, rp, (LONG)sx, (LONG)sy, (LONG)w, (LONG)h, TRUE, FALSE);
    for (y = 0; y < h && y < mask->Rows; y++)
    {
        UBYTE *row = (UBYTE *)mask->Planes[0] + y * mask->BytesPerRow;

        for (x = 0; x < w && (x >> 3) < mask->BytesPerRow; x++)
        {
            BOOL hit;
            UBYTE bit = (UBYTE)(0x80 >> (x & 7));

            if (clut)
                hit = buf[y * w + x] == (colour & 0xff);
            else
                hit = (*(ULONG *)(buf + (y * w + x) * 4) & 0x00ffffffUL) == (colour & 0x00ffffffUL);
            if (hit)
                row[x >> 3] |= bit;
            else
                row[x >> 3] &= (UBYTE)~bit;
        }
    }
    FreeVec(buf);
    return 0xff;    /* reference: TRUE as a byte */
}

/* -156 DoCDrawMethodTagList: the hook gets the whole bitmap, the layer's
 * position on it and the size of the layer */
void _cgx_DoCDrawMethodTagList ( register struct Library *CyberGfxBase __asm("a6"),
                                 register struct Hook *hook __asm("a0"),
                                 register struct RastPort *rp __asm("a1"),
                                 register struct TagItem *tags __asm("a2") )
{
    struct BitMap *bm = cgx_rtg_bm(rp);
    struct Layer *l;
    struct CDrawMsg msg;

    if (!hook || !bm)
        return;
    l = rp->Layer;
    if (l)
        LockLayerRom(l);
    msg.cdm_MemPtr = bm->Planes[0];
    if (l)
    {
        msg.cdm_offx = l->bounds.MinX;
        msg.cdm_offy = l->bounds.MinY;
        msg.cdm_xsize = l->bounds.MaxX - l->bounds.MinX + 1;
        msg.cdm_ysize = l->bounds.MaxY - l->bounds.MinY + 1;
    }
    else
    {
        msg.cdm_offx = 0;
        msg.cdm_offy = 0;
        msg.cdm_xsize = LXA_RTG_EXTL(bm, LXA_RTG_EXT_WIDTH);
        msg.cdm_ysize = bm->Rows;
    }
    msg.cdm_BytesPerRow = bm->BytesPerRow;
    msg.cdm_BytesPerPix = (UWORD)LXA_RTG_EXTL(bm, LXA_RTG_EXT_BPP);
    msg.cdm_ColorModel = (UWORD)cgx_pixfmt(cgx_bm_fmt(bm));
    CallHookPkt(hook, rp, &msg);
    if (l)
        UnlockLayerRom(l);
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

extern APTR              __g_lxa_cgx_FuncTab [];
extern struct MyDataInit __g_lxa_cgx_DataTab;
extern struct InitTable  __g_lxa_cgx_InitTab;
extern APTR              __g_lxa_cgx_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_cgx_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_LIBRARY,
    0,
    &_g_cgx_ExLibName[0],
    &_g_cgx_ExLibID[0],
    &__g_lxa_cgx_InitTab
};

APTR __g_lxa_cgx_EndResident;
struct Resident *__lxa_cgx_ROMTag = &ROMTag;

struct InitTable __g_lxa_cgx_InitTab =
{
    (ULONG)               sizeof(struct Library),
    (APTR              *) &__g_lxa_cgx_FuncTab[0],
    (APTR)                &__g_lxa_cgx_DataTab,
    (APTR)                __g_lxa_cgx_InitLib
};

/* 4 standard + 36 vectors: NegSize 240 (reference) */
APTR __g_lxa_cgx_FuncTab [] =
{
    __g_lxa_cgx_OpenLib,             // -6
    __g_lxa_cgx_CloseLib,            // -12
    __g_lxa_cgx_ExpungeLib,          // -18
    __g_lxa_cgx_ExtFuncLib,          // -24
    _cgx_Private,                    // -30 private
    _cgx_Private,                    // -36 private
    _cgx_Private,                    // -42 private
    _cgx_Private,                    // -48 private
    _cgx_IsCyberModeID,              // -54
    _cgx_BestCModeIDTagList,         // -60
    _cgx_CModeRequestTagList,        // -66
    _cgx_AllocCModeListTagList,      // -72
    _cgx_FreeCModeList,              // -78
    _cgx_Private,                    // -84 private
    _cgx_ScalePixelArray,            // -90
    _cgx_GetCyberMapAttr,            // -96
    _cgx_GetCyberIDAttr,             // -102
    _cgx_ReadRGBPixel,               // -108
    _cgx_WriteRGBPixel,              // -114
    _cgx_ReadPixelArray,             // -120
    _cgx_WritePixelArray,            // -126
    _cgx_MovePixelArray,             // -132
    _cgx_Private,                    // -138 private
    _cgx_InvertPixelArray,           // -144
    _cgx_FillPixelArray,             // -150
    _cgx_DoCDrawMethodTagList,       // -156
    _cgx_CVideoCtrlTagList,          // -162
    _cgx_LockBitMapTagList,          // -168
    _cgx_UnLockBitMap,               // -174
    _cgx_UnLockBitMapTagList,        // -180
    _cgx_ExtractColor,               // -186
    _cgx_Private,                    // -192 private
    _cgx_WriteLUTPixelArray,         // -198
    _cgx_Private,                    // -204 private
    _cgx_Private,                    // -210
    _cgx_Private,                    // -216
    _cgx_Private,                    // -222
    _cgx_Private,                    // -228
    _cgx_Private,                    // -234
    _cgx_Private,                    // -240
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_cgx_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_cgx_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_cgx_ExLibID[0],
    (ULONG) 0
};
