/*
 * lxa_p96.h - Picasso96API.library interface definitions for lxa.
 *
 * lxa's own description of the public Picasso96 2.x programming interface
 * (function offsets, tags, attribute numbers, RGBFTYPE and the RenderInfo /
 * TrueColorInfo / P96Mode structures).  These are interface facts needed
 * for binary compatibility with programs written against Picasso96; the
 * values were checked against the behaviour of Picasso96 2.0 on the `rtg`
 * reference machine (roadmap M4).  No Picasso96 code is used by lxa.
 *
 * Define LXA_P96_INLINES before including this file to get inline calls
 * (probes and test programs; they need P96Base).
 */
#ifndef LXA_P96_H
#define LXA_P96_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <utility/tagitem.h>

#define P96NAME "Picasso96API.library"

/* RGBFTYPE: pixel formats */
typedef ULONG RGBFTYPE;
#define RGBFB_NONE        0
#define RGBFB_CLUT        1     /* 8 bit, palette */
#define RGBFB_R8G8B8      2
#define RGBFB_B8G8R8      3
#define RGBFB_R5G6B5PC    4     /* little-endian 16 bit */
#define RGBFB_R5G5B5PC    5
#define RGBFB_A8R8G8B8    6
#define RGBFB_A8B8G8R8    7
#define RGBFB_R8G8B8A8    8
#define RGBFB_B8G8R8A8    9
#define RGBFB_R5G6B5      10    /* big-endian 16 bit */
#define RGBFB_R5G5B5      11
#define RGBFB_B5G6R5PC    12
#define RGBFB_B5G5R5PC    13
#define RGBFB_Y4U2V2      14
#define RGBFB_Y4U1V1      15
#define RGBFB_MaxFormats  16

#define RGBFF_NONE        (1UL << RGBFB_NONE)
#define RGBFF_CLUT        (1UL << RGBFB_CLUT)
#define RGBFF_R8G8B8      (1UL << RGBFB_R8G8B8)
#define RGBFF_B8G8R8      (1UL << RGBFB_B8G8R8)
#define RGBFF_R5G6B5PC    (1UL << RGBFB_R5G6B5PC)
#define RGBFF_R5G5B5PC    (1UL << RGBFB_R5G5B5PC)
#define RGBFF_A8R8G8B8    (1UL << RGBFB_A8R8G8B8)
#define RGBFF_A8B8G8R8    (1UL << RGBFB_A8B8G8R8)
#define RGBFF_R8G8B8A8    (1UL << RGBFB_R8G8B8A8)
#define RGBFF_B8G8R8A8    (1UL << RGBFB_B8G8R8A8)
#define RGBFF_R5G6B5      (1UL << RGBFB_R5G6B5)
#define RGBFF_R5G5B5      (1UL << RGBFB_R5G5B5)
#define RGBFF_B5G6R5PC    (1UL << RGBFB_B5G6R5PC)
#define RGBFF_B5G5R5PC    (1UL << RGBFB_B5G5R5PC)
#define RGBFF_Y4U2V2      (1UL << RGBFB_Y4U2V2)
#define RGBFF_Y4U1V1      (1UL << RGBFB_Y4U1V1)

#define RGBFF_HICOLOR     (RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_R5G6B5 | RGBFF_R5G5B5 | \
                           RGBFF_B5G6R5PC | RGBFF_B5G5R5PC)
#define RGBFF_TRUECOLOR   (RGBFF_R8G8B8 | RGBFF_B8G8R8)
#define RGBFF_TRUEALPHA   (RGBFF_A8R8G8B8 | RGBFF_A8B8G8R8 | RGBFF_R8G8B8A8 | RGBFF_B8G8R8A8)

/* p96AllocBitMap() flag: a private render buffer, never on a board */
#define BMF_USERPRIVATE   0x8000

/* p96GetBitMapAttr() attributes */
#define P96BMA_WIDTH          0
#define P96BMA_HEIGHT         1
#define P96BMA_DEPTH          2
#define P96BMA_MEMORY         3
#define P96BMA_BYTESPERROW    4
#define P96BMA_BYTESPERPIXEL  5
#define P96BMA_BITSPERPIXEL   6
#define P96BMA_RGBFORMAT      7
#define P96BMA_ISP96          8
#define P96BMA_ISONBOARD      9
#define P96BMA_BOARDMEMBASE   10
#define P96BMA_BOARDIOBASE    11
#define P96BMA_BOARDMEMIOBASE 12

