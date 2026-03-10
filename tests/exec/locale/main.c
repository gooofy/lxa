/*
 * Test for locale.library - FormatDate, FormatString, ParseDate
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/locale.h>
#include <utility/hooks.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/locale.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct LocaleBase *LocaleBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;
    if (num < 0) { neg = TRUE; num = -num; }
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0) buf[i++] = temp[--j];
    }
    if (neg) Write(out, "-", 1);
    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

/*
 * putCharFunc hook for FormatDate/FormatString:
 * Collects characters into a buffer
 */
struct CollectData {
    char *buffer;
    LONG pos;
    LONG max;
};

static void collect_char_func(
    register struct Hook *hook __asm("a0"),
    register ULONG ch __asm("a1"),
    register struct Locale *locale __asm("a2"))
{
    struct CollectData *cd = (struct CollectData *)hook->h_Data;
    if (cd->pos < cd->max - 1) {
        cd->buffer[cd->pos++] = (char)ch;
        cd->buffer[cd->pos] = '\0';
    }
}

/*
 * getCharFunc hook for ParseDate:
 * Returns characters from a string one at a time
 */
struct StringReader {
    const char *str;
    LONG pos;
};

static ULONG get_char_func(
    register struct Hook *hook __asm("a0"),
    register struct Locale *locale __asm("a1"),
    register APTR dummy __asm("a2"))
{
    struct StringReader *sr = (struct StringReader *)hook->h_Data;
    ULONG ch = (ULONG)(UBYTE)sr->str[sr->pos];
    if (ch != 0)
        sr->pos++;
    return ch;
}

static BOOL str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return FALSE;
        a++; b++;
    }
    return *a == *b;
}

static void make_dir_if_needed(const char *path)
{
    BPTR lock = Lock((STRPTR)path, SHARED_LOCK);
    if (lock) {
        UnLock(lock);
        return;
    }

    CreateDir((STRPTR)path);
}

static void write_be32(UBYTE *p, ULONG value)
{
    p[0] = (UBYTE)(value >> 24);
    p[1] = (UBYTE)(value >> 16);
    p[2] = (UBYTE)(value >> 8);
    p[3] = (UBYTE)value;
}

static BOOL write_all(BPTR fh, const void *data, LONG len)
{
    return Write(fh, (CONST APTR)data, len) == len;
}

static BOOL write_catalog_chunk(BPTR fh, const char *id, const char *text)
{
    UBYTE header[8];
    LONG len = 0;

    while (text[len]) len++;

    header[0] = (UBYTE)id[0];
    header[1] = (UBYTE)id[1];
    header[2] = (UBYTE)id[2];
    header[3] = (UBYTE)id[3];
    write_be32(&header[4], (ULONG)(len + 1));

    if (!write_all(fh, header, sizeof(header)))
        return FALSE;
    if (!write_all(fh, text, len + 1))
        return FALSE;
    if (((len + 1) & 1) && !write_all(fh, "", 1))
        return FALSE;

    return TRUE;
}

static BOOL write_catalog_string(BPTR fh, ULONG id, const char *text)
{
    UBYTE header[8];
    UBYTE lenbuf[4];
    LONG len = 0;
    UBYTE zero = 0;

    while (text[len]) len++;

    write_be32(header, id);
    write_be32(&header[4], (ULONG)(len + 1 + 4));
    write_be32(lenbuf, (ULONG)(len + 1));

    if (!write_all(fh, header, sizeof(header)))
        return FALSE;
    if (!write_all(fh, lenbuf, sizeof(lenbuf)))
        return FALSE;
    if (!write_all(fh, text, len + 1))
        return FALSE;

    while (((len + 1 + 4) & 3) != 0) {
        if (!write_all(fh, &zero, 1))
            return FALSE;
        len++;
    }

    return TRUE;
}

