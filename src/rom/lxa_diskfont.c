/*
 * lxa diskfont.library implementation
 *
 * Provides font loading from disk (FONTS: assign), font enumeration
 * (AvailFonts), font contents handling and scaled disk fonts.
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
#include <dos/doshunks.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <graphics/gfx.h>
#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include <diskfont/diskfont.h>
#include <diskfont/glyph.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>

#include <utility/tagitem.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

#define VERSION    45
#define REVISION   1
#define EXLIBNAME  "diskfont"
#define EXLIBVER   " 45.1 (2025/02/01)"

char __aligned _g_diskfont_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_diskfont_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_diskfont_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_diskfont_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;
extern struct DosLibrary    *DOSBase;
extern struct GfxBase       *GfxBase;
extern struct UtilityBase   *UtilityBase;

/* DiskfontBase structure */
struct DiskfontBase {
    struct Library lib;
    UWORD          Pad;
    BPTR           SegList;
    LONG           xdpi;
    LONG           ydpi;
    LONG           xdotp;
    LONG           ydotp;
    LONG           cache_enabled;
    LONG           sort_mode;
    ULONG          charset;
    struct TextAttr cached_attr;
    struct TextFont *cached_font;
    struct TextFont *cached_scaled_font;
};

struct CachedFontEntry
{
    struct MinNode  node;
    char            name[MAXFONTPATH];
    UWORD           ysize;
    UBYTE           style;
    UBYTE           flags;
    struct TextFont *font;
};

struct DiskFontCacheState
{
    struct MinList font_entries;
};

struct DiskfontGlyphEngineState
{
    struct GlyphEngine glyph_engine;
    STRPTR             glyph_name;
    STRPTR             current_otag_path;
    struct TagItem    *current_otag_list;
    BOOL               owns_current_otag_path;
    BOOL               owns_current_otag_list;
    ULONG              device_dpi;
    ULONG              dot_size;
    ULONG              point_height;
    ULONG              set_factor;
    ULONG              shear_sin;
    ULONG              shear_cos;
    ULONG              rotate_sin;
    ULONG              rotate_cos;
    ULONG              embolden_x;
    ULONG              embolden_y;
    ULONG              point_size;
    ULONG              glyph_code;
    ULONG              glyph_code2;
    ULONG              glyph_code32;
    ULONG              glyph_code2_32;
    ULONG              glyph_width;
    ULONG              underlined;
};

struct DiskfontOutlinePrivate
{
    struct Library *engine_library;
};

struct df_color_diskfont_header
{
    struct Node          dfh_DF;
    UWORD                dfh_FileID;
    UWORD                dfh_Revision;
    BPTR                 dfh_Segment;
    TEXT                 dfh_Name[MAXFONTNAME];
    struct ColorTextFont ctf;
};

struct df_charset_info
{
    ULONG        number;
    CONST_STRPTR name;
    CONST_STRPTR mime_name;
    CONST ULONG *map_table;
};

static const ULONG g_df_charset_ascii_map[256] = {
    0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000006, 0x00000007,
    0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c, 0x0000000d, 0x0000000e, 0x0000000f,
    0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014, 0x00000015, 0x00000016, 0x00000017,
    0x00000018, 0x00000019, 0x0000001a, 0x0000001b, 0x0000001c, 0x0000001d, 0x0000001e, 0x0000001f,
    0x00000020, 0x00000021, 0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000026, 0x00000027,
    0x00000028, 0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d, 0x0000002e, 0x0000002f,
    0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034, 0x00000035, 0x00000036, 0x00000037,
    0x00000038, 0x00000039, 0x0000003a, 0x0000003b, 0x0000003c, 0x0000003d, 0x0000003e, 0x0000003f,
    0x00000040, 0x00000041, 0x00000042, 0x00000043, 0x00000044, 0x00000045, 0x00000046, 0x00000047,
    0x00000048, 0x00000049, 0x0000004a, 0x0000004b, 0x0000004c, 0x0000004d, 0x0000004e, 0x0000004f,
    0x00000050, 0x00000051, 0x00000052, 0x00000053, 0x00000054, 0x00000055, 0x00000056, 0x00000057,
    0x00000058, 0x00000059, 0x0000005a, 0x0000005b, 0x0000005c, 0x0000005d, 0x0000005e, 0x0000005f,
    0x00000060, 0x00000061, 0x00000062, 0x00000063, 0x00000064, 0x00000065, 0x00000066, 0x00000067,
    0x00000068, 0x00000069, 0x0000006a, 0x0000006b, 0x0000006c, 0x0000006d, 0x0000006e, 0x0000006f,
    0x00000070, 0x00000071, 0x00000072, 0x00000073, 0x00000074, 0x00000075, 0x00000076, 0x00000077,
    0x00000078, 0x00000079, 0x0000007a, 0x0000007b, 0x0000007c, 0x0000007d, 0x0000007e, 0x0000007f,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd,
    0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd, 0x0000fffd
};

static const ULONG g_df_charset_identity_map[256] = {
    0x00000000, 0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005, 0x00000006, 0x00000007,
    0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c, 0x0000000d, 0x0000000e, 0x0000000f,
    0x00000010, 0x00000011, 0x00000012, 0x00000013, 0x00000014, 0x00000015, 0x00000016, 0x00000017,
    0x00000018, 0x00000019, 0x0000001a, 0x0000001b, 0x0000001c, 0x0000001d, 0x0000001e, 0x0000001f,
    0x00000020, 0x00000021, 0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000026, 0x00000027,
    0x00000028, 0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d, 0x0000002e, 0x0000002f,
    0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034, 0x00000035, 0x00000036, 0x00000037,
    0x00000038, 0x00000039, 0x0000003a, 0x0000003b, 0x0000003c, 0x0000003d, 0x0000003e, 0x0000003f,
    0x00000040, 0x00000041, 0x00000042, 0x00000043, 0x00000044, 0x00000045, 0x00000046, 0x00000047,
    0x00000048, 0x00000049, 0x0000004a, 0x0000004b, 0x0000004c, 0x0000004d, 0x0000004e, 0x0000004f,
    0x00000050, 0x00000051, 0x00000052, 0x00000053, 0x00000054, 0x00000055, 0x00000056, 0x00000057,
    0x00000058, 0x00000059, 0x0000005a, 0x0000005b, 0x0000005c, 0x0000005d, 0x0000005e, 0x0000005f,
    0x00000060, 0x00000061, 0x00000062, 0x00000063, 0x00000064, 0x00000065, 0x00000066, 0x00000067,
    0x00000068, 0x00000069, 0x0000006a, 0x0000006b, 0x0000006c, 0x0000006d, 0x0000006e, 0x0000006f,
    0x00000070, 0x00000071, 0x00000072, 0x00000073, 0x00000074, 0x00000075, 0x00000076, 0x00000077,
    0x00000078, 0x00000079, 0x0000007a, 0x0000007b, 0x0000007c, 0x0000007d, 0x0000007e, 0x0000007f,
    0x00000080, 0x00000081, 0x00000082, 0x00000083, 0x00000084, 0x00000085, 0x00000086, 0x00000087,
    0x00000088, 0x00000089, 0x0000008a, 0x0000008b, 0x0000008c, 0x0000008d, 0x0000008e, 0x0000008f,
    0x00000090, 0x00000091, 0x00000092, 0x00000093, 0x00000094, 0x00000095, 0x00000096, 0x00000097,
    0x00000098, 0x00000099, 0x0000009a, 0x0000009b, 0x0000009c, 0x0000009d, 0x0000009e, 0x0000009f,
    0x000000a0, 0x000000a1, 0x000000a2, 0x000000a3, 0x000000a4, 0x000000a5, 0x000000a6, 0x000000a7,
    0x000000a8, 0x000000a9, 0x000000aa, 0x000000ab, 0x000000ac, 0x000000ad, 0x000000ae, 0x000000af,
    0x000000b0, 0x000000b1, 0x000000b2, 0x000000b3, 0x000000b4, 0x000000b5, 0x000000b6, 0x000000b7,
    0x000000b8, 0x000000b9, 0x000000ba, 0x000000bb, 0x000000bc, 0x000000bd, 0x000000be, 0x000000bf,
    0x000000c0, 0x000000c1, 0x000000c2, 0x000000c3, 0x000000c4, 0x000000c5, 0x000000c6, 0x000000c7,
    0x000000c8, 0x000000c9, 0x000000ca, 0x000000cb, 0x000000cc, 0x000000cd, 0x000000ce, 0x000000cf,
    0x000000d0, 0x000000d1, 0x000000d2, 0x000000d3, 0x000000d4, 0x000000d5, 0x000000d6, 0x000000d7,
    0x000000d8, 0x000000d9, 0x000000da, 0x000000db, 0x000000dc, 0x000000dd, 0x000000de, 0x000000df,
    0x000000e0, 0x000000e1, 0x000000e2, 0x000000e3, 0x000000e4, 0x000000e5, 0x000000e6, 0x000000e7,
    0x000000e8, 0x000000e9, 0x000000ea, 0x000000eb, 0x000000ec, 0x000000ed, 0x000000ee, 0x000000ef,
    0x000000f0, 0x000000f1, 0x000000f2, 0x000000f3, 0x000000f4, 0x000000f5, 0x000000f6, 0x000000f7,
    0x000000f8, 0x000000f9, 0x000000fa, 0x000000fb, 0x000000fc, 0x000000fd, 0x000000fe, 0x000000ff
};

static const struct df_charset_info g_df_charsets[] = {
    { 3,   (CONST_STRPTR)"US-ASCII",   (CONST_STRPTR)"US-ASCII",   g_df_charset_ascii_map },
    { 4,   (CONST_STRPTR)"ISO-8859-1", (CONST_STRPTR)"ISO-8859-1", g_df_charset_identity_map },
    { 106, (CONST_STRPTR)"UTF-8",      (CONST_STRPTR)"UTF-8",      g_df_charset_identity_map }
};

static struct DiskFontCacheState g_diskfont_cache_state;

static VOID _df_new_min_list(struct MinList *list);
static VOID _df_flush_cache(struct DiskfontBase *DiskfontBase);
static int _df_stricmp(const char *s1, const char *s2);
static BOOL _df_endswith(const char *str, const char *suffix);
static BOOL _df_has_path(const char *name);
LONG _diskfont_EOpenEngine ( register struct DiskfontBase  *DiskfontBase __asm("a6"),
                             register struct EGlyphEngine  *eEngine      __asm("a0"));
VOID _diskfont_ECloseEngine ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                              register struct EGlyphEngine *eEngine      __asm("a0"));
ULONG _diskfont_ESetInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct EGlyphEngine    *eEngine      __asm("a0"),
                            register CONST struct TagItem   *taglist      __asm("a1"));
VOID _diskfont_CloseOutlineFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                  register struct OutlineFont  *olf          __asm("a0"),
                                  register struct List         *list         __asm("a1"));

