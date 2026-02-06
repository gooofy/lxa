#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <graphics/sprite.h>
#include <graphics/view.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <graphics/regions.h>
#include <graphics/clip.h>
#include <graphics/layers.h>
#include <graphics/gels.h>
#include <graphics/scale.h>
#include <hardware/blit.h>

#include <intuition/intuitionbase.h>

#include "util.h"
#include "topaz8_font.h"

/* Forward declaration for input processing (defined in lxa_intuition.c) */
extern VOID _intuition_ProcessInputEvents(struct Screen *screen);

/* Global IntuitionBase (defined in exec.c) - used to avoid OpenLibrary from interrupt context */
extern struct IntuitionBase *IntuitionBase;

/* Forward declarations for Topaz font (defined later in this file) */
static struct TextFont g_topaz8_font;
static void init_topaz8_font(void);

/* Forward declarations for graphics functions */
static VOID _graphics_RectFill ( register struct GfxBase * GfxBase __asm("a6"),
                                 register struct RastPort * rp __asm("a1"),
                                 register WORD xMin __asm("d0"),
                                 register WORD yMin __asm("d1"),
                                 register WORD xMax __asm("d2"),
                                 register WORD yMax __asm("d3"));
static PLANEPTR _graphics_AllocRaster ( register struct GfxBase * GfxBase __asm("a6"),
                                        register UWORD width __asm("d0"),
                                        register UWORD height __asm("d1"));
static VOID _graphics_FreeRaster ( register struct GfxBase * GfxBase __asm("a6"),
                                   register PLANEPTR p __asm("a0"),
                                   register UWORD width __asm("d0"),
                                   register UWORD height __asm("d1"));
static VOID _graphics_ClearEOL ( register struct GfxBase * GfxBase __asm("a6"),
                                 register struct RastPort * rp __asm("a1"));
static LONG _graphics_WritePixel ( register struct GfxBase * GfxBase __asm("a6"),
                                   register struct RastPort * rp __asm("a1"),
                                   register WORD x __asm("d0"),
                                   register WORD y __asm("d1"));
static ULONG _graphics_ReadPixel ( register struct GfxBase * GfxBase __asm("a6"),
                                   register struct RastPort * rp __asm("a1"),
                                   register WORD x __asm("d0"),
                                   register WORD y __asm("d1"));
static LONG _graphics_AreaMove ( register struct GfxBase * GfxBase __asm("a6"),
                                 register struct RastPort * rp __asm("a1"),
                                 register LONG x __asm("d0"),
                                 register LONG y __asm("d1"));
static LONG _graphics_AreaDraw ( register struct GfxBase * GfxBase __asm("a6"),
                                 register struct RastPort * rp __asm("a1"),
                                 register LONG x __asm("d0"),
                                 register LONG y __asm("d1"));
static VOID _graphics_Draw ( register struct GfxBase * GfxBase __asm("a6"),
                             register struct RastPort * rp __asm("a1"),
                             register WORD x __asm("d0"),
                             register WORD y __asm("d1"));
static VOID _graphics_Move ( register struct GfxBase * GfxBase __asm("a6"),
                             register struct RastPort * rp __asm("a1"),
                             register WORD x __asm("d0"),
                             register WORD y __asm("d1"));
static LONG _graphics_ReadPixelArray8 ( register struct GfxBase * GfxBase __asm("a6"),
                                        register struct RastPort * rp __asm("a0"),
                                        register ULONG xstart __asm("d0"),
                                        register ULONG ystart __asm("d1"),
                                        register ULONG xstop __asm("d2"),
                                        register ULONG ystop __asm("d3"),
                                        register UBYTE * array __asm("a2"),
                                        register struct RastPort * temprp __asm("a1"));
static LONG _graphics_WritePixelArray8 ( register struct GfxBase * GfxBase __asm("a6"),
                                         register struct RastPort * rp __asm("a0"),
                                         register ULONG xstart __asm("d0"),
                                         register ULONG ystart __asm("d1"),
                                         register ULONG xstop __asm("d2"),
                                         register ULONG ystop __asm("d3"),
                                         register UBYTE * array __asm("a2"),
                                         register struct RastPort * temprp __asm("a1"));
static ULONG _graphics_GetAPen ( register struct GfxBase * GfxBase __asm("a6"),
                                 register struct RastPort * rp __asm("a0"));
static ULONG _graphics_GetOutlinePen ( register struct GfxBase * GfxBase __asm("a6"),
                                       register struct RastPort * rp __asm("a0"));
static VOID _graphics_SetAPen ( register struct GfxBase * GfxBase __asm("a6"),
                                register struct RastPort * rp __asm("a1"),
                                register UBYTE pen __asm("d0"));
static VOID _graphics_SetDrMd ( register struct GfxBase * GfxBase __asm("a6"),
                                register struct RastPort * rp __asm("a1"),
                                register UBYTE drawMode __asm("d0"));
static UWORD _graphics_ScalerDiv ( register struct GfxBase * GfxBase __asm("a6"),
                                   register ULONG factor __asm("d0"),
                                   register ULONG numerator __asm("d1"),
                                   register ULONG denominator __asm("d2"));
static VOID _graphics_EraseRect ( register struct GfxBase * GfxBase __asm("a6"),
                                  register struct RastPort * rp __asm("a1"),
                                  register LONG xMin __asm("d0"),
                                  register LONG yMin __asm("d1"),
                                  register LONG xMax __asm("d2"),
                                  register LONG yMax __asm("d3"));

/* Drawing modes from rastport.h */
#ifndef JAM1
#define JAM1        0
#define JAM2        1
#define COMPLEMENT  2
#define INVERSVID   4
#endif

/* RastPort flags */
#ifndef FRST_DOT
#define FRST_DOT    0x01
#define ONE_DOT     0x02
#define DBUFFER     0x04
#define AREAOUTLINE 0x08
#define NOCROSSFILL 0x20
#endif

/* BitMap flags */
#ifndef BMF_STANDARD
#define BMF_STANDARD (1L << 3)   /* BMB_STANDARD = 3 */
#endif

/* Macros for flood fill tmpras access */
#define WIDTH_TO_BYTES(w) ((((w) + 15) >> 4) << 1)
#define COORD_TO_BYTEIDX(x, y, bpr) ((y) * (bpr) + ((x) >> 3))
#define XCOORD_TO_MASK(x) (0x80 >> ((x) & 7))

/* Use lxa_memset instead of memset to avoid conflict with libnix string.h */
static void lxa_memset(void *s, int c, ULONG n)
{
    UBYTE *p = (UBYTE *)s;
    while (n--)
        *p++ = (UBYTE)c;
}

/*
 * Helper: Calculate intersection of two rectangles (for clipping)
 * Returns TRUE if intersection exists, FALSE otherwise
 */
static BOOL ClipIntersectRects(WORD minX1, WORD minY1, WORD maxX1, WORD maxY1,
                                WORD minX2, WORD minY2, WORD maxX2, WORD maxY2,
                                WORD *outMinX, WORD *outMinY, WORD *outMaxX, WORD *outMaxY)
{
    *outMinX = (minX1 > minX2) ? minX1 : minX2;
    *outMinY = (minY1 > minY2) ? minY1 : minY2;
    *outMaxX = (maxX1 < maxX2) ? maxX1 : maxX2;
    *outMaxY = (maxY1 < maxY2) ? maxY1 : maxY2;

    return (*outMinX <= *outMaxX && *outMinY <= *outMaxY);
}

/*
 * Helper: Set a single pixel in a bitmap (no layer handling)
 * Used by layer-aware drawing functions to set individual pixels
 */
static void SetPixelDirect(struct BitMap *bm, WORD x, WORD y, BYTE pen, UBYTE drawmode)
{
    UWORD byteOffset;
    UBYTE bitMask;
    UBYTE plane;
    UBYTE basemode = drawmode & ~INVERSVID;

    if (x < 0 || y < 0 || x >= (bm->BytesPerRow * 8) || y >= bm->Rows)
        return;

    byteOffset = y * bm->BytesPerRow + (x >> 3);
    bitMask = 0x80 >> (x & 7);

    for (plane = 0; plane < bm->Depth; plane++)
    {
        if (bm->Planes[plane])
        {
            if (basemode == COMPLEMENT)
            {
                if (pen & (1 << plane))
                {
                    bm->Planes[plane][byteOffset] ^= bitMask;
                }
            }
            else
            {
                if (pen & (1 << plane))
                {
                    bm->Planes[plane][byteOffset] |= bitMask;
                }
                else
                {
                    bm->Planes[plane][byteOffset] &= ~bitMask;
                }
            }
        }
    }
}

/*
 * Helper: Fill a rectangle directly in a bitmap (no layer handling)
 * Coordinates are already in absolute screen space, clipped to bitmap bounds
 */
static void FillRectDirect(struct BitMap *bm, WORD xMin, WORD yMin, WORD xMax, WORD yMax, 
                           BYTE pen, UBYTE drawmode)
{
    WORD x, y;
    WORD bmMaxX = bm->BytesPerRow * 8;
    WORD bmMaxY = bm->Rows;

    DPRINTF(LOG_DEBUG, "_graphics: FillRectDirect() bm=%08lx bpr=%d rows=%d depth=%d\n",
            (ULONG)bm, bm->BytesPerRow, bm->Rows, bm->Depth);
    DPRINTF(LOG_DEBUG, "_graphics: FillRectDirect() rect (%d,%d)-(%d,%d) pen=%d dm=%d\n",
            xMin, yMin, xMax, yMax, pen, drawmode);
    DPRINTF(LOG_DEBUG, "_graphics: FillRectDirect() planes[0]=%08lx planes[1]=%08lx\n",
            (ULONG)bm->Planes[0], (ULONG)bm->Planes[1]);

    /* Clamp to bitmap bounds */
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax >= bmMaxX) xMax = bmMaxX - 1;
    if (yMax >= bmMaxY) yMax = bmMaxY - 1;

    if (xMin > xMax || yMin > yMax)
        return;

    /* Fill the rectangle */
    for (y = yMin; y <= yMax; y++)
    {
        for (x = xMin; x <= xMax; x++)
        {
            UWORD byteOffset = y * bm->BytesPerRow + (x >> 3);
            UBYTE bitMask = 0x80 >> (x & 7);
            UBYTE plane;

            for (plane = 0; plane < bm->Depth; plane++)
            {
                if (bm->Planes[plane])
                {
                    if (drawmode == COMPLEMENT)
                    {
                        if (pen & (1 << plane))
                        {
                            bm->Planes[plane][byteOffset] ^= bitMask;
                        }
                    }
                    else
                    {
                        if (pen & (1 << plane))
                        {
                            bm->Planes[plane][byteOffset] |= bitMask;
                        }
                        else
                        {
                            bm->Planes[plane][byteOffset] &= ~bitMask;
                        }
                    }
                }
            }
        }
    }
}

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "graphics"
#define EXLIBVER   " 40.1 (2022/03/20)"

char __aligned _g_graphics_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_graphics_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_graphics_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_graphics_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

// libBase: GfxBase
// baseType: struct GfxBase *
// libname: graphics.library

struct GfxBase * __g_lxa_graphics_InitLib    ( register struct GfxBase *graphicsb    __asm("d0"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_graphics: InitLib() initializing GfxBase\n");

    /* Initialize the built-in Topaz-8 font */
    init_topaz8_font();

    /* Set the default font for the system */
    graphicsb->DefaultFont = &g_topaz8_font;

    DPRINTF (LOG_DEBUG, "_graphics: InitLib() DefaultFont set to 0x%08lx (g_topaz8_font at 0x%08lx)\n", 
             (ULONG)graphicsb->DefaultFont, (ULONG)&g_topaz8_font);

    return graphicsb;
}

struct GfxBase * __g_lxa_graphics_OpenLib ( register struct GfxBase  *GfxBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_graphics: OpenLib() called\n");
    // FIXME GfxBase->dl_lib.lib_OpenCnt++;
    // FIXME GfxBase->dl_lib.lib_Flags &= ~LIBF_DELEXP;
    return GfxBase;
}

BPTR __g_lxa_graphics_CloseLib ( register struct GfxBase  *graphicsb __asm("a6"))
{
    return NULL;
}

BPTR __g_lxa_graphics_ExpungeLib ( register struct GfxBase  *graphicsb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_graphics_ExtFuncLib(void)
{
    return NULL;
}

static LONG _graphics_BltBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct BitMap * srcBitMap __asm("a0"),
                                                        register WORD xSrc __asm("d0"),
                                                        register WORD ySrc __asm("d1"),
                                                        register struct BitMap * destBitMap __asm("a1"),
                                                        register WORD xDest __asm("d2"),
                                                        register WORD yDest __asm("d3"),
                                                        register WORD xSize __asm("d4"),
                                                        register WORD ySize __asm("d5"),
                                                        register UBYTE minterm __asm("d6"),
                                                        register UBYTE mask __asm("d7"),
                                                        register PLANEPTR tempA __asm("a2"))
{
    LONG row, col, plane;
    LONG srcBytesPerRow, destBytesPerRow;
    LONG srcDepth, destDepth, depth;
    LONG srcWidth, srcHeight, destWidth, destHeight;
    LONG actualWidth, actualHeight;
    LONG lxSrc = xSrc, lySrc = ySrc, lxDest = xDest, lyDest = yDest;
    LONG lxSize = xSize, lySize = ySize;

    DPRINTF (LOG_DEBUG, "_graphics: BltBitMap() src=0x%08lx (%ld,%ld) dest=0x%08lx (%ld,%ld) size=%ldx%ld minterm=0x%02x mask=0x%02x\n",
             (ULONG)srcBitMap, lxSrc, lySrc, (ULONG)destBitMap, lxDest, lyDest, lxSize, lySize, (unsigned)minterm, (unsigned)mask);

    if (!srcBitMap || !destBitMap)
    {
        LPRINTF (LOG_ERROR, "_graphics: BltBitMap() NULL bitmap\n");
        return 0;
    }

    if (lxSize <= 0 || lySize <= 0)
    {
        return 0;
    }

    /* Get bitmap dimensions */
    srcBytesPerRow = srcBitMap->BytesPerRow;
    destBytesPerRow = destBitMap->BytesPerRow;
    srcDepth = srcBitMap->Depth;
    destDepth = destBitMap->Depth;
    srcWidth = srcBytesPerRow * 8;
    srcHeight = srcBitMap->Rows;
    destWidth = destBytesPerRow * 8;
    destHeight = destBitMap->Rows;

    /* Use minimum depth */
    depth = (srcDepth < destDepth) ? srcDepth : destDepth;

    /* Clip to source and destination bounds */
    actualWidth = lxSize;
    actualHeight = lySize;

    if (lxSrc < 0) { lxDest -= lxSrc; actualWidth += lxSrc; lxSrc = 0; }
    if (lySrc < 0) { lyDest -= lySrc; actualHeight += lySrc; lySrc = 0; }
    if (lxDest < 0) { lxSrc -= lxDest; actualWidth += lxDest; lxDest = 0; }
    if (lyDest < 0) { lySrc -= lyDest; actualHeight += lyDest; lyDest = 0; }

    if (lxSrc + actualWidth > srcWidth) actualWidth = srcWidth - lxSrc;
    if (lySrc + actualHeight > srcHeight) actualHeight = srcHeight - lySrc;
    if (lxDest + actualWidth > destWidth) actualWidth = destWidth - lxDest;
    if (lyDest + actualHeight > destHeight) actualHeight = destHeight - lyDest;

    if (actualWidth <= 0 || actualHeight <= 0)
    {
        return 0;
    }

    /* Process each plane */
    for (plane = 0; plane < depth; plane++)
    {
        UBYTE *srcPlane = srcBitMap->Planes[plane];
        UBYTE *destPlane = destBitMap->Planes[plane];

        /* Skip if plane is masked out */
        if (!(mask & (1 << plane)))
            continue;

        if (!srcPlane || !destPlane)
            continue;

        /* Process each row */
        for (row = 0; row < actualHeight; row++)
        {
            LONG srcRowStart = (lySrc + row) * srcBytesPerRow;
            LONG destRowStart = (lyDest + row) * destBytesPerRow;

            /* Process each pixel in the row */
            for (col = 0; col < actualWidth; col++)
            {
                LONG srcPixelX = lxSrc + col;
                LONG destPixelX = lxDest + col;
                LONG srcByteOffset = srcRowStart + (srcPixelX / 8);
                LONG destByteOffset = destRowStart + (destPixelX / 8);
                UBYTE srcBitMask = (UBYTE)(0x80 >> (srcPixelX % 8));
                UBYTE destBitMask = (UBYTE)(0x80 >> (destPixelX % 8));
                UBYTE srcBit = (srcPlane[srcByteOffset] & srcBitMask) ? 1 : 0;
                UBYTE destBit = (destPlane[destByteOffset] & destBitMask) ? 1 : 0;
                UBYTE resultBit;

                /* Apply minterm logic
                 * Minterm is an 8-bit value that encodes all possible logic operations.
                 * The Amiga blitter uses ABC notation where:
                 *   A = Source bit
                 *   B = Destination bit  
                 *   C = Pattern bit (we assume C=1 since no pattern is provided)
                 * 
                 * Index into minterm:
                 *   0 = !A & !B & !C
                 *   1 = !A & !B & C
                 *   2 = !A & B & !C
                 *   3 = !A & B & C
                 *   4 = A & !B & !C
                 *   5 = A & !B & C
                 *   6 = A & B & !C
                 *   7 = A & B & C
                 *
                 * Common minterms:
                 *   0xC0 = COPY: ABC + ABc = A (copy source)
                 *   0xF0 = ALL: set all bits where src=1
                 *   0x30 = INVERT: aBc (invert source)
                 */
                {
                    /* Calculate index: A*4 + B*2 + C (C=1 assumed) */
                    UBYTE idx = (srcBit << 2) | (destBit << 1) | 1;
                    resultBit = (minterm >> idx) & 1;
                }

                /* Write result */
                if (resultBit)
                    destPlane[destByteOffset] |= destBitMask;
                else
                    destPlane[destByteOffset] &= ~destBitMask;
            }
        }
    }

    /* Return number of planes actually affected */
    return depth;
}

static VOID _graphics_BltTemplate ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST PLANEPTR source __asm("a0"),
                                                        register LONG xSrc __asm("d0"),
                                                        register LONG srcMod __asm("d1"),
                                                        register struct RastPort * destRP __asm("a1"),
                                                        register LONG xDest __asm("d2"),
                                                        register LONG yDest __asm("d3"),
                                                        register LONG xSize __asm("d4"),
                                                        register LONG ySize __asm("d5"))
{
    struct BitMap *bm;
    LONG row, col, plane;
    UBYTE fgpen, bgpen, drawmode;

    DPRINTF (LOG_DEBUG, "_graphics: BltTemplate() source=0x%08lx xSrc=%ld srcMod=%ld dest=(%ld,%ld) size=%ldx%ld\n",
             (ULONG)source, xSrc, srcMod, xDest, yDest, xSize, ySize);

    if (!source || !destRP)
    {
        LPRINTF (LOG_ERROR, "_graphics: BltTemplate() NULL parameter\n");
        return;
    }

    bm = destRP->BitMap;
    if (!bm)
    {
        LPRINTF (LOG_ERROR, "_graphics: BltTemplate() RastPort has no BitMap\n");
        return;
    }

    if (xSize <= 0 || ySize <= 0)
    {
        return;
    }

    fgpen = (UBYTE)destRP->FgPen;
    bgpen = (UBYTE)destRP->BgPen;
    drawmode = destRP->DrawMode;

    /* Handle INVERSVID - swap foreground and background colors */
    if (drawmode & INVERSVID)
    {
        UBYTE tmp = fgpen;
        fgpen = bgpen;
        bgpen = tmp;
    }

    /* Mask out INVERSVID to get base draw mode */
    UBYTE basemode = drawmode & ~INVERSVID;

    /* Process each row of the template */
    for (row = 0; row < ySize; row++)
    {
        LONG destY = yDest + row;
        const UBYTE *srcRow = (const UBYTE *)source + row * srcMod;

        /* Skip if outside bitmap */
        if (destY < 0 || destY >= (LONG)bm->Rows)
            continue;

        for (col = 0; col < xSize; col++)
        {
            LONG destX = xDest + col;
            LONG srcPixelX = xSrc + col;
            LONG srcByteOffset = srcPixelX / 8;
            UBYTE srcBitMask = (UBYTE)(0x80 >> (srcPixelX % 8));
            BOOL templateBit = (srcRow[srcByteOffset] & srcBitMask) != 0;

            /* Skip if outside bitmap */
            if (destX < 0 || destX >= (LONG)(bm->BytesPerRow * 8))
                continue;

            LONG destByteOffset = destX / 8;
            UBYTE destBitMask = (UBYTE)(0x80 >> (destX % 8));

            if (basemode == JAM1)
            {
                /* JAM1: only draw where template is 1 */
                if (templateBit)
                {
                    for (plane = 0; plane < bm->Depth; plane++)
                    {
                        UBYTE *destPlane = bm->Planes[plane] + destY * bm->BytesPerRow + destByteOffset;
                        if (fgpen & (1 << plane))
                            *destPlane |= destBitMask;
                        else
                            *destPlane &= ~destBitMask;
                    }
                }
            }
            else if (basemode == JAM2)
            {
                /* JAM2: draw foreground where template is 1, background where 0 */
                UBYTE pen = templateBit ? fgpen : bgpen;
                for (plane = 0; plane < bm->Depth; plane++)
                {
                    UBYTE *destPlane = bm->Planes[plane] + destY * bm->BytesPerRow + destByteOffset;
                    if (pen & (1 << plane))
                        *destPlane |= destBitMask;
                    else
                        *destPlane &= ~destBitMask;
                }
            }
            else if (basemode == COMPLEMENT)
            {
                /* COMPLEMENT: XOR where template is 1 */
                if (templateBit)
                {
                    for (plane = 0; plane < bm->Depth; plane++)
                    {
                        UBYTE *destPlane = bm->Planes[plane] + destY * bm->BytesPerRow + destByteOffset;
                        *destPlane ^= destBitMask;
                    }
                }
            }
        }
    }
}

static VOID _graphics_ClearEOL ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    UBYTE oldDrMd;
    WORD width;
    WORD ymin, ymax;

    DPRINTF (LOG_DEBUG, "_graphics: ClearEOL() rp=0x%08lx\n", (ULONG)rp);

    if (!rp || !rp->BitMap || !rp->Font)
        return;

    /* Get width of bitmap */
    width = (WORD)(rp->BitMap->BytesPerRow * 8);

    /* Calculate vertical range: from current position (minus baseline) to font height */
    ymin = rp->cp_y - rp->TxBaseline;
    ymax = ymin + rp->Font->tf_YSize - 1;

    /* Save old draw mode and invert INVERSVID to draw with BgPen */
    oldDrMd = rp->DrawMode;
    rp->DrawMode ^= INVERSVID;

    /* Fill from current x position to end of line */
    _graphics_RectFill(GfxBase, rp, rp->cp_x, ymin, width - 1, ymax);

    /* Restore draw mode */
    rp->DrawMode = oldDrMd;
}

