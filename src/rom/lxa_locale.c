/*
 * lxa locale.library implementation
 *
 * Provides internationalization and localization support.
 * Locales come from the built-in AmigaOS 3.1 defaults or from locale prefs
 * files (OpenLocale(), LocalePrefsUpdate() - Phase 236).
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
#include <prefs/prefhdr.h>
#include <prefs/locale.h>
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
    struct Locale    *cached_locale;    /* the default locale (OpenLocale(NULL)):
                                         * built in, or installed by IPrefs
                                         * with LocalePrefsUpdate() */
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

/* Default locale structure: the AmigaOS 3.1 built-in defaults, used when
 * no locale prefs exist (reference-verified, Phase 222f) */
/* AmigaOS 3.1 without locale prefs: loc_LocaleName "united_states.country",
 * loc_LanguageName "english.language", loc_PrefLanguages[0] "english"
 * (reference-verified, Phase 220) */
static UBYTE g_DefaultLocaleName[] = "united_states.country";
static UBYTE g_DefaultLocaleLanguageName[] = "english.language";
static UBYTE g_DefaultLanguageName[] = "english";
static UBYTE g_DefaultDateTimeFormat[] = "%A %B %e %Y %Q:%M %p";
static UBYTE g_DefaultDateFormat[] = "%A %B %e %Y";
static UBYTE g_DefaultTimeFormat[] = "%Q:%M %p";
static UBYTE g_DefaultShortDateTimeFormat[] = "%m/%d/%y %Q:%M %p";
static UBYTE g_DefaultShortDateFormat[] = "%m/%d/%y";
static UBYTE g_DefaultShortTimeFormat[] = "%Q:%M %p";
static UBYTE g_DefaultDecimalPoint[] = ".";
static UBYTE g_DefaultGroupSeparator[] = ",";
static UBYTE g_DefaultMonCS[] = "$";
static UBYTE g_DefaultMonIntCS[] = "USD";
static UBYTE g_DefaultMonSmallCS[] = "\xa2";
static UBYTE g_DefaultGrouping[] = { 3, 0 };

