/*
 * lxa_iprefs.h - the private interface between C:IPrefs and the system
 * libraries (roadmap Phase 236).
 *
 * IPrefs reads ENV:Sys/<name>.prefs and hands what belongs to Intuition to
 * the private intuition.library function SetIPrefs() (LVO -576):
 *
 *     ULONG SetIPrefs(APTR data, ULONG length, ULONG type)   (A0, D0, D1)
 *
 * The type codes and data layouts are the ones AROS documents for this
 * slot (compiler/include/intuition/iprefs.h), so the same calls work for
 * every IPrefs that follows that convention.  The default locale is
 * installed with the private locale.library function LocalePrefsUpdate()
 * (LVO -168, A0 = locale from OpenLocale("ENV:Sys/locale.prefs")).
 */
#ifndef LXA_IPREFS_H
#define LXA_IPREFS_H

#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/text.h>

#define LXA_IPREFS_SCREENMODE   1   /* struct LxaIScreenModePrefs */
#define LXA_IPREFS_FONT         2   /* struct LxaIFontPrefs */
#define LXA_IPREFS_OVERSCAN     3   /* struct LxaIOverscanPrefs */
#define LXA_IPREFS_ICONTROL     4   /* struct LxaIIControlPrefs */
#define LXA_IPREFS_POINTER      7   /* struct LxaIPointerPrefs, returns -1 */
#define LXA_IPREFS_PALETTE      8   /* struct ColorSpec[], ColorIndex -1 ends:
                                     * 0-7 Workbench colours, 8-10 the pointer
                                     * colours 17-19; 16 bit components */
#define LXA_IPREFS_PENS         9   /* struct LxaIPenPrefs */

#define LXA_IPREFS_NUMPENS     12   /* dri pens of AmigaOS 3.1 (NUMDRIPENS) */

struct LxaIScreenModePrefs
{
    ULONG smp_DisplayID;
    UWORD smp_Width;            /* (UWORD)~0: the mode's text overscan */
    UWORD smp_Height;
    UWORD smp_Depth;
    UWORD smp_Control;
};

struct LxaIFontPrefs
{
    struct TextAttr fp_TextAttr;    /* ta_Name points at fp_Name */
    UBYTE           fp_Name[32];
    ULONG           fp_Reserved;
    BOOL            fp_ScrFont;     /* FALSE: system default font
                                     * (GfxBase->DefaultFont),
                                     * TRUE: the screen font */
};

struct LxaIOverscanPrefs
{
    ULONG            os_DisplayID;
    Point            os_ViewPos;
    Point            os_Text;       /* text overscan size */
    struct Rectangle os_Standard;
};

struct LxaIIControlPrefs
{
    UWORD ic_TimeOut;
    WORD  ic_MetaDrag;
    ULONG ic_Flags;
    UBYTE ic_WBtoFront;
    UBYTE ic_FrontToBack;
    UBYTE ic_ReqTrue;
    UBYTE ic_ReqFalse;
};

struct LxaIPointerPrefs
{
    struct BitMap *BitMap;          /* the image, 2 planes */
    WORD  XOffset;                  /* hot spot */
    WORD  YOffset;
    UWORD BytesPerRow;
    UWORD Size;                     /* pointer resolution (pointerclass) */
    UWORD YSize;
    UWORD Which;                    /* 0 normal, 1 busy */
    ULONG Reserved;
};

struct LxaIPenPrefs
{
    UWORD Count;
    UWORD Type;                     /* 0: screens < 8 colours, 1: >= 8 */
    ULONG Reserved;
    UWORD PenTable[LXA_IPREFS_NUMPENS + 1];   /* (UWORD)~0 ends */
};

#endif
