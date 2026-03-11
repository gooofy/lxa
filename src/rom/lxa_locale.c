/*
 * lxa locale.library implementation
 *
 * Provides internationalization and localization support.
 * This is a stub implementation with basic US English defaults.
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

#include <libraries/locale.h>
#include <libraries/iffparse.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>
#include <clib/utility_protos.h>
#include <inline/utility.h>

#include "util.h"

#define VERSION    47
#define REVISION   1
#define EXLIBNAME  "locale"
#define EXLIBVER   " 47.1 (2025/02/01)"

char __aligned _g_locale_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_locale_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_locale_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_locale_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
extern struct UtilityBase *UtilityBase;

/* LocaleBase structure (extends standard definition) */
struct MyLocaleBase {
    struct LocaleBase lb;
    BPTR              SegList;
    struct Locale    *cached_locale;
    struct Catalog   *cached_catalog;
    STRPTR            cached_locale_name;
    STRPTR            cached_catalog_name;
    STRPTR            cached_catalog_language;
    UWORD             cached_catalog_version;
};

#define LXA_CATALOG_MAGIC 0x4c434154UL

struct CatalogEntry
{
    struct CatalogEntry *next;
    LONG                 id;
    STRPTR               string;
};

struct MyCatalog
{
    struct Catalog       cat;
    ULONG                magic;
    struct CatalogEntry *entries;
};

/* Default locale strings (English US) */
static const char *g_LocaleStrings[] = {
    NULL,           /* 0 - unused */
    "Sunday",       /* DAY_1 */
    "Monday",       /* DAY_2 */
    "Tuesday",      /* DAY_3 */
    "Wednesday",    /* DAY_4 */
    "Thursday",     /* DAY_5 */
    "Friday",       /* DAY_6 */
    "Saturday",     /* DAY_7 */
    "Sun",          /* ABDAY_1 */
    "Mon",          /* ABDAY_2 */
    "Tue",          /* ABDAY_3 */
    "Wed",          /* ABDAY_4 */
    "Thu",          /* ABDAY_5 */
    "Fri",          /* ABDAY_6 */
    "Sat",          /* ABDAY_7 */
    "January",      /* MON_1 */
    "February",     /* MON_2 */
    "March",        /* MON_3 */
    "April",        /* MON_4 */
    "May",          /* MON_5 */
    "June",         /* MON_6 */
    "July",         /* MON_7 */
    "August",       /* MON_8 */
    "September",    /* MON_9 */
    "October",      /* MON_10 */
    "November",     /* MON_11 */
    "December",     /* MON_12 */
    "Jan",          /* ABMON_1 */
    "Feb",          /* ABMON_2 */
    "Mar",          /* ABMON_3 */
    "Apr",          /* ABMON_4 */
    "May",          /* ABMON_5 */
    "Jun",          /* ABMON_6 */
    "Jul",          /* ABMON_7 */
    "Aug",          /* ABMON_8 */
    "Sep",          /* ABMON_9 */
    "Oct",          /* ABMON_10 */
    "Nov",          /* ABMON_11 */
    "Dec",          /* ABMON_12 */
    "Yes",          /* YESSTR */
    "No",           /* NOSTR */
    "AM",           /* AM_STR */
    "PM",           /* PM_STR */
    "-",            /* SOFTHYPHEN */
    "-",            /* HARDHYPHEN */
    "\"",           /* OPENQUOTE */
    "\"",           /* CLOSEQUOTE */
    "Yesterday",    /* YESTERDAYSTR */
    "Today",        /* TODAYSTR */
    "Tomorrow",     /* TOMORROWSTR */
    "Future",       /* FUTURESTR */
    NULL,           /* 51 - unused */
    "Sunday",       /* ALTDAY_1 */
    "Monday",       /* ALTDAY_2 */
    "Tuesday",      /* ALTDAY_3 */
    "Wednesday",    /* ALTDAY_4 */
    "Thursday",     /* ALTDAY_5 */
    "Friday",       /* ALTDAY_6 */
    "Saturday",     /* ALTDAY_7 */
    "January",      /* ALTMON_1 */
    "February",     /* ALTMON_2 */
    "March",        /* ALTMON_3 */
    "April",        /* ALTMON_4 */
    "May",          /* ALTMON_5 */
    "June",         /* ALTMON_6 */
    "July",         /* ALTMON_7 */
    "August",       /* ALTMON_8 */
    "September",    /* ALTMON_9 */
    "October",      /* ALTMON_10 */
    "November",     /* ALTMON_11 */
    "December",     /* ALTMON_12 */
};

/* Default locale structure */
static UBYTE g_DefaultLocaleName[] = "english";
static UBYTE g_DefaultLanguageName[] = "english";
static UBYTE g_DefaultDateTimeFormat[] = "%A %B %e %Y %H:%M:%S";
static UBYTE g_DefaultDateFormat[] = "%A %B %e %Y";
static UBYTE g_DefaultTimeFormat[] = "%H:%M:%S";
static UBYTE g_DefaultShortDateTimeFormat[] = "%m/%d/%y %H:%M";
static UBYTE g_DefaultShortDateFormat[] = "%m/%d/%y";
static UBYTE g_DefaultShortTimeFormat[] = "%H:%M";
static UBYTE g_DefaultDecimalPoint[] = ".";
static UBYTE g_DefaultGroupSeparator[] = ",";
static UBYTE g_DefaultMonCS[] = "$";
static UBYTE g_DefaultMonIntCS[] = "USD";
static UBYTE g_DefaultGrouping[] = { 3, 0 };

static struct Locale g_DefaultLocale = {
    g_DefaultLocaleName,             /* loc_LocaleName */
    g_DefaultLanguageName,           /* loc_LanguageName */
    { NULL },                        /* loc_PrefLanguages */
    0,                               /* loc_Flags */
    0,                               /* loc_CodeSet */
    1,                               /* loc_CountryCode (USA) */
    1,                               /* loc_TelephoneCode */
    0,                               /* loc_GMTOffset (UTC for now) */
    MS_AMERICAN,                     /* loc_MeasuringSystem */
    CT_7SUN,                         /* loc_CalendarType */
    { 0, 0 },                        /* loc_Reserved0 */
    g_DefaultDateTimeFormat,         /* loc_DateTimeFormat */
    g_DefaultDateFormat,             /* loc_DateFormat */
    g_DefaultTimeFormat,             /* loc_TimeFormat */
    g_DefaultShortDateTimeFormat,    /* loc_ShortDateTimeFormat */
    g_DefaultShortDateFormat,        /* loc_ShortDateFormat */
    g_DefaultShortTimeFormat,        /* loc_ShortTimeFormat */
    g_DefaultDecimalPoint,           /* loc_DecimalPoint */
    g_DefaultGroupSeparator,         /* loc_GroupSeparator */
    g_DefaultGroupSeparator,         /* loc_FracGroupSeparator */
    g_DefaultGrouping,               /* loc_Grouping */
    g_DefaultGrouping,               /* loc_FracGrouping */
    g_DefaultDecimalPoint,           /* loc_MonDecimalPoint */
    g_DefaultGroupSeparator,         /* loc_MonGroupSeparator */
    g_DefaultGroupSeparator,         /* loc_MonFracGroupSeparator */
    g_DefaultGrouping,               /* loc_MonGrouping */
    g_DefaultGrouping,               /* loc_MonFracGrouping */
    2,                               /* loc_MonFracDigits */
    2,                               /* loc_MonIntFracDigits */
    { 0, 0 },                        /* loc_Reserved1 */
    g_DefaultMonCS,                  /* loc_MonCS */
    g_DefaultMonCS,                  /* loc_MonSmallCS */
    g_DefaultMonIntCS,               /* loc_MonIntCS */
    (STRPTR)"",                      /* loc_MonPositiveSign */
    SS_SPACE,                        /* loc_MonPositiveSpaceSep */
    SP_PREC_ALL,                     /* loc_MonPositiveSignPos */
    CSP_PRECEDES,                    /* loc_MonPositiveCSPos */
    0,                               /* loc_Reserved2 */
    (STRPTR)"-",                     /* loc_MonNegativeSign */
    SS_SPACE,                        /* loc_MonNegativeSpaceSep */
    SP_PREC_ALL,                     /* loc_MonNegativeSignPos */
    CSP_PRECEDES,                    /* loc_MonNegativeCSPos */
    0,                               /* loc_Reserved3 */
};

static LONG _loc_strlen(CONST_STRPTR str)
{
    LONG len = 0;

    if (!str)
        return 0;

    while (str[len])
        len++;

    return len;
}

static STRPTR _loc_dup_bytes(const UBYTE *buffer, ULONG size);

static LONG _loc_strcmp(CONST_STRPTR lhs, CONST_STRPTR rhs)
{
    if (!lhs)
        return rhs ? -1 : 0;
    if (!rhs)
        return 1;

    while (*lhs && *rhs && *lhs == *rhs)
    {
        lhs++;
        rhs++;
    }

    return (LONG)(UBYTE)*lhs - (LONG)(UBYTE)*rhs;
}