static struct Locale g_DefaultLocale = {
    g_DefaultLocaleName,             /* loc_LocaleName */
    g_DefaultLocaleLanguageName,     /* loc_LanguageName */
    { g_DefaultLanguageName },       /* loc_PrefLanguages */
    0,                               /* loc_Flags */
    0,                               /* loc_CodeSet */
    0x55534100,                      /* loc_CountryCode "USA" */
    1,                               /* loc_TelephoneCode */
    -300,                            /* loc_GMTOffset (minutes, as on 3.1) */
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
    g_DefaultMonSmallCS,             /* loc_MonSmallCS */
    g_DefaultMonIntCS,               /* loc_MonIntCS */
    (STRPTR)"",                      /* loc_MonPositiveSign */
    SS_NOSPACE,                      /* loc_MonPositiveSpaceSep */
    SP_PREC_ALL,                     /* loc_MonPositiveSignPos */
    CSP_PRECEDES,                    /* loc_MonPositiveCSPos */
    0,                               /* loc_Reserved2 */
    (STRPTR)"-",                     /* loc_MonNegativeSign */
    SS_NOSPACE,                      /* loc_MonNegativeSpaceSep */
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

/*
 * Character tables of the built-in (english, ISO-8859-1) locale.  The
 * classification bitmaps and the collation keys were captured from
 * AmigaOS 3.1 (tests/probes/locale/basics.ref.out, Phase 222f).
 */
/* character classes of the built-in (english) locale, AmigaOS 3.1 */
#define LC_ALNUM 0x0001
#define LC_ALPHA 0x0002
#define LC_CNTRL 0x0004
#define LC_DIGIT 0x0008
#define LC_GRAPH 0x0010
#define LC_LOWER 0x0020
#define LC_PRINT 0x0040
#define LC_PUNCT 0x0080
#define LC_SPACE 0x0100
#define LC_UPPER 0x0200
#define LC_XDIGIT 0x0400
static const UWORD g_CharClass[256] = {
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0144, 0x0144, 0x0144, 0x0144, 0x0144, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0140, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x0459, 0x0459, 0x0459, 0x0459, 0x0459, 0x0459, 0x0459, 0x0459,
    0x0459, 0x0459, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x0653, 0x0653, 0x0653, 0x0653, 0x0653, 0x0653, 0x0253,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253,
    0x0253, 0x0253, 0x0253, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x0473, 0x0473, 0x0473, 0x0473, 0x0473, 0x0473, 0x0073,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073,
    0x0073, 0x0073, 0x0073, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0140, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253,
    0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0253, 0x0273,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x00d0,
    0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073, 0x0073,
};

/* SC_COLLATE1/SC_COLLATE2 primary sort key of each character, AmigaOS 3.1 */
static const UBYTE g_CollateKey[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    0x20, 0x21, 0x24, 0x24, 0x67, 0x68, 0x69, 0x53, 0x6a, 0x6b, 0x6c, 0x22, 0x6d, 0x6e, 0x6f, 0x70,
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x22, 0x7c, 0x7d, 0x7e, 0x3f,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49,
    0x44, 0x4e, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x2a, 0x4f, 0x55, 0x55, 0x55, 0x55, 0x59, 0x50, 0x59,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x43, 0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49,
    0x44, 0x4e, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x2f, 0x4f, 0x55, 0x55, 0x55, 0x55, 0x59, 0x50, 0x59,
};

static ULONG _loc_class(ULONG character, UWORD mask)
{
    return (g_CharClass[character & 0xff] & mask) ? TRUE : FALSE;
}

/* ConvToUpper: a-z and 0xe0-0xfe (also 0xf7) map down by 0x20; 0xdf and 0xff stay */
static ULONG _loc_to_upper(ULONG character)
{
    character &= 0xff;
    if ((character >= 'a' && character <= 'z') || (character >= 0xe0 && character <= 0xfe))
        return character - 0x20;
    return character;
}

static ULONG _loc_to_lower(ULONG character)
{
    character &= 0xff;
    if ((character >= 'A' && character <= 'Z') || (character >= 0xc0 && character <= 0xde))
        return character + 0x20;
    return character;
}

static UBYTE _loc_key(ULONG type, UBYTE character)
{
    if (type == SC_ASCII)
        return (UBYTE)_loc_to_upper(character);
    return g_CollateKey[character];
}

static LONG _loc_strlen_n(CONST_STRPTR s)
{
    LONG n = 0;
    while (s[n])
        n++;
    return n;
}

/*
 * StrConvert as on AmigaOS 3.1: SC_ASCII/SC_COLLATE1 write one key per
 * character (at most bufferSize - 1) plus a NUL.  SC_COLLATE2 writes the
 * primary key of character i at buffer[i] and the original character (when
 * it differs from its key) at buffer[strlen + j]; a character is only
 * converted while two more bytes fit, and the NUL goes after the secondary
 * part (even past bufferSize, as on 3.1).  The result is
 * min(bufferSize, converted length + 1).
 */
static ULONG _loc_strconvert(CONST_STRPTR string, UBYTE *dst, ULONG size, ULONG type)
{
    LONG len, i, total;

    if (size == 0)
        return 0;

    len = _loc_strlen_n(string);

    if (type == SC_COLLATE2)
    {
        ULONG count = 0;
        LONG j = 0;

        total = len;
        for (i = 0; i < len; i++)
        {
            UBYTE c = (UBYTE)string[i];
            if (g_CollateKey[c] != c)
                total++;
        }

        for (i = 0; i < len; i++)
        {
            UBYTE c = (UBYTE)string[i];
            UBYTE k = g_CollateKey[c];

            if (count + 2 > size)
                break;
            dst[i] = k;
            count++;
            if (k != c)
            {
                dst[len + j++] = c;
                count++;
            }
        }
        dst[len + j] = 0;
    }
    else
    {
        LONG n = len;
        if ((ULONG)n > size - 1)
            n = (LONG)(size - 1);
        for (i = 0; i < n; i++)
            dst[i] = _loc_key(type, (UBYTE)string[i]);
        dst[n] = 0;
        total = len;
    }

    return (ULONG)(total + 1) < size ? (ULONG)(total + 1) : size;
}

/*
 * StrnCmp: compares at most length characters (length < 0: all) by their
 * keys; SC_COLLATE2 breaks ties of the primary keys by the original bytes.
 */
static LONG _loc_strncmp_keys(CONST_STRPTR s1, CONST_STRPTR s2, LONG length, ULONG type, BOOL raw)
{
    while (length != 0)
    {
        UBYTE a = (UBYTE)*s1++;
        UBYTE b = (UBYTE)*s2++;
        LONG ka = raw ? a : _loc_key(type, a);
        LONG kb = raw ? b : _loc_key(type, b);

        if (ka != kb)
            return ka - kb;
        if (!a)
            break;
        if (length > 0)
            length--;
    }
    return 0;
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
    PRIVATE_FUNCTION_ERROR("_locale", "ExtFuncLib");
    return 0;
}

/* Reserved function stub */
static ULONG _locale_Reserved(void) { PRIVATE_FUNCTION_ERROR("_locale", "Reserved"); return 0; }

/****************************************************************************/
/* Locale preferences (Phase 236)                                            */
/****************************************************************************/

/*
 * A locale opened from a locale prefs file (prefs/locale.h: FORM PREF with
 * an LCLE chunk holding struct LocalePrefs).  As on AmigaOS 3.1 (reference:
 * tests/scenarios/gallery-prefs-sys.yaml) loc_LocaleName is the file name,
 * the country fields come from the file's CountryPrefs, loc_GMTOffset is
 * lp_GMTOffset and the preferred languages are the file's list.  Language
 * strings stay English: languages other than the built-in english need the
 * LOCALE:Languages libraries (Phase 257).
 */
#define LXA_LOCALE_MAGIC 0x4c4f434cUL   /* 'LOCL' */

struct LXALocale
{
    struct Locale       loc;
    ULONG               magic;
    LONG                opencnt;
    UBYTE               name[32];
    UBYTE               langname[40];
    UBYTE               preflang[10][30];
    struct CountryPrefs cp;
};

static BOOL _loc_is_lxa_locale(struct Locale *locale)
{
    return locale && locale != &g_DefaultLocale &&
           ((struct LXALocale *)locale)->magic == LXA_LOCALE_MAGIC;
}

static void _loc_copy_str(UBYTE *dst, CONST UBYTE *src, LONG max)
{
    LONG i;

    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

static ULONG _loc_get_long(CONST UBYTE *p)
{
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | (ULONG)p[3];
}

/* Read the LCLE chunk of a locale prefs file into *lp; FALSE when the file
 * is not a locale prefs file. */
static BOOL _loc_read_prefs(CONST_STRPTR name, struct LocalePrefs *lp)
{
    BPTR fh = Open((STRPTR)name, MODE_OLDFILE);
    UBYTE hdr[12];
    BOOL ok = FALSE;

    if (!fh)
        return FALSE;
    if (Read(fh, hdr, 12) == 12 && _loc_get_long(hdr) == ID_FORM && _loc_get_long(hdr + 8) == ID_PREF)
    {
        for (;;)
        {
            ULONG id, len;

            if (Read(fh, hdr, 8) != 8)
                break;
            id = _loc_get_long(hdr);
            len = _loc_get_long(hdr + 4);
            if (id == ID_LCLE)
            {
                ULONG want = len < sizeof(*lp) ? len : sizeof(*lp);
                memset(lp, 0, sizeof(*lp));
                ok = Read(fh, lp, want) == (LONG)want && want >= sizeof(*lp) - sizeof(lp->lp_CountryData) / 2;
                break;
            }
            if (Seek(fh, (len + 1) & ~1UL, OFFSET_CURRENT) < 0)
                break;
        }
    }
    Close(fh);
    return ok;
}

static struct Locale *_loc_open_prefs_locale(CONST_STRPTR name)
{
    struct LocalePrefs *lp;
    struct LXALocale *ll;
    struct CountryPrefs *cp;
    UWORD i, n;

    lp = AllocVec(sizeof(*lp), MEMF_CLEAR);
    if (!lp)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    if (!_loc_read_prefs(name, lp))
    {
        FreeVec(lp);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return NULL;
    }
    ll = AllocVec(sizeof(*ll), MEMF_CLEAR | MEMF_PUBLIC);
    if (!ll)
    {
        FreeVec(lp);
        SetIoErr(ERROR_NO_FREE_STORE);
        return NULL;
    }
    ll->magic = LXA_LOCALE_MAGIC;
    ll->opencnt = 1;
    CopyMem(&lp->lp_CountryData, &ll->cp, sizeof(ll->cp));
    cp = &ll->cp;

    _loc_copy_str(ll->name, (CONST UBYTE *)FilePart((STRPTR)name), sizeof(ll->name));
    _loc_copy_str(ll->langname, (CONST UBYTE *)"english.language", sizeof(ll->langname));
    for (i = 0, n = 0; i < 10; i++)
    {
        if (!lp->lp_PreferredLanguages[i][0])
            continue;
        _loc_copy_str(ll->preflang[n], (CONST UBYTE *)lp->lp_PreferredLanguages[i], 30);
        ll->loc.loc_PrefLanguages[n] = (STRPTR)ll->preflang[n];
        n++;
    }
    if (n == 0)
    {
        _loc_copy_str(ll->preflang[0], g_DefaultLanguageName, 30);
        ll->loc.loc_PrefLanguages[0] = (STRPTR)ll->preflang[0];
    }
    ll->loc.loc_GMTOffset = lp->lp_GMTOffset;
    ll->loc.loc_Flags = (UBYTE)lp->lp_Flags;
    FreeVec(lp);

    ll->loc.loc_LocaleName = (STRPTR)ll->name;
    ll->loc.loc_LanguageName = (STRPTR)ll->langname;
    ll->loc.loc_CodeSet = 0;
    ll->loc.loc_CountryCode = cp->cp_CountryCode;
    ll->loc.loc_TelephoneCode = cp->cp_TelephoneCode;
    ll->loc.loc_MeasuringSystem = cp->cp_MeasuringSystem;
    ll->loc.loc_CalendarType = cp->cp_CalendarType;
    ll->loc.loc_DateTimeFormat = (STRPTR)cp->cp_DateTimeFormat;
    ll->loc.loc_DateFormat = (STRPTR)cp->cp_DateFormat;
    ll->loc.loc_TimeFormat = (STRPTR)cp->cp_TimeFormat;
    ll->loc.loc_ShortDateTimeFormat = (STRPTR)cp->cp_ShortDateTimeFormat;
    ll->loc.loc_ShortDateFormat = (STRPTR)cp->cp_ShortDateFormat;
    ll->loc.loc_ShortTimeFormat = (STRPTR)cp->cp_ShortTimeFormat;
    ll->loc.loc_DecimalPoint = (STRPTR)cp->cp_DecimalPoint;
    ll->loc.loc_GroupSeparator = (STRPTR)cp->cp_GroupSeparator;
    ll->loc.loc_FracGroupSeparator = (STRPTR)cp->cp_FracGroupSeparator;
    ll->loc.loc_Grouping = cp->cp_Grouping;
    ll->loc.loc_FracGrouping = cp->cp_FracGrouping;
    ll->loc.loc_MonDecimalPoint = (STRPTR)cp->cp_MonDecimalPoint;
    ll->loc.loc_MonGroupSeparator = (STRPTR)cp->cp_MonGroupSeparator;
    ll->loc.loc_MonFracGroupSeparator = (STRPTR)cp->cp_MonFracGroupSeparator;
    ll->loc.loc_MonGrouping = cp->cp_MonGrouping;
    ll->loc.loc_MonFracGrouping = cp->cp_MonFracGrouping;
    ll->loc.loc_MonFracDigits = cp->cp_MonFracDigits;
    ll->loc.loc_MonIntFracDigits = cp->cp_MonIntFracDigits;
    ll->loc.loc_MonCS = (STRPTR)cp->cp_MonCS;
    ll->loc.loc_MonSmallCS = (STRPTR)cp->cp_MonSmallCS;
    ll->loc.loc_MonIntCS = (STRPTR)cp->cp_MonIntCS;
    ll->loc.loc_MonPositiveSign = (STRPTR)cp->cp_MonPositiveSign;
    ll->loc.loc_MonPositiveSpaceSep = cp->cp_MonPositiveSpaceSep;
    ll->loc.loc_MonPositiveSignPos = cp->cp_MonPositiveSignPos;
    ll->loc.loc_MonPositiveCSPos = cp->cp_MonPositiveCSPos;
    ll->loc.loc_MonNegativeSign = (STRPTR)cp->cp_MonNegativeSign;
    ll->loc.loc_MonNegativeSpaceSep = cp->cp_MonNegativeSpaceSep;
    ll->loc.loc_MonNegativeSignPos = cp->cp_MonNegativeSignPos;
    ll->loc.loc_MonNegativeCSPos = cp->cp_MonNegativeCSPos;
    SetIoErr(0);
    return &ll->loc;
}

static void _loc_release(struct Locale *locale)
{
    struct LXALocale *ll = (struct LXALocale *)locale;

    if (!_loc_is_lxa_locale(locale))
        return;
    if (--ll->opencnt <= 0)
    {
        ll->magic = 0;
        FreeVec(ll);
    }
}

/* LocalePrefsUpdate() (-168, private): IPrefs makes a locale opened from
 * ENV:Sys/locale.prefs the default locale.  The library keeps the caller's
 * reference; the previous default is returned for the caller to close. */
struct Locale * _locale_LocalePrefsUpdate ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                                            register struct Locale       *locale     __asm("a0"))
{
    struct Locale *old;

