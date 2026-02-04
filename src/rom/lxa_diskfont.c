/*
 * lxa diskfont.library implementation
 *
 * Provides font loading from disk (FONTS: assign) and font enumeration.
 * Currently a minimal implementation with stubs for most functions.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <graphics/gfx.h>
#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <diskfont/diskfont.h>
#include <diskfont/oterrors.h>

#include "util.h"

#define VERSION    45
#define REVISION   1
#define EXLIBNAME  "diskfont"
#define EXLIBVER   " 45.1 (2025/02/01)"

char __aligned _g_diskfont_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_diskfont_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_diskfont_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_diskfont_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;
extern struct DosLibrary    *DOSBase;
extern struct GfxBase       *GfxBase;

/* DiskfontBase structure */
struct DiskfontBase {
    struct Library lib;
    UWORD          Pad;
    BPTR           SegList;
};

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct DiskfontBase * __g_lxa_diskfont_InitLib ( register struct DiskfontBase *dfb    __asm("d0"),
                                                  register BPTR                seglist __asm("a0"),
                                                  register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: InitLib() called\n");
    dfb->SegList = seglist;
    return dfb;
}

struct DiskfontBase * __g_lxa_diskfont_OpenLib ( register struct DiskfontBase *DiskfontBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: OpenLib() called\n");
    DiskfontBase->lib.lib_OpenCnt++;
    DiskfontBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return DiskfontBase;
}

BPTR __g_lxa_diskfont_CloseLib ( register struct DiskfontBase *dfb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: CloseLib() called\n");
    dfb->lib.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_diskfont_ExpungeLib ( register struct DiskfontBase *dfb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_diskfont_ExtFuncLib(void)
{
    return 0;
}

/****************************************************************************/
/* Main functions                                                            */
/****************************************************************************/

struct TextFont * _diskfont_OpenDiskFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                           register struct TextAttr     *textAttr     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: OpenDiskFont() called name='%s' size=%d\n",
             textAttr ? (char *)textAttr->ta_Name : "(null)",
             textAttr ? textAttr->ta_YSize : 0);
    
    if (!textAttr || !textAttr->ta_Name)
        return NULL;
    
    /* First try to open from ROM font list via graphics.library */
    struct TextFont *font = OpenFont(textAttr);
    if (font)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: Found font in ROM\n");
        return font;
    }
    
    /* TODO: Try to load from FONTS: directory
     * This would involve:
     * 1. Lock FONTS:
     * 2. Look for <fontname>.font file
     * 3. Parse the .font contents file
     * 4. Load the appropriate size bitmap font
     * 5. Register with graphics.library
     * For now, just return NULL if not in ROM
     */
    
    DPRINTF (LOG_DEBUG, "_diskfont: Font not found\n");
    return NULL;
}

LONG _diskfont_AvailFonts ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct AvailFontsHeader *buffer      __asm("a0"),
                            register LONG                    bufBytes     __asm("d0"),
                            register ULONG                   flags        __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() called buffer=0x%08lx bufBytes=%ld flags=0x%08lx\n",
             buffer, bufBytes, flags);
    
    if (!buffer || bufBytes < (LONG)sizeof(struct AvailFontsHeader))
        return bufBytes; /* Return bytes still needed */
    
    /* Initialize header */
    buffer->afh_NumEntries = 0;
    
    /* TODO: Enumerate ROM fonts and/or disk fonts based on flags:
     * AFF_MEMORY - include fonts in memory (ROM fonts)
     * AFF_DISK - include fonts on disk
     * AFF_SCALED - include scaled fonts
     * AFF_BITMAP - include bitmap fonts
     * AFF_TAGGED - use tagged format
     *
     * For now, return empty list
     */
    
    return 0; /* 0 = success, all fonts fit in buffer */
}

/****************************************************************************/
/* V34+ functions                                                            */
/****************************************************************************/

struct FontContentsHeader * _diskfont_NewFontContents ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                        register BPTR                 fontsLock    __asm("a0"),
                                                        register CONST_STRPTR         fontName     __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: NewFontContents() unimplemented STUB called\n");
    return NULL;
}

VOID _diskfont_DisposeFontContents ( register struct DiskfontBase       *DiskfontBase        __asm("a6"),
                                     register struct FontContentsHeader *fontContentsHeader  __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: DisposeFontContents() unimplemented STUB called\n");
}

/****************************************************************************/
/* V36+ functions                                                            */
/****************************************************************************/

struct DiskFont * _diskfont_NewScaledDiskFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                register struct TextFont     *sourceFont   __asm("a0"),
                                                register struct TextAttr     *destTextAttr __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: NewScaledDiskFont() unimplemented STUB called\n");
    return NULL;
}

/****************************************************************************/
/* V45+ functions                                                            */
/****************************************************************************/

LONG _diskfont_GetDiskFontCtrl ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                 register LONG                 tagid        __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: GetDiskFontCtrl() unimplemented STUB called\n");
    return 0;
}

VOID _diskfont_SetDiskFontCtrlA ( register struct DiskfontBase     *DiskfontBase __asm("a6"),
                                  register CONST struct TagItem    *taglist      __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: SetDiskFontCtrlA() unimplemented STUB called\n");
}

/****************************************************************************/
/* V47+ functions (outline font support)                                     */
/****************************************************************************/

LONG _diskfont_EOpenEngine ( register struct DiskfontBase  *DiskfontBase __asm("a6"),
                             register struct EGlyphEngine  *eEngine      __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: EOpenEngine() unimplemented STUB called\n");
    return OTERR_UnknownTag;
}

VOID _diskfont_ECloseEngine ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                              register struct EGlyphEngine *eEngine      __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: ECloseEngine() unimplemented STUB called\n");
}