static STRPTR _df_strdup(CONST_STRPTR src)
{
    ULONG len;
    STRPTR copy;

    if (!src)
        return NULL;

    len = strlen((const char *)src) + 1;
    copy = (STRPTR)AllocMem(len, MEMF_PUBLIC);
    if (!copy)
        return NULL;

    CopyMem((APTR)src, copy, len);
    return copy;
}

static VOID _df_copy_name(TEXT *dst, CONST_STRPTR src, ULONG max_len)
{
    ULONG i;

    if (!dst || max_len == 0)
        return;

    for (i = 0; i + 1 < max_len && src && src[i]; i++)
        dst[i] = (TEXT)src[i];

    dst[i] = 0;
}

static struct df_charset_info *_df_find_charset_by_number(ULONG number)
{
    ULONG i;

    for (i = 0; i < (sizeof(g_df_charsets) / sizeof(g_df_charsets[0])); i++)
    {
        if (g_df_charsets[i].number == number)
            return (struct df_charset_info *)&g_df_charsets[i];
    }

    return NULL;
}

static struct df_charset_info *_df_find_charset_by_string(ULONG tag, CONST_STRPTR value)
{
    ULONG i;

    if (!value)
        return NULL;

    for (i = 0; i < (sizeof(g_df_charsets) / sizeof(g_df_charsets[0])); i++)
    {
        CONST_STRPTR probe = (tag == DFCS_MIMENAME) ? g_df_charsets[i].mime_name : g_df_charsets[i].name;

        if (probe && _df_stricmp((const char *)probe, (const char *)value) == 0)
            return (struct df_charset_info *)&g_df_charsets[i];
    }

    return NULL;
}

static struct df_charset_info *_df_find_charset_next(ULONG after_number)
{
    ULONG i;
    struct df_charset_info *best = NULL;

    for (i = 0; i < (sizeof(g_df_charsets) / sizeof(g_df_charsets[0])); i++)
    {
        if (g_df_charsets[i].number > after_number)
        {
            if (!best || g_df_charsets[i].number < best->number)
                best = (struct df_charset_info *)&g_df_charsets[i];
        }
    }

    return best;
}

static struct df_charset_info *_df_find_charset(ULONG knownTag, ULONG knownValue)
{
    switch (knownTag)
    {
        case DFCS_NUMBER:
            return _df_find_charset_by_number(knownValue);

        case DFCS_NAME:
            return _df_find_charset_by_string(knownTag, (CONST_STRPTR)knownValue);

        case DFCS_MIMENAME:
            return _df_find_charset_by_string(knownTag, (CONST_STRPTR)knownValue);

        case DFCS_NEXTNUMBER:
            return _df_find_charset_next(knownValue);

        default:
            return NULL;
    }
}

static struct DiskfontGlyphEngineState *_df_glyph_state(struct EGlyphEngine *eEngine)
{
    if (!eEngine)
        return NULL;

    return (struct DiskfontGlyphEngineState *)eEngine->ege_Reserved;
}

static VOID _df_glyph_state_defaults(struct DiskfontGlyphEngineState *state)
{
    if (!state)
        return;

    state->device_dpi = (72UL << 16) | 72UL;
    state->dot_size = (100UL << 16) | 100UL;
    state->set_factor = 0x00010000UL;
    state->shear_cos = 0x00010000UL;
    state->rotate_cos = 0x00010000UL;
    state->underlined = OTUL_None;
}

static VOID _df_glyph_state_free(struct DiskfontGlyphEngineState *state)
{
    if (!state)
        return;

    if (state->current_otag_path && state->owns_current_otag_path)
        FreeMem(state->current_otag_path, strlen((const char *)state->current_otag_path) + 1);
    if (state->current_otag_list && state->owns_current_otag_list)
        FreeMem(state->current_otag_list, state->current_otag_list[0].ti_Data);
    if (state->glyph_name)
        FreeMem(state->glyph_name, strlen((const char *)state->glyph_name) + 1);
    FreeMem(state, sizeof(*state));
}

static BOOL _df_otag_build_path(CONST_STRPTR name, char *path, ULONG path_size)
{
    ULONG len;

    if (!name || !path || path_size < 8)
        return FALSE;

    path[0] = '\0';
    if (_df_has_path((const char *)name))
    {
        if (strlen((const char *)name) + 1 > path_size)
            return FALSE;

        strcpy(path, (const char *)name);
    }
    else
    {
        strcpy(path, "FONTS:");
        if (!AddPart((STRPTR)path, (STRPTR)name, path_size))
            return FALSE;
    }

    len = strlen(path);
    if (_df_endswith(path, ".font"))
    {
        if (len + 1 < 5)
            return FALSE;

        path[len - 4] = 'o';
        path[len - 3] = 't';
        path[len - 2] = 'a';
        path[len - 1] = 'g';
    }
    else if (!_df_endswith(path, ".otag"))
    {
        if (len + strlen(OTSUFFIX) + 1 > path_size)
            return FALSE;

        strcat(path, OTSUFFIX);
    }

    return TRUE;
}

static BOOL _df_read_file(CONST_STRPTR path, UBYTE **data_out, ULONG *size_out)
{
    BPTR fh;
    LONG size;
    UBYTE *data;

    if (!path || !data_out || !size_out)
        return FALSE;

    *data_out = NULL;
    *size_out = 0;

    fh = Open(path, MODE_OLDFILE);
    if (!fh)
        return FALSE;

    /* Seek() returns the previous position: seek to the end, then the
     * seek back to the start yields the file size */
    if (Seek(fh, 0, OFFSET_END) < 0 || (size = Seek(fh, 0, OFFSET_BEGINNING)) <= 0)
    {
        Close(fh);
        return FALSE;
    }

    data = (UBYTE *)AllocMem((ULONG)size, MEMF_PUBLIC);
    if (!data)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        Close(fh);
        return FALSE;
    }

    if (Read(fh, data, size) != size)
    {
        FreeMem(data, (ULONG)size);
        Close(fh);
        return FALSE;
    }

    Close(fh);
    *data_out = data;
    *size_out = (ULONG)size;
    return TRUE;
}

static BOOL _df_validate_otag_list(struct TagItem *taglist, ULONG size)
{
    ULONG tag_count = 0;
    BOOL saw_file_ident = FALSE;
    BOOL saw_engine = FALSE;
    UBYTE *base = (UBYTE *)taglist;

    while (((UBYTE *)&taglist[tag_count] + sizeof(struct TagItem)) <= (base + size))
    {
        struct TagItem *tag = &taglist[tag_count];

        if (tag_count == 0)
        {
            if (tag->ti_Tag != OT_FileIdent || tag->ti_Data != size)
                return FALSE;

            saw_file_ident = TRUE;
        }

        if (tag->ti_Tag == TAG_DONE)
            return saw_file_ident && saw_engine;

        if (tag->ti_Tag == OT_Engine)
            saw_engine = TRUE;

        if (tag->ti_Tag & OT_Indirect)
        {
            if (tag->ti_Data >= size)
                return FALSE;

            tag->ti_Data = (ULONG)(base + tag->ti_Data);
        }

        tag_count++;
    }

    return FALSE;
}

static ULONG _df_font_char_count(CONST struct TextFont *font)
{
    if (!font || font->tf_HiChar < font->tf_LoChar)
        return 0;

    return (ULONG)(font->tf_HiChar - font->tf_LoChar + 1);
}

static BOOL _df_write_all(BPTR fh, CONST_APTR data, ULONG size)
{
    return fh && size >= 0 && Write(fh, data, (LONG)size) == (LONG)size;
}

