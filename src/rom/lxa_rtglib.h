/*
 * lxa_rtglib.h - shared m68k code of lxa's RTG disk libraries
 * (Picasso96API.library, cybergraphics.library).
 *
 * Both libraries are thin: they parse their arguments and let the host
 * (src/lxa/lxa_rtg.c, EMU_CALL_GFX_RTG) do the pixel work on the chunky
 * bitmaps graphics.library allocates (lxa_rtg.h).
 */
#ifndef LXA_RTGLIB_H
#define LXA_RTGLIB_H

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>

extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library *LayersBase;
extern struct Library *UtilityBase;

#define LXA_RTG_M68K
#include "lxa_rtg.h"

/* the libraries need the RTG board (lxa --rtg / LXA_RTG=1) */
BOOL rtgl_open_libs(void);
BOOL rtgl_enabled(void);

ULONG rtgl_call(ULONG op, ULONG a, ULONG b, ULONG c, ULONG d);

/* one RTG_OP_RENDER request (struct LxaRtgArgs without the target) */
typedef struct
{
    ULONG op;
    ULONG fg, bg, mode, cmap;
    APTR  src;
    LONG  srcmod, srcx, srcy;
    ULONG fmt, extra;
} rtgl_req_t;

ULONG rtgl_render_bm(const rtgl_req_t *r, struct BitMap *bm, LONG x, LONG y, LONG w, LONG h);

/*
 * Run a request on a RastPort: directly on an unlayered RTG bitmap, else
 * on a temporary RTG bitmap that is copied from (read_first) and/or to
 * (write_back) the RastPort with ClipBlit()/BltBitMapRastPort(), which
 * clips to the layer and its obscured parts.  Returns the host result.
 */
ULONG rtgl_render_rp(const rtgl_req_t *r, struct RastPort *rp, LONG x, LONG y, LONG w, LONG h,
                     BOOL read_first, BOOL write_back);

/* allocate a chunky bitmap like p96AllocBitMap(); friend may be NULL */
struct BitMap *rtgl_alloc_bitmap(ULONG w, ULONG h, ULONG depth, ULONG flags, struct BitMap *friend_bm,
                                 ULONG fmt);
void rtgl_free_bitmap(struct BitMap *bm);

/* simple tag walking (TAG_MORE/SKIP/IGNORE) */
struct TagItem *rtgl_next_tag(struct TagItem **state);

#endif