    DPRINTF (LOG_DEBUG, "_locale: LocalePrefsUpdate() locale=0x%08lx\n", (ULONG)locale);
    if (!locale)
        return NULL;
    Forbid();
    old = LocaleBase->cached_locale;
    LocaleBase->cached_locale = locale;
    if (LocaleBase->cached_locale_name)
    {
        FreeVec(LocaleBase->cached_locale_name);
        LocaleBase->cached_locale_name = NULL;
    }
    Permit();
    return (old == &g_DefaultLocale) ? NULL : old;
}

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
    /* the built-in locale is never freed; a prefs locale when its last
     * user closes it (the default locale holds a reference of its own) */
    (void)LocaleBase;
    _loc_release(locale);
}

ULONG _locale_ConvToLower ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;

    return _loc_to_lower(character);
}

ULONG _locale_ConvToUpper ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;

    return _loc_to_upper(character);
}

/****************************************************************************/
/* Helper: call putCharFunc hook with a single character                      */
/****************************************************************************/

static void _loc_putchar(struct Hook *hook, ULONG ch, struct Locale *locale)
{
    /* FormatDate/FormatString hook convention:
     * A0 = hook, A1 = character (value, not pointer), A2 = locale
     */
    typedef void (*HookFuncPtr)(
        register struct Hook    *hook   __asm("a0"),
        register ULONG           ch     __asm("a1"),
        register struct Locale  *locale __asm("a2")
    );
    ((HookFuncPtr)hook->h_Entry)(hook, ch, locale);
}