static ULONG _loc_ascii_upper(ULONG character)
{
    if (character >= 'a' && character <= 'z')
        return character - ('a' - 'A');

    return character;
}

static ULONG _loc_ascii_lower(ULONG character)
{
    if (character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');

    return character;
}

static BOOL _loc_strieq_ascii(CONST_STRPTR lhs, CONST_STRPTR rhs)
{
    if (!lhs || !rhs)
        return lhs == rhs;

    while (*lhs && *rhs)
    {
        if (_loc_ascii_lower((ULONG)(UBYTE)*lhs) != _loc_ascii_lower((ULONG)(UBYTE)*rhs))
            return FALSE;

        lhs++;
        rhs++;
    }

    return *lhs == *rhs;
}

static CONST_STRPTR _loc_effective_locale_name(CONST_STRPTR name)
{
    if (!name || !*name)
        return (CONST_STRPTR)g_DefaultLocaleName;

    return name;
}

static BOOL _loc_is_default_locale_name(CONST_STRPTR name)
{
    CONST_STRPTR effective_name = _loc_effective_locale_name(name);

    return _loc_strieq_ascii(effective_name, (CONST_STRPTR)g_DefaultLocaleName) ||
           _loc_strieq_ascii(effective_name, (CONST_STRPTR)g_DefaultLanguageName);
}

static BOOL _loc_catalog_cache_matches(struct MyLocaleBase *LocaleBase,
                                       CONST_STRPTR         name,
                                       CONST_STRPTR         language,
                                       UWORD                required_version)
{
    return LocaleBase &&
           LocaleBase->cached_catalog &&
           LocaleBase->cached_catalog_name &&
           LocaleBase->cached_catalog_language &&
           LocaleBase->cached_catalog_version == required_version &&
           _loc_strieq_ascii(LocaleBase->cached_catalog_name, name) &&
           _loc_strieq_ascii(LocaleBase->cached_catalog_language, language);
}

static VOID _loc_clear_catalog_cache(struct MyLocaleBase *LocaleBase,
                                     CONST struct Catalog *catalog)
{
    if (!LocaleBase || !LocaleBase->cached_catalog)
        return;

    if (catalog && LocaleBase->cached_catalog != catalog)
        return;

    LocaleBase->cached_catalog = NULL;
    LocaleBase->cached_catalog_version = 0;

    if (LocaleBase->cached_catalog_name)
    {
        FreeVec(LocaleBase->cached_catalog_name);
        LocaleBase->cached_catalog_name = NULL;
    }

    if (LocaleBase->cached_catalog_language)
    {
        FreeVec(LocaleBase->cached_catalog_language);
        LocaleBase->cached_catalog_language = NULL;
    }
}

static VOID _loc_cache_catalog(struct MyLocaleBase *LocaleBase,
                               CONST_STRPTR         name,
                               CONST_STRPTR         language,
                               UWORD                required_version,
                               struct Catalog      *catalog)
{
    STRPTR cached_name;
    STRPTR cached_language;

    if (!LocaleBase || !catalog || !name || !language)
        return;

    cached_name = _loc_dup_bytes((const UBYTE *)name, _loc_strlen(name));
    if (!cached_name)
        return;

    cached_language = _loc_dup_bytes((const UBYTE *)language, _loc_strlen(language));
    if (!cached_language)
    {
        FreeVec(cached_name);
        return;
    }

    _loc_clear_catalog_cache(LocaleBase, NULL);

    LocaleBase->cached_catalog = catalog;
    LocaleBase->cached_catalog_name = cached_name;
    LocaleBase->cached_catalog_language = cached_language;
    LocaleBase->cached_catalog_version = required_version;
}

static ULONG _loc_be32(const UBYTE *buffer)
{
    return ((ULONG)buffer[0] << 24) |
           ((ULONG)buffer[1] << 16) |
           ((ULONG)buffer[2] << 8)  |
           ((ULONG)buffer[3]);
}

static BOOL _loc_read_exact(BPTR fh, APTR buffer, ULONG size)
{
    if (size == 0)
        return TRUE;

    return Read(fh, buffer, size) == (LONG)size;
}

static BOOL _loc_skip_bytes(BPTR fh, ULONG size)
{
    if (size == 0)
        return TRUE;

    return Seek(fh, size, OFFSET_CURRENT) >= 0;
}

static STRPTR _loc_dup_bytes(const UBYTE *buffer, ULONG size)
{
    STRPTR result;

    while (size > 0 && buffer[size - 1] == 0)
        size--;

    result = AllocVec(size + 1, MEMF_CLEAR);
    if (!result)
        return NULL;

    if (size > 0)
        CopyMem((APTR)buffer, result, size);

    result[size] = '\0';
    return result;
}

static void _loc_free_catalog(struct MyCatalog *catalog)
{
    struct CatalogEntry *entry;
    struct CatalogEntry *next;

    if (!catalog || catalog->magic != LXA_CATALOG_MAGIC)
        return;

    entry = catalog->entries;
    while (entry)
    {
        next = entry->next;
        if (entry->string)
            FreeVec(entry->string);
        FreeVec(entry);
        entry = next;
    }

    if (catalog->cat.cat_Language)
        FreeVec(catalog->cat.cat_Language);

    catalog->magic = 0;
    FreeVec(catalog);
}

static BOOL _loc_add_catalog_entry(struct MyCatalog *catalog, LONG id, STRPTR string)
{
    struct CatalogEntry *entry;
    struct CatalogEntry **tail;

    entry = AllocVec(sizeof(*entry), MEMF_CLEAR);
    if (!entry)
        return FALSE;

    entry->id = id;
    entry->string = string;

    tail = &catalog->entries;
    while (*tail)
        tail = &(*tail)->next;

    *tail = entry;
    return TRUE;
}

static void _loc_parse_version_string(CONST_STRPTR version_string,
                                      UWORD       *version,
                                      UWORD       *revision)
{
    ULONG value = 0;

    *version = 0;
    *revision = 0;

    if (!version_string)
        return;

    while (*version_string)
    {
        if (*version_string >= '0' && *version_string <= '9')
        {
            value = 0;
            while (*version_string >= '0' && *version_string <= '9')
            {
                value = value * 10 + (*version_string - '0');
                version_string++;
            }

            *version = (UWORD)value;

            if (*version_string == '.')
            {
                version_string++;
                value = 0;
                while (*version_string >= '0' && *version_string <= '9')
                {
                    value = value * 10 + (*version_string - '0');
                    version_string++;
                }
                *revision = (UWORD)value;
            }
            return;
        }

        version_string++;
    }
}

static ULONG _loc_collate_primary(ULONG character)
{
    return _loc_ascii_upper(character);
}

static ULONG _loc_collate_secondary(ULONG character)
{
    ULONG primary = _loc_collate_primary(character);

    if (character == primary)
        return 0;

    return character;
}

static LONG _loc_compare_folded(CONST_STRPTR string1, CONST_STRPTR string2, LONG length)
{
    while (*string1 && *string2 && length != 0)
    {
        LONG c1 = (LONG)_loc_collate_primary((ULONG)(UBYTE)*string1++);
        LONG c2 = (LONG)_loc_collate_primary((ULONG)(UBYTE)*string2++);

        if (c1 != c2)
            return c1 - c2;

        if (length > 0)
            length--;
    }

    if (length == 0)
        return 0;

    return (LONG)_loc_collate_primary((ULONG)(UBYTE)*string1) -
           (LONG)_loc_collate_primary((ULONG)(UBYTE)*string2);
}

static LONG _loc_compare_secondary(CONST_STRPTR string1, CONST_STRPTR string2, LONG length)
{
    while (*string1 && *string2 && length != 0)
    {
        LONG c1 = (LONG)_loc_collate_secondary((ULONG)(UBYTE)*string1++);
        LONG c2 = (LONG)_loc_collate_secondary((ULONG)(UBYTE)*string2++);

        if (c1 != c2)
            return c1 - c2;

        if (length > 0)
            length--;
    }

    if (length == 0)
        return 0;

    return (LONG)_loc_collate_secondary((ULONG)(UBYTE)*string1) -
           (LONG)_loc_collate_secondary((ULONG)(UBYTE)*string2);
}

static ULONG _loc_strconvert_ascii(CONST_STRPTR string,
                                   APTR         buffer,
                                   ULONG        buffer_size,
                                   ULONG        type)
{
    UBYTE *dst = (UBYTE *)buffer;
    ULONG remaining = buffer_size;
    ULONG written = 0;
    CONST_STRPTR orig;

    if (!buffer || buffer_size == 0)
        return 0;

    if (!string)
        string = (CONST_STRPTR)"";

    orig = string;

    while (*string)
    {
        ULONG folded = _loc_collate_primary((ULONG)(UBYTE)*string);

        if (remaining > 1)
        {
            *dst++ = (UBYTE)folded;
            remaining--;
            written++;
        }

        string++;
    }

    if (type == SC_COLLATE2)
    {
        string = orig;
        while (*string)
        {
            ULONG original = (ULONG)(UBYTE)*string;
            ULONG folded = _loc_collate_primary(original);

            if (original != folded && remaining > 1)
            {
                *dst++ = (UBYTE)original;
                remaining--;
                written++;
            }

            string++;
        }
    }

    if (remaining > 0)
        *dst = '\0';
    else
        ((UBYTE *)buffer)[buffer_size - 1] = '\0';

    return written;
}

static BOOL _loc_parse_catalog_strings(BPTR fh, struct MyCatalog *catalog, ULONG chunk_size)
{
    ULONG consumed = 0;

    while (consumed < chunk_size)
    {
        UBYTE header[8];
        ULONG string_size;
        ULONG padded_size;
        UBYTE *payload;
        STRPTR string;

        if (chunk_size - consumed < 8)
            return FALSE;

        if (!_loc_read_exact(fh, header, sizeof(header)))
            return FALSE;

        consumed += sizeof(header);
        string_size = _loc_be32(&header[4]);
        padded_size = (string_size + 3) & ~3UL;

        if (padded_size > chunk_size - consumed)
            return FALSE;

        payload = AllocVec(padded_size, MEMF_CLEAR);
        if (!payload)
            return FALSE;

        if (!_loc_read_exact(fh, payload, padded_size))
        {
            FreeVec(payload);
            return FALSE;
        }

        consumed += padded_size;
        string = _loc_dup_bytes(payload, string_size);
        FreeVec(payload);
        if (!string)
            return FALSE;

        if (!_loc_add_catalog_entry(catalog, (LONG)_loc_be32(&header[0]), string))
        {
            FreeVec(string);
            return FALSE;
        }
    }

    return consumed == chunk_size;
}

static struct Catalog *_loc_try_open_catalog(CONST_STRPTR path, UWORD required_version)
{
    BPTR fh;
    UBYTE header[12];
    ULONG form_size;
    ULONG remaining;
    struct MyCatalog *catalog;
    LONG ioerr = ERROR_OBJECT_NOT_FOUND;

    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (!fh)
        return NULL;

    if (!_loc_read_exact(fh, header, sizeof(header)))
    {
        ioerr = ERROR_OBJECT_WRONG_TYPE;
        goto fail;
    }

    if (_loc_be32(&header[0]) != MAKE_ID('F', 'O', 'R', 'M') ||
        _loc_be32(&header[8]) != MAKE_ID('C', 'T', 'L', 'G'))
    {
        ioerr = ERROR_OBJECT_WRONG_TYPE;
        goto fail;
    }

    form_size = _loc_be32(&header[4]);
    if (form_size < 4)
    {
        ioerr = ERROR_OBJECT_WRONG_TYPE;
        goto fail;
    }

    catalog = AllocVec(sizeof(*catalog), MEMF_CLEAR);
    if (!catalog)
    {
        ioerr = ERROR_NO_FREE_STORE;
        goto fail;
    }

    catalog->magic = LXA_CATALOG_MAGIC;
    remaining = form_size - 4;

    while (remaining >= 8)
    {
        UBYTE chunk_header[8];
        ULONG chunk_id;
        ULONG chunk_size;
        ULONG padded_chunk_size;

        if (!_loc_read_exact(fh, chunk_header, sizeof(chunk_header)))
        {
            ioerr = ERROR_OBJECT_WRONG_TYPE;
            goto fail_catalog;
        }

        remaining -= 8;
        chunk_id = _loc_be32(&chunk_header[0]);
        chunk_size = _loc_be32(&chunk_header[4]);
        padded_chunk_size = (chunk_size + 1) & ~1UL;

        if (padded_chunk_size > remaining)
        {
            ioerr = ERROR_OBJECT_WRONG_TYPE;
            goto fail_catalog;
        }

        if (chunk_id == MAKE_ID('L', 'A', 'N', 'G') ||
            chunk_id == MAKE_ID('F', 'V', 'E', 'R'))
        {
            UBYTE *payload = AllocVec(padded_chunk_size, MEMF_CLEAR);
            STRPTR text;

            if (!payload)
            {
                ioerr = ERROR_NO_FREE_STORE;
                goto fail_catalog;
            }

            if (!_loc_read_exact(fh, payload, padded_chunk_size))
            {
                FreeVec(payload);
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }

            text = _loc_dup_bytes(payload, chunk_size);
            FreeVec(payload);
            if (!text)
            {
                ioerr = ERROR_NO_FREE_STORE;
                goto fail_catalog;
            }

            if (chunk_id == MAKE_ID('L', 'A', 'N', 'G'))
            {
                if (catalog->cat.cat_Language)
                    FreeVec(catalog->cat.cat_Language);
                catalog->cat.cat_Language = text;
            }
            else
            {
                _loc_parse_version_string(text, &catalog->cat.cat_Version, &catalog->cat.cat_Revision);
                FreeVec(text);
            }
        }
        else if (chunk_id == MAKE_ID('C', 'S', 'E', 'T'))
        {
            UBYTE codeset[4];

            if (chunk_size < 4 || !_loc_read_exact(fh, codeset, sizeof(codeset)))
            {
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }

            catalog->cat.cat_CodeSet = _loc_be32(codeset);
            if (!_loc_skip_bytes(fh, padded_chunk_size - 4))
            {
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }
        }
        else if (chunk_id == MAKE_ID('S', 'T', 'R', 'S'))
        {
            if (!_loc_parse_catalog_strings(fh, catalog, chunk_size))
            {
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }

            if ((chunk_size & 1) && !_loc_skip_bytes(fh, 1))
            {
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }
        }
        else
        {
            if (!_loc_skip_bytes(fh, padded_chunk_size))
            {
                ioerr = ERROR_OBJECT_WRONG_TYPE;
                goto fail_catalog;
            }
        }

        remaining -= padded_chunk_size;
    }

    Close(fh);

    if (required_version != 0 && catalog->cat.cat_Version != required_version)
    {
        ioerr = ERROR_OBJECT_WRONG_TYPE;
        goto fail_catalog_only;
    }

    if (!catalog->cat.cat_Language)
    {
        catalog->cat.cat_Language = _loc_dup_bytes((const UBYTE *)g_DefaultLanguageName,
                                                   _loc_strlen((STRPTR)g_DefaultLanguageName));
        if (!catalog->cat.cat_Language)
        {
            ioerr = ERROR_NO_FREE_STORE;
            goto fail_catalog_only;
        }
    }

    SetIoErr(0);
    return &catalog->cat;

fail_catalog:
    Close(fh);

fail_catalog_only:
    _loc_free_catalog(catalog);
    SetIoErr(ioerr);
    return NULL;

fail:
    Close(fh);
    SetIoErr(ioerr);
    return NULL;
}

static struct Catalog *_loc_open_catalog_for_language(CONST_STRPTR name,
                                                      CONST_STRPTR language,
                                                      UWORD        required_version)
{
    static const char progdir_prefix[] = "PROGDIR:Catalogs/";
    static const char locale_prefix[] = "LOCALE:Catalogs/";
    const char *prefixes[2] = { progdir_prefix, locale_prefix };
    LONG last_error = ERROR_OBJECT_NOT_FOUND;
    int i;

    for (i = 0; i < 2; i++)
    {
        LONG prefix_len = _loc_strlen((STRPTR)prefixes[i]);
        LONG language_len = _loc_strlen(language);
        LONG name_len = _loc_strlen(name);
        LONG path_len = prefix_len + language_len + 1 + name_len + 1;
        STRPTR path = AllocVec(path_len, MEMF_CLEAR);
        struct Catalog *catalog;

        if (!path)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return NULL;
        }

        CopyMem((APTR)prefixes[i], path, prefix_len);
        CopyMem((APTR)language, path + prefix_len, language_len);
        path[prefix_len + language_len] = '/';
        CopyMem((APTR)name, path + prefix_len + language_len + 1, name_len);
        path[path_len - 1] = '\0';

        catalog = _loc_try_open_catalog(path, required_version);
        if (catalog)
        {
            FreeVec(path);
            return catalog;
        }

        last_error = IoErr();
        FreeVec(path);
    }

    SetIoErr(last_error);
    return NULL;
}

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct MyLocaleBase * __g_lxa_locale_InitLib ( register struct MyLocaleBase *locb    __asm("d0"),
                                                register BPTR                seglist __asm("a0"),
                                                register struct ExecBase    *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_locale: InitLib() called\n");
    locb->SegList = seglist;
    locb->lb.lb_SysPatches = FALSE;
    locb->cached_locale = &g_DefaultLocale;
    locb->cached_catalog = NULL;
    locb->cached_locale_name = NULL;
    locb->cached_catalog_name = NULL;
    locb->cached_catalog_language = NULL;
    locb->cached_catalog_version = 0;
    return locb;
}

