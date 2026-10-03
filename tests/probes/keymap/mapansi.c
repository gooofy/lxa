/*
 * Probe (Phase 222f): keymap.library MapANSI().
 * Every character 1-255 with the default keymap, strings (incl. string
 * keys and dead-key composed characters), buffer-too-small cases, return
 * codes, and a custom keymap with double dead keys.  Each result is
 * checked for reversibility with MapRawKey().  Output compared with
 * AmigaOS 3.1.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <clib/exec_protos.h>
#include <clib/keymap_protos.h>
#include <inline/exec.h>
#include <inline/keymap.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *KeymapBase;

static UBYTE buf[80];
static UBYTE in[64];

static void fill(void)
{
    LONG i;
    for (i = 0; i < (LONG)sizeof(buf); i++)
        buf[i] = 0xee;
}

static void put_pairs(LONG rv, LONG show)
{
    static const char hx[] = "0123456789abcdef";
    LONG i;
    probe_dec(rv);
    if (show <= 0)
        return;
    probe_ch('[');
    for (i = 0; i < show * 2; i++) {
        if (i && !(i & 1))
            probe_ch(' ');
        probe_ch(hx[buf[i] >> 4]);
        probe_ch(hx[buf[i] & 15]);
    }
    probe_ch(']');
}

/* map back the last char of a single-char result with MapRawKey */
static void reverse(LONG rv, struct KeyMap *km)
{
    struct InputEvent ie;
    UBYTE out[16];
    WORD r;
    UBYTE *p = buf;
    ie.ie_NextEvent = NULL;
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_SubClass = 0;
    ie.ie_Prev1DownCode = ie.ie_Prev1DownQual = 0;
    ie.ie_Prev2DownCode = ie.ie_Prev2DownQual = 0;
    if (rv == 3) {
        ie.ie_Prev2DownCode = *p++;
        ie.ie_Prev2DownQual = *p++;
    }
    if (rv >= 2) {
        ie.ie_Prev1DownCode = *p++;
        ie.ie_Prev1DownQual = *p++;
    }
    ie.ie_Code = *p++;
    ie.ie_Qualifier = *p;
    r = MapRawKey(&ie, (STRPTR)out, 16, km);
    probe_s(" rev ");
    if (r == 1)
        probe_hex(out[0], 2);
    else
        probe_dec(r);
}

static LONG ansi(const UBYTE *s, LONG count, LONG length, struct KeyMap *km)
{
    fill();
    return MapANSI((STRPTR)s, count, (STRPTR)buf, length, km);
}

/* --- custom keymap with double dead keys --------------------------------- */
static UBYTE c_lotypes[0x40], c_hitypes[0x38];
static ULONG c_lomap[0x40], c_himap[0x38];
static UBYTE c_locaps[8], c_lorep[8], c_hicaps[7], c_hirep[7];
static struct KeyMap ckm;
static const UBYTE dd1[] = {DPF_DEAD, 0x31};
static const UBYTE dd2[] = {DPF_DEAD, 0x32, 0, 'Y'};
static const UBYTE ddmod[] = {DPF_MOD, 4, 0, 'X',
                              0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8};
static const UBYTE str2[] = {2, 4, 3, 6, 'x', 'y', 'x', 'y', 'z'};