static LONG _df_write_diskfont_hunk(CONST_STRPTR fileName,
                                    UBYTE *payload,
                                    ULONG payload_size,
                                    ULONG *reloc_offsets,
                                    ULONG reloc_count)
{
    BPTR fh;
    ULONG hunk_header = HUNK_HEADER;
    ULONG hunk_code = HUNK_CODE;
    ULONG hunk_reloc32 = HUNK_RELOC32;
    ULONG hunk_end = HUNK_END;
    ULONG zero = 0;
    ULONG one = 1;
    ULONG hunk_size_longs;
    ULONG padded_size;
    UBYTE pad[4] = {0, 0, 0, 0};

    if (!fileName || !payload)
        return FALSE;

    fh = Open(fileName, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    hunk_size_longs = (payload_size + 3) / 4;
    padded_size = hunk_size_longs * 4;

    if (!_df_write_all(fh, &hunk_header, 4) ||
        !_df_write_all(fh, &zero, 4) ||
        !_df_write_all(fh, &one, 4) ||
        !_df_write_all(fh, &zero, 4) ||
        !_df_write_all(fh, &zero, 4) ||
        !_df_write_all(fh, &hunk_size_longs, 4) ||
        !_df_write_all(fh, &hunk_code, 4) ||
        !_df_write_all(fh, &hunk_size_longs, 4) ||
        !_df_write_all(fh, payload, payload_size) ||
        (padded_size > payload_size && !_df_write_all(fh, pad, padded_size - payload_size)) ||
        !_df_write_all(fh, &hunk_reloc32, 4) ||
        !_df_write_all(fh, &reloc_count, 4) ||
        !_df_write_all(fh, &zero, 4) ||
        (reloc_count > 0 && !_df_write_all(fh, reloc_offsets, reloc_count * sizeof(ULONG))) ||
        !_df_write_all(fh, &zero, 4) ||
        !_df_write_all(fh, &hunk_end, 4))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct DiskfontBase * __g_lxa_diskfont_InitLib ( register struct DiskfontBase *dfb    __asm("d0"),
                                                  register BPTR                seglist __asm("a0"),
                                                  register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_diskfont: InitLib() called\n");
    dfb->SegList = seglist;
    dfb->xdpi = 72;
    dfb->ydpi = 72;
    dfb->xdotp = 100;
    dfb->ydotp = 100;
    dfb->cache_enabled = FALSE;
    dfb->sort_mode = DFCTRL_SORT_OFF;
    dfb->charset = 0;
    memset(&dfb->cached_attr, 0, sizeof(dfb->cached_attr));
    dfb->cached_font = NULL;
    dfb->cached_scaled_font = NULL;
    _df_new_min_list(&g_diskfont_cache_state.font_entries);
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
    PRIVATE_FUNCTION_ERROR("_diskfont", "ExtFuncLib");
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

static ULONG _df_font_contents_entry_size(UWORD file_id)
{
    if (file_id == TFCH_ID || file_id == OFCH_ID)
        return sizeof(struct TFontContents);

    return sizeof(struct FontContents);
}

static BOOL _df_has_path(const char *name)
{
    if (!name)
        return FALSE;

    while (*name)
    {
        if (*name == ':' || *name == '/')
            return TRUE;
        name++;
    }

    return FALSE;
}

static BOOL _df_parent_dir(const char *path, char *buf, int bufsize)
{
    int len;
    int split = -1;
    int i;

    if (!path || !buf || bufsize <= 1)
        return FALSE;

    len = strlen(path);
    for (i = 0; i < len; i++)
    {
        if (path[i] == ':' || path[i] == '/')
            split = i;
    }

    if (split < 0)
        return FALSE;

    if (split + 2 > bufsize)
        return FALSE;

    for (i = 0; i <= split; i++)
        buf[i] = path[i];
    buf[split + 1] = '\0';

    return TRUE;
}

struct df_avail_fonts_state
{
    ULONG  request_flags;
    UBYTE *buffer;
    ULONG  buf_bytes;
    ULONG  entry_size;
    UWORD  num_entries;         /* entries stored */
    BOOL   filling;             /* still storing entries */
    ULONG  used;                /* bytes used by the stored entries */
    ULONG  missing;             /* bytes of the entries that did not fit */
    UBYTE *name_end;            /* names are stored from the buffer end down */
    STRPTR last_name;           /* name of the last stored entry */
};

static VOID _df_new_min_list(struct MinList *list)
{
    list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
    list->mlh_Tail = NULL;
    list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
}

static VOID _df_flush_cache(struct DiskfontBase *DiskfontBase)
{
    struct CachedFontEntry *entry;
    struct CachedFontEntry *next;

    for (entry = (struct CachedFontEntry *)g_diskfont_cache_state.font_entries.mlh_Head;
         entry->node.mln_Succ;
         entry = next)
    {
        next = (struct CachedFontEntry *)entry->node.mln_Succ;
        Remove((struct Node *)entry);
        FreeMem(entry, sizeof(*entry));
    }

    if (DiskfontBase)
    {
        memset(&DiskfontBase->cached_attr, 0, sizeof(DiskfontBase->cached_attr));
        DiskfontBase->cached_font = NULL;
        DiskfontBase->cached_scaled_font = NULL;
    }
}

/*
 * AvailFonts() buffer filling (AmigaOS 3.1, verified on the reference
 * machine): the entries are stored in order while they fit; an entry
 * needs its AvailFonts/TAvailFonts structure plus its name, unless the
 * name equals the name of the previously stored entry (the string is
 * shared).  Once an entry does not fit, nothing more is stored and the
 * remaining entries are counted with their names, sharing only with the
 * last stored name.  The result is the number of bytes missing (0 when
 * everything fit); afh_NumEntries holds the number of stored entries
 * whenever the buffer has room for it.
 */

static VOID _df_avail_emit_entry(struct df_avail_fonts_state *state,
                                 UWORD                        type,
                                 CONST_STRPTR                 name,
                                 UWORD                        ysize,
                                 UBYTE                        style,
                                 UBYTE                        font_flags,
                                 BOOL                         set_tags)
{
    ULONG name_len;
    ULONG cost;
    BOOL share;

    if (!state || !name)
        return;

    name_len = strlen((const char *)name) + 1;
    share = state->last_name && strcmp((const char *)state->last_name, (const char *)name) == 0;
    cost = state->entry_size + (share ? 0 : name_len);

    if (state->filling && state->used + cost <= state->buf_bytes)
    {
        UBYTE *entry = state->buffer + sizeof(struct AvailFontsHeader) + (ULONG)state->num_entries * state->entry_size;

        if (!share)
        {
            state->name_end -= name_len;
            CopyMem((APTR)name, state->name_end, name_len);
            state->last_name = (STRPTR)state->name_end;
        }

        if (state->request_flags & AFF_TAGGED)
        {
            struct TAvailFonts *taf = (struct TAvailFonts *)entry;

            taf->taf_Type = type;
            taf->taf_Attr.tta_Name = state->last_name;
            taf->taf_Attr.tta_YSize = ysize;
            taf->taf_Attr.tta_Style = style;
            taf->taf_Attr.tta_Flags = font_flags;
            if (set_tags)
                taf->taf_Attr.tta_Tags = NULL;
        }
        else
        {
            struct AvailFonts *af = (struct AvailFonts *)entry;

            af->af_Type = type;
            af->af_Attr.ta_Name = state->last_name;
            af->af_Attr.ta_YSize = ysize;
            af->af_Attr.ta_Style = style;
            af->af_Attr.ta_Flags = font_flags;
        }

        state->num_entries++;
        state->used += cost;
    }
    else
    {
        state->filling = FALSE;
        state->missing += cost;
    }
}

static VOID _df_avail_collect_memory_fonts(struct df_avail_fonts_state *state)
{
    struct Node *node;
    BOOL tagged = (state->request_flags & AFF_TAGGED) != 0;

    Forbid();
    for (node = GETHEAD(&GfxBase->TextFonts); node; node = GETSUCC(node))
    {
        struct TextFont *font = (struct TextFont *)node;

        if (!font->tf_Message.mn_Node.ln_Name)
            continue;

        if (!(state->request_flags & AFF_SCALED) && !(font->tf_Flags & FPF_DESIGNED))
            continue;

        /* tagged entries of memory fonts carry FSF_TAGGED and no tags */
        _df_avail_emit_entry(state,
                             (font->tf_Flags & FPF_DESIGNED) ? AFF_MEMORY : AFF_SCALED,
                             (CONST_STRPTR)font->tf_Message.mn_Node.ln_Name,
                             font->tf_YSize,
                             tagged ? (UBYTE)(font->tf_Style | FSF_TAGGED) : font->tf_Style,
                             font->tf_Flags,
                             TRUE);
    }
    Permit();
}

static VOID _df_avail_collect_disk_fonts(struct df_avail_fonts_state *state)
{
    BPTR fontsLock;
    BOOL tagged = (state->request_flags & AFF_TAGGED) != 0;

    fontsLock = Lock((STRPTR)"FONTS:", ACCESS_READ);
    if (fontsLock)
    {
        struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_PUBLIC | MEMF_CLEAR);
        struct TFontContents *tfc = (struct TFontContents *)AllocVec(sizeof(struct TFontContents), MEMF_PUBLIC);

        if (fib && tfc && Examine(fontsLock, fib))
        {
            while (ExNext(fontsLock, fib))
            {
                char fontPath[300];
                struct FontContentsHeader fch;
                BPTR fh;
                UWORD j;
                UWORD disk_type = AFF_DISK;

                if (fib->fib_DirEntryType >= 0 || !_df_endswith((const char *)fib->fib_FileName, ".font"))
                    continue;

                strcpy(fontPath, "FONTS:");
                strcat(fontPath, (const char *)fib->fib_FileName);

                fh = Open((STRPTR)fontPath, MODE_OLDFILE);
                if (!fh)
                    continue;

                if (Read(fh, &fch, sizeof(fch)) != sizeof(fch) ||
                    (fch.fch_FileID != FCH_ID && fch.fch_FileID != TFCH_ID && fch.fch_FileID != OFCH_ID) ||
                    ((state->request_flags & AFF_OTAG) && fch.fch_FileID != OFCH_ID) ||
                    ((state->request_flags & AFF_BITMAP) && fch.fch_FileID == OFCH_ID))
                {
                    Close(fh);
                    continue;
                }

                if (state->request_flags & AFF_TYPE)
                    disk_type = (fch.fch_FileID == OFCH_ID) ? (AFF_DISK | AFF_OTAG) : (AFF_DISK | AFF_BITMAP);

                for (j = 0; j < fch.fch_NumEntries; j++)
                {
                    UBYTE style;

                    if (Read(fh, tfc, sizeof(*tfc)) != sizeof(*tfc))
                        break;

                    /* AmigaOS 3.1: entries of a TFCH_ID file are marked
                     * FSF_TAGGED with tta_Tags NULL when AFF_TAGGED is
                     * set and have FSF_TAGGED cleared otherwise; the
                     * tta_Tags of FCH_ID entries are left untouched. */
                    style = tfc->tfc_Style;
                    if (fch.fch_FileID != FCH_ID)
                        style = tagged ? (UBYTE)(style | FSF_TAGGED) : (UBYTE)(style & ~FSF_TAGGED);

                    _df_avail_emit_entry(state,
                                         disk_type,
                                         (CONST_STRPTR)fib->fib_FileName,
                                         tfc->tfc_YSize,
                                         style,
                                         (tfc->tfc_Flags & ~FPF_ROMFONT) | FPF_DISKFONT,
                                         fch.fch_FileID != FCH_ID);
                }

                Close(fh);
            }
        }

        if (tfc)
            FreeVec(tfc);
        if (fib)
            FreeVec(fib);
        UnLock(fontsLock);
    }
}

/****************************************************************************/
/* Bitmap font scaling (AmigaOS 3.1 semantics, observed on the reference)    */
/****************************************************************************/

/*
 * The scaler maps pixels like graphics.library BitMapScale(): destination
 * pixel i shows source pixel (i * src + off) / dst, off = dst / 2 when
 * shrinking and (src - 1) / 2 otherwise.  Rows scale by the font sizes,
 * columns (within each glyph) by the sizes times the TA_DeviceDPI aspect.
 * Glyph widths, tf_XSize, tf_BoldSmear and the spacing table take the
 * number of destination pixels a source span covers; kerning is chosen so
 * that space + kern scales the same way.
 */
static ULONG _df_scale_off(ULONG sf, ULONG df)
{
    return sf > df ? (df >> 1) : ((sf - 1) >> 1);
}

static ULONG _df_scale_count(LONG n, ULONG sf, ULONG df)
{
    if (n <= 0)
        return 0;
    return ((ULONG)n * df - 1 - _df_scale_off(sf, df)) / sf + 1;
}

static ULONG _df_scale_src(ULONG i, ULONG sf, ULONG df)
{
    return (i * sf + _df_scale_off(sf, df)) / df;
}

static LONG _df_scale_signed(LONG n, ULONG sf, ULONG df)
{
    return n < 0 ? -(LONG)_df_scale_count(-n, sf, df) : (LONG)_df_scale_count(n, sf, df);
}

static ULONG _df_gcd(ULONG a, ULONG b)
{
    while (b)
    {
        ULONG t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/* TA_DeviceDPI of a tag list, 0 when absent or without an aspect */
static ULONG _df_dpi_of(struct TagItem *tags)
{
    ULONG dpi;

    if (!tags)
        return 0;
    dpi = GetTagData(TA_DeviceDPI, 0, tags);
    if (!(dpi >> 16) || !(dpi & 0xffff) || (dpi >> 16) == (dpi & 0xffff))
        return 0;
    return dpi;
}

static BOOL _df_same_aspect(ULONG a, ULONG b)
{
    ULONG ax = a ? (a >> 16) : 1, ay = a ? (a & 0xffff) : 1;
    ULONG bx = b ? (b >> 16) : 1, by = b ? (b & 0xffff) : 1;

    return ax * by == bx * ay;
}

static struct TextFontExtension *_df_font_ext(struct TextFont *tf)
{
    struct TextFontExtension *tfe = (struct TextFontExtension *)tf->tf_Extension;

    if (tfe && tfe->tfe_MatchWord == 0x4e1b && tfe->tfe_BackPtr == tf)
        return tfe;
    return NULL;
}

static ULONG _df_font_dpi(struct TextFont *tf)
{
    struct TextFontExtension *tfe = _df_font_ext(tf);

    return tfe ? _df_dpi_of(tfe->tfe_Tags) : 0;
}

static BOOL _df_get_bit(CONST UBYTE *row, ULONG x)
{
    return (row[x >> 3] >> (7 - (x & 7))) & 1;
}

/*
 * Builds a scaled copy of srcFont in one block laid out like a loaded
 * segment (UnLoadSeg(dfh_Segment) frees it).  dpi is TA_DeviceDPI or 0.
 */
static struct DiskFontHeader *_df_scale_font(struct TextFont *src, UWORD T, ULONG dpi)
{
    UWORD S = src->tf_YSize;
    ULONG sfx = S, dfx = T;
    UWORD n, i, j, p, depth = 1;
    ULONG *dloc;
    ULONG total = 0, modulo, size, hdr, off_loc, off_space = 0, off_kern = 0, off_data, off_colors = 0;
    ULONG ncolors = 0;
    BOOL color = (src->tf_Style & FSF_COLORFONT) != 0;
    struct ColorTextFont *sctf = (struct ColorTextFont *)src;
    ULONG *block;
    UBYTE *base;
    struct DiskFontHeader *dfh;
    struct TextFont *tf;
    CONST ULONG *sloc = (CONST ULONG *)src->tf_CharLoc;
    UWORD y;

    if (!S || !T || !sloc || !src->tf_CharData)
        return NULL;

    if (dpi)
    {
        ULONG g;
        sfx = (ULONG)S * (dpi & 0xffff);
        dfx = (ULONG)T * (dpi >> 16);
        g = _df_gcd(sfx, dfx);
        sfx /= g;
        dfx /= g;
        /* keep the products below 32 bits */
        while (sfx > 0xffff || dfx > 0xffff)
        {
            sfx = (sfx + 1) >> 1;
            dfx = (dfx + 1) >> 1;
        }
    }

    n = (UWORD)(src->tf_HiChar - src->tf_LoChar + 2);
    dloc = (ULONG *)AllocVec((ULONG)n * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
    if (!dloc)
        return NULL;

    /* glyphs are packed in order; equal source glyphs share one copy */
    for (i = 0; i < n; i++)
    {
        for (j = 0; j < i; j++)
            if (sloc[j] == sloc[i])
                break;
        if (j < i)
        {
            dloc[i] = dloc[j];
            continue;
        }
        {
            ULONG w = _df_scale_count((LONG)(sloc[i] & 0xffff), sfx, dfx);
            dloc[i] = (total << 16) | w;
            total += w;
        }
    }
    modulo = ((total + 15) >> 4) << 1;

    if (color && sctf->ctf_Depth > 1)
        depth = sctf->ctf_Depth > 8 ? 8 : sctf->ctf_Depth;
    if (color && sctf->ctf_ColorFontColors)
        ncolors = sctf->ctf_ColorFontColors->cfc_Count;

    /* segment: size, next segment, ReturnCode, header, tables, data */
    hdr = 3 * sizeof(ULONG) + sizeof(struct DiskFontHeader) +
          (color ? sizeof(struct ColorTextFont) - sizeof(struct TextFont) : 0);
    hdr = (hdr + 3) & ~3UL;
    off_loc = hdr;
    size = off_loc + (ULONG)n * 4;
    if (src->tf_CharSpace)
    {
        off_space = size;
        size += (ULONG)n * 2;
    }
    if (src->tf_CharKern)
    {
        off_kern = size;
        size += (ULONG)n * 2;
    }
    size = (size + 3) & ~3UL;
    off_data = size;
    size += (ULONG)depth * modulo * T;
    if (color && sctf->ctf_ColorFontColors)
    {
        size = (size + 3) & ~3UL;
        off_colors = size;
        size += sizeof(struct ColorFontColors) + ncolors * sizeof(UWORD);
    }

    block = (ULONG *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!block)
    {
        FreeVec(dloc);
        return NULL;
    }
    base = (UBYTE *)block;
    block[0] = size;
    block[1] = 0;
    block[2] = 0x70004e75;          /* moveq #0,d0; rts */
    dfh = (struct DiskFontHeader *)&block[3];
    tf = &dfh->dfh_TF;

    dfh->dfh_DF.ln_Type = NT_FONT;
    dfh->dfh_DF.ln_Name = (char *)dfh->dfh_Name;
    dfh->dfh_FileID = DFH_ID;
    dfh->dfh_Segment = MKBADDR(&block[1]);
    if (src->tf_Message.mn_Node.ln_Name)
        _df_copy_name(dfh->dfh_Name, (CONST_STRPTR)src->tf_Message.mn_Node.ln_Name, MAXFONTNAME);

    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    tf->tf_Message.mn_Node.ln_Name = (char *)dfh->dfh_Name;
    tf->tf_Message.mn_Length = (UWORD)(size - 3 * sizeof(ULONG) -
                                       ((UBYTE *)tf - (UBYTE *)dfh));
    tf->tf_YSize = T;
    tf->tf_Style = src->tf_Style;
    tf->tf_Flags = src->tf_Flags & ~(FPF_DESIGNED | FPF_DISKFONT | FPF_ROMFONT | FPF_REMOVED);
    tf->tf_XSize = (UWORD)_df_scale_count(src->tf_XSize, sfx, dfx);
    tf->tf_Baseline = (UWORD)(T < S ? _df_scale_count(src->tf_Baseline, S, T)
                                    : _df_scale_count(src->tf_Baseline + 1, S, T) - 1);
    tf->tf_BoldSmear = (UWORD)_df_scale_count(src->tf_BoldSmear, sfx, dfx);
    tf->tf_LoChar = src->tf_LoChar;
    tf->tf_HiChar = src->tf_HiChar;
    tf->tf_Modulo = (UWORD)modulo;
    tf->tf_CharLoc = base + off_loc;
    CopyMem(dloc, tf->tf_CharLoc, (ULONG)n * 4);
    tf->tf_CharData = base + off_data;

    if (src->tf_CharSpace)
    {
        WORD *ss = (WORD *)src->tf_CharSpace, *ds = (WORD *)(base + off_space);
        tf->tf_CharSpace = ds;
        for (i = 0; i < n; i++)
            ds[i] = (WORD)_df_scale_signed(ss[i], sfx, dfx);
    }
    if (src->tf_CharKern)
    {
        WORD *sk = (WORD *)src->tf_CharKern, *dk = (WORD *)(base + off_kern);
        tf->tf_CharKern = dk;
        for (i = 0; i < n; i++)
        {
            if (src->tf_CharSpace)
                dk[i] = (WORD)(_df_scale_signed(((WORD *)src->tf_CharSpace)[i] + sk[i], sfx, dfx) -
                               ((WORD *)tf->tf_CharSpace)[i]);
            else
                dk[i] = (WORD)_df_scale_signed(sk[i], sfx, dfx);
        }
    }

    for (p = 0; p < depth; p++)
    {
        CONST UBYTE *splane = (CONST UBYTE *)(color && depth > 1 ? sctf->ctf_CharData[p] : src->tf_CharData);
        UBYTE *dplane = base + off_data + (ULONG)p * modulo * T;

        if (!splane)
            continue;

        for (y = 0; y < T; y++)
        {
            CONST UBYTE *srow = splane + _df_scale_src(y, S, T) * src->tf_Modulo;
            UBYTE *drow = dplane + (ULONG)y * modulo;

            for (i = 0; i < n; i++)
            {
                ULONG so = sloc[i] >> 16, dof = dloc[i] >> 16, dw = dloc[i] & 0xffff;
                ULONG x;

                for (j = 0; j < i; j++)
                    if (sloc[j] == sloc[i])
                        break;
                if (j < i)
                    continue;

                for (x = 0; x < dw; x++)
                    if (_df_get_bit(srow, so + _df_scale_src(x, sfx, dfx)))
                        drow[(dof + x) >> 3] |= (UBYTE)(0x80 >> ((dof + x) & 7));
            }
        }
    }

    if (color)
    {
        struct ColorTextFont *ctf = (struct ColorTextFont *)tf;

        ctf->ctf_Flags = sctf->ctf_Flags;
        ctf->ctf_Depth = sctf->ctf_Depth;
        ctf->ctf_FgColor = sctf->ctf_FgColor;
        ctf->ctf_Low = sctf->ctf_Low;
        ctf->ctf_High = sctf->ctf_High;
        ctf->ctf_PlanePick = sctf->ctf_PlanePick;
        ctf->ctf_PlaneOnOff = sctf->ctf_PlaneOnOff;
        for (p = 0; p < depth; p++)
            ctf->ctf_CharData[p] = (depth > 1 && !sctf->ctf_CharData[p]) ? NULL :
                                   base + off_data + (ULONG)p * modulo * T;
        if (sctf->ctf_ColorFontColors)
        {
            struct ColorFontColors *cfc = (struct ColorFontColors *)(base + off_colors);
            cfc->cfc_Reserved = sctf->ctf_ColorFontColors->cfc_Reserved;
            cfc->cfc_Count = (UWORD)ncolors;
            cfc->cfc_ColorTable = (UWORD *)(cfc + 1);
            if (ncolors && sctf->ctf_ColorFontColors->cfc_ColorTable)
                CopyMem(sctf->ctf_ColorFontColors->cfc_ColorTable, cfc->cfc_ColorTable, ncolors * sizeof(UWORD));
            ctf->ctf_ColorFontColors = cfc;
        }
    }

    FreeVec(dloc);
    ExtendFont(tf, NULL);
    return dfh;
}

/****************************************************************************/
/* OpenDiskFont                                                              */
/****************************************************************************/

struct df_contents
{
    UWORD  num;
    UWORD *ysize;               /* per entry */
    TEXT (*file)[MAXFONTPATH];  /* per entry */
};

static VOID _df_free_contents(struct df_contents *c)
{
    if (c->ysize)
        FreeVec(c->ysize);
    if (c->file)
        FreeVec(c->file);
    c->ysize = NULL;
    c->file = NULL;
    c->num = 0;
}

/* reads the contents file; FALSE when it cannot be opened or is invalid */
static BOOL _df_read_contents(CONST_STRPTR path, struct df_contents *c)
{
    struct FontContentsHeader fch;
    struct TFontContents *tfc;
    BPTR fh;
    UWORD i;

    c->num = 0;
    c->ysize = NULL;
    c->file = NULL;

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return FALSE;

    if (Read(fh, &fch, sizeof(fch)) != sizeof(fch) ||
        (fch.fch_FileID != FCH_ID && fch.fch_FileID != TFCH_ID))
    {
        Close(fh);
        return FALSE;
    }

    tfc = (struct TFontContents *)AllocVec(sizeof(*tfc), MEMF_PUBLIC);
    if (fch.fch_NumEntries)
    {
        c->ysize = (UWORD *)AllocVec((ULONG)fch.fch_NumEntries * sizeof(UWORD), MEMF_PUBLIC | MEMF_CLEAR);
        c->file = (TEXT (*)[MAXFONTPATH])AllocVec((ULONG)fch.fch_NumEntries * MAXFONTPATH, MEMF_PUBLIC | MEMF_CLEAR);
    }

    if (tfc && (!fch.fch_NumEntries || (c->ysize && c->file)))
    {
        for (i = 0; i < fch.fch_NumEntries; i++)
        {
            if (Read(fh, tfc, sizeof(*tfc)) != sizeof(*tfc))
                break;
            c->ysize[c->num] = tfc->tfc_YSize;
            _df_copy_name(c->file[c->num], (CONST_STRPTR)tfc->tfc_FileName,
                          fch.fch_FileID == TFCH_ID ? MAXFONTPATH - 2 : MAXFONTPATH);
            c->num++;
        }
    }

    if (tfc)
        FreeVec(tfc);
    Close(fh);
    return TRUE;
}

/*
 * The source for a size that is not designed (AmigaOS 3.1): a size of
 * which the request is twice, or twice the request; else the largest
 * smaller size; else the smallest larger one.  Returns the entry index.
 */
static WORD _df_pick_source(struct df_contents *c, UWORD T)
{
    WORD i, best = -1;

    for (i = 0; i < (WORD)c->num; i++)
        if (c->ysize[i] && (ULONG)c->ysize[i] * 2 == T)
            return i;
    for (i = 0; i < (WORD)c->num; i++)
        if ((ULONG)T * 2 == c->ysize[i])
            return i;
    for (i = 0; i < (WORD)c->num; i++)
        if (c->ysize[i] < T && (best < 0 || c->ysize[i] > c->ysize[best]))
            best = i;
    if (best >= 0)
        return best;
    for (i = 0; i < (WORD)c->num; i++)
        if (c->ysize[i] > T && (best < 0 || c->ysize[i] < c->ysize[best]))
            best = i;
    return best;
}

static CONST_STRPTR _df_basename(CONST_STRPTR name)
{
    CONST_STRPTR base = name;

    while (*name)
    {
        if (*name == '/' || *name == ':')
            base = name + 1;
        name++;
    }
    return base;
}

/* a memory font of that name and size (designed ones first) */
static struct TextFont *_df_find_memory_font(CONST_STRPTR name, UWORD ysize, ULONG dpi, BOOL designed_only)
{
    struct Node *node;
    struct TextFont *found = NULL;

    Forbid();
    for (node = GETHEAD(&GfxBase->TextFonts); node; node = GETSUCC(node))
    {
        struct TextFont *font = (struct TextFont *)node;

        if (!font->tf_Message.mn_Node.ln_Name ||
            strcmp(font->tf_Message.mn_Node.ln_Name, (const char *)name) != 0 ||
            font->tf_YSize != ysize ||
            !_df_same_aspect(_df_font_dpi(font), dpi) ||
            (designed_only && !(font->tf_Flags & FPF_DESIGNED)))
            continue;

        if (!found || ((font->tf_Flags & FPF_DESIGNED) && !(found->tf_Flags & FPF_DESIGNED)))
            found = font;
    }
    Permit();
    return found;
}

/* rank of a memory font for the no-contents fallback: designed, then
 * without a TA_DeviceDPI aspect */
static WORD _df_font_rank(struct TextFont *font)
{
    return (WORD)(((font->tf_Flags & FPF_DESIGNED) ? 2 : 0) + (_df_font_dpi(font) ? 0 : 1));
}

/*
 * Without a contents file (AmigaOS 3.1): the memory font of that name
 * with the largest size not above the request, else the smallest larger
 * one.
 */
static struct TextFont *_df_closest_memory_font(CONST_STRPTR name, UWORD ysize)
{
    struct Node *node;
    struct TextFont *below = NULL, *above = NULL;

    Forbid();
    for (node = GETHEAD(&GfxBase->TextFonts); node; node = GETSUCC(node))
    {
        struct TextFont *font = (struct TextFont *)node;

        if (!font->tf_Message.mn_Node.ln_Name ||
            strcmp(font->tf_Message.mn_Node.ln_Name, (const char *)name) != 0)
            continue;

        if (font->tf_YSize <= ysize)
        {
            if (!below || font->tf_YSize > below->tf_YSize ||
                (font->tf_YSize == below->tf_YSize && _df_font_rank(font) > _df_font_rank(below)))
                below = font;
        }
        else if (!above || font->tf_YSize < above->tf_YSize ||
                 (font->tf_YSize == above->tf_YSize && _df_font_rank(font) > _df_font_rank(above)))
        {
            above = font;
        }
    }
    Permit();
    return below ? below : above;
}

/*
 * Loads one font file and adds it to the public font list (accessors 0).
 * The font is registered under the requested name; FSF_TAGGED and the
 * file's tags are dropped and it gets a TextFontExtension marked
 * TE0F_NOREMFONT (AmigaOS 3.1).
 */
static struct TextFont *_df_load_font_file(CONST_STRPTR dir, CONST_STRPTR file, CONST_STRPTR regname)
{
    char path[300];
    BPTR segList;
    ULONG *dataBase;
    struct DiskFontHeader *dfh;
    struct TextFont *tf;
    struct TextFontExtension *tfe;

    if (strlen((const char *)dir) >= sizeof(path))
        return NULL;
    strcpy(path, (const char *)dir);
    if (!AddPart((STRPTR)path, (STRPTR)file, sizeof(path)))
        return NULL;

    segList = LoadSeg((STRPTR)path);
    if (!segList)
        return NULL;

    /* segment: next-segment BPTR, ReturnCode, DiskFontHeader */
    dataBase = (ULONG *)BADDR(segList) + 1;
    dfh = (struct DiskFontHeader *)(dataBase + 1);
    if (dfh->dfh_FileID != DFH_ID)
    {
        UnLoadSeg(segList);
        return NULL;
    }

    tf = &dfh->dfh_TF;
    _df_copy_name(dfh->dfh_Name, regname, MAXFONTNAME);
    tf->tf_Message.mn_Node.ln_Name = (char *)dfh->dfh_Name;
    tf->tf_Message.mn_Node.ln_Type = NT_FONT;
    dfh->dfh_DF.ln_Type = NT_FONT;
    dfh->dfh_Segment = segList;
    tf->tf_Flags = (tf->tf_Flags & ~FPF_ROMFONT) | FPF_DISKFONT;
    tf->tf_Style &= ~FSF_TAGGED;
    tf->tf_Accessors = 0;

    ExtendFont(tf, NULL);
    tfe = _df_font_ext(tf);
    if (tfe)
        tfe->tfe_Flags0 |= TE0F_NOREMFONT;

    AddFont(tf);
    return tf;
}

/* the designed font of a contents entry: from memory, else from disk */
static struct TextFont *_df_designed_font(CONST_STRPTR dir, CONST_STRPTR name, struct df_contents *c, WORD idx)
{
    struct TextFont *tf = _df_find_memory_font(name, c->ysize[idx], 0, TRUE);

    if (tf)
        return tf;
    return _df_load_font_file(dir, (CONST_STRPTR)c->file[idx], name);
}

struct TextFont * _diskfont_OpenDiskFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                           register struct TextAttr     *textAttr     __asm("a0"))
{
    struct TTextAttr *tta = (struct TTextAttr *)textAttr;
    CONST_STRPTR name;
    UWORD T;
    ULONG dpi = 0;
    BOOL designed_only;
    struct TextFont *font;
    struct df_contents c;
    char path[300];
    char dir[300];
    WORD idx, i;

    DPRINTF (LOG_DEBUG, "_diskfont: OpenDiskFont() called name='%s' size=%d\n",
             textAttr ? (char *)textAttr->ta_Name : "(null)",
             textAttr ? textAttr->ta_YSize : 0);

    if (!textAttr || !textAttr->ta_Name)
        return NULL;

    name = _df_basename((CONST_STRPTR)textAttr->ta_Name);
    T = textAttr->ta_YSize;
    designed_only = (textAttr->ta_Flags & FPF_DESIGNED) != 0;
    if (textAttr->ta_Style & FSF_TAGGED)
        dpi = _df_dpi_of(tta->tta_Tags);

    /* 1. a font in memory */
    font = _df_find_memory_font(name, T, dpi, designed_only);
    if (font)
    {
        font->tf_Accessors++;
        return font;
    }

    /* 2. the contents file */
    if (_df_has_path((const char *)textAttr->ta_Name))
    {
        if (strlen((const char *)textAttr->ta_Name) >= sizeof(path) ||
            !_df_parent_dir((const char *)textAttr->ta_Name, dir, sizeof(dir)))
            return NULL;
        strcpy(path, (const char *)textAttr->ta_Name);
    }
    else
    {
        if (strlen((const char *)name) + 7 > sizeof(path))
            return NULL;
        strcpy(path, "FONTS:");
        strcat(path, (const char *)name);
        strcpy(dir, "FONTS:");
    }

    if (!_df_read_contents((CONST_STRPTR)path, &c))
    {
        /* no contents file: the closest font in memory (OpenFont()) */
        font = _df_closest_memory_font(name, T);
        if (font)
            font->tf_Accessors++;
        return font;
    }

    font = NULL;
    idx = -1;
    for (i = 0; i < (WORD)c.num; i++)
        if (c.ysize[i] == T)
        {
            idx = i;
            break;
        }

    if (idx >= 0 && (!dpi || designed_only))
    {
        /* a designed size */
        font = _df_designed_font((CONST_STRPTR)dir, name, &c, idx);
        if (font)
            font->tf_Accessors++;
    }
    else
    {
        if (idx < 0)
            idx = _df_pick_source(&c, T);

        if (idx >= 0)
        {
            struct TextFont *src = _df_designed_font((CONST_STRPTR)dir, name, &c, idx);

            if (src && designed_only)
            {
                font = src;
                font->tf_Accessors++;
            }
            else if (src)
            {
                struct DiskFontHeader *dfh = _df_scale_font(src, T, dpi);

                if (dfh)
                {
                    struct TextFontExtension *tfe;

                    font = &dfh->dfh_TF;
                    tfe = _df_font_ext(font);
                    if (tfe)
                    {
                        tfe->tfe_Flags0 |= TE0F_NOREMFONT;
                        if (dpi)
                        {
                            tfe->tfe_Flags0 |= 0x80;
                            tfe->tfe_Tags = CloneTagItems(tta->tta_Tags);
                        }
                    }
                    AddFont(font);
                    font->tf_Accessors = 1;
                }
            }
        }
    }

    _df_free_contents(&c);
    return font;
}

LONG _diskfont_AvailFonts ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct AvailFontsHeader *buffer      __asm("a0"),
                            register LONG                    bufBytes     __asm("d0"),
                            register ULONG                   flags        __asm("d1"))
{
    struct df_avail_fonts_state state;

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() called buffer=0x%08lx bufBytes=%ld flags=0x%08lx\n",
             (ULONG)buffer, bufBytes, flags);

    memset(&state, 0, sizeof(state));
    state.request_flags = flags;
    state.buffer = (UBYTE *)buffer;
    state.buf_bytes = (buffer && bufBytes > 0) ? (ULONG)bufBytes : 0;
    state.entry_size = (flags & AFF_TAGGED) ? sizeof(struct TAvailFonts) : sizeof(struct AvailFonts);
    state.used = sizeof(struct AvailFontsHeader);
    state.filling = state.buf_bytes >= sizeof(struct AvailFontsHeader);
    state.name_end = state.buffer + state.buf_bytes;

    if (flags & AFF_MEMORY)
        _df_avail_collect_memory_fonts(&state);

    if (flags & AFF_DISK)
        _df_avail_collect_disk_fonts(&state);

    if (state.buf_bytes >= sizeof(struct AvailFontsHeader))
        buffer->afh_NumEntries = state.num_entries;

    if (state.filling)
        return 0;

    return (LONG)(state.used + state.missing) - (LONG)state.buf_bytes;
}

/****************************************************************************/
/* V34+ functions                                                            */
/****************************************************************************/

struct FontContentsHeader * _diskfont_NewFontContents ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                        register BPTR                 fontsLock    __asm("a0"),
                                                        register CONST_STRPTR         fontName     __asm("a1"))
{
    /*
     * AmigaOS 3.1 (verified on the reference machine) does not read the
     * existing .font file: it scans the font's directory
     * (<fontsLock>/<name without ".font">) and loads every font file found
     * there.  Each entry gets "<dir>/<file>", the file's tf_YSize/tf_Style
     * and tf_Flags with FPF_DISKFONT set.  The header is always FCH_ID.
     * The name must end in ".font" (case matters); a failure leaves
     * IoErr() 0.
     */
    struct FontContentsHeader *result = NULL;
    struct FontContents *entries = NULL;
    ULONG max_entries = 0;
    ULONG num_entries = 0;
    ULONG name_len;
    struct FileInfoBlock *fib = NULL;
    BPTR old_dir;
    BPTR dir_lock;
    char base[MAXFONTPATH];

    DPRINTF (LOG_DEBUG, "_diskfont: NewFontContents() fontsLock=0x%08lx fontName='%s'\n",
             (ULONG)fontsLock, STRORNULL(fontName));

    name_len = fontName ? strlen((const char *)fontName) : 0;
    if (name_len < 5 || name_len >= MAXFONTPATH || strcmp((const char *)fontName + name_len - 5, ".font") != 0)
    {
        SetIoErr(0);
        return NULL;
    }

    _df_strip_font_suffix((const char *)fontName, base, sizeof(base));

    old_dir = CurrentDir(fontsLock);
    dir_lock = Lock((STRPTR)base, SHARED_LOCK);
    if (!dir_lock)
    {
        CurrentDir(old_dir);
        SetIoErr(0);
        return NULL;
    }

    fib = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_PUBLIC | MEMF_CLEAR);
    if (!fib || !Examine(dir_lock, fib) || fib->fib_DirEntryType < 0)
    {
        SetIoErr(0);
        goto done;
    }

    while (ExNext(dir_lock, fib))
    {
        char path[MAXFONTPATH];
        BPTR seg;
        struct DiskFontHeader *dfh;

        if (fib->fib_DirEntryType >= 0)
            continue;
        if (strlen(base) + 1 + strlen((const char *)fib->fib_FileName) >= MAXFONTPATH)
            continue;

        strcpy(path, base);
        strcat(path, "/");
        strcat(path, (const char *)fib->fib_FileName);

        seg = LoadSeg((STRPTR)path);
        if (!seg)
            continue;

        /* segment: next-segment BPTR, ReturnCode, DiskFontHeader */
        dfh = (struct DiskFontHeader *)((ULONG *)BADDR(seg) + 2);
        if (dfh->dfh_FileID == DFH_ID)
        {
            if (num_entries == max_entries)
            {
                ULONG new_max = max_entries ? max_entries * 2 : 8;
                struct FontContents *grown = (struct FontContents *)AllocVec(new_max * sizeof(struct FontContents),
                                                                           MEMF_PUBLIC | MEMF_CLEAR);
                if (!grown)
                {
                    UnLoadSeg(seg);
                    break;
                }
                if (entries)
                {
                    CopyMem(entries, grown, num_entries * sizeof(struct FontContents));
                    FreeVec(entries);
                }
                entries = grown;
                max_entries = new_max;
            }

            memset(&entries[num_entries], 0, sizeof(struct FontContents));
            strcpy((char *)entries[num_entries].fc_FileName, path);
            entries[num_entries].fc_YSize = dfh->dfh_TF.tf_YSize;
            entries[num_entries].fc_Style = dfh->dfh_TF.tf_Style;
            entries[num_entries].fc_Flags = (dfh->dfh_TF.tf_Flags & ~FPF_ROMFONT) | FPF_DISKFONT;
            num_entries++;
        }

        UnLoadSeg(seg);
    }

    result = (struct FontContentsHeader *)AllocMem(sizeof(struct FontContentsHeader) +
                                                   num_entries * sizeof(struct FontContents), MEMF_PUBLIC | MEMF_CLEAR);
    if (result)
    {
        result->fch_FileID = FCH_ID;
        result->fch_NumEntries = (UWORD)num_entries;
        if (num_entries)
            CopyMem(entries, result + 1, num_entries * sizeof(struct FontContents));
    }
    else
    {
        SetIoErr(ERROR_NO_FREE_STORE);
    }

done:
    if (entries)
        FreeVec(entries);
    if (fib)
        FreeVec(fib);
    UnLock(dir_lock);
    CurrentDir(old_dir);
    return result;
}