static VOID _graphics_ClearScreen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    UBYTE oldDrMd;
    WORD width, height;
    WORD ymin;

    DPRINTF (LOG_DEBUG, "_graphics: ClearScreen() rp=0x%08lx\n", (ULONG)rp);

    if (!rp || !rp->BitMap || !rp->Font)
        return;

    /* Get bitmap dimensions */
    width = (WORD)(rp->BitMap->BytesPerRow * 8);
    height = rp->BitMap->Rows;

    /* First clear end of current line */
    _graphics_ClearEOL(GfxBase, rp);

    /* Calculate where to start clearing (line after current text line) */
    ymin = rp->cp_y - rp->TxBaseline + rp->Font->tf_YSize;

    /* If there are more lines below, clear them */
    if (height >= ymin)
    {
        /* Save old draw mode and invert INVERSVID to draw with BgPen */
        oldDrMd = rp->DrawMode;
        rp->DrawMode ^= INVERSVID;

        /* Fill from beginning of next line to end of screen */
        _graphics_RectFill(GfxBase, rp, 0, ymin, width - 1, height - 1);

        /* Restore draw mode */
        rp->DrawMode = oldDrMd;
    }
}

/*
 * Phase 15: Built-in Topaz 8x8 font
 * This is a statically allocated TextFont structure for the built-in font.
 * We store it here to avoid needing dynamic allocation for the default font.
 *
 * Note: The TextFont structure starts with a Message header which contains
 * a Node. We initialize only the fields we need; the linker will zero the rest.
 */
static char g_topaz8_name[] = "topaz.font";

static struct TextFont g_topaz8_font;  /* Initialized in init function */
static BOOL g_topaz8_initialized;     /* Starts as FALSE (BSS is zeroed) */

static void init_topaz8_font(void)
{
    if (g_topaz8_initialized)
        return;

    /* Initialize the font structure */
    lxa_memset(&g_topaz8_font, 0, sizeof(g_topaz8_font));

    /* Message/Node header */
    g_topaz8_font.tf_Message.mn_Node.ln_Type = NT_FONT;
    g_topaz8_font.tf_Message.mn_Node.ln_Name = g_topaz8_name;

    /* Font metrics */
    g_topaz8_font.tf_YSize = TOPAZ8_HEIGHT;
    g_topaz8_font.tf_Style = FS_NORMAL;
    g_topaz8_font.tf_Flags = FPF_ROMFONT | FPF_DESIGNED;
    g_topaz8_font.tf_XSize = TOPAZ8_WIDTH;
    g_topaz8_font.tf_Baseline = TOPAZ8_BASELINE;
    g_topaz8_font.tf_BoldSmear = 1;
    g_topaz8_font.tf_Accessors = 0;
    g_topaz8_font.tf_LoChar = TOPAZ8_FIRST;
    g_topaz8_font.tf_HiChar = TOPAZ8_LAST;
    g_topaz8_font.tf_CharData = (APTR)g_topaz8_data;
    g_topaz8_font.tf_Modulo = TOPAZ8_HEIGHT;  /* Bytes per character in our data */

    g_topaz8_initialized = TRUE;
}

static struct TextFont *get_default_font(void)
{
    init_topaz8_font();
    return &g_topaz8_font;
}

static WORD _graphics_TextLength ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD count __asm("d0"))
{
    struct TextFont *font;
    WORD width;

    DPRINTF (LOG_DEBUG, "_graphics: TextLength() rp=0x%08lx, string='%s', count=%lu\n",
             (ULONG)rp, string ? (char *)string : "(null)", count);

    if (!rp || !string || count == 0)
    {
        return 0;
    }

    /* Get the font from the RastPort, or use default */
    font = rp->Font;
    if (!font)
    {
        font = get_default_font();
    }

    /* For fixed-width fonts, it's simply count * XSize */
    width = (WORD)(count * font->tf_XSize);

    return width;
}

static LONG _graphics_Text ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register UWORD count __asm("d0"))
{
    struct TextFont *font;
    struct BitMap *bm;
    WORD x, y;
    ULONG i, row, col;
    UBYTE fgpen, bgpen;
    const UBYTE *glyph;
    UBYTE drawmode;

    DPRINTF (LOG_DEBUG, "_graphics: Text() string='%s' count=%u pos=(%d,%d) fg=%d bg=%d dm=%d\n",
             string ? (char *)string : "(null)", (unsigned int)count,
             rp ? (int)rp->cp_x : 0, rp ? (int)rp->cp_y : 0,
             rp ? (int)rp->FgPen : 0, rp ? (int)rp->BgPen : 0,
             rp ? (int)rp->DrawMode : 0);

    if (!rp || !string || count == 0)
    {
        return 0;
    }

    bm = rp->BitMap;
    if (!bm)
    {
        LPRINTF (LOG_ERROR, "_graphics: Text() RastPort has no BitMap\n");
        return 0;
    }

    /* Get the font from the RastPort, or use default */
    font = rp->Font;
    if (!font)
    {
        font = get_default_font();
    }

    /* Get drawing position and colors */
    x = rp->cp_x;
    y = rp->cp_y - font->tf_Baseline;  /* Adjust for baseline */
    fgpen = (UBYTE)rp->FgPen;
    bgpen = (UBYTE)rp->BgPen;
    drawmode = rp->DrawMode;

    /* If RastPort has a Layer, add layer offset for coordinate translation */
    if (rp->Layer)
    {
        x += rp->Layer->bounds.MinX;
        y += rp->Layer->bounds.MinY;
    }

    /* Handle INVERSVID - swap foreground and background colors */
    if (drawmode & INVERSVID)
    {
        UBYTE tmp = fgpen;
        fgpen = bgpen;
        bgpen = tmp;
    }

    /* Mask out INVERSVID to get base draw mode */
    UBYTE basemode = drawmode & ~INVERSVID;

    /* Draw each character */
    for (i = 0; i < count; i++)
    {
        UBYTE ch = (UBYTE)string[i];
        glyph = topaz8_get_glyph(ch);

        /* Draw the character glyph */
        for (row = 0; row < (ULONG)font->tf_YSize; row++)
        {
            UBYTE bits = glyph[row];
            WORD py = y + (WORD)row;

            /* Skip if row is outside bitmap */
            if (py < 0 || py >= (WORD)bm->Rows)
                continue;

            for (col = 0; col < (ULONG)font->tf_XSize; col++)
            {
                WORD px = x + (WORD)col;
                BOOL pixel_set = (bits & (0x80 >> col)) != 0;

                /* Skip if column is outside bitmap */
                if (px < 0 || px >= (WORD)(bm->BytesPerRow * 8))
                    continue;

                /* Determine what to draw based on draw mode */
                if (basemode == JAM1)
                {
                    /* JAM1: Only draw foreground pixels */
                    if (pixel_set)
                    {
                        /* Set pixel to FgPen */
                        WORD byte_idx = px / 8;
                        UBYTE bit_mask = (UBYTE)(0x80 >> (px % 8));
                        ULONG plane;

                        for (plane = 0; plane < (ULONG)bm->Depth; plane++)
                        {
                            UBYTE *plane_ptr = bm->Planes[plane] + py * bm->BytesPerRow + byte_idx;
                            if (fgpen & (1 << plane))
                                *plane_ptr |= bit_mask;
                            else
                                *plane_ptr &= ~bit_mask;
                        }
                    }
                }
                else if (basemode == JAM2)
                {
                    /* JAM2: Draw both foreground and background */
                    WORD byte_idx = px / 8;
                    UBYTE bit_mask = (UBYTE)(0x80 >> (px % 8));
                    UBYTE pen = pixel_set ? fgpen : bgpen;
                    ULONG plane;

                    for (plane = 0; plane < (ULONG)bm->Depth; plane++)
                    {
                        UBYTE *plane_ptr = bm->Planes[plane] + py * bm->BytesPerRow + byte_idx;
                        if (pen & (1 << plane))
                            *plane_ptr |= bit_mask;
                        else
                            *plane_ptr &= ~bit_mask;
                    }
                }
                else if (basemode == COMPLEMENT)
                {
                    /* COMPLEMENT: XOR with existing pixels */
                    if (pixel_set)
                    {
                        WORD byte_idx = px / 8;
                        UBYTE bit_mask = (UBYTE)(0x80 >> (px % 8));
                        ULONG plane;

                        for (plane = 0; plane < (ULONG)bm->Depth; plane++)
                        {
                            UBYTE *plane_ptr = bm->Planes[plane] + py * bm->BytesPerRow + byte_idx;
                            *plane_ptr ^= bit_mask;
                        }
                    }
                }
            }
        }

        /* Advance position */
        x += (WORD)font->tf_XSize;
    }

    /* Update RastPort cursor position */
    rp->cp_x = x;

    return (LONG)count;
}

static LONG _graphics_SetFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST struct TextFont * textFont __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetFont() rp=0x%08lx, font=0x%08lx\n",
             (ULONG)rp, (ULONG)textFont);

    if (!rp)
    {
        return 0;
    }

    /* Allow NULL to reset to default font */
    if (!textFont)
    {
        textFont = get_default_font();
    }

    rp->Font = (struct TextFont *)textFont;
    
    /* Update TxHeight and TxBaseline from the font */
    rp->TxHeight = textFont->tf_YSize;
    rp->TxBaseline = textFont->tf_Baseline;

    return 1;  /* Success */
}

static struct TextFont * _graphics_OpenFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextAttr * textAttr __asm("a0"))
{
    struct TextFont *font;

    DPRINTF (LOG_DEBUG, "_graphics: OpenFont() textAttr=0x%08lx\n", (ULONG)textAttr);

    font = get_default_font();

    if (!textAttr)
    {
        /* No attr specified, return default font */
        font->tf_Accessors++;
        return font;
    }

    DPRINTF (LOG_DEBUG, "_graphics: OpenFont() name='%s', size=%d\n",
             textAttr->ta_Name ? (char *)textAttr->ta_Name : "(null)",
             (int)textAttr->ta_YSize);

    /* Check if this is a request for topaz */
    if (textAttr->ta_Name)
    {
        /* Accept "topaz.font" or "topaz" with size 8 */
        const char *name = (const char *)textAttr->ta_Name;
        BOOL is_topaz = FALSE;

        /* Simple string comparison */
        if ((name[0] == 't' || name[0] == 'T') &&
            (name[1] == 'o' || name[1] == 'O') &&
            (name[2] == 'p' || name[2] == 'P') &&
            (name[3] == 'a' || name[3] == 'A') &&
            (name[4] == 'z' || name[4] == 'Z'))
        {
            is_topaz = TRUE;
        }

        if (is_topaz && (textAttr->ta_YSize == 0 || textAttr->ta_YSize == 8))
        {
            font->tf_Accessors++;
            return font;
        }
    }

    /* For any other font, just return the built-in topaz for now */
    /* A full implementation would search diskfont.library */
    DPRINTF (LOG_WARNING, "_graphics: OpenFont() font not found, using built-in topaz\n");
    font->tf_Accessors++;
    return font;
}

static VOID _graphics_CloseFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * textFont __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: CloseFont() font=0x%08lx\n", (ULONG)textFont);

    if (!textFont)
    {
        return;
    }

    /* Decrease reference count */
    if (textFont->tf_Accessors > 0)
    {
        textFont->tf_Accessors--;
    }

    /* For our built-in font, we never actually free it */
    /* A full implementation would free dynamically loaded fonts when refcount hits 0 */
}

static ULONG _graphics_AskSoftStyle ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    /*
     * AskSoftStyle() returns a mask of font styles that can be algorithmically
     * generated for the current font. Styles include:
     *   FSF_UNDERLINED (0x01), FSF_BOLD (0x02), FSF_ITALIC (0x04), FSF_EXTENDED (0x08)
     * We return that all styles can be software-rendered.
     */
    DPRINTF (LOG_DEBUG, "_graphics: AskSoftStyle() rp=0x%08lx\n", rp);
    return 0x0F;  /* All basic styles available: underline, bold, italic, extended */
}

static ULONG _graphics_SetSoftStyle ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register ULONG style __asm("d0"),
                                                        register ULONG enable __asm("d1"))
{
    /*
     * SetSoftStyle() sets which styles should be algorithmically rendered.
     * Returns the styles that were actually enabled.
     * We just accept whatever is requested within the enable mask.
     */
    DPRINTF (LOG_DEBUG, "_graphics: SetSoftStyle() rp=0x%08lx style=0x%08lx enable=0x%08lx\n",
             rp, style, enable);
    return style & enable;
}

static VOID _graphics_AddBob ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Bob * bob __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AddBob() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_AddVSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct VSprite * vSprite __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AddVSprite() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_DoCollision ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: DoCollision() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_DrawGList ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: DrawGList() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_InitGels ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct VSprite * head __asm("a0"),
                                                        register struct VSprite * tail __asm("a1"),
                                                        register struct GelsInfo * gelsInfo __asm("a2"))
{
    /*
     * InitGels() initializes the GEL (Graphics ELement) system for a RastPort.
     * This sets up the VSprite list with head/tail sentinels for sprite animation.
     * Since we don't support hardware sprites/GELs, we just initialize the structure.
     */
    DPRINTF (LOG_DEBUG, "_graphics: InitGels() head=0x%08lx tail=0x%08lx gelsInfo=0x%08lx\n",
             (ULONG)head, (ULONG)tail, (ULONG)gelsInfo);
    
    if (gelsInfo) {
        /* Set up the VSprite list with sentinels */
        gelsInfo->sprRsrvd = 0;
        gelsInfo->Flags = 0;
        gelsInfo->gelHead = head;
        gelsInfo->gelTail = tail;
        gelsInfo->nextLine = NULL;
        gelsInfo->lastColor = NULL;
        gelsInfo->collHandler = NULL;
        gelsInfo->leftmost = 0;
        gelsInfo->rightmost = 0;
        gelsInfo->topmost = 0;
        gelsInfo->bottommost = 0;
        gelsInfo->firstBlissObj = NULL;
        gelsInfo->lastBlissObj = NULL;
        
        /* Link the head and tail */
        if (head) {
            head->NextVSprite = tail;
            head->PrevVSprite = NULL;
            head->ClearPath = NULL;
        }
        if (tail) {
            tail->PrevVSprite = head;
            tail->NextVSprite = NULL;
            tail->ClearPath = NULL;
        }
    }
}

static VOID _graphics_InitMasks ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct VSprite * vSprite __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: InitMasks() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_RemIBob ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Bob * bob __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register struct ViewPort * vp __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: RemIBob() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_RemVSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct VSprite * vSprite __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: RemVSprite() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SetCollision ( register struct GfxBase  *GfxBase   __asm("a6"),
                                              register void            (*routine) __asm("a0"),
                                              register struct GelsInfo *GInfo     __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetCollision() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SortGList ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: SortGList() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_AddAnimOb ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AnimOb * anOb __asm("a0"),
                                                        register struct AnimOb ** anKey __asm("a1"),
                                                        register struct RastPort * rp __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: AddAnimOb() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_Animate ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AnimOb ** anKey __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: Animate() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL _graphics_GetGBuffers ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AnimOb * anOb __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG flag __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetGBuffers() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

static VOID _graphics_InitGMasks ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AnimOb * anOb __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: InitGMasks() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_DrawEllipse ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD xCenter __asm("d0"),
                                                        register WORD yCenter __asm("d1"),
                                                        register WORD a __asm("d2"),
                                                        register WORD b __asm("d3"))
{
    LONG x, y;
    LONG a2, b2;
    LONG fa2, fb2;
    LONG sigma;

    if (!rp)
        return;

    /* Handle degenerate cases */
    if (a == 0 && b == 0)
    {
        _graphics_WritePixel(GfxBase, rp, (WORD)xCenter, (WORD)yCenter);
        return;
    }
    if (a == 0)
    {
        /* Vertical line */
        WORD y_coord;
        for (y_coord = yCenter - b; y_coord <= yCenter + b; y_coord++)
            _graphics_WritePixel(GfxBase, rp, xCenter, y_coord);
        return;
    }
    if (b == 0)
    {
        /* Horizontal line */
        WORD x_coord;
        for (x_coord = xCenter - a; x_coord <= xCenter + a; x_coord++)
            _graphics_WritePixel(GfxBase, rp, x_coord, yCenter);
        return;
    }

    /* Midpoint ellipse algorithm */
    x = 0;
    y = b;
    a2 = a * a;
    b2 = b * b;
    fa2 = 4 * a2;
    fb2 = 4 * b2;
    sigma = 2 * b2 + a2 * (1 - 2 * b);

    /* Region 1 */
    while (b2 * x <= a2 * y)
    {
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter + x), (WORD)(yCenter + y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter - x), (WORD)(yCenter + y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter + x), (WORD)(yCenter - y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter - x), (WORD)(yCenter - y));

        if (sigma >= 0)
        {
            sigma += fa2 * (1 - y);
            y--;
        }
        sigma += b2 * ((4 * x) + 6);
        x++;
    }

    /* Region 2 */
    sigma = 2 * a2 + b2 * (1 - 2 * a);
    x = a;
    y = 0;
    while (x >= 0)
    {
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter + x), (WORD)(yCenter + y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter - x), (WORD)(yCenter + y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter + x), (WORD)(yCenter - y));
        _graphics_WritePixel(GfxBase, rp, (WORD)(xCenter - x), (WORD)(yCenter - y));

        if (sigma >= 0)
        {
            sigma += fb2 * (1 - x);
            x--;
        }
        sigma += a2 * ((4 * y) + 6);
        y++;
    }
}

static LONG _graphics_AreaEllipse ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD xCenter __asm("d0"),
                                                        register WORD yCenter __asm("d1"),
                                                        register WORD a __asm("d2"),
                                                        register WORD b __asm("d3"))
{
    LONG x, y;
    LONG a2, b2;
    LONG fa2, fb2;
    LONG sigma;
    LONG result;

    DPRINTF (LOG_DEBUG, "_graphics: AreaEllipse() rp=0x%08lx, center=(%ld,%ld), a=%ld, b=%ld\n",
             (ULONG)rp, xCenter, yCenter, a, b);

    if (!rp)
        return -1;

    /* Handle degenerate cases */
    if (a == 0 && b == 0)
    {
        return _graphics_AreaMove(GfxBase, rp, xCenter, yCenter);
    }

    /* Start at the rightmost point of the ellipse */
    result = _graphics_AreaMove(GfxBase, rp, xCenter + a, yCenter);
    if (result != 0)
        return result;

    /* Midpoint ellipse algorithm - trace the upper right quadrant */
    x = 0;
    y = b;
    a2 = a * a;
    b2 = b * b;
    fa2 = 4 * a2;
    fb2 = 4 * b2;
    sigma = 2 * b2 + a2 * (1 - 2 * b);

    /* Region 1 - trace to the top */
    while (b2 * x <= a2 * y)
    {
        result = _graphics_AreaDraw(GfxBase, rp, xCenter + x, yCenter - y);
        if (result != 0)
            return result;

        if (sigma >= 0)
        {
            sigma += fa2 * (1 - y);
            y--;
        }
        sigma += b2 * ((4 * x) + 6);
        x++;
    }

    /* Region 2 - continue to the left */
    sigma = 2 * a2 + b2 * (1 - 2 * a);
    x = a;
    y = 0;
    while (a2 * y <= b2 * x)
    {
        result = _graphics_AreaDraw(GfxBase, rp, xCenter - x, yCenter - y);
        if (result != 0)
            return result;

        if (sigma >= 0)
        {
            sigma += fb2 * (1 - x);
            x--;
        }
        sigma += a2 * ((4 * y) + 6);
        y++;
    }

    /* Continue to bottom left quadrant */
    x = 0;
    y = b;
    sigma = 2 * b2 + a2 * (1 - 2 * b);
    while (b2 * x <= a2 * y)
    {
        result = _graphics_AreaDraw(GfxBase, rp, xCenter - x, yCenter + y);
        if (result != 0)
            return result;

        if (sigma >= 0)
        {
            sigma += fa2 * (1 - y);
            y--;
        }
        sigma += b2 * ((4 * x) + 6);
        x++;
    }

    /* Complete to bottom right quadrant */
    sigma = 2 * a2 + b2 * (1 - 2 * a);
    x = a;
    y = 0;
    while (a2 * y <= b2 * x)
    {
        result = _graphics_AreaDraw(GfxBase, rp, xCenter + x, yCenter + y);
        if (result != 0)
            return result;

        if (sigma >= 0)
        {
            sigma += fb2 * (1 - x);
            x--;
        }
        sigma += a2 * ((4 * y) + 6);
        y++;
    }

    /* Close back to start */
    return _graphics_AreaDraw(GfxBase, rp, xCenter + a, yCenter);
}

static VOID _graphics_LoadRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register CONST UWORD * colors __asm("a1"),
                                                        register LONG count __asm("d0"))
{
    /*
     * LoadRGB4 loads color palette entries from an array of 4-bit per gun colors.
     * Each UWORD contains 0x0RGB format (4 bits per component).
     *
     * For lxa, we use the host display system which handles colors differently.
     * We log the call but don't need to actually modify any palette since
     * our rendering converts to 24-bit colors internally.
     */
    DPRINTF (LOG_DEBUG, "_graphics: LoadRGB4() vp=0x%08lx colors=0x%08lx count=%ld (no-op)\n",
             (ULONG)vp, (ULONG)colors, count);
    /* No-op: palette is handled by host display */
}

static VOID _graphics_InitRastPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: InitRastPort() rp=0x%08lx\n", (ULONG)rp);

    if (!rp)
        return;

    /* Zero out the entire RastPort structure */
    lxa_memset(rp, 0, sizeof(struct RastPort));

    /* Explicitly NULL critical pointers (belt and suspenders) */
    rp->Layer = NULL;
    rp->BitMap = NULL;
    rp->Font = NULL;
    rp->AreaPtrn = NULL;
    rp->TmpRas = NULL;
    rp->AreaInfo = NULL;
    rp->GelsInfo = NULL;

    /* Set default values per AROS/RKRM specification */
    rp->Mask = 0xFF;           /* All planes enabled for writing */
    rp->FgPen = -1;            /* Foreground pen = -1 (0xFF) per AROS */
    rp->BgPen = 0;             /* Background pen = 0 */
    rp->AOlPen = -1;           /* Outline pen = -1 per AROS */
    rp->DrawMode = JAM2;       /* Default drawing mode */
    rp->LinePtrn = 0xFFFF;     /* Solid line pattern */
    rp->Flags = FRST_DOT;      /* Draw first dot */
    rp->PenWidth = 1;
    rp->PenHeight = 1;

    /* Set font to GfxBase->DefaultFont if available */
    if (GfxBase && GfxBase->DefaultFont)
    {
        rp->Font = GfxBase->DefaultFont;
        rp->TxHeight = GfxBase->DefaultFont->tf_YSize;
        rp->TxBaseline = GfxBase->DefaultFont->tf_Baseline;
        DPRINTF (LOG_DEBUG, "_graphics: InitRastPort() Font set to 0x%08lx\n", (ULONG)rp->Font);
    }
    else
    {
        DPRINTF (LOG_DEBUG, "_graphics: InitRastPort() GfxBase=0x%08lx, DefaultFont=0x%08lx - no font set!\n",
                 (ULONG)GfxBase, GfxBase ? (ULONG)GfxBase->DefaultFont : 0);
    }
}