static void build_custom(void)
{
    LONG i;
    for (i = 0; i < 0x40; i++) {
        c_lotypes[i] = KCF_NOP;
        c_lomap[i] = 0;
    }
    for (i = 0; i < 0x38; i++) {
        c_hitypes[i] = KCF_NOP;
        c_himap[i] = 0;
    }
    c_lotypes[0x19] = KCF_DEAD;
    c_lomap[0x19] = (ULONG)dd1;
    c_lotypes[0x1a] = KCF_DEAD | KCF_SHIFT;
    c_lomap[0x1a] = (ULONG)dd2;
    c_lotypes[0x20] = KCF_DEAD | KCF_SHIFT;
    c_lomap[0x20] = (ULONG)ddmod;
    c_lotypes[0x21] = KC_VANILLA;
    c_lomap[0x21] = 0xe1c16141;
    c_lotypes[0x22] = KCF_STRING | KCF_SHIFT;
    c_lomap[0x22] = (ULONG)str2;
    c_lotypes[0x23] = KCF_CONTROL;
    c_lomap[0x23] = 0x00000171;
    c_lotypes[0x24] = KC_NOQUAL;
    c_lomap[0x24] = 0x00000171;
    c_hitypes[0x00] = KC_NOQUAL;
    c_himap[0x00] = 0x00000071;
    ckm.km_LoKeyMapTypes = c_lotypes;
    ckm.km_LoKeyMap = c_lomap;
    ckm.km_LoCapsable = c_locaps;
    ckm.km_LoRepeatable = c_lorep;
    ckm.km_HiKeyMapTypes = c_hitypes;
    ckm.km_HiKeyMap = c_himap;
    ckm.km_HiCapsable = c_hicaps;
    ckm.km_HiRepeatable = c_hirep;
}


/* --- tiny keymaps for priority rules ------------------------------------- */
static UBYTE t_lotypes[0x40], t_hitypes[0x38];
static ULONG t_lomap[0x40], t_himap[0x38];
static UBYTE t_bits[8];
static struct KeyMap tkm;

enum { P0, PS, PA, PC, PSA, STR1, DEAD0, DMOD0, VAN, PCTRL, PSCTRL, DEADSEQ, NSPEC };
static const char *const spec_name[NSPEC] = {
    "P0", "PS", "PA", "PC", "PSA", "STR1", "DEAD0", "DMOD0", "VAN", "P1a", "PS1a", "DSEQ",
};
static const UBYTE t_str1[] = {1, 2, 'Z'};
static const UBYTE t_dead0[] = {0, 'Z'};
static const UBYTE t_dmod0[] = {DPF_MOD, 2, 'Z', 'q'};
static const UBYTE t_dseq[] = {DPF_MOD, 2, 'q', 'Z'};
static const UBYTE t_dk[] = {DPF_DEAD, 1};

static void t_clear(void)
{
    LONG i;
    for (i = 0; i < 0x40; i++) {
        t_lotypes[i] = KCF_NOP;
        t_lomap[i] = 0;
    }
    for (i = 0; i < 0x38; i++) {
        t_hitypes[i] = KCF_NOP;
        t_himap[i] = 0;
    }
    tkm.km_LoKeyMapTypes = t_lotypes;
    tkm.km_LoKeyMap = t_lomap;
    tkm.km_LoCapsable = t_bits;
    tkm.km_LoRepeatable = t_bits;
    tkm.km_HiKeyMapTypes = t_hitypes;
    tkm.km_HiKeyMap = t_himap;
    tkm.km_HiCapsable = t_bits;
    tkm.km_HiRepeatable = t_bits;
}

static void t_key(LONG code, LONG spec)
{
    UBYTE type = KCF_NOP;
    ULONG m = 0;
    switch (spec) {
    case P0: type = KC_NOQUAL; m = 'Z'; break;
    case PS: type = KCF_SHIFT; m = 'q' | ('Z' << 8); break;
    case PA: type = KCF_ALT; m = 'q' | ('Z' << 8); break;
    case PC: type = KCF_CONTROL; m = 'q' | ('Z' << 8); break;
    case PSA: type = KCF_SHIFT | KCF_ALT; m = 'q' | ('r' << 8) | ('s' << 16) | ((ULONG)'Z' << 24); break;
    case STR1: type = KCF_STRING; m = (ULONG)t_str1; break;
    case DEAD0: type = KCF_DEAD; m = (ULONG)t_dead0; break;
    case DMOD0: type = KCF_DEAD; m = (ULONG)t_dmod0; break;
    case VAN: type = KC_VANILLA; m = 0x7a; break;               /* 'z': ctrl -> 0x1a */
    case PCTRL: type = KC_NOQUAL; m = 0x1a; break;
    case PSCTRL: type = KCF_SHIFT; m = 'q' | (0x1a << 8); break;
    case DEADSEQ: type = KCF_DEAD; m = (ULONG)t_dseq; break;
    }
    if (code < 0x40) {
        t_lotypes[code] = type;
        t_lomap[code] = m;
    } else {
        t_hitypes[code - 0x40] = type;
        t_himap[code - 0x40] = m;
    }
}