VOID _diskfont_DisposeFontContents ( register struct DiskfontBase       *DiskfontBase        __asm("a6"),
                                     register struct FontContentsHeader *fontContentsHeader  __asm("a1"))
{
    ULONG total_size;

    DPRINTF (LOG_DEBUG, "_diskfont: DisposeFontContents() fontContentsHeader=0x%08lx\n",
             (ULONG)fontContentsHeader);

    if (!fontContentsHeader)
        return;

    total_size = sizeof(struct FontContentsHeader) +
                 ((ULONG)fontContentsHeader->fch_NumEntries * _df_font_contents_entry_size(fontContentsHeader->fch_FileID));

    FreeMem(fontContentsHeader, total_size);
}

/****************************************************************************/
/* V36+ functions                                                            */
/****************************************************************************/

struct DiskFont * _diskfont_NewScaledDiskFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                register struct TextFont     *sourceFont   __asm("a0"),
                                                register struct TextAttr     *destTextAttr __asm("a1"))
{
    struct TTextAttr *tta = (struct TTextAttr *)destTextAttr;
    struct DiskFontHeader *dfh;
    ULONG dpi = 0;

    DPRINTF (LOG_DEBUG, "_diskfont: NewScaledDiskFont() sourceFont=0x%08lx destTextAttr=0x%08lx\n",
             (ULONG)sourceFont, (ULONG)destTextAttr);

    if (!sourceFont || !destTextAttr)
        return NULL;

    if (destTextAttr->ta_Style & FSF_TAGGED)
        dpi = _df_dpi_of(tta->tta_Tags);

    /* the result is not in the font list and has no accessors; free it
     * with StripFont() and UnLoadSeg(dfh_Segment) */
    dfh = _df_scale_font(sourceFont, destTextAttr->ta_YSize, dpi);
    if (dfh && dpi)
    {
        struct TextFontExtension *tfe = _df_font_ext(&dfh->dfh_TF);
        if (tfe)
            tfe->tfe_Tags = CloneTagItems(tta->tta_Tags);
    }

    return (struct DiskFont *)dfh;
}