/* p96GetModeIDAttr() attributes */
#define P96IDA_WIDTH              0
#define P96IDA_HEIGHT             1
#define P96IDA_DEPTH              2
#define P96IDA_BYTESPERPIXEL      3
#define P96IDA_BITSPERPIXEL       4
#define P96IDA_RGBFORMAT          5
#define P96IDA_ISP96              6
#define P96IDA_BOARDNUMBER        7
#define P96IDA_STDBYTESPERROW     8
#define P96IDA_BOARDNAME          9
#define P96IDA_COMPATIBLEFORMATS  10
#define P96IDA_VIDEOCOMPATIBLE    11
#define P96IDA_PABLOIVCOMPATIBLE  12
#define P96IDA_PALOMAIVCOMPATIBLE 13

/* p96BestModeIDTagList() */
#define P96BIDTAG_Dummy               (TAG_USER + 96)
#define P96BIDTAG_FormatsAllowed      (P96BIDTAG_Dummy + 0x0001)
#define P96BIDTAG_FormatsForbidden    (P96BIDTAG_Dummy + 0x0002)
#define P96BIDTAG_NominalWidth        (P96BIDTAG_Dummy + 0x0003)
#define P96BIDTAG_NominalHeight       (P96BIDTAG_Dummy + 0x0004)
#define P96BIDTAG_Depth               (P96BIDTAG_Dummy + 0x0005)
#define P96BIDTAG_VideoCompatible     (P96BIDTAG_Dummy + 0x0006)
#define P96BIDTAG_PabloIVCompatible   (P96BIDTAG_Dummy + 0x0007)
#define P96BIDTAG_PalomaIVCompatible  (P96BIDTAG_Dummy + 0x0008)

/* p96RequestModeIDTagList() and p96AllocModeListTagList() */
#define P96MA_Dummy               (TAG_USER + 0x10000 + 96)
#define P96MA_MinWidth            (P96MA_Dummy + 0x0001)
#define P96MA_MinHeight           (P96MA_Dummy + 0x0002)
#define P96MA_MinDepth            (P96MA_Dummy + 0x0003)
#define P96MA_MaxWidth            (P96MA_Dummy + 0x0004)
#define P96MA_MaxHeight           (P96MA_Dummy + 0x0005)
#define P96MA_MaxDepth            (P96MA_Dummy + 0x0006)
#define P96MA_DisplayID           (P96MA_Dummy + 0x0007)
#define P96MA_FormatsAllowed      (P96MA_Dummy + 0x0008)
#define P96MA_FormatsForbidden    (P96MA_Dummy + 0x0009)
#define P96MA_WindowTitle         (P96MA_Dummy + 0x000a)
#define P96MA_OKText              (P96MA_Dummy + 0x000b)
#define P96MA_CancelText          (P96MA_Dummy + 0x000c)
#define P96MA_Window              (P96MA_Dummy + 0x000d)
#define P96MA_PubScreenName       (P96MA_Dummy + 0x000e)
#define P96MA_Screen              (P96MA_Dummy + 0x000f)
#define P96MA_VideoCompatible     (P96MA_Dummy + 0x0010)
#define P96MA_PabloIVCompatible   (P96MA_Dummy + 0x0011)
#define P96MA_PalomaIVCompatible  (P96MA_Dummy + 0x0012)

/* p96OpenScreenTagList() */
#define P96SA_Dummy               (TAG_USER + 0x20000 + 96)
#define P96SA_Left                (P96SA_Dummy + 0x0001)
#define P96SA_Top                 (P96SA_Dummy + 0x0002)
#define P96SA_Width               (P96SA_Dummy + 0x0003)
#define P96SA_Height              (P96SA_Dummy + 0x0004)
#define P96SA_Depth               (P96SA_Dummy + 0x0005)
#define P96SA_DetailPen           (P96SA_Dummy + 0x0006)
#define P96SA_BlockPen            (P96SA_Dummy + 0x0007)
#define P96SA_Title               (P96SA_Dummy + 0x0008)
#define P96SA_Colors              (P96SA_Dummy + 0x0009)
#define P96SA_ErrorCode           (P96SA_Dummy + 0x000a)
#define P96SA_Font                (P96SA_Dummy + 0x000b)
#define P96SA_SysFont             (P96SA_Dummy + 0x000c)
#define P96SA_Type                (P96SA_Dummy + 0x000d)
#define P96SA_BitMap              (P96SA_Dummy + 0x000e)
#define P96SA_PubName             (P96SA_Dummy + 0x000f)
#define P96SA_PubSig              (P96SA_Dummy + 0x0010)
#define P96SA_PubTask             (P96SA_Dummy + 0x0011)
#define P96SA_DisplayID           (P96SA_Dummy + 0x0012)
#define P96SA_DClip               (P96SA_Dummy + 0x0013)
#define P96SA_ShowTitle           (P96SA_Dummy + 0x0014)
#define P96SA_Behind              (P96SA_Dummy + 0x0015)
#define P96SA_Quiet               (P96SA_Dummy + 0x0016)
#define P96SA_AutoScroll          (P96SA_Dummy + 0x0017)
#define P96SA_Pens                (P96SA_Dummy + 0x0018)
#define P96SA_SharePens           (P96SA_Dummy + 0x0019)
#define P96SA_BackFill            (P96SA_Dummy + 0x001a)
#define P96SA_Colors32            (P96SA_Dummy + 0x001b)
#define P96SA_VideoControl        (P96SA_Dummy + 0x001c)
#define P96SA_RGBFormat           (P96SA_Dummy + 0x001d)
#define P96SA_NoSprite            (P96SA_Dummy + 0x001e)
#define P96SA_NoMemory            (P96SA_Dummy + 0x001f)
#define P96SA_RenderFunc          (P96SA_Dummy + 0x0020)
#define P96SA_SaveFunc            (P96SA_Dummy + 0x0021)
#define P96SA_UserData            (P96SA_Dummy + 0x0022)
#define P96SA_Alignment           (P96SA_Dummy + 0x0023)
#define P96SA_FixedScreen         (P96SA_Dummy + 0x0024)
#define P96SA_Exclusive           (P96SA_Dummy + 0x0025)
#define P96SA_ConstantBytesPerRow (P96SA_Dummy + 0x0026)