static VOID _graphics_InitVPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    /* Initialize a ViewPort structure to default values.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: InitVPort(vp=%p)\n", vp);
    
    if (!vp)
        return;
    
    vp->Next = NULL;
    vp->ColorMap = NULL;
    vp->DspIns = NULL;
    vp->SprIns = NULL;
    vp->ClrIns = NULL;
    vp->UCopIns = NULL;
    vp->DWidth = 0;
    vp->DHeight = 0;
    vp->DxOffset = 0;
    vp->DyOffset = 0;
    vp->Modes = 0;
    vp->SpritePriorities = 0x24;  /* Default sprite priorities */
    vp->ExtendedModes = 0;
    vp->RasInfo = NULL;
}

static ULONG _graphics_MrgCop ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a1"))
{
    /* MrgCop() merges the copper lists from all ViewPorts into a single list.
     * In lxa, we use SDL-based rendering through Intuition, so we don't need
     * actual copper lists. This stub returns success (0) to allow RKM samples
     * like RGBBoxes to run without crashing.
     */
    DPRINTF (LOG_DEBUG, "_graphics: MrgCop() view=0x%08lx (stub - no-op)\n", (ULONG)view);
    return 0;  /* MVP_OK - success */
}

static ULONG _graphics_MakeVPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a0"),
                                                        register struct ViewPort * vp __asm("a1"))
{
    /* MakeVPort() builds the copper list for a ViewPort.
     * In lxa, we use SDL-based rendering, so copper lists are not needed.
     * This stub returns success to allow RKM samples like RGBBoxes to run.
     */
    DPRINTF (LOG_DEBUG, "_graphics: MakeVPort() view=0x%08lx, vp=0x%08lx (stub - no-op)\n", (ULONG)view, (ULONG)vp);
    return 0;  /* MVP_OK - success */
}

static VOID _graphics_LoadView ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a1"))
{
    /* LoadView() loads the View's copper list into the hardware.
     * In lxa, we use SDL-based rendering through Intuition screens/windows.
     * For direct View manipulation (like RGBBoxes sample), we update GfxBase->ActiView
     * but actual display is handled by Intuition's screen system.
     */
    DPRINTF (LOG_DEBUG, "_graphics: LoadView() view=0x%08lx (stub - updates ActiView only)\n", (ULONG)view);
    
    if (GfxBase) {
        GfxBase->ActiView = view;
    }
}

static VOID _graphics_WaitBlit ( register struct GfxBase * GfxBase __asm("a6"))
{
    /* No-op: No real blitter hardware to wait for */
    DPRINTF (LOG_DEBUG, "_graphics: WaitBlit() (no-op)\n");
}

static VOID _graphics_SetRast ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register UBYTE pen __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetRast() rp=0x%08lx, pen=%u\n", (ULONG)rp, (unsigned)pen);

    if (!rp || !rp->BitMap)
        return;

    struct BitMap *bm = rp->BitMap;
    ULONG planeSize = bm->BytesPerRow * bm->Rows;

    /* Fill each plane with 0x00 or 0xFF depending on pen bit */
    for (UBYTE plane = 0; plane < bm->Depth; plane++)
    {
        if (bm->Planes[plane])
        {
            UBYTE fillByte = (pen & (1 << plane)) ? 0xFF : 0x00;
            lxa_memset(bm->Planes[plane], fillByte, planeSize);
        }
    }
}

static VOID _graphics_Move ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD x __asm("d0"),
                                                        register WORD y __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: Move() rp=0x%08lx, x=%d, y=%d\n", (ULONG)rp, (int)x, (int)y);

    if (rp)
    {
        rp->cp_x = (WORD)x;
        rp->cp_y = (WORD)y;
    }
}

static VOID _graphics_Draw ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD x __asm("d0"),
                                                        register WORD y __asm("d1"))
{
    struct BitMap *bm;
    BYTE pen;
    WORD x0, y0, x1, y1;
    WORD dx, dy, sx, sy, err;

    DPRINTF (LOG_DEBUG, "_graphics: Draw() rp=0x%08lx, from (%d,%d) to (%d,%d)\n",
             (ULONG)rp, rp ? rp->cp_x : 0, rp ? rp->cp_y : 0, (int)x, (int)y);

    if (!rp || !rp->BitMap)
    {
        /* No bitmap to draw to - just update position */
        if (rp)
        {
            rp->cp_x = (WORD)x;
            rp->cp_y = (WORD)y;
        }
        return;
    }

    bm = rp->BitMap;
    pen = rp->FgPen;

    /* If RastPort has a Layer, use ClipRect-aware drawing */
    if (rp->Layer)
    {
        struct Layer *layer = rp->Layer;
        WORD layerOffX = layer->bounds.MinX;
        WORD layerOffY = layer->bounds.MinY;

        /* Software line drawing using Bresenham's algorithm */
        x0 = rp->cp_x + layerOffX;
        y0 = rp->cp_y + layerOffY;
        x1 = x + layerOffX;
        y1 = y + layerOffY;

        dx = x1 > x0 ? x1 - x0 : x0 - x1;
        dy = y1 > y0 ? y0 - y1 : y1 - y0;  /* Negative for Bresenham */
        sx = x0 < x1 ? 1 : -1;
        sy = y0 < y1 ? 1 : -1;
        err = dx + dy;

        while (1)
        {
            /* For each pixel, check if it falls in a visible ClipRect */
            if (x0 >= 0 && y0 >= 0 && x0 < (bm->BytesPerRow * 8) && y0 < bm->Rows)
            {
                struct ClipRect *cr;
                for (cr = layer->ClipRect; cr != NULL; cr = cr->Next)
                {
                    if (cr->obscured)
                        continue;

                    if (x0 >= cr->bounds.MinX && x0 <= cr->bounds.MaxX &&
                        y0 >= cr->bounds.MinY && y0 <= cr->bounds.MaxY)
                    {
                        /* Pixel is visible - draw it */
                        SetPixelDirect(bm, x0, y0, pen, rp->DrawMode);
                        break;
                    }
                }
            }

            if (x0 == x1 && y0 == y1)
                break;

            {
                WORD e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x0 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        /* Update current position */
        rp->cp_x = (WORD)x;
        rp->cp_y = (WORD)y;
        return;
    }

    /* No layer - simple drawing with just bitmap bounds check */
    x0 = rp->cp_x;
    y0 = rp->cp_y;
    x1 = x;
    y1 = y;

    dx = x1 > x0 ? x1 - x0 : x0 - x1;
    dy = y1 > y0 ? y0 - y1 : y1 - y0;
    sx = x0 < x1 ? 1 : -1;
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    while (1)
    {
        if (x0 >= 0 && y0 >= 0 && x0 < (bm->BytesPerRow * 8) && y0 < bm->Rows)
        {
            SetPixelDirect(bm, x0, y0, pen, rp->DrawMode);
        }

        if (x0 == x1 && y0 == y1)
            break;

        {
            WORD e2 = 2 * err;
            if (e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    /* Update current position */
    rp->cp_x = (WORD)x;
    rp->cp_y = (WORD)y;
}

/* AreaInfo flag constants (from AROS graphics_intern.h) */
#ifndef AREAINFOFLAG_MOVE
#define AREAINFOFLAG_MOVE      0x00
#define AREAINFOFLAG_DRAW      0x01
#define AREAINFOFLAG_CLOSEDRAW 0x02
#endif

/* Helper function to close a polygon (simplified version from AROS) */
static void areaclosepolygon(struct AreaInfo *areainfo)
{
    /* Only close if last operation was a draw and we're not already at the start */
    if (areainfo->Count > 0 && areainfo->FlagPtr[-1] == AREAINFOFLAG_DRAW)
    {
        if ((areainfo->VctrPtr[-1] != areainfo->FirstY) ||
            (areainfo->VctrPtr[-2] != areainfo->FirstX))
        {
            /* Add closing vertex if there's room */
            if (areainfo->Count < areainfo->MaxCount)
            {
                areainfo->Count++;
                areainfo->VctrPtr[0] = areainfo->FirstX;
                areainfo->VctrPtr[1] = areainfo->FirstY;
                areainfo->FlagPtr[0] = AREAINFOFLAG_CLOSEDRAW;
                areainfo->VctrPtr = &areainfo->VctrPtr[2];
                areainfo->FlagPtr++;
            }
        }
    }
}

static LONG _graphics_AreaMove ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    struct AreaInfo *areainfo;
    WORD wx = (WORD)x;
    WORD wy = (WORD)y;

    DPRINTF (LOG_DEBUG, "_graphics: AreaMove() rp=0x%08lx, x=%ld, y=%ld\n",
             (ULONG)rp, x, y);

    if (!rp || !rp->AreaInfo)
    {
        LPRINTF (LOG_ERROR, "_graphics: AreaMove() NULL RastPort or AreaInfo\n");
        return -1;
    }

    areainfo = rp->AreaInfo;

    /* Check if there's room for one entry */
    if (areainfo->Count + 1 > areainfo->MaxCount)
        return -1;

    /* Is this the first entry? */
    if (areainfo->Count == 0)
    {
        areainfo->FirstX = wx;
        areainfo->FirstY = wy;

        areainfo->VctrPtr[0] = wx;
        areainfo->VctrPtr[1] = wy;
        areainfo->VctrPtr = &areainfo->VctrPtr[2];

        areainfo->FlagPtr[0] = AREAINFOFLAG_MOVE;
        areainfo->FlagPtr++;

        areainfo->Count++;
    }
    else
    {
        /* If previous command was also AreaMove, replace it */
        if (areainfo->FlagPtr[-1] == AREAINFOFLAG_MOVE)
        {
            areainfo->FirstX = wx;
            areainfo->FirstY = wy;

            areainfo->VctrPtr[-2] = wx;
            areainfo->VctrPtr[-1] = wy;
        }
        else
        {
            /* Close previous polygon if needed */
            areaclosepolygon(areainfo);

            /* Check again for room after potential close */
            if (areainfo->Count + 1 > areainfo->MaxCount)
                return -1;

            areainfo->FirstX = wx;
            areainfo->FirstY = wy;

            areainfo->VctrPtr[0] = wx;
            areainfo->VctrPtr[1] = wy;
            areainfo->VctrPtr = &areainfo->VctrPtr[2];

            areainfo->FlagPtr[0] = AREAINFOFLAG_MOVE;
            areainfo->FlagPtr++;

            areainfo->Count++;
        }
    }

    return 0;
}

static LONG _graphics_AreaDraw ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    struct AreaInfo *areainfo;
    WORD wx = (WORD)x;
    WORD wy = (WORD)y;

    DPRINTF (LOG_DEBUG, "_graphics: AreaDraw() rp=0x%08lx, x=%ld, y=%ld\n",
             (ULONG)rp, x, y);

    if (!rp || !rp->AreaInfo)
    {
        LPRINTF (LOG_ERROR, "_graphics: AreaDraw() NULL RastPort or AreaInfo\n");
        return -1;
    }

    areainfo = rp->AreaInfo;

    /* Check if there's room for one entry */
    if (areainfo->Count + 1 > areainfo->MaxCount)
        return -1;

    /* Add point to vector list */
    areainfo->Count++;
    areainfo->VctrPtr[0] = wx;
    areainfo->VctrPtr[1] = wy;
    areainfo->FlagPtr[0] = AREAINFOFLAG_DRAW;

    areainfo->VctrPtr = &areainfo->VctrPtr[2];
    areainfo->FlagPtr++;

    return 0;
}

static LONG _graphics_AreaEnd ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    struct AreaInfo *areainfo;
    WORD *CurVctr;
    BYTE *CurFlag;
    UWORD Count;
    UWORD Rem_cp_x, Rem_cp_y;

    DPRINTF (LOG_DEBUG, "_graphics: AreaEnd() rp=0x%08lx\n", (ULONG)rp);

    if (!rp || !rp->AreaInfo)
    {
        LPRINTF (LOG_ERROR, "_graphics: AreaEnd() NULL RastPort or AreaInfo\n");
        return -1;
    }

    areainfo = rp->AreaInfo;

    /* Check if there's anything to draw and we have TmpRas */
    if (areainfo->Count == 0 || !rp->TmpRas)
    {
        /* Reset AreaInfo for next polygon */
        areainfo->VctrPtr = areainfo->VctrTbl;
        areainfo->FlagPtr = areainfo->FlagTbl;
        areainfo->Count = 0;
        return (areainfo->Count == 0) ? 0 : -1;
    }

    /* Save cursor position */
    Rem_cp_x = rp->cp_x;
    Rem_cp_y = rp->cp_y;

    /* Close the polygon if needed */
    areaclosepolygon(areainfo);

    Count = areainfo->Count;
    CurVctr = areainfo->VctrTbl;
    CurFlag = areainfo->FlagTbl;

    /* Draw the polygon outline */
    while (Count > 0)
    {
        switch ((unsigned char)CurFlag[0])
        {
            case AREAINFOFLAG_MOVE:
                _graphics_Move(GfxBase, rp, CurVctr[0], CurVctr[1]);
                break;

            case AREAINFOFLAG_DRAW:
            case AREAINFOFLAG_CLOSEDRAW:
                _graphics_Draw(GfxBase, rp, CurVctr[0], CurVctr[1]);
                break;
        }

        CurVctr = &CurVctr[2];
        CurFlag = &CurFlag[1];
        Count--;
    }

    /* TODO: Implement proper polygon filling using scan-line algorithm from AROS areafill.c */
    /* For now, we just draw the outline */

    /* Restore cursor position */
    rp->cp_x = Rem_cp_x;
    rp->cp_y = Rem_cp_y;

    /* Reset AreaInfo for next polygon */
    areainfo->VctrPtr = areainfo->VctrTbl;
    areainfo->FlagPtr = areainfo->FlagTbl;
    areainfo->Count = 0;

    return 0;
}


static VOID _graphics_WaitTOF ( register struct GfxBase * GfxBase __asm("a6"))
{
    struct Screen *screen;
    int i;
    
    DPRINTF (LOG_DEBUG, "_graphics: WaitTOF()\n");
    
    /*
     * WaitTOF() waits for the next vertical blank (top of frame).
     * This is approximately 1/50th of a second (20ms for PAL).
     * 
     * We implement this by:
     * 1. Calling EMU_CALL_WAIT multiple times to wait ~20ms total
     * 2. Processing input events
     * 3. Refreshing displays
     * 
     * EMU_CALL_WAIT sleeps for 1ms, so we call it ~20 times.
     */
    for (i = 0; i < 20; i++)
    {
        emucall0(EMU_CALL_WAIT);
    }
    
    /* Process input events and refresh displays for all Intuition screens.
     * This is a good hook point since WaitTOF is called in main loops.
     * 
     * IMPORTANT: Use global IntuitionBase instead of OpenLibrary() to avoid
     * reentrancy issues when VBlank interrupts fire during library calls.
     * The global is set up at ROM initialization time (exec.c).
     */
    if (!IntuitionBase)
    {
        return;
    }
    
    for (screen = IntuitionBase->FirstScreen; screen; screen = screen->NextScreen)
    {
        _intuition_ProcessInputEvents(screen);
        
        /* Refresh the screen's display from its planar bitmap */
        ULONG display_handle = (ULONG)screen->ExtData;
        if (display_handle)
        {
            /* Build planes pointer array for emucall */
            /* The screen's BitMap.Planes[] array contains the plane addresses */
            ULONG bpr = screen->BitMap.BytesPerRow;
            ULONG depth = screen->BitMap.Depth;
            
            /* Pack bpr and depth into single parameter: (bpr << 16) | depth */
            ULONG bpr_depth = (bpr << 16) | (depth & 0xFFFF);
            
            /* Pass the address of the Planes array */
            emucall3(EMU_CALL_INT_REFRESH_SCREEN, display_handle, 
                     (ULONG)&screen->BitMap.Planes[0], bpr_depth);
        }
    }
}

static VOID _graphics_QBlit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct bltnode * blit __asm("a1"))
{
    /*
     * QBlit() queues a blitter node for execution.
     * In emulation we have no hardware blitter queue, so we just call
     * the blit function directly if provided.
     */
    DPRINTF (LOG_DEBUG, "_graphics: QBlit() blit=0x%08lx - executing immediately (no queue)\n", (ULONG)blit);
    
    if (blit && blit->function) {
        /* Execute the blit function directly */
        /* Note: On real hardware this would be queued and executed by the blitter interrupt */
        blit->function();
    }
}

static VOID _graphics_InitArea ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AreaInfo * areaInfo __asm("a0"),
                                                        register APTR vectorBuffer __asm("a1"),
                                                        register LONG maxVectors __asm("d0"))
{
    /*
     * InitArea() initializes an AreaInfo structure for area fill operations.
     * The vectorBuffer must be maxVectors*5 bytes (each vertex takes 5 bytes:
     * 2 WORDs for X,Y coords + 1 BYTE for flags).
     */
    DPRINTF (LOG_DEBUG, "_graphics: InitArea() areaInfo=0x%08lx vectorBuffer=0x%08lx maxVectors=%ld\n",
             areaInfo, vectorBuffer, maxVectors);

    if (!areaInfo || !vectorBuffer)
        return;

    /* VctrTbl and VctrPtr point to the vertex coordinate storage */
    areaInfo->VctrTbl = (WORD *)vectorBuffer;
    areaInfo->VctrPtr = (WORD *)vectorBuffer;

    /* FlagTbl and FlagPtr point to the flags portion (after coordinate data) */
    /* Each vertex uses 2 WORDs (4 bytes), flags follow the coord data */
    areaInfo->FlagTbl = (BYTE *)vectorBuffer + (maxVectors * 4);
    areaInfo->FlagPtr = areaInfo->FlagTbl;

    areaInfo->Count = 0;
    areaInfo->MaxCount = (WORD)maxVectors;
    areaInfo->FirstX = 0;
    areaInfo->FirstY = 0;
}

static VOID _graphics_SetRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register LONG index __asm("d0"),
                                                        register ULONG red __asm("d1"),
                                                        register ULONG green __asm("d2"),
                                                        register ULONG blue __asm("d3"))
{
    /*
     * SetRGB4() sets a color register in the viewport's colormap.
     * Colors are 4-bit values (0-15) for old OCS/ECS machines.
     * For now we just log and ignore - our display doesn't use the colormap directly.
     */
    DPRINTF (LOG_DEBUG, "_graphics: SetRGB4() vp=0x%08lx index=%ld RGB=(%lu,%lu,%lu)\n",
             (ULONG)vp, index, red, green, blue);
}

static VOID _graphics_QBSBlit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct bltnode * blit __asm("a1"))
{
    /*
     * QBSBlit() queues a bltnode for synchronized blitter execution.
     * Similar to QBlit but with beam synchronization (waits for specific scanline).
     * In emulation we execute immediately (no beam sync in software rendering).
     */
    DPRINTF (LOG_DEBUG, "_graphics: QBSBlit() blit=0x%08lx - executing immediately (no queue/sync)\n", (ULONG)blit);
    
    if (blit && blit->function) {
        blit->function();
    }
}

static VOID _graphics_BltClear ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register PLANEPTR memBlock __asm("a1"),
                                                        register ULONG byteCount __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    /*
     * BltClear() clears a block of memory using the blitter.
     * byteCount's format depends on flags:
     *   If flags bit 1 set: high word = rows, low word = bytes per row
     *   Otherwise: byteCount is total bytes
     * flags bit 0: 0 = wait for completion, 1 = return immediately
     * flags bit 2: 0 = clear to 0, 1 = fill with $FF (set)
     * 
     * In emulation we use CPU memset (no real blitter).
     */
    ULONG totalBytes;
    UBYTE fillValue;
    
    DPRINTF (LOG_DEBUG, "_graphics: BltClear() memBlock=0x%08lx byteCount=0x%08lx flags=0x%08lx\n",
             (ULONG)memBlock, byteCount, flags);
    
    if (!memBlock)
        return;
    
    /* Determine fill value */
    fillValue = (flags & 0x04) ? 0xFF : 0x00;
    
    /* Calculate total bytes */
    if (flags & 0x02) {
        /* Row format: high word = rows, low word = bytes per row */
        UWORD rows = (byteCount >> 16) & 0xFFFF;
        UWORD bytesPerRow = byteCount & 0xFFFF;
        totalBytes = (ULONG)rows * (ULONG)bytesPerRow;
    } else {
        totalBytes = byteCount;
    }
    
    /* Clear the memory */
    lxa_memset(memBlock, fillValue, totalBytes);
}

static VOID _graphics_RectFill ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD xMin __asm("d0"),
                                                        register WORD yMin __asm("d1"),
                                                        register WORD xMax __asm("d2"),
                                                        register WORD yMax __asm("d3"))
{
    struct BitMap *bm;
    BYTE pen;
    UBYTE drawmode;

    DPRINTF (LOG_DEBUG, "_graphics: RectFill() rp=0x%08lx, (%d,%d)-(%d,%d)\n",
             (ULONG)rp, (int)xMin, (int)yMin, (int)xMax, (int)yMax);

    if (!rp || !rp->BitMap)
        return;

    bm = rp->BitMap;
    pen = rp->FgPen;
    drawmode = rp->DrawMode;

    /* If RastPort has a Layer, use ClipRect-aware drawing */
    if (rp->Layer)
    {
        struct Layer *layer = rp->Layer;
        struct ClipRect *cr;
        WORD absXMin, absYMin, absXMax, absYMax;

        /* Convert to absolute screen coordinates */
        absXMin = xMin + layer->bounds.MinX;
        absYMin = yMin + layer->bounds.MinY;
        absXMax = xMax + layer->bounds.MinX;
        absYMax = yMax + layer->bounds.MinY;

        /* For each ClipRect, fill the intersection */
        for (cr = layer->ClipRect; cr != NULL; cr = cr->Next)
        {
            WORD clipXMin, clipYMin, clipXMax, clipYMax;

            /* Skip obscured ClipRects (for LAYERSIMPLE) */
            if (cr->obscured)
                continue;

            /* Calculate intersection between fill rect and ClipRect */
            if (ClipIntersectRects(absXMin, absYMin, absXMax, absYMax,
                                   cr->bounds.MinX, cr->bounds.MinY,
                                   cr->bounds.MaxX, cr->bounds.MaxY,
                                   &clipXMin, &clipYMin, &clipXMax, &clipYMax))
            {
                /* Fill this clipped portion */
                FillRectDirect(bm, clipXMin, clipYMin, clipXMax, clipYMax, pen, drawmode);
            }
        }
        return;
    }

    /* No layer - simple drawing with just bitmap bounds check */
    FillRectDirect(bm, xMin, yMin, xMax, yMax, pen, drawmode);
}