static void t_run(const char *label, UBYTE ch)
{
    LONG rv;
    in[0] = ch;
    rv = ansi(in, 1, 4, &tkm);
    probe_s(label);
    probe_s(" = ");
    put_pairs(rv, rv > 0 ? rv : 0);
    probe_ch('\n');
}

/* --- second double-dead keymap: dead idx 1,2,3 with factor 5 ----------- */
static const UBYTE e_d1[] = {DPF_DEAD, 0x51};
static const UBYTE e_d2[] = {DPF_DEAD, 0x52};
static const UBYTE e_d3[] = {DPF_DEAD, 0x53};
static const UBYTE e_d3b[] = {DPF_DEAD, 0x03, DPF_DEAD, 0x53};
static UBYTE e_mod[4 + 25];

static void build_e(void)
{
    LONG i;
    t_clear();
    e_mod[0] = DPF_MOD;
    e_mod[1] = 4;
    e_mod[2] = 0;
    e_mod[3] = 'X';
    for (i = 0; i < 25; i++)
        e_mod[4 + i] = (UBYTE)(0xa0 + i);
    t_lotypes[0x10] = KCF_DEAD; t_lomap[0x10] = (ULONG)e_d1;
    t_lotypes[0x11] = KCF_DEAD; t_lomap[0x11] = (ULONG)e_d2;
    t_lotypes[0x12] = KCF_DEAD; t_lomap[0x12] = (ULONG)e_d3;
    t_lotypes[0x13] = KCF_DEAD | KCF_SHIFT; t_lomap[0x13] = (ULONG)e_d3b;
    t_lotypes[0x20] = KCF_DEAD | KCF_SHIFT; t_lomap[0x20] = (ULONG)e_mod;
}

static void str_case(const char *label, const char *s, LONG count, LONG length, struct KeyMap *km)
{
    LONG rv = ansi((const UBYTE *)s, count, length, km);
    probe_s(label);
    probe_s(" = ");
    put_pairs(rv, rv > 0 ? rv : 0);
    probe_ch('\n');
}