/* p96PIP_*() */
#define P96PIP_Dummy              (TAG_USER + 0x30000 + 96)
#define P96PIP_SourceFormat       (P96PIP_Dummy + 1)
#define P96PIP_SourceBitMap       (P96PIP_Dummy + 2)
#define P96PIP_SourceRPort        (P96PIP_Dummy + 3)
#define P96PIP_SourceWidth        (P96PIP_Dummy + 4)
#define P96PIP_SourceHeight       (P96PIP_Dummy + 5)
#define P96PIP_Type               (P96PIP_Dummy + 6)
#define P96PIP_ErrorCode          (P96PIP_Dummy + 7)

/* p96GetRTGDataTagList() / p96GetBoardDataTagList() */
#define P96RD_Dummy               (TAG_USER + 0x40000 + 96)
#define P96RD_NumberOfBoards      (P96RD_Dummy + 1)

#define P96BD_Dummy               (TAG_USER + 0x50000 + 96)
#define P96BD_BoardName           (P96BD_Dummy + 1)
#define P96BD_ChipName            (P96BD_Dummy + 2)
#define P96BD_TotalMemory         (P96BD_Dummy + 4)
#define P96BD_FreeMemory          (P96BD_Dummy + 5)
#define P96BD_LargestFreeMemory   (P96BD_Dummy + 6)
#define P96BD_MonitorSwitch       (P96BD_Dummy + 7)
#define P96BD_RGBFormats          (P96BD_Dummy + 8)
#define P96BD_MemoryClock         (P96BD_Dummy + 9)

#define P96_MODENAMELENGTH 48

struct P96Mode
{
    struct Node Node;
    char        Description[P96_MODENAMELENGTH];
    UWORD       Width;
    UWORD       Height;
    UWORD       Depth;
    ULONG       DisplayID;
};

/* a block of pixels in memory */
struct RenderInfo
{
    APTR     Memory;
    WORD     BytesPerRow;
    WORD     pad;
    RGBFTYPE RGBFormat;
};

/* separate red/green/blue component arrays (p96Write/ReadTrueColorData) */
struct TrueColorInfo
{
    ULONG  PixelDistance;
    ULONG  BytesPerRow;
    UBYTE *RedData;
    UBYTE *GreenData;
    UBYTE *BlueData;
};

#ifdef LXA_P96_INLINES
#include <inline/macros.h>

#ifndef P96_BASE_NAME
#define P96_BASE_NAME P96Base
#endif

#define p96AllocBitMap(w, h, d, f, fr, fmt) \
    LP6(0x1e, struct BitMap *, p96AllocBitMap, ULONG, w, d0, ULONG, h, d1, ULONG, d, d2, ULONG, f, d3, \
        struct BitMap *, fr, a0, ULONG, fmt, d7, , P96_BASE_NAME)
#define p96FreeBitMap(bm) \
    LP1NR(0x24, p96FreeBitMap, struct BitMap *, bm, a0, , P96_BASE_NAME)
#define p96GetBitMapAttr(bm, a) \
    LP2(0x2a, ULONG, p96GetBitMapAttr, struct BitMap *, bm, a0, ULONG, a, d0, , P96_BASE_NAME)
#define p96LockBitMap(bm, buf, sz) \
    LP3(0x30, LONG, p96LockBitMap, struct BitMap *, bm, a0, UBYTE *, buf, a1, ULONG, sz, d0, , P96_BASE_NAME)
#define p96UnlockBitMap(bm, lk) \
    LP2NR(0x36, p96UnlockBitMap, struct BitMap *, bm, a0, LONG, lk, d0, , P96_BASE_NAME)
