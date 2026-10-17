/*
 * lxa_rtg.h - RTG (chunky, Picasso96-style) display support shared by the
 * ROM (graphics.library, intuition.library), the Picasso96API and
 * cybergraphics disk libraries and the host (src/lxa/lxa_rtg.c).
 *
 * RTG is enabled per run (LXA_RTG=1, `lxa --rtg`, lxa_config_t.rtg); it
 * models the `rtg` reference profile: Picasso96 2.0 with one uaegfx board.
 *
 * An RTG bitmap is a struct BitMap whose pad field is 0xf8c0 | RGBFTYPE
 * and whose Planes[0] points to the pixel memory (as on Picasso96).  lxa
 * keeps its own descriptor (struct LxaRtgExt) directly in front of the
 * pixel memory.  Every graphics primitive checks for RTG bitmaps and lets
 * the host render into the chunky memory (EMU_CALL_GFX_RTG).
 */
#ifndef LXA_RTG_H
#define LXA_RTG_H

#define LXA_RTG_PAD_MASK    0xfff0
#define LXA_RTG_PAD_TAG     0xf8c0
#define LXA_RTG_MAGIC       0x4c585254UL    /* 'LXRT' */

/* struct LxaRtgExt (in emulated memory, big-endian), in front of the
 * pixel memory: ext = Planes[0] - LXA_RTG_EXT_SIZE */
#define LXA_RTG_EXT_MAGIC   0    /* ULONG LXA_RTG_MAGIC */
#define LXA_RTG_EXT_WIDTH   4    /* ULONG pixels per row (BytesPerRow / bpp) */
#define LXA_RTG_EXT_HEIGHT  8    /* ULONG rows */
#define LXA_RTG_EXT_FORMAT  12   /* ULONG RGBFTYPE */
#define LXA_RTG_EXT_BPP     16   /* ULONG bytes per pixel */
#define LXA_RTG_EXT_CMAP    20   /* ULONG struct ColorMap * for pens (0: the RTG screen's) */
#define LXA_RTG_EXT_ALLOC   24   /* ULONG start of the allocation (FreeMem) */
#define LXA_RTG_EXT_SIZE_A  28   /* ULONG size of the allocation */
#define LXA_RTG_EXT_LOCK    32   /* ULONG p96LockBitMap() nesting */
#define LXA_RTG_EXT_FLAGS   36   /* ULONG LXA_RTGF_* */
#define LXA_RTG_EXT_DEPTH   40   /* ULONG BMA_DEPTH / P96BMA_DEPTH */
#define LXA_RTG_EXT_SIZE    48

#define LXA_RTGF_ONBOARD    0x0001  /* P96BMA_ISONBOARD */
#define LXA_RTGF_SCREEN     0x0002  /* a screen's bitmap */

/* EMU_CALL_GFX_RTG operations: d1 = op, d2..d5 = arguments */
#define RTG_OP_ENABLED      0   /* -> 1 if RTG is enabled */
#define RTG_OP_NEXT_ID      1   /* (id) -> next listed RTG mode ID (INVALID_ID: first) */
#define RTG_OP_AVAIL        2   /* (id) -> ~0 unknown, else DI_AVAIL_* flags */
#define RTG_OP_INFO         3   /* (id, tag, buf, size) -> bytes (GetDisplayInfoData) */
#define RTG_OP_MONITOR      4   /* (index, namebuf[32]) -> rows << 16 | clocks, 0: end */
#define RTG_OP_SET_MONITOR  5   /* (index, MonitorSpec *) */
#define RTG_OP_MONITOR_OF   6   /* (id) -> MonitorSpec * or 0 */
#define RTG_OP_BESTMODE     7   /* (tags) -> mode ID, INVALID_ID or RTG_BEST_CLASSIC */
#define RTG_OP_MODE_ATTR    8   /* (id, P96IDA_*) -> value */
#define RTG_OP_RENDER       9   /* (render op, struct LxaRtgArgs *) -> result */
#define RTG_OP_ENCODE       10  /* (RGBFTYPE, 0xAARRGGBB) -> p96EncodeColor() */
#define RTG_OP_SET_SCREEN_CM 11 /* (ColorMap *) the frontmost RTG screen's colours */
#define RTG_OP_BOARD_MEM    12  /* (0 total, 1 free, 2 largest free) */

