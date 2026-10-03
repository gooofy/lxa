/*
 * Probe (Phase 222f): locale.library - the default Locale (OpenLocale(NULL)),
 * GetLocaleStr for every string id, the character classification and case
 * conversion tables, StrnCmp, StrConvert and the catalog functions with
 * missing catalogs.  Output compared with AmigaOS 3.1.
 */
#include "locprobe.h"

static void p_grouping(const char *label, const UBYTE *g)
{
    int i;
    probe_s(label);
    probe_s(" =");
    if (!g) {
        probe_s(" NULL\n");
        return;
    }
    for (i = 0; i < 8; i++) {
        probe_ch(' ');
        probe_dec(g[i]);
        if (g[i] == 0 || g[i] == 255)
            break;
    }
    probe_ch('\n');
}

static void p_classtable(const char *label, struct Locale *loc, ULONG (*fn)(struct Locale *, ULONG))
{
    UBYTE bits[32];
    int c;
    for (c = 0; c < 32; c++)
        bits[c] = 0;
    for (c = 0; c < 256; c++)
        if (fn(loc, c))
            bits[c >> 3] |= (UBYTE)(1 << (c & 7));
    P_BYTES(label, bits, 32);
}

/* wrappers so the inline macros can be passed as functions */
static ULONG f_alnum(struct Locale *l, ULONG c) { return IsAlNum(l, c); }
static ULONG f_alpha(struct Locale *l, ULONG c) { return IsAlpha(l, c); }
static ULONG f_cntrl(struct Locale *l, ULONG c) { return IsCntrl(l, c); }
static ULONG f_digit(struct Locale *l, ULONG c) { return IsDigit(l, c); }
static ULONG f_graph(struct Locale *l, ULONG c) { return IsGraph(l, c); }
static ULONG f_lower(struct Locale *l, ULONG c) { return IsLower(l, c); }
static ULONG f_print(struct Locale *l, ULONG c) { return IsPrint(l, c); }
static ULONG f_punct(struct Locale *l, ULONG c) { return IsPunct(l, c); }
static ULONG f_space(struct Locale *l, ULONG c) { return IsSpace(l, c); }
static ULONG f_upper(struct Locale *l, ULONG c) { return IsUpper(l, c); }
static ULONG f_xdigit(struct Locale *l, ULONG c) { return IsXDigit(l, c); }

static const char *const cmp_pairs[][2] = {
    {"abc", "abc"}, {"abc", "ABC"}, {"ABC", "abc"}, {"abc", "abd"}, {"abd", "abc"},
    {"", ""}, {"a", ""}, {"", "a"}, {"abc", "abcd"}, {"abcd", "abc"},
    {"a", "B"}, {"B", "a"}, {"Z", "a"}, {"[", "a"}, {"_", "A"}, {"1", "a"}, {" ", "!"},
    {"\xe4", "a"}, {"\xe4", "b"}, {"\xc4", "\xe4"}, {"\xe9", "e"}, {"\xe9", "f"},
    {"\xdf", "ss"}, {"\xdf", "st"}, {"\xf8", "o"}, {"\xf8", "p"}, {"\xe6", "ae"},
    {"\xff", "y"}, {"\xd0", "d"}, {"\xfe", "t"}, {"cote", "c\xf4te"}, {"c\xf4te", "cot\xe9"},
    {"Amiga", "amiga"}, {"amiga", "Amiga"}, {"a-b", "ab"}, {"a b", "ab"}, {"\x80", "\xa0"},
    {"\x01", "\x02"}, {"x\x01", "x"}, {"MUELLER", "M\xdcLLER"}, {"Mueller", "M\xfcller"},
};

static LONG sign(LONG v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }

int main(void)
{
    struct Locale *loc;
    int i, t;

    if (!open_locale_lib())
        return 20;
    P_LONG("lib_Version >= 38", LocaleBase->lb_LibNode.lib_Version >= 38);

    loc = OpenLocale(NULL);
    P_NULL("OpenLocale(NULL)", loc);
    if (!loc)
        return 20;

    P_SECTION("default locale");
    P_QS("loc_LocaleName", (UBYTE *)loc->loc_LocaleName);
    P_QS("loc_LanguageName", (UBYTE *)loc->loc_LanguageName);
    for (i = 0; i < 10; i++) {
        probe_s("loc_PrefLanguages[");
        probe_dec(i);
        probe_s("] = ");
        probe_qs((UBYTE *)loc->loc_PrefLanguages[i]);
        probe_ch('\n');
        if (!loc->loc_PrefLanguages[i])
            break;
    }
    P_HEX("loc_Flags", loc->loc_Flags);
    P_HEX("loc_CodeSet", loc->loc_CodeSet);
    P_HEX("loc_CountryCode", loc->loc_CountryCode);
    P_LONG("loc_TelephoneCode", loc->loc_TelephoneCode);
    P_LONG("loc_GMTOffset", loc->loc_GMTOffset);
    P_LONG("loc_MeasuringSystem", loc->loc_MeasuringSystem);
    P_LONG("loc_CalendarType", loc->loc_CalendarType);
    P_QS("loc_DateTimeFormat", (UBYTE *)loc->loc_DateTimeFormat);
    P_QS("loc_DateFormat", (UBYTE *)loc->loc_DateFormat);
    P_QS("loc_TimeFormat", (UBYTE *)loc->loc_TimeFormat);
    P_QS("loc_ShortDateTimeFormat", (UBYTE *)loc->loc_ShortDateTimeFormat);
    P_QS("loc_ShortDateFormat", (UBYTE *)loc->loc_ShortDateFormat);
    P_QS("loc_ShortTimeFormat", (UBYTE *)loc->loc_ShortTimeFormat);
    P_QS("loc_DecimalPoint", (UBYTE *)loc->loc_DecimalPoint);
    P_QS("loc_GroupSeparator", (UBYTE *)loc->loc_GroupSeparator);
    P_QS("loc_FracGroupSeparator", (UBYTE *)loc->loc_FracGroupSeparator);
    p_grouping("loc_Grouping", loc->loc_Grouping);
    p_grouping("loc_FracGrouping", loc->loc_FracGrouping);
    P_QS("loc_MonDecimalPoint", (UBYTE *)loc->loc_MonDecimalPoint);
    P_QS("loc_MonGroupSeparator", (UBYTE *)loc->loc_MonGroupSeparator);
    P_QS("loc_MonFracGroupSeparator", (UBYTE *)loc->loc_MonFracGroupSeparator);
    p_grouping("loc_MonGrouping", loc->loc_MonGrouping);
    p_grouping("loc_MonFracGrouping", loc->loc_MonFracGrouping);
    P_LONG("loc_MonFracDigits", loc->loc_MonFracDigits);
    P_LONG("loc_MonIntFracDigits", loc->loc_MonIntFracDigits);
    P_QS("loc_MonCS", (UBYTE *)loc->loc_MonCS);
    P_QS("loc_MonSmallCS", (UBYTE *)loc->loc_MonSmallCS);
    P_QS("loc_MonIntCS", (UBYTE *)loc->loc_MonIntCS);
    P_QS("loc_MonPositiveSign", (UBYTE *)loc->loc_MonPositiveSign);
    P_LONG("loc_MonPositiveSpaceSep", loc->loc_MonPositiveSpaceSep);
    P_LONG("loc_MonPositiveSignPos", loc->loc_MonPositiveSignPos);
    P_LONG("loc_MonPositiveCSPos", loc->loc_MonPositiveCSPos);
    P_QS("loc_MonNegativeSign", (UBYTE *)loc->loc_MonNegativeSign);
    P_LONG("loc_MonNegativeSpaceSep", loc->loc_MonNegativeSpaceSep);
    P_LONG("loc_MonNegativeSignPos", loc->loc_MonNegativeSignPos);
    P_LONG("loc_MonNegativeCSPos", loc->loc_MonNegativeCSPos);

    P_SECTION("OpenLocale variants");
    {
        struct Locale *l2 = OpenLocale(NULL);
        P_NULL("OpenLocale(NULL) again", l2);
        if (l2) {
            P_QS("  name", (UBYTE *)l2->loc_LocaleName);
            CloseLocale(l2);
        }
        l2 = OpenLocale((STRPTR)"lxa_no_such_locale.prefs");
        P_NULL("OpenLocale(missing file)", l2);
        if (l2)
            CloseLocale(l2);
        CloseLocale(NULL);
        P_LONG("CloseLocale(NULL) survived", 1);
    }

    P_SECTION("GetLocaleStr");
    for (i = 0; i <= 50; i++) {
        probe_s("GetLocaleStr(");
        probe_dec(i);
        probe_s(") = ");
        probe_qs((UBYTE *)GetLocaleStr(loc, i));
        probe_ch('\n');
    }

    P_SECTION("character classes");
    p_classtable("IsAlNum", loc, f_alnum);
    p_classtable("IsAlpha", loc, f_alpha);
    p_classtable("IsCntrl", loc, f_cntrl);
    p_classtable("IsDigit", loc, f_digit);
    p_classtable("IsGraph", loc, f_graph);
    p_classtable("IsLower", loc, f_lower);
    p_classtable("IsPrint", loc, f_print);
    p_classtable("IsPunct", loc, f_punct);
    p_classtable("IsSpace", loc, f_space);
    p_classtable("IsUpper", loc, f_upper);
    p_classtable("IsXDigit", loc, f_xdigit);

    P_SECTION("ConvToLower/ConvToUpper");
    {
        UBYTE up[256], lo[256];
        for (i = 0; i < 256; i++) {
            up[i] = (UBYTE)ConvToUpper(loc, i);
            lo[i] = (UBYTE)ConvToLower(loc, i);
        }
        for (i = 0; i < 256; i += 64) {
            probe_s("ConvToUpper ");
            probe_hex(i, 2);
            P_BYTES("", up + i, 64);
        }
        for (i = 0; i < 256; i += 64) {
            probe_s("ConvToLower ");
            probe_hex(i, 2);
            P_BYTES("", lo + i, 64);
        }
    }

    P_SECTION("StrnCmp");
    for (t = SC_ASCII; t <= SC_COLLATE2; t++) {
        for (i = 0; i < (int)(sizeof(cmp_pairs) / sizeof(cmp_pairs[0])); i++) {
            probe_s("StrnCmp type ");
            probe_dec(t);
            probe_s(" ");
            probe_qs((const UBYTE *)cmp_pairs[i][0]);
            probe_s(" ");
            probe_qs((const UBYTE *)cmp_pairs[i][1]);
            probe_s(": -1 ");
            probe_dec(sign(StrnCmp(loc, (STRPTR)cmp_pairs[i][0], (STRPTR)cmp_pairs[i][1], -1, t)));
            probe_s(" 0 ");
            probe_dec(sign(StrnCmp(loc, (STRPTR)cmp_pairs[i][0], (STRPTR)cmp_pairs[i][1], 0, t)));
            probe_s(" 1 ");
            probe_dec(sign(StrnCmp(loc, (STRPTR)cmp_pairs[i][0], (STRPTR)cmp_pairs[i][1], 1, t)));
            probe_s(" 3 ");
            probe_dec(sign(StrnCmp(loc, (STRPTR)cmp_pairs[i][0], (STRPTR)cmp_pairs[i][1], 3, t)));
            probe_ch('\n');
        }
    }

    P_SECTION("StrConvert");
    {
        static const char *const strs[] = {
            "abc", "ABC", "Hello World", "", "\xe4\xf6\xfc\xdf", "\xc4\xd6\xdc", "a1_B2-c3",
            "\xe9t\xe9", "zZ09", "\x01\x7f\x80\xff",
        };
        static const ULONG sizes[] = {0, 1, 2, 4, 64};
        UBYTE buf[80];
        int s, z;
        for (t = SC_ASCII; t <= SC_COLLATE2; t++) {
            for (s = 0; s < (int)(sizeof(strs) / sizeof(strs[0])); s++) {
                for (z = 0; z < (int)(sizeof(sizes) / sizeof(sizes[0])); z++) {
                    ULONG r;
                    int k;
                    for (k = 0; k < 80; k++)
                        buf[k] = 0xaa;
                    r = StrConvert(loc, (STRPTR)strs[s], buf, sizes[z], t);
                    probe_s("StrConvert type ");
                    probe_dec(t);
                    probe_s(" ");
                    probe_qs((const UBYTE *)strs[s]);
                    probe_s(" size ");
                    probe_dec((LONG)sizes[z]);
                    probe_s(" -> ");
                    probe_dec((LONG)r);
                    P_BYTES(" buf", buf, (LONG)(sizes[z] < 24 ? sizes[z] + 1 : 24));
                }
            }
        }
    }

    P_SECTION("StrConvert per character");
    {
        UBYTE in[2], buf[8], tab[256];
        int k;
        for (t = SC_ASCII; t <= SC_COLLATE2; t++) {
            UBYTE len2[256];
            for (i = 1; i < 256; i++) {
                ULONG r;
                in[0] = (UBYTE)i;
                in[1] = 0;
                for (k = 0; k < 8; k++)
                    buf[k] = 0xaa;
                r = StrConvert(loc, in, buf, 8, t);
                tab[i] = buf[0];
                len2[i] = (UBYTE)((r & 15) << 4 | (buf[1] == 0 ? 0 : buf[2] == 0 ? 1 : 2));
                if (t == SC_COLLATE2 && r != 2) {
                    /* only characters with a secondary key */
                    if (!(r == 3 && buf[1] == (UBYTE)i && buf[2] == 0)) {
                        probe_s("  odd StrConvert type 2 char ");
                        probe_hex(i, 2);
                        P_BYTES("", buf, 5);
                    }
                }
            }
            tab[0] = 0;
            len2[0] = 0;
            for (i = 0; i < 256; i += 64) {
                probe_s("StrConvert type ");
                probe_dec(t);
                probe_s(" key ");
                probe_hex(i, 2);
                P_BYTES("", tab + i, 64);
            }
            for (i = 0; i < 256; i += 64) {
                probe_s("StrConvert type ");
                probe_dec(t);
                probe_s(" len ");
                probe_hex(i, 2);
                P_BYTES("", len2 + i, 64);
            }
        }
    }

    P_SECTION("StrConvert sizes");
    {
        static const char *const strs[] = {"abc", "aBc", "ABC", "a", "A", "Ab", "aB", "\xe4X"};
        UBYTE buf[16];
        int s, z, k;
        for (t = SC_ASCII; t <= SC_COLLATE2; t++) {
            for (s = 0; s < (int)(sizeof(strs) / sizeof(strs[0])); s++) {
                for (z = 0; z <= 8; z++) {
                    ULONG r;
                    for (k = 0; k < 16; k++)
                        buf[k] = 0xaa;
                    r = StrConvert(loc, (STRPTR)strs[s], buf, z, t);
                    probe_s("StrConvert type ");
                    probe_dec(t);
                    probe_s(" ");
                    probe_qs((const UBYTE *)strs[s]);
                    probe_s(" size ");
                    probe_dec(z);
                    probe_s(" -> ");
                    probe_dec((LONG)r);
                    P_BYTES(" buf", buf, 10);
                }
            }
        }
    }

    P_SECTION("StrnCmp per character");
    {
        /* compare each character against its neighbours and 'a'/'A' */
        UBYTE a[2], b[2], row[64];
        int n;
        a[1] = b[1] = 0;
        for (t = SC_ASCII; t <= SC_COLLATE2; t++) {
            for (i = 0; i < 256; i += 64) {
                for (n = 0; n < 64; n++) {
                    LONG r1, r2;
                    a[0] = (UBYTE)(i + n);
                    b[0] = 'a';
                    r1 = sign(StrnCmp(loc, a, b, -1, t));
                    b[0] = 'A';
                    r2 = sign(StrnCmp(loc, a, b, -1, t));
                    row[n] = (UBYTE)((r1 + 1) * 3 + (r2 + 1));
                }
                probe_s("StrnCmp type ");
                probe_dec(t);
                probe_s(" vs a/A ");
                probe_hex(i, 2);
                P_BYTES("", row, 64);
            }
        }
    }

    P_SECTION("catalogs");
    {
        static UBYTE dflt[] = "built-in default";
        struct Catalog *cat;
        STRPTR s;
        cat = OpenCatalogA(NULL, (STRPTR)"lxa_no_such.catalog", NULL);
        P_NULL("OpenCatalogA(NULL, missing)", cat);
        if (cat)
            CloseCatalog(cat);
        cat = OpenCatalogA(loc, (STRPTR)"lxa_no_such.catalog", NULL);
        P_NULL("OpenCatalogA(loc, missing)", cat);
        if (cat)
            CloseCatalog(cat);
        {
            struct TagItem tags[3];
            tags[0].ti_Tag = OC_BuiltInLanguage;
            tags[0].ti_Data = (ULONG)"english";
            tags[1].ti_Tag = OC_Version;
            tags[1].ti_Data = 1;
            tags[2].ti_Tag = TAG_DONE;
            cat = OpenCatalogA(loc, (STRPTR)"lxa_no_such.catalog", tags);
            P_NULL("OpenCatalogA(builtin english, missing)", cat);
            if (cat)
                CloseCatalog(cat);
            tags[0].ti_Data = (ULONG)"deutsch";
            cat = OpenCatalogA(loc, (STRPTR)"lxa_no_such.catalog", tags);
            P_NULL("OpenCatalogA(builtin deutsch, missing)", cat);
            if (cat)
                CloseCatalog(cat);
            tags[0].ti_Tag = OC_Language;
            tags[0].ti_Data = (ULONG)"lxa_no_language";
            cat = OpenCatalogA(loc, (STRPTR)"lxa_no_such.catalog", tags);
            P_NULL("OpenCatalogA(language missing, missing)", cat);
            if (cat)
                CloseCatalog(cat);
        }
        s = GetCatalogStr(NULL, 1, dflt);
        P_QS("GetCatalogStr(NULL, 1, dflt)", (UBYTE *)s);
        P_LONG("  returns the default pointer", s == (STRPTR)dflt);
        s = GetCatalogStr(NULL, -1, dflt);
        P_LONG("GetCatalogStr(NULL, -1, dflt) is default", s == (STRPTR)dflt);
        s = GetCatalogStr(NULL, 0, (STRPTR)"");
        P_QS("GetCatalogStr(NULL, 0, \"\")", (UBYTE *)s);
        CloseCatalog(NULL);
        P_LONG("CloseCatalog(NULL) survived", 1);
    }

    CloseLocale(loc);
    CloseLibrary((struct Library *)LocaleBase);
    return 0;
}