static VOID _graphics_BltPattern ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST PLANEPTR mask __asm("a0"),
                                                        register LONG xMin __asm("d0"),
                                                        register LONG yMin __asm("d1"),
                                                        register LONG xMax __asm("d2"),
                                                        register LONG yMax __asm("d3"),
                                                        register ULONG maskBPR __asm("d4"))
{
    UBYTE old_drawmode;
    
    DPRINTF (LOG_DEBUG, "_graphics: BltPattern() rp=0x%08lx, mask=0x%08lx, rect=(%ld,%ld)-(%ld,%ld), bpr=%lu\n",
             (ULONG)rp, (ULONG)mask, xMin, yMin, xMax, yMax, maskBPR);
    
    if (!rp)
        return;
    
    /* For now, we don't support AreaPtrn patterns - just mask or solid fill */
    if (rp->AreaPtrn)
    {
        DPRINTF(LOG_DEBUG, "_graphics: BltPattern() with AreaPtrn not yet fully supported\n");
        /* Fall through to mask/solid handling */
    }
    
    if (mask)
    {
        /* Use BltTemplate to blit the mask */
        /* Save and restore draw mode if needed */
        old_drawmode = (UBYTE)rp->DrawMode;
        
        /* If in JAM2 mode, switch to JAM1 for template blitting */
        if ((old_drawmode & ~INVERSVID) == JAM2)
        {
            _graphics_SetDrMd(GfxBase, rp, JAM1 | (old_drawmode & INVERSVID));
        }
        
        /* BltTemplate params: source, xSrc, srcMod, destRP, xDest, yDest, xSize, ySize */
        _graphics_BltTemplate(GfxBase, (CONST PLANEPTR)mask, 0, maskBPR, rp,
                             xMin, yMin, xMax - xMin + 1, yMax - yMin + 1);
        
        /* Restore original draw mode */
        _graphics_SetDrMd(GfxBase, rp, old_drawmode);
    }
    else
    {
        /* No mask - just fill the rectangle */
        _graphics_RectFill(GfxBase, rp, (WORD)xMin, (WORD)yMin, (WORD)xMax, (WORD)yMax);
    }
}

static ULONG _graphics_ReadPixel ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD x __asm("d0"),
                                                        register WORD y __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: ReadPixel() rp=0x%08lx, x=%d, y=%d\n", (ULONG)rp, (int)x, (int)y);

    if (!rp || !rp->BitMap)
        return (ULONG)-1;  /* Error */

    struct BitMap *bm = rp->BitMap;
    WORD absX = x;
    WORD absY = y;

    /* If RastPort has a Layer, translate coordinates */
    if (rp->Layer)
    {
        struct Layer *layer = rp->Layer;
        absX = x + layer->bounds.MinX;
        absY = y + layer->bounds.MinY;
    }

    /* Bounds check */
    if (absX < 0 || absY < 0 || absX >= (bm->BytesPerRow * 8) || absY >= bm->Rows)
        return (ULONG)-1;

    UWORD byteOffset = absY * bm->BytesPerRow + (absX >> 3);
    UBYTE bitMask = 0x80 >> (absX & 7);
    ULONG color = 0;

    /* Read bits from each plane */
    for (UBYTE plane = 0; plane < bm->Depth; plane++)
    {
        if (bm->Planes[plane] && (bm->Planes[plane][byteOffset] & bitMask))
        {
            color |= (1 << plane);
        }
    }

    return color;
}

static LONG _graphics_WritePixel ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD x __asm("d0"),
                                                        register WORD y __asm("d1"))
{
    struct BitMap *bm;
    BYTE pen;
    UBYTE drawmode;
    WORD absX, absY;

    if (!rp || !rp->BitMap)
        return -1;  /* Error */

    bm = rp->BitMap;
    pen = rp->FgPen;
    drawmode = rp->DrawMode;

    /* Handle INVERSVID - use BgPen instead of FgPen */
    if (drawmode & INVERSVID)
    {
        pen = rp->BgPen;
    }

    /* If RastPort has a Layer, use ClipRect-aware drawing */
    if (rp->Layer)
    {
        struct Layer *layer = rp->Layer;
        struct ClipRect *cr;

        /* Convert to absolute screen coordinates */
        absX = x + layer->bounds.MinX;
        absY = y + layer->bounds.MinY;

        /* Bounds check against bitmap */
        if (absX < 0 || absY < 0 || absX >= (bm->BytesPerRow * 8) || absY >= bm->Rows)
            return -1;

        /* Check each ClipRect to see if the pixel is visible */
        for (cr = layer->ClipRect; cr != NULL; cr = cr->Next)
        {
            /* Skip obscured ClipRects (for LAYERSIMPLE) */
            if (cr->obscured)
                continue;

            /* Check if pixel falls within this ClipRect */
            if (absX >= cr->bounds.MinX && absX <= cr->bounds.MaxX &&
                absY >= cr->bounds.MinY && absY <= cr->bounds.MaxY)
            {
                /* Pixel is visible - draw it */
                SetPixelDirect(bm, absX, absY, pen, drawmode);
                return 0;  /* Success */
            }
        }

        /* Pixel not in any visible ClipRect - don't draw */
        return -1;
    }

    /* No layer - simple drawing with just bitmap bounds check */
    if (x < 0 || y < 0 || x >= (bm->BytesPerRow * 8) || y >= bm->Rows)
        return -1;

    SetPixelDirect(bm, x, y, pen, drawmode);
    return 0;  /* Success */
}

/* Flood fill helper structures and functions */
struct FloodFillInfo
{
    struct GfxBase *gfxbase;
    struct RastPort *rp;
    BYTE *rasptr;        /* TmpRas buffer (PLANEPTR) */
    ULONG bpr;           /* Bytes per row in tmpras */
    ULONG fillpen;       /* Pen to compare against */
    ULONG orig_apen;     /* Original APen */
    UWORD rp_width;
    UWORD rp_height;
    BOOL (*is_fillable)(struct FloodFillInfo *, LONG, LONG);
};

#define FLOOD_STACK_SIZE 100

struct FloodStack
{
    ULONG current;
    struct {
        LONG x, y;
    } items[FLOOD_STACK_SIZE];
};

static void SetTmpRasPixel(BYTE *rasptr, LONG x, LONG y, ULONG bpr, BOOL state)
{
    ULONG idx = COORD_TO_BYTEIDX(x, y, bpr);
    UBYTE mask = XCOORD_TO_MASK(x);
    
    if (state)
        rasptr[idx] |= mask;
    else
        rasptr[idx] &= ~mask;
}

static BOOL GetTmpRasPixel(BYTE *rasptr, LONG x, LONG y, ULONG bpr)
{
    ULONG idx = COORD_TO_BYTEIDX(x, y, bpr);
    UBYTE mask = XCOORD_TO_MASK(x);
    return ((rasptr[idx] & mask) != 0);
}

static BOOL FloodIsFillable_Outline(struct FloodFillInfo *fi, LONG x, LONG y)
{
    /* mode 0: fill until we hit a pixel with the outline pen color */
    if (x < 0 || y < 0 || x >= fi->rp_width || y >= fi->rp_height)
        return FALSE;
    
    if (GetTmpRasPixel(fi->rasptr, x, y, fi->bpr))
        return FALSE;  /* Already checked */
    
    return (fi->fillpen != _graphics_ReadPixel(fi->gfxbase, fi->rp, x, y));
}

static BOOL FloodIsFillable_Color(struct FloodFillInfo *fi, LONG x, LONG y)
{
    /* mode 1: fill while pixels have same color as starting pixel */
    if (x < 0 || y < 0 || x >= fi->rp_width || y >= fi->rp_height)
        return FALSE;
    
    if (GetTmpRasPixel(fi->rasptr, x, y, fi->bpr))
        return FALSE;  /* Already checked */
    
    return (fi->fillpen == _graphics_ReadPixel(fi->gfxbase, fi->rp, x, y));
}

static void FloodPutPixel(struct FloodFillInfo *fi, LONG x, LONG y)
{
    /* Write pixel using current APen */
    _graphics_WritePixel(fi->gfxbase, fi->rp, x, y);
    
    /* Mark as processed in tmpras */
    SetTmpRasPixel(fi->rasptr, x, y, fi->bpr, TRUE);
}

static void FloodInitStack(struct FloodStack *s)
{
    s->current = 0;
}

static BOOL FloodPush(struct FloodStack *s, LONG x, LONG y)
{
    if (s->current >= FLOOD_STACK_SIZE)
        return FALSE;
    
    s->items[s->current].x = x;
    s->items[s->current].y = y;
    s->current++;
    return TRUE;
}

static BOOL FloodPop(struct FloodStack *s, LONG *x, LONG *y)
{
    if (s->current == 0)
        return FALSE;
    
    s->current--;
    *x = s->items[s->current].x;
    *y = s->items[s->current].y;
    return TRUE;
}

static BOOL FloodFillScanline(struct FloodFillInfo *fi, LONG start_x, LONG start_y)
{
    LONG x;
    LONG rightmost_above, rightmost_below;
    LONG leftmost_above, leftmost_below;
    struct FloodStack stack;
    
    FloodInitStack(&stack);
    
    for (;;)
    {
        /* Scan right */
        rightmost_above = start_x;
        rightmost_below = start_x;
        
        for (x = start_x + 1; ; x++)
        {
            if (fi->is_fillable(fi, x, start_y))
            {
                FloodPutPixel(fi, x, start_y);
                
                /* Check above */
                if (x > rightmost_above)
                {
                    if (fi->is_fillable(fi, x, start_y - 1))
                    {
                        /* Find rightmost fillable pixel above */
                        for (rightmost_above = x; ; rightmost_above++)
                        {
                            if (!fi->is_fillable(fi, rightmost_above + 1, start_y - 1))
                                break;
                        }
                        
                        if (!FloodPush(&stack, rightmost_above, start_y - 1))
                            return FALSE;  /* Stack full */
                    }
                }
                
                /* Check below */
                if (x > rightmost_below)
                {
                    if (fi->is_fillable(fi, x, start_y + 1))
                    {
                        /* Find rightmost fillable pixel below */
                        for (rightmost_below = x; ; rightmost_below++)
                        {
                            if (!fi->is_fillable(fi, rightmost_below + 1, start_y + 1))
                                break;
                        }
                        
                        if (!FloodPush(&stack, rightmost_below, start_y + 1))
                            return FALSE;  /* Stack full */
                    }
                }
            }
            else
                break;
        }
        
        /* Scan left */
        leftmost_above = start_x + 1;
        leftmost_below = start_x + 1;
        
        for (x = start_x; ; x--)
        {
            if (fi->is_fillable(fi, x, start_y))
            {
                FloodPutPixel(fi, x, start_y);
                
                /* Check above */
                if (x <= leftmost_above)
                {
                    if (fi->is_fillable(fi, x, start_y - 1))
                    {
                        /* Find leftmost fillable pixel above */
                        for (leftmost_above = x; ; leftmost_above--)
                        {
                            if (!fi->is_fillable(fi, leftmost_above - 1, start_y - 1))
                                break;
                        }
                        
                        if (!FloodPush(&stack, leftmost_above, start_y - 1))
                            return FALSE;  /* Stack full */
                    }
                }
                
                /* Check below */
                if (x < leftmost_below)
                {
                    if (fi->is_fillable(fi, x, start_y + 1))
                    {
                        /* Find leftmost fillable pixel below */
                        for (leftmost_below = x; ; leftmost_below--)
                        {
                            if (!fi->is_fillable(fi, leftmost_below - 1, start_y + 1))
                                break;
                        }
                        
                        if (!FloodPush(&stack, leftmost_below, start_y + 1))
                            return FALSE;  /* Stack full */
                    }
                }
            }
            else
                break;
        }
        
        /* Pop next scanline from stack */
        if (!FloodPop(&stack, &start_x, &start_y))
            break;  /* Done */
    }
    
    return TRUE;
}

static BOOL _graphics_Flood ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register ULONG mode __asm("d2"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    struct TmpRas *tmpras;
    struct FloodFillInfo fi;
    ULONG bpr, needed_size;
    BOOL success;
    
    DPRINTF (LOG_DEBUG, "_graphics: Flood() rp=0x%08lx, mode=%lu, x=%ld, y=%ld\n", 
             (ULONG)rp, mode, x, y);
    
    if (!rp)
        return FALSE;
    
    /* Require TmpRas for flood fill */
    tmpras = rp->TmpRas;
    if (!tmpras || !tmpras->RasPtr)
    {
        DPRINTF(LOG_ERROR, "_graphics: Flood() requires TmpRas\n");
        return FALSE;
    }
    
    /* Get rastport dimensions */
    if (rp->Layer)
    {
        fi.rp_width = rp->Layer->Width;
        fi.rp_height = rp->Layer->Height;
    }
    else if (rp->BitMap)
    {
        fi.rp_width = rp->BitMap->BytesPerRow * 8;
        fi.rp_height = rp->BitMap->Rows;
    }
    else
    {
        DPRINTF(LOG_ERROR, "_graphics: Flood() no bitmap\n");
        return FALSE;
    }
    
    /* Check coordinates */
    if (x < 0 || y < 0 || x >= fi.rp_width || y >= fi.rp_height)
    {
        DPRINTF(LOG_DEBUG, "_graphics: Flood() coordinates out of bounds\n");
        return FALSE;
    }
    
    /* Calculate tmpras requirements */
    bpr = WIDTH_TO_BYTES(fi.rp_width);
    needed_size = bpr * fi.rp_height;
    
    if (tmpras->Size < needed_size)
    {
        DPRINTF(LOG_ERROR, "_graphics: Flood() tmpras too small (%lu < %lu)\n", 
                tmpras->Size, needed_size);
        return FALSE;
    }
    
    /* Clear tmpras */
    lxa_memset(tmpras->RasPtr, 0, needed_size);
    
    /* Setup fill info */
    fi.gfxbase = GfxBase;
    fi.rp = rp;
    fi.rasptr = tmpras->RasPtr;
    fi.bpr = bpr;
    fi.orig_apen = _graphics_GetAPen(GfxBase, rp);
    
    if (mode == 0)
    {
        /* Outline mode: fill until we hit outline pen */
        fi.fillpen = _graphics_GetOutlinePen(GfxBase, rp);
        fi.is_fillable = FloodIsFillable_Outline;
    }
    else
    {
        /* Color mode: fill while same color */
        fi.fillpen = _graphics_ReadPixel(GfxBase, rp, x, y);
        fi.is_fillable = FloodIsFillable_Color;
    }
    
    /* Do the flood fill */
    success = FloodFillScanline(&fi, x, y);
    
    /* Restore original APen */
    _graphics_SetAPen(GfxBase, rp, (UBYTE)fi.orig_apen);
    
    DPRINTF(LOG_DEBUG, "_graphics: Flood() completed %s\n", success ? "successfully" : "with errors");
    
    return success;
}

static VOID _graphics_PolyDraw ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG count __asm("d0"),
                                                        register CONST WORD * polyTable __asm("a0"))
{
    UWORD i;
    UWORD cnt;
    WORD x, y;

    DPRINTF (LOG_DEBUG, "_graphics: PolyDraw() rp=0x%08lx, count=%ld\n", (ULONG)rp, count);

    if (!rp || !polyTable)
        return;

    /* Only use low 16 bits of count (official ROM behavior) */
    cnt = (UWORD)count;

    for (i = 0; i < cnt; i++)
    {
        x = *polyTable++;
        y = *polyTable++;
        _graphics_Draw(GfxBase, rp, x, y);
    }
}

static VOID _graphics_SetAPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register UBYTE pen __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetAPen() rp=0x%08lx, pen=%u\n", (ULONG)rp, (unsigned)pen);

    if (rp)
    {
        rp->FgPen = (BYTE)pen;
    }
}

static VOID _graphics_SetBPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register UBYTE pen __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetBPen() rp=0x%08lx, pen=%u\n", (ULONG)rp, (unsigned)pen);

    if (rp)
    {
        rp->BgPen = (BYTE)pen;
    }
}

static VOID _graphics_SetDrMd ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register UBYTE drawMode __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetDrMd() rp=0x%08lx, drawMode=%u\n", (ULONG)rp, (unsigned)drawMode);

    if (rp)
    {
        rp->DrawMode = (BYTE)drawMode;
    }
}

static VOID _graphics_InitView ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a1"))
{
    /* Initialize a View structure to default values.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: InitView(view=%p)\n", view);
    
    if (!view)
        return;
    
    view->ViewPort = NULL;
    view->LOFCprList = NULL;
    view->SHFCprList = NULL;
    view->DyOffset = 0;
    view->DxOffset = 0;
    view->Modes = 0;
}

static VOID _graphics_CBump ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct UCopList * copList __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: CBump() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_CMove ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct UCopList * copList __asm("a1"),
                                                        register APTR destination __asm("d0"),
                                                        register LONG data __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: CMove() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_CWait ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct UCopList * copList __asm("a1"),
                                                        register LONG v __asm("d0"),
                                                        register LONG h __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: CWait() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _graphics_VBeamPos ( register struct GfxBase * GfxBase __asm("a6"))
{
    /* Return vertical beam position.
     * In emulation, we return a cycling value that simulates beam position.
     * Most applications use this for timing/sync purposes.
     */
    static LONG beam_pos = 0;
    
    /* Simulate a PAL display: 312 lines total */
    beam_pos = (beam_pos + 1) % 312;
    
    DPRINTF (LOG_DEBUG, "_graphics: VBeamPos() returning %ld\n", beam_pos);
    return beam_pos;
}

static VOID _graphics_InitBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct BitMap * bitMap __asm("a0"),
                                                        register BYTE depth __asm("d0"),
                                                        register WORD width __asm("d1"),
                                                        register WORD height __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_graphics: InitBitMap() bm=0x%08lx, depth=%d, width=%d, height=%d\n",
             (ULONG)bitMap, (int)depth, (int)width, (int)height);

    if (!bitMap)
        return;

    /* Calculate bytes per row (word-aligned) per AROS:
     * ((width + 15) >> 3) & ~0x1 is equivalent to ((width + 15) / 16) * 2 */
    UWORD bytesPerRow = (((UWORD)width + 15) >> 3) & ~0x1;

    bitMap->BytesPerRow = bytesPerRow;
    bitMap->Rows = (UWORD)height;
    bitMap->Flags = BMF_STANDARD;  /* Set BMF_STANDARD per AROS (was 0) */
    bitMap->Depth = (UBYTE)depth;
    bitMap->pad = 0;

    /* Note: Planes[] are NOT cleared - caller must set them up.
     * This matches AROS behavior where planes are left untouched. */
}

static VOID _graphics_ScrollRaster ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG dx __asm("d0"),
                                                        register LONG dy __asm("d1"),
                                                        register LONG xMin __asm("d2"),
                                                        register LONG yMin __asm("d3"),
                                                        register LONG xMax __asm("d4"),
                                                        register LONG yMax __asm("d5"))
{
    struct BitMap *bm;
    LONG srcX, srcY, destX, destY;
    LONG width, height;
    LONG clearX1, clearY1, clearX2, clearY2;
    UBYTE savedAPen;
    
    DPRINTF (LOG_DEBUG, "_graphics: ScrollRaster() rp=0x%08lx dx=%ld dy=%ld rect=(%ld,%ld)-(%ld,%ld)\n",
             (ULONG)rp, dx, dy, xMin, yMin, xMax, yMax);
    
    if (!rp || !rp->BitMap) {
        DPRINTF(LOG_ERROR, "_graphics: ScrollRaster() NULL rp or bitmap\n");
        return;
    }
    
    bm = rp->BitMap;
    
    /* Calculate the source and destination regions */
    width = xMax - xMin + 1;
    height = yMax - yMin + 1;
    
    /* If scroll amount exceeds the area, just clear the entire area */
    if (dx >= width || dx <= -width || dy >= height || dy <= -height) {
        /* Clear entire area */
        savedAPen = rp->FgPen;
        _graphics_SetAPen(GfxBase, rp, rp->BgPen);
        _graphics_RectFill(GfxBase, rp, xMin, yMin, xMax, yMax);
        _graphics_SetAPen(GfxBase, rp, savedAPen);
        return;
    }
    
    /* Calculate source and destination coordinates for the blit */
    /* Positive dx = scroll right (content moves left), negative dx = scroll left */
    /* Positive dy = scroll down (content moves up), negative dy = scroll up */
    
    if (dx > 0) {
        /* Content moves left (we're scrolling right) */
        srcX = xMin + dx;
        destX = xMin;
        width -= dx;
        clearX1 = xMax - dx + 1;
        clearX2 = xMax;
    } else if (dx < 0) {
        /* Content moves right (we're scrolling left) */
        srcX = xMin;
        destX = xMin - dx;
        width += dx;
        clearX1 = xMin;
        clearX2 = xMin - dx - 1;
    } else {
        srcX = xMin;
        destX = xMin;
        clearX1 = clearX2 = -1;  /* No horizontal clear needed */
    }
    
    if (dy > 0) {
        /* Content moves up (we're scrolling down) */
        srcY = yMin + dy;
        destY = yMin;
        height -= dy;
        clearY1 = yMax - dy + 1;
        clearY2 = yMax;
    } else if (dy < 0) {
        /* Content moves down (we're scrolling up) */
        srcY = yMin;
        destY = yMin - dy;
        height += dy;
        clearY1 = yMin;
        clearY2 = yMin - dy - 1;
    } else {
        srcY = yMin;
        destY = yMin;
        clearY1 = clearY2 = -1;  /* No vertical clear needed */
    }
    
    /* Perform the blit if there's any content to move */
    if (width > 0 && height > 0) {
        _graphics_BltBitMap(GfxBase, bm, srcX, srcY, bm, destX, destY, 
                           width, height, 0xC0, 0xFF, NULL);  /* 0xC0 = copy minterm */
    }
    
    /* Clear the exposed area with background pen */
    savedAPen = rp->FgPen;
    _graphics_SetAPen(GfxBase, rp, rp->BgPen);
    
    /* Clear horizontal strip if dx != 0 */
    if (dx != 0 && clearX1 >= xMin && clearX2 <= xMax) {
        _graphics_RectFill(GfxBase, rp, clearX1, yMin, clearX2, yMax);
    }
    
    /* Clear vertical strip if dy != 0 */
    if (dy != 0 && clearY1 >= yMin && clearY2 <= yMax) {
        _graphics_RectFill(GfxBase, rp, xMin, clearY1, xMax, clearY2);
    }
    
    _graphics_SetAPen(GfxBase, rp, savedAPen);
}

static VOID _graphics_WaitBOVP ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: WaitBOVP() unimplemented STUB called.\n");
    assert(FALSE);
}

static WORD _graphics_GetSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct SimpleSprite * sprite __asm("a0"),
                                                        register LONG num __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetSprite() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_FreeSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register LONG num __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeSprite() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_ChangeSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register struct SimpleSprite * sprite __asm("a1"),
                                                        register UWORD * newData __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: ChangeSprite() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_MoveSprite ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register struct SimpleSprite * sprite __asm("a1"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: MoveSprite() unimplemented STUB called.\n");
    assert(FALSE);
}

/* NOTE: These functions use __attribute__((optimize("O0"))) to work around
 * an internal compiler error in m68k-amigaos-gcc when DPRINTF is optimized out
 * and the function becomes effectively empty with a5/a6 register constraints. */