/****************************************************************************/
/* V45+ functions                                                            */
/****************************************************************************/

LONG _diskfont_GetDiskFontCtrl ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                 register LONG                 tagid        __asm("d0"))
{
    /* tagid is a 32-bit tag constant (e.g. 0x8B000001) — do NOT sign-extend */

    DPRINTF (LOG_DEBUG, "_diskfont: GetDiskFontCtrl(tag=0x%08lx)\n", (ULONG)tagid);

    if (!DiskfontBase)
        return 0;

    switch (tagid)
    {
        case DFCTRL_XDPI:
            return DiskfontBase->xdpi;
        case DFCTRL_YDPI:
            return DiskfontBase->ydpi;
        case DFCTRL_XDOTP:
            return DiskfontBase->xdotp;
        case DFCTRL_YDOTP:
            return DiskfontBase->ydotp;
        case DFCTRL_CACHE:
            return DiskfontBase->cache_enabled;
        case DFCTRL_SORTMODE:
            return DiskfontBase->sort_mode;
        case DFCTRL_CHARSET:
            return (LONG)DiskfontBase->charset;
        default:
            return 0;
    }
}

VOID _diskfont_SetDiskFontCtrlA ( register struct DiskfontBase     *DiskfontBase __asm("a6"),
                                  register CONST struct TagItem    *taglist      __asm("a0"))
{
    CONST struct TagItem *tag;

    DPRINTF (LOG_DEBUG, "_diskfont: SetDiskFontCtrlA(taglist=0x%08lx)\n", (ULONG)taglist);

    if (!DiskfontBase || !taglist)
        return;

    for (tag = taglist; tag && tag->ti_Tag != TAG_DONE; tag++)
    {
        switch (tag->ti_Tag)
        {
            case TAG_IGNORE:
                break;

            case TAG_MORE:
                tag = (CONST struct TagItem *)tag->ti_Data - 1;
                break;

            case TAG_SKIP:
                tag += (LONG)tag->ti_Data;
                break;

            case DFCTRL_XDPI:
                DiskfontBase->xdpi = (LONG)tag->ti_Data;
                break;

            case DFCTRL_YDPI:
                DiskfontBase->ydpi = (LONG)tag->ti_Data;
                break;

            case DFCTRL_XDOTP:
                DiskfontBase->xdotp = (LONG)tag->ti_Data;
                break;

            case DFCTRL_YDOTP:
                DiskfontBase->ydotp = (LONG)tag->ti_Data;
                break;

            case DFCTRL_CACHE:
                DiskfontBase->cache_enabled = tag->ti_Data ? TRUE : FALSE;
                break;

            case DFCTRL_SORTMODE:
                DiskfontBase->sort_mode = (LONG)tag->ti_Data;
                break;

            case DFCTRL_CHARSET:
                DiskfontBase->charset = tag->ti_Data;
                break;

            case DFCTRL_CACHEFLUSH:
                if (tag->ti_Data)
                    _df_flush_cache(DiskfontBase);
                break;

            default:
                break;
        }
    }
}

