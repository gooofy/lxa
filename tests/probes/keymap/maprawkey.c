/*
 * Probe (Phase 222f): keymap.library MapRawKey().
 * Every raw key code 0x00-0x7f under many qualifier combinations with
 * the default keymap, key-up codes, small buffers, dead and double-dead
 * key sequences (ie_Prev1Down/ie_Prev2Down), and a custom keymap that
 * exercises every key type (KC_NOQUAL..KC_VANILLA, KCF_STRING, KCF_DEAD,
 * capsable/repeatable bits).  Output compared with AmigaOS 3.1.
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

static struct InputEvent ie;
static UBYTE buf[64];

static const UWORD quals[] = {
    0x0000,                                          /* none */
    IEQUALIFIER_LSHIFT, IEQUALIFIER_RSHIFT, IEQUALIFIER_CAPSLOCK, IEQUALIFIER_CONTROL,
    IEQUALIFIER_LALT, IEQUALIFIER_RALT, IEQUALIFIER_LCOMMAND, IEQUALIFIER_RCOMMAND,
    IEQUALIFIER_NUMERICPAD, IEQUALIFIER_REPEAT,
    IEQUALIFIER_LSHIFT | IEQUALIFIER_LALT, IEQUALIFIER_RSHIFT | IEQUALIFIER_RALT,
    IEQUALIFIER_LSHIFT | IEQUALIFIER_CONTROL, IEQUALIFIER_CONTROL | IEQUALIFIER_LALT,
    IEQUALIFIER_LSHIFT | IEQUALIFIER_CONTROL | IEQUALIFIER_LALT,
    IEQUALIFIER_CAPSLOCK | IEQUALIFIER_LSHIFT, IEQUALIFIER_CAPSLOCK | IEQUALIFIER_LALT,
    IEQUALIFIER_CAPSLOCK | IEQUALIFIER_CONTROL, IEQUALIFIER_REPEAT | IEQUALIFIER_LSHIFT,
    IEQUALIFIER_RBUTTON | IEQUALIFIER_LSHIFT,
};
#define NQ (sizeof(quals) / sizeof(quals[0]))

static void set_event(UWORD code, UWORD qual, UBYTE p1c, UBYTE p1q, UBYTE p2c, UBYTE p2q)
{
    ie.ie_NextEvent = NULL;
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_SubClass = 0;
    ie.ie_Code = code;
    ie.ie_Qualifier = qual;
    ie.ie_Prev1DownCode = p1c;
    ie.ie_Prev1DownQual = p1q;
    ie.ie_Prev2DownCode = p2c;
    ie.ie_Prev2DownQual = p2q;
}

static void fill(void)
{
    LONG i;
    for (i = 0; i < (LONG)sizeof(buf); i++)
        buf[i] = 0xee;
}

/* print one result compactly: rv <= 0 as decimal, else the bytes */
static void put_result(WORD rv)
{
    WORD i;
    static const char hx[] = "0123456789abcdef";
    if (rv <= 0) {
        probe_dec(rv);
        return;
    }
    if (rv > (WORD)sizeof(buf)) {
        probe_s("big");
        probe_dec(rv);
        return;
    }
    for (i = 0; i < rv; i++) {
        probe_ch(hx[buf[i] >> 4]);
        probe_ch(hx[buf[i] & 15]);
    }
}

static WORD map(UWORD code, UWORD qual, UBYTE p1c, UBYTE p1q, UBYTE p2c, UBYTE p2q, WORD len, struct KeyMap *km)
{
    set_event(code, qual, p1c, p1q, p2c, p2q);
    fill();
    return MapRawKey(&ie, (STRPTR)buf, len, km);
}

/* --- custom keymap ------------------------------------------------------- */
static UBYTE c_lotypes[0x40], c_hitypes[0x38];
static ULONG c_lomap[0x40], c_himap[0x38];
static UBYTE c_locaps[8], c_lorep[8], c_hicaps[7], c_hirep[7];
static struct KeyMap ckm;

