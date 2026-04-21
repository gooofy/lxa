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

    size = Seek(fh, 0, OFFSET_END);
    if (size <= 0 || Seek(fh, 0, OFFSET_BEGINNING) < 0)
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
    BOOL   fill_entries;
    UWORD  num_entries;
    ULONG  name_bytes;
    UBYTE *entry_ptr;
    STRPTR name_ptr;
};

static VOID _df_new_min_list(struct MinList *list)
{
    list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
    list->mlh_Tail = NULL;
    list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
}

static BOOL _df_textattr_matches(CONST struct TextAttr *lhs, CONST struct TextAttr *rhs)
{
    return lhs && rhs && lhs->ta_Name && rhs->ta_Name &&
           _df_stricmp((const char *)lhs->ta_Name, (const char *)rhs->ta_Name) == 0 &&
           lhs->ta_YSize == rhs->ta_YSize &&
           lhs->ta_Style == rhs->ta_Style &&
           lhs->ta_Flags == rhs->ta_Flags;
}

static struct CachedFontEntry *_df_find_cached_font_entry(CONST struct TextAttr *textAttr)
{
    struct CachedFontEntry *entry;

    if (!textAttr || !textAttr->ta_Name)
        return NULL;

    for (entry = (struct CachedFontEntry *)g_diskfont_cache_state.font_entries.mlh_Head;
         entry->node.mln_Succ;
         entry = (struct CachedFontEntry *)entry->node.mln_Succ)
    {
        if (_df_stricmp(entry->name, (const char *)textAttr->ta_Name) == 0 &&
            entry->ysize == textAttr->ta_YSize &&
            entry->style == textAttr->ta_Style &&
            entry->flags == textAttr->ta_Flags)
        {
            return entry;
        }
    }

    return NULL;
}

static struct TextFont *_df_find_cached_font(CONST struct TextAttr *textAttr)
{
    struct CachedFontEntry *entry = _df_find_cached_font_entry(textAttr);

    return entry ? entry->font : NULL;
}

static VOID _df_cache_font(struct DiskfontBase *DiskfontBase,
                           CONST struct TextAttr *textAttr,
                           struct TextFont       *font)
{
    struct CachedFontEntry *entry;

    if (!DiskfontBase || !textAttr || !textAttr->ta_Name || !font)
        return;

    entry = _df_find_cached_font_entry(textAttr);
    if (!entry)
    {
        entry = AllocMem(sizeof(*entry), MEMF_PUBLIC | MEMF_CLEAR);
        if (!entry)
            return;

        AddHead((struct List *)&g_diskfont_cache_state.font_entries, (struct Node *)entry);
    }

    strcpy(entry->name, (const char *)textAttr->ta_Name);
    entry->ysize = textAttr->ta_YSize;
    entry->style = textAttr->ta_Style;
    entry->flags = textAttr->ta_Flags;
    entry->font = font;

