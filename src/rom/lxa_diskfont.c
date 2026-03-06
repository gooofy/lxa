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

/*
 * Helper: case-insensitive string comparison for font names
 */
static int _df_stricmp(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        char c1 = *s1;
        char c2 = *s2;
        /* Simple toupper for ASCII letters */
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        if (c1 != c2)
            return c1 - c2;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/*
 * Helper: check if string ends with a suffix (case-insensitive)
 */
static BOOL _df_endswith(const char *str, const char *suffix)
{
    int slen = strlen(str);
    int xlen = strlen(suffix);
    if (xlen > slen)
        return FALSE;
    return _df_stricmp(str + slen - xlen, suffix) == 0;
}

/*
 * Helper: strip ".font" suffix from a font name into a buffer.
 * Returns pointer to buf (the stripped name).
 */
static char * _df_strip_font_suffix(const char *name, char *buf, int bufsize)
{
    int len = strlen(name);
    int i;

    /* Copy up to bufsize-1 chars */
    for (i = 0; i < len && i < bufsize - 1; i++)
        buf[i] = name[i];
    buf[i] = '\0';

    /* Strip trailing ".font" if present */
    int blen = strlen(buf);
    if (blen > 5 && _df_stricmp(buf + blen - 5, ".font") == 0)
        buf[blen - 5] = '\0';

    return buf;
}

/*
 * Try to load a bitmap font from FONTS: directory.
 *
 * Steps:
 *   1. Build path "FONTS:<fontname>.font"
 *   2. Open and read the FontContentsHeader
 *   3. Find the best matching size in the FontContents entries
 *   4. LoadSeg() the bitmap font file
 *   5. Parse the DiskFontHeader at the start of the loaded segment
 *   6. Fix up pointers in the TextFont struct
 *   7. Register with AddFont()
 *   8. Return the TextFont*
 */
static struct TextFont * _df_load_from_disk(struct TextAttr *textAttr)
{
    char path[300];
    char stripped[64];
    BPTR fh;
    struct FontContentsHeader fch;
    LONG bytesRead;
    UWORD bestSize = 0;
    LONG bestDiff = 0x7FFF;
    char bestFile[MAXFONTPATH];
    UWORD i;

    if (!textAttr || !textAttr->ta_Name)
        return NULL;

    /* Strip ".font" suffix to get the base name */
    _df_strip_font_suffix((const char *)textAttr->ta_Name, stripped, sizeof(stripped));

    DPRINTF (LOG_DEBUG, "_diskfont: _df_load_from_disk() stripped name='%s' requested size=%d\n",
             stripped, textAttr->ta_YSize);

    /* Build path to .font contents file: "FONTS:<stripped>.font" */
    strcpy(path, "FONTS:");
    strcat(path, stripped);
    strcat(path, ".font");

    DPRINTF (LOG_DEBUG, "_diskfont: trying to open '%s'\n", path);

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: could not open '%s'\n", path);
        return NULL;
    }

    /* Read FontContentsHeader (4 bytes) */
    bytesRead = Read(fh, &fch, sizeof(fch));
    if (bytesRead != sizeof(fch))
    {
        DPRINTF (LOG_DEBUG, "_diskfont: failed to read FontContentsHeader\n");
        Close(fh);
        return NULL;
    }

    DPRINTF (LOG_DEBUG, "_diskfont: fch_FileID=0x%04x fch_NumEntries=%d\n",
             fch.fch_FileID, fch.fch_NumEntries);

    /* Validate file ID */
    if (fch.fch_FileID != FCH_ID && fch.fch_FileID != TFCH_ID && fch.fch_FileID != OFCH_ID)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: invalid FileID 0x%04x\n", fch.fch_FileID);
        Close(fh);
        return NULL;
    }

    if (fch.fch_NumEntries == 0)
    {
        Close(fh);
        return NULL;
    }

    /* Read entries to find the best matching size.
     * Both FontContents and TFontContents are 260 bytes each.
     * The fc_YSize/tfc_YSize is at offset 256, fc_Style/tfc_Style at 258, fc_Flags/tfc_Flags at 259.
     */
    bestFile[0] = '\0';

    for (i = 0; i < fch.fch_NumEntries; i++)
    {
        struct FontContents fc;

        bytesRead = Read(fh, &fc, sizeof(fc));
        if (bytesRead != sizeof(fc))
        {
            DPRINTF (LOG_DEBUG, "_diskfont: failed to read FontContents entry %d\n", i);
            break;
        }

        DPRINTF (LOG_DEBUG, "_diskfont: entry %d: file='%s' size=%d style=%d flags=0x%02x\n",
                 i, fc.fc_FileName, fc.fc_YSize, fc.fc_Style, fc.fc_Flags);

        /* Calculate how well this size matches */
        LONG diff = (LONG)fc.fc_YSize - (LONG)textAttr->ta_YSize;
        if (diff < 0)
            diff = -diff;

        /* Prefer exact match, then closest size */
        if (diff < bestDiff || (diff == bestDiff && fc.fc_YSize > bestSize))
        {
            bestDiff = diff;
            bestSize = fc.fc_YSize;
            /* Copy filename */
            {
                UWORD j;
                for (j = 0; j < MAXFONTPATH - 1 && fc.fc_FileName[j]; j++)
                    bestFile[j] = fc.fc_FileName[j];
                bestFile[j] = '\0';
            }
        }
    }

    Close(fh);

    if (bestFile[0] == '\0')
    {
        DPRINTF (LOG_DEBUG, "_diskfont: no matching font entry found\n");
        return NULL;
    }

    DPRINTF (LOG_DEBUG, "_diskfont: best match: file='%s' size=%d (diff=%ld)\n",
             bestFile, bestSize, bestDiff);

    /* Build the full path to the bitmap font file: "FONTS:<bestFile>" */
    strcpy(path, "FONTS:");
    strcat(path, bestFile);

    DPRINTF (LOG_DEBUG, "_diskfont: LoadSeg('%s')...\n", path);

    /* Load the bitmap font as a hunk executable */
    BPTR segList = LoadSeg((STRPTR)path);
    if (!segList)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: LoadSeg('%s') failed\n", path);
        return NULL;
    }

    DPRINTF (LOG_DEBUG, "_diskfont: LoadSeg succeeded, segList=0x%08lx\n", segList);

    /* The loaded segment contains:
     *   ULONG NextSegment   (at BPTR*4, filled by loader, part of alloc header)
     *   ULONG ReturnCode    (MOVEQ #0,D0 : RTS = 0x70004E75)
     *   struct DiskFontHeader dfh
     *
     * The segment data starts at (segList << 2), which is the BPTR->APTR conversion.
     * The first longword at that address is the next segment pointer.
     * The actual data (ReturnCode + DiskFontHeader) follows.
     */
    ULONG *segBase = (ULONG *)(segList << 2);

    /* Skip the NextSegment BPTR at offset 0 (which is the linkage word, NOT part of the data) */
    /* The ReturnCode is the first data word */
    ULONG *dataBase = segBase + 1;  /* skip NextSegment BPTR */

    /* Check for ReturnCode (MOVEQ #0,D0 : RTS = 0x70004E75) */
    if (dataBase[0] != 0x70004E75)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: invalid ReturnCode 0x%08lx (expected 0x70004E75)\n", dataBase[0]);
        /* Try to continue anyway - some fonts may not have this */
    }

    /* DiskFontHeader starts after the ReturnCode longword */
    struct DiskFontHeader *dfh = (struct DiskFontHeader *)(dataBase + 1);

    /* Validate the DFH_ID */
    if (dfh->dfh_FileID != DFH_ID)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: invalid DFH_ID 0x%04x (expected 0x%04x)\n",
                 dfh->dfh_FileID, DFH_ID);
        UnLoadSeg(segList);
        return NULL;
    }

    DPRINTF (LOG_DEBUG, "_diskfont: DFH valid, name='%s' revision=%d\n",
             dfh->dfh_Name, dfh->dfh_Revision);

    /* Get pointer to the TextFont within the DiskFontHeader */
    struct TextFont *tf = &dfh->dfh_TF;

    DPRINTF (LOG_DEBUG, "_diskfont: TextFont: YSize=%d XSize=%d LoChar=%d HiChar=%d Modulo=%d\n",
             tf->tf_YSize, tf->tf_XSize, tf->tf_LoChar, tf->tf_HiChar, tf->tf_Modulo);
    DPRINTF (LOG_DEBUG, "_diskfont: TextFont: CharData=0x%08lx CharLoc=0x%08lx CharSpace=0x%08lx CharKern=0x%08lx\n",
             (ULONG)tf->tf_CharData, (ULONG)tf->tf_CharLoc, (ULONG)tf->tf_CharSpace, (ULONG)tf->tf_CharKern);

    /* Fix up the pointers - they are stored as offsets relative to the start
     * of the DiskFontHeader data. We need to add the base address of the segment
     * to make them absolute pointers.
     *
     * AROS and AmigaOS use the segment start as the relocation base.
     * Since LoadSeg() performs RELOC32 processing, the pointers should
     * already be relocated to absolute addresses. We can use them directly.
     */

    /* Set the font name pointer */
    tf->tf_Message.mn_Node.ln_Name = (char *)dfh->dfh_Name;
    tf->tf_Message.mn_Node.ln_Type = NT_FONT;

    /* Store the segment list so we don't unload it (the font data lives there) */
    dfh->dfh_Segment = segList;

    /* Mark as a disk font */
    tf->tf_Flags &= ~FPF_ROMFONT;
    tf->tf_Flags |= FPF_DISKFONT;

    /* Register with graphics.library so future OpenFont() calls find it */
    AddFont(tf);

    DPRINTF (LOG_DEBUG, "_diskfont: font loaded and registered successfully\n");

    tf->tf_Accessors++;
    return tf;
}

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
        DPRINTF (LOG_DEBUG, "_diskfont: Found font in ROM/memory\n");
        return font;
    }

    /* Try to load from FONTS: directory */
    font = _df_load_from_disk(textAttr);
    if (font)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: Loaded font from disk\n");
        return font;
    }

    DPRINTF (LOG_DEBUG, "_diskfont: Font not found\n");
    return NULL;
}

