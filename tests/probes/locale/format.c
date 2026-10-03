/*
 * Probe (Phase 222f): locale.library FormatDate (every % code on several
 * DateStamps), FormatString (every format type, flags, widths, limits,
 * argument positions, the byte stream sent to the hook and the returned
 * data-stream position) and ParseDate (good and bad input, templates,
 * which DateStamp fields are written, how many characters are read).
 * Output compared with AmigaOS 3.1.
 */
#include <dos/dos.h>
#include "locprobe.h"

static struct Locale *loc;

/* ---- FormatDate ---- */

static void fd(const char *fmt, const struct DateStamp *ds)
{
    sink_reset();
    FormatDate(loc, (STRPTR)fmt, (struct DateStamp *)ds, &put_hook);
    probe_s("  ");
    probe_qs((const UBYTE *)fmt);
    probe_s(" -> ");
    sink_print();
    probe_ch('\n');
}

static const struct DateStamp dates[] = {
    {0, 0, 0},           /* 1978-01-01 Sun 00:00:00 */
    {0, 1439, 2999},     /* 1978-01-01 23:59:59 */
    {789, 60, 50},       /* 1980-02-29 01:00:01 */
    {8035, 720, 0},      /* 2000-01-01 12:00:00 */
    {8034, 779, 1249},   /* 1999-12-31 12:59:24 */
    {16801, 754, 1550},  /* 2024-01-01 12:34:31 */
    {11000, 785, 3},     /* 2008-02-13 13:05:00 */
    {29219, 59, 49},     /* 2057-12-31 00:59:00 */
    {5843, 1000, 700},   /* 1993-12-30 16:40:14 */
};

static const char *const date_codes[] = {
    "%a", "%A", "%b", "%B", "%c", "%C", "%d", "%D", "%e", "%h", "%H", "%I", "%j", "%m",
    "%M", "%n", "%p", "%q", "%Q", "%r", "%R", "%S", "%t", "%T", "%U", "%w", "%W", "%x",
    "%X", "%y", "%Y", "%%", "%z", "%E", "%0", "x%", "plain text", "", "%d.%m.%Y %H:%M:%S",
    "%%%d%%", "%A, %B %e, %Y (%j)",
};

/* ---- FormatString ---- */

static UWORD data[32];
static int nd;
static void W(UWORD v) { data[nd++] = v; }
static void L(ULONG v) { data[nd++] = (UWORD)(v >> 16); data[nd++] = (UWORD)v; }

static void fs(const char *fmt)
{
    APTR ret;
    sink_reset();
    ret = FormatString(loc, (STRPTR)fmt, data, &put_hook);
    probe_s("  ");
    probe_qs((const UBYTE *)fmt);
    probe_s(" -> ");
    sink_print();
    probe_s(" used=");
    probe_dec((LONG)((UBYTE *)ret - (UBYTE *)data));
    probe_ch('\n');
    nd = 0;
}

/* ---- ParseDate ---- */

static void pd(const char *tmpl, const char *input)
{
    struct DateStamp ds;
    struct Source src;
    struct Hook h;
    BOOL ok;
    ds.ds_Days = 1111;
    ds.ds_Minute = 222;
    ds.ds_Tick = 333;
    src.p = (const UBYTE *)input;
    src.calls = 0;
    h.h_Entry = (ULONG (*)())probe_getch;
    h.h_SubEntry = 0;
    h.h_Data = &src;
    ok = ParseDate(loc, &ds, (STRPTR)tmpl, &h);
    probe_s("  ");
    probe_qs((const UBYTE *)tmpl);
    probe_s(" ");
    probe_qs((const UBYTE *)input);
    probe_s(" -> ");
    probe_s(ok ? "TRUE" : "FALSE");
    probe_s(" ds=");
    probe_dec(ds.ds_Days);
    probe_ch('/');
    probe_dec(ds.ds_Minute);
    probe_ch('/');
    probe_dec(ds.ds_Tick);
    probe_s(" read=");
    probe_dec((LONG)src.calls);
    probe_ch('\n');
}