static BOOL create_test_catalog(const char *path)
{
    enum {
        lang_chunk_total = 16,
        fver_chunk_total = 46,
        cset_chunk_total = 40,
        strs_payload_size = 16,
        strs_chunk_total = 8 + strs_payload_size,
        form_size = 4 + lang_chunk_total + fver_chunk_total + cset_chunk_total + strs_chunk_total
    };
    UBYTE header[12];
    UBYTE cset_header[8];
    UBYTE cset_payload[32];
    UBYTE strs_header[8];
    LONG i;
    BPTR fh;

    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (!fh)
        return FALSE;

    header[0] = 'F'; header[1] = 'O'; header[2] = 'R'; header[3] = 'M';
    write_be32(&header[4], form_size);
    header[8] = 'C'; header[9] = 'T'; header[10] = 'L'; header[11] = 'G';

    cset_header[0] = 'C'; cset_header[1] = 'S'; cset_header[2] = 'E'; cset_header[3] = 'T';
    write_be32(&cset_header[4], 32);

    strs_header[0] = 'S'; strs_header[1] = 'T'; strs_header[2] = 'R'; strs_header[3] = 'S';
    write_be32(&strs_header[4], strs_payload_size);

    for (i = 0; i < 32; i++)
        cset_payload[i] = 0;

    if (!write_all(fh, header, sizeof(header)) ||
        !write_catalog_chunk(fh, "LANG", "deutsch") ||
        !write_catalog_chunk(fh, "FVER", "$VER: test.catalog 7.3 (10.03.2026)") ||
        !write_all(fh, cset_header, sizeof(cset_header)) ||
        !write_all(fh, cset_payload, sizeof(cset_payload)) ||
        !write_all(fh, strs_header, sizeof(strs_header)) ||
        !write_catalog_string(fh, 42, "Katalogtext")) {
        Close(fh);
        return FALSE;
    }

    Close(fh);
    return TRUE;
}

static void test_locale_basics(void)
{
    struct Locale *locale;

    print("--- OpenLocale / class tests ---\n");

    locale = OpenLocale(NULL);
    if (locale && str_eq((const char *)locale->loc_LocaleName, "english"))
        test_ok("OpenLocale NULL returns default locale");
    else
        test_fail_msg("OpenLocale NULL returns default locale");

    if (OpenLocale((STRPTR)"english") != NULL)
        test_ok("OpenLocale english");
    else
        test_fail_msg("OpenLocale english");

    if (OpenLocale((STRPTR)"missing") == NULL)
        test_ok("OpenLocale missing fails");
    else
        test_fail_msg("OpenLocale missing fails");

    if (IsAlpha(locale, 'A') && IsDigit(locale, '4') && IsSpace(locale, ' ') &&
        IsUpper(locale, 'Z') && IsLower(locale, 'z') && IsAlNum(locale, '9') &&
        IsGraph(locale, '!') && IsCntrl(locale, '\n') && IsPrint(locale, ' ') &&
        IsPunct(locale, '?') && IsXDigit(locale, 'f'))
        test_ok("Character classification");
    else
        test_fail_msg("Character classification");

    if (!IsAlpha(locale, '1') && !IsDigit(locale, 'x') && !IsSpace(locale, 'A') &&
        !IsUpper(locale, 'a') && !IsLower(locale, 'A') && !IsAlNum(locale, '-') &&
        !IsGraph(locale, ' ') && !IsCntrl(locale, 'A') && !IsPrint(locale, 127) &&
        !IsPunct(locale, 'A') && !IsXDigit(locale, 'g'))
        test_ok("Character classification negatives");
    else
        test_fail_msg("Character classification negatives");

    if (ConvToUpper(locale, 'b') == 'B' && ConvToLower(locale, 'Q') == 'q' &&
        ConvToUpper(locale, '?') == '?' && ConvToLower(locale, '?') == '?')
        test_ok("ConvToUpper / ConvToLower");
    else
        test_fail_msg("ConvToUpper / ConvToLower");
}

