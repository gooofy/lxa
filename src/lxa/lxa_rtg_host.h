/*
 * lxa_rtg.h - host side of lxa's RTG (Picasso96-style) display support.
 */
#ifndef LXA_RTG_HOST_H
#define LXA_RTG_HOST_H

#include <stdint.h>
#include <stdbool.h>

/* RTG on/off for this run: lxa_config_t.rtg, `lxa --rtg`, LXA_RTG=1 */
void rtg_set_enabled(bool enabled);
bool rtg_enabled(void);
/* forget per-run state (monitor pointers, screen colours) */
void rtg_reset(void);

/* EMU_CALL_GFX_RTG: d1..d5 -> d0 */
uint32_t rtg_emucall(uint32_t op, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

/* An RTG bitmap (emulated address of its struct BitMap) */
typedef struct
{
    uint32_t bm;
    uint32_t memory;    /* pixel memory */
    uint32_t bpr;
    uint32_t width;     /* pixels per row */
    uint32_t height;
    uint32_t format;    /* RGBFTYPE */
    uint32_t bpp;
    uint32_t cmap;      /* struct ColorMap * */
    uint32_t depth;
} rtg_bitmap_t;

bool rtg_bitmap_get(uint32_t bm, rtg_bitmap_t *out);

/* BltBitMap() with an RTG source or destination: true if handled */
bool rtg_blt_bitmap(uint32_t src_bm, int sx, int sy, uint32_t dst_bm, int dx, int dy,
                    int w, int h, uint8_t minterm, uint8_t mask, uint32_t pmask,
                    uint32_t pmask_bpr, int mask_mode, uint32_t *planes_affected);

/* Convert a row of an RTG bitmap for the host display:
 * pens (CLUT, 1 byte per pixel) or 0x00RRGGBB (true colour) */
void rtg_read_row_pens(const rtg_bitmap_t *b, int y, int x0, int count, uint8_t *dst);
void rtg_read_row_rgb(const rtg_bitmap_t *b, int y, int x0, int count, uint32_t *dst);

#endif