static VOID __attribute__((optimize("O0"))) _graphics_LockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a5"))
{
    DPRINTF (LOG_DEBUG, "_graphics: LockLayerRom() layer=0x%08lx\n", (ULONG)layer);

    /* LockLayerRom is a compatibility wrapper that calls layers.library LockLayer().
     * In lxa, layers.library functions are internal and can be called directly via emucall.
     * For now, we implement a simple no-op since layer locking is handled by layers.library
     * functions that already lock when needed. */
    
    /* Prevent empty function body when DPRINTF is disabled (GCC ICE workaround) */
    (void)GfxBase;
    (void)layer;
}

static VOID __attribute__((optimize("O0"))) _graphics_UnlockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a5"))
{
    DPRINTF (LOG_DEBUG, "_graphics: UnlockLayerRom() layer=0x%08lx\n", (ULONG)layer);

    /* UnlockLayerRom is a compatibility wrapper that calls layers.library UnlockLayer().
     * In lxa, layers.library functions are internal and can be called directly via emucall.
     * For now, we implement a simple no-op since layer locking is handled by layers.library
     * functions that already lock when needed. */
    
    /* Prevent empty function body when DPRINTF is disabled (GCC ICE workaround) */
    (void)GfxBase;
    (void)layer;
     
    /* TODO: If actual locking is needed, open layers.library and call UnlockLayer() */
}

static VOID _graphics_SyncSBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: SyncSBitMap() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_CopySBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: CopySBitMap() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_OwnBlitter ( register struct GfxBase * GfxBase __asm("a6"))
{
    /*
     * OwnBlitter() grants exclusive blitter access to the calling task.
     * In emulation we don't have real blitter hardware, so this is a no-op.
     * Apps typically call OwnBlitter/DisownBlitter around BltBitMap sequences.
     */
    DPRINTF (LOG_DEBUG, "_graphics: OwnBlitter() - no-op (no hardware blitter)\n");
}

static VOID _graphics_DisownBlitter ( register struct GfxBase * GfxBase __asm("a6"))
{
    /*
     * DisownBlitter() releases exclusive blitter access.
     * In emulation this is a no-op since we don't have real blitter hardware.
     */
    DPRINTF (LOG_DEBUG, "_graphics: DisownBlitter() - no-op (no hardware blitter)\n");
}

static struct TmpRas * _graphics_InitTmpRas ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TmpRas * tmpRas __asm("a0"),
                                                        register PLANEPTR buffer __asm("a1"),
                                                        register LONG size __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: InitTmpRas() tmpRas=0x%08lx buffer=0x%08lx size=%ld\n",
             (ULONG)tmpRas, (ULONG)buffer, size);

    if (!tmpRas)
        return NULL;

    /* Initialize TmpRas structure */
    tmpRas->RasPtr = (BYTE *)buffer;  /* Cast to BYTE * to match TmpRas.RasPtr type */
    tmpRas->Size = (ULONG)size;

    return tmpRas;
}

static VOID _graphics_AskFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register struct TextAttr * textAttr __asm("a0"))
{
    /* Query the attributes of the current font in a RastPort.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: AskFont(rp=%p, textAttr=%p)\n", rp, textAttr);
    
    if (!rp || !textAttr || !rp->Font)
        return;
    
    textAttr->ta_Name  = (STRPTR)rp->Font->tf_Message.mn_Node.ln_Name;
    textAttr->ta_YSize = rp->Font->tf_YSize;
    textAttr->ta_Style = rp->Font->tf_Style;
    textAttr->ta_Flags = rp->Font->tf_Flags;
}

static VOID _graphics_AddFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * textFont __asm("a1"))
{
    /* Add a font to the list of public fonts.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: AddFont(textFont=%p)\n", textFont);
    
    if (!textFont)
        return;
    
    textFont->tf_Message.mn_Node.ln_Type = NT_FONT;
    textFont->tf_Accessors = 0;
    textFont->tf_Flags &= ~FPF_REMOVED;
    
    Forbid();
    AddHead(&GfxBase->TextFonts, (struct Node *)textFont);
    Permit();
}

static VOID _graphics_RemFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * textFont __asm("a1"))
{
    /* Remove a font from the list of public fonts.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: RemFont(textFont=%p)\n", textFont);
    
    if (!textFont)
        return;
    
    Forbid();
    
    /* Only remove if not already removed */
    if (!(textFont->tf_Flags & FPF_REMOVED)) {
        textFont->tf_Flags |= FPF_REMOVED;
        Remove(&textFont->tf_Message.mn_Node);
    }
    
    Permit();
}

static PLANEPTR _graphics_AllocRaster ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register UWORD width __asm("d0"),
                                                        register UWORD height __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: AllocRaster() width=%u, height=%u\n", (unsigned)width, (unsigned)height);

    /* Calculate size using RASSIZE macro formula */
    ULONG size = (ULONG)height * ((((ULONG)width + 15) >> 3) & 0xFFFE);

    /* Allocate chip memory (MEMF_CHIP) and clear it */
    PLANEPTR raster = (PLANEPTR)AllocMem(size, MEMF_CHIP | MEMF_CLEAR);

    DPRINTF (LOG_DEBUG, "_graphics: AllocRaster() -> 0x%08lx (size=%lu)\n", (ULONG)raster, size);

    return raster;
}

static VOID _graphics_FreeRaster ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register PLANEPTR p __asm("a0"),
                                                        register UWORD width __asm("d0"),
                                                        register UWORD height __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: FreeRaster() p=0x%08lx, width=%u, height=%u\n",
             (ULONG)p, (unsigned)width, (unsigned)height);

    if (!p)
        return;

    /* Calculate size using RASSIZE macro formula */
    ULONG size = (ULONG)height * ((((ULONG)width + 15) >> 3) & 0xFFFE);

    FreeMem(p, size);
}

/*
 * Region management functions
 *
 * Regions are used to represent arbitrary collections of rectangles,
 * often used for damage tracking and clipping. The implementation uses
 * a list of RegionRectangles.
 */

/*
 * Helper: Allocate a new RegionRectangle
 */
static struct RegionRectangle *AllocRegionRectangle(void)
{
    struct RegionRectangle *rr = AllocMem(sizeof(struct RegionRectangle), MEMF_PUBLIC | MEMF_CLEAR);
    return rr;
}

/*
 * Helper: Free a RegionRectangle
 */
static void FreeRegionRectangle(struct RegionRectangle *rr)
{
    if (rr)
        FreeMem(rr, sizeof(struct RegionRectangle));
}

/*
 * Helper: Check if two rectangles overlap
 */
static BOOL RectanglesOverlap(const struct Rectangle *r1, const struct Rectangle *r2)
{
    return (r1->MinX <= r2->MaxX && r1->MaxX >= r2->MinX &&
            r1->MinY <= r2->MaxY && r1->MaxY >= r2->MinY);
}

/*
 * Helper: Check if rect1 fully contains rect2
 */
static BOOL RectangleContains(const struct Rectangle *outer, const struct Rectangle *inner)
{
    return (inner->MinX >= outer->MinX && inner->MaxX <= outer->MaxX &&
            inner->MinY >= outer->MinY && inner->MaxY <= outer->MaxY);
}

/*
 * Helper: Update region bounds after adding/removing rectangles
 */
static void UpdateRegionBounds(struct Region *region)
{
    struct RegionRectangle *rr = region->RegionRectangle;

    if (!rr)
    {
        region->bounds.MinX = 0;
        region->bounds.MinY = 0;
        region->bounds.MaxX = 0;
        region->bounds.MaxY = 0;
        return;
    }

    /* Start with first rectangle's bounds (relative to region->bounds, so convert) */
    region->bounds.MinX = rr->bounds.MinX;
    region->bounds.MinY = rr->bounds.MinY;
    region->bounds.MaxX = rr->bounds.MaxX;
    region->bounds.MaxY = rr->bounds.MaxY;

    /* Expand to include all rectangles */
    for (rr = rr->Next; rr; rr = rr->Next)
    {
        if (rr->bounds.MinX < region->bounds.MinX)
            region->bounds.MinX = rr->bounds.MinX;
        if (rr->bounds.MinY < region->bounds.MinY)
            region->bounds.MinY = rr->bounds.MinY;
        if (rr->bounds.MaxX > region->bounds.MaxX)
            region->bounds.MaxX = rr->bounds.MaxX;
        if (rr->bounds.MaxY > region->bounds.MaxY)
            region->bounds.MaxY = rr->bounds.MaxY;
    }
}

/*
 * NewRegion - Create a new empty Region (offset -516)
 */
static struct Region * _graphics_NewRegion ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF(LOG_DEBUG, "_graphics: NewRegion() called\n");

    struct Region *region = AllocMem(sizeof(struct Region), MEMF_PUBLIC | MEMF_CLEAR);
    if (!region)
        return NULL;

    region->bounds.MinX = 0;
    region->bounds.MinY = 0;
    region->bounds.MaxX = 0;
    region->bounds.MaxY = 0;
    region->RegionRectangle = NULL;

    return region;
}

/*
 * ClearRegion - Clear all rectangles from a region (offset -528)
 */
static VOID _graphics_ClearRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                    register struct Region * region __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_graphics: ClearRegion() called region=0x%08lx\n", (ULONG)region);

    if (!region)
        return;

    /* Free all RegionRectangles */
    struct RegionRectangle *rr = region->RegionRectangle;
    while (rr)
    {
        struct RegionRectangle *next = rr->Next;
        FreeRegionRectangle(rr);
        rr = next;
    }

    region->RegionRectangle = NULL;
    region->bounds.MinX = 0;
    region->bounds.MinY = 0;
    region->bounds.MaxX = 0;
    region->bounds.MaxY = 0;
}

/*
 * DisposeRegion - Free a region and all its rectangles (offset -534)
 */
static VOID _graphics_DisposeRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                      register struct Region * region __asm("a0"))
{
    DPRINTF(LOG_DEBUG, "_graphics: DisposeRegion() called region=0x%08lx\n", (ULONG)region);

    if (!region)
        return;

    /* Free all RegionRectangles first */
    _graphics_ClearRegion(GfxBase, region);

    /* Free the Region structure itself */
    FreeMem(region, sizeof(struct Region));
}

/*
 * OrRectRegion - Add a rectangle to a region (OR operation) (offset -510)
 *
 * Returns TRUE on success, FALSE on failure (out of memory)
 */
static BOOL _graphics_OrRectRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                     register struct Region * region __asm("a0"),
                                     register CONST struct Rectangle * rectangle __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: OrRectRegion() region=0x%08lx rect=[%d,%d]-[%d,%d]\n",
            (ULONG)region,
            rectangle ? rectangle->MinX : -1, rectangle ? rectangle->MinY : -1,
            rectangle ? rectangle->MaxX : -1, rectangle ? rectangle->MaxY : -1);

    if (!region || !rectangle)
        return FALSE;

    /* Validate rectangle */
    if (rectangle->MinX > rectangle->MaxX || rectangle->MinY > rectangle->MaxY)
        return TRUE;  /* Empty rectangle, nothing to do */

    /* Allocate new RegionRectangle */
    struct RegionRectangle *rr = AllocRegionRectangle();
    if (!rr)
        return FALSE;

    /* Copy rectangle bounds */
    rr->bounds = *rectangle;
    rr->Next = NULL;
    rr->Prev = NULL;

    /* Add to list (simple append for now - a full implementation would merge overlapping rects) */
    if (!region->RegionRectangle)
    {
        region->RegionRectangle = rr;
    }
    else
    {
        /* Find end of list */
        struct RegionRectangle *last = region->RegionRectangle;
        while (last->Next)
            last = last->Next;
        last->Next = rr;
        rr->Prev = last;
    }

    /* Update bounds */
    UpdateRegionBounds(region);

    return TRUE;
}

/*
 * AndRectRegion - Intersect region with a rectangle (AND operation) (offset -504)
 *
 * Removes all parts of the region that are outside the rectangle.
 */
static VOID _graphics_AndRectRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                      register struct Region * region __asm("a0"),
                                      register CONST struct Rectangle * rectangle __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: AndRectRegion() region=0x%08lx rect=[%d,%d]-[%d,%d]\n",
            (ULONG)region,
            rectangle ? rectangle->MinX : -1, rectangle ? rectangle->MinY : -1,
            rectangle ? rectangle->MaxX : -1, rectangle ? rectangle->MaxY : -1);

    if (!region || !rectangle)
        return;

    struct RegionRectangle *rr = region->RegionRectangle;
    struct RegionRectangle *prev = NULL;

    while (rr)
    {
        struct RegionRectangle *next = rr->Next;

        /* Check if rectangles overlap */
        if (RectanglesOverlap(&rr->bounds, rectangle))
        {
            /* Clip rr->bounds to rectangle */
            if (rr->bounds.MinX < rectangle->MinX)
                rr->bounds.MinX = rectangle->MinX;
            if (rr->bounds.MinY < rectangle->MinY)
                rr->bounds.MinY = rectangle->MinY;
            if (rr->bounds.MaxX > rectangle->MaxX)
                rr->bounds.MaxX = rectangle->MaxX;
            if (rr->bounds.MaxY > rectangle->MaxY)
                rr->bounds.MaxY = rectangle->MaxY;
            prev = rr;
        }
        else
        {
            /* Remove this rectangle - no overlap */
            if (prev)
                prev->Next = next;
            else
                region->RegionRectangle = next;

            if (next)
                next->Prev = prev;

            FreeRegionRectangle(rr);
        }
        rr = next;
    }

    /* Update bounds */
    UpdateRegionBounds(region);
}

/*
 * ClearRectRegion - Remove a rectangle from a region (XOR/subtract) (offset -522)
 *
 * This is more complex as it may split existing rectangles.
 * For simplicity, we implement a basic version that removes fully contained rects.
 * Returns TRUE on success, FALSE on failure.
 */
static BOOL _graphics_ClearRectRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                        register struct Region * region __asm("a0"),
                                        register CONST struct Rectangle * rectangle __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: ClearRectRegion() region=0x%08lx rect=[%d,%d]-[%d,%d]\n",
            (ULONG)region,
            rectangle ? rectangle->MinX : -1, rectangle ? rectangle->MinY : -1,
            rectangle ? rectangle->MaxX : -1, rectangle ? rectangle->MaxY : -1);

    if (!region || !rectangle)
        return FALSE;

    struct RegionRectangle *rr = region->RegionRectangle;
    struct RegionRectangle *prev = NULL;

    while (rr)
    {
        struct RegionRectangle *next = rr->Next;

        /* Check if this rect is fully contained in the clear rectangle */
        if (RectangleContains(rectangle, &rr->bounds))
        {
            /* Remove this rectangle entirely */
            if (prev)
                prev->Next = next;
            else
                region->RegionRectangle = next;

            if (next)
                next->Prev = prev;

            FreeRegionRectangle(rr);
        }
        else if (RectanglesOverlap(&rr->bounds, rectangle))
        {
            /* Partial overlap - would need to split the rectangle
             * For now, we just skip it (simplified implementation)
             * TODO: Implement proper rectangle splitting */
            prev = rr;
        }
        else
        {
            /* No overlap, keep rectangle */
            prev = rr;
        }
        rr = next;
    }

    /* Update bounds */
    UpdateRegionBounds(region);

    return TRUE;
}

static VOID _graphics_FreeVPortCopLists ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    /*
     * FreeVPortCopLists() frees intermediate copper lists for a ViewPort.
     * In lxa we don't use real copper lists (SDL-based rendering), so this is a no-op.
     * We just need to clear the pointers to indicate lists are freed.
     */
    DPRINTF(LOG_DEBUG, "_graphics: FreeVPortCopLists(vp=0x%08lx)\n", (ULONG)vp);
    
    if (vp)
    {
        /* Clear copper list pointers - they weren't really allocated */
        vp->DspIns = NULL;
        vp->SprIns = NULL;
        vp->ClrIns = NULL;
        vp->UCopIns = NULL;
    }
}

static VOID _graphics_FreeCopList ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct CopList * copList __asm("a0"))
{
    /*
     * FreeCopList() frees a CopList structure.
     * In lxa we don't use real copper lists (SDL-based rendering), so this is a no-op.
     */
    DPRINTF(LOG_DEBUG, "_graphics: FreeCopList(copList=0x%08lx) - no-op\n", (ULONG)copList);
    /* Nothing to free - we never allocated real copper lists */
}

static VOID _graphics_ClipBlit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * srcRP __asm("a0"),
                                                        register LONG xSrc __asm("d0"),
                                                        register LONG ySrc __asm("d1"),
                                                        register struct RastPort * destRP __asm("a1"),
                                                        register LONG xDest __asm("d2"),
                                                        register LONG yDest __asm("d3"),
                                                        register LONG xSize __asm("d4"),
                                                        register LONG ySize __asm("d5"),
                                                        register ULONG minterm __asm("d6"))
{
    DPRINTF (LOG_DEBUG, "_graphics: ClipBlit() srcRP=0x%08lx (%ld,%ld) destRP=0x%08lx (%ld,%ld) size=%ldx%ld mt=0x%02lx\n",
             (ULONG)srcRP, xSrc, ySrc, (ULONG)destRP, xDest, yDest, xSize, ySize, minterm);

    if (!srcRP || !destRP || !srcRP->BitMap || !destRP->BitMap)
    {
        LPRINTF (LOG_ERROR, "_graphics: ClipBlit() NULL pointer\n");
        return;
    }

    /* Handle layer locking if needed */
    if (srcRP->Layer)
        _graphics_LockLayerRom(GfxBase, srcRP->Layer);
    if (destRP->Layer && destRP->Layer != srcRP->Layer)
        _graphics_LockLayerRom(GfxBase, destRP->Layer);

    /* Simple implementation: just blit from source BitMap to dest BitMap */
    /* Adjust coordinates for layers if present */
    if (srcRP->Layer)
    {
        xSrc += srcRP->Layer->bounds.MinX;
        ySrc += srcRP->Layer->bounds.MinY;
    }
    if (destRP->Layer)
    {
        xDest += destRP->Layer->bounds.MinX;
        yDest += destRP->Layer->bounds.MinY;
    }

    /* Perform the blit */
    _graphics_BltBitMap(GfxBase, srcRP->BitMap, xSrc, ySrc,
                        destRP->BitMap, xDest, yDest,
                        xSize, ySize, minterm, 0xFF, NULL);

    /* Unlock layers */
    if (destRP->Layer && destRP->Layer != srcRP->Layer)
        _graphics_UnlockLayerRom(GfxBase, destRP->Layer);
    if (srcRP->Layer)
        _graphics_UnlockLayerRom(GfxBase, srcRP->Layer);
}

/*
 * XorRectRegion - XOR a rectangle with a region (offset -558)
 *
 * Returns TRUE on success, FALSE on failure.
 */
static BOOL _graphics_XorRectRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                      register struct Region * region __asm("a0"),
                                      register CONST struct Rectangle * rectangle __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: XorRectRegion() region=0x%08lx rect=[%d,%d]-[%d,%d]\n",
            (ULONG)region,
            rectangle ? rectangle->MinX : -1, rectangle ? rectangle->MinY : -1,
            rectangle ? rectangle->MaxX : -1, rectangle ? rectangle->MaxY : -1);

    if (!region || !rectangle)
        return FALSE;

    /* XOR is complex - for now, we implement a simple version:
     * If rectangle overlaps existing rects, remove the overlapping parts,
     * otherwise add the rectangle.
     * A full implementation would properly compute the symmetric difference.
     *
     * Simplified approach: Just add the rectangle (like OrRectRegion)
     * This is not a complete XOR but works for simple cases. */
    return _graphics_OrRectRegion(GfxBase, region, rectangle);
}

static VOID _graphics_FreeCprList ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct cprlist * cprList __asm("a0"))
{
    /*
     * FreeCprList() frees a cprlist structure (compiled copper list).
     * In lxa we don't use real copper lists (SDL-based rendering), so this is a no-op.
     */
    DPRINTF(LOG_DEBUG, "_graphics: FreeCprList(cprList=0x%08lx) - no-op\n", (ULONG)cprList);
    /* Nothing to free - we never allocated real copper lists */
}

static struct ColorMap * _graphics_GetColorMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register LONG entries __asm("d0"))
{
    /* Allocate and initialize a ColorMap structure.
     * Based on AROS implementation.
     */
    struct ColorMap *cm;
    UWORD *colorTable, *lowColorBits;
    
    DPRINTF (LOG_DEBUG, "_graphics: GetColorMap(entries=%ld)\n", entries);
    
    if (entries <= 0)
        return NULL;
    
    /* Allocate the ColorMap structure */
    cm = AllocMem(sizeof(struct ColorMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!cm)
        return NULL;
    
    /* Allocate the ColorTable */
    colorTable = AllocMem(entries * sizeof(UWORD), MEMF_PUBLIC | MEMF_CLEAR);
    if (!colorTable) {
        FreeMem(cm, sizeof(struct ColorMap));
        return NULL;
    }
    
    /* Allocate LowColorBits */
    lowColorBits = AllocMem(entries * sizeof(UWORD), MEMF_PUBLIC | MEMF_CLEAR);
    if (!lowColorBits) {
        FreeMem(colorTable, entries * sizeof(UWORD));
        FreeMem(cm, sizeof(struct ColorMap));
        return NULL;
    }
    
    /* Initialize the ColorMap */
    cm->Type = COLORMAP_TYPE_V39;
    cm->Count = entries;
    cm->ColorTable = colorTable;
    cm->LowColorBits = lowColorBits;
    cm->SpriteResolution = SPRITERESN_DEFAULT;
    cm->SpriteResDefault = SPRITERESN_ECS;
    cm->VPModeID = (ULONG)-1;
    cm->SpriteBase_Even = 0x0001;
    cm->SpriteBase_Odd = 0x0001;
    cm->Bp_1_base = 0x0008;
    
    /* Initialize with default Amiga Workbench colors for first 4 entries */
    if (entries > 0) colorTable[0] = 0x0AAA;  /* Light gray background */
    if (entries > 1) colorTable[1] = 0x0000;  /* Black text */
    if (entries > 2) colorTable[2] = 0x0FFF;  /* White */
    if (entries > 3) colorTable[3] = 0x068B;  /* Blue highlights */
    
    return cm;
}

static VOID _graphics_FreeColorMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * colorMap __asm("a0"))
{
    /* Free a ColorMap structure and all associated memory.
     * Based on AROS implementation.
     */
    DPRINTF (LOG_DEBUG, "_graphics: FreeColorMap(cm=%p)\n", colorMap);
    
    if (!colorMap)
        return;
    
    /* Free the ColorTable */
    if (colorMap->ColorTable)
        FreeMem(colorMap->ColorTable, colorMap->Count * sizeof(UWORD));
    
    /* Free the LowColorBits */
    if (colorMap->LowColorBits)
        FreeMem(colorMap->LowColorBits, colorMap->Count * sizeof(UWORD));
    
    /* Free the ColorMap structure itself */
    FreeMem(colorMap, sizeof(struct ColorMap));
}