    DiskfontBase->cached_attr = *textAttr;
    DiskfontBase->cached_font = font;
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

static VOID _df_avail_emit_entry(struct df_avail_fonts_state *state,
                                 UWORD                        type,
                                 CONST_STRPTR                 name,
                                 UWORD                        ysize,
                                 UBYTE                        style,
                                 UBYTE                        font_flags)
{
    ULONG name_len;

    if (!state || !name)
        return;

    name_len = strlen((const char *)name) + 1;

    if (state->fill_entries)
    {
        if (state->request_flags & AFF_TAGGED)
        {
            struct TAvailFonts *taf = (struct TAvailFonts *)state->entry_ptr;

            taf->taf_Type = type;
            taf->taf_Attr.tta_Name = state->name_ptr;
            taf->taf_Attr.tta_YSize = ysize;
            taf->taf_Attr.tta_Style = style;
            taf->taf_Attr.tta_Flags = font_flags;
            taf->taf_Attr.tta_Tags = NULL;

            state->entry_ptr += sizeof(struct TAvailFonts);
        }
        else
        {
            struct AvailFonts *af = (struct AvailFonts *)state->entry_ptr;

            af->af_Type = type;
            af->af_Attr.ta_Name = state->name_ptr;
            af->af_Attr.ta_YSize = ysize;
            af->af_Attr.ta_Style = style;
            af->af_Attr.ta_Flags = font_flags;

            state->entry_ptr += sizeof(struct AvailFonts);
        }

        CopyMem((APTR)name, state->name_ptr, name_len);
        state->name_ptr += name_len;
    }
    else
    {
        state->name_bytes += name_len;
    }

    state->num_entries++;
}

static BOOL _df_entry_matches_family(CONST_STRPTR font_file_name, CONST_STRPTR entry_file_name)
{
    char family[MAXFONTPATH];
    ULONG i = 0;
    ULONG j = 0;

    if (!font_file_name || !entry_file_name)
        return FALSE;

    while (font_file_name[i] && font_file_name[i] != '.' && i + 1 < sizeof(family))
    {
        family[i] = font_file_name[i];
        i++;
    }
    family[i] = '\0';

    while (entry_file_name[j] && entry_file_name[j] != '/' && entry_file_name[j] != ':' && j + 1 < sizeof(family))
    {
        if (family[j] == '\0' || family[j] != entry_file_name[j])
            return FALSE;
        j++;
    }

    return family[j] == '\0' && (entry_file_name[j] == '/' || entry_file_name[j] == ':' || entry_file_name[j] == '\0');
}

static VOID _df_avail_collect_memory_fonts(struct df_avail_fonts_state *state)
{
    struct Node *node;

    if (!state)
        return;

    for (node = GETHEAD(&GfxBase->TextFonts); node; node = GETSUCC(node))
    {
        struct TextFont *font = (struct TextFont *)node;

        if (!font->tf_Message.mn_Node.ln_Name)
            continue;

        if (!(state->request_flags & AFF_SCALED) && !(font->tf_Flags & FPF_DESIGNED))
            continue;

        _df_avail_emit_entry(state,
                             AFF_MEMORY,
                             (CONST_STRPTR)font->tf_Message.mn_Node.ln_Name,
                             font->tf_YSize,
                             font->tf_Style,
                             font->tf_Flags);
    }
}

static VOID _df_avail_collect_disk_fonts(struct df_avail_fonts_state *state)
{
    BPTR fontsLock;

    if (!state)
        return;

    fontsLock = Lock((STRPTR)"FONTS:", ACCESS_READ);
    if (fontsLock)
    {
        struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocVec(sizeof(struct FileInfoBlock), MEMF_PUBLIC | MEMF_CLEAR);

        if (fib)
        {
            if (Examine(fontsLock, fib))
            {
                BOOL done = FALSE;

                while (!done)
                {
                    if (fib->fib_DirEntryType < 0 && _df_endswith((const char *)fib->fib_FileName, ".font"))
                    {
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
                                UWORD disk_type = AFF_DISK;

                                if (state->request_flags & AFF_TYPE)
                                {
                                    disk_type = (fch.fch_FileID == OFCH_ID) ? (AFF_DISK | AFF_OTAG) : (AFF_DISK | AFF_BITMAP);
                                }

                                if ((state->request_flags & AFF_OTAG) && fch.fch_FileID != OFCH_ID)
                                {
                                    Close(fh);
                                    goto next_diskfont_entry;
                                }

                                if ((state->request_flags & AFF_BITMAP) && fch.fch_FileID == OFCH_ID)
                                {
                                    Close(fh);
                                    goto next_diskfont_entry;
                                }

                                for (j = 0; j < fch.fch_NumEntries; j++)
                                {
                                    if (fch.fch_FileID == FCH_ID)
                                    {
                                        struct FontContents fc;

                                        if (Read(fh, &fc, sizeof(fc)) != sizeof(fc))
                                            break;

                                        if (!_df_entry_matches_family((CONST_STRPTR)fib->fib_FileName,
                                                                      (CONST_STRPTR)fc.fc_FileName))
                                        {
                                            continue;
                                        }

                                        _df_avail_emit_entry(state,
                                                             disk_type,
                                                             (CONST_STRPTR)fib->fib_FileName,
                                                             fc.fc_YSize,
                                                             fc.fc_Style,
                                                             fc.fc_Flags);
                                    }
                                    else
                                    {
                                        struct TFontContents tfc;

                                        if (Read(fh, &tfc, sizeof(tfc)) != sizeof(tfc))
                                            break;

                                        if (!_df_entry_matches_family((CONST_STRPTR)fib->fib_FileName,
                                                                      (CONST_STRPTR)tfc.tfc_FileName))
                                        {
                                            continue;
                                        }

                                        _df_avail_emit_entry(state,
                                                             disk_type,
                                                             (CONST_STRPTR)fib->fib_FileName,
                                                             tfc.tfc_YSize,
                                                             tfc.tfc_Style,
                                                             tfc.tfc_Flags);
                                    }
                                }
                            }

                            Close(fh);
                        }
                    }

next_diskfont_entry:
                    if (!ExNext(fontsLock, fib))
                        done = TRUE;
                }
            }

            FreeVec(fib);
        }