#define RTG_BEST_CLASSIC    0xfffffffeUL    /* BestModeIDA(): use the native modes */

/* RTG_OP_RENDER operations */
#define RTG_R_FILL          0   /* rect x,y,w,h with pen fg (mode: draw mode, mask) */
#define RTG_R_PATTERN       1   /* rect with area pattern src (srcmod = rows, srcx/srcy = origin) */
#define RTG_R_TEMPLATE      2   /* 1-bit template src (srcmod bytes/row, srcx bit offset) */
#define RTG_R_WRITEPIXEL    3   /* one pixel with pen fg */
#define RTG_R_READPIXEL     4   /* -> pen at x,y */
#define RTG_R_WRITE8        5   /* pen array src (srcmod bytes/row) to rect */
#define RTG_R_READ8         6   /* rect to pen array src (srcmod bytes/row) */
#define RTG_R_FILLRGB       7   /* rect with colour fg (0xAARRGGBB) */
#define RTG_R_WRITERGB      8   /* pixel array src (fmt, srcmod, srcx, srcy) to rect */
#define RTG_R_READRGB       9   /* rect to pixel array src (fmt, srcmod, srcx, srcy) */
#define RTG_R_READPIXELRGB  10  /* -> 0x00RRGGBB at x,y */
#define RTG_R_WRITEPIXELRGB 11  /* pixel x,y = colour fg */
#define RTG_R_INVERT        12  /* rect: invert all bits */
#define RTG_R_WRITELUT      13  /* pen array src via colour table extra (ULONG 0x00RRGGBB) */
#define RTG_R_WRITETC       14  /* TrueColorInfo extra to rect (srcx/srcy origin) */
#define RTG_R_READTC        15  /* rect to TrueColorInfo extra */
#define RTG_R_ALPHA         16  /* ARGB array src blended onto rect (CGX WritePixelArrayAlpha) */

/* struct LxaRtgArgs (emulated memory, big-endian) */
#define RTGA_BM      0      /* ULONG struct BitMap * */
#define RTGA_X       4      /* LONG */
#define RTGA_Y       8      /* LONG */
#define RTGA_W       12     /* LONG */
#define RTGA_H       16     /* LONG */
#define RTGA_FG      20     /* ULONG pen or colour */
#define RTGA_BG      24     /* ULONG pen */
#define RTGA_MODE    28     /* ULONG drawmode | mask << 8 */
#define RTGA_CMAP    32     /* ULONG struct ColorMap * (0: the bitmap's) */
#define RTGA_SRC     36     /* ULONG source / destination memory */
#define RTGA_SRCMOD  40     /* LONG bytes per row */
#define RTGA_SRCX    44     /* LONG */
#define RTGA_SRCY    48     /* LONG */
#define RTGA_FMT     52     /* ULONG RGBFTYPE or RECTFMT */
#define RTGA_EXTRA   56     /* ULONG */
#define RTGA_SIZE    60

/* RTGA_MODE extra flags */
#define RTGM_PIXEL   0x10000    /* WritePixel COMPLEMENT: only the colour bytes */
#define RTGM_ARRAYMASK 0x20000  /* READ8: mask pens with the bitmap depth */

/* RTGA_FMT for WRITERGB/READRGB: an RGBFTYPE, or one of these (CGX RECTFMT) */
#define RTG_FMT_CGX_BASE    0x100
#define RTG_FMT_CGX_RGB     0x100   /* RECTFMT_RGB   3 bytes R,G,B */
#define RTG_FMT_CGX_RGBA    0x101   /* RECTFMT_RGBA  4 bytes R,G,B,A */
#define RTG_FMT_CGX_ARGB    0x102   /* RECTFMT_ARGB  4 bytes A,R,G,B */
#define RTG_FMT_CGX_LUT8    0x103   /* RECTFMT_LUT8  pens */
#define RTG_FMT_CGX_GREY8   0x104   /* RECTFMT_GREY8 grey levels */

#endif /* LXA_RTG_H */