static void _loc_putstr(struct Hook *hook, CONST_STRPTR str, struct Locale *locale)
{
    if (!str)
        return;
    while (*str)
        _loc_putchar(hook, (ULONG)(UBYTE)*str++, locale);
}

/* value (>= 0) with exactly `digits` digits (value is taken modulo 10^digits),
 * padded with `pad`; digits == 0 means no padding */
static void _loc_putnum(struct Hook *hook, LONG value, int digits, char pad, struct Locale *locale)
{
    char buf[12];
    int i = 0;
    ULONG v = (ULONG)value;

    do
    {
        buf[i++] = (char)('0' + v % 10);
        v /= 10;
    } while (i < 11 && (digits ? i < digits : v != 0));

    if (digits && pad != '0')
    {
        /* replace leading zeros with the pad character */
        int k = i - 1;
        while (k > 0 && buf[k] == '0')
            buf[k--] = pad;
    }

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
    LONG ylen;    /* 365 or 366 */
};

static void _datestamp_to_parts(const struct DateStamp *ds, struct DateParts *dp)
{
    LONG days = ds->ds_Days;
    LONG minutes = ds->ds_Minute;
    LONG ticks = ds->ds_Tick;
    LONG year = 1978;
    LONG month = 0;
    BOOL leap;

    dp->hour = minutes / 60;
    dp->min = minutes % 60;
    dp->sec = ticks / TICKS_PER_SECOND;

    /* Jan 1, 1978 was a Sunday (wday=0) */
    dp->wday = days % 7;
    if (dp->wday < 0)
        dp->wday += 7;

    while (days >= (_is_leap_year(year) ? 366 : 365))
    {
        days -= (_is_leap_year(year) ? 366 : 365);
        year++;
    }

    leap = _is_leap_year(year);
    dp->year = year;
    dp->yday = days + 1;
    dp->ylen = leap ? 366 : 365;

    while (month < 12 && days >= g_DaysInMonth[leap ? 1 : 0][month])
    {
        days -= g_DaysInMonth[leap ? 1 : 0][month];
        month++;
    }

    dp->month = month + 1;
    dp->day = days + 1;
}

/****************************************************************************/
/* FormatDate implementation                                                 */
/****************************************************************************/

static CONST_STRPTR _loc_str(struct Locale *locale, ULONG id)
{
    (void)locale;
    if (id < sizeof(g_LocaleStrings) / sizeof(g_LocaleStrings[0]))
        return (CONST_STRPTR)g_LocaleStrings[id];
    return NULL;
}

/*
 * One template.  Every template - also the built-in ones that %c, %C, %D,
 * %r, %R, %T, %x and %X expand to - ends with a NUL sent to the hook, as on
 * AmigaOS 3.1.  The day-of-year behind %j, %U and %W is counted as on 3.1
 * (day of year + 1348 - length of the year, printed modulo 1000 / 100).
 */
static void _loc_format_date(struct Hook *hook, struct Locale *locale,
                             const UBYTE *fmt, const struct DateParts *dp)
{
    LONG h12 = dp->hour % 12;
    LONG jday = dp->yday + 1348 - dp->ylen;

    if (h12 == 0)
        h12 = 12;

    while (*fmt)
    {
        if (*fmt != '%')
        {
            _loc_putchar(hook, (ULONG)*fmt++, locale);
            continue;
        }
        fmt++;
        if (!*fmt)
            break;

        switch (*fmt)
        {
            case '%': _loc_putchar(hook, '%', locale); break;
            case 'a': _loc_putstr(hook, _loc_str(locale, ABDAY_1 + dp->wday), locale); break;
            case 'A': _loc_putstr(hook, _loc_str(locale, DAY_1 + dp->wday), locale); break;
            case 'b':
            case 'h': _loc_putstr(hook, _loc_str(locale, ABMON_1 + dp->month - 1), locale); break;
            case 'B': _loc_putstr(hook, _loc_str(locale, MON_1 + dp->month - 1), locale); break;
            case 'c': _loc_format_date(hook, locale, (const UBYTE *)"%a %b %d %H:%M:%S %Y", dp); break;
            case 'C': _loc_format_date(hook, locale, (const UBYTE *)"%a %b %e %T %Z %Y", dp); break;
            case 'd': _loc_putnum(hook, dp->day, 2, '0', locale); break;
            case 'D':
            case 'x': _loc_format_date(hook, locale, (const UBYTE *)"%m/%d/%y", dp); break;
            case 'e': _loc_putnum(hook, dp->day, 2, ' ', locale); break;
            case 'H': _loc_putnum(hook, dp->hour, 2, '0', locale); break;
            case 'I': _loc_putnum(hook, h12, 2, '0', locale); break;
            case 'j': _loc_putnum(hook, jday, 3, '0', locale); break;
            case 'm': _loc_putnum(hook, dp->month, 2, '0', locale); break;
            case 'M': _loc_putnum(hook, dp->min, 2, '0', locale); break;
            case 'n': _loc_putchar(hook, '\n', locale); break;
            case 'p': _loc_putstr(hook, _loc_str(locale, dp->hour < 12 ? AM_STR : PM_STR), locale); break;
            case 'q': _loc_putnum(hook, dp->hour, 0, '0', locale); break;
            case 'Q': _loc_putnum(hook, h12, 0, '0', locale); break;
            case 'r':
                _loc_format_date(hook, locale, (const UBYTE *)"%I:%M:%S ", dp);
                _loc_putstr(hook, _loc_str(locale, dp->hour < 12 ? AM_STR : PM_STR), locale);
                break;
            case 'R': _loc_format_date(hook, locale, (const UBYTE *)"%H:%M", dp); break;
            case 'S': _loc_putnum(hook, dp->sec, 2, '0', locale); break;
            case 't': _loc_putchar(hook, '\t', locale); break;
            case 'T':
            case 'X': _loc_format_date(hook, locale, (const UBYTE *)"%H:%M:%S", dp); break;
            case 'U': _loc_putnum(hook, (jday - 1 + 7 - dp->wday) / 7, 2, '0', locale); break;
            case 'w': _loc_putnum(hook, dp->wday, 1, '0', locale); break;
            case 'W': _loc_putnum(hook, (jday - 1 + 7 - (dp->wday + 6) % 7) / 7, 2, '0', locale); break;
            case 'y': _loc_putnum(hook, dp->year, 2, '0', locale); break;
            case 'Y': _loc_putnum(hook, dp->year, 4, '0', locale); break;
            default:
                /* unknown conversion: only the '%' is printed */
                _loc_putchar(hook, '%', locale);
                break;
        }
        fmt++;
    }

    _loc_putchar(hook, 0, locale);
}