        UnLock(fontsLock);
    }
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
    char font_dir[300];
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

    if (_df_has_path((const char *)textAttr->ta_Name) && _df_endswith((const char *)textAttr->ta_Name, ".font"))
    {
        strcpy(path, (const char *)textAttr->ta_Name);
        if (!_df_parent_dir((const char *)textAttr->ta_Name, font_dir, sizeof(font_dir)))
            return NULL;
    }
    else
    {
        strcpy(path, "FONTS:");
        strcat(path, stripped);
        strcat(path, ".font");
        strcpy(font_dir, "FONTS:");
    }

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

    strcpy(path, font_dir);
    if (!AddPart((STRPTR)path, (STRPTR)bestFile, sizeof(path)))
        return NULL;

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

    /* Determine the registration name for this font.
     *
     * Real AmigaOS stores the plain font name (e.g. "Personal.font") in
     * dfh_Name inside the bitmap file.  Some commercial fonts embed only a
     * short token (e.g. "Personal8") without the ".font" suffix.  When the
     * caller supplied a full Amiga path ("PPaint:fonts/Personal.font") we
     * extract the basename from ta_Name so the font is registered under
     * "Personal.font" — the name that subsequent plain OpenFont() calls will
     * use.  When ta_Name is already a plain name we copy it as-is; if
     * dfh_Name lacks ".font" we append the suffix. */
    if (_df_has_path((const char *)textAttr->ta_Name))
    {
        /* Extract the last path component (after the last '/' or ':') as the
         * registration name. */
        const UBYTE *p = textAttr->ta_Name;
        const UBYTE *base = p;
        while (*p)
        {
            if (*p == '/' || *p == ':')
                base = p + 1;
            p++;
        }
        if (strlen((const char *)base) < sizeof(dfh->dfh_Name))
            strcpy((char *)dfh->dfh_Name, (const char *)base);
    }
    else if (strlen((const char *)textAttr->ta_Name) < sizeof(dfh->dfh_Name))
    {
        strcpy((char *)dfh->dfh_Name, (const char *)textAttr->ta_Name);
    }

    /* Ensure the registration name ends with ".font". */
    if (!_df_endswith((const char *)dfh->dfh_Name, ".font") &&
        (strlen((const char *)dfh->dfh_Name) + 5) < sizeof(dfh->dfh_Name))
    {
        strcat((char *)dfh->dfh_Name, ".font");
    }

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

static struct TextFont *_df_lookup_loaded_font(struct DiskfontBase *DiskfontBase,
                                               struct TextAttr     *textAttr)
{
    struct TextFont *font;

    if (!textAttr || !textAttr->ta_Name)
        return NULL;

    if (DiskfontBase->cache_enabled && _df_textattr_matches(&DiskfontBase->cached_attr, textAttr) &&
        DiskfontBase->cached_font)
    {
        font = DiskfontBase->cached_font;
        font->tf_Accessors++;
        return font;
    }

    if (DiskfontBase->cache_enabled)
    {
        font = _df_find_cached_font(textAttr);
        if (font)
        {
            DiskfontBase->cached_attr = *textAttr;
            DiskfontBase->cached_font = font;
            font->tf_Accessors++;
            return font;
        }
    }

    return NULL;
}