struct MyLocaleBase * __g_lxa_locale_OpenLib ( register struct MyLocaleBase *LocaleBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_locale: OpenLib() called\n");
    LocaleBase->lb.lb_LibNode.lib_OpenCnt++;
    LocaleBase->lb.lb_LibNode.lib_Flags &= ~LIBF_DELEXP;
    return LocaleBase;
}

BPTR __g_lxa_locale_CloseLib ( register struct MyLocaleBase *locb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_locale: CloseLib() called\n");
    locb->lb.lb_LibNode.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_locale_ExpungeLib ( register struct MyLocaleBase *locb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_locale_ExtFuncLib(void)
{
    return 0;
}

/* Reserved function stub */
static ULONG _locale_Reserved(void) { return 0; }

/****************************************************************************/
/* Main functions                                                            */
/****************************************************************************/

VOID _locale_CloseCatalog ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Catalog      *catalog    __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_locale: CloseCatalog() called\n");

    if (catalog)
    {
        _loc_clear_catalog_cache(LocaleBase, catalog);
        _loc_free_catalog((struct MyCatalog *)catalog);
    }
}

VOID _locale_CloseLocale ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                           register struct Locale       *locale     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_locale: CloseLocale() called\n");
    /* Don't free the default locale */
    if (locale && locale != &g_DefaultLocale)
    {
        if (LocaleBase && LocaleBase->cached_locale == locale)
        {
            LocaleBase->cached_locale = &g_DefaultLocale;
            if (LocaleBase->cached_locale_name)
            {
                FreeVec(LocaleBase->cached_locale_name);
                LocaleBase->cached_locale_name = NULL;
            }
        }

        FreeMem(locale, sizeof(struct Locale));
    }
}