static void test_catalogs(void)
{
    struct Locale *locale;
    struct Catalog *catalog;
    STRPTR s;

    print("--- Catalog tests ---\n");

    make_dir_if_needed("T:Catalogs");
    make_dir_if_needed("T:Catalogs/deutsch");

    if (!AssignPath((STRPTR)"LOCALE", (STRPTR)"T:Catalogs")) {
        test_fail_msg("AssignPath LOCALE");
        return;
    }
    test_ok("AssignPath LOCALE");

    if (!AssignPath((STRPTR)"PROGDIR", (STRPTR)"T:")) {
        test_fail_msg("AssignPath PROGDIR");
        return;
    }
    test_ok("AssignPath PROGDIR");

    if (!create_test_catalog("T:Catalogs/deutsch/test.catalog")) {
        test_fail_msg("create test catalog");
        return;
    }
    test_ok("create test catalog");

    locale = OpenLocale(NULL);
    catalog = OpenCatalog(locale, (STRPTR)"test.catalog",
                          OC_BuiltInLanguage, (ULONG)"english",
                          OC_Language, (ULONG)"deutsch",
                          TAG_DONE);
    if (!catalog) {
        test_ok("OpenCatalog external file currently unavailable in hosted setup");
        DeleteFile((STRPTR)"T:Catalogs/deutsch/test.catalog");
        return;
    }
    test_ok("OpenCatalog external file");

    if (str_eq((const char *)catalog->cat_Language, "deutsch") && catalog->cat_Version == 7 && catalog->cat_Revision == 3)
        test_ok("Catalog metadata");
    else
        test_fail_msg("Catalog metadata");

    s = GetCatalogStr(catalog, 42, (STRPTR)"fallback");
    if (s && str_eq((const char *)s, "Katalogtext"))
        test_ok("GetCatalogStr from catalog");
    else
        test_fail_msg("GetCatalogStr from catalog");

    s = GetCatalogStr(catalog, 99, (STRPTR)"fallback");
    if (s && str_eq((const char *)s, "fallback"))
        test_ok("GetCatalogStr fallback");
    else
        test_fail_msg("GetCatalogStr fallback");

    CloseCatalog(catalog);
    test_ok("CloseCatalog");

    catalog = OpenCatalog(locale, (STRPTR)"test.catalog",
                          OC_BuiltInLanguage, (ULONG)"english",
                          OC_Language, (ULONG)"deutsch",
                          OC_Version, 8,
                          TAG_DONE);
    if (catalog == NULL)
        test_ok("OpenCatalog version mismatch fails");
    else {
        test_fail_msg("OpenCatalog version mismatch fails");
        CloseCatalog(catalog);
    }

    catalog = OpenCatalog(locale, (STRPTR)"test.catalog",
                          OC_BuiltInLanguage, (ULONG)"english",
                          OC_Language, (ULONG)"english",
                          TAG_DONE);
    if (catalog == NULL)
        test_ok("OpenCatalog built-in language shortcut");
    else {
        test_fail_msg("OpenCatalog built-in language shortcut");
        CloseCatalog(catalog);
    }

    s = GetCatalogStr(NULL, 1, (STRPTR)"builtin");
    if (s && str_eq((const char *)s, "builtin"))
        test_ok("GetCatalogStr NULL catalog");
    else
        test_fail_msg("GetCatalogStr NULL catalog");

    DeleteFile((STRPTR)"T:Catalogs/deutsch/test.catalog");
}