struct TextFont * _diskfont_OpenDiskFont ( register struct DiskfontBase *DiskfontBase __asm("a6"),
                                           register struct TextAttr     *textAttr     __asm("a0"))
{
    struct TextFont *font;

    DPRINTF (LOG_DEBUG, "_diskfont: OpenDiskFont() called name='%s' size=%d\n",
             textAttr ? (char *)textAttr->ta_Name : "(null)",
             textAttr ? textAttr->ta_YSize : 0);

    if (!textAttr || !textAttr->ta_Name)
        return NULL;

    font = _df_lookup_loaded_font(DiskfontBase, textAttr);
    if (font)
        return font;

    /* First try to open from ROM font list via graphics.library */
    font = OpenFont(textAttr);
    if (font)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: Found font in ROM/memory\n");
        if (DiskfontBase->cache_enabled)
            _df_cache_font(DiskfontBase, textAttr, font);
        return font;
    }

    /* Try to load from FONTS: directory */
    font = _df_load_from_disk(textAttr);
    if (font)
    {
        DPRINTF (LOG_DEBUG, "_diskfont: Loaded font from disk\n");
        if (DiskfontBase->cache_enabled)
            _df_cache_font(DiskfontBase, textAttr, font);
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
    struct df_avail_fonts_state state;
    ULONG bytesNeeded;
    ULONG entry_size;
    UWORD num_entries;
    LONG shortage;

    /* bufBytes is a LONG buffer size passed via move.l — do NOT sign-extend */

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() called buffer=0x%08lx bufBytes=%ld flags=0x%08lx\n",
             (ULONG)buffer, bufBytes, flags);

    memset(&state, 0, sizeof(state));
    state.request_flags = flags;

    if (flags & AFF_MEMORY)
        _df_avail_collect_memory_fonts(&state);

    if (flags & AFF_DISK)
        _df_avail_collect_disk_fonts(&state);

    entry_size = (flags & AFF_TAGGED) ? sizeof(struct TAvailFonts) : sizeof(struct AvailFonts);
    num_entries = state.num_entries;
    bytesNeeded = sizeof(struct AvailFontsHeader) +
                  ((ULONG)num_entries * entry_size) +
                  state.name_bytes;

    DPRINTF (LOG_DEBUG, "_diskfont: AvailFonts() numEntries=%d bytesNeeded=%ld\n",
             state.num_entries, bytesNeeded);

    if (!buffer || bufBytes < bytesNeeded)
    {
        shortage = (LONG)bytesNeeded - (bufBytes > 0 ? bufBytes : 0);
        return shortage > 0 ? shortage : 0;
    }

    memset(&state, 0, sizeof(state));
    state.request_flags = flags;
    state.fill_entries = TRUE;
    buffer->afh_NumEntries = num_entries;
    state.entry_ptr = (UBYTE *)(buffer + 1);
    state.name_ptr = (STRPTR)((UBYTE *)(buffer + 1) + ((ULONG)num_entries * entry_size));

    if (flags & AFF_MEMORY)
        _df_avail_collect_memory_fonts(&state);

    if (flags & AFF_DISK)
        _df_avail_collect_disk_fonts(&state);

    buffer->afh_NumEntries = state.num_entries;

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
    struct FontContentsHeader header;
    struct FontContentsHeader *result;
    ULONG entry_size;
    ULONG total_size;
    LONG remaining;
    BPTR fh;
    char path[512];

    DPRINTF (LOG_DEBUG, "_diskfont: NewFontContents() fontsLock=0x%08lx fontName='%s'\n",
             (ULONG)fontsLock, STRORNULL(fontName));

    if (!fontName || !_df_endswith((const char *)fontName, ".font") || strlen((const char *)fontName) >= MAXFONTPATH)
        return NULL;

    if (fontsLock)
    {
        if (!NameFromLock(fontsLock, (STRPTR)path, sizeof(path)))
            return NULL;

        if (!AddPart((STRPTR)path, (STRPTR)fontName, sizeof(path)))
            return NULL;
    }
    else
    {
        strcpy(path, (const char *)fontName);
    }

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return NULL;

    if (Read(fh, &header, sizeof(header)) != sizeof(header))
    {
        Close(fh);
        return NULL;
    }

    if (header.fch_FileID != FCH_ID && header.fch_FileID != TFCH_ID && header.fch_FileID != OFCH_ID)
    {
        Close(fh);
        return NULL;
    }

    entry_size = _df_font_contents_entry_size(header.fch_FileID);
    total_size = sizeof(struct FontContentsHeader) + ((ULONG)header.fch_NumEntries * entry_size);

    result = (struct FontContentsHeader *)AllocMem(total_size, MEMF_PUBLIC);
    if (!result)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        Close(fh);
        return NULL;
    }

    result->fch_FileID = header.fch_FileID;
    result->fch_NumEntries = header.fch_NumEntries;

    remaining = (LONG)(total_size - sizeof(struct FontContentsHeader));
    if (remaining > 0)
    {
        if (Read(fh, (UBYTE *)result + sizeof(struct FontContentsHeader), remaining) != remaining)
        {
            FreeMem(result, total_size);
            Close(fh);
            return NULL;
        }
    }

    Close(fh);
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
    struct TextAttr source_attr;

    DPRINTF (LOG_DEBUG, "_diskfont: NewScaledDiskFont() sourceFont=0x%08lx destTextAttr=0x%08lx\n",
             (ULONG)sourceFont, (ULONG)destTextAttr);

    if (!sourceFont || !destTextAttr)
        return NULL;

    if (DiskfontBase->cache_enabled && DiskfontBase->cached_scaled_font &&
        DiskfontBase->cached_scaled_font == (struct TextFont *)sourceFont &&
        DiskfontBase->cached_attr.ta_Name == destTextAttr->ta_Name &&
        DiskfontBase->cached_attr.ta_YSize == destTextAttr->ta_YSize &&
        DiskfontBase->cached_attr.ta_Style == destTextAttr->ta_Style &&
        DiskfontBase->cached_attr.ta_Flags == destTextAttr->ta_Flags)
    {
        return (struct DiskFont *)sourceFont;
    }

    source_attr.ta_Name = (STRPTR)sourceFont->tf_Message.mn_Node.ln_Name;
    source_attr.ta_YSize = destTextAttr->ta_YSize;
    source_attr.ta_Style = destTextAttr->ta_Style;
    source_attr.ta_Flags = destTextAttr->ta_Flags;

    if (!_df_lookup_loaded_font(DiskfontBase, &source_attr))
        return NULL;

    DiskfontBase->cached_scaled_font = sourceFont;
    DiskfontBase->cached_attr = *destTextAttr;
    return (struct DiskFont *)sourceFont;
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