VOID _locale_FormatDate ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                          register struct Locale       *locale       __asm("a0"),
                          register CONST_STRPTR         fmtTemplate  __asm("a1"),
                          register CONST struct DateStamp *date      __asm("a2"),
                          register struct Hook         *putCharFunc  __asm("a3"))
{
    struct DateParts dp;

    DPRINTF (LOG_DEBUG, "_locale: FormatDate() called\n");

    if (!putCharFunc)
        return;

    if (!locale)
        locale = &g_DefaultLocale;

    if (!fmtTemplate || !date)
    {
        _loc_putchar(putCharFunc, 0, locale);
        return;
    }

    _datestamp_to_parts(date, &dp);
    _loc_format_date(putCharFunc, locale, (const UBYTE *)fmtTemplate, &dp);
}

/****************************************************************************/
/* FormatString implementation                                               */
/****************************************************************************/

#define LOC_MAX_ARGS 64

struct LocFmtSpec
{
    LONG  argnum;      /* 0: no argument */
    BOOL  left;
    BOOL  zero;
    LONG  width;
    LONG  limit;       /* -1: none */
    BOOL  is_long;
    UBYTE type;        /* 0: template ended inside the specification */
};

/* parse one specification after the '%'; returns the position after it */
static const UBYTE *_loc_parse_spec(const UBYTE *fmt, struct LocFmtSpec *spec, LONG *cur)
{
    const UBYTE *look = fmt;
    LONG pos = 0;

    while (*look >= '0' && *look <= '9')
        pos = pos * 10 + (*look++ - '0');
    if (*look == '$')
    {
        *cur = pos;
        spec->argnum = pos;
        fmt = look + 1;
    }
    else
        spec->argnum = -1;          /* sequential: assigned below */

    spec->left = FALSE;
    spec->zero = FALSE;
    while (*fmt == '-' || *fmt == '0')
    {
        if (*fmt == '-')
            spec->left = TRUE;
        else
            spec->zero = TRUE;
        fmt++;
    }

    spec->width = 0;
    while (*fmt >= '0' && *fmt <= '9')
        spec->width = spec->width * 10 + (*fmt++ - '0');

    spec->limit = -1;
    if (*fmt == '.')
    {
        fmt++;
        spec->limit = 0;
        while (*fmt >= '0' && *fmt <= '9')
            spec->limit = spec->limit * 10 + (*fmt++ - '0');
    }

    spec->is_long = FALSE;
    if (*fmt == 'l')
    {
        spec->is_long = TRUE;
        fmt++;
    }

    spec->type = *fmt;
    if (*fmt)
        fmt++;

    switch (spec->type)
    {
        case 'd': case 'D': case 'u': case 'U': case 'x': case 'X':
        case 'c': case 's': case 'b':
            if (spec->argnum < 0)
                spec->argnum = (*cur)++;
            break;
        default:
            spec->argnum = 0;
            break;
    }
    return fmt;
}

static LONG _loc_arg_size(const struct LocFmtSpec *spec)
{
    if (spec->type == 's' || spec->type == 'b' || spec->is_long)
        return 4;
    return 2;
}

/* unsigned value with the locale's digit grouping; returns TRUE if a
 * separator was inserted.  Digits are written backwards from end. */
static BOOL _loc_group_digits(struct Locale *locale, ULONG value, char **pstart, BOOL group)
{
    char *p = *pstart;
    const UBYTE *grouping = locale->loc_Grouping;
    CONST_STRPTR sep = (CONST_STRPTR)locale->loc_GroupSeparator;
    LONG gsize = (group && grouping && grouping[0] && grouping[0] != 255) ? grouping[0] : 0;
    LONG gi = 0, n = 0;
    BOOL inserted = FALSE;

    do
    {
        if (gsize && n == gsize)
        {
            LONG sl = sep ? _loc_strlen_n(sep) : 0;
            while (sl > 0)
                *--p = sep[--sl];
            inserted = TRUE;
            n = 0;
            if (grouping[gi + 1] == 255)
                gsize = 0;
            else if (grouping[gi + 1] != 0)
                gsize = grouping[++gi];
        }
        *--p = (char)('0' + value % 10);
        value /= 10;
        n++;
    } while (value);

    *pstart = p;
    return inserted;
}