/****************************************************************************/
/* V47+ functions (outline font support)                                     */
/****************************************************************************/

LONG _diskfont_EOpenEngine ( register struct DiskfontBase  *DiskfontBase __asm("a6"),
                             register struct EGlyphEngine  *eEngine      __asm("a0"))
{
    struct DiskfontGlyphEngineState *state;
    struct GlyphEngine *glyph_engine;

    (void)DiskfontBase;

    if (!eEngine || !eEngine->ege_BulletBase)
    {
        if (eEngine)
            eEngine->ege_GlyphEngine = NULL;
        return FALSE;
    }

    if (eEngine->ege_GlyphEngine)
        return TRUE;

    glyph_engine = (struct GlyphEngine *)AllocMem(sizeof(*glyph_engine), MEMF_PUBLIC | MEMF_CLEAR);
    if (!glyph_engine)
    {
        eEngine->ege_GlyphEngine = NULL;
        return FALSE;
    }

    glyph_engine->gle_Library = eEngine->ege_BulletBase;
    glyph_engine->gle_Name = (STRPTR)((struct Library *)eEngine->ege_BulletBase)->lib_Node.ln_Name;

    state = (struct DiskfontGlyphEngineState *)AllocMem(sizeof(*state), MEMF_PUBLIC | MEMF_CLEAR);
    if (!state)
    {
        FreeMem(glyph_engine, sizeof(*glyph_engine));
        eEngine->ege_GlyphEngine = NULL;
        return FALSE;
    }

    state->glyph_engine = *glyph_engine;
    state->glyph_name = _df_strdup(glyph_engine->gle_Name);
    _df_glyph_state_defaults(state);

    eEngine->ege_Reserved = state;
    eEngine->ege_GlyphEngine = glyph_engine;
    return TRUE;
}