static ULONG _graphics_GetRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * colorMap __asm("a0"),
                                                        register LONG entry __asm("d0"))
{
    /* Read a color value from the ColorMap.
     * Returns 12-bit RGB (4 bits per gun).
     * Based on AROS implementation.
     */
    UWORD *ct;
    
    /* Validate parameters */
    if (!colorMap || entry < 0 || entry >= colorMap->Count)
        return (ULONG)-1;
    
    ct = colorMap->ColorTable;
    if (!ct)
        return (ULONG)-1;
    
    return ct[entry];
}

static VOID _graphics_ScrollVPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: ScrollVPort() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct CopList * _graphics_UCopperListInit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct UCopList * uCopList __asm("a0"),
                                                        register LONG n __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: UCopperListInit() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_FreeGBuffers ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AnimOb * anOb __asm("a0"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG flag __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeGBuffers() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_BltBitMapRastPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct BitMap * srcBitMap __asm("a0"),
                                                        register LONG xSrc __asm("d0"),
                                                        register LONG ySrc __asm("d1"),
                                                        register struct RastPort * destRP __asm("a1"),
                                                        register LONG xDest __asm("d2"),
                                                        register LONG yDest __asm("d3"),
                                                        register LONG xSize __asm("d4"),
                                                        register LONG ySize __asm("d5"),
                                                        register ULONG minterm __asm("d6"))
{
    DPRINTF (LOG_DEBUG, "_graphics: BltBitMapRastPort() src=0x%08lx (%ld,%ld) destRP=0x%08lx (%ld,%ld) size=%ldx%ld minterm=0x%02lx\n",
             (ULONG)srcBitMap, xSrc, ySrc, (ULONG)destRP, xDest, yDest, xSize, ySize, minterm);

    if (!srcBitMap || !destRP || !destRP->BitMap)
    {
        LPRINTF (LOG_ERROR, "_graphics: BltBitMapRastPort() NULL pointer\n");
        return;
    }

    /* If RastPort has a Layer, add layer offset for coordinate translation */
    if (destRP->Layer)
    {
        xDest += destRP->Layer->bounds.MinX;
        yDest += destRP->Layer->bounds.MinY;
    }

    /* BltBitMapRastPort is a wrapper around BltBitMap.
     * It blits from a source BitMap to the RastPort's BitMap.
     * The mask is 0xFF (all planes) and tempA is NULL. */
    _graphics_BltBitMap(GfxBase, srcBitMap, xSrc, ySrc,
                        destRP->BitMap, xDest, yDest,
                        xSize, ySize, minterm, 0xFF, NULL);
}

/*
 * OrRegionRegion - OR two regions together (offset -612)
 *
 * Adds all rectangles from srcRegion to destRegion.
 * Returns TRUE on success, FALSE on failure.
 */
static BOOL _graphics_OrRegionRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                       register CONST struct Region * srcRegion __asm("a0"),
                                       register struct Region * destRegion __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: OrRegionRegion() src=0x%08lx dest=0x%08lx\n",
            (ULONG)srcRegion, (ULONG)destRegion);

    if (!destRegion)
        return FALSE;

    if (!srcRegion || !srcRegion->RegionRectangle)
        return TRUE;  /* Nothing to add */

    /* Add each rectangle from src to dest */
    struct RegionRectangle *rr = srcRegion->RegionRectangle;
    while (rr)
    {
        if (!_graphics_OrRectRegion(GfxBase, destRegion, &rr->bounds))
            return FALSE;
        rr = rr->Next;
    }

    return TRUE;
}

/*
 * XorRegionRegion - XOR two regions together (offset -618)
 *
 * Returns TRUE on success, FALSE on failure.
 */
static BOOL _graphics_XorRegionRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                        register CONST struct Region * srcRegion __asm("a0"),
                                        register struct Region * destRegion __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: XorRegionRegion() src=0x%08lx dest=0x%08lx\n",
            (ULONG)srcRegion, (ULONG)destRegion);

    if (!destRegion)
        return FALSE;

    if (!srcRegion || !srcRegion->RegionRectangle)
        return TRUE;  /* Nothing to XOR */

    /* Simplified: XOR each rectangle from src to dest */
    struct RegionRectangle *rr = srcRegion->RegionRectangle;
    while (rr)
    {
        if (!_graphics_XorRectRegion(GfxBase, destRegion, &rr->bounds))
            return FALSE;
        rr = rr->Next;
    }

    return TRUE;
}

/*
 * AndRegionRegion - AND two regions together (offset -624)
 *
 * Keeps only the intersection of the two regions in destRegion.
 * Returns TRUE on success, FALSE on failure.
 */
static BOOL _graphics_AndRegionRegion ( register struct GfxBase * GfxBase __asm("a6"),
                                        register CONST struct Region * srcRegion __asm("a0"),
                                        register struct Region * destRegion __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_graphics: AndRegionRegion() src=0x%08lx dest=0x%08lx\n",
            (ULONG)srcRegion, (ULONG)destRegion);

    if (!destRegion)
        return FALSE;

    if (!srcRegion || !srcRegion->RegionRectangle)
    {
        /* AND with empty = empty */
        _graphics_ClearRegion(GfxBase, destRegion);
        return TRUE;
    }

    /* For each rectangle in dest, intersect with the entire src region
     * This is a simplified implementation that ANDs with the src bounds only */
    _graphics_AndRectRegion(GfxBase, destRegion, &srcRegion->bounds);

    return TRUE;
}

static VOID _graphics_SetRGB4CM ( register struct GfxBase * GfxBase __asm("a6"),
                                  register struct ColorMap * colorMap __asm("a0"),
                                  register LONG index __asm("d0"),
                                  register ULONG red __asm("d1"),
                                  register ULONG green __asm("d2"),
                                  register ULONG blue __asm("d3"))
{
    /* Set a color value in the ColorMap.
     * Each color is stored as 12-bit RGB (4 bits per gun).
     * Red, green, blue should be 0-15 (4-bit values).
     */
    UWORD *ct;
    
    DPRINTF(LOG_DEBUG, "_graphics: SetRGB4CM(cm=%p, index=%ld, r=%ld, g=%ld, b=%ld)\n",
            colorMap, index, red, green, blue);
    
    /* Validate parameters */
    if (!colorMap || index < 0 || index >= colorMap->Count)
        return;
    
    ct = colorMap->ColorTable;
    if (!ct)
        return;
    
    /* Pack RGB into 12-bit format: 0x0RGB */
    ct[index] = ((red & 0xF) << 8) | ((green & 0xF) << 4) | (blue & 0xF);
}

static VOID _graphics_BltMaskBitMapRastPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct BitMap * srcBitMap __asm("a0"),
                                                        register LONG xSrc __asm("d0"),
                                                        register LONG ySrc __asm("d1"),
                                                        register struct RastPort * destRP __asm("a1"),
                                                        register LONG xDest __asm("d2"),
                                                        register LONG yDest __asm("d3"),
                                                        register LONG xSize __asm("d4"),
                                                        register LONG ySize __asm("d5"),
                                                        register ULONG minterm __asm("d6"),
                                                        register CONST PLANEPTR bltMask __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_graphics: BltMaskBitMapRastPort() src=0x%08lx (%ld,%ld) destRP=0x%08lx (%ld,%ld) size=%ldx%ld mt=0x%02lx mask=0x%08lx\n",
             (ULONG)srcBitMap, xSrc, ySrc, (ULONG)destRP, xDest, yDest, xSize, ySize, minterm, (ULONG)bltMask);

    if (!srcBitMap || !destRP || !destRP->BitMap)
    {
        LPRINTF (LOG_ERROR, "_graphics: BltMaskBitMapRastPort() NULL pointer\n");
        return;
    }

    /* Adjust destination coordinates for layer offset if present */
    if (destRP->Layer)
    {
        xDest += destRP->Layer->bounds.MinX;
        yDest += destRP->Layer->bounds.MinY;
    }

    /* BltMaskBitMapRastPort blits from a source BitMap to the RastPort's BitMap
     * using a mask plane. The mask determines which pixels to blit.
     * Pass the mask to BltBitMap which already supports masking. */
    _graphics_BltBitMap(GfxBase, srcBitMap, xSrc, ySrc,
                        destRP->BitMap, xDest, yDest,
                        xSize, ySize, minterm, 0xFF, bltMask);
}

static VOID _graphics_private0 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private0() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_private1 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private1() unimplemented STUB called.\n");
    assert(FALSE);
}

static BOOL __attribute__((optimize("O0"))) _graphics_AttemptLockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
                                            register struct Layer * layer __asm("a5"))
{
    /* Attempt to lock a layer without blocking.
     * In our single-threaded emulation, this always succeeds.
     * Based on LockLayerRom which is already a no-op.
     */
    DPRINTF (LOG_DEBUG, "_graphics: AttemptLockLayerRom(layer=%p)\n", layer);
    
    /* No actual locking needed in emulation - always succeeds */
    (void)GfxBase;
    return TRUE;
}

static APTR _graphics_GfxNew ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG gfxNodeType __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GfxNew() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_GfxFree ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register APTR gfxNodePtr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GfxFree() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_GfxAssociate ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST APTR associateNode __asm("a0"),
                                                        register APTR gfxNodePtr __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: GfxAssociate() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_BitMapScale ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct BitScaleArgs * bsa __asm("a0"))
{
    UWORD *linePattern;
    UWORD destWidth, destHeight;
    UWORD srcX, srcY, destX, destY;
    UWORD srcWidth, srcHeight;
    struct BitMap *srcBM, *destBM;
    UWORD y, x, plane;
    
    DPRINTF (LOG_DEBUG, "_graphics: BitMapScale() bsa=0x%08lx\n", (ULONG)bsa);
    
    if (!bsa || !bsa->bsa_SrcBitMap || !bsa->bsa_DestBitMap)
        return;
    
    srcBM = bsa->bsa_SrcBitMap;
    destBM = bsa->bsa_DestBitMap;
    srcX = bsa->bsa_SrcX;
    srcY = bsa->bsa_SrcY;
    srcWidth = bsa->bsa_SrcWidth;
    srcHeight = bsa->bsa_SrcHeight;
    destX = bsa->bsa_DestX;
    destY = bsa->bsa_DestY;
    
    /* Calculate destination dimensions using scale factors */
    destWidth = _graphics_ScalerDiv(GfxBase, srcWidth, 
                                     bsa->bsa_XDestFactor, bsa->bsa_XSrcFactor);
    destHeight = _graphics_ScalerDiv(GfxBase, srcHeight, 
                                      bsa->bsa_YDestFactor, bsa->bsa_YSrcFactor);
    
    /* Store calculated results back in structure */
    bsa->bsa_DestWidth = destWidth;
    bsa->bsa_DestHeight = destHeight;
    
    if (destWidth == 0 || destHeight == 0)
        return;
    
    /* Allocate line pattern buffer to precalculate Y mapping */
    linePattern = (UWORD *)AllocMem(sizeof(UWORD) * destHeight, 0);
    if (!linePattern)
        return;
    
    /* Precalculate which source Y line maps to each destination Y line */
    {
        ULONG count = 0;
        UWORD ys = srcY;
        ULONG dyd = destHeight;
        ULONG dys = srcHeight;
        LONG accuys = dyd;
        LONG accuyd = -(dys >> 1);
        
        while (count < destHeight) {
            accuyd += dys;
            while (accuyd > accuys) {
                ys++;
                accuys += dyd;
            }
            linePattern[count] = ys;
            count++;
        }
    }
    
    /* Now perform the actual scaling, pixel by pixel */
    /* Using simple nearest-neighbor algorithm */
    {
        UWORD minDepth = (srcBM->Depth < destBM->Depth) ? srcBM->Depth : destBM->Depth;
        ULONG dxd = destWidth;
        ULONG dxs = srcWidth;
        
        for (y = 0; y < destHeight; y++) {
            UWORD sourceY = linePattern[y];
            UWORD xs = srcX;
            LONG accuxs = dxd;
            LONG accuxd = -(dxs >> 1);
            
            for (x = 0; x < destWidth; x++) {
                /* Calculate source X for this destination X */
                accuxd += dxs;
                while (accuxd > accuxs) {
                    xs++;
                    accuxs += dxd;
                }
                
                /* Copy pixel from source to destination for each plane */
                for (plane = 0; plane < minDepth; plane++) {
                    UBYTE *srcPlane = srcBM->Planes[plane];
                    UBYTE *destPlane = destBM->Planes[plane];
                    ULONG srcByteIdx, destByteIdx;
                    UBYTE srcBit, destBit;
                    
                    if (!srcPlane || !destPlane)
                        continue;
                    
                    /* Calculate source byte and bit */
                    srcByteIdx = sourceY * srcBM->BytesPerRow + (xs >> 3);
                    srcBit = 0x80 >> (xs & 7);
                    
                    /* Calculate destination byte and bit */
                    destByteIdx = (destY + y) * destBM->BytesPerRow + ((destX + x) >> 3);
                    destBit = 0x80 >> ((destX + x) & 7);
                    
                    /* Copy the pixel value */
                    if (srcPlane[srcByteIdx] & srcBit)
                        destPlane[destByteIdx] |= destBit;
                    else
                        destPlane[destByteIdx] &= ~destBit;
                }
            }
        }
    }
    
    FreeMem(linePattern, sizeof(UWORD) * destHeight);
}

static UWORD _graphics_ScalerDiv ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG factor __asm("d0"),
                                                        register ULONG numerator __asm("d1"),
                                                        register ULONG denominator __asm("d2"))
{
    ULONG res;
    
    DPRINTF (LOG_DEBUG, "_graphics: ScalerDiv() factor=%lu, num=%lu, denom=%lu\n",
             factor, numerator, denominator);
    
    /* Handle edge cases */
    if (factor == 0 || numerator == 0 || denominator == 0)
        return 0;
    
    /* Calculate scaled result: (factor * numerator) / denominator */
    res = (factor * numerator) / denominator;
    
    /* Minimum result is 1 if inputs were non-zero */
    if (res == 0)
        return 1;
    
    /* Round up if remainder >= half denominator */
    if (((factor * numerator) % denominator) >= ((denominator + 1) >> 1))
        res++;
    
    return (UWORD)res;
}

static WORD _graphics_TextExtent ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register LONG count __asm("d0"),
                                                        register struct TextExtent * textExtent __asm("a2"))
{
    struct TextFont *tf;
    WORD width;

    DPRINTF (LOG_DEBUG, "_graphics: TextExtent() rp=0x%08lx, string='%s', count=%ld\n",
             (ULONG)rp, string ? (char *)string : "(null)", count);

    if (!rp || !textExtent)
    {
        return 0;
    }

    /* Get the font from the RastPort, or use default */
    tf = rp->Font;
    if (!tf)
    {
        tf = get_default_font();
    }

    /* Calculate text width using TextLength */
    width = _graphics_TextLength(GfxBase, rp, string, (UWORD)count);
    
    textExtent->te_Width = width;
    textExtent->te_Height = tf->tf_YSize;
    textExtent->te_Extent.MinY = -tf->tf_Baseline;
    textExtent->te_Extent.MaxY = textExtent->te_Height - 1 - tf->tf_Baseline;

    /* For fixed-width fonts (like Topaz-8), MinX is 0 and MaxX is width-1 */
    /* TODO: Handle proportional fonts with kerning/spacing tables */
    textExtent->te_Extent.MinX = 0;
    textExtent->te_Extent.MaxX = (width > 0) ? (width - 1) : 0;

    /* Handle bold style - adds smear to right side */
    if (rp->AlgoStyle & FSF_BOLD)
    {
        textExtent->te_Extent.MaxX += tf->tf_BoldSmear;
    }

    /* Handle italic style - shears the text */
    if (rp->AlgoStyle & FSF_ITALIC)
    {
        /* Italic shifts top right and bottom left */
        textExtent->te_Extent.MaxX += tf->tf_Baseline / 2;
        textExtent->te_Extent.MinX -= (tf->tf_YSize - tf->tf_Baseline) / 2;
    }

    DPRINTF (LOG_DEBUG, "_graphics: TextExtent() -> width=%d height=%d extent=(%d,%d)-(%d,%d)\n",
             textExtent->te_Width, textExtent->te_Height,
             textExtent->te_Extent.MinX, textExtent->te_Extent.MinY,
             textExtent->te_Extent.MaxX, textExtent->te_Extent.MaxY);

    return width;
}

static ULONG _graphics_TextFit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register ULONG strLen __asm("d0"),
                                                        register struct TextExtent * textExtent __asm("a2"),
                                                        register CONST struct TextExtent * constrainingExtent __asm("a3"),
                                                        register LONG strDirection __asm("d1"),
                                                        register ULONG constrainingBitWidth __asm("d2"),
                                                        register ULONG constrainingBitHeight __asm("d3"))
{
    struct TextFont *tf;
    ULONG retval = 0;

    DPRINTF (LOG_DEBUG, "_graphics: TextFit() rp=0x%08lx, strLen=%lu, dir=%ld, w=%lu, h=%lu\n",
             (ULONG)rp, strLen, strDirection, constrainingBitWidth, constrainingBitHeight);

    if (!rp || !textExtent)
    {
        return 0;
    }

    /* Get the font from the RastPort, or use default */
    tf = rp->Font;
    if (!tf)
    {
        tf = get_default_font();
    }

    /* Check if height constraint allows at least one line of text */
    if (strLen && (constrainingBitHeight >= tf->tf_YSize))
    {
        BOOL ok = TRUE;

        /* Initialize textExtent with font metrics */
        textExtent->te_Extent.MinX = 0;
        textExtent->te_Extent.MinY = -tf->tf_Baseline;
        textExtent->te_Extent.MaxX = 0;
        textExtent->te_Extent.MaxY = tf->tf_YSize - tf->tf_Baseline - 1;
        textExtent->te_Width       = 0;
        textExtent->te_Height      = tf->tf_YSize;

        /* Check if constrainingExtent allows our font height */
        if (constrainingExtent)
        {
            if (constrainingExtent->te_Extent.MinY > textExtent->te_Extent.MinY ||
                constrainingExtent->te_Extent.MaxY < textExtent->te_Extent.MaxY ||
                constrainingExtent->te_Height < textExtent->te_Height)
            {
                ok = FALSE;
            }
        }

        if (ok)
        {
            /* Try to fit characters one by one */
            while (strLen--)
            {
                struct TextExtent char_extent;
                WORD newwidth, newminx, newmaxx, minx, maxx;

                /* Get extent for this single character */
                _graphics_TextExtent(GfxBase, rp, string, 1, &char_extent);
                string += strDirection;

                /* Calculate new dimensions if we include this character */
                newwidth = textExtent->te_Width + char_extent.te_Width;
                minx = textExtent->te_Width + char_extent.te_Extent.MinX;
                maxx = textExtent->te_Width + char_extent.te_Extent.MaxX;

                newminx = (minx < textExtent->te_Extent.MinX) ?
                    minx : textExtent->te_Extent.MinX;
                newmaxx = (maxx > textExtent->te_Extent.MaxX) ?
                    maxx : textExtent->te_Extent.MaxX;

                /* Check if new character exceeds width constraint */
                if ((ULONG)(newmaxx - newminx + 1) > constrainingBitWidth)
                    break;

                /* Check constrainingExtent constraints */
                if (constrainingExtent)
                {
                    if (constrainingExtent->te_Extent.MinX > newminx) break;
                    if (constrainingExtent->te_Extent.MaxX < newmaxx) break;
                    if (constrainingExtent->te_Width < newwidth) break;
                }

                /* Character fits, update textExtent */
                textExtent->te_Width = newwidth;
                textExtent->te_Extent.MinX = newminx;
                textExtent->te_Extent.MaxX = newmaxx;

                retval++;
            }
        }
    }

    /* If no characters fit, zero out the extent */
    if (retval == 0)
    {
        textExtent->te_Width = 0;
        textExtent->te_Height = 0;
        textExtent->te_Extent.MinX = 0;
        textExtent->te_Extent.MinY = 0;
        textExtent->te_Extent.MaxX = 0;
        textExtent->te_Extent.MaxY = 0;
    }

    DPRINTF (LOG_DEBUG, "_graphics: TextFit() -> %lu chars fit\n", retval);

    return retval;
}

static APTR _graphics_GfxLookUp ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST APTR associateNode __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GfxLookUp() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static BOOL _graphics_VideoControl ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * colorMap __asm("a0"),
                                                        register struct TagItem * tagarray __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: VideoControl() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

static struct MonitorSpec * _graphics_OpenMonitor ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST_STRPTR monitorName __asm("a1"),
                                                        register ULONG displayID __asm("d0"))
{
    /* 
     * OpenMonitor() opens a MonitorSpec for a given display mode.
     * In lxa emulation, we don't have real hardware monitors, so we return NULL
     * indicating the requested monitor is not available.
     * Apps should handle NULL return gracefully.
     */
    DPRINTF (LOG_DEBUG, "_graphics: OpenMonitor() monitorName='%s' displayID=0x%08lx - returning NULL (not available)\n",
             monitorName ? (char *)monitorName : "(null)", displayID);
    return NULL;
}

static BOOL _graphics_CloseMonitor ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct MonitorSpec * monitorSpec __asm("a0"))
{
    /*
     * CloseMonitor() closes a MonitorSpec opened by OpenMonitor().
     * Since we never return real MonitorSpecs, just return TRUE.
     */
    DPRINTF (LOG_DEBUG, "_graphics: CloseMonitor() monitorSpec=0x%08lx\n", (ULONG)monitorSpec);
    return TRUE;
}

static DisplayInfoHandle _graphics_FindDisplayInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG displayID __asm("d0"))
{
    /*
     * FindDisplayInfo() finds a display info handle for a given display ID.
     * We return a simple non-NULL handle for common PAL/NTSC modes that 
     * GetDisplayInfoData can recognize, NULL for unknown modes.
     */
    DPRINTF (LOG_DEBUG, "_graphics: FindDisplayInfo() displayID=0x%08lx\n", displayID);
    
    /* Return displayID+1 as handle for known modes (non-zero = valid handle) */
    /* This allows GetDisplayInfoData to retrieve the displayID from handle-1 */
    switch (displayID & 0xFFFF0000) {
        case 0x00000000:  /* LORES/HIRES modes */
        case 0x00010000:  /* NTSC monitors */
        case 0x00020000:  /* PAL monitors */
            return (DisplayInfoHandle)(displayID + 1);
        default:
            return (DisplayInfoHandle)0;  /* Unknown mode */
    }
}

static ULONG _graphics_NextDisplayInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG displayID __asm("d0"))
{
    /*
     * NextDisplayInfo() iterates through available display modes.
     * Returns INVALID_ID (0xFFFFFFFF) when done.
     * We just provide a minimal set: LORES and HIRES.
     */
    DPRINTF (LOG_DEBUG, "_graphics: NextDisplayInfo() displayID=0x%08lx\n", displayID);
    
    if (displayID == 0xFFFFFFFF) {
        /* First call - return LORES */
        return 0x00000000;  /* LORES_KEY */
    } else if (displayID == 0x00000000) {
        /* Return HIRES */
        return 0x00008000;  /* HIRES_KEY */
    }
    
    /* Done */
    return 0xFFFFFFFF;
}