APTR _locale_FormatString ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                            register struct Locale       *locale       __asm("a0"),
                            register CONST_STRPTR         fmtTemplate  __asm("a1"),
                            register APTR                 dataStream   __asm("a2"),
                            register struct Hook         *putCharFunc  __asm("a3"))
{
    UBYTE sizes[LOC_MAX_ARGS + 1];
    LONG offsets[LOC_MAX_ARGS + 2];
    const UBYTE *fmt;
    struct LocFmtSpec spec;
    LONG cur, i, total;
    BOOL grouped = FALSE;

    DPRINTF (LOG_DEBUG, "_locale: FormatString() called\n");

    if (!putCharFunc || !fmtTemplate)
        return dataStream;

    if (!locale)
        locale = &g_DefaultLocale;

    /* pass 1: the size of every argument; an argument no specification
     * refers to takes no room in the data stream (AmigaOS 3.1) */
    for (i = 0; i <= LOC_MAX_ARGS; i++)
        sizes[i] = 0;
    cur = 1;
    fmt = (const UBYTE *)fmtTemplate;
    while (*fmt)
    {
        if (*fmt++ != '%')
            continue;
        fmt = _loc_parse_spec(fmt, &spec, &cur);
        if (spec.argnum > 0 && spec.argnum <= LOC_MAX_ARGS)
            sizes[spec.argnum] = (UBYTE)_loc_arg_size(&spec);
        if (!spec.type)
            break;
    }
    offsets[1] = 0;
    for (i = 1; i <= LOC_MAX_ARGS; i++)
        offsets[i + 1] = offsets[i] + sizes[i];
    total = offsets[LOC_MAX_ARGS + 1];

    /* pass 2: output */
    cur = 1;
    fmt = (const UBYTE *)fmtTemplate;
    while (*fmt)
    {
        char buf[48];
        char *str, *end;
        LONG len;
        UBYTE *arg;

        if (*fmt != '%')
        {
            _loc_putchar(putCharFunc, (ULONG)*fmt++, locale);
            continue;
        }
        fmt = _loc_parse_spec(fmt + 1, &spec, &cur);
        if (!spec.type)
            break;

        arg = NULL;
        if (spec.argnum > 0 && spec.argnum <= LOC_MAX_ARGS)
            arg = (UBYTE *)dataStream + offsets[spec.argnum];

        end = buf + sizeof(buf) - 1;
        *end = 0;
        str = end;

        switch (spec.type)
        {
            case 'd': case 'D': case 'u': case 'U':
            {
                LONG value;
                BOOL neg = FALSE;

                if (spec.is_long)
                    value = *(LONG *)arg;
                else if (spec.type == 'd' || spec.type == 'D')
                    value = (WORD)*(UWORD *)arg;
                else
                    value = *(UWORD *)arg;

                if ((spec.type == 'd' || spec.type == 'D') && value < 0)
                {
                    neg = TRUE;
                    value = -value;
                }
                if (_loc_group_digits(locale, (ULONG)value, &str,
                                      spec.type == 'D' || spec.type == 'U'))
                    grouped = TRUE;
                if (neg)
                    *--str = '-';
                break;
            }
            case 'x': case 'X':
            {
                const char *hex = spec.type == 'x' ? "0123456789ABCDEF" : "0123456789abcdef";
                ULONG value = spec.is_long ? *(ULONG *)arg : *(UWORD *)arg;
                do
                {
                    *--str = hex[value & 15];
                    value >>= 4;
                } while (value);
                break;
            }
            case 'c':
                *--str = (char)(spec.is_long ? arg[3] : arg[1]);
                break;
            case 's':
                str = *(char **)arg;
                if (!str)
                    str = (char *)"";
                break;
            case 'b':
            {
                UBYTE *b = (UBYTE *)BADDR(*(BPTR *)arg);
                /* BSTR: printed from its length byte, not NUL-terminated */
                len = b ? b[0] : 0;
                str = b ? (char *)b + 1 : (char *)"";
                goto have_len;
            }
            default:
                /* unknown type: the character itself, without padding */
                _loc_putchar(putCharFunc, (ULONG)spec.type, locale);
                continue;
        }

        len = 0;
        while (str[len])
            len++;
have_len:
        if (spec.limit >= 0 && len > spec.limit)
            len = spec.limit;

        if (spec.left)
        {
            for (i = 0; i < len; i++)
                _loc_putchar(putCharFunc, (ULONG)(UBYTE)str[i], locale);
            for (i = len; i < spec.width; i++)
                _loc_putchar(putCharFunc, ' ', locale);
        }
        else if (spec.zero && str[0] == '-' && spec.width > len)
        {
            /* as on 3.1: the sign, the zeros, then len characters after the
             * sign - one more than the number has (usually its NUL) */
            _loc_putchar(putCharFunc, '-', locale);
            for (i = len; i < spec.width; i++)
                _loc_putchar(putCharFunc, '0', locale);
            for (i = 1; i <= len; i++)
                _loc_putchar(putCharFunc, (ULONG)(UBYTE)str[i], locale);
        }
        else
        {
            for (i = len; i < spec.width; i++)
                _loc_putchar(putCharFunc, spec.zero ? '0' : ' ', locale);
            for (i = 0; i < len; i++)
                _loc_putchar(putCharFunc, (ULONG)(UBYTE)str[i], locale);
        }
    }

    _loc_putchar(putCharFunc, 0, locale);

    /* AmigaOS 3.1 returns dataStream + 2 once digit grouping inserted a
     * separator (reference-verified, Phase 222f) */
    if (grouped)
        return (UBYTE *)dataStream + 2;
    return (UBYTE *)dataStream + total;
}

STRPTR _locale_GetCatalogStr ( register struct MyLocaleBase   *LocaleBase    __asm("a6"),
                               register CONST struct Catalog  *catalog       __asm("a0"),
                               register LONG                   stringNum     __asm("d0"),
                               register CONST_STRPTR           defaultString __asm("a1"))
{
    const struct MyCatalog *my_catalog = (const struct MyCatalog *)catalog;
    struct CatalogEntry *entry;

    stringNum = (LONG)(WORD)stringNum; /* sign-extend: GCC m68k move.w workaround */

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
LONG _locale_IsAlNum ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_ALNUM);
}

LONG _locale_IsAlpha ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_ALPHA);
}

LONG _locale_IsCntrl ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_CNTRL);
}

LONG _locale_IsDigit ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_DIGIT);
}

LONG _locale_IsGraph ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_GRAPH);
}

LONG _locale_IsLower ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_LOWER);
}

LONG _locale_IsPrint ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_PRINT);
}

LONG _locale_IsPunct ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_PUNCT);
}

LONG _locale_IsSpace ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_SPACE);
}

LONG _locale_IsUpper ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_UPPER);
}

