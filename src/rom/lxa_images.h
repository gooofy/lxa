/*
 * lxa_images.h - built-in BOOPSI image classes and shared frame/sysimage
 * drawing (Phase 223).  See lxa_images.c.
 */
#ifndef LXA_IMAGES_H
#define LXA_IMAGES_H

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/screens.h>
#include <graphics/rastport.h>
#include <graphics/text.h>

/* SYSIA_Size flag of Intuition's own gadgets: the old-look images */
#ifndef LXA_SYSI_OLDLOOK
#define LXA_SYSI_OLDLOOK 0x10
#endif

const UWORD *lxa_default_pens(void);

BOOL lxa_sysi_dims(UWORD which, UWORD size, UWORD *width, UWORD *height);
BOOL lxa_sysi_stretches(UWORD which, UWORD size);
BOOL lxa_sysi_draw_h(struct RastPort *rp, UWORD which, UWORD size, WORD x, WORD y, WORD h,
                     ULONG state, const UWORD *pens);
BOOL lxa_sysi_draw(struct RastPort *rp, UWORD which, UWORD size, WORD x, WORD y,
                   ULONG state, const UWORD *pens);
BOOL lxa_sysi_menu_size(UWORD which, const struct TextFont *tf, UWORD *width, UWORD *height);
BOOL lxa_sysi_menu_draw(struct RastPort *rp, UWORD which, UWORD width, UWORD height,
                        WORD x, WORD y, const UWORD *pens);

void lxa_draw_frame(struct RastPort *rp, ULONG type, BOOL recessed, WORD x, WORD y,
                    WORD w, WORD h, ULONG state, BOOL edges_only, const UWORD *pens);
WORD lxa_text_fit(struct RastPort *rp, CONST_STRPTR s, WORD len, WORD width);
void lxa_ghost_rect(struct RastPort *rp, WORD x0, WORD y0, WORD x1, WORD y1, const UWORD *pens);
void lxa_ghost_rect_pen(struct RastPort *rp, WORD x0, WORD y0, WORD x1, WORD y1, UWORD pen, WORD phase_y);

ULONG lxa_imageclass_dispatch(register struct IClass *cl __asm("a0"),
                              register Object *obj __asm("a2"),
                              register Msg msg __asm("a1"));
ULONG lxa_sysiclass_dispatch(register struct IClass *cl __asm("a0"),
                             register Object *obj __asm("a2"),
                             register Msg msg __asm("a1"));
ULONG lxa_frameiclass_dispatch(register struct IClass *cl __asm("a0"),
                               register Object *obj __asm("a2"),
                               register Msg msg __asm("a1"));
extern const ULONG lxa_sysiclass_instsize;
extern const ULONG lxa_frameiclass_instsize;
ULONG lxa_fillrectclass_dispatch(register struct IClass *cl __asm("a0"),
                                 register Object *obj __asm("a2"),
                                 register Msg msg __asm("a1"));
extern const ULONG lxa_fillrectclass_instsize;
ULONG lxa_itexticlass_dispatch(register struct IClass *cl __asm("a0"),
                               register Object *obj __asm("a2"),
                               register Msg msg __asm("a1"));
ULONG lxa_pointerclass_dispatch(register struct IClass *cl __asm("a0"),
                                register Object *obj __asm("a2"),
                                register Msg msg __asm("a1"));
extern const ULONG lxa_pointerclass_instsize;

#endif
