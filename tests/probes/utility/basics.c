/*
 * Probe (Phase 222a): utility.library strings, character case, dates,
 * 32-bit math and tag-list basics.  Output compared with AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <utility/date.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <inline/exec.h>
#include <inline/utility.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *UtilityBase;

static const char *const pairs[][2] = {
    {"abc", "ABC"}, {"abc", "abd"}, {"abd", "abc"}, {"", ""}, {"a", ""}, {"", "a"},
    {"\xe4pfel", "\xc4PFEL"}, {"zzz", "ZZZ{"}, {"[", "{"}, {"_", "a"}, {"Amiga", "AMIGA500"},
};

int main(void)
{
    struct ClockData cd;
    ULONG secs, i;
    UBYTE upper[256], lower[256];

    UtilityBase = OpenLibrary((STRPTR)"utility.library", 37);
    if (!UtilityBase)
        return 20;

    P_SECTION("Stricmp/Strnicmp");
    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        LONG r = Stricmp((STRPTR)pairs[i][0], (STRPTR)pairs[i][1]);
        LONG n = Strnicmp((STRPTR)pairs[i][0], (STRPTR)pairs[i][1], 3);
        probe_s("pair ");
        probe_dec(i);
        probe_s(": stricmp sign = ");
        probe_dec(r < 0 ? -1 : r > 0 ? 1 : 0);
        probe_s(", strnicmp(3) sign = ");
        probe_dec(n < 0 ? -1 : n > 0 ? 1 : 0);
        probe_ch('\n');
    }

    P_SECTION("ToUpper/ToLower");
    for (i = 0; i < 256; i++) {
        upper[i] = ToUpper(i);
        lower[i] = ToLower(i);
    }
    P_HEX("ToUpper table hash", probe_hash(upper, 256));
    P_HEX("ToLower table hash", probe_hash(lower, 256));
    P_BYTES("ToUpper 0xe0..0xff", upper + 0xe0, 32);
    P_BYTES("ToLower 0xc0..0xdf", lower + 0xc0, 32);

    P_SECTION("Amiga2Date/Date2Amiga/CheckDate");
    {
        static const ULONG samples[] = {0, 1, 59, 86399, 86400, 31535999, 63071999, 63072000,
                                        662774400, 1000000000, 0x7fffffff, 0xffffffff};
        for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
            Amiga2Date(samples[i], &cd);
            probe_s("Amiga2Date ");
            probe_dec((LONG)i);
            probe_s(": ");
            probe_dec(cd.year); probe_ch('-'); probe_dec(cd.month); probe_ch('-'); probe_dec(cd.mday);
            probe_ch(' ');
            probe_dec(cd.hour); probe_ch(':'); probe_dec(cd.min); probe_ch(':'); probe_dec(cd.sec);
            probe_s(" wday ");
            probe_dec(cd.wday);
            probe_s(" back ");
            secs = Date2Amiga(&cd);
            probe_hex(secs, 8);
            probe_ch('\n');
        }
        cd.sec = 0; cd.min = 0; cd.hour = 0; cd.mday = 29; cd.month = 2; cd.year = 2000; cd.wday = 0;
        P_HEX("CheckDate 2000-02-29", CheckDate(&cd));
        cd.year = 1900;
        P_HEX("CheckDate 1900-02-29", CheckDate(&cd));
        cd.year = 1977;
        P_HEX("CheckDate 1977-02-29", CheckDate(&cd));
        cd.year = 1978; cd.mday = 31; cd.month = 4;
        P_HEX("CheckDate 1978-04-31", CheckDate(&cd));
        cd.mday = 1; cd.month = 13;
        P_HEX("CheckDate 1978-13-01", CheckDate(&cd));
        cd.month = 12; cd.hour = 24;
        P_HEX("CheckDate hour 24", CheckDate(&cd));
    }

    P_SECTION("SMult32/UMult32/SDivMod32/UDivMod32");
    P_HEX("SMult32(-7, 6)", SMult32(-7, 6));
    P_HEX("SMult32(0x10000, 0x10000)", SMult32(0x10000, 0x10000));
    P_HEX("UMult32(0xffffffff, 2)", UMult32(0xffffffff, 2));
    P_HEX("UMult32(123456, 654321)", UMult32(123456, 654321));

    P_SECTION("TagItems");
    {
        struct TagItem list[] = {
            {TAG_USER + 1, 11}, {TAG_IGNORE, 99}, {TAG_USER + 2, 22},
            {TAG_SKIP, 1}, {TAG_USER + 3, 33}, {TAG_USER + 4, 44}, {TAG_DONE, 0}
        };
        struct TagItem *ti;
        P_LONG("GetTagData(+1)", GetTagData(TAG_USER + 1, -1, list));
        P_LONG("GetTagData(+3 skipped)", GetTagData(TAG_USER + 3, -1, list));
        P_LONG("GetTagData(+4)", GetTagData(TAG_USER + 4, -1, list));
        P_LONG("GetTagData(missing)", GetTagData(TAG_USER + 9, -1, list));
        ti = FindTagItem(TAG_USER + 2, list);
        P_LONG("FindTagItem(+2) index", ti ? (LONG)(ti - list) : -1);
        {
            static Tag arr[] = {TAG_USER + 5, TAG_USER + 2, TAG_DONE};
            P_BOOL("TagInArray(+2)", TagInArray(TAG_USER + 2, arr));
            P_BOOL("TagInArray(+7)", TagInArray(TAG_USER + 7, arr));
        }
    }

    CloseLibrary(UtilityBase);
    return 0;
}