#define p96BestModeIDTagList(t) \
    LP1(0x3c, ULONG, p96BestModeIDTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96RequestModeIDTagList(t) \
    LP1(0x42, ULONG, p96RequestModeIDTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96AllocModeListTagList(t) \
    LP1(0x48, struct List *, p96AllocModeListTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96FreeModeList(l) \
    LP1NR(0x4e, p96FreeModeList, struct List *, l, a0, , P96_BASE_NAME)
#define p96GetModeIDAttr(m, a) \
    LP2(0x54, ULONG, p96GetModeIDAttr, ULONG, m, d0, ULONG, a, d1, , P96_BASE_NAME)
#define p96OpenScreenTagList(t) \
    LP1(0x5a, struct Screen *, p96OpenScreenTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96CloseScreen(s) \
    LP1(0x60, LONG, p96CloseScreen, struct Screen *, s, a0, , P96_BASE_NAME)
#define p96WritePixelArray(ri, sx, sy, rp, dx, dy, w, h) \
    LP8NR(0x66, p96WritePixelArray, struct RenderInfo *, ri, a0, ULONG, sx, d0, ULONG, sy, d1, \
          struct RastPort *, rp, a1, ULONG, dx, d2, ULONG, dy, d3, ULONG, w, d4, ULONG, h, d5, , P96_BASE_NAME)
#define p96ReadPixelArray(ri, dx, dy, rp, sx, sy, w, h) \
    LP8NR(0x6c, p96ReadPixelArray, struct RenderInfo *, ri, a0, ULONG, dx, d0, ULONG, dy, d1, \
          struct RastPort *, rp, a1, ULONG, sx, d2, ULONG, sy, d3, ULONG, w, d4, ULONG, h, d5, , P96_BASE_NAME)
#define p96WritePixel(rp, x, y, c) \
    LP4(0x72, ULONG, p96WritePixel, struct RastPort *, rp, a1, ULONG, x, d0, ULONG, y, d1, ULONG, c, d2, , \
        P96_BASE_NAME)
#define p96ReadPixel(rp, x, y) \
    LP3(0x78, ULONG, p96ReadPixel, struct RastPort *, rp, a1, ULONG, x, d0, ULONG, y, d1, , P96_BASE_NAME)
#define p96RectFill(rp, x0, y0, x1, y1, c) \
    LP6NR(0x7e, p96RectFill, struct RastPort *, rp, a1, ULONG, x0, d0, ULONG, y0, d1, ULONG, x1, d2, \
          ULONG, y1, d3, ULONG, c, d4, , P96_BASE_NAME)
#define p96WriteTrueColorData(tci, sx, sy, rp, dx, dy, w, h) \
    LP8NR(0x84, p96WriteTrueColorData, struct TrueColorInfo *, tci, a0, ULONG, sx, d0, ULONG, sy, d1, \
          struct RastPort *, rp, a1, ULONG, dx, d2, ULONG, dy, d3, ULONG, w, d4, ULONG, h, d5, , P96_BASE_NAME)
#define p96ReadTrueColorData(tci, dx, dy, rp, sx, sy, w, h) \
    LP8NR(0x8a, p96ReadTrueColorData, struct TrueColorInfo *, tci, a0, ULONG, dx, d0, ULONG, dy, d1, \
          struct RastPort *, rp, a1, ULONG, sx, d2, ULONG, sy, d3, ULONG, w, d4, ULONG, h, d5, , P96_BASE_NAME)
#define p96PIP_OpenTagList(t) \
    LP1(0x90, struct Window *, p96PIP_OpenTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96PIP_Close(w) \
    LP1(0x96, LONG, p96PIP_Close, struct Window *, w, a0, , P96_BASE_NAME)
#define p96PIP_SetTagList(w, t) \
    LP2(0x9c, LONG, p96PIP_SetTagList, struct Window *, w, a0, struct TagItem *, t, a1, , P96_BASE_NAME)
#define p96PIP_GetTagList(w, t) \
    LP2(0xa2, LONG, p96PIP_GetTagList, struct Window *, w, a0, struct TagItem *, t, a1, , P96_BASE_NAME)
#define p96GetRTGDataTagList(t) \
    LP1(0xb4, LONG, p96GetRTGDataTagList, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96GetBoardDataTagList(b, t) \
    LP2(0xba, LONG, p96GetBoardDataTagList, ULONG, b, d0, struct TagItem *, t, a0, , P96_BASE_NAME)
#define p96EncodeColor(f, c) \
    LP2(0xc0, ULONG, p96EncodeColor, ULONG, f, d0, ULONG, c, d1, , P96_BASE_NAME)
#endif /* LXA_P96_INLINES */

#endif /* LXA_P96_H */