static VOID _graphics_private2 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private2() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_private3 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private3() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_private4 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private4() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_GetDisplayInfoData ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST DisplayInfoHandle handle __asm("a0"),
                                                        register APTR buf __asm("a1"),
                                                        register ULONG size __asm("d0"),
                                                        register ULONG tagID __asm("d1"),
                                                        register ULONG displayID __asm("d2"))
{
    /*
     * GetDisplayInfoData() retrieves information about a display mode.
     * Returns the number of bytes copied, or 0 on failure.
     * We provide basic info for LORES/HIRES modes.
     */
    ULONG actualDisplayID;
    
    DPRINTF (LOG_DEBUG, "_graphics: GetDisplayInfoData() handle=0x%08lx buf=0x%08lx size=%lu tagID=0x%08lx displayID=0x%08lx\n",
             (ULONG)handle, (ULONG)buf, size, tagID, displayID);
    
    if (!buf || size == 0)
        return 0;
    
    /* Get actual display ID - either from parameter or from handle */
    if (displayID != 0 && displayID != 0xFFFFFFFF) {
        actualDisplayID = displayID;
    } else if (handle) {
        actualDisplayID = ((ULONG)handle) - 1;  /* Handle is displayID+1 */
    } else {
        return 0;
    }
    
    /* Clear buffer */
    lxa_memset(buf, 0, size);
    
    switch (tagID) {
        case 0x80000000: /* DTAG_DISP - DisplayInfo */
        {
            /* Provide minimal DisplayInfo */
            struct {
                ULONG StructID;
                ULONG DisplayID;
                ULONG SkipID;
                ULONG Length;
                UWORD NotAvailable;
                ULONG PropertyFlags;
            } *di = (void *)buf;
            
            ULONG copySize = (size < 24) ? size : 24;
            
            di->StructID = tagID;
            di->DisplayID = actualDisplayID;
            di->SkipID = 0;
            di->Length = 2;  /* 2 double-longwords after header */
            di->NotAvailable = 0;  /* Available */
            di->PropertyFlags = 0x00000340;  /* DIPF_IS_WB | DIPF_IS_DRAGGABLE | DIPF_IS_SPRITES */
            
            if (actualDisplayID & 0x00008000) {
                /* HIRES mode */
                di->PropertyFlags |= 0;  /* No extra flags for HIRES */
            }
            
            return copySize;
        }
        
        case 0x80001000: /* DTAG_DIMS - DimensionInfo */
        {
            /* Provide minimal DimensionInfo */
            struct {
                ULONG StructID;
                ULONG DisplayID;
                ULONG SkipID;
                ULONG Length;
                UWORD MaxDepth;
                UWORD MinRasterWidth;
                UWORD MinRasterHeight;
                UWORD MaxRasterWidth;
                UWORD MaxRasterHeight;
            } *dims = (void *)buf;
            
            ULONG copySize = (size < 28) ? size : 28;
            
            dims->StructID = tagID;
            dims->DisplayID = actualDisplayID;
            dims->SkipID = 0;
            dims->Length = 3;
            dims->MaxDepth = 8;  /* 256 colors max */
            
            if (actualDisplayID & 0x00008000) {
                /* HIRES mode */
                dims->MinRasterWidth = 640;
                dims->MinRasterHeight = 200;
                dims->MaxRasterWidth = 1280;
                dims->MaxRasterHeight = 512;
            } else {
                /* LORES mode */
                dims->MinRasterWidth = 320;
                dims->MinRasterHeight = 200;
                dims->MaxRasterWidth = 704;
                dims->MaxRasterHeight = 512;
            }
            
            return copySize;
        }
        
        case 0x80003000: /* DTAG_NAME - NameInfo */
        {
            /* Provide mode name */
            struct {
                ULONG StructID;
                ULONG DisplayID;
                ULONG SkipID;
                ULONG Length;
                char Name[32];
            } *name = (void *)buf;
            
            ULONG copySize = (size < 48) ? size : 48;
            const char *modeName;
            int i;
            
            name->StructID = tagID;
            name->DisplayID = actualDisplayID;
            name->SkipID = 0;
            name->Length = 4;
            
            modeName = (actualDisplayID & 0x00008000) ? "HIRES" : "LORES";
            for (i = 0; modeName[i] && i < 31; i++)
                name->Name[i] = modeName[i];
            name->Name[i] = 0;
            
            return copySize;
        }
        
        default:
            DPRINTF (LOG_DEBUG, "_graphics: GetDisplayInfoData() unknown tagID=0x%08lx\n", tagID);
            return 0;
    }
}

static VOID _graphics_FontExtent ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct TextFont * font __asm("a0"),
                                                        register struct TextExtent * fontExtent __asm("a1"))
{
    /* Fill out a TextExtent structure with the maximum extent of all characters
     * in the font. Based on AROS implementation.
     */
    WORD i;
    WORD maxwidth = -32767;
    WORD minwidth = 32767;
    WORD width = 0;
    
    DPRINTF (LOG_DEBUG, "_graphics: FontExtent(font=%p, fontExtent=%p)\n", font, fontExtent);
    
    if (!font || !fontExtent)
        return;
    
    /* Loop through all characters in the font */
    for (i = 0; i <= font->tf_HiChar - font->tf_LoChar; i++) {
        WORD kern = 0;
        WORD wspace;
        
        /* Get kerning value if available */
        if (font->tf_CharKern)
            kern = ((WORD *)font->tf_CharKern)[i];
        
        /* Track minimum (leftmost) position */
        if (kern < minwidth)
            minwidth = kern;
        
        /* Get character bitmap width from CharLoc */
        if (font->tf_CharLoc) {
            /* CharLoc is an array of ULONG, low 16 bits is width */
            WORD charWidth = ((ULONG *)font->tf_CharLoc)[i] & 0xFFFF;
            WORD right = kern + charWidth;
            if (right > maxwidth)
                maxwidth = right;
        }
        
        /* Calculate character spacing */
        if (font->tf_CharSpace)
            wspace = kern + ((WORD *)font->tf_CharSpace)[i];
        else
            wspace = kern + font->tf_XSize;
        
        /* Track width based on path direction */
        if (font->tf_Flags & FPF_REVPATH) {
            if (wspace < width)
                width = wspace;
        } else {
            if (wspace > width)
                width = wspace;
        }
    }
    
    /* Handle case where font has no characters or no CharLoc */
    if (maxwidth == -32767)
        maxwidth = font->tf_XSize;
    if (minwidth == 32767)
        minwidth = 0;
    
    fontExtent->te_Width = width;
    fontExtent->te_Height = font->tf_YSize;
    fontExtent->te_Extent.MinX = minwidth;
    fontExtent->te_Extent.MaxX = maxwidth - 1;
    fontExtent->te_Extent.MinY = -font->tf_Baseline;
    fontExtent->te_Extent.MaxY = font->tf_YSize - font->tf_Baseline - 1;
}

static LONG _graphics_ReadPixelLine8 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register LONG xstart __asm("d0"),
                                                        register LONG ystart __asm("d1"),
                                                        register ULONG width __asm("d2"),
                                                        register UBYTE * array __asm("a2"),
                                                        register struct RastPort * tempRP __asm("a1"))
{
    /* TODO: See WritePixelLine8 comment */
    DPRINTF (LOG_ERROR, "_graphics: ReadPixelLine8() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_ReadPixelArray8 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG xstart __asm("d0"),
                                                        register ULONG ystart __asm("d1"),
                                                        register ULONG xstop __asm("d2"),
                                                        register ULONG ystop __asm("d3"),
                                                        register UBYTE * array __asm("a2"),
                                                        register struct RastPort * temprp __asm("a1"))
{
    /* TODO: See WritePixelLine8 comment */
    DPRINTF (LOG_ERROR, "_graphics: ReadPixelArray8() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_WritePixelArray8 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG xstart __asm("d0"),
                                                        register ULONG ystart __asm("d1"),
                                                        register ULONG xstop __asm("d2"),
                                                        register ULONG ystop __asm("d3"),
                                                        register UBYTE * array __asm("a2"),
                                                        register struct RastPort * temprp __asm("a1"))
{
    /* TODO: See WritePixelLine8 comment */
    DPRINTF (LOG_ERROR, "_graphics: WritePixelArray8() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_WritePixelLine8 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG xstart __asm("d0"),
                                                        register ULONG ystart __asm("d1"),
                                                        register ULONG width __asm("d2"),
                                                        register UBYTE * array __asm("a2"),
                                                        register struct RastPort * tempRP __asm("a1"))
{
    /* TODO: Proper implementation requires optimized blitting, not just WritePixel() loops.
     * See AROS write_pixels_8() in gfxfuncsupport.c for reference.
     * For now, return stub to avoid crashes. */
    DPRINTF (LOG_ERROR, "_graphics: WritePixelLine8() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_GetVPModeID ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct ViewPort * vp __asm("a0"))
{
    ULONG modeID = 0x00000000;  /* Default to LORES_KEY */
    
    DPRINTF (LOG_DEBUG, "_graphics: GetVPModeID() vp=0x%08lx\n", (ULONG)vp);
    
    if (!vp)
        return INVALID_ID;
    
    /* Build mode ID from viewport modes */
    if (vp->Modes & HIRES)
        modeID |= 0x00008000;  /* HIRES_KEY */
    if (vp->Modes & LACE)
        modeID |= 0x00000004;  /* LACE bit */
    if (vp->Modes & HAM)
        modeID |= 0x00000800;  /* HAM bit */
    if (vp->Modes & DUALPF)
        modeID |= 0x00000400;  /* DUALPF bit */
    
    DPRINTF (LOG_DEBUG, "_graphics: GetVPModeID() -> 0x%08lx\n", modeID);
    return modeID;
}

static LONG _graphics_ModeNotAvailable ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG modeID __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: ModeNotAvailable() modeID=0x%08lx -> 0 (available)\n", modeID);
    /* Return 0 = mode IS available (no error flags)
     * Non-zero returns would be DI_AVAIL_* flags indicating why mode is not available.
     * For our emulated display, we accept all modes.
     */
    return 0;
}

static VOID _graphics_private5 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private5() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_EraseRect ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG xMin __asm("d0"),
                                                        register LONG yMin __asm("d1"),
                                                        register LONG xMax __asm("d2"),
                                                        register LONG yMax __asm("d3"))
{
    UBYTE oldDrawMode;

    DPRINTF (LOG_DEBUG, "_graphics: EraseRect() rp=0x%08lx, (%ld,%ld)-(%ld,%ld)\n",
             (ULONG)rp, xMin, yMin, xMax, yMax);

    if (!rp)
        return;

    /* EraseRect fills with background pen (BgPen) instead of foreground pen.
     * We can achieve this by temporarily inverting INVERSVID flag and using RectFill */
    oldDrawMode = rp->DrawMode;
    rp->DrawMode ^= INVERSVID;  /* Toggle INVERSVID */

    _graphics_RectFill(GfxBase, rp, (WORD)xMin, (WORD)yMin, (WORD)xMax, (WORD)yMax);

    rp->DrawMode = oldDrawMode;  /* Restore original draw mode */
}

static ULONG _graphics_ExtendFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * font __asm("a0"),
                                                        register CONST struct TagItem * fontTags __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: ExtendFont() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_StripFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * font __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: StripFont() unimplemented STUB called.\n");
    assert(FALSE);
}

static UWORD _graphics_CalcIVG ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * v __asm("a0"),
                                                        register struct ViewPort * vp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: CalcIVG() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_AttachPalExtra ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a0"),
                                                        register struct ViewPort * vp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AttachPalExtra() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_ObtainBestPenA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a0"),
                                                        register ULONG r __asm("d1"),
                                                        register ULONG g __asm("d2"),
                                                        register ULONG b __asm("d3"),
                                                        register CONST struct TagItem * tags __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: ObtainBestPenA() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_private6 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private6() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SetRGB32 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register ULONG n __asm("d0"),
                                                        register ULONG r __asm("d1"),
                                                        register ULONG g __asm("d2"),
                                                        register ULONG b __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetRGB32() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_GetAPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: GetAPen() rp=0x%08lx\n", (ULONG)rp);

    if (rp)
        return (ULONG)(UBYTE)rp->FgPen;

    return 0;
}

static ULONG _graphics_GetBPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: GetBPen() rp=0x%08lx\n", (ULONG)rp);

    if (rp)
        return (ULONG)(UBYTE)rp->BgPen;

    return 0;
}

static ULONG _graphics_GetDrMd ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: GetDrMd() rp=0x%08lx\n", (ULONG)rp);

    if (rp)
        return (ULONG)(UBYTE)rp->DrawMode;

    return 0;
}

static ULONG _graphics_GetOutlinePen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: GetOutlinePen() rp=0x%08lx\n", (ULONG)rp);

    if (rp)
        return (ULONG)(UBYTE)rp->AOlPen;

    return 0;
}

static VOID _graphics_LoadRGB32 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register CONST ULONG * table __asm("a1"))
{
    /*
     * LoadRGB32 loads color palette entries from an array of 32-bit per gun colors.
     * Format: count (high word), first (low word), then R,G,B ULONGs for each color.
     *
     * For lxa, we use the host display system which handles colors differently.
     * We log the call but don't need to actually modify any palette.
     */
    DPRINTF (LOG_DEBUG, "_graphics: LoadRGB32() vp=0x%08lx table=0x%08lx (no-op)\n",
             (ULONG)vp, (ULONG)table);
    /* No-op: palette is handled by host display */
}

static ULONG _graphics_SetChipRev ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG want __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetChipRev() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_SetABPenDrMd ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register ULONG apen __asm("d0"),
                                                        register ULONG bpen __asm("d1"),
                                                        register ULONG drawmode __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_graphics: SetABPenDrMd() rp=0x%08lx, apen=%lu, bpen=%lu, dm=%lu\n",
             (ULONG)rp, apen, bpen, drawmode);

    if (rp)
    {
        rp->FgPen = (BYTE)apen;
        rp->BgPen = (BYTE)bpen;
        rp->DrawMode = (BYTE)drawmode;
    }
}

static VOID _graphics_GetRGB32 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct ColorMap * cm __asm("a0"),
                                                        register ULONG firstcolor __asm("d0"),
                                                        register ULONG ncolors __asm("d1"),
                                                        register ULONG * table __asm("a1"))
{
    /*
     * GetRGB32 - Get 32-bit per gun color values from a ColorMap.
     *
     * This function retrieves color palette entries and stores them in the
     * provided table array. Each color uses 3 ULONGs (R, G, B) with 32 bits
     * per gun.
     *
     * The 4-bit color values (0-15) stored in ColorTable are expanded to
     * 32-bit by replicating the 4-bit value across the entire 32-bit word.
     * For example: 0xF -> 0xFFFFFFFF, 0x8 -> 0x88888888
     */
    DPRINTF (LOG_DEBUG, "_graphics: GetRGB32() cm=0x%08lx, first=%lu, n=%lu, table=0x%08lx\n",
             (ULONG)cm, firstcolor, ncolors, (ULONG)table);
    
    if (!cm || !table)
        return;
    
    UWORD *ct = cm->ColorTable;
    if (!ct)
        return;
    
    for (ULONG i = 0; i < ncolors; i++) {
        ULONG color_index = firstcolor + i;
        ULONG rgb4;
        
        if (color_index < cm->Count) {
            rgb4 = ct[color_index];
        } else {
            rgb4 = 0;  /* Out of range - return black */
        }
        
        /* Extract 4-bit components from 0x0RGB format */
        ULONG r4 = (rgb4 >> 8) & 0xF;
        ULONG g4 = (rgb4 >> 4) & 0xF;
        ULONG b4 = rgb4 & 0xF;
        
        /* Expand 4-bit to 32-bit by replicating bits
         * 0xF -> 0xFFFFFFFF, 0x8 -> 0x88888888, etc.
         */
        ULONG r32 = r4 | (r4 << 4);
        r32 = r32 | (r32 << 8);
        r32 = r32 | (r32 << 16);
        
        ULONG g32 = g4 | (g4 << 4);
        g32 = g32 | (g32 << 8);
        g32 = g32 | (g32 << 16);
        
        ULONG b32 = b4 | (b4 << 4);
        b32 = b32 | (b32 << 8);
        b32 = b32 | (b32 << 16);
        
        /* Store in table: R, G, B for each color */
        table[i * 3 + 0] = r32;
        table[i * 3 + 1] = g32;
        table[i * 3 + 2] = b32;
    }
}

static VOID _graphics_private7 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private7() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_private8 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private8() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct BitMap * _graphics_AllocBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG sizex __asm("d0"),
                                                        register ULONG sizey __asm("d1"),
                                                        register ULONG depth __asm("d2"),
                                                        register ULONG flags __asm("d3"),
                                                        register const struct BitMap * friend_bitmap __asm("a0"))
{
    struct BitMap *bm;
    ULONG plane;
    ULONG extraPlanes;

    DPRINTF (LOG_DEBUG, "_graphics: AllocBitMap(%lu, %lu, %lu, 0x%08lx, 0x%08lx)\n",
             sizex, sizey, depth, flags, (ULONG)friend_bitmap);

    if (depth == 0 || depth > 8)
    {
        DPRINTF (LOG_WARNING, "_graphics: AllocBitMap() depth %lu out of range (1-8), clamping\n", depth);
        if (depth == 0) depth = 1;
        if (depth > 8) depth = 8;
    }

    /* Calculate extra plane pointers needed for depths > 8 */
    extraPlanes = (depth > 8) ? (depth - 8) : 0;

    /* Allocate BitMap structure with extra plane pointers if needed */
    bm = (struct BitMap *)AllocMem(sizeof(struct BitMap) + extraPlanes * sizeof(PLANEPTR),
                                    MEMF_PUBLIC | MEMF_CLEAR);

    if (!bm)
    {
        DPRINTF (LOG_ERROR, "_graphics: AllocBitMap() failed to allocate BitMap structure\n");
        return NULL;
    }

    /* Initialize BitMap structure */
    bm->BytesPerRow = ((sizex + 15) >> 4) * 2;  /* Round up to nearest word boundary */
    bm->Rows = (UWORD)sizey;
    bm->Flags = (UBYTE)(flags | BMF_STANDARD);
    bm->Depth = (UBYTE)depth;
    bm->pad = 0;

    /* Allocate plane data (AllocRaster already handles BMF_CLEAR via MEMF_CLEAR) */
    for (plane = 0; plane < depth; plane++)
    {
        bm->Planes[plane] = _graphics_AllocRaster(GfxBase, (UWORD)sizex, (UWORD)sizey);

        if (!bm->Planes[plane])
        {
            DPRINTF (LOG_ERROR, "_graphics: AllocBitMap() failed to allocate plane %lu\n", plane);

            /* Free previously allocated planes */
            for (ULONG p = 0; p < plane; p++)
            {
                if (bm->Planes[p])
                    _graphics_FreeRaster(GfxBase, bm->Planes[p], (UWORD)sizex, (UWORD)sizey);
            }

            FreeMem(bm, sizeof(struct BitMap) + extraPlanes * sizeof(PLANEPTR));
            return NULL;
        }
    }

    DPRINTF (LOG_DEBUG, "_graphics: AllocBitMap() -> 0x%08lx\n", (ULONG)bm);
    return bm;
}

static VOID _graphics_FreeBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct BitMap * bm __asm("a0"))
{
    ULONG plane;
    ULONG width;
    ULONG extraPlanes;

    DPRINTF (LOG_DEBUG, "_graphics: FreeBitMap(0x%08lx)\n", (ULONG)bm);

    if (!bm)
        return;

    /* Calculate width from BytesPerRow */
    width = bm->BytesPerRow * 8;

    /* Free all plane data */
    for (plane = 0; plane < bm->Depth; plane++)
    {
        /* Handle special plane pointer values:
         * NULL = all 0's plane (no allocation)
         * -1 = all 1's plane (no allocation)
         */
        if (bm->Planes[plane] && bm->Planes[plane] != (PLANEPTR)-1)
        {
            _graphics_FreeRaster(GfxBase, bm->Planes[plane], (UWORD)width, bm->Rows);
        }
    }

    /* Calculate extra plane pointers for depths > 8 */
    extraPlanes = (bm->Depth > 8) ? (bm->Depth - 8) : 0;

    /* Free BitMap structure */
    FreeMem(bm, sizeof(struct BitMap) + extraPlanes * sizeof(PLANEPTR));
}

static LONG _graphics_GetExtSpriteA ( register struct GfxBase       *GfxBase __asm("a6"),
                                               register struct ExtSprite     *ss      __asm("a2"),
                                               register CONST struct TagItem *tags    __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetExtSpriteA() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_CoerceMode ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register ULONG monitorid __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: CoerceMode() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_ChangeVPBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register struct BitMap * bm __asm("a1"),
                                                        register struct DBufInfo * db __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: ChangeVPBitMap() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_ReleasePen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a0"),
                                                        register ULONG n __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: ReleasePen() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_ObtainPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a0"),
                                                        register ULONG n __asm("d0"),
                                                        register ULONG r __asm("d1"),
                                                        register ULONG g __asm("d2"),
                                                        register ULONG b __asm("d3"),
                                                        register LONG f __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_graphics: ObtainPen() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_GetBitMapAttr ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct BitMap * bm __asm("a0"),
                                                        register ULONG attrnum __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: GetBitMapAttr() bm=0x%08lx attr=%ld\n", (ULONG)bm, attrnum);
    
    if (!bm)
        return 0;
    
    switch (attrnum) {
        case 0:  /* BMA_HEIGHT */
            return bm->Rows;
        case 4:  /* BMA_DEPTH */
            return bm->Depth;
        case 8:  /* BMA_WIDTH */
            return bm->BytesPerRow * 8;  /* Convert bytes to pixels */
        case 12: /* BMA_FLAGS */
            return bm->Flags;
        default:
            DPRINTF (LOG_ERROR, "_graphics: GetBitMapAttr() unknown attr %ld\n", attrnum);
            return 0;
    }
}

static struct DBufInfo * _graphics_AllocDBufInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: AllocDBufInfo() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_FreeDBufInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct DBufInfo * dbi __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeDBufInfo() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_SetOutlinePen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG pen __asm("d0"))
{
    ULONG oldPen;

    DPRINTF (LOG_DEBUG, "_graphics: SetOutlinePen(rp=0x%08lx, pen=%lu)\n",
             (ULONG)rp, pen);

    if (!rp)
        return 0;

    oldPen = (ULONG)(UBYTE)rp->AOlPen;
    rp->AOlPen = (UBYTE)pen;
    return oldPen;
}

static ULONG _graphics_SetWriteMask ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG msk __asm("d0"))
{
    ULONG oldMask;

    /*
     * SetWriteMask() - Set the bit plane write mask for a RastPort
     * 
     * This controls which bitplanes can be written to during drawing operations.
     */
    DPRINTF (LOG_DEBUG, "_graphics: SetWriteMask(rp=0x%08lx, msk=0x%08lx)\n", 
             (ULONG)rp, msk);

    if (!rp)
        return 0;

    oldMask = (ULONG)rp->Mask;
    rp->Mask = (UBYTE)msk;
    return oldMask;
}