int main(void)
{
    LONG c, rv, len, i;

    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!KeymapBase)
        return 20;

    P_SECTION("chars 1-255, count 1, length 3");
    for (c = 1; c < 256; c++) {
        in[0] = (UBYTE)c;
        rv = ansi(in, 1, 3, NULL);
        probe_s("ansi ");
        probe_hex(c, 2);
        probe_s(" = ");
        put_pairs(rv, rv > 0 ? rv : 0);
        if (rv > 0)
            reverse(rv, NULL);
        probe_ch('\n');
    }

    P_SECTION("char 0");
    in[0] = 0;
    rv = ansi(in, 1, 3, NULL);
    probe_s("ansi 0x00 = ");
    put_pairs(rv, rv > 0 ? rv : 0);
    probe_ch('\n');

    P_SECTION("length 1 and 0 (buffer too small)");
    {
        static const UBYTE cs[] = {'a', 'A', 0xe1, 0xc4, 0xa0, 0x9b, 0x01, ' ', 0xb4, 0xfd, 0x7f, 0xff};
        for (i = 0; i < (LONG)sizeof(cs); i++) {
            in[0] = cs[i];
            probe_s("char ");
            probe_hex(cs[i], 2);
            probe_ch(':');
            for (len = 0; len <= 2; len++) {
                rv = ansi(in, 1, len, NULL);
                probe_s(" len");
                probe_dec(len);
                probe_ch('=');
                put_pairs(rv, rv > 0 ? rv : 0);
            }
            probe_ch('\n');
        }
    }

    P_SECTION("strings (default keymap)");
    str_case("Hello", "Hello", 5, 20, NULL);
    str_case("Hello len 4", "Hello", 5, 4, NULL);
    str_case("Hello count 3", "Hello", 3, 20, NULL);
    str_case("count 0", "Hello", 0, 20, NULL);
    str_case("F1 CSI 0~", "\x9b" "0~", 3, 20, NULL);
    str_case("shift F1 CSI 10~", "\x9b" "10~", 4, 20, NULL);
    str_case("cursor up CSI A", "\x9b" "A", 2, 20, NULL);
    str_case("shift right CSI space @", "\x9b" " @", 3, 20, NULL);
    str_case("help CSI ?~", "\x9b" "?~", 3, 20, NULL);
    str_case("shift tab CSI Z", "\x9b" "Z", 2, 20, NULL);
    str_case("CSI alone", "\x9b", 1, 20, NULL);
    str_case("CSI x", "\x9b" "x", 2, 20, NULL);
    str_case("CSI 0 (partial F1)", "\x9b" "0", 2, 20, NULL);
    str_case("F1 len 0", "\x9b" "0~", 3, 0, NULL);
    str_case("composed a acute e grave", "\xe1\xe8", 2, 20, NULL);
    str_case("composed len 1", "\xe1", 1, 1, NULL);
    str_case("composed len 2", "\xe1", 1, 2, NULL);
    str_case("a + composed len 2", "a\xe1", 2, 2, NULL);
    str_case("a + composed len 3", "a\xe1", 2, 3, NULL);
    str_case("tab cr lf esc del", "\t\r\n\x1b\x7f", 5, 20, NULL);
    str_case("ungeneratable middle", "ab\x80" "cd", 5, 20, NULL);
    str_case("ungeneratable first", "\x80", 1, 20, NULL);
    str_case("nul in string", "a\0b", 3, 20, NULL);
    str_case("pause CSI 43~", "\x9b" "43~", 4, 20, NULL);
    str_case("F12 CSI 21~", "\x9b" "21~", 4, 20, NULL);
    str_case("insert CSI 40~", "\x9b" "40~", 4, 20, NULL);
    str_case("end CSI 45~", "\x9b" "45~", 4, 20, NULL);
    str_case("digits", "0123456789", 10, 20, NULL);
    str_case("punct", "!@#$%^&*()-=_+[]{};':\",./<>?`~\\|", 32, 40, NULL);

    build_custom();

    P_SECTION("custom keymap with double dead keys");
    for (c = 1; c < 256; c++) {
        in[0] = (UBYTE)c;
        rv = ansi(in, 1, 3, &ckm);
        if (rv == 0)
            continue;
        probe_s("ansi ");
        probe_hex(c, 2);
        probe_s(" = ");
        put_pairs(rv, rv > 0 ? rv : 0);
        if (rv > 0)
            reverse(rv, &ckm);
        probe_ch('\n');
    }
    str_case("xyz", "xyz", 3, 20, &ckm);
    str_case("xyxy", "xyxy", 4, 20, &ckm);
    str_case("xyzxy", "xyzxy", 5, 20, &ckm);
    for (c = 0xc0; c <= 0xc8; c++) {
        in[0] = (UBYTE)c;
        probe_s("double dead ");
        probe_hex(c, 2);
        probe_ch(':');
        for (len = 0; len <= 3; len++) {
            rv = ansi(in, 1, len, &ckm);
            probe_s(" len");
            probe_dec(len);
            probe_ch('=');
            put_pairs(rv, rv > 0 ? rv : 0);
        }
        probe_ch('\n');
    }

    P_SECTION("priority rules (char Z, keys at code A then B)");
    {
        static const UBYTE pairs[][2] = {
            {P0, STR1}, {P0, DEAD0}, {P0, DMOD0}, {STR1, DEAD0}, {DEAD0, DMOD0},
            {PS, PA}, {PC, PSA}, {PS, PC}, {PA, PC}, {P0, P0}, {PS, PS}, {P0, PS},
            {PSA, P0}, {DEADSEQ, PSA}, {STR1, PS},
        };
        static const UBYTE codes[][2] = {{0x10, 0x11}, {0x10, 0x50}, {0x30, 0x41}, {0x45, 0x05}};
        LONG p, o, cc;
        for (p = 0; p < (LONG)(sizeof(pairs) / sizeof(pairs[0])); p++)
            for (o = 0; o < 2; o++)
                for (cc = 0; cc < 4; cc++) {
                    LONG a = pairs[p][o], b = pairs[p][1 - o];
                    t_clear();
                    if (a == DEADSEQ || b == DEADSEQ) {
                        t_lotypes[0x3f] = KCF_DEAD;
                        t_lomap[0x3f] = (ULONG)t_dk;
                    }
                    t_key(codes[cc][0], a);
                    t_key(codes[cc][1], b);
                    probe_s(spec_name[a]);
                    probe_ch('@');
                    probe_hex(codes[cc][0], 2);
                    probe_ch(' ');
                    probe_s(spec_name[b]);
                    probe_ch('@');
                    probe_hex(codes[cc][1], 2);
                    t_run("", 'Z');
                }
        /* ctrl inverse of a vanilla key vs plain mappings of 0x1a */
        {
            static const UBYTE cp[][2] = {{VAN, PCTRL}, {PCTRL, VAN}, {VAN, PSCTRL}, {PSCTRL, VAN}};
            for (p = 0; p < 4; p++) {
                t_clear();
                t_key(0x10, cp[p][0]);
                t_key(0x11, cp[p][1]);
                probe_s(spec_name[cp[p][0]]);
                probe_s("@0x10 ");
                probe_s(spec_name[cp[p][1]]);
                probe_s("@0x11");
                t_run("", 0x1a);
            }
        }
        /* scan range: a single key anywhere */
        {
            static const UBYTE pos[] = {0x00, 0x3f, 0x40, 0x5f, 0x60, 0x67, 0x68, 0x6e, 0x71, 0x77};
            for (p = 0; p < (LONG)sizeof(pos); p++) {
                t_clear();
                t_key(pos[p], P0);
                probe_s("P0 alone @");
                probe_hex(pos[p], 2);
                t_run("", 'Z');
                t_clear();
                t_key(pos[p], STR1);
                probe_s("STR1 alone @");
                probe_hex(pos[p], 2);
                t_run("", 'Z');
            }
        }
    }

    P_SECTION("double dead keys, factor 5 (dead 0x10 idx1, 0x11 idx2, 0x12 idx3, 0x13 idx3/shift idx3)");
    build_e();
    for (c = 0xa0; c < 0xa0 + 25; c++) {
        in[0] = (UBYTE)c;
        rv = ansi(in, 1, 3, &tkm);
        probe_s("ansi ");
        probe_hex(c, 2);
        probe_s(" = ");
        put_pairs(rv, rv > 0 ? rv : 0);
        if (rv > 0)
            reverse(rv, &tkm);
        probe_ch('\n');
    }
    {
        struct InputEvent ie;
        UBYTE out[8];
        static const UBYTE pk[] = {0, 0x10, 0x11, 0x12, 0x13};
        LONG a, b;
        ie.ie_NextEvent = NULL;
        ie.ie_Class = IECLASS_RAWKEY;
        ie.ie_SubClass = 0;
        ie.ie_Code = 0x20;
        ie.ie_Qualifier = 0;
        for (a = 0; a < 5; a++) {
            probe_s("raw prev1 ");
            probe_hex(pk[a], 2);
            probe_s(" x prev2:");
            for (b = 0; b < 5; b++) {
                WORD r;
                ie.ie_Prev1DownCode = pk[a];
                ie.ie_Prev1DownQual = 0;
                ie.ie_Prev2DownCode = pk[b];
                ie.ie_Prev2DownQual = 0;
                r = MapRawKey(&ie, (STRPTR)out, 8, &tkm);
                probe_ch(' ');
                if (r == 1)
                    probe_hex(out[0], 2);
                else
                    probe_dec(r);
            }
            probe_ch('\n');
        }
    }

    CloseLibrary(KeymapBase);
    return 0;
}
