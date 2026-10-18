/*
 * lxa_rtglib.c - shared m68k code of lxa's RTG disk libraries
 * (Picasso96API.library, cybergraphics.library); see lxa_rtglib.h.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>
#include <utility/tagitem.h>

#include "util.h"
#include "lxa_rtglib.h"

struct ExecBase *SysBase = NULL;
struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Library *LayersBase = NULL;
struct Library *UtilityBase = NULL;

ULONG emucall5(ULONG func, ULONG param1, ULONG param2, ULONG param3, ULONG param4, ULONG param5)
{
    ULONG res;
    asm volatile( "move.l    %1, d0\n\t"
         "move.l    %2, d1\n\t"
         "move.l    %3, d2\n\t"
         "move.l    %4, d3\n\t"
         "move.l    %5, d4\n\t"
         "move.l    %6, d5\n\t"
         "illegal\n\t"
         "move.l    d0, %0\n"
        : "=r" (res)
        : "r" (func), "r" (param1), "r" (param2), "r" (param3), "r" (param4), "r" (param5)
        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5"   /* the host reads and writes memory */
        );
    return res;
}

ULONG rtgl_call(ULONG op, ULONG a, ULONG b, ULONG c, ULONG d)
{
    return emucall5(EMU_CALL_GFX_RTG, op, a, b, c, d);
}

BOOL rtgl_enabled(void)
{
    return rtgl_call(RTG_OP_ENABLED, 0, 0, 0, 0) != 0;
}

BOOL rtgl_open_libs(void)
{
    if (!GfxBase)
        GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase)
        IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    if (!LayersBase)
        LayersBase = OpenLibrary((CONST_STRPTR)"layers.library", 39);
    if (!UtilityBase)
        UtilityBase = OpenLibrary((CONST_STRPTR)"utility.library", 39);
    return GfxBase && IntuitionBase && LayersBase && UtilityBase;
}

ULONG rtgl_render_bm(const rtgl_req_t *r, struct BitMap *bm, LONG x, LONG y, LONG w, LONG h)
{
    ULONG a[RTGA_SIZE / 4];

    a[RTGA_BM / 4] = (ULONG)bm;
    a[RTGA_X / 4] = (ULONG)x;
    a[RTGA_Y / 4] = (ULONG)y;
    a[RTGA_W / 4] = (ULONG)w;
    a[RTGA_H / 4] = (ULONG)h;
    a[RTGA_FG / 4] = r->fg;
    a[RTGA_BG / 4] = r->bg;
    a[RTGA_MODE / 4] = r->mode;
    a[RTGA_CMAP / 4] = r->cmap;
    a[RTGA_SRC / 4] = (ULONG)r->src;
    a[RTGA_SRCMOD / 4] = (ULONG)r->srcmod;
    a[RTGA_SRCX / 4] = (ULONG)r->srcx;
    a[RTGA_SRCY / 4] = (ULONG)r->srcy;
    a[RTGA_FMT / 4] = r->fmt;
    a[RTGA_EXTRA / 4] = r->extra;
    return rtgl_call(RTG_OP_RENDER, r->op, (ULONG)a, 0, 0);
}

struct BitMap *rtgl_alloc_bitmap(ULONG w, ULONG h, ULONG depth, ULONG flags, struct BitMap *friend_bm,
                                 ULONG fmt)
{
    ULONG rflags = 0, cmap = 0;
    struct BitMap *bm;

    if (friend_bm && lxa_rtg_is(friend_bm))
    {
        if (fmt == 0)
            fmt = LXA_RTG_EXTL(friend_bm, LXA_RTG_EXT_FORMAT);
        cmap = LXA_RTG_EXTL(friend_bm, LXA_RTG_EXT_CMAP);
        rflags = LXA_RTGF_ONBOARD;
    }
    if (fmt == 0)
        fmt = 1;    /* RGBFB_NONE without a friend: CLUT (reference) */
    if (flags & 0x8000)     /* BMF_USERPRIVATE: never on the board */
        rflags = 0;
    bm = lxa_rtg_alloc_bitmap(w, h, depth, fmt, rflags);
    if (bm)
    {
        LXA_RTG_EXTL(bm, LXA_RTG_EXT_CMAP) = cmap;
        /* BMF_CLEAR: a true colour bitmap without a screen's colours is
         * cleared to the board's pen 0, 0xAAAAAA (reference: probes
         * rtg/p96pixels and rtg/cgx) */
        if ((flags & BMF_CLEAR) && fmt != 1 && !cmap)
        {
            rtgl_req_t r = {RTG_R_FILLRGB, 0x00aaaaaa, 0, 0, 0, NULL, 0, 0, 0, 0, 0};

            rtgl_render_bm(&r, bm, 0, 0, (LONG)w, (LONG)h);
        }
    }
    return bm;
}

void rtgl_free_bitmap(struct BitMap *bm)
{
    if (!bm)
        return;
    if (lxa_rtg_is(bm))
        lxa_rtg_free_bitmap(bm);
    else
        FreeBitMap(bm);
}

ULONG rtgl_render_rp(const rtgl_req_t *r, struct RastPort *rp, LONG x, LONG y, LONG w, LONG h,
                     BOOL read_first, BOOL write_back)
{
    struct BitMap *bm, *tmp;
    struct RastPort trp;
    ULONG res;

    if (!rp || !(bm = rp->BitMap) || w <= 0 || h <= 0)
        return 0;
    if (!rp->Layer && lxa_rtg_is(bm))
        return rtgl_render_bm(r, bm, x, y, w, h);

    /* layered or planar: go through a temporary chunky bitmap */
    tmp = rtgl_alloc_bitmap((ULONG)w, (ULONG)h, 8, 0, lxa_rtg_is(bm) ? bm : NULL, 0);
    if (!tmp)
        return 0;
    InitRastPort(&trp);
    trp.BitMap = tmp;
    if (read_first)
    {
        if (rp->Layer)
            ClipBlit(rp, (WORD)x, (WORD)y, &trp, 0, 0, (WORD)w, (WORD)h, 0xc0);
        else
            BltBitMap(bm, (WORD)x, (WORD)y, tmp, 0, 0, (WORD)w, (WORD)h, 0xc0, 0xff, NULL);
    }
    res = rtgl_render_bm(r, tmp, 0, 0, w, h);
    if (write_back)
    {
        if (rp->Layer)
            BltBitMapRastPort(tmp, 0, 0, rp, (WORD)x, (WORD)y, (WORD)w, (WORD)h, 0xc0);
        else
            BltBitMap(tmp, 0, 0, bm, (WORD)x, (WORD)y, (WORD)w, (WORD)h, 0xc0, 0xff, NULL);
    }
    lxa_rtg_free_bitmap(tmp);
    return res;
}

struct TagItem *rtgl_next_tag(struct TagItem **state)
{
    struct TagItem *t = *state;

    while (t)
    {
        switch (t->ti_Tag)
        {
            case TAG_DONE:
                *state = NULL;
                return NULL;
            case TAG_IGNORE:
                t++;
                break;
            case TAG_MORE:
                t = (struct TagItem *)t->ti_Data;
                break;
            case TAG_SKIP:
                t += t->ti_Data + 1;
                break;
            default:
                *state = t + 1;
                return t;
        }
    }
    *state = NULL;
    return NULL;
}