VOID _diskfont_ECloseEngine ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                              register struct EGlyphEngine *eEngine      __asm("a0"))
{
    struct DiskfontGlyphEngineState *state;

    (void)DiskfontBase;

    if (!eEngine || !eEngine->ege_GlyphEngine)
        return;

    state = _df_glyph_state(eEngine);
    FreeMem(eEngine->ege_GlyphEngine, sizeof(struct GlyphEngine));
    _df_glyph_state_free(state);
    eEngine->ege_Reserved = NULL;
    eEngine->ege_GlyphEngine = NULL;
}

ULONG _diskfont_ESetInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                            register struct EGlyphEngine    *eEngine      __asm("a0"),
                            register CONST struct TagItem   *taglist      __asm("a1"))
{
    struct DiskfontGlyphEngineState *state = _df_glyph_state(eEngine);
    struct TagItem *iter;
    struct TagItem *tag;
    ULONG result;

    (void)DiskfontBase;

    if (!eEngine || !eEngine->ege_BulletBase || !eEngine->ege_GlyphEngine || !taglist || !state)
        return OTERR_BadData;

    iter = (struct TagItem *)taglist;
    while ((tag = NextTagItem(&iter)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case OT_DeviceDPI:   state->device_dpi = tag->ti_Data; break;
            case OT_DotSize:     state->dot_size = tag->ti_Data; break;
            case OT_PointHeight: state->point_height = tag->ti_Data; break;
            case OT_SetFactor:   state->set_factor = tag->ti_Data; break;
            case OT_ShearSin:    state->shear_sin = tag->ti_Data; break;
            case OT_ShearCos:    state->shear_cos = tag->ti_Data; break;
            case OT_RotateSin:   state->rotate_sin = tag->ti_Data; break;
            case OT_RotateCos:   state->rotate_cos = tag->ti_Data; break;
            case OT_EmboldenX:   state->embolden_x = tag->ti_Data; break;
            case OT_EmboldenY:   state->embolden_y = tag->ti_Data; break;
            case OT_PointSize:   state->point_size = tag->ti_Data; break;
            case OT_GlyphCode:   state->glyph_code = tag->ti_Data; break;
            case OT_GlyphCode2:  state->glyph_code2 = tag->ti_Data; break;
            case OT_GlyphCode_32: state->glyph_code32 = tag->ti_Data; break;
            case OT_GlyphCode2_32: state->glyph_code2_32 = tag->ti_Data; break;
            case OT_GlyphWidth:  state->glyph_width = tag->ti_Data; break;
            case OT_UnderLined:  state->underlined = tag->ti_Data; break;

            case OT_OTagPath:
                if (!tag->ti_Data)
                    return OTERR_BadData;
                break;

            case OT_OTagList:
                if (!tag->ti_Data)
                    return OTERR_BadData;
                break;

            default:
                return OTERR_UnknownTag;
        }
    }

    result = OTERR_Success;
    if (result != OTERR_Success)
        return result;

    iter = (struct TagItem *)taglist;
    while ((tag = NextTagItem(&iter)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case OT_OTagPath:
                if (state->current_otag_path && state->owns_current_otag_path)
                    FreeMem(state->current_otag_path, strlen((const char *)state->current_otag_path) + 1);
                state->current_otag_path = (STRPTR)tag->ti_Data;
                state->owns_current_otag_path = FALSE;
                break;

            case OT_OTagList:
                if (state->current_otag_list && state->owns_current_otag_list)
                    FreeMem(state->current_otag_list, state->current_otag_list[0].ti_Data);
                state->current_otag_list = (struct TagItem *)tag->ti_Data;
                state->owns_current_otag_list = FALSE;
                break;

            default:
                break;
        }
    }

    return OTERR_Success;
}

ULONG _diskfont_EObtainInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                               register struct EGlyphEngine    *eEngine      __asm("a0"),
                               register CONST struct TagItem   *taglist      __asm("a1"))
{
    struct DiskfontGlyphEngineState *state = _df_glyph_state(eEngine);
    struct TagItem *iter;
    struct TagItem *tag;

    (void)DiskfontBase;

    if (!eEngine || !eEngine->ege_BulletBase || !eEngine->ege_GlyphEngine || !taglist || !state)
        return OTERR_BadData;

    iter = (struct TagItem *)taglist;
    while ((tag = NextTagItem(&iter)) != NULL)
    {
        ULONG *dest = (ULONG *)tag->ti_Data;

        if (!dest)
            return OTERR_BadData;

        switch (tag->ti_Tag)
        {
            case OT_DeviceDPI:   *dest = state->device_dpi; break;
            case OT_DotSize:     *dest = state->dot_size; break;
            case OT_PointHeight: *dest = state->point_height; break;
            case OT_SetFactor:   *dest = state->set_factor; break;
            case OT_ShearSin:    *dest = state->shear_sin; break;
            case OT_ShearCos:    *dest = state->shear_cos; break;
            case OT_RotateSin:   *dest = state->rotate_sin; break;
            case OT_RotateCos:   *dest = state->rotate_cos; break;
            case OT_EmboldenX:   *dest = state->embolden_x; break;
            case OT_EmboldenY:   *dest = state->embolden_y; break;
            case OT_PointSize:   *dest = state->point_size; break;
            case OT_GlyphCode:   *dest = state->glyph_code; break;
            case OT_GlyphCode2:  *dest = state->glyph_code2; break;
            case OT_GlyphCode_32: *dest = state->glyph_code32; break;
            case OT_GlyphCode2_32: *dest = state->glyph_code2_32; break;
            case OT_GlyphWidth:  *dest = state->glyph_width; break;
            case OT_UnderLined:
                *dest = state->underlined;
                break;

            case OT_OTagPath:
                if (!state->current_otag_path)
                    return OTERR_NoFace;
                *dest = (ULONG)state->current_otag_path;
                break;

            case OT_OTagList:
                if (!state->current_otag_list)
                    return OTERR_NoFace;
                *dest = (ULONG)state->current_otag_list;
                break;

            default:
                return OTERR_UnknownTag;
        }
    }

    return OTERR_Success;
}

ULONG _diskfont_EReleaseInfoA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                                register struct EGlyphEngine    *eEngine      __asm("a0"),
                                register CONST struct TagItem   *taglist      __asm("a1"))
{
    struct DiskfontGlyphEngineState *state = _df_glyph_state(eEngine);
    struct TagItem *iter;
    struct TagItem *tag;

    (void)DiskfontBase;

    if (!eEngine || !eEngine->ege_BulletBase || !eEngine->ege_GlyphEngine || !taglist || !state)
        return OTERR_BadData;

    iter = (struct TagItem *)taglist;
    while ((tag = NextTagItem(&iter)) != NULL)
    {
        switch (tag->ti_Tag)
        {
            case OT_OTagPath:
            case OT_OTagList:
            case OT_DeviceDPI:
            case OT_DotSize:
            case OT_PointHeight:
            case OT_SetFactor:
            case OT_ShearSin:
            case OT_ShearCos:
            case OT_RotateSin:
            case OT_RotateCos:
            case OT_EmboldenX:
            case OT_EmboldenY:
            case OT_PointSize:
            case OT_GlyphCode:
            case OT_GlyphCode2:
            case OT_GlyphCode_32:
            case OT_GlyphCode2_32:
            case OT_GlyphWidth:
            case OT_UnderLined:
                break;

            default:
                return OTERR_UnknownTag;
        }
    }

    return OTERR_Success;
}