LONG _diskfont_AvailFonts ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct AvailFontsHeader *buffer      __asm("a0"),
                            register LONG                    bufBytes     __asm("d0"),
                            register ULONG                   flags        __asm("d1"))
{
    LONG bytesNeeded;
    UWORD numEntries = 0;
    struct AvailFonts *af;

    /* Temporary storage for disk font entries (max 64 entries) */
    struct {
        char   name[MAXFONTPATH];
        UWORD  ysize;
        UBYTE  style;
        UBYTE  flags;
    } diskFonts[64];
    UWORD numDiskFonts = 0;

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() called buffer=0x%08lx bufBytes=%ld flags=0x%08lx\n",
             (ULONG)buffer, bufBytes, flags);

    /* Count memory fonts */
    if (flags & AFF_MEMORY)
    {
        numEntries = 1; /* topaz.font */
    }

    /* Scan FONTS: directory for disk fonts */
    if (flags & AFF_DISK)
    {
        BPTR fontsLock = Lock((STRPTR)"FONTS:", ACCESS_READ);
        if (fontsLock)
        {
            struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_PUBLIC | MEMF_CLEAR);
            if (fib)
            {
                if (Examine(fontsLock, fib))
                {
                    while (ExNext(fontsLock, fib) && numDiskFonts < 64)
                    {
                        /* Look for .font files (not directories) */
                        if (fib->fib_DirEntryType < 0 && _df_endswith((const char *)fib->fib_FileName, ".font"))
                        {
                            /* Read this .font contents file to get available sizes */
                            char fontPath[300];
                            BPTR fh;

                            strcpy(fontPath, "FONTS:");
                            strcat(fontPath, (const char *)fib->fib_FileName);

                            fh = Open((STRPTR)fontPath, MODE_OLDFILE);
                            if (fh)
                            {
                                struct FontContentsHeader fch;
                                if (Read(fh, &fch, sizeof(fch)) == sizeof(fch) &&
                                    (fch.fch_FileID == FCH_ID || fch.fch_FileID == TFCH_ID || fch.fch_FileID == OFCH_ID))
                                {
                                    UWORD j;
                                    for (j = 0; j < fch.fch_NumEntries && numDiskFonts < 64; j++)
                                    {
                                        struct FontContents fc;
                                        if (Read(fh, &fc, sizeof(fc)) != sizeof(fc))
                                            break;

                                        /* Add this size to our list */
                                        strcpy(diskFonts[numDiskFonts].name, (const char *)fib->fib_FileName);
                                        diskFonts[numDiskFonts].ysize = fc.fc_YSize;
                                        diskFonts[numDiskFonts].style = fc.fc_Style;
                                        diskFonts[numDiskFonts].flags = fc.fc_Flags;
                                        numDiskFonts++;
                                    }
                                }
                                Close(fh);
                            }
                        }
                    }
                }
                FreeVec(fib);
            }
            UnLock(fontsLock);
        }
        numEntries += numDiskFonts;
    }

    /* Calculate bytes needed */
    bytesNeeded = sizeof(struct AvailFontsHeader) +
                  numEntries * sizeof(struct AvailFonts);

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() numEntries=%d (disk=%d) bytesNeeded=%ld\n",
             numEntries, numDiskFonts, bytesNeeded);

    /* If buffer is NULL or too small, return bytes still needed */
    if (!buffer || bufBytes < bytesNeeded)
    {
        return bytesNeeded;
    }

    /* Fill in the header */
    buffer->afh_NumEntries = numEntries;

    /* Fill in font entries */
    af = (struct AvailFonts *)(buffer + 1);  /* Points after header */

    if (flags & AFF_MEMORY)
    {
        /* Entry for topaz.font size 8 (ROM font) */
        af->af_Type = AFF_MEMORY;
        af->af_Attr.ta_Name = (STRPTR)"topaz.font";
        af->af_Attr.ta_YSize = 8;
        af->af_Attr.ta_Style = 0;  /* FS_NORMAL */
        af->af_Attr.ta_Flags = FPF_ROMFONT | FPF_DESIGNED;
        af++;
    }

    /* Add disk font entries */
    {
        UWORD d;
        for (d = 0; d < numDiskFonts; d++)
        {
            af->af_Type = AFF_DISK;
            af->af_Attr.ta_Name = (STRPTR)diskFonts[d].name;
            af->af_Attr.ta_YSize = diskFonts[d].ysize;
            af->af_Attr.ta_Style = diskFonts[d].style;
            af->af_Attr.ta_Flags = diskFonts[d].flags;
            af++;
        }
    }

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() returning 0 (success)\n");

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