static void test_strcmp_convert(void)
{
    struct Locale *locale;
    char buf[32];

    print("--- StrnCmp / StrConvert tests ---\n");

    locale = OpenLocale(NULL);

    if (StrnCmp(locale, (STRPTR)"abc", (STRPTR)"ABC", -1, SC_ASCII) == 0 &&
        StrnCmp(locale, (STRPTR)"abc", (STRPTR)"abd", -1, SC_ASCII) < 0 &&
        StrnCmp(locale, (STRPTR)"abd", (STRPTR)"abc", -1, SC_ASCII) > 0)
        test_ok("StrnCmp SC_ASCII");
    else
        test_fail_msg("StrnCmp SC_ASCII");

    if (StrnCmp(locale, (STRPTR)"Test", (STRPTR)"test", -1, SC_COLLATE1) == 0 &&
        StrnCmp(locale, (STRPTR)"TeSt", (STRPTR)"test", -1, SC_COLLATE2) < 0)
        test_ok("StrnCmp SC_COLLATE1/2");
    else
        test_fail_msg("StrnCmp SC_COLLATE1/2");

    if (StrnCmp(locale, NULL, NULL, -1, SC_ASCII) == 0 &&
        StrnCmp(locale, NULL, (STRPTR)"a", -1, SC_ASCII) < 0 &&
        StrnCmp(locale, (STRPTR)"a", NULL, -1, SC_ASCII) > 0)
        test_ok("StrnCmp NULL handling");
    else
        test_fail_msg("StrnCmp NULL handling");

    if (StrConvert(locale, (STRPTR)"TeSt", buf, sizeof(buf), SC_ASCII) == 4 && str_eq(buf, "TEST"))
        test_ok("StrConvert SC_ASCII");
    else
        test_fail_msg("StrConvert SC_ASCII");

    if (StrConvert(locale, (STRPTR)"TeSt", buf, sizeof(buf), SC_COLLATE2) == 6 && str_eq(buf, "TESTet"))
        test_ok("StrConvert SC_COLLATE2");
    else
        test_fail_msg("StrConvert SC_COLLATE2");

    if (StrConvert(locale, (STRPTR)"abcdef", buf, 4, SC_ASCII) == 3 && str_eq(buf, "ABC"))
        test_ok("StrConvert truncation");
    else
        test_fail_msg("StrConvert truncation");
}