ULONG _locale_ConvToLower ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;

    return _loc_ascii_lower(character);
}

ULONG _locale_ConvToUpper ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;

    return _loc_ascii_upper(character);
}

/****************************************************************************/
/* Helper: call putCharFunc hook with a single character                      */
/****************************************************************************/

static void _loc_putchar(struct Hook *hook, ULONG ch, struct Locale *locale)
{
    /* FormatDate hook convention:
     * A0 = hook, A1 = character (value, not pointer), A2 = locale
     */
    typedef void (*HookFuncPtr)(
        register struct Hook    *hook   __asm("a0"),
        register ULONG           ch     __asm("a1"),
        register struct Locale  *locale __asm("a2")
    );
    ((HookFuncPtr)hook->h_Entry)(hook, ch, locale);
}

static void _loc_putstr(struct Hook *hook, const char *str, struct Locale *locale)
{
    while (*str)
        _loc_putchar(hook, (ULONG)(UBYTE)*str++, locale);
}

/****************************************************************************/
/* Helper: output a number with leading zeros or spaces                      */
/****************************************************************************/

static void _loc_putnumz(struct Hook *hook, LONG value, int width, struct Locale *locale)
{
    char buf[16];
    int i = 0;
    LONG v = value;
    BOOL neg = FALSE;

    if (v < 0)
    {
        neg = TRUE;
        v = -v;
    }

    /* Generate digits in reverse */
    do
    {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    } while (v > 0 && i < 15);

    if (neg)
        buf[i++] = '-';

    /* Pad with leading zeros if needed */
    while (i < width && i < 15)
        buf[i++] = '0';

    /* Output in correct order */
    while (i > 0)
        _loc_putchar(hook, (ULONG)(UBYTE)buf[--i], locale);
}

/* Like _loc_putnumz but pads with spaces instead of zeros */
static void _loc_putnums(struct Hook *hook, LONG value, int width, struct Locale *locale)
{
    char buf[16];
    int i = 0;
    LONG v = value;

    do
    {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    } while (v > 0 && i < 15);

    while (i < width && i < 15)
        buf[i++] = ' ';

    while (i > 0)
        _loc_putchar(hook, (ULONG)(UBYTE)buf[--i], locale);
}

/****************************************************************************/
/* Helper: convert DateStamp to year/month/day/hour/min/sec                  */
/****************************************************************************/

/* Days per month (non-leap and leap) */
static const UBYTE g_DaysInMonth[2][12] = {
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },  /* non-leap */
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }   /* leap */
};