/* 8 strings "0", "11", "222", ... */
static const UBYTE str8[] = {
    1, 16, 2, 17, 3, 19, 4, 22, 5, 26, 6, 31, 7, 37, 8, 44,
    '0', '1', '1', '2', '2', '2', '3', '3', '3', '3', '4', '4', '4', '4', '4',
    '5', '5', '5', '5', '5', '5', '6', '6', '6', '6', '6', '6', '6',
    '7', '7', '7', '7', '7', '7', '7', '7',
};
/* plain dead descriptors: 8 x (0, 'a'+k) */
static const UBYTE dead8[] = {
    0, 'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e', 0, 'f', 0, 'g', 0, 'h',
};
/* string with zero length and one with control bytes */
static const UBYTE strz[] = {0, 4, 3, 4, 0x00, 0x9b, 0xff};
/* double dead keys: index 1 / 2, factor 3 */
static const UBYTE dd1[] = {DPF_DEAD, 0x31};
static const UBYTE dd2[] = {DPF_DEAD, 0x32, 0, 'Y'};
static const UBYTE ddsingle[] = {DPF_DEAD, 0x02};
/* dead-able key: unshifted DPF_MOD with a 9-entry table, shifted plain */
static const UBYTE ddmod[] = {DPF_MOD, 4, 0, 'X',
                              '0', '1', '2', '3', '4', '5', '6', '7', '8'};

static void build_custom(struct KeyMap *def)
{
    LONG i;
    for (i = 0; i < 0x40; i++) {
        c_lotypes[i] = def->km_LoKeyMapTypes[i];
        c_lomap[i] = def->km_LoKeyMap[i];
    }
    for (i = 0; i < 0x38; i++) {
        c_hitypes[i] = def->km_HiKeyMapTypes[i];
        c_himap[i] = def->km_HiKeyMap[i];
    }
    for (i = 0; i < 8; i++) {
        c_locaps[i] = def->km_LoCapsable[i];
        c_lorep[i] = def->km_LoRepeatable[i];
    }
    for (i = 0; i < 7; i++) {
        c_hicaps[i] = def->km_HiCapsable[i];
        c_hirep[i] = def->km_HiRepeatable[i];
    }
    /* 0x00-0x07: plain keys of type 0..7, map 'A','B','C','D' (low = 'D') */
    for (i = 0; i < 8; i++) {
        c_lotypes[i] = (UBYTE)i;
        c_lomap[i] = 0xc1e2c3e4;
    }
    /* 0x08-0x0f: string keys of type 0..7 */
    for (i = 0; i < 8; i++) {
        c_lotypes[8 + i] = (UBYTE)(KCF_STRING | i);
        c_lomap[8 + i] = (ULONG)str8;
    }
    /* 0x10-0x17: dead-class keys (plain entries) of type 0..7 */
    for (i = 0; i < 8; i++) {
        c_lotypes[0x10 + i] = (UBYTE)(KCF_DEAD | i);
        c_lomap[0x10 + i] = (ULONG)dead8;
    }
    c_lotypes[0x18] = KCF_STRING | KCF_SHIFT;
    c_lomap[0x18] = (ULONG)strz;
    c_lotypes[0x19] = KCF_DEAD;
    c_lomap[0x19] = (ULONG)dd1;
    c_lotypes[0x1a] = KCF_DEAD | KCF_SHIFT;
    c_lomap[0x1a] = (ULONG)dd2;
    c_lotypes[0x1b] = KCF_DEAD;
    c_lomap[0x1b] = (ULONG)ddsingle;
    c_lotypes[0x20] = KCF_DEAD | KCF_SHIFT;
    c_lomap[0x20] = (ULONG)ddmod;
    /* key 0x21: vanilla with control bytes / high chars */
    c_lotypes[0x21] = KC_VANILLA;
    c_lomap[0x21] = 0x00ff7f20;
    c_lotypes[0x22] = KC_VANILLA;
    c_lomap[0x22] = 0x3f5f3f40;
    /* capsable: 0x00-0x03 and 0x08,0x10; repeatable: not 0x04-0x07 */
    c_locaps[0] = 0x0f;
    c_locaps[1] = 0x01;
    c_locaps[2] = 0x01;
    c_lorep[0] = 0x0f;

    ckm.km_LoKeyMapTypes = c_lotypes;
    ckm.km_LoKeyMap = c_lomap;
    ckm.km_LoCapsable = c_locaps;
    ckm.km_LoRepeatable = c_lorep;
    ckm.km_HiKeyMapTypes = c_hitypes;
    ckm.km_HiKeyMap = c_himap;
    ckm.km_HiCapsable = c_hicaps;
    ckm.km_HiRepeatable = c_hirep;
}

