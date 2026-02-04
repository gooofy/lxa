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

#include <libraries/locale.h>
#include <utility/tagitem.h>
#include <utility/hooks.h>

#include "util.h"

#define VERSION    47
#define REVISION   1
#define EXLIBNAME  "locale"
#define EXLIBVER   " 47.1 (2025/02/01)"

char __aligned _g_locale_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_locale_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_locale_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_locale_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* LocaleBase structure (extends standard definition) */
struct MyLocaleBase {
    struct LocaleBase lb;
    BPTR              SegList;
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
        FreeMem(catalog, sizeof(struct Catalog));
}

VOID _locale_CloseLocale ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                           register struct Locale       *locale     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_locale: CloseLocale() called\n");
    /* Don't free the default locale */
    if (locale && locale != &g_DefaultLocale)
        FreeMem(locale, sizeof(struct Locale));
}

ULONG _locale_ConvToLower ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    if (character >= 'A' && character <= 'Z')
        return character + ('a' - 'A');
    return character;
}

ULONG _locale_ConvToUpper ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                            register struct Locale       *locale     __asm("a0"),
                            register ULONG                character  __asm("d0"))
{
    if (character >= 'a' && character <= 'z')
        return character - ('a' - 'A');
    return character;
}

VOID _locale_FormatDate ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                          register struct Locale       *locale       __asm("a0"),
                          register CONST_STRPTR         fmtTemplate  __asm("a1"),
                          register CONST struct DateStamp *date      __asm("a2"),
                          register struct Hook         *putCharFunc  __asm("a3"))
{
    DPRINTF (LOG_DEBUG, "_locale: FormatDate() unimplemented STUB called\n");
    /* TODO: Implement date formatting */
}

APTR _locale_FormatString ( register struct MyLocaleBase *LocaleBase   __asm("a6"),
                            register struct Locale       *locale       __asm("a0"),
                            register CONST_STRPTR         fmtTemplate  __asm("a1"),
                            register APTR                 dataStream   __asm("a2"),
                            register struct Hook         *putCharFunc  __asm("a3"))
{
    DPRINTF (LOG_DEBUG, "_locale: FormatString() unimplemented STUB called\n");
    return dataStream;
}

STRPTR _locale_GetCatalogStr ( register struct MyLocaleBase   *LocaleBase    __asm("a6"),
                               register CONST struct Catalog  *catalog       __asm("a0"),
                               register LONG                   stringNum     __asm("d0"),
                               register CONST_STRPTR           defaultString __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_locale: GetCatalogStr() called stringNum=%ld\n", stringNum);
    /* No catalog support - just return default */
    return (STRPTR)defaultString;
}

STRPTR _locale_GetLocaleStr ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                              register struct Locale       *locale     __asm("a0"),
                              register ULONG                stringNum  __asm("d0"))
{
    DPRINTF (LOG_DEBUG, "_locale: GetLocaleStr() called stringNum=%ld\n", stringNum);
    
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
    DPRINTF (LOG_DEBUG, "_locale: OpenCatalogA() called name='%s'\n", name ? (char *)name : "(null)");
    /* Return NULL - no catalog support yet */
    return NULL;
}

struct Locale * _locale_OpenLocale ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                                     register CONST_STRPTR         name       __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_locale: OpenLocale() called name='%s'\n", name ? (char *)name : "(null)");
    /* Always return the default locale */
    return &g_DefaultLocale;
}

BOOL _locale_ParseDate ( register struct MyLocaleBase    *LocaleBase  __asm("a6"),
                         register CONST struct Locale    *locale      __asm("a0"),
                         register struct DateStamp       *date        __asm("a1"),
                         register CONST_STRPTR            fmtTemplate __asm("a2"),
                         register struct Hook            *getCharFunc __asm("a3"))
{
    DPRINTF (LOG_DEBUG, "_locale: ParseDate() unimplemented STUB called\n");
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
    
    if (!string || !buffer || bufferSize == 0)
        return 0;
    
    /* Simple copy for now */
    UBYTE *dst = (UBYTE *)buffer;
    const UBYTE *src = (const UBYTE *)string;
    ULONG count = 0;
    
    while (*src && count < bufferSize - 1)
    {
        *dst++ = *src++;
        count++;
    }
    *dst = '\0';
    
    return count;
}

LONG _locale_StrnCmp ( register struct MyLocaleBase *LocaleBase __asm("a6"),
                       register struct Locale       *locale     __asm("a0"),
                       register CONST_STRPTR         string1    __asm("a1"),
                       register CONST_STRPTR         string2    __asm("a2"),
                       register LONG                 length     __asm("d0"),
                       register ULONG                type       __asm("d1"))
{
    if (!string1 && !string2)
        return 0;
    if (!string1)
        return -1;
    if (!string2)
        return 1;
    
    const UBYTE *s1 = (const UBYTE *)string1;
    const UBYTE *s2 = (const UBYTE *)string2;
    
    while (length > 0 && *s1 && *s2)
    {
        LONG c1 = *s1++;
        LONG c2 = *s2++;
        
        /* For SC_COLLATE1/2, do case-insensitive comparison */
        if (type != SC_ASCII)
        {
            if (c1 >= 'A' && c1 <= 'Z')
                c1 += 'a' - 'A';
            if (c2 >= 'A' && c2 <= 'Z')
                c2 += 'a' - 'A';
        }
        
        if (c1 != c2)
            return c1 - c2;
        
        length--;
    }
    
    if (length == 0)
        return 0;
    
    return *s1 - *s2;
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
    _locale_StrConvert,               // -168 (0xae) pos 27
    _locale_StrnCmp,                  // -174 (0xb4) pos 28
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