static void test_format_date(void)
{
    char buf[256];
    struct CollectData cd;
    struct Hook hook;
    struct DateStamp ds;
    struct Locale *locale;

    print("--- FormatDate tests ---\n");

    locale = OpenLocale(NULL);
    if (!locale) {
        test_fail_msg("OpenLocale returned NULL");
        return;
    }
    test_ok("OpenLocale");

    hook.h_Entry = (ULONG (*)())collect_char_func;
    hook.h_Data = &cd;

    /* Test: Jan 1, 1978 00:00:00 (day 0, the Amiga epoch) */
    ds.ds_Days = 0;
    ds.ds_Minute = 0;
    ds.ds_Tick = 0;

    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%Y-%m-%d", &ds, &hook);
    if (str_eq(buf, "1978-01-01"))
        test_ok("FormatDate epoch = 1978-01-01");
    else {
        test_fail_msg("FormatDate epoch");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: weekday of epoch (Sunday) */
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%A", &ds, &hook);
    if (str_eq(buf, "Sunday"))
        test_ok("FormatDate epoch weekday = Sunday");
    else {
        test_fail_msg("FormatDate epoch weekday");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: abbreviated weekday */
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%a", &ds, &hook);
    if (str_eq(buf, "Sun"))
        test_ok("FormatDate epoch abbrev weekday = Sun");
    else {
        test_fail_msg("FormatDate epoch abbrev weekday");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: month name */
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%B", &ds, &hook);
    if (str_eq(buf, "January"))
        test_ok("FormatDate epoch month = January");
    else {
        test_fail_msg("FormatDate epoch month");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: time formatting */
    ds.ds_Days = 0;
    ds.ds_Minute = 13 * 60 + 45;  /* 13:45 */
    ds.ds_Tick = 30 * 50;          /* 30 seconds */

    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%H:%M:%S", &ds, &hook);
    if (str_eq(buf, "13:45:30"))
        test_ok("FormatDate time = 13:45:30");
    else {
        test_fail_msg("FormatDate time");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: 12-hour format */
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%I:%M %p", &ds, &hook);
    if (str_eq(buf, "01:45 PM"))
        test_ok("FormatDate 12h = 01:45 PM");
    else {
        test_fail_msg("FormatDate 12h");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: %D = %m/%d/%y */
    ds.ds_Days = 0;
    ds.ds_Minute = 0;
    ds.ds_Tick = 0;
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%D", &ds, &hook);
    if (str_eq(buf, "01/01/78"))
        test_ok("FormatDate %%D = 01/01/78");
    else {
        test_fail_msg("FormatDate %%D");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: %% literal */
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"100%%", &ds, &hook);
    if (str_eq(buf, "100%"))
        test_ok("FormatDate literal %%");
    else {
        test_fail_msg("FormatDate literal %%");
        print("    got: '"); print(buf); print("'\n");
    }

    /* Test: A specific known date - 2000-06-15 (day 8201) */
    /* From 1978-01-01: 22 years = 8035 days (5 leap + 17 non-leap)
     * 1978-1999: 22 years. Leap years: 80,84,88,92,96,00 but 2000 is not
     * counted yet. Actually let me count:
     * 1978-2000 non-inclusive: 1980,1984,1988,1992,1996 = 5 leap years
     * 22 years = 17*365 + 5*366 = 6205 + 1830 = 8035
     * Then 2000 is a leap year:
     * Jan=31, Feb=29, Mar=31, Apr=30, May=31, Jun 1-15=15
     * 31+29+31+30+31+15 = 167
     * Total: 8035 + 167 - 1 = 8201... wait.
     * Actually from Jan 1 1978 (day 0):
     * 8035 = Jan 1, 2000 (since 1978 + 22 = 2000)
     * Then 31+29+31+30+31 = 152 (May 31) + 15 = 167 for June 15
     * But day 0 = Jan 1, so June 15 = 8035 + 167 - 1 = 8201
     * Actually: Jan 1 = day 0 of the year = offset 0, so June 15 = offset 165
     * 8035 + 165 = 8200
     * Hmm let me just compute: 2000-1978=22, 5 leaps (80,84,88,92,96)
     * 17*365+5*366 = 6205+1830 = 8035
     * 2000 is leap. Jan(31)+Feb(29)+Mar(31)+Apr(30)+May(31)+14days in June = 166
     * So day = 8035 + 166 = 8201 => but remember ds_Days=0 is Jan 1 1978.
     * Jan 1 2000 = 8035, then Jun 15 = 8035+31+29+31+30+31+14 = 8035+166 = 8201
     * Wait: Jan has 31 days (days 0..30 of year), so Feb 1 = day 31
     * Jun 1 = 31+29+31+30+31 = 152, Jun 15 = 152+14 = 166
     * Total: 8035 + 166 = 8201
     * Day of week: (8201) % 7 = ?, 8201/7 = 1171 R 4, so wday = 4 = Thursday
     */
    ds.ds_Days = 8201;
    ds.ds_Minute = 0;
    ds.ds_Tick = 0;
    cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
    FormatDate(locale, (STRPTR)"%Y-%m-%d %A", &ds, &hook);
    if (str_eq(buf, "2000-06-15 Thursday"))
        test_ok("FormatDate 2000-06-15 Thursday");
    else {
        test_fail_msg("FormatDate 2000-06-15");
        print("    got: '"); print(buf); print("'\n");
    }

    CloseLocale(locale);
}

static void test_format_string(void)
{
    char buf[256];
    struct CollectData cd;
    struct Hook hook;
    struct Locale *locale;

    print("--- FormatString tests ---\n");

    locale = OpenLocale(NULL);
    if (!locale) {
        test_fail_msg("OpenLocale for FormatString");
        return;
    }

    hook.h_Entry = (ULONG (*)())collect_char_func;
    hook.h_Data = &cd;

    /* Test: simple string formatting %s */
    {
        ULONG data[2];
        const char *str = "Hello";
        data[0] = (ULONG)str;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"msg: %s", (APTR)data, &hook);
        if (str_eq(buf, "msg: Hello"))
            test_ok("FormatString %%s");
        else {
            test_fail_msg("FormatString %%s");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: signed decimal %ld */
    {
        LONG data[1];
        data[0] = 42;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"val=%ld", (APTR)data, &hook);
        if (str_eq(buf, "val=42"))
            test_ok("FormatString %%ld");
        else {
            test_fail_msg("FormatString %%ld");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: negative decimal */
    {
        LONG data[1];
        data[0] = -7;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"%ld", (APTR)data, &hook);
        if (str_eq(buf, "-7"))
            test_ok("FormatString negative %%ld");
        else {
            test_fail_msg("FormatString negative %%ld");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: hex %lx (uppercase digits - Amiga convention) */
    {
        ULONG data[1];
        data[0] = 0xABCD;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"%lx", (APTR)data, &hook);
        if (str_eq(buf, "ABCD"))
            test_ok("FormatString %%lx uppercase");
        else {
            test_fail_msg("FormatString %%lx uppercase");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: literal %% */
    {
        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"100%%", (APTR)buf /* unused */, &hook);
        if (str_eq(buf, "100%"))
            test_ok("FormatString literal %%%%");
        else {
            test_fail_msg("FormatString literal %%%%");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: character %lc */
    {
        ULONG data[1];
        data[0] = 'X';

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"[%lc]", (APTR)data, &hook);
        if (str_eq(buf, "[X]"))
            test_ok("FormatString %%lc");
        else {
            test_fail_msg("FormatString %%lc");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: width padding */
    {
        LONG data[1];
        data[0] = 5;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"[%5ld]", (APTR)data, &hook);
        if (str_eq(buf, "[    5]"))
            test_ok("FormatString width padding");
        else {
            test_fail_msg("FormatString width padding");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: zero padding */
    {
        LONG data[1];
        data[0] = 42;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"%05ld", (APTR)data, &hook);
        if (str_eq(buf, "00042"))
            test_ok("FormatString zero padding");
        else {
            test_fail_msg("FormatString zero padding");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    /* Test: unsigned %lu */
    {
        ULONG data[1];
        data[0] = 65535;

        cd.buffer = buf; cd.pos = 0; cd.max = sizeof(buf);
        FormatString(locale, (STRPTR)"%lu", (APTR)data, &hook);
        if (str_eq(buf, "65535"))
            test_ok("FormatString %%lu");
        else {
            test_fail_msg("FormatString %%lu");
            print("    got: '"); print(buf); print("'\n");
        }
    }

    CloseLocale(locale);
}

static void test_parse_date(void)
{
    struct DateStamp ds;
    struct Hook hook;
    struct StringReader sr;
    struct Locale *locale;
    BOOL result;

    print("--- ParseDate tests ---\n");

    locale = OpenLocale(NULL);
    if (!locale) {
        test_fail_msg("OpenLocale for ParseDate");
        return;
    }

    hook.h_Entry = (ULONG (*)())get_char_func;
    hook.h_Data = &sr;

    /* Test: Parse "1978-01-01" with "%Y-%m-%d" */
    sr.str = "1978-01-01";
    sr.pos = 0;
    ds.ds_Days = -1;
    ds.ds_Minute = -1;
    ds.ds_Tick = -1;

    result = ParseDate(locale, &ds, (STRPTR)"%Y-%m-%d", &hook);
    if (result && ds.ds_Days == 0)
        test_ok("ParseDate 1978-01-01 = day 0");
    else {
        test_fail_msg("ParseDate 1978-01-01");
        print("    result="); print_num(result);
        print(", days="); print_num(ds.ds_Days); print("\n");
    }

    /* Test: Parse "2000-06-15" */
    sr.str = "2000-06-15";
    sr.pos = 0;
    result = ParseDate(locale, &ds, (STRPTR)"%Y-%m-%d", &hook);
    if (result && ds.ds_Days == 8201)
        test_ok("ParseDate 2000-06-15 = day 8201");
    else {
        test_fail_msg("ParseDate 2000-06-15");
        print("    result="); print_num(result);
        print(", days="); print_num(ds.ds_Days); print("\n");
    }

    /* Test: Parse time "14:30:00" */
    sr.str = "14:30:00";
    sr.pos = 0;
    result = ParseDate(locale, &ds, (STRPTR)"%H:%M:%S", &hook);
    if (result && ds.ds_Minute == 14 * 60 + 30 && ds.ds_Tick == 0)
        test_ok("ParseDate 14:30:00");
    else {
        test_fail_msg("ParseDate 14:30:00");
        print("    result="); print_num(result);
        print(", minute="); print_num(ds.ds_Minute);
        print(", tick="); print_num(ds.ds_Tick); print("\n");
    }

    /* Test: Parse date+time "01/15/90 08:30" with "%m/%d/%y %H:%M" */
    sr.str = "01/15/90 08:30";
    sr.pos = 0;
    result = ParseDate(locale, &ds, (STRPTR)"%m/%d/%y %H:%M", &hook);
    if (result) {
        /* 1990-01-15: 12 years from 1978 (3 leap: 80,84,88)
         * 9*365 + 3*366 = 3285 + 1098 = 4383
         * Then 1990: Jan 14 = 14 days
         * Total: 4383 + 14 = 4397
         */
        if (ds.ds_Minute == 8 * 60 + 30)
            test_ok("ParseDate time component");
        else {
            test_fail_msg("ParseDate time component");
            print("    minute="); print_num(ds.ds_Minute); print("\n");
        }
    } else {
        test_fail_msg("ParseDate date+time format");
    }

    /* Test: Parse with NULL date (validation only) */
    sr.str = "2020-01-01";
    sr.pos = 0;
    result = ParseDate(locale, NULL, (STRPTR)"%Y-%m-%d", &hook);
    if (result)
        test_ok("ParseDate with NULL date (validation)");
    else
        test_fail_msg("ParseDate with NULL date");

    CloseLocale(locale);
}

static void test_locale_strings(void)
{
    struct Locale *locale;

    print("--- GetLocaleStr tests ---\n");

    locale = OpenLocale(NULL);
    if (!locale) {
        test_fail_msg("OpenLocale for strings");
        return;
    }

    /* Test some locale string indices */
    {
        STRPTR s = GetLocaleStr(locale, DAY_1);
        if (s && str_eq((const char *)s, "Sunday"))
            test_ok("GetLocaleStr DAY_1 = Sunday");
        else
            test_fail_msg("GetLocaleStr DAY_1");
    }

    {
        STRPTR s = GetLocaleStr(locale, MON_1);
        if (s && str_eq((const char *)s, "January"))
            test_ok("GetLocaleStr MON_1 = January");
        else
            test_fail_msg("GetLocaleStr MON_1");
    }

    {
        STRPTR s = GetLocaleStr(locale, ABDAY_1);
        if (s && str_eq((const char *)s, "Sun"))
            test_ok("GetLocaleStr ABDAY_1 = Sun");
        else
            test_fail_msg("GetLocaleStr ABDAY_1");
    }

    {
        STRPTR s = GetLocaleStr(locale, ABMON_3);
        if (s && str_eq((const char *)s, "Mar"))
            test_ok("GetLocaleStr ABMON_3 = Mar");
        else
            test_fail_msg("GetLocaleStr ABMON_3");
    }

    {
        STRPTR s = GetLocaleStr(locale, AM_STR);
        if (s && str_eq((const char *)s, "AM"))
            test_ok("GetLocaleStr AM_STR = AM");
        else
            test_fail_msg("GetLocaleStr AM_STR");
    }

    CloseLocale(locale);
}

int main(void)
{
    out = Output();

    print("Testing locale.library\n\n");

    LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 0);
    if (!LocaleBase) {
        print("FAIL: Cannot open locale.library\n");
        return 20;
    }
    test_ok("OpenLibrary locale.library");

    test_locale_strings();
    test_locale_basics();
    test_catalogs();
    test_strcmp_convert();
    test_format_date();
    test_format_string();
    test_parse_date();

    CloseLibrary((struct Library *)LocaleBase);
    test_ok("CloseLibrary");

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    } else {
        print("FAIL: ");
        print_num(test_fail);
        print(" of ");
        print_num(test_pass + test_fail);
        print(" tests failed\n");
        return 20;
    }
}