static VOID _graphics_SetMaxPen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG maxpen __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetMaxPen() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SetRGB32CM ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a0"),
                                                        register ULONG n __asm("d0"),
                                                        register ULONG r __asm("d1"),
                                                        register ULONG g __asm("d2"),
                                                        register ULONG b __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetRGB32CM() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_ScrollRasterBF ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG dx __asm("d0"),
                                                        register LONG dy __asm("d1"),
                                                        register LONG xMin __asm("d2"),
                                                        register LONG yMin __asm("d3"),
                                                        register LONG xMax __asm("d4"),
                                                        register LONG yMax __asm("d5"))
{
    struct BitMap *bm;
    LONG srcX, srcY, destX, destY;
    LONG width, height;
    LONG absdx, absdy;
    
    DPRINTF (LOG_DEBUG, "_graphics: ScrollRasterBF() rp=0x%08lx dx=%ld dy=%ld rect=(%ld,%ld)-(%ld,%ld)\n",
             (ULONG)rp, dx, dy, xMin, yMin, xMax, yMax);
    
    if (!rp || !rp->BitMap) {
        DPRINTF(LOG_DEBUG, "_graphics: ScrollRasterBF() NULL rp or bitmap\n");
        return;
    }
    
    /* Validate bounds */
    if (xMin > xMax || yMin > yMax)
        return;
    
    bm = rp->BitMap;
    
    /* Adjust bounds based on layer or bitmap */
    if (rp->Layer) {
        struct Layer *L = rp->Layer;
        LONG layerWidth = L->bounds.MaxX - L->bounds.MinX;
        LONG layerHeight = L->bounds.MaxY - L->bounds.MinY;
        
        if (xMax > layerWidth)
            xMax = layerWidth;
        if (yMax > layerHeight)
            yMax = layerHeight;
    } else {
        LONG bmWidth = bm->BytesPerRow * 8;
        LONG bmHeight = bm->Rows;
        
        if (xMax >= bmWidth)
            xMax = bmWidth - 1;
        if (yMax >= bmHeight)
            yMax = bmHeight - 1;
    }
    
    width = xMax - xMin + 1;
    height = yMax - yMin + 1;
    
    if (width < 1 || height < 1)
        return;
    
    absdx = (dx >= 0) ? dx : -dx;
    absdy = (dy >= 0) ? dy : -dy;
    
    /* If scroll amount exceeds the area, just erase the entire area */
    if (absdx >= width || absdy >= height) {
        _graphics_EraseRect(GfxBase, rp, xMin, yMin, xMax, yMax);
        return;
    }
    
    /* Calculate source and destination coordinates for the blit */
    /* Positive dx = scroll right (content moves left), negative dx = scroll left */
    /* Positive dy = scroll down (content moves up), negative dy = scroll up */
    
    srcX = xMin;
    srcY = yMin;
    destX = xMin;
    destY = yMin;
    
    if (dx > 0) {
        srcX = xMin + dx;
        destX = xMin;
    } else if (dx < 0) {
        srcX = xMin;
        destX = xMin - dx;
    }
    
    if (dy > 0) {
        srcY = yMin + dy;
        destY = yMin;
    } else if (dy < 0) {
        srcY = yMin;
        destY = yMin - dy;
    }
    
    /* Perform the blit if there's any content to move */
    if ((width - absdx) > 0 && (height - absdy) > 0) {
        _graphics_BltBitMap(GfxBase, bm, srcX, srcY, bm, destX, destY, 
                           width - absdx, height - absdy, 0xC0, 0xFF, NULL);
    }
    
    /* Erase the exposed areas using EraseRect (respects BackFill hook) */
    
    /* Horizontal strip */
    if (dx > 0) {
        /* Scrolled towards left, clearing on the right */
        _graphics_EraseRect(GfxBase, rp, xMax - dx + 1, yMin, xMax, yMax);
    } else if (dx < 0) {
        /* Scrolled towards right, clearing on the left */
        _graphics_EraseRect(GfxBase, rp, xMin, yMin, xMin - dx - 1, yMax);
    }
    
    /* Vertical strip */
    if (dy > 0) {
        /* Scrolled up, clearing on the bottom */
        _graphics_EraseRect(GfxBase, rp, xMin, yMax - dy + 1, xMax, yMax);
    } else if (dy < 0) {
        /* Scrolled down, clearing on the top */
        _graphics_EraseRect(GfxBase, rp, xMin, yMin, xMax, yMin - dy - 1);
    }
}

static LONG _graphics_FindColor ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * cm __asm("a3"),
                                                        register ULONG r __asm("d1"),
                                                        register ULONG g __asm("d2"),
                                                        register ULONG b __asm("d3"),
                                                        register LONG maxcolor __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_graphics: FindColor() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_private9 ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: private9() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct ExtSprite * _graphics_AllocSpriteDataA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct BitMap * bm __asm("a2"),
                                                        register CONST struct TagItem * tags __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AllocSpriteDataA() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static LONG _graphics_ChangeExtSpriteA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register struct ExtSprite * oldsprite __asm("a1"),
                                                        register struct ExtSprite * newsprite __asm("a2"),
                                                        register CONST struct TagItem * tags __asm("a3"))
{
    DPRINTF (LOG_ERROR, "_graphics: ChangeExtSpriteA() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_FreeSpriteData ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ExtSprite * sp __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeSpriteData() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SetRPAttrsA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register CONST struct TagItem * tags __asm("a1"))
{
    CONST struct TagItem *tag;

    DPRINTF (LOG_DEBUG, "_graphics: SetRPAttrsA() rp=0x%08lx, tags=0x%08lx\n",
             (ULONG)rp, (ULONG)tags);

    if (!rp || !tags)
        return;

    /* Iterate through tags */
    tag = tags;
    while (tag->ti_Tag != TAG_DONE)
    {
        switch (tag->ti_Tag)
        {
            case TAG_IGNORE:
                break;

            case TAG_MORE:
                tag = (CONST struct TagItem *)tag->ti_Data;
                continue;

            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                continue;

            case 0x80000000: /* RPTAG_Font */
                _graphics_SetFont(GfxBase, rp, (struct TextFont *)tag->ti_Data);
                break;

            case 0x80000002: /* RPTAG_APen */
                _graphics_SetAPen(GfxBase, rp, (UBYTE)tag->ti_Data);
                break;

            case 0x80000003: /* RPTAG_BPen */
                _graphics_SetBPen(GfxBase, rp, (UBYTE)tag->ti_Data);
                break;

            case 0x80000004: /* RPTAG_DrMd */
                _graphics_SetDrMd(GfxBase, rp, (UBYTE)tag->ti_Data);
                break;

            case 0x80000005: /* RPTAG_OutlinePen */
                _graphics_SetOutlinePen(GfxBase, rp, (UBYTE)tag->ti_Data);
                break;

            case 0x80000006: /* RPTAG_WriteMask */
                _graphics_SetWriteMask(GfxBase, rp, (ULONG)tag->ti_Data);
                break;

            case 0x80000007: /* RPTAG_MaxPen */
                /* Read-only, ignore */
                break;

            case 0x80000008: /* RPTAG_DrawBounds */
                /* Not implemented yet */
                break;

            default:
                DPRINTF (LOG_DEBUG, "_graphics: SetRPAttrsA() unknown tag 0x%08lx\n",
                         tag->ti_Tag);
                break;
        }
        tag++;
    }
}

static VOID _graphics_GetRPAttrsA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct RastPort * rp __asm("a0"),
                                                        register CONST struct TagItem * tags __asm("a1"))
{
    CONST struct TagItem *tag;
    ULONG MaxPen, z;

    DPRINTF (LOG_DEBUG, "_graphics: GetRPAttrsA() rp=0x%08lx, tags=0x%08lx\n",
             (ULONG)rp, (ULONG)tags);

    if (!rp || !tags)
        return;

    /* Iterate through tags */
    tag = tags;
    while (tag->ti_Tag != TAG_DONE)
    {
        switch (tag->ti_Tag)
        {
            case TAG_IGNORE:
                break;

            case TAG_MORE:
                tag = (CONST struct TagItem *)tag->ti_Data;
                continue;

            case TAG_SKIP:
                tag += tag->ti_Data + 1;
                continue;

            case 0x80000000: /* RPTAG_Font */
                *((ULONG *)tag->ti_Data) = (ULONG)rp->Font;
                break;

            case 0x80000002: /* RPTAG_APen */
                *((ULONG *)tag->ti_Data) = (ULONG)_graphics_GetAPen(GfxBase, (struct RastPort *)rp);
                break;

            case 0x80000003: /* RPTAG_BPen */
                *((ULONG *)tag->ti_Data) = (ULONG)_graphics_GetBPen(GfxBase, (struct RastPort *)rp);
                break;

            case 0x80000004: /* RPTAG_DrMd */
                *((ULONG *)tag->ti_Data) = (ULONG)_graphics_GetDrMd(GfxBase, (struct RastPort *)rp);
                break;

            case 0x80000005: /* RPTAG_OutlinePen */
                *((ULONG *)tag->ti_Data) = (ULONG)_graphics_GetOutlinePen(GfxBase, (struct RastPort *)rp);
                break;

            case 0x80000006: /* RPTAG_WriteMask */
                *((ULONG *)tag->ti_Data) = (ULONG)rp->Mask;
                break;

            case 0x80000007: /* RPTAG_MaxPen */
                /* Calculate MaxPen from write mask */
                MaxPen = 0x01;
                z = (LONG)rp->Mask;
                if (z == 0)
                    MaxPen = 0x100;
                else
                    while (z != 0)
                    {
                        z >>= 1;
                        MaxPen <<= 1;
                    }
                *((ULONG *)tag->ti_Data) = MaxPen;
                break;

            case 0x80000008: /* RPTAG_DrawBounds */
                /* Not implemented yet - return zero bounds */
                ((struct Rectangle *)tag->ti_Data)->MinX = 0;
                ((struct Rectangle *)tag->ti_Data)->MinY = 0;
                ((struct Rectangle *)tag->ti_Data)->MaxX = 0;
                ((struct Rectangle *)tag->ti_Data)->MaxY = 0;
                break;

            default:
                DPRINTF (LOG_DEBUG, "_graphics: GetRPAttrsA() unknown tag 0x%08lx\n",
                         tag->ti_Tag);
                break;
        }
        tag++;
    }
}

static ULONG _graphics_BestModeIDA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct TagItem * tags __asm("a0"))
{
    /*
     * BestModeIDA() finds the best display mode matching the given criteria.
     * Returns the ModeID or INVALID_ID (0xFFFFFFFF) if no suitable mode found.
     * 
     * We provide simple matching: HIRES for width > 400, LORES otherwise.
     */
    ULONG desiredWidth = 640;
    ULONG desiredHeight = 200;
    ULONG depth = 4;
    const struct TagItem *tag;
    
    DPRINTF (LOG_DEBUG, "_graphics: BestModeIDA() tags=0x%08lx\n", (ULONG)tags);
    
    /* Parse tags */
    if (tags) {
        for (tag = tags; tag->ti_Tag != 0; tag++) {
            switch (tag->ti_Tag) {
                case 0x80000004: /* BIDTAG_NominalWidth */
                case 0x80000006: /* BIDTAG_DesiredWidth */
                    desiredWidth = tag->ti_Data;
                    break;
                case 0x80000005: /* BIDTAG_NominalHeight */
                case 0x80000007: /* BIDTAG_DesiredHeight */
                    desiredHeight = tag->ti_Data;
                    break;
                case 0x80000008: /* BIDTAG_Depth */
                    depth = tag->ti_Data;
                    break;
                case 0x80000001: /* BIDTAG_DIPFMustHave */
                case 0x80000002: /* BIDTAG_DIPFMustNotHave */
                case 0x80000003: /* BIDTAG_ViewPort */
                case 0x80000009: /* BIDTAG_MonitorID */
                case 0x8000000a: /* BIDTAG_SourceID */
                    /* Ignore these for now */
                    break;
            }
            
            /* Handle TAG_DONE, TAG_SKIP, TAG_IGNORE */
            if (tag->ti_Tag == 0xFFFFFFFF) /* TAG_DONE */
                break;
        }
    }
    
    (void)desiredHeight;  /* Unused for now */
    (void)depth;          /* Unused for now */
    
    /* Return HIRES for wider screens, LORES otherwise */
    if (desiredWidth > 400) {
        return 0x00008000;  /* HIRES_KEY */
    } else {
        return 0x00000000;  /* LORES_KEY */
    }
}

static VOID _graphics_WriteChunkyPixels ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG xstart __asm("d0"),
                                                        register ULONG ystart __asm("d1"),
                                                        register ULONG xstop __asm("d2"),
                                                        register ULONG ystop __asm("d3"),
                                                        register CONST UBYTE * array __asm("a2"),
                                                        register LONG bytesperrow __asm("d4"))
{
    ULONG x, y;
    CONST UBYTE *src;
    BYTE oldPen;

    DPRINTF (LOG_DEBUG, "_graphics: WriteChunkyPixels() rp=0x%08lx, (%ld,%ld)-(%ld,%ld), bpr=%ld\n",
             (ULONG)rp, xstart, ystart, xstop, ystop, bytesperrow);

    if (!rp || !array)
        return;

    /* Validate coordinates */
    if (xstart > xstop || ystart > ystop)
        return;

    /* Save current pen */
    oldPen = rp->FgPen;

    /* Write pixels row by row */
    for (y = ystart; y <= ystop; y++)
    {
        src = array + (y - ystart) * bytesperrow;
        for (x = xstart; x <= xstop; x++)
        {
            rp->FgPen = (BYTE)src[x - xstart];
            _graphics_WritePixel(GfxBase, rp, (WORD)x, (WORD)y);
        }
    }

    /* Restore pen */
    rp->FgPen = oldPen;
}

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

extern APTR              __g_lxa_graphics_FuncTab [];
extern struct MyDataInit __g_lxa_graphics_DataTab;
extern struct InitTable  __g_lxa_graphics_InitTab;
extern APTR              __g_lxa_graphics_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_graphics_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_graphics_ExLibName[0],     // char  *rt_Name;                   14
    &_g_graphics_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_graphics_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_graphics_EndResident;
struct Resident *__lxa_graphics_ROMTag = &ROMTag;

struct InitTable __g_lxa_graphics_InitTab =
{
    (ULONG)               sizeof(struct GfxBase),        // LibBaseSize
    (APTR              *) &__g_lxa_graphics_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_graphics_DataTab,     // DataTable
    (APTR)                __g_lxa_graphics_InitLib       // InitLibFn
};

APTR __g_lxa_graphics_FuncTab [] =
{
    __g_lxa_graphics_OpenLib,
    __g_lxa_graphics_CloseLib,
    __g_lxa_graphics_ExpungeLib,
    __g_lxa_graphics_ExtFuncLib,
    _graphics_BltBitMap, // offset = -30
    _graphics_BltTemplate, // offset = -36
    _graphics_ClearEOL, // offset = -42
    _graphics_ClearScreen, // offset = -48
    _graphics_TextLength, // offset = -54
    _graphics_Text, // offset = -60
    _graphics_SetFont, // offset = -66
    _graphics_OpenFont, // offset = -72
    _graphics_CloseFont, // offset = -78
    _graphics_AskSoftStyle, // offset = -84
    _graphics_SetSoftStyle, // offset = -90
    _graphics_AddBob, // offset = -96
    _graphics_AddVSprite, // offset = -102
    _graphics_DoCollision, // offset = -108
    _graphics_DrawGList, // offset = -114
    _graphics_InitGels, // offset = -120
    _graphics_InitMasks, // offset = -126
    _graphics_RemIBob, // offset = -132
    _graphics_RemVSprite, // offset = -138
    _graphics_SetCollision, // offset = -144
    _graphics_SortGList, // offset = -150
    _graphics_AddAnimOb, // offset = -156
    _graphics_Animate, // offset = -162
    _graphics_GetGBuffers, // offset = -168
    _graphics_InitGMasks, // offset = -174
    _graphics_DrawEllipse, // offset = -180
    _graphics_AreaEllipse, // offset = -186
    _graphics_LoadRGB4, // offset = -192
    _graphics_InitRastPort, // offset = -198
    _graphics_InitVPort, // offset = -204
    _graphics_MrgCop, // offset = -210
    _graphics_MakeVPort, // offset = -216
    _graphics_LoadView, // offset = -222
    _graphics_WaitBlit, // offset = -228
    _graphics_SetRast, // offset = -234
    _graphics_Move, // offset = -240
    _graphics_Draw, // offset = -246
    _graphics_AreaMove, // offset = -252
    _graphics_AreaDraw, // offset = -258
    _graphics_AreaEnd, // offset = -264
    _graphics_WaitTOF, // offset = -270
    _graphics_QBlit, // offset = -276
    _graphics_InitArea, // offset = -282
    _graphics_SetRGB4, // offset = -288
    _graphics_QBSBlit, // offset = -294
    _graphics_BltClear, // offset = -300
    _graphics_RectFill, // offset = -306
    _graphics_BltPattern, // offset = -312
    _graphics_ReadPixel, // offset = -318
    _graphics_WritePixel, // offset = -324
    _graphics_Flood, // offset = -330
    _graphics_PolyDraw, // offset = -336
    _graphics_SetAPen, // offset = -342
    _graphics_SetBPen, // offset = -348
    _graphics_SetDrMd, // offset = -354
    _graphics_InitView, // offset = -360
    _graphics_CBump, // offset = -366
    _graphics_CMove, // offset = -372
    _graphics_CWait, // offset = -378
    _graphics_VBeamPos, // offset = -384
    _graphics_InitBitMap, // offset = -390
    _graphics_ScrollRaster, // offset = -396
    _graphics_WaitBOVP, // offset = -402
    _graphics_GetSprite, // offset = -408
    _graphics_FreeSprite, // offset = -414
    _graphics_ChangeSprite, // offset = -420
    _graphics_MoveSprite, // offset = -426
    _graphics_LockLayerRom, // offset = -432
    _graphics_UnlockLayerRom, // offset = -438
    _graphics_SyncSBitMap, // offset = -444
    _graphics_CopySBitMap, // offset = -450
    _graphics_OwnBlitter, // offset = -456
    _graphics_DisownBlitter, // offset = -462
    _graphics_InitTmpRas, // offset = -468
    _graphics_AskFont, // offset = -474
    _graphics_AddFont, // offset = -480
    _graphics_RemFont, // offset = -486
    _graphics_AllocRaster, // offset = -492
    _graphics_FreeRaster, // offset = -498
    _graphics_AndRectRegion, // offset = -504
    _graphics_OrRectRegion, // offset = -510
    _graphics_NewRegion, // offset = -516
    _graphics_ClearRectRegion, // offset = -522
    _graphics_ClearRegion, // offset = -528
    _graphics_DisposeRegion, // offset = -534
    _graphics_FreeVPortCopLists, // offset = -540
    _graphics_FreeCopList, // offset = -546
    _graphics_ClipBlit, // offset = -552
    _graphics_XorRectRegion, // offset = -558
    _graphics_FreeCprList, // offset = -564
    _graphics_GetColorMap, // offset = -570
    _graphics_FreeColorMap, // offset = -576
    _graphics_GetRGB4, // offset = -582
    _graphics_ScrollVPort, // offset = -588
    _graphics_UCopperListInit, // offset = -594
    _graphics_FreeGBuffers, // offset = -600
    _graphics_BltBitMapRastPort, // offset = -606
    _graphics_OrRegionRegion, // offset = -612
    _graphics_XorRegionRegion, // offset = -618
    _graphics_AndRegionRegion, // offset = -624
    _graphics_SetRGB4CM, // offset = -630
    _graphics_BltMaskBitMapRastPort, // offset = -636
    _graphics_private0, // offset = -642
    _graphics_private1, // offset = -648
    _graphics_AttemptLockLayerRom, // offset = -654
    _graphics_GfxNew, // offset = -660
    _graphics_GfxFree, // offset = -666
    _graphics_GfxAssociate, // offset = -672
    _graphics_BitMapScale, // offset = -678
    _graphics_ScalerDiv, // offset = -684
    _graphics_TextExtent, // offset = -690
    _graphics_TextFit, // offset = -696
    _graphics_GfxLookUp, // offset = -702
    _graphics_VideoControl, // offset = -708
    _graphics_OpenMonitor, // offset = -714
    _graphics_CloseMonitor, // offset = -720
    _graphics_FindDisplayInfo, // offset = -726
    _graphics_NextDisplayInfo, // offset = -732
    _graphics_private2, // offset = -738
    _graphics_private3, // offset = -744
    _graphics_private4, // offset = -750
    _graphics_GetDisplayInfoData, // offset = -756
    _graphics_FontExtent, // offset = -762
    _graphics_ReadPixelLine8, // offset = -768
    _graphics_WritePixelLine8, // offset = -774
    _graphics_ReadPixelArray8, // offset = -780
    _graphics_WritePixelArray8, // offset = -786
    _graphics_GetVPModeID, // offset = -792
    _graphics_ModeNotAvailable, // offset = -798
    _graphics_private5, // offset = -804
    _graphics_EraseRect, // offset = -810
    _graphics_ExtendFont, // offset = -816
    _graphics_StripFont, // offset = -822
    _graphics_CalcIVG, // offset = -828
    _graphics_AttachPalExtra, // offset = -834
    _graphics_ObtainBestPenA, // offset = -840
    _graphics_private6, // offset = -846
    _graphics_SetRGB32, // offset = -852
    _graphics_GetAPen, // offset = -858
    _graphics_GetBPen, // offset = -864
    _graphics_GetDrMd, // offset = -870
    _graphics_GetOutlinePen, // offset = -876
    _graphics_LoadRGB32, // offset = -882
    _graphics_SetChipRev, // offset = -888
    _graphics_SetABPenDrMd, // offset = -894
    _graphics_GetRGB32, // offset = -900
    _graphics_private7, // offset = -906
    _graphics_private8, // offset = -912
    _graphics_AllocBitMap, // offset = -918
    _graphics_FreeBitMap, // offset = -924
    _graphics_GetExtSpriteA, // offset = -930
    _graphics_CoerceMode, // offset = -936
    _graphics_ChangeVPBitMap, // offset = -942
    _graphics_ReleasePen, // offset = -948
    _graphics_ObtainPen, // offset = -954
    _graphics_GetBitMapAttr, // offset = -960
    _graphics_AllocDBufInfo, // offset = -966
    _graphics_FreeDBufInfo, // offset = -972
    _graphics_SetOutlinePen, // offset = -978
    _graphics_SetWriteMask, // offset = -984
    _graphics_SetMaxPen, // offset = -990
    _graphics_SetRGB32CM, // offset = -996
    _graphics_ScrollRasterBF, // offset = -1002
    _graphics_FindColor, // offset = -1008
    _graphics_private9, // offset = -1014
    _graphics_AllocSpriteDataA, // offset = -1020
    _graphics_ChangeExtSpriteA, // offset = -1026
    _graphics_FreeSpriteData, // offset = -1032
    _graphics_SetRPAttrsA, // offset = -1038
    _graphics_GetRPAttrsA, // offset = -1044
    _graphics_BestModeIDA, // offset = -1050
    _graphics_WriteChunkyPixels, // offset = -1056
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_graphics_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_graphics_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_graphics_ExLibID[0],
    (ULONG) 0
};