struct OutlineFont * _diskfont_OpenOutlineFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                                 register CONST_STRPTR         name         __asm("a0"),
                                                 register struct List          *list        __asm("a1"),
                                                 register ULONG                flags        __asm("d0"))
{
    struct OutlineFont *outline;
    UBYTE *otag_data = NULL;
    ULONG otag_size = 0;
    char path[512];
    struct TagItem *engine_tag;

    (void)DiskfontBase;
    (void)list;

    if (!name || !_df_otag_build_path(name, path, sizeof(path)))
        return NULL;

    if (!_df_read_file((CONST_STRPTR)path, &otag_data, &otag_size))
        return NULL;

    if (!_df_validate_otag_list((struct TagItem *)otag_data, otag_size))
    {
        FreeMem(otag_data, otag_size);
        return NULL;
    }

    outline = (struct OutlineFont *)AllocMem(sizeof(*outline), MEMF_PUBLIC | MEMF_CLEAR);
    if (!outline)
    {
        FreeMem(otag_data, otag_size);
        return NULL;
    }

    outline->olf_OTagPath = _df_strdup((CONST_STRPTR)path);
    outline->olf_OTagList = (struct TagItem *)otag_data;

    engine_tag = FindTagItem(OT_Engine, outline->olf_OTagList);
    outline->olf_EngineName = engine_tag ? _df_strdup((CONST_STRPTR)engine_tag->ti_Data) : NULL;

    if (outline->olf_EngineName)
    {
        ULONG lib_len = strlen((const char *)outline->olf_EngineName) + 9;
        outline->olf_LibraryName = (STRPTR)AllocMem(lib_len, MEMF_PUBLIC | MEMF_CLEAR);
        if (!outline->olf_LibraryName)
        {
            _diskfont_CloseOutlineFont(DiskfontBase, outline, list);
            return NULL;
        }

        strcpy((char *)outline->olf_LibraryName, (const char *)outline->olf_EngineName);
        strcat((char *)outline->olf_LibraryName, ".library");
    }

    if (outline->olf_LibraryName)
    {
        outline->olf_EEngine.ege_BulletBase = (struct Library *)OpenLibrary(outline->olf_LibraryName, 0);
        if (!outline->olf_EEngine.ege_BulletBase)
        {
            _diskfont_CloseOutlineFont(DiskfontBase, outline, list);
            return NULL;
        }

        if (!_diskfont_EOpenEngine(DiskfontBase, &outline->olf_EEngine))
        {
            _diskfont_CloseOutlineFont(DiskfontBase, outline, list);
            return NULL;
        }
    }

    if (flags & OFF_OPEN)
    {
        struct TagItem otags[3];
        ULONG err;

        if (!outline->olf_LibraryName || !outline->olf_EEngine.ege_BulletBase || !outline->olf_EEngine.ege_GlyphEngine)
        {
            _diskfont_CloseOutlineFont(DiskfontBase, outline, list);
            return NULL;
        }

        otags[0].ti_Tag = OT_OTagPath;
        otags[0].ti_Data = (ULONG)outline->olf_OTagPath;
        otags[1].ti_Tag = OT_OTagList;
        otags[1].ti_Data = (ULONG)outline->olf_OTagList;
        otags[2].ti_Tag = TAG_DONE;
        otags[2].ti_Data = 0;
        err = _diskfont_ESetInfoA(DiskfontBase, &outline->olf_EEngine, otags);
        if (err != OTERR_Success)
        {
            _diskfont_CloseOutlineFont(DiskfontBase, outline, list);
            return NULL;
        }
    }

    return outline;
}

VOID _diskfont_CloseOutlineFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                  register struct OutlineFont  *olf          __asm("a0"),
                                  register struct List         *list         __asm("a1"))
{
    (void)DiskfontBase;
    (void)list;

    if (!olf)
        return;

    if (olf->olf_EEngine.ege_GlyphEngine)
        _diskfont_ECloseEngine(DiskfontBase, &olf->olf_EEngine);

    if (olf->olf_EEngine.ege_BulletBase)
        CloseLibrary((struct Library *)olf->olf_EEngine.ege_BulletBase);

    if (olf->olf_OTagList)
        FreeMem(olf->olf_OTagList, olf->olf_OTagList[0].ti_Data);
    if (olf->olf_OTagPath)
        FreeMem(olf->olf_OTagPath, strlen((const char *)olf->olf_OTagPath) + 1);
    if (olf->olf_EngineName)
        FreeMem(olf->olf_EngineName, strlen((const char *)olf->olf_EngineName) + 1);
    if (olf->olf_LibraryName)
        FreeMem(olf->olf_LibraryName, strlen((const char *)olf->olf_LibraryName) + 1);

    FreeMem(olf, sizeof(*olf));
}

LONG _diskfont_WriteFontContents ( register struct DiskfontBase        *DiskfontBase        __asm("a6"),
                                   register BPTR                        fontsLock           __asm("a0"),
                                   register CONST_STRPTR                fontName            __asm("a1"),
                                   register CONST struct FontContentsHeader *fontContentsHeader __asm("a2"))
{
    ULONG total_size;
    ULONG entry_size;
    char path[512];
    BPTR fh;

    (void)DiskfontBase;

    if (!fontName || !fontContentsHeader || !_df_endswith((const char *)fontName, ".font"))
        return FALSE;

    if (fontsLock)
    {
        if (!NameFromLock(fontsLock, (STRPTR)path, sizeof(path)) ||
            !AddPart((STRPTR)path, (STRPTR)fontName, sizeof(path)))
        {
            return FALSE;
        }
    }
    else
    {
        if (strlen((const char *)fontName) >= sizeof(path))
            return FALSE;
        strcpy(path, (const char *)fontName);
    }

    entry_size = _df_font_contents_entry_size(fontContentsHeader->fch_FileID);
    total_size = sizeof(struct FontContentsHeader) + ((ULONG)fontContentsHeader->fch_NumEntries * entry_size);

    fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    if (!_df_write_all(fh, fontContentsHeader, total_size))
    {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

LONG _diskfont_WriteDiskFontHeaderA ( register struct DiskfontBase    *DiskfontBase __asm("a6"),
                                      register CONST struct TextFont  *font         __asm("a0"),
                                      register CONST_STRPTR            fileName     __asm("a1"),
                                      register CONST struct TagItem   *tagList      __asm("a2"))
{
    ULONG char_count;
    ULONG char_data_size;
    ULONG char_loc_size;
    ULONG char_space_size;
    ULONG char_kern_size;
    ULONG header_size;
    ULONG payload_size;
    ULONG reloc_offsets[12];
    ULONG reloc_count = 0;
    UBYTE *payload;
    UBYTE *cursor;
    ULONG return_code = 0x70004E75;
    ULONG plane_size = 0;
    ULONG i;

    (void)DiskfontBase;
    (void)tagList;

    if (!font || !fileName || !font->tf_CharData || !font->tf_CharLoc)
        return FALSE;

    char_count = _df_font_char_count(font);
    char_data_size = (ULONG)font->tf_Modulo * (ULONG)font->tf_YSize;
    char_loc_size = char_count * sizeof(ULONG);
    char_space_size = font->tf_CharSpace ? (char_count * sizeof(WORD)) : 0;
    char_kern_size = font->tf_CharKern ? (char_count * sizeof(WORD)) : 0;

    if (font->tf_Style & FSF_COLORFONT)
    {
        struct ColorTextFont *ctf = (struct ColorTextFont *)font;
        plane_size = char_data_size;
        header_size = sizeof(struct df_color_diskfont_header);
        payload_size = sizeof(ULONG) + header_size + (plane_size * ctf->ctf_Depth) + char_loc_size;
    }
    else
    {
        header_size = sizeof(struct DiskFontHeader);
        payload_size = sizeof(ULONG) + header_size + char_data_size + char_loc_size + char_space_size + char_kern_size;
    }

    payload = (UBYTE *)AllocMem(payload_size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!payload)
        return FALSE;

    CopyMem(&return_code, payload, sizeof(return_code));
    cursor = payload + sizeof(ULONG);

    if (font->tf_Style & FSF_COLORFONT)
    {
        struct ColorTextFont *src = (struct ColorTextFont *)font;
        struct df_color_diskfont_header *dst = (struct df_color_diskfont_header *)cursor;
        UBYTE *char_data_base;
        UBYTE *char_loc_base;

        dst->dfh_FileID = DFH_ID;
        dst->dfh_Revision = 1;
        _df_copy_name(dst->dfh_Name, (CONST_STRPTR)font->tf_Message.mn_Node.ln_Name, MAXFONTNAME);
        dst->ctf = *src;

        char_data_base = cursor + header_size;
        char_loc_base = char_data_base + (plane_size * src->ctf_Depth);
        dst->ctf.ctf_TF.tf_CharData = (APTR)(char_data_base - payload);
        dst->ctf.ctf_TF.tf_CharLoc = (APTR)(char_loc_base - payload);
        reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->ctf.ctf_TF.tf_CharData - payload);
        reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->ctf.ctf_TF.tf_CharLoc - payload);

        for (i = 0; i < src->ctf_Depth; i++)
        {
            CopyMem(src->ctf_CharData[i], char_data_base + (plane_size * i), plane_size);
            dst->ctf.ctf_CharData[i] = (APTR)((char_data_base + (plane_size * i)) - payload);
            reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->ctf.ctf_CharData[i] - payload);
        }

        CopyMem(font->tf_CharLoc, char_loc_base, char_loc_size);
    }
    else
    {
        struct DiskFontHeader *dst = (struct DiskFontHeader *)cursor;
        UBYTE *char_data_base;
        UBYTE *char_loc_base;
        UBYTE *char_space_base;
        UBYTE *char_kern_base;

        dst->dfh_FileID = DFH_ID;
        dst->dfh_Revision = 1;
        _df_copy_name(dst->dfh_Name, (CONST_STRPTR)font->tf_Message.mn_Node.ln_Name, MAXFONTNAME);
        dst->dfh_TF = *font;

        char_data_base = cursor + header_size;
        char_loc_base = char_data_base + char_data_size;
        char_space_base = char_loc_base + char_loc_size;
        char_kern_base = char_space_base + char_space_size;

        dst->dfh_TF.tf_CharData = (APTR)(char_data_base - payload);
        dst->dfh_TF.tf_CharLoc = (APTR)(char_loc_base - payload);
        reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->dfh_TF.tf_CharData - payload);
        reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->dfh_TF.tf_CharLoc - payload);

        CopyMem(font->tf_CharData, char_data_base, char_data_size);
        CopyMem(font->tf_CharLoc, char_loc_base, char_loc_size);

        if (char_space_size)
        {
            dst->dfh_TF.tf_CharSpace = (APTR)(char_space_base - payload);
            reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->dfh_TF.tf_CharSpace - payload);
            CopyMem(font->tf_CharSpace, char_space_base, char_space_size);
        }

        if (char_kern_size)
        {
            dst->dfh_TF.tf_CharKern = (APTR)(char_kern_base - payload);
            reloc_offsets[reloc_count++] = (ULONG)((UBYTE *)&dst->dfh_TF.tf_CharKern - payload);
            CopyMem(font->tf_CharKern, char_kern_base, char_kern_size);
        }
    }

    if (!_df_write_diskfont_hunk(fileName, payload, payload_size, reloc_offsets, reloc_count))
    {
        FreeMem(payload, payload_size);
        return FALSE;
    }

    FreeMem(payload, payload_size);
    return TRUE;
}

ULONG _diskfont_ObtainCharsetInfo ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                    register ULONG                knownTag     __asm("d0"),
                                    register ULONG                knownValue   __asm("d1"),
                                    register ULONG                wantedTag    __asm("d2"))
{
    struct df_charset_info *info;

    (void)DiskfontBase;

    info = _df_find_charset(knownTag, knownValue);
    if (!info)
        return 0;

    switch (wantedTag)
    {
        case DFCS_NUMBER:
        case DFCS_NEXTNUMBER:
            return info->number;

        case DFCS_NAME:
            return (ULONG)info->name;

        case DFCS_MIMENAME:
            return (ULONG)info->mime_name;

        case DFCS_MAPTABLE:
            return (ULONG)info->map_table;

        default:
            return 0;
    }
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