LONG _locale_IsXDigit ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                        register struct Locale       *locale     __asm("a0"),
                        register ULONG                character  __asm("d0"))
{
    (void)LocaleBase;
    (void)locale;
    return _loc_class(character, LC_XDIGIT);
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

    /* catalogs are looked up by the preferred language names
     * ("english"), not by loc_LanguageName ("english.language") */
    requested_language = locale->loc_PrefLanguages[0] ? locale->loc_PrefLanguages[0]
                                                      : (STRPTR)g_DefaultLanguageName;

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
    struct Locale *locale;

    DPRINTF (LOG_DEBUG, "_locale: OpenLocale() called name='%s'\n", name ? (char *)name : "(null)");

    /* the default locale (Phase 236: installed by IPrefs from
     * ENV:Sys/locale.prefs, else the built-in one) */
    if (!name || !*name)
    {
        Forbid();
        locale = LocaleBase ? LocaleBase->cached_locale : &g_DefaultLocale;
        if (_loc_is_lxa_locale(locale))
            ((struct LXALocale *)locale)->opencnt++;
        Permit();
        SetIoErr(0);
        return locale ? locale : &g_DefaultLocale;
    }

    /* a name is a locale prefs file: a missing file yields NULL (AmigaOS
     * 3.1, reference-verified) */
    {
        BPTR lock = Lock((STRPTR)name, SHARED_LOCK);

        if (!lock)
        {
            SetIoErr(ERROR_OBJECT_NOT_FOUND);
            return NULL;
        }
        UnLock(lock);
        return _loc_open_prefs_locale(name);
    }
}

/****************************************************************************/
/* ParseDate implementation                                                  */
/****************************************************************************/

/*
 * The behaviour below (character by character, including how many
 * characters are read from the hook) was established by black-box probing
 * of AmigaOS 3.1 (tests/probes/locale/format.c, Phase 222f):
 *
 *  - leading white space is skipped, an empty input fails;
 *  - numeric fields (%d %e %m %y %Y %H %I %M %S) skip white space and read
 *    digits; a field followed by white space is range-checked at once;
 *  - month names (%b %B %h) accept a number, or a word that starts with
 *    an abbreviated name when the field ends the template, or exactly an
 *    abbreviated or full name otherwise; weekday names (%a %A) are only
 *    checked when the template continues; %p only works at the end of the
 *    template; "%%" swallows one word, other conversions read nothing;
 *  - a white-space template character skips any white space; any other
 *    template character is searched for (case-insensitively) - characters
 *    skipped on the way make the result FALSE, white space or the end of
 *    the input before it fails at once;
 *  - after the template, trailing characters are skipped (white space
 *    followed by the end of the input fails);
 *  - out-of-range fields fail without touching *date; an impossible date
 *    (Feb 30, beyond 2^32 seconds) clears *date and fails.
 */

struct LocParse
{
    struct Hook   *hook;
    struct Locale *locale;
    ULONG          c;
};

static void _loc_pgetc(struct LocParse *ps)
{
    /* ParseDate hook convention: A0 = hook, A2 = locale, A1 = NULL */
    typedef ULONG (*GetCharFuncPtr)(
        register struct Hook   *hook   __asm("a0"),
        register APTR           dummy  __asm("a1"),
        register struct Locale *locale __asm("a2")
    );
    ps->c = ((GetCharFuncPtr)ps->hook->h_Entry)(ps->hook, NULL, ps->locale) & 0xff;
}