static BOOL _is_leap_year(LONG year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

struct DateParts
{
    LONG year;
    LONG month;   /* 1-12 */
    LONG day;     /* 1-31 */
    LONG hour;    /* 0-23 */
    LONG min;     /* 0-59 */
    LONG sec;     /* 0-59 */
    LONG wday;    /* 0=Sunday, 1=Monday, ... 6=Saturday */
    LONG yday;    /* 1-366 */
};

static void _datestamp_to_parts(const struct DateStamp *ds, struct DateParts *dp)
{
    LONG days = ds->ds_Days;
    LONG minutes = ds->ds_Minute;
    LONG ticks = ds->ds_Tick;

    /* Time */
    dp->hour = minutes / 60;
    dp->min = minutes % 60;
    dp->sec = ticks / TICKS_PER_SECOND;

    /* Day of week: Jan 1, 1978 was a Sunday (wday=0) */
    dp->wday = (days + 0) % 7;  /* 0=Sun */
    if (dp->wday < 0) dp->wday += 7;

    /* Convert days since 1978-01-01 to year/month/day */
    LONG year = 1978;
    while (days >= (_is_leap_year(year) ? 366 : 365))
    {
        days -= (_is_leap_year(year) ? 366 : 365);
        year++;
    }

    dp->year = year;
    dp->yday = days + 1;  /* 1-based */

    BOOL leap = _is_leap_year(year);
    LONG month = 0;
    while (month < 12 && days >= g_DaysInMonth[leap ? 1 : 0][month])
    {
        days -= g_DaysInMonth[leap ? 1 : 0][month];
        month++;
    }

    dp->month = month + 1;  /* 1-based */
    dp->day = days + 1;     /* 1-based */
}

/****************************************************************************/
/* FormatDate implementation                                                 */
/****************************************************************************/

VOID _locale_FormatDate ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                          register struct Locale       *locale       __asm("a0"),
                          register CONST_STRPTR         fmtTemplate  __asm("a1"),
                          register CONST struct DateStamp *date      __asm("a2"),
                          register struct Hook         *putCharFunc  __asm("a3"))
{
    struct DateParts dp;
    const UBYTE *fmt;

    DPRINTF (LOG_DEBUG, "_locale: FormatDate() called\n");

    if (!putCharFunc)
        return;

    if (!locale)
        locale = &g_DefaultLocale;

    if (!fmtTemplate)
    {
        _loc_putchar(putCharFunc, 0, locale);
        return;
    }

    if (!date)
    {
        _loc_putchar(putCharFunc, 0, locale);
        return;
    }

    _datestamp_to_parts(date, &dp);

    fmt = (const UBYTE *)fmtTemplate;
    while (*fmt)
    {
        if (*fmt != '%')
        {
            _loc_putchar(putCharFunc, (ULONG)*fmt, locale);
            fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        switch (*fmt)
        {
            case '%':
                _loc_putchar(putCharFunc, '%', locale);
                break;
            case 'a':
                /* abbreviated weekday name */
                _loc_putstr(putCharFunc, g_LocaleStrings[ABDAY_1 + dp.wday], locale);
                break;
            case 'A':
                /* full weekday name */
                _loc_putstr(putCharFunc, g_LocaleStrings[DAY_1 + dp.wday], locale);
                break;
            case 'b':
            case 'h':
                /* abbreviated month name */
                _loc_putstr(putCharFunc, g_LocaleStrings[ABMON_1 + dp.month - 1], locale);
                break;
            case 'B':
                /* full month name */
                _loc_putstr(putCharFunc, g_LocaleStrings[MON_1 + dp.month - 1], locale);
                break;
            case 'c':
                /* %a %b %d %H:%M:%S %Y */
                _loc_putstr(putCharFunc, g_LocaleStrings[ABDAY_1 + dp.wday], locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putstr(putCharFunc, g_LocaleStrings[ABMON_1 + dp.month - 1], locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnumz(putCharFunc, dp.day, 2, locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnumz(putCharFunc, dp.year, 4, locale);
                break;
            case 'C':
                /* %a %b %e %T %Z %Y - we output without timezone */
                _loc_putstr(putCharFunc, g_LocaleStrings[ABDAY_1 + dp.wday], locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putstr(putCharFunc, g_LocaleStrings[ABMON_1 + dp.month - 1], locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnums(putCharFunc, dp.day, 2, locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putstr(putCharFunc, "UTC", locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putnumz(putCharFunc, dp.year, 4, locale);
                break;
            case 'd':
                /* day of month with leading zero */
                _loc_putnumz(putCharFunc, dp.day, 2, locale);
                break;
            case 'D':
                /* %m/%d/%y */
                _loc_putnumz(putCharFunc, dp.month, 2, locale);
                _loc_putchar(putCharFunc, '/', locale);
                _loc_putnumz(putCharFunc, dp.day, 2, locale);
                _loc_putchar(putCharFunc, '/', locale);
                _loc_putnumz(putCharFunc, dp.year % 100, 2, locale);
                break;
            case 'e':
                /* day of month with leading space */
                _loc_putnums(putCharFunc, dp.day, 2, locale);
                break;
            case 'H':
                /* hour 24-hour with leading zero */
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                break;
            case 'I':
            {
                /* hour 12-hour with leading zero */
                LONG h12 = dp.hour % 12;
                if (h12 == 0) h12 = 12;
                _loc_putnumz(putCharFunc, h12, 2, locale);
                break;
            }
            case 'j':
                /* julian day (day of year) */
                _loc_putnumz(putCharFunc, dp.yday, 3, locale);
                break;
            case 'm':
                /* month number with leading zero */
                _loc_putnumz(putCharFunc, dp.month, 2, locale);
                break;
            case 'M':
                /* minutes with leading zero */
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                break;
            case 'n':
                _loc_putchar(putCharFunc, '\n', locale);
                break;
            case 'p':
                /* AM/PM string */
                _loc_putstr(putCharFunc, g_LocaleStrings[dp.hour < 12 ? AM_STR : PM_STR], locale);
                break;
            case 'q':
                /* hour 24-hour without leading zero (just the number) */
                _loc_putnumz(putCharFunc, dp.hour, 1, locale);
                break;
            case 'Q':
            {
                /* hour 12-hour without leading zero */
                LONG h12 = dp.hour % 12;
                if (h12 == 0) h12 = 12;
                _loc_putnumz(putCharFunc, h12, 1, locale);
                break;
            }
            case 'r':
                /* %I:%M:%S %p */
            {
                LONG h12 = dp.hour % 12;
                if (h12 == 0) h12 = 12;
                _loc_putnumz(putCharFunc, h12, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                _loc_putchar(putCharFunc, ' ', locale);
                _loc_putstr(putCharFunc, g_LocaleStrings[dp.hour < 12 ? AM_STR : PM_STR], locale);
                break;
            }
            case 'R':
                /* %H:%M */
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                break;
            case 'S':
                /* seconds with leading zero */
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                break;
            case 't':
                _loc_putchar(putCharFunc, '\t', locale);
                break;
            case 'T':
                /* %H:%M:%S */
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                break;
            case 'U':
            {
                /* Week number (Sunday as first day) */
                LONG wn = (dp.yday - 1 + ((7 - dp.wday) % 7)) / 7;
                _loc_putnumz(putCharFunc, wn, 2, locale);
                break;
            }
            case 'w':
                /* weekday number (0=Sunday) */
                _loc_putnumz(putCharFunc, dp.wday, 1, locale);
                break;
            case 'W':
            {
                /* Week number (Monday as first day) */
                LONG mwday = (dp.wday + 6) % 7;  /* 0=Monday */
                LONG wn = (dp.yday - 1 + ((7 - mwday) % 7)) / 7;
                _loc_putnumz(putCharFunc, wn, 2, locale);
                break;
            }
            case 'x':
                /* %m/%d/%y */
                _loc_putnumz(putCharFunc, dp.month, 2, locale);
                _loc_putchar(putCharFunc, '/', locale);
                _loc_putnumz(putCharFunc, dp.day, 2, locale);
                _loc_putchar(putCharFunc, '/', locale);
                _loc_putnumz(putCharFunc, dp.year % 100, 2, locale);
                break;
            case 'X':
                /* %H:%M:%S */
                _loc_putnumz(putCharFunc, dp.hour, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.min, 2, locale);
                _loc_putchar(putCharFunc, ':', locale);
                _loc_putnumz(putCharFunc, dp.sec, 2, locale);
                break;
            case 'y':
                /* 2-digit year with leading zero */
                _loc_putnumz(putCharFunc, dp.year % 100, 2, locale);
                break;
            case 'Y':
                /* 4-digit year */
                _loc_putnumz(putCharFunc, dp.year, 4, locale);
                break;
            case '\0':
                /* % at end of string - just output % and stop */
                _loc_putchar(putCharFunc, '%', locale);
                goto done;
            default:
                /* Unknown - output literal */
                _loc_putchar(putCharFunc, '%', locale);
                _loc_putchar(putCharFunc, (ULONG)*fmt, locale);
                break;
        }
        fmt++;
    }
done:
    /* Output the terminating null */
    _loc_putchar(putCharFunc, 0, locale);
}

/****************************************************************************/
/* FormatString implementation                                               */
/****************************************************************************/

APTR _locale_FormatString ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                            register struct Locale       *locale       __asm("a0"),
                            register CONST_STRPTR         fmtTemplate  __asm("a1"),
                            register APTR                 dataStream   __asm("a2"),
                            register struct Hook         *putCharFunc  __asm("a3"))
{
    const UBYTE *fmt;
    UWORD *args;
    UWORD *max_args;

    DPRINTF (LOG_DEBUG, "_locale: FormatString() called\n");

    if (!putCharFunc)
        return dataStream;

    if (!locale)
        locale = &g_DefaultLocale;

    if (!fmtTemplate)
        return dataStream;

    fmt = (const UBYTE *)fmtTemplate;
    args = (UWORD *)dataStream;
    max_args = args;  /* Track the highest arg pointer for return value */

    while (*fmt)
    {
        if (*fmt != '%')
        {
            _loc_putchar(putCharFunc, (ULONG)*fmt, locale);
            fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        if (*fmt == '%')
        {
            _loc_putchar(putCharFunc, '%', locale);
            fmt++;
            continue;
        }

        /* Check for positional argument: N$ */
        UWORD *cur_args = args;
        BOOL positional = FALSE;
        {
            const UBYTE *look = fmt;
            LONG pos = 0;
            while (*look >= '0' && *look <= '9')
            {
                pos = pos * 10 + (*look - '0');
                look++;
            }
            if (*look == '$' && pos > 0)
            {
                /* Positional argument: rewind to dataStream base + offset */
                cur_args = (UWORD *)dataStream;
                /* Skip to the pos-th argument. We need to scan the format
                 * string to know sizes, but for simplicity we'll just
                 * index by position. Each arg is at least a WORD. We use
                 * a fixed-size arg table approach. */
                positional = TRUE;

                /* Walk the format string to compute offsets for each arg position.
                 * For simplicity, skip forward pos-1 arguments using the format
                 * string. Since this gets complex, we'll use a simpler approach:
                 * just skip (pos-1) * 2 bytes (WORD each) for non-pointer types.
                 * However, this is not accurate for mixed types. Real Amiga
                 * locale.library stores arg offsets in a table.
                 *
                 * For a basic but functional implementation, we re-scan from the
                 * start of the format string and count arguments up to position
                 * pos-1 to compute the byte offset.
                 */
                const UBYTE *scan = (const UBYTE *)fmtTemplate;
                LONG arg_byte_offsets[128];
                LONG arg_count = 0;
                LONG byte_offset = 0;

                /* Build a table of byte offsets for each sequential argument */
                while (*scan && arg_count < 128)
                {
                    if (*scan != '%')
                    {
                        scan++;
                        continue;
                    }
                    scan++;
                    if (*scan == '%')
                    {
                        scan++;
                        continue;
                    }

                    /* Skip positional spec if present */
                    const UBYTE *spec_start = scan;
                    {
                        const UBYTE *t = scan;
                        LONG n = 0;
                        while (*t >= '0' && *t <= '9')
                        {
                            n = n * 10 + (*t - '0');
                            t++;
                        }
                        if (*t == '$' && n > 0)
                            scan = t + 1;
                        else
                            scan = spec_start;
                    }

                    /* Skip flags */
                    while (*scan == '-' || *scan == '0')
                        scan++;

                    /* Skip width */
                    while (*scan >= '0' && *scan <= '9')
                        scan++;

                    /* Skip precision */
                    if (*scan == '.')
                    {
                        scan++;
                        while (*scan >= '0' && *scan <= '9')
                            scan++;
                    }

                    /* Check for 'l' modifier */
                    BOOL is_long = FALSE;
                    if (*scan == 'l')
                    {
                        is_long = TRUE;
                        scan++;
                    }

                    /* Record offset for this argument position */
                    arg_byte_offsets[arg_count] = byte_offset;
                    arg_count++;

                    /* Compute size consumed by this argument */
                    switch (*scan)
                    {
                        case 's':
                        case 'b':
                            byte_offset += 4;  /* always 32-bit pointer */
                            break;
                        case 'd': case 'D':
                        case 'u': case 'U':
                        case 'x': case 'X':
                            byte_offset += is_long ? 4 : 2;
                            break;
                        case 'c':
                            byte_offset += is_long ? 4 : 2;
                            break;
                        default:
                            byte_offset += 2;
                            break;
                    }
                    if (*scan)
                        scan++;
                }

                if (pos >= 1 && pos <= arg_count)
                    cur_args = (UWORD *)((UBYTE *)dataStream + arg_byte_offsets[pos - 1]);
                else
                    cur_args = args;

                fmt = look + 1;  /* skip past N$ */
            }
        }

        /* Parse flags */
        BOOL left_align = FALSE;
        BOOL zero_pad = FALSE;

        while (*fmt == '-' || *fmt == '0')
        {
            if (*fmt == '-') left_align = TRUE;
            if (*fmt == '0' && !left_align) zero_pad = TRUE;
            fmt++;
        }

        /* Parse width */
        LONG width = 0;
        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse limit (precision for strings) */
        LONG limit = -1;
        if (*fmt == '.')
        {
            fmt++;
            limit = 0;
            while (*fmt >= '0' && *fmt <= '9')
            {
                limit = limit * 10 + (*fmt - '0');
                fmt++;
            }
        }

        /* Parse length modifier */
        BOOL is_long = FALSE;
        if (*fmt == 'l')
        {
            is_long = TRUE;
            fmt++;
        }

        /* Parse type */
        char type = *fmt;
        if (*fmt)
            fmt++;

        switch (type)
        {
            case 'd':
            case 'D':
            {
                /* Signed decimal */
                LONG value;
                if (is_long)
                {
                    value = *(LONG *)cur_args;
                    cur_args += 2;
                }
                else
                {
                    value = (WORD)*cur_args++;
                }

                char buf[12];
                int bi = 0;
                BOOL neg = FALSE;
                ULONG uval;

                if (value < 0)
                {
                    neg = TRUE;
                    uval = (ULONG)(-value);
                }
                else
                {
                    uval = (ULONG)value;
                }

                /* Generate digits in reverse */
                do
                {
                    buf[bi++] = '0' + (uval % 10);
                    uval /= 10;
                } while (uval && bi < 11);

                if (neg)
                    buf[bi++] = '-';

                LONG len = bi;
                char pad_char = zero_pad ? '0' : ' ';

                if (!left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, (ULONG)(UBYTE)pad_char, locale);
                }

                while (bi > 0)
                    _loc_putchar(putCharFunc, (ULONG)(UBYTE)buf[--bi], locale);

                if (left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            case 'u':
            case 'U':
            {
                /* Unsigned decimal */
                ULONG value;
                if (is_long)
                {
                    value = *(ULONG *)cur_args;
                    cur_args += 2;
                }
                else
                {
                    value = *cur_args++;
                }

                char buf[11];
                int bi = 0;

                do
                {
                    buf[bi++] = '0' + (value % 10);
                    value /= 10;
                } while (value && bi < 10);

                LONG len = bi;
                char pad_char = zero_pad ? '0' : ' ';

                if (!left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, (ULONG)(UBYTE)pad_char, locale);
                }

                while (bi > 0)
                    _loc_putchar(putCharFunc, (ULONG)(UBYTE)buf[--bi], locale);

                if (left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            case 'x':
            case 'X':
            {
                /* Hex: %x = uppercase, %X = lowercase (Amiga convention, swapped from C) */
                ULONG value;
                if (is_long)
                {
                    value = *(ULONG *)cur_args;
                    cur_args += 2;
                }
                else
                {
                    value = *cur_args++;
                }

                const char *hex_digits = (type == 'x') ? "0123456789ABCDEF" : "0123456789abcdef";

                char buf[9];
                int bi = 0;

                do
                {
                    buf[bi++] = hex_digits[value & 0xF];
                    value >>= 4;
                } while (value && bi < 8);

                LONG len = bi;
                char pad_char = zero_pad ? '0' : ' ';

                if (!left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, (ULONG)(UBYTE)pad_char, locale);
                }

                while (bi > 0)
                    _loc_putchar(putCharFunc, (ULONG)(UBYTE)buf[--bi], locale);

                if (left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            case 's':
            {
                /* String (32-bit pointer, always) */
                char *str = (char *)*(ULONG *)cur_args;
                cur_args += 2;

                if (!str)
                    str = "";

                /* Calculate length respecting limit */
                LONG len = 0;
                {
                    const char *s = str;
                    while (*s && (limit < 0 || len < limit))
                    {
                        len++;
                        s++;
                    }
                }

                if (!left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                {
                    const char *s = str;
                    LONG printed = 0;
                    while (*s && (limit < 0 || printed < limit))
                    {
                        _loc_putchar(putCharFunc, (ULONG)(UBYTE)*s++, locale);
                        printed++;
                    }
                }

                if (left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            case 'b':
            {
                /* BSTR (32-bit BPTR) */
                BPTR bstr = *(BPTR *)cur_args;
                cur_args += 2;

                UBYTE *str = (UBYTE *)BADDR(bstr);
                LONG len = 0;

                if (str)
                {
                    len = str[0];
                    str++;
                }

                if (limit >= 0 && len > limit)
                    len = limit;

                if (!left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                {
                    LONG i;
                    for (i = 0; i < len; i++)
                        _loc_putchar(putCharFunc, (ULONG)str[i], locale);
                }

                if (left_align)
                {
                    LONG i;
                    for (i = len; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            case 'c':
            {
                /* Character */
                ULONG ch;
                if (is_long)
                {
                    ch = *(ULONG *)cur_args;
                    cur_args += 2;
                }
                else
                {
                    ch = *cur_args++;
                }

                if (!left_align)
                {
                    LONG i;
                    for (i = 1; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                _loc_putchar(putCharFunc, ch & 0xFF, locale);

                if (left_align)
                {
                    LONG i;
                    for (i = 1; i < width; i++)
                        _loc_putchar(putCharFunc, ' ', locale);
                }

                if (!positional)
                    args = cur_args;
                break;
            }

            default:
                /* Unknown format - output literally */
                _loc_putchar(putCharFunc, '%', locale);
                _loc_putchar(putCharFunc, (ULONG)(UBYTE)type, locale);
                break;
        }

        /* Track maximum args pointer for return value */
        if (cur_args > max_args)
            max_args = cur_args;
        if (args > max_args)
            max_args = args;
    }

    /* Output terminating NUL */
    _loc_putchar(putCharFunc, 0, locale);

    return (APTR)max_args;
}

STRPTR _locale_GetCatalogStr ( register struct MyLocaleBase   *LocaleBase    __asm("a6"),
                               register CONST struct Catalog  *catalog       __asm("a0"),
                               register LONG                   stringNum     __asm("d0"),
                               register CONST_STRPTR           defaultString __asm("a1"))
{
    const struct MyCatalog *my_catalog = (const struct MyCatalog *)catalog;
    struct CatalogEntry *entry;

    DPRINTF (LOG_DEBUG, "_locale: GetCatalogStr() called stringNum=%ld\n", stringNum);

    (void)LocaleBase;

    if (!catalog || my_catalog->magic != LXA_CATALOG_MAGIC)
        return (STRPTR)defaultString;

    if (stringNum >= 0)
    {
        entry = my_catalog->entries;

        if (LocaleBase &&
            _loc_catalog_cache_matches(LocaleBase,
                                       LocaleBase->cached_catalog_name,
                                       LocaleBase->cached_catalog_language,
                                       LocaleBase->cached_catalog_version) &&
            LocaleBase->cached_catalog == catalog &&
            entry && entry->id == stringNum)
        {
            return entry->string;
        }

        while (entry)
        {
            if (entry->id == stringNum)
            {
                if (LocaleBase && my_catalog->entries != entry)
                {
                    struct CatalogEntry *prev = my_catalog->entries;

                    while (prev && prev->next != entry)
                        prev = prev->next;

                    if (prev)
                    {
                        prev->next = entry->next;
                        entry->next = my_catalog->entries;
                        ((struct MyCatalog *)my_catalog)->entries = entry;
                    }
                }

                return entry->string;
            }

            entry = entry->next;
        }
    }

    return (STRPTR)defaultString;
}

STRPTR _locale_GetLocaleStr ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                              register struct Locale       *locale     __asm("a0"),
                              register ULONG                stringNum  __asm("d0"))
{
    struct Locale *effective_locale = locale;

    DPRINTF (LOG_DEBUG, "_locale: GetLocaleStr() called stringNum=%ld\n", stringNum);

    if (LocaleBase && LocaleBase->cached_locale)
        effective_locale = LocaleBase->cached_locale;

    (void)effective_locale;
    
    if (stringNum < sizeof(g_LocaleStrings) / sizeof(g_LocaleStrings[0]))
    {
        const char *str = g_LocaleStrings[stringNum];
        if (str)
            return (STRPTR)str;
    }
    return (STRPTR)"";
}

/* Character classification functions */
BOOL _locale_IsAlNum ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}

BOOL _locale_IsAlpha ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z');
}

BOOL _locale_IsCntrl ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character < 32 || character == 127;
}

BOOL _locale_IsDigit ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character >= '0' && character <= '9';
}

BOOL _locale_IsGraph ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character > 32 && character < 127;
}

BOOL _locale_IsLower ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character >= 'a' && character <= 'z';
}

BOOL _locale_IsPrint ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character >= 32 && character < 127;
}

BOOL _locale_IsPunct ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return (character >= 33 && character <= 47) ||
           (character >= 58 && character <= 64) ||
           (character >= 91 && character <= 96) ||
           (character >= 123 && character <= 126);
}

BOOL _locale_IsSpace ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character == ' ' || character == '\t' || character == '\n' ||
           character == '\r' || character == '\f' || character == '\v';
}

BOOL _locale_IsUpper ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    return character >= 'A' && character <= 'Z';
}

BOOL _locale_IsXDigit ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                        register struct Locale       *locale     __asm("a0"),
                        register ULONG                character  __asm("d0"))
{
    return (character >= '0' && character <= '9') ||
           (character >= 'A' && character <= 'F') ||
           (character >= 'a' && character <= 'f');
}

struct Catalog * _locale_OpenCatalogA ( register struct MyLocaleBase    *LocaleBase __asm("a6"),
                                        register struct Locale          *locale     __asm("a0"),
                                        register CONST_STRPTR            name       __asm("a1"),
                                        register CONST struct TagItem   *tags       __asm("a2"))
{
    CONST_STRPTR requested_language;
    CONST_STRPTR builtin_language = (CONST_STRPTR)g_DefaultLanguageName;
    UWORD required_version = 0;
    struct Catalog *catalog;

    DPRINTF (LOG_DEBUG, "_locale: OpenCatalogA() called name='%s'\n", name ? (char *)name : "(null)");

    (void)LocaleBase;

    if (!name || !*name)
    {
        SetIoErr(ERROR_REQUIRED_ARG_MISSING);
        return NULL;
    }

    if (!locale)
        locale = &g_DefaultLocale;

    requested_language = locale->loc_LanguageName ? locale->loc_LanguageName : (STRPTR)g_DefaultLanguageName;

    if (tags)
    {
        builtin_language = (CONST_STRPTR)GetTagData(OC_BuiltInLanguage, (ULONG)builtin_language, (struct TagItem *)tags);
        requested_language = (CONST_STRPTR)GetTagData(OC_Language, (ULONG)requested_language, (struct TagItem *)tags);
        required_version = (UWORD)GetTagData(OC_Version, 0, (struct TagItem *)tags);
    }

    if (builtin_language && _loc_strieq_ascii(requested_language, builtin_language))
    {
        SetIoErr(0);
        return NULL;
    }

    if (_loc_catalog_cache_matches(LocaleBase, name, requested_language, required_version))
    {
        SetIoErr(0);
        return LocaleBase->cached_catalog;
    }

    catalog = _loc_open_catalog_for_language(name, requested_language, required_version);
    if (catalog)
        _loc_cache_catalog(LocaleBase, name, requested_language, required_version, catalog);

    return catalog;
}

struct Locale * _locale_OpenLocale ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                                     register CONST_STRPTR         name       __asm("a0"))
{
    CONST_STRPTR effective_name;

    DPRINTF (LOG_DEBUG, "_locale: OpenLocale() called name='%s'\n", name ? (char *)name : "(null)");

    effective_name = _loc_effective_locale_name(name);

    if (LocaleBase &&
        LocaleBase->cached_locale &&
        LocaleBase->cached_locale_name &&
        _loc_strieq_ascii(LocaleBase->cached_locale_name, effective_name))
    {
        SetIoErr(0);
        return LocaleBase->cached_locale;
    }

    if (_loc_is_default_locale_name(name))
    {
        if (LocaleBase)
        {
            if (LocaleBase->cached_locale_name)
                FreeVec(LocaleBase->cached_locale_name);
            LocaleBase->cached_locale_name = _loc_dup_bytes((const UBYTE *)effective_name,
                                                            _loc_strlen(effective_name));
            LocaleBase->cached_locale = &g_DefaultLocale;
        }

        SetIoErr(0);
        return &g_DefaultLocale;
    }

    SetIoErr(ERROR_OBJECT_NOT_FOUND);
    return NULL;
}

/****************************************************************************/
/* ParseDate implementation                                                  */
/****************************************************************************/

/* Helper: call getCharFunc hook to get one character */
static ULONG _loc_getchar(struct Hook *hook, struct Locale *locale)
{
    /* ParseDate hook convention:
     * A0 = hook, A1 = locale, A2 = NULL
     * Returns character in D0
     */
    typedef ULONG (*GetCharFuncPtr)(
        register struct Hook   *hook   __asm("a0"),
        register struct Locale *locale __asm("a1"),
        register APTR           dummy  __asm("a2")
    );
    return ((GetCharFuncPtr)hook->h_Entry)(hook, locale, NULL);
}

/* Helper: parse a decimal number from the character stream */
static BOOL _loc_parse_num(struct Hook *hook, struct Locale *locale,
                           LONG *result, int min_digits, int max_digits,
                           ULONG *next_ch)
{
    LONG value = 0;
    int digits = 0;
    ULONG ch = *next_ch;

    /* Skip leading spaces */
    while (ch == ' ')
        ch = _loc_getchar(hook, locale);

    while (ch >= '0' && ch <= '9' && digits < max_digits)
    {
        value = value * 10 + (ch - '0');
        digits++;
        ch = _loc_getchar(hook, locale);
    }

    *next_ch = ch;

    if (digits < min_digits)
        return FALSE;

    *result = value;
    return TRUE;
}

/* Helper: case-insensitive prefix match against getchar stream */
static BOOL _loc_match_str(struct Hook *hook, struct Locale *locale,
                           const char *str, ULONG *next_ch)
{
    ULONG ch = *next_ch;

    while (*str)
    {
        ULONG s = (ULONG)(UBYTE)*str;
        ULONG c = ch;

        /* Case-insensitive */
        if (s >= 'A' && s <= 'Z') s += 'a' - 'A';
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

        if (s != c)
            return FALSE;

        str++;
        ch = _loc_getchar(hook, locale);
    }

    *next_ch = ch;
    return TRUE;
}

/* Helper: convert date parts back to a DateStamp */
static void _parts_to_datestamp(const struct DateParts *dp, struct DateStamp *ds)
{
    LONG days = 0;
    LONG y;

    /* Count days from 1978 to dp->year */
    for (y = 1978; y < dp->year; y++)
        days += _is_leap_year(y) ? 366 : 365;

    /* Add days in months */
    {
        BOOL leap = _is_leap_year(dp->year);
        LONG m;
        for (m = 0; m < dp->month - 1; m++)
            days += g_DaysInMonth[leap ? 1 : 0][m];
    }

    /* Add day of month (1-based) */
    days += dp->day - 1;

    ds->ds_Days = days;
    ds->ds_Minute = dp->hour * 60 + dp->min;
    ds->ds_Tick = dp->sec * TICKS_PER_SECOND;
}

BOOL _locale_ParseDate ( register struct MyLocaleBase    *LocaleBase  __asm("a6"),
                         register CONST struct Locale    *locale      __asm("a0"),
                         register struct DateStamp       *date        __asm("a1"),
                         register CONST_STRPTR            fmtTemplate __asm("a2"),
                         register struct Hook            *getCharFunc __asm("a3"))
{
    struct DateParts dp;
    const UBYTE *fmt;
    ULONG ch;

    DPRINTF (LOG_DEBUG, "_locale: ParseDate() called\n");

    if (!getCharFunc || !fmtTemplate)
        return FALSE;

    if (!locale)
        locale = (struct Locale *)&g_DefaultLocale;

    /* Initialize date parts to defaults */
    dp.year = 1978;
    dp.month = 1;
    dp.day = 1;
    dp.hour = 0;
    dp.min = 0;
    dp.sec = 0;
    dp.wday = 0;
    dp.yday = 1;

    fmt = (const UBYTE *)fmtTemplate;
    ch = _loc_getchar(getCharFunc, (struct Locale *)locale);

    while (*fmt)
    {
        if (*fmt != '%')
        {
            /* Literal character must match */
            if (*fmt == ' ')
            {
                /* Space in template matches any amount of whitespace */
                while (ch == ' ' || ch == '\t')
                    ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                fmt++;
                continue;
            }

            if ((ULONG)(UBYTE)*fmt != ch)
                return FALSE;

            ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
            fmt++;
            continue;
        }

        fmt++; /* skip '%' */

        switch (*fmt)
        {
            case 'd':
            case 'e':
            {
                /* Day of month */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 1 || val > 31)
                    return FALSE;
                dp.day = val;
                break;
            }

            case 'm':
            {
                /* Month number */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 1 || val > 12)
                    return FALSE;
                dp.month = val;
                break;
            }

            case 'y':
            {
                /* 2-digit year */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 2, 2, &ch))
                    return FALSE;
                dp.year = (val >= 78) ? 1900 + val : 2000 + val;
                break;
            }

            case 'Y':
            {
                /* 4-digit year */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 4, 4, &ch))
                    return FALSE;
                dp.year = val;
                break;
            }

            case 'H':
            {
                /* 24-hour hour */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 0 || val > 23)
                    return FALSE;
                dp.hour = val;
                break;
            }

            case 'I':
            {
                /* 12-hour hour */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 1 || val > 12)
                    return FALSE;
                /* Will be adjusted by %p AM/PM if present */
                dp.hour = (val == 12) ? 0 : val;
                break;
            }

            case 'M':
            {
                /* Minutes */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 0 || val > 59)
                    return FALSE;
                dp.min = val;
                break;
            }

            case 'S':
            {
                /* Seconds */
                LONG val;
                if (!_loc_parse_num(getCharFunc, (struct Locale *)locale, &val, 1, 2, &ch))
                    return FALSE;
                if (val < 0 || val > 59)
                    return FALSE;
                dp.sec = val;
                break;
            }

            case 'p':
            {
                /* AM/PM */
                ULONG c1 = ch;
                ULONG c2 = _loc_getchar(getCharFunc, (struct Locale *)locale);

                if ((c1 == 'P' || c1 == 'p') && (c2 == 'M' || c2 == 'm'))
                {
                    if (dp.hour < 12)
                        dp.hour += 12;
                }
                else if ((c1 == 'A' || c1 == 'a') && (c2 == 'M' || c2 == 'm'))
                {
                    /* AM - hour stays as-is (already 0-11 from %I) */
                }
                else
                {
                    return FALSE;
                }
                ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                break;
            }

            case 'a':
            {
                /* Abbreviated weekday - try to match, result is ignored */
                BOOL matched = FALSE;
                LONG i;
                for (i = 0; i < 7 && !matched; i++)
                {
                    ULONG saved_ch = ch;
                    if (_loc_match_str(getCharFunc, (struct Locale *)locale,
                                       g_LocaleStrings[ABDAY_1 + i], &ch))
                    {
                        matched = TRUE;
                        dp.wday = i;
                    }
                    else
                    {
                        /* Can't un-read characters easily, so for abbreviated
                         * weekday matching we rely on the first try working.
                         * This is a limitation of the hook-based approach. */
                        ch = saved_ch;
                    }
                }
                /* If no match, just skip some letters */
                if (!matched)
                {
                    while (ch >= 'A' && ch <= 'z')
                        ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                }
                break;
            }

            case 'A':
            {
                /* Full weekday - skip letters */
                while ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
                    ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                break;
            }

            case 'b':
            case 'h':
            {
                /* Abbreviated month name - try matching */
                BOOL matched = FALSE;
                LONG i;
                for (i = 0; i < 12 && !matched; i++)
                {
                    if (_loc_match_str(getCharFunc, (struct Locale *)locale,
                                       g_LocaleStrings[ABMON_1 + i], &ch))
                    {
                        matched = TRUE;
                        dp.month = i + 1;
                    }
                }
                if (!matched)
                {
                    /* Skip letters */
                    while ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
                        ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                }
                break;
            }

            case 'B':
            {
                /* Full month name - try matching */
                BOOL matched = FALSE;
                LONG i;
                for (i = 0; i < 12 && !matched; i++)
                {
                    if (_loc_match_str(getCharFunc, (struct Locale *)locale,
                                       g_LocaleStrings[MON_1 + i], &ch))
                    {
                        matched = TRUE;
                        dp.month = i + 1;
                    }
                }
                if (!matched)
                {
                    while ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
                        ch = _loc_getchar(getCharFunc, (struct Locale *)locale);
                }
                break;
            }

            case '\0':
                goto done;

            default:
                /* Unknown format code - skip */
                break;
        }

        if (*fmt)
            fmt++;
    }