static const UWORD q8[] = {
    0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT, IEQUALIFIER_LSHIFT | IEQUALIFIER_LALT,
    IEQUALIFIER_CONTROL, IEQUALIFIER_CONTROL | IEQUALIFIER_LSHIFT,
    IEQUALIFIER_CONTROL | IEQUALIFIER_LALT,
    IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_LSHIFT,
    IEQUALIFIER_CAPSLOCK, IEQUALIFIER_REPEAT,
};

int main(void)
{
    struct KeyMap *km;
    UWORD code, q;
    WORD rv, full, len;
    LONG i, j;

    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 37);
    if (!KeymapBase)
        return 20;
    km = AskKeyMapDefault();

    P_SECTION("all codes x qualifiers (none LS RS CAPS CTRL LA RA LCMD RCMD NUMPAD REPEAT LS+LA RS+RA LS+CTRL CTRL+LA LS+CTRL+LA CAPS+LS CAPS+LA CAPS+CTRL REP+LS RBUT+LS)");
    for (code = 0; code < 0x80; code++) {
        probe_s("raw ");
        probe_hex(code, 2);
        probe_ch(':');
        for (q = 0; q < NQ; q++) {
            probe_ch(' ');
            put_result(map(code, quals[q], 0, 0, 0, 0, 32, NULL));
        }
        probe_ch('\n');
    }

    P_SECTION("explicit keymap = default");
    probe_s("raw 0x00-0x7f LS+LA hash ");
    {
        ULONG h = 2166136261UL;
        for (code = 0; code < 0x80; code++) {
            rv = map(code, IEQUALIFIER_LSHIFT | IEQUALIFIER_LALT, 0, 0, 0, 0, 32, km);
            h = (h ^ (UBYTE)rv) * 16777619UL;
            if (rv > 0)
                for (i = 0; i < rv; i++)
                    h = (h ^ buf[i]) * 16777619UL;
        }
        probe_hex(h, 8);
        probe_ch('\n');
    }

    P_SECTION("key-up codes (none, LS, LA)");
    for (j = 0; j < 3; j++) {
        static const UWORD kq[] = {0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT};
        for (code = 0x80; code < 0x100; code += 32) {
            probe_s("up ");
            probe_hex(code, 2);
            probe_s(" q");
            probe_hex(kq[j], 4);
            probe_ch(':');
            for (i = 0; i < 32; i++) {
                probe_ch(' ');
                put_result(map(code + i, kq[j], 0, 0, 0, 0, 32, NULL));
            }
            probe_ch('\n');
        }
    }
    probe_s("big codes:");
    {
        static const UWORD big[] = {0x100, 0x120, 0x1a0, 0x7f20, 0xff20, 0xffff};
        for (i = 0; i < 6; i++) {
            probe_ch(' ');
            put_result(map(big[i], 0, 0, 0, 0, 0, 32, NULL));
        }
    }
    probe_ch('\n');

    P_SECTION("small buffers");
    probe_s("len 0, codes 0x00-0x7f none:");
    for (code = 0; code < 0x80; code++) {
        probe_ch(' ');
        put_result(map(code, 0, 0, 0, 0, 0, 0, NULL));
    }
    probe_ch('\n');
    for (q = 0; q < 3; q++) {
        static const UWORD sq[] = {0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT};
        for (code = 0; code < 0x78; code++) {
            full = map(code, sq[q], 0, 0, 0, 0, 32, NULL);
            if (full < 2)
                continue;
            probe_s("raw ");
            probe_hex(code, 2);
            probe_s(" q");
            probe_hex(sq[q], 4);
            probe_ch(':');
            for (len = 0; len <= full; len++) {
                rv = map(code, sq[q], 0, 0, 0, 0, len, NULL);
                probe_s(" len");
                probe_dec(len);
                probe_ch('=');
                probe_dec(rv);
                probe_ch('/');
                for (i = 0; i < full; i++)
                    probe_hex(buf[i], 2), probe_ch(i + 1 < full ? ',' : ' ');
            }
            probe_ch('\n');
        }
    }

    P_SECTION("other event classes");
    {
        static const UBYTE classes[] = {IECLASS_NULL, IECLASS_RAWMOUSE, IECLASS_EVENT,
                                        IECLASS_TIMER, IECLASS_NEWPREFS};
        for (i = 0; i < 5; i++) {
            set_event(0x20, 0, 0, 0, 0, 0);
            ie.ie_Class = classes[i];
            fill();
            rv = MapRawKey(&ie, (STRPTR)buf, 32, NULL);
            probe_s("class ");
            probe_dec(classes[i]);
            probe_s(" code 0x20: ");
            put_result(rv);
            probe_ch('\n');
        }
        set_event(0x20, 0, 0, 0, 0, 0);
        ie.ie_SubClass = 1;
        fill();
        rv = MapRawKey(&ie, (STRPTR)buf, 32, NULL);
        probe_s("subclass 1 code 0x20: ");
        put_result(rv);
        probe_ch('\n');
    }

    P_SECTION("dead keys (default keymap)");
    {
        static const UBYTE pre[][2] = {
            {0, 0}, {0x23, 0x10}, {0x24, 0x10}, {0x25, 0x10}, {0x26, 0x10}, {0x27, 0x10},
            {0x23, 0x20}, {0x23, 0x11}, {0x24, 0x22}, {0x23, 0x00}, {0x23, 0x01},
            {0x23, 0x18}, {0x23, 0x14}, {0xa3, 0x10}, {0x20, 0x10}, {0x40, 0x00},
            {0x25, 0x30}, {0x26, 0x04}, {0x27, 0x12},
        };
        static const UBYTE deadable[] = {0x12, 0x15, 0x16, 0x17, 0x18, 0x20, 0x36, 0x40, 0x10, 0x23};
        static const UWORD dq[] = {0, IEQUALIFIER_LSHIFT, IEQUALIFIER_CAPSLOCK, IEQUALIFIER_LALT,
                                   IEQUALIFIER_CONTROL, IEQUALIFIER_RSHIFT};
        for (i = 0; i < (LONG)(sizeof(pre) / sizeof(pre[0])); i++) {
            probe_s("prev1 ");
            probe_hex(pre[i][0], 2);
            probe_ch('/');
            probe_hex(pre[i][1], 2);
            probe_ch(':');
            for (j = 0; j < (LONG)sizeof(deadable); j++)
                for (q = 0; q < 6; q++) {
                    probe_ch(' ');
                    put_result(map(deadable[j], dq[q], pre[i][0], pre[i][1], 0, 0, 32, NULL));
                }
            probe_ch('\n');
        }
        /* prev2 has no effect without a double-dead factor */
        for (i = 1; i < 6; i++) {
            probe_s("prev2 ");
            probe_hex(pre[i][0], 2);
            probe_s(" (prev1 none / prev1 0x24 alt):");
            for (j = 0; j < (LONG)sizeof(deadable); j++) {
                probe_ch(' ');
                put_result(map(deadable[j], 0, 0, 0, pre[i][0], pre[i][1], 32, NULL));
                probe_ch(' ');
                put_result(map(deadable[j], 0, 0x24, 0x10, pre[i][0], pre[i][1], 32, NULL));
            }
            probe_ch('\n');
        }
        /* dead key itself pressed: produces nothing */
        probe_s("dead key down, prev1 dead:");
        for (j = 0x23; j <= 0x27; j++) {
            probe_ch(' ');
            put_result(map(j, IEQUALIFIER_LALT, 0x24, 0x10, 0, 0, 32, NULL));
        }
        probe_ch('\n');
        probe_s("dead+mod with len 0:");
        probe_ch(' ');
        put_result(map(0x20, 0, 0x23, 0x10, 0, 0, 0, NULL));
        probe_ch(' ');
        put_result(map(0x20, 0, 0x23, 0x10, 0, 0, 1, NULL));
        probe_ch('\n');
    }

    build_custom(km);

    P_SECTION("custom keymap: key types (none LS LA LS+LA CTRL CTRL+LS CTRL+LA CTRL+LA+LS CAPS REPEAT)");
    for (code = 0; code < 0x23; code++) {
        if (code > 0x1b && code < 0x20)
            continue;
        probe_s("key ");
        probe_hex(code, 2);
        probe_ch(':');
        for (q = 0; q < sizeof(q8) / sizeof(q8[0]); q++) {
            probe_ch(' ');
            put_result(map(code, q8[q], 0, 0, 0, 0, 32, &ckm));
        }
        probe_ch('\n');
    }
    probe_s("caps+shift on capsable key 0x03:");
    probe_ch(' ');
    put_result(map(3, IEQUALIFIER_CAPSLOCK | IEQUALIFIER_LSHIFT, 0, 0, 0, 0, 32, &ckm));
    probe_ch(' ');
    put_result(map(3, IEQUALIFIER_CAPSLOCK | IEQUALIFIER_LALT, 0, 0, 0, 0, 32, &ckm));
    probe_ch(' ');
    put_result(map(0x0b, IEQUALIFIER_CAPSLOCK, 0, 0, 0, 0, 32, &ckm));
    probe_ch(' ');
    put_result(map(0x13, IEQUALIFIER_CAPSLOCK, 0, 0, 0, 0, 32, &ckm));
    probe_ch('\n');
    probe_s("string key 0x0f small buffers:");
    for (len = 0; len <= 8; len++) {
        probe_ch(' ');
        put_result(map(0x0f, IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_LSHIFT, 0, 0, 0, 0, len, &ckm));
    }
    probe_ch('\n');

    P_SECTION("custom keymap: double dead keys");
    {
        static const UBYTE pc[][2] = {
            {0, 0}, {0x19, 0}, {0x1a, 1}, {0x1a, 0}, {0x1b, 0}, {0x19, 1}, {0x22, 0},
        };
        for (i = 0; i < 7; i++) {
            probe_s("prev1 ");
            probe_hex(pc[i][0], 2);
            probe_ch('/');
            probe_hex(pc[i][1], 2);
            probe_s(" x prev2:");
            for (j = 0; j < 7; j++) {
                probe_ch(' ');
                put_result(map(0x20, 0, pc[i][0], pc[i][1], pc[j][0], pc[j][1], 32, &ckm));
            }
            probe_ch(' ');
            put_result(map(0x20, IEQUALIFIER_LSHIFT, pc[i][0], pc[i][1], 0x19, 0, 32, &ckm));
            probe_ch('\n');
        }
        probe_s("dead keys themselves:");
        probe_ch(' ');
        put_result(map(0x19, 0, 0, 0, 0, 0, 32, &ckm));
        probe_ch(' ');
        put_result(map(0x1a, IEQUALIFIER_LSHIFT, 0x19, 0, 0, 0, 32, &ckm));
        probe_ch(' ');
        put_result(map(0x1a, 0, 0, 0, 0, 0, 32, &ckm));
        probe_ch('\n');
    }

    CloseLibrary(KeymapBase);
    return 0;
}