static BOOL _loc_pspace(ULONG c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

static BOOL _loc_palpha(ULONG c)
{
    return (g_CharClass[c & 0xff] & LC_ALPHA) ? TRUE : FALSE;
}

static void _loc_pskipws(struct LocParse *ps)
{
    while (_loc_pspace(ps->c))
        _loc_pgetc(ps);
}

static LONG _loc_pnumber(struct LocParse *ps)
{
    LONG v = 0;
    while (ps->c >= '0' && ps->c <= '9')
    {
        if (v > (0x7fffffff - 9) / 10)
            v = 0x7fffffff;
        else
            v = v * 10 + (LONG)(ps->c - '0');
        _loc_pgetc(ps);
    }
    return v;
}

/* reads a word of letters (upper-cased) */
static void _loc_pword(struct LocParse *ps, UBYTE *word, LONG size)
{
    LONG n = 0;
    while (_loc_palpha(ps->c))
    {
        if (n < size - 1)
            word[n++] = (UBYTE)_loc_to_upper(ps->c);
        _loc_pgetc(ps);
    }
    word[n] = 0;
}

static BOOL _loc_pword_is(const UBYTE *word, CONST_STRPTR name, BOOL prefix)
{
    if (!name)
        return FALSE;
    while (*name)
    {
        if (*word != (UBYTE)_loc_to_upper((UBYTE)*name))
            return FALSE;
        word++;
        name++;
    }
    return prefix || *word == 0;
}

struct LocPDate
{
    LONG day, mon, year, hour, min, sec;
    BOOL yinvalid;
};

static BOOL _loc_pdate_invalid(const struct LocPDate *d)
{
    return d->mon < 1 || d->mon > 12 || d->day < 1 || d->day > 31 ||
           d->hour > 23 || d->min > 59 || d->sec > 59 ||
           d->yinvalid || d->year < 1978;
}

LONG _locale_ParseDate ( register struct MyLocaleBase    *LocaleBase  __asm("a6"),
                         register CONST struct Locale    *locale      __asm("a0"),
                         register struct DateStamp       *date        __asm("a1"),
                         register CONST_STRPTR            fmtTemplate __asm("a2"),
                         register struct Hook            *getCharFunc __asm("a3"))
{
    struct LocParse ps;
    struct LocPDate d;
    const UBYTE *t;
    LONG ampm = -1;
    BOOL mismatch = FALSE;
    UBYTE word[32];
    LONG days, y, m;
    ULONG secs;

    DPRINTF (LOG_DEBUG, "_locale: ParseDate() called\n");

    if (!getCharFunc || !fmtTemplate)
        return FALSE;

    if (!locale)
        locale = (struct Locale *)&g_DefaultLocale;

    ps.hook = getCharFunc;
    ps.locale = (struct Locale *)locale;

    d.day = 1;
    d.mon = 1;
    d.year = 1978;
    d.hour = 0;
    d.min = 0;
    d.sec = 0;
    d.yinvalid = FALSE;

    _loc_pgetc(&ps);
    _loc_pskipws(&ps);
    if (!ps.c)
        return FALSE;

    t = (const UBYTE *)fmtTemplate;
    while (*t)
    {
        if (*t == '%')
        {
            UBYTE code = t[1];
            UBYTE term;

            if (!code)
            {
                t++;
                continue;
            }
            term = t[2];
            t += 2;

            switch (code)
            {
                case 'd': case 'e': case 'm': case 'y': case 'Y':
                case 'H': case 'I': case 'M': case 'S':
                {
                    LONG v;
                    _loc_pskipws(&ps);
                    v = _loc_pnumber(&ps);
                    switch (code)
                    {
                        case 'd': case 'e': d.day = v; break;
                        case 'm': d.mon = v; break;
                        case 'y':
                            d.yinvalid = v > 99;
                            d.year = v < 78 ? 2000 + v : 1900 + v;
                            break;
                        case 'Y':
                            d.yinvalid = FALSE;
                            d.year = v;
                            break;
                        case 'H': case 'I': d.hour = v; break;
                        case 'M': d.min = v; break;
                        case 'S': d.sec = v; break;
                    }
                    if (_loc_pspace(ps.c) && _loc_pdate_invalid(&d))
                        return FALSE;
                    break;
                }

                case 'b': case 'B': case 'h':
                    _loc_pskipws(&ps);
                    if (ps.c >= '0' && ps.c <= '9')
                    {
                        d.mon = _loc_pnumber(&ps);
                        if (_loc_pspace(ps.c) && _loc_pdate_invalid(&d))
                            return FALSE;
                    }
                    else if (_loc_palpha(ps.c))
                    {
                        LONG i, found = 0;
                        _loc_pword(&ps, word, sizeof(word));
                        for (i = 0; i < 12 && !found; i++)
                        {
                            CONST_STRPTR ab = _loc_str(ps.locale, ABMON_1 + i);
                            if (!term)
                            {
                                if (_loc_pword_is(word, ab, TRUE))
                                    found = i + 1;
                            }
                            else if (_loc_pword_is(word, ab, FALSE) ||
                                     _loc_pword_is(word, _loc_str(ps.locale, MON_1 + i), FALSE))
                                found = i + 1;
                        }
                        if (!found)
                            goto fail_skip;
                        d.mon = found;
                    }
                    else
                        goto fail_skip;
                    break;

                case 'a': case 'A':
                    _loc_pskipws(&ps);
                    if (_loc_palpha(ps.c))
                    {
                        _loc_pword(&ps, word, sizeof(word));
                        if (term)
                        {
                            LONG i;
                            BOOL ok = FALSE;
                            for (i = 0; i < 7 && !ok; i++)
                                ok = _loc_pword_is(word, _loc_str(ps.locale, ABDAY_1 + i), FALSE) ||
                                     _loc_pword_is(word, _loc_str(ps.locale, DAY_1 + i), FALSE);
                            if (!ok)
                                goto fail_skip;
                        }
                    }
                    break;

                case 'p':
                    _loc_pskipws(&ps);
                    _loc_pword(&ps, word, sizeof(word));
                    if (term)
                        goto fail_skip;
                    if (_loc_pword_is(word, _loc_str(ps.locale, AM_STR), TRUE))
                        ampm = 0;
                    else if (_loc_pword_is(word, _loc_str(ps.locale, PM_STR), TRUE))
                        ampm = 1;
                    else
                        goto fail_skip;
                    break;

                case '%':
                    while (ps.c && !_loc_pspace(ps.c))
                        _loc_pgetc(&ps);
                    break;

                default:
                    /* other conversions read nothing */
                    break;
            }
        }
        else if (_loc_pspace(*t))
        {
            _loc_pskipws(&ps);
            t++;
        }
        else
        {
            while (_loc_to_upper(ps.c) != _loc_to_upper(*t))
            {
                if (!ps.c || _loc_pspace(ps.c))
                    return FALSE;
                mismatch = TRUE;
                _loc_pgetc(&ps);
            }
            _loc_pgetc(&ps);
            t++;
        }
    }

    /* trailing input */
    if (ps.c)
    {
        _loc_pskipws(&ps);
        if (!ps.c)
            return FALSE;
        while (ps.c)
            _loc_pgetc(&ps);
    }

    if (mismatch || _loc_pdate_invalid(&d))
        return FALSE;

    if (ampm == 0 && d.hour >= 12)
        d.hour -= 12;
    else if (ampm == 1 && d.hour < 12)
        d.hour += 12;

    /* an impossible date clears *date */
    if (date)
    {
        date->ds_Days = 0;
        date->ds_Minute = 0;
        date->ds_Tick = 0;
    }

    if (d.day > g_DaysInMonth[_is_leap_year(d.year) ? 1 : 0][d.mon - 1] || d.year > 2114)
        return FALSE;

    days = 0;
    for (y = 1978; y < d.year; y++)
        days += _is_leap_year(y) ? 366 : 365;
    for (m = 0; m < d.mon - 1; m++)
        days += g_DaysInMonth[_is_leap_year(d.year) ? 1 : 0][m];
    days += d.day - 1;

    /* the date must fit into 32 bits of seconds since 1978 */
    if (days > 49710)
        return FALSE;
    secs = (ULONG)days * 86400UL + (ULONG)(d.hour * 3600 + d.min * 60 + d.sec);
    if (days == 49710 && secs < (ULONG)days * 86400UL)
        return FALSE;

    if (date)
    {
        date->ds_Days = days;
        date->ds_Minute = d.hour * 60 + d.min;
        date->ds_Tick = d.sec * TICKS_PER_SECOND;
    }
    return TRUE;

fail_skip:
    while (ps.c && !_loc_pspace(ps.c))
        _loc_pgetc(&ps);
    return FALSE;
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

    if (!string || !buffer)
        return 0;

    if (type > SC_COLLATE2)
        type = SC_ASCII;

    return _loc_strconvert(string, (UBYTE *)buffer, bufferSize, type);
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

    if (type > SC_COLLATE2)
        type = SC_ASCII;

    result = _loc_strncmp_keys(string1, string2, length, type, FALSE);
    if (result == 0 && type == SC_COLLATE2)
        result = _loc_strncmp_keys(string1, string2, length, type, TRUE);
    return result;
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
    _locale_LocalePrefsUpdate,        // -168 (0xa8) pos 27 - private (IPrefs)
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