int main(void)
{
    static UBYTE str1[] = "hello";
    static UBYTE str2[] = "";
    static ULONG bstr_storage[4];
    UBYTE *bstr = (UBYTE *)bstr_storage;
    int d, c;

    if (!open_locale_lib())
        return 20;
    loc = OpenLocale(NULL);
    if (!loc)
        return 20;

    bstr[0] = 5;
    bstr[1] = 'A'; bstr[2] = 'm'; bstr[3] = 'i'; bstr[4] = 'g'; bstr[5] = 'a';
    bstr[6] = 'X'; bstr[7] = 0;

    P_SECTION("FormatDate");
    for (d = 0; d < (int)(sizeof(dates) / sizeof(dates[0])); d++) {
        probe_s("date ");
        probe_dec(dates[d].ds_Days);
        probe_ch('/');
        probe_dec(dates[d].ds_Minute);
        probe_ch('/');
        probe_dec(dates[d].ds_Tick);
        probe_ch('\n');
        for (c = 0; c < (int)(sizeof(date_codes) / sizeof(date_codes[0])); c++)
            fd(date_codes[c], &dates[d]);
        fd((const char *)loc->loc_DateTimeFormat, &dates[d]);
        fd((const char *)loc->loc_DateFormat, &dates[d]);
        fd((const char *)loc->loc_TimeFormat, &dates[d]);
        fd((const char *)loc->loc_ShortDateTimeFormat, &dates[d]);
        fd((const char *)loc->loc_ShortDateFormat, &dates[d]);
        fd((const char *)loc->loc_ShortTimeFormat, &dates[d]);
    }

    P_SECTION("FormatDate week numbers");
    {
        static const LONG starts[] = {0, 365, 730, 1095, 1461, 8035, 16801, 17166};
        int s, k;
        for (s = 0; s < (int)(sizeof(starts) / sizeof(starts[0])); s++) {
            for (k = -3; k < 11; k++) {
                struct DateStamp ds;
                if (starts[s] + k < 0)
                    continue;
                ds.ds_Days = starts[s] + k;
                ds.ds_Minute = 0;
                ds.ds_Tick = 0;
                fd("%Y-%m-%d %a j%j U%U W%W w%w", &ds);
            }
        }
    }

    P_SECTION("FormatDate month boundaries");
    {
        /* first and last day of every month of 1978, 1980 and 2100 */
        static const LONG ystart[] = {0, 730, 44560};
        static const UBYTE mdays[2][12] = {
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
            {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        };
        int y, m;
        for (y = 0; y < 3; y++) {
            LONG day = ystart[y];
            for (m = 0; m < 12; m++) {
                struct DateStamp ds;
                ds.ds_Minute = 0;
                ds.ds_Tick = 0;
                ds.ds_Days = day;
                fd("%Y-%m-%d j%j U%U W%W", &ds);
                day += mdays[y == 1][m];
                ds.ds_Days = day - 1;
                fd("%Y-%m-%d j%j U%U W%W", &ds);
            }
        }
    }

    P_SECTION("FormatString");
    fs("plain");
    fs("");
    W(42); fs("%d");
    W(0xffff); fs("%d");
    W(0x8000); fs("%d");
    W(0xffff); fs("%u");
    W(0xbeef); fs("%x");
    W(0xbeef); fs("%X");
    W(0x41); fs("%c");
    L(0x10000); fs("%ld");
    L(0xffffffff); fs("%ld");
    L(0xffffffff); fs("%lu");
    L(0x80000000); fs("%ld");
    L(0xdeadbeef); fs("%lx");
    L(0xdeadbeef); fs("%lX");
    L(0x42); fs("%lc");
    W(1234); fs("%D");
    W(0xffff); fs("%D");
    W(65535); fs("%U");
    L(1234567); fs("%lD");
    L(-1234567); fs("%lD");
    L(999); fs("%lD");
    L(1000); fs("%lD");
    L(4000000000UL); fs("%lU");
    L(123456); fs("%lU");
    L(123456); fs("%10lU|");
    L(123456); fs("%-10lD|");
    L(123456); fs("%010lD|");
    W(7); fs("%5d|");
    W(7); fs("%-5d|");
    W(7); fs("%05d|");
    W(0xfff9); fs("%05d|");
    W(0xfff9); fs("%-05d|");
    W(7); fs("%1d|");
    W(12345); fs("%2d|");
    W(0xab); fs("%08x|");
    L((ULONG)str1); fs("%s");
    L((ULONG)str1); fs("%10s|");
    L((ULONG)str1); fs("%-10s|");
    L((ULONG)str1); fs("%.3s|");
    L((ULONG)str1); fs("%8.2s|");
    L((ULONG)str1); fs("%-8.2s|");
    L((ULONG)str2); fs("[%s]");
    L((ULONG)str1); fs("%ls");
    L((ULONG)bstr >> 2); fs("%b");
    L((ULONG)bstr >> 2); fs("%8b|");
    L((ULONG)bstr >> 2); fs("%.3b|");
    fs("100%%");
    fs("%%d");
    W(5); fs("%d%%");
    W(1); W(2); W(3); fs("%d,%d,%d");
    W(1); L(2); W(3); fs("%d,%ld,%d");
    W(1); W(2); fs("%2$d %1$d");
    W(1); W(2); fs("%1$d %1$d %2$d");
    W(1); L(70000); fs("%2$ld %1$d");
    L((ULONG)str1); W(9); fs("%2$d %1$s");
    L(0x12345678); W(0x41); fs("%2$c%1$lx");
    W(7); fs("%1$5d|");
    W(7); fs("%1$-5d|");
    W(5); fs("%5.3d|");
    W(5); fs("%.3d|");
    W(5); fs("%hd");
    W(5); fs("%i");
    W(5); fs("%e");
    W(5); fs("%z");
    W(5); fs("abc%");
    L(1234567); W(5); fs("%lD,%d");
    L(1234567); W(5); fs("%lU,%d");
    L(999); W(5); fs("%lD,%d");
    W(1234); W(5); fs("%D,%d");
    W(12); W(5); fs("%D,%d");
    L(1234567); W(5); fs("%ld,%d");
    L(0xfffffff9); fs("%05ld|");
    L(0xfffffff9); fs("%5ld|");
    L(0xfffffff9); fs("%01ld|");
    L(0xfffffff9); fs("%02ld|");
    L(0xfffffff9); fs("%03ld|");
    W(0xfff9); fs("%5d|");
    W(0xfb2e); fs("%7D|");
    L(0xffed2979); fs("%-12lD|");
    W(0xff); fs("%05x|");
    W(0xff); fs("%-5x|");
    W(0); fs("%d");
    W(0); fs("%05d|");
    W(0); fs("%D");
    W(999); fs("%D");
    L(100000); fs("%lD");
    L(0x7fffffff); fs("%lD");
    L(0x80000000); fs("%lD");
    L(0xffffffff); fs("%lU");
    W(1); W(2); W(3); fs("%3$d %1$d");
    W(1); W(2); W(3); fs("%3$d");
    W(1); W(2); fs("%2$d %d");
    W(1); W(2); fs("%d %2$d");
    W(7); fs("%1$d%%");
    W(0x41); fs("%5c|");
    W(0x41); fs("%-5c|");
    W(0x41); fs("%05c|");
    L((ULONG)str1); fs("%010s|");
    L((ULONG)str1); fs("%.0s|");
    L((ULONG)str1); fs("%.10s|");
    W(5); fs("%l");
    W(5); fs("%5");
    W(5); fs("%-");
    W(5); fs("%y|");
    W(5); fs("%ly|");
    W(5); fs("%5y|");
    W(1); W(2); L(1234567); fs("%d,%d,%lD");
    L(1234567); L(7654321); fs("%lD,%lD");
    W(1); L(1234567); W(9); fs("%d,%lD,%d");
    W(1); W(1234); fs("%d %D");
    W(1); W(1234); W(7); fs("%d %D %d");
    W(1); W(2); W(3); W(1234); fs("%d%d%d%D");
    L(1234567); fs("%1$lD");
    W(5); L(1234567); fs("%2$lD %1$d");
    L(1234567); fs("%lD%%");
    W(1234); fs("%5D|");
    W(1234); fs("%-7D|");
    W(1234); fs("%07D|");
    W(1234); fs("%.2D|");
    L(12345678); fs("%lU");
    L(1234567); fs("%lu");
    W(0x41); fs("%ld");
    L(0x12345678); fs("%lc");
    W(0xfff9); fs("%u");
    W(0xfff9); fs("%U");
    W(0xfff9); fs("%x");
    L(0xfffffff9); fs("%lU");
    L(0xfffffff9); fs("%lD");
    L(0xfffffff9); fs("%lu");
    W(7); fs("%+d");
    W(7); fs("% d");
    W(7); fs("%#x");
    W(7); fs("%*d");

    P_SECTION("ParseDate");
    pd("%m/%d/%y", "12/25/93");
    pd("%m/%d/%y", "01/01/78");
    pd("%m/%d/%y", "1/2/78");
    pd("%m/%d/%y", "12/31/77");
    pd("%m/%d/%y", "01/01/00");
    pd("%m/%d/%y", "06/15/45");
    pd("%m/%d/%y", "13/01/93");
    pd("%m/%d/%y", "00/01/93");
    pd("%m/%d/%y", "02/29/92");
    pd("%m/%d/%y", "02/29/93");
    pd("%m/%d/%y", "02/30/92");
    pd("%m/%d/%y", "04/31/93");
    pd("%m/%d/%y", "12/25/93x");
    pd("%m/%d/%y", " 12/25/93");
    pd("%m/%d/%y", "12-25-93");
    pd("%m/%d/%y", "12/25");
    pd("%m/%d/%y", "");
    pd("%m/%d/%y", "ab/cd/ef");
    pd("%m/%d/%Y", "12/25/1993");
    pd("%m/%d/%Y", "12/25/93");
    pd("%m/%d/%Y", "01/01/1977");
    pd("%m/%d/%Y", "01/01/2100");
    pd("%d.%m.%Y", "24.12.1999");
    pd("%d.%m.%Y", "1.1.2000");
    pd("%Y-%m-%d", "2024-01-01");
    pd("%e %B %Y", "5 January 1990");
    pd("%e %B %Y", "5 JANUARY 1990");
    pd("%e %B %Y", "5 januar 1990");
    pd("%d %b %Y", "05 Feb 1990");
    pd("%d %b %Y", "05 February 1990");
    pd("%d %h %Y", "05 Mar 1990");
    pd("%A %d %b %Y", "Monday 01 Jan 1990");
    pd("%a %d %b %Y", "Mon 01 Jan 1990");
    pd("%a %d %b %Y", "Sun 01 Jan 1990");
    pd("%A", "Tuesday");
    pd("%H:%M:%S", "12:34:56");
    pd("%H:%M:%S", "00:00:00");
    pd("%H:%M:%S", "23:59:59");
    pd("%H:%M:%S", "24:00:00");
    pd("%H:%M:%S", "12:60:00");
    pd("%H:%M:%S", "12:00:60");
    pd("%H:%M", "7:5");
    pd("%H:%M", "25:00");
    pd("%I:%M %p", "07:30 PM");
    pd("%I:%M %p", "07:30 AM");
    pd("%I:%M %p", "12:00 AM");
    pd("%I:%M %p", "12:00 PM");
    pd("%I:%M %p", "13:00 PM");
    pd("%I:%M %p", "07:30 pm");
    pd("%H:%M %p", "07:30 PM");
    pd("%m/%d/%y %H:%M:%S", "12/25/93 12:34:56");
    pd("%m/%d/%y %H:%M", "12/25/93 12:34");
    pd("%y", "93");
    pd("%Y", "1993");
    pd("%d", "15");
    pd("%m", "6");
    pd("%M", "30");
    pd("%S", "30");
    pd("%j", "100");
    pd("%q", "1");
    pd("%T", "12:34:56");
    pd("%D", "12/25/93");
    pd("plain", "plain");
    pd("plain", "plane");
    pd("", "");
    pd("", "x");
    pd("%%", "%");
    pd("%m/%d/%y", "12/25/1993");
    pd("%m/%d/%y", "+1/25/93");
    pd("%m/%d/%y", "-1/25/93");
    pd("%d %m", "15   6");
    pd("%d %m", "15 6");
    pd("%d %m", "15\t6");
    pd("%d%m", "1506");
    pd("%d%m", "15");
    pd("%d/%m", "5/ 6");
    pd("%d/%m", "5 /6");
    pd("%a", "Xyz");
    pd("%a", "mon");
    pd("%A", "Mon");
    pd("%A", "MONDAY");
    pd("%b", "Febr");
    pd("%B", "feb");
    pd("%h", "February");
    pd("%B", "Mayday");
    pd("%m/%d/%y %H:%M", "02/30/92 12:34");
    pd("%m/%d/%y %H:%M", "13/30/92 12:34");
    pd("%Y", "2200");
    pd("%Y", "2114");
    pd("%Y", "2115");
    pd("%Y", "9999");
    pd("%Y", "01993");
    pd("%m", "012");
    pd("%d", " 5");
    pd("%d", "   5");
    pd("%d", "\t5");
    pd("%d", "5 ");
    pd("%d", "0");
    pd("%d", "31");
    pd("%d", "32");
    pd("%m", "2");
    pd("%d %m", "29 2");
    pd("%d %m", "31 4");
    pd("%H", "007");
    pd("%M", "5x");
    pd("%S", "059");
    pd("%S", "5");
    pd("%p", "PM");
    pd("%p", "am");
    pd("%p", "XM");
    pd("%p", "P");
    pd("%H %p", "12 AM");
    pd("%H %p", "0 PM");
    pd("%H %p", "23 AM");
    pd("%I %p", "0 AM");
    pd("%I", "0");
    pd("%I", "23");
    pd("%I", "24");
    pd(" %d", "5");
    pd(" %d", " 5");
    pd("x%d", "x5");
    pd("x%d", "y5");
    pd("x%d", "yx5");
    pd("%m/%d/%y", "1//93");
    pd("%m/%d/%y", "/1/93");
    pd("%d", "");
    pd("%d.%m.", "1.2.");
    pd("%d.%m.", "1.2");
    pd("%d.%m.", "1.2.x");
    pd("%y", "0");
    pd("%y", "100");
    pd("%y", "77");
    pd("%y", "78");
    pd("%%%d", "%5");
    pd("%m/%d/%y", "12/25/93 extra");
    pd("%A %d", "Tuesday 5");
    pd("abc", "ABC");
    pd("abc", "xxabc");
    pd("abc", "abcxx");
    pd("a", "");
    pd("", " ");
    pd("", "  x");
    pd("%d", "5x6");
    pd("%H:%M:%S", "1:2:3");
    pd("%H:%M:%S", "1:2");
    pd("%M", "60");
    pd("%M", "59");
    pd("%S", "60");
    pd("%H", "24");
    pd("%H", "23");
    pd("%e", " 5");
    pd("%e", "05");
    pd("%B", "januar");
    pd("%B %Y", "januar 1990");
    pd("%B %Y", "Febr 1990");
    pd("%B %Y", "Mayday 1990");
    pd("%B %Y", "Jan 1990");
    pd("%B %Y", "janu 1990");
    pd("%B %Y", "ja 1990");
    pd("%B", "ja");
    pd("%B", "Ma");
    pd("%B", "Jun");
    pd("%B", "Ju");
    pd("%B", "Junk");
    pd("%B", "Augusta");
    pd("%B", "Sept");
    pd("%B", "Septem");
    pd("%b", "December");
    pd("%B", "Decembe");
    pd("%B", "Novembers");
    pd("%B", "Mar");
    pd("%B", "March");
    pd("%B", "Marc");
    pd("%B", "Ma");
    pd("%B", "Jul");
    pd("%B", "July");
    pd("%B", "Juli");
    pd("%B", "Janua");
    pd("%B", "Januaryx");
    pd("%B", "xJan");
    pd("%B", "");
    pd("%B", "1");
    pd("%B %d", "May5 3");
    pd("%B", "May5");
    pd("%b/%d", "Feb/5");
    pd("%b%d", "Feb5");
    pd("%b.%d", "Feb.5");
    pd("%p", "PMX");
    pd("%p", "AM.");
    pd("%p", "A");
    pd("%p", "AMPM");
    pd("%H%p", "7PM");
    pd("%H %p", "7 X");
    pd("%p %H", "PM 7");
    pd("%a %d", "Xyz 5");
    pd("%a/%d", "Mon/5");
    pd("%a%d", "Mon5");
    pd("%A", "");
    pd("%A", "123");
    pd("%j %d", "100 5");
    pd("%j/%d", "100/5");
    pd("%j/%d", "1 0/5");
    pd("%m/%d/%Y", "02/07/2114");
    pd("%m/%d/%Y", "02/08/2114");
    pd("%m/%d/%Y %H:%M:%S", "02/07/2114 06:28:15");
    pd("%m/%d/%Y %H:%M:%S", "02/07/2114 06:28:16");
    pd("%m/%d/%Y", "12/31/2114");
    pd("%m/%d/%Y", "02/29/2000");
    pd("%m/%d/%Y", "02/29/2100");
    pd("%m/%d/%Y", "06/31/1990");
    pd("%m/%d/%Y", "12/31/1977");
    pd("%m/%d/%Y", "01/01/1978");
    pd("%d %d", "5 7");
    pd("%m %m", "5 7");
    pd("%y %Y", "90 1995");
    pd("%Y %y", "1995 90");
    pd("%d-%m", "5--6");
    pd("%d-%m", "5-x-6");
    pd("%d-%m", "5 -6");
    pd("%d - %m", "5-6");
    pd("%d - %m", "5  -  6");
    pd("%d  %m", "5 6");
    pd("%d%%m", "5%6");
    pd("%", "x");
    pd("%d%", "5");
    pd("%H:%M:%S", "12:34:56.78");
    pd("%M:%S", "99:99");
    pd("%d", "4294967301");
    pd("%d", "65541");
    pd("%m", "65537");
    pd("%b/%d", "Febr/5");
    pd("%B-%d", "Mayday-5");
    pd("%b%d", "Febr5");
    pd("%B %d", "feb 5");
    pd("%B %d", "FEBRUARY 5");
    pd("%B %d", "5 3");
    pd("%B/%d", "/3");
    pd("%B %d", "Ma 3");
    pd("%a/%d", "Xyz/5");
    pd("%a%d", "Xyz5");
    pd("%a %d", "Mo 5");
    pd("%a %d", "Monday 5");
    pd("%A %d", "Mon 5");
    pd("%a %d", "Mond 5");
    pd("%a %d", "mon 5");
    pd("%a %d", "5");
    pd("%a %d", "1 5");
    pd("%p/%H", "PM/7");
    pd("%p%H", "PM7");
    pd("%p %H", "AM 7");
    pd("%p %H", "pm 7");
    pd("%p,%H", "PM,7");
    pd("%p %H", "7");
    pd("%p", "1");
    pd("%j%d", "15");
    pd("%q %d", "1 5");
    pd("%%", "x");
    pd("%%", "%%");
    pd("%B%%", "May%");
    pd("%d %B", "5 Junk");
    pd("%d %B", "5 Mayday");
    pd("%d %p", "5 PMX");
    pd("%d %a", "5 Xyz");
    pd((const char *)loc->loc_ShortDateFormat, "12/25/93");
    pd((const char *)loc->loc_ShortTimeFormat, "12:34");
    pd((const char *)loc->loc_DateFormat, "Saturday December 25 1993");
    pd((const char *)loc->loc_TimeFormat, "12:34:56");

    CloseLocale(loc);
    CloseLibrary((struct Library *)LocaleBase);
    return 0;
}