done:
    /* Store result if date pointer was provided */
    if (date)
        _parts_to_datestamp(&dp, date);

    return TRUE;
}

ULONG _locale_StrConvert ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                           register struct Locale       *locale     __asm("a0"),
                           register CONST_STRPTR         string     __asm("a1"),
                           register APTR                 buffer     __asm("a2"),
                           register ULONG                bufferSize __asm("d0"),
                           register ULONG                type       __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_locale: StrConvert() called\n");

    (void)LocaleBase;
    (void)locale;

    switch (type)
    {
        case SC_ASCII:
        case SC_COLLATE1:
        case SC_COLLATE2:
            return _loc_strconvert_ascii(string, buffer, bufferSize, type);
        default:
            if (buffer && bufferSize > 0)
                ((UBYTE *)buffer)[0] = '\0';
            return 0;
    }
}

LONG _locale_StrnCmp ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register CONST_STRPTR         string1    __asm("a1"),
                       register CONST_STRPTR         string2    __asm("a2"),
                       register LONG                 length     __asm("d0"),
                       register ULONG                type       __asm("d1"))
{
    LONG result;

    (void)LocaleBase;
    (void)locale;

    if (!string1 && !string2)
        return 0;
    if (!string1)
        return -1;
    if (!string2)
        return 1;

    switch (type)
    {
        case SC_ASCII:
        case SC_COLLATE1:
            return _loc_compare_folded(string1, string2, length);

        case SC_COLLATE2:
            result = _loc_compare_folded(string1, string2, length);
            if (result != 0)
                return result;
            return _loc_compare_secondary(string1, string2, length);

        default:
            if (length == 0)
                return 0;

            if (length < 0)
                return _loc_strcmp(string1, string2);

            while (*string1 && *string2 && length-- > 0)
            {
                LONG c1 = (LONG)(UBYTE)*string1++;
                LONG c2 = (LONG)(UBYTE)*string2++;

                if (c1 != c2)
                    return c1 - c2;
            }

            if (length == 0)
                return 0;

            return (LONG)(UBYTE)*string1 - (LONG)(UBYTE)*string2;
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

extern APTR              __g_lxa_locale_FuncTab [];
extern struct MyDataInit __g_lxa_locale_DataTab;
extern struct InitTable  __g_lxa_locale_InitTab;
extern APTR              __g_lxa_locale_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                    // UWORD rt_MatchWord
    &ROMTag,                          // struct Resident *rt_MatchTag
    &__g_lxa_locale_EndResident,      // APTR  rt_EndSkip
    RTF_AUTOINIT,                     // UBYTE rt_Flags
    VERSION,                          // UBYTE rt_Version
    NT_LIBRARY,                       // UBYTE rt_Type
    0,                                // BYTE  rt_Pri
    &_g_locale_ExLibName[0],          // char  *rt_Name
    &_g_locale_ExLibID[0],            // char  *rt_IdString
    &__g_lxa_locale_InitTab           // APTR  rt_Init
};

APTR __g_lxa_locale_EndResident;
struct Resident *__lxa_locale_ROMTag = &ROMTag;

struct InitTable __g_lxa_locale_InitTab =
{
    (ULONG)               sizeof(struct MyLocaleBase),
    (APTR              *) &__g_lxa_locale_FuncTab[0],
    (APTR)                &__g_lxa_locale_DataTab,
    (APTR)                __g_lxa_locale_InitLib
};

/* Function table - offsets from locale_pragmas.h 
 * First real function (CloseCatalog) is at 0x24 = 36
 */
APTR __g_lxa_locale_FuncTab [] =
{
    __g_lxa_locale_OpenLib,           // -6   (0x06) pos 0
    __g_lxa_locale_CloseLib,          // -12  (0x0c) pos 1
    __g_lxa_locale_ExpungeLib,        // -18  (0x12) pos 2
    __g_lxa_locale_ExtFuncLib,        // -24  (0x18) pos 3
    _locale_Reserved,                 // -30  (0x1e) pos 4 - reserved (ARexx host)
    _locale_CloseCatalog,             // -36  (0x24) pos 5
    _locale_CloseLocale,              // -42  (0x2a) pos 6
    _locale_ConvToLower,              // -48  (0x30) pos 7
    _locale_ConvToUpper,              // -54  (0x36) pos 8
    _locale_FormatDate,               // -60  (0x3c) pos 9
    _locale_FormatString,             // -66  (0x42) pos 10
    _locale_GetCatalogStr,            // -72  (0x48) pos 11
    _locale_GetLocaleStr,             // -78  (0x4e) pos 12
    _locale_IsAlNum,                  // -84  (0x54) pos 13
    _locale_IsAlpha,                  // -90  (0x5a) pos 14
    _locale_IsCntrl,                  // -96  (0x60) pos 15
    _locale_IsDigit,                  // -102 (0x66) pos 16
    _locale_IsGraph,                  // -108 (0x6c) pos 17
    _locale_IsLower,                  // -114 (0x72) pos 18
    _locale_IsPrint,                  // -120 (0x78) pos 19
    _locale_IsPunct,                  // -126 (0x7e) pos 20
    _locale_IsSpace,                  // -132 (0x84) pos 21
    _locale_IsUpper,                  // -138 (0x8a) pos 22
    _locale_IsXDigit,                 // -144 (0x90) pos 23
    _locale_OpenCatalogA,             // -150 (0x96) pos 24
    _locale_OpenLocale,               // -156 (0x9c) pos 25
    _locale_ParseDate,                // -162 (0xa2) pos 26
    _locale_Reserved,                 // -168 (0xa8) pos 27 - reserved
    _locale_StrConvert,               // -174 (0xae) pos 28
    _locale_StrnCmp,                  // -180 (0xb4) pos 29
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_locale_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_locale_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_locale_ExLibID[0],
    (ULONG) 0
};
