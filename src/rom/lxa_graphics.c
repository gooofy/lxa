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

#include <intuition/intuitionbase.h>

#include "util.h"
#include "topaz8_font.h"

/* Forward declaration for input processing (defined in lxa_intuition.c) */
extern VOID _intuition_ProcessInputEvents(struct Screen *screen);

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

/* Use lxa_memset instead of memset to avoid conflict with libnix string.h */
static void lxa_memset(void *s, int c, ULONG n)
{
    UBYTE *p = (UBYTE *)s;
    while (n--)
        *p++ = (UBYTE)c;
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

struct GfxBase * __g_lxa_graphics_InitLib    ( register struct GfxBase *graphicsb    __asm("a6"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_graphics: WARNING: InitLib() unimplemented STUB called.\n");
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
    DPRINTF (LOG_ERROR, "_graphics: ClearEOL() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_ClearScreen ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: ClearScreen() unimplemented STUB called.\n");
    assert(FALSE);
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
static BOOL g_topaz8_initialized = FALSE;

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
    DPRINTF (LOG_ERROR, "_graphics: AskSoftStyle() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_SetSoftStyle ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register ULONG style __asm("d0"),
                                                        register ULONG enable __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetSoftStyle() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: InitGels() unimplemented STUB called.\n");
    assert(FALSE);
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
                                                        register LONG xCenter __asm("d0"),
                                                        register LONG yCenter __asm("d1"),
                                                        register LONG a __asm("d2"),
                                                        register LONG b __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: DrawEllipse() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _graphics_AreaEllipse ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG xCenter __asm("d0"),
                                                        register LONG yCenter __asm("d1"),
                                                        register LONG a __asm("d2"),
                                                        register LONG b __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: AreaEllipse() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_LoadRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register CONST UWORD * colors __asm("a1"),
                                                        register LONG count __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: LoadRGB4() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_InitRastPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_graphics: InitRastPort() rp=0x%08lx\n", (ULONG)rp);

    if (!rp)
        return;

    /* Zero out the entire RastPort structure */
    lxa_memset(rp, 0, sizeof(struct RastPort));

    /* Set default values */
    rp->Mask = 0xFF;           /* All planes enabled for writing */
    rp->FgPen = 1;             /* Foreground pen = 1 (typically black) */
    rp->BgPen = 0;             /* Background pen = 0 (typically white/gray) */
    rp->AOlPen = 1;            /* Outline pen for area fills */
    rp->DrawMode = JAM2;       /* Default drawing mode */
    rp->LinePtrn = 0xFFFF;     /* Solid line pattern */
    rp->Flags = FRST_DOT;      /* Draw first dot */
    rp->PenWidth = 1;
    rp->PenHeight = 1;
}

static VOID _graphics_InitVPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: InitVPort() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_MrgCop ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: MrgCop() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_MakeVPort ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a0"),
                                                        register struct ViewPort * vp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: MakeVPort() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_LoadView ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct View * view __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: LoadView() unimplemented STUB called.\n");
    assert(FALSE);
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

    /* Software line drawing using Bresenham's algorithm */
    WORD x0 = rp->cp_x;
    WORD y0 = rp->cp_y;
    WORD x1 = (WORD)x;
    WORD y1 = (WORD)y;
    BYTE pen = rp->FgPen;
    struct BitMap *bm = rp->BitMap;

    WORD dx = x1 > x0 ? x1 - x0 : x0 - x1;
    WORD dy = y1 > y0 ? y0 - y1 : y1 - y0;  /* Negative for Bresenham */
    WORD sx = x0 < x1 ? 1 : -1;
    WORD sy = y0 < y1 ? 1 : -1;
    WORD err = dx + dy;

    while (1)
    {
        /* Set pixel at (x0, y0) with the foreground pen */
        if (x0 >= 0 && y0 >= 0 && x0 < (bm->BytesPerRow * 8) && y0 < bm->Rows)
        {
            UWORD byteOffset = y0 * bm->BytesPerRow + (x0 >> 3);
            UBYTE bitMask = 0x80 >> (x0 & 7);

            /* Set or clear bits in each plane based on pen color */
            for (UBYTE plane = 0; plane < bm->Depth; plane++)
            {
                if (bm->Planes[plane])
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

        if (x0 == x1 && y0 == y1)
            break;

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

    /* Update current position */
    rp->cp_x = (WORD)x;
    rp->cp_y = (WORD)y;
}

static LONG _graphics_AreaMove ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AreaMove() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_AreaDraw ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AreaDraw() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_AreaEnd ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AreaEnd() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}


static VOID _graphics_WaitTOF ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_graphics: WaitTOF()\n");
    
    /* Process input events and refresh displays for all Intuition screens */
    /* This is a good hook point since WaitTOF is called in main loops */
    struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (IntuitionBase)
    {
        struct Screen *screen;
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
        CloseLibrary((struct Library *)IntuitionBase);
    }
}

static VOID _graphics_QBlit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct bltnode * blit __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: QBlit() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_InitArea ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct AreaInfo * areaInfo __asm("a0"),
                                                        register APTR vectorBuffer __asm("a1"),
                                                        register LONG maxVectors __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: InitArea() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_SetRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register LONG index __asm("d0"),
                                                        register ULONG red __asm("d1"),
                                                        register ULONG green __asm("d2"),
                                                        register ULONG blue __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetRGB4() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_QBSBlit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct bltnode * blit __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: QBSBlit() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_BltClear ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register PLANEPTR memBlock __asm("a1"),
                                                        register ULONG byteCount __asm("d0"),
                                                        register ULONG flags __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: BltClear() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_RectFill ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register WORD xMin __asm("d0"),
                                                        register WORD yMin __asm("d1"),
                                                        register WORD xMax __asm("d2"),
                                                        register WORD yMax __asm("d3"))
{
    DPRINTF (LOG_DEBUG, "_graphics: RectFill() rp=0x%08lx, (%d,%d)-(%d,%d)\n",
             (ULONG)rp, (int)xMin, (int)yMin, (int)xMax, (int)yMax);

    if (!rp || !rp->BitMap)
        return;

    struct BitMap *bm = rp->BitMap;
    BYTE pen = rp->FgPen;
    WORD maxX = bm->BytesPerRow * 8;
    WORD maxY = bm->Rows;

    /* Clamp to bitmap bounds */
    if (xMin < 0) xMin = 0;
    if (yMin < 0) yMin = 0;
    if (xMax >= maxX) xMax = maxX - 1;
    if (yMax >= maxY) yMax = maxY - 1;

    if (xMin > xMax || yMin > yMax)
        return;

    /* Fill the rectangle */
    for (WORD y = (WORD)yMin; y <= (WORD)yMax; y++)
    {
        for (WORD x = (WORD)xMin; x <= (WORD)xMax; x++)
        {
            UWORD byteOffset = y * bm->BytesPerRow + (x >> 3);
            UBYTE bitMask = 0x80 >> (x & 7);

            for (UBYTE plane = 0; plane < bm->Depth; plane++)
            {
                if (bm->Planes[plane])
                {
                    if (rp->DrawMode == COMPLEMENT)
                    {
                        /* In COMPLEMENT mode, only XOR planes where pen bit is set */
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

static VOID _graphics_BltPattern ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST PLANEPTR mask __asm("a0"),
                                                        register LONG xMin __asm("d0"),
                                                        register LONG yMin __asm("d1"),
                                                        register LONG xMax __asm("d2"),
                                                        register LONG yMax __asm("d3"),
                                                        register ULONG maskBPR __asm("d4"))
{
    DPRINTF (LOG_ERROR, "_graphics: BltPattern() unimplemented STUB called.\n");
    assert(FALSE);
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

    /* Bounds check */
    if (x < 0 || y < 0 || x >= (bm->BytesPerRow * 8) || y >= bm->Rows)
        return (ULONG)-1;

    UWORD byteOffset = y * bm->BytesPerRow + (x >> 3);
    UBYTE bitMask = 0x80 >> (x & 7);
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
    DPRINTF (LOG_DEBUG, "_graphics: WritePixel() rp=0x%08lx, x=%d, y=%d\n", (ULONG)rp, (int)x, (int)y);

    if (!rp || !rp->BitMap)
        return -1;  /* Error */

    struct BitMap *bm = rp->BitMap;
    BYTE pen = rp->FgPen;
    UBYTE drawmode = rp->DrawMode;

    /* Handle INVERSVID - use BgPen instead of FgPen */
    if (drawmode & INVERSVID)
    {
        pen = rp->BgPen;
    }

    /* Mask out INVERSVID to get base draw mode */
    UBYTE basemode = drawmode & ~INVERSVID;

    /* Bounds check */
    if (x < 0 || y < 0 || x >= (bm->BytesPerRow * 8) || y >= bm->Rows)
        return -1;

    UWORD byteOffset = y * bm->BytesPerRow + (x >> 3);
    UBYTE bitMask = 0x80 >> (x & 7);

    /* Set or clear bits in each plane based on pen color and drawing mode */
    for (UBYTE plane = 0; plane < bm->Depth; plane++)
    {
        if (bm->Planes[plane])
        {
            if (basemode == COMPLEMENT)
            {
                /* In COMPLEMENT mode, only XOR planes where pen bit is set */
                if (pen & (1 << plane))
                {
                    bm->Planes[plane][byteOffset] ^= bitMask;
                }
            }
            else
            {
                /* JAM1 or JAM2 mode */
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

    return 0;  /* Success */
}

static BOOL _graphics_Flood ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register ULONG mode __asm("d2"),
                                                        register LONG x __asm("d0"),
                                                        register LONG y __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_graphics: Flood() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

static VOID _graphics_PolyDraw ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register LONG count __asm("d0"),
                                                        register CONST WORD * polyTable __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: PolyDraw() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: InitView() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: VBeamPos() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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

    /* Calculate bytes per row (word-aligned) */
    UWORD bytesPerRow = (((UWORD)width + 15) >> 4) << 1;  /* Round up to word boundary */

    bitMap->BytesPerRow = bytesPerRow;
    bitMap->Rows = (UWORD)height;
    bitMap->Flags = 0;
    bitMap->Depth = (UBYTE)depth;
    bitMap->pad = 0;

    /* Clear plane pointers - caller must allocate planes */
    for (int i = 0; i < 8; i++)
    {
        bitMap->Planes[i] = NULL;
    }
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
    DPRINTF (LOG_ERROR, "_graphics: ScrollRaster() unimplemented STUB called.\n");
    assert(FALSE);
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

static VOID _graphics_LockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a5"))
{
    DPRINTF (LOG_ERROR, "_graphics: LockLayerRom() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_UnlockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct Layer * layer __asm("a5"))
{
    DPRINTF (LOG_ERROR, "_graphics: UnlockLayerRom() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: OwnBlitter() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_DisownBlitter ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: DisownBlitter() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct TmpRas * _graphics_InitTmpRas ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TmpRas * tmpRas __asm("a0"),
                                                        register PLANEPTR buffer __asm("a1"),
                                                        register LONG size __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: InitTmpRas() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_AskFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register struct TextAttr * textAttr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: AskFont() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_AddFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * textFont __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: AddFont() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_RemFont ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct TextFont * textFont __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: RemFont() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: FreeVPortCopLists() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_FreeCopList ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct CopList * copList __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeCopList() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: ClipBlit() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: FreeCprList() unimplemented STUB called.\n");
    assert(FALSE);
}

static struct ColorMap * _graphics_GetColorMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register LONG entries __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetColorMap() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_FreeColorMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * colorMap __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeColorMap() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_GetRGB4 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ColorMap * colorMap __asm("a0"),
                                                        register LONG entry __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetRGB4() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: SetRGB4CM() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: BltMaskBitMapRastPort() unimplemented STUB called.\n");
    assert(FALSE);
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

static VOID _graphics_AttemptLockLayerRom ( register struct GfxBase * GfxBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_graphics: _graphics_AttemptLockLayerRom() unimplemented STUB called.\n");
    assert(FALSE);
}

//static BOOL _graphics_AttemptLockLayerRom ( register struct GfxBase * GfxBase __asm("a6"),
//                                            register struct Layer * layer __asm("a5"))
//{
//    //DPRINTF (LOG_ERROR, "_graphics: AttemptLockLayerRom() unimplemented STUB called.\n");
//    //assert(FALSE);
//    return FALSE;
//}

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
                                                        register struct BitScaleArgs * bitScaleArgs __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: BitMapScale() unimplemented STUB called.\n");
    assert(FALSE);
}

static UWORD _graphics_ScalerDiv ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG factor __asm("d0"),
                                                        register ULONG numerator __asm("d1"),
                                                        register ULONG denominator __asm("d2"))
{
    DPRINTF (LOG_ERROR, "_graphics: ScalerDiv() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static WORD _graphics_TextExtent ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register LONG count __asm("d0"),
                                                        register struct TextExtent * textExtent __asm("a2"))
{
    DPRINTF (LOG_ERROR, "_graphics: TextExtent() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_TextFit ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a1"),
                                                        register CONST_STRPTR string __asm("a0"),
                                                        register ULONG strLen __asm("d0"),
                                                        register CONST struct TextExtent * textExtent __asm("a2"),
                                                        register CONST struct TextExtent * constrainingExtent __asm("a3"),
                                                        register LONG strDirection __asm("d1"),
                                                        register ULONG constrainingBitWidth __asm("d2"),
                                                        register ULONG constrainingBitHeight __asm("d3"))
{
    DPRINTF (LOG_ERROR, "_graphics: TextFit() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: OpenMonitor() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static BOOL _graphics_CloseMonitor ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct MonitorSpec * monitorSpec __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: CloseMonitor() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

static DisplayInfoHandle _graphics_FindDisplayInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG displayID __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FindDisplayInfo() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_NextDisplayInfo ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG displayID __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: NextDisplayInfo() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: GetDisplayInfoData() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_FontExtent ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct TextFont * font __asm("a0"),
                                                        register struct TextExtent * fontExtent __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: FontExtent() unimplemented STUB called.\n");
    assert(FALSE);
}

static LONG _graphics_ReadPixelLine8 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG xstart __asm("d0"),
                                                        register ULONG ystart __asm("d1"),
                                                        register ULONG width __asm("d2"),
                                                        register UBYTE * array __asm("a2"),
                                                        register struct RastPort * tempRP __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: ReadPixelLine8() unimplemented STUB called.\n");
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
    DPRINTF (LOG_ERROR, "_graphics: WritePixelLine8() unimplemented STUB called.\n");
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
    DPRINTF (LOG_ERROR, "_graphics: WritePixelArray8() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_GetVPModeID ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct ViewPort * vp __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetVPModeID() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static LONG _graphics_ModeNotAvailable ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register ULONG modeID __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: ModeNotAvailable() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: EraseRect() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: GetOutlinePen() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static VOID _graphics_LoadRGB32 ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct ViewPort * vp __asm("a0"),
                                                        register CONST ULONG * table __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: LoadRGB32() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: GetRGB32() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: AllocBitMap() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

static VOID _graphics_FreeBitMap ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct BitMap * bm __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: FreeBitMap() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: GetBitMapAttr() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: SetOutlinePen() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

static ULONG _graphics_SetWriteMask ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register struct RastPort * rp __asm("a0"),
                                                        register ULONG msk __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_graphics: SetWriteMask() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: ScrollRasterBF() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_graphics: SetRPAttrsA() unimplemented STUB called.\n");
    assert(FALSE);
}

static VOID _graphics_GetRPAttrsA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct RastPort * rp __asm("a0"),
                                                        register CONST struct TagItem * tags __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_graphics: GetRPAttrsA() unimplemented STUB called.\n");
    assert(FALSE);
}

static ULONG _graphics_BestModeIDA ( register struct GfxBase * GfxBase __asm("a6"),
                                                        register CONST struct TagItem * tags __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_graphics: BestModeIDA() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
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
    DPRINTF (LOG_ERROR, "_graphics: WriteChunkyPixels() unimplemented STUB called.\n");
    assert(FALSE);
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