ULONG _diskfont_ESetInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct EGlyphEngine    *eEngine      __asm("a0"),
                            register CONST struct TagItem   *taglist      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: ESetInfoA() unimplemented STUB called\n");
    return OTERR_UnknownTag;
}

ULONG _diskfont_EObtainInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                               register struct EGlyphEngine    *eEngine      __asm("a0"),
                               register CONST struct TagItem   *taglist      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: EObtainInfoA() unimplemented STUB called\n");
    return OTERR_UnknownTag;
}

ULONG _diskfont_EReleaseInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                                register struct EGlyphEngine    *eEngine      __asm("a0"),
                                register CONST struct TagItem   *taglist      __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: EReleaseInfoA() unimplemented STUB called\n");
    return 0;
}

struct OutlineFont * _diskfont_OpenOutlineFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                 register CONST_STRPTR         name         __asm("a0"),
                                                 register struct List          *list        __asm("a1"),
                                                 register ULONG                flags        __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: OpenOutlineFont() unimplemented STUB called\n");
    return NULL;
}

VOID _diskfont_CloseOutlineFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                  register struct OutlineFont  *olf          __asm("a0"),
                                  register struct List         *list         __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: CloseOutlineFont() unimplemented STUB called\n");
}

LONG _diskfont_WriteFontContents ( register struct DiskfontBase        *DiskfontBase        __asm("a6"),
                                   register BPTR                        fontsLock           __asm("a0"),
                                   register CONST_STRPTR                fontName            __asm("a1"),
                                   register CONST struct FontContentsHeader *fontContentsHeader __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: WriteFontContents() unimplemented STUB called\n");
    return FALSE;
}

LONG _diskfont_WriteDiskFontHeaderA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                                      register CONST struct TextFont  *font         __asm("a0"),
                                      register CONST_STRPTR            fileName     __asm("a1"),
                                      register CONST struct TagItem   *tagList      __asm("a2"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: WriteDiskFontHeaderA() unimplemented STUB called\n");
    return FALSE;
}

ULONG _diskfont_ObtainCharsetInfo ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                    register ULONG                knownTag     __asm("d0"),
                                    register ULONG                knownValue   __asm("d1"),
                                    register ULONG                wantedTag    __asm("d2"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: ObtainCharsetInfo() unimplemented STUB called\n");
    return 0;
}

/****************************************************************************/
/* ROMTag and library initialization                                         */
/****************************************************************************/

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

extern APTR              __g_lxa_diskfont_FuncTab [];
extern struct MyDataInit __g_lxa_diskfont_DataTab;
extern struct InitTable  __g_lxa_diskfont_InitTab;
extern APTR              __g_lxa_diskfont_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                      // UWORD rt_MatchWord
    &ROMTag,                            // struct Resident *rt_MatchTag
    &__g_lxa_diskfont_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                       // UBYTE rt_Flags
    VERSION,                            // UBYTE rt_Version
    NT_LIBRARY,                         // UBYTE rt_Type
    0,                                  // BYTE  rt_Pri
    &_g_diskfont_ExLibName[0],          // char  *rt_Name
    &_g_diskfont_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_diskfont_InitTab           // APTR  rt_Init
};

APTR __g_lxa_diskfont_EndResident;
struct Resident *__lxa_diskfont_ROMTag = &ROMTag;

struct InitTable __g_lxa_diskfont_InitTab =
{
    (ULONG)               sizeof(struct DiskfontBase),
    (APTR              *) &__g_lxa_diskfont_FuncTab[0],
    (APTR)                &__g_lxa_diskfont_DataTab,
    (APTR)                __g_lxa_diskfont_InitLib
};

/* Function table - offsets from diskfont_pragmas.h */
APTR __g_lxa_diskfont_FuncTab [] =
{
    __g_lxa_diskfont_OpenLib,           // -6   (0x06) pos 0
    __g_lxa_diskfont_CloseLib,          // -12  (0x0c) pos 1
    __g_lxa_diskfont_ExpungeLib,        // -18  (0x12) pos 2
    __g_lxa_diskfont_ExtFuncLib,        // -24  (0x18) pos 3
    _diskfont_OpenDiskFont,             // -30  (0x1e) pos 4
    _diskfont_AvailFonts,               // -36  (0x24) pos 5
    _diskfont_NewFontContents,          // -42  (0x2a) pos 6 (V34)
    _diskfont_DisposeFontContents,      // -48  (0x30) pos 7 (V34)
    _diskfont_NewScaledDiskFont,        // -54  (0x36) pos 8 (V36)
    _diskfont_GetDiskFontCtrl,          // -60  (0x3c) pos 9 (V45)
    _diskfont_SetDiskFontCtrlA,         // -66  (0x42) pos 10 (V45)
    _diskfont_EOpenEngine,              // -72  (0x48) pos 11 (V47)
    _diskfont_ECloseEngine,             // -78  (0x4e) pos 12 (V47)
    _diskfont_ESetInfoA,                // -84  (0x54) pos 13 (V47)
    _diskfont_EObtainInfoA,             // -90  (0x5a) pos 14 (V47)
    _diskfont_EReleaseInfoA,            // -96  (0x60) pos 15 (V47)
    _diskfont_OpenOutlineFont,          // -102 (0x66) pos 16 (V47)
    _diskfont_CloseOutlineFont,         // -108 (0x6c) pos 17 (V47)
    _diskfont_WriteFontContents,        // -114 (0x72) pos 18 (V47)
    _diskfont_WriteDiskFontHeaderA,     // -120 (0x78) pos 19 (V47)
    _diskfont_ObtainCharsetInfo,        // -126 (0x7e) pos 20 (V47)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_diskfont_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_diskfont_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_diskfont_ExLibID[0],
    (ULONG) 0
};
