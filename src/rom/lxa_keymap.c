/*
 * lxa keymap.library implementation
 *
 * Provides keyboard mapping functionality for converting raw key events
 * to ANSI characters and vice versa.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/keymap.h>
#include <devices/inputevent.h>

#include "util.h"


#define VERSION    40
#define REVISION   4
#define EXLIBNAME  "keymap"
#define EXLIBVER   " 40.4 (12.3.93)\r\n"

char __aligned _g_keymap_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_keymap_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_keymap_Copyright [] = "(C)opyright 2025 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_keymap_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase *SysBase;

/* KeymapBase structure */
struct KeymapBase {
    struct Library        lib;
    UWORD                 Pad;
    BPTR                  SegList;
    struct KeyMap        *DefaultKeymap;
    struct KeyMapResource KeymapResource;
};

/****************************************************************************/
/* Default keymap: "usa", identical to the AmigaOS 3.1 built-in keymap      */
/* (tests/probes/keymap/tables.c dumps it on both systems).                 */
/****************************************************************************/

static const UBYTE km_d12[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xa9, 0x00, 0xa9, 0x00, 0x05, 0x00, 0x05, 0x00, 0x85, 0x00, 0x85, 0x65, 0xe9, 0xe8, 0xea, 0x65, 0xeb, 0x45, 0xc9, 0xc8, 0xca, 0x45, 0xcb };
static const UBYTE km_d15[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xa4, 0x00, 0xa5, 0x00, 0x19, 0x00, 0x19, 0x00, 0x99, 0x00, 0x99, 0x79, 0xfd, 0x79, 0x79, 0x79, 0xff, 0x59, 0xdd, 0x59, 0x59, 0x59, 0x59 };
static const UBYTE km_d16[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xb5, 0x00, 0xb5, 0x00, 0x15, 0x00, 0x15, 0x00, 0x95, 0x00, 0x95, 0x75, 0xfa, 0xf9, 0xfb, 0x75, 0xfc, 0x55, 0xda, 0xd9, 0xdb, 0x55, 0xdc };
static const UBYTE km_d17[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xa1, 0x00, 0xa6, 0x00, 0x09, 0x00, 0x09, 0x00, 0x89, 0x00, 0x89, 0x69, 0xed, 0xec, 0xee, 0x69, 0xef, 0x49, 0xcd, 0xcc, 0xce, 0x49, 0xcf };
static const UBYTE km_d18[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xf8, 0x00, 0xd8, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x8f, 0x00, 0x8f, 0x6f, 0xf3, 0xf2, 0xf4, 0xf5, 0xf6, 0x4f, 0xd3, 0xd2, 0xd4, 0xd5, 0xd6 };
static const UBYTE km_d20[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xe6, 0x00, 0xc6, 0x00, 0x01, 0x00, 0x01, 0x00, 0x81, 0x00, 0x81, 0x61, 0xe1, 0xe0, 0xe2, 0xe3, 0xe4, 0x41, 0xc1, 0xc0, 0xc2, 0xc3, 0xc4 };
static const UBYTE km_d23[] = { 0x00, 0x66, 0x00, 0x46, DPF_DEAD, 0x01, DPF_DEAD, 0x01, 0x00, 0x06, 0x00, 0x06, 0x00, 0x86, 0x00, 0x86 };
static const UBYTE km_d24[] = { 0x00, 0x67, 0x00, 0x47, DPF_DEAD, 0x02, DPF_DEAD, 0x02, 0x00, 0x07, 0x00, 0x07, 0x00, 0x87, 0x00, 0x87 };
static const UBYTE km_d25[] = { 0x00, 0x68, 0x00, 0x48, DPF_DEAD, 0x03, DPF_DEAD, 0x03, 0x00, 0x08, 0x00, 0x08, 0x00, 0x88, 0x00, 0x88 };
static const UBYTE km_d26[] = { 0x00, 0x6a, 0x00, 0x4a, DPF_DEAD, 0x04, DPF_DEAD, 0x04, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x8a, 0x00, 0x8a };
static const UBYTE km_d27[] = { 0x00, 0x6b, 0x00, 0x4b, DPF_DEAD, 0x05, DPF_DEAD, 0x05, 0x00, 0x0b, 0x00, 0x0b, 0x00, 0x8b, 0x00, 0x8b };
static const UBYTE km_d36[] = { DPF_MOD, 0x10, DPF_MOD, 0x16, 0x00, 0xad, 0x00, 0xaf, 0x00, 0x0e, 0x00, 0x0e, 0x00, 0x8e, 0x00, 0x8e, 0x6e, 0x6e, 0x6e, 0x6e, 0xf1, 0x6e, 0x4e, 0x4e, 0x4e, 0x4e, 0xd1, 0x4e };
static const UBYTE km_d40[] = { DPF_MOD, 0x04, 0x00, 0xa0, 0x20, 0xb4, 0x60, 0x5e, 0x7e, 0xa8 };
static const UBYTE km_d42[] = { 0x01, 0x04, 0x02, 0x05, 0x09, 0x9b, 0x5a };
static const UBYTE km_d47[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x30, 0x7e, 0x9b, 0x35, 0x30, 0x7e };
static const UBYTE km_d48[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x31, 0x7e, 0x9b, 0x35, 0x31, 0x7e };
static const UBYTE km_d49[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x32, 0x7e, 0x9b, 0x35, 0x32, 0x7e };
static const UBYTE km_d4b[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x32, 0x30, 0x7e, 0x9b, 0x33, 0x30, 0x7e };
static const UBYTE km_d4c[] = { 0x02, 0x04, 0x02, 0x06, 0x9b, 0x41, 0x9b, 0x54 };
static const UBYTE km_d4d[] = { 0x02, 0x04, 0x02, 0x06, 0x9b, 0x42, 0x9b, 0x53 };
static const UBYTE km_d4e[] = { 0x02, 0x04, 0x03, 0x06, 0x9b, 0x43, 0x9b, 0x20, 0x40 };
static const UBYTE km_d4f[] = { 0x02, 0x04, 0x03, 0x06, 0x9b, 0x44, 0x9b, 0x20, 0x41 };
static const UBYTE km_d50[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x30, 0x7e, 0x9b, 0x31, 0x30, 0x7e };
static const UBYTE km_d51[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x31, 0x7e, 0x9b, 0x31, 0x31, 0x7e };
static const UBYTE km_d52[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x32, 0x7e, 0x9b, 0x31, 0x32, 0x7e };
static const UBYTE km_d53[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x33, 0x7e, 0x9b, 0x31, 0x33, 0x7e };
static const UBYTE km_d54[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x34, 0x7e, 0x9b, 0x31, 0x34, 0x7e };
static const UBYTE km_d55[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x35, 0x7e, 0x9b, 0x31, 0x35, 0x7e };
static const UBYTE km_d56[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x36, 0x7e, 0x9b, 0x31, 0x36, 0x7e };
static const UBYTE km_d57[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x37, 0x7e, 0x9b, 0x31, 0x37, 0x7e };
static const UBYTE km_d58[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x38, 0x7e, 0x9b, 0x31, 0x38, 0x7e };
static const UBYTE km_d59[] = { 0x03, 0x04, 0x04, 0x07, 0x9b, 0x39, 0x7e, 0x9b, 0x31, 0x39, 0x7e };
static const UBYTE km_d5f[] = { 0x03, 0x02, 0x9b, 0x3f, 0x7e };
static const UBYTE km_d6e[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x33, 0x7e, 0x9b, 0x35, 0x33, 0x7e };
static const UBYTE km_d6f[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x32, 0x31, 0x7e, 0x9b, 0x33, 0x31, 0x7e };
static const UBYTE km_d70[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x34, 0x7e, 0x9b, 0x35, 0x34, 0x7e };
static const UBYTE km_d71[] = { 0x04, 0x04, 0x04, 0x08, 0x9b, 0x34, 0x35, 0x7e, 0x9b, 0x35, 0x35, 0x7e };

static const UBYTE lokeymaptypes[] =
{
    0x07, 0x03, 0x07, 0x03, 0x03, 0x03, 0x07, 0x03,
    0x03, 0x03, 0x03, 0x07, 0x01, 0x07, 0x80, 0x00,
    0x07, 0x07, 0x27, 0x07, 0x07, 0x27, 0x27, 0x27,
    0x27, 0x07, 0x07, 0x07, 0x80, 0x00, 0x00, 0x00,
    0x27, 0x07, 0x07, 0x27, 0x27, 0x27, 0x27, 0x27,
    0x07, 0x01, 0x01, 0x80, 0x80, 0x00, 0x00, 0x00,
    0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x27, 0x07,
    0x01, 0x01, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00,
};

static const ULONG lokeymap[] =
{
    /* 00 */ 0x7e607e60,
    /* 01 */ 0x21b92131,
    /* 02 */ 0x40b24032,
    /* 03 */ 0x23b32333,
    /* 04 */ 0x24a22434,
    /* 05 */ 0x25bc2535,
    /* 06 */ 0x5ebd5e36,
    /* 07 */ 0x26be2637,
    /* 08 */ 0x2ab72a38,
    /* 09 */ 0x28ab2839,
    /* 0a */ 0x29bb2930,
    /* 0b */ 0x5f2d5f2d,
    /* 0c */ 0x2b3d2b3d,
    /* 0d */ 0x7c5c7c5c,
    /* 0e */ 0,
    /* 0f */ 0x00000030,
    /* 10 */ 0xc5e55171,
    /* 11 */ 0xb0b05777,
    /* 12 */ (ULONG)km_d12,
    /* 13 */ 0xaeae5272,
    /* 14 */ 0xdefe5474,
    /* 15 */ (ULONG)km_d15,
    /* 16 */ (ULONG)km_d16,
    /* 17 */ (ULONG)km_d17,
    /* 18 */ (ULONG)km_d18,
    /* 19 */ 0xb6b65070,
    /* 1a */ 0x7b5b7b5b,
    /* 1b */ 0x7d5d7d5d,
    /* 1c */ 0,
    /* 1d */ 0x00000031,
    /* 1e */ 0x00000032,
    /* 1f */ 0x00000033,
    /* 20 */ (ULONG)km_d20,
    /* 21 */ 0xa7df5373,
    /* 22 */ 0xd0f04464,
    /* 23 */ (ULONG)km_d23,
    /* 24 */ (ULONG)km_d24,
    /* 25 */ (ULONG)km_d25,
    /* 26 */ (ULONG)km_d26,
    /* 27 */ (ULONG)km_d27,
    /* 28 */ 0xa3a34c6c,
    /* 29 */ 0x3a3b3a3b,
    /* 2a */ 0x22272227,
    /* 2b */ 0,
    /* 2c */ 0,
    /* 2d */ 0x00000034,
    /* 2e */ 0x00000035,
    /* 2f */ 0x00000036,
    /* 30 */ 0xbbab3e3c,
    /* 31 */ 0xacb15a7a,
    /* 32 */ 0xf7d75878,
    /* 33 */ 0xc7e74363,
    /* 34 */ 0xaaaa5676,
    /* 35 */ 0xbaba4262,
    /* 36 */ (ULONG)km_d36,
    /* 37 */ 0xbfb84d6d,
    /* 38 */ 0x3c2c3c2c,
    /* 39 */ 0x3e2e3e2e,
    /* 3a */ 0x3f2f3f2f,
    /* 3b */ 0,
    /* 3c */ 0x0000002e,
    /* 3d */ 0x00000037,
    /* 3e */ 0x00000038,
    /* 3f */ 0x00000039,
};

static const UBYTE locapsable[] =
{
    0x00, 0x00, 0xff, 0x03, 0xff, 0x01, 0xfe, 0x00,
};

static const UBYTE lorepeatable[] =
{
    0xff, 0xbf, 0xff, 0xef, 0xff, 0xef, 0xff, 0xf7,
};

static const UBYTE hikeymaptypes[] =
{
    0x22, 0x00, 0x41, 0x00, 0x04, 0x02, 0x00, 0x41,
    0x41, 0x41, 0x00, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
    0x41, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x41, 0x41,
    0x41, 0x41, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
};

static const ULONG hikeymap[] =
{
    /* 40 */ (ULONG)km_d40,
    /* 41 */ 0x00000008,
    /* 42 */ (ULONG)km_d42,
    /* 43 */ 0x0000000d,
    /* 44 */ 0x00000a0d,
    /* 45 */ 0x00009b1b,
    /* 46 */ 0x0000007f,
    /* 47 */ (ULONG)km_d47,
    /* 48 */ (ULONG)km_d48,
    /* 49 */ (ULONG)km_d49,
    /* 4a */ 0x0000002d,
    /* 4b */ (ULONG)km_d4b,
    /* 4c */ (ULONG)km_d4c,
    /* 4d */ (ULONG)km_d4d,
    /* 4e */ (ULONG)km_d4e,
    /* 4f */ (ULONG)km_d4f,
    /* 50 */ (ULONG)km_d50,
    /* 51 */ (ULONG)km_d51,
    /* 52 */ (ULONG)km_d52,
    /* 53 */ (ULONG)km_d53,
    /* 54 */ (ULONG)km_d54,
    /* 55 */ (ULONG)km_d55,
    /* 56 */ (ULONG)km_d56,
    /* 57 */ (ULONG)km_d57,
    /* 58 */ (ULONG)km_d58,
    /* 59 */ (ULONG)km_d59,
    /* 5a */ 0x00000028,
    /* 5b */ 0x00000029,
    /* 5c */ 0x0000002f,
    /* 5d */ 0x0000002a,
    /* 5e */ 0x0000002b,
    /* 5f */ (ULONG)km_d5f,
    /* 60 */ 0,
    /* 61 */ 0,
    /* 62 */ 0,
    /* 63 */ 0,
    /* 64 */ 0,
    /* 65 */ 0,
    /* 66 */ 0,
    /* 67 */ 0,
    /* 68 */ 0,
    /* 69 */ 0,
    /* 6a */ 0,
    /* 6b */ 0,
    /* 6c */ 0,
    /* 6d */ 0,
    /* 6e */ (ULONG)km_d6e,
    /* 6f */ (ULONG)km_d6f,
    /* 70 */ (ULONG)km_d70,
    /* 71 */ (ULONG)km_d71,
    /* 72 */ 0,
    /* 73 */ 0,
    /* 74 */ 0,
    /* 75 */ 0,
    /* 76 */ 0,
    /* 77 */ 0,
};

static const UBYTE hicapsable[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const UBYTE hirepeatable[] =
{
    0x47, 0xff, 0xff, 0x7f, 0x00, 0x80, 0x00,
};

/* Default keymap structure */
static struct KeyMap default_keymap =
{
    (UBYTE *)lokeymaptypes,
    (ULONG *)lokeymap,
    (UBYTE *)locapsable,
    (UBYTE *)lorepeatable,
    (UBYTE *)hikeymaptypes,
    (ULONG *)hikeymap,
    (UBYTE *)hicapsable,
    (UBYTE *)hirepeatable,
};

/****************************************************************************/
/* Helper functions                                                          */
/****************************************************************************/

#define NUM_KEYS 0x78   /* 0x00-0x3f low map, 0x40-0x77 high map */

/* number of entries (characters / strings / dead descriptors) per type */
static const UBYTE g_num_entries[8] = { 1, 2, 2, 4, 2, 4, 4, 8 };

/* IEQUALIFIER low byte for each entry of each key type (KCF_ bits & 7) */
static const UBYTE g_entry_qual[8][8] =
{
    { 0 },
    { 0, IEQUALIFIER_LSHIFT },
    { 0, IEQUALIFIER_LALT },
    { 0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT, IEQUALIFIER_LSHIFT | IEQUALIFIER_LALT },
    { 0, IEQUALIFIER_CONTROL },
    { 0, IEQUALIFIER_LSHIFT, IEQUALIFIER_CONTROL, IEQUALIFIER_LSHIFT | IEQUALIFIER_CONTROL },
    { 0, IEQUALIFIER_LALT, IEQUALIFIER_CONTROL, IEQUALIFIER_LALT | IEQUALIFIER_CONTROL },
    { 0, IEQUALIFIER_LSHIFT, IEQUALIFIER_LALT, IEQUALIFIER_LSHIFT | IEQUALIFIER_LALT,
      IEQUALIFIER_CONTROL, IEQUALIFIER_CONTROL | IEQUALIFIER_LSHIFT,
      IEQUALIFIER_CONTROL | IEQUALIFIER_LALT,
      IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_LSHIFT },
};

static UBYTE key_type(const struct KeyMap *km, UWORD code)
{
    return code < 0x40 ? km->km_LoKeyMapTypes[code] : km->km_HiKeyMapTypes[code - 0x40];
}

static ULONG key_map(const struct KeyMap *km, UWORD code)
{
    return code < 0x40 ? km->km_LoKeyMap[code] : km->km_HiKeyMap[code - 0x40];
}

static BOOL key_bit(const UBYTE *lo, const UBYTE *hi, UWORD code)
{
    if (code < 0x40)
        return (lo[code >> 3] >> (code & 7)) & 1;
    code -= 0x40;
    return (hi[code >> 3] >> (code & 7)) & 1;
}

/*
 * Entry index of a key press: the KCF_SHIFT/ALT/CONTROL qualifiers that
 * the key type does not list are ignored, the remaining ones select the
 * entry in the order of g_entry_qual.
 */
static WORD entry_index(UBYTE type, UBYTE kcf)
{
    WORD idx = 0, bit = 0;
    UBYTE t;

    for (t = 1; t <= KCF_CONTROL; t <<= 1)
    {
        if (type & t)
        {
            if (kcf & t)
                idx |= 1 << bit;
            bit++;
        }
    }
    return idx;
}

struct KeyPress
{
    UBYTE type;
    ULONG map;
    UBYTE kcf;      /* KCF_SHIFT/ALT/CONTROL of the press */
};

/* Decode code + qualifier; FALSE if the key produces nothing. */
static BOOL get_key_press(struct KeyPress *kp, UWORD code, UWORD qual, const struct KeyMap *km)
{
    if (code >= NUM_KEYS)
        return FALSE;   /* also rejects key-up codes (IECODE_UP_PREFIX) */

    kp->type = key_type(km, code);
    kp->map  = key_map(km, code);
    kp->kcf  = 0;

    if ((qual & IEQUALIFIER_REPEAT) && !key_bit(km->km_LoRepeatable, km->km_HiRepeatable, code))
        return FALSE;

    if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
        kp->kcf |= KCF_SHIFT;
    if ((qual & IEQUALIFIER_CAPSLOCK) && key_bit(km->km_LoCapsable, km->km_HiCapsable, code))
        kp->kcf |= KCF_SHIFT;
    if (qual & (IEQUALIFIER_LALT | IEQUALIFIER_RALT))
        kp->kcf |= KCF_ALT;
    if (qual & IEQUALIFIER_CONTROL)
        kp->kcf |= KCF_CONTROL;

    return TRUE;
}

/* Dead key descriptor value (index | factor << 4) of a previous key, or -1. */
static WORD dead_key_value(UBYTE code, UBYTE qual, const struct KeyMap *km)
{
    struct KeyPress kp;
    const UBYTE *d;
    WORD idx;

    if (!get_key_press(&kp, code, qual, km))
        return -1;
    if ((kp.type & KCF_NOP) || !(kp.type & KCF_DEAD))
        return -1;

    idx = entry_index(kp.type, kp.kcf);
    d = (const UBYTE *)kp.map;
    if (d[idx * 2] != DPF_DEAD)
        return -1;
    return d[idx * 2 + 1];
}

/* Control-key mapping of a KC_VANILLA key: the first of the four
 * characters, starting at the one the shift/alt state selects, that lies
 * in 0x40-0x7f gives (c & 0x1f), plus bit 7 with alt. */
static BOOL vanilla_control(ULONG map, UBYTE kcf, UBYTE *out)
{
    WORD start = ((kcf & KCF_SHIFT) ? 1 : 0) + ((kcf & KCF_ALT) ? 2 : 0);
    WORD i;

    for (i = 0; i < 4; i++)
    {
        UBYTE c = (UBYTE)(map >> (((start + i) & 3) * 8));
        if ((c & 0xc0) == 0x40)
        {
            *out = (UBYTE)((c & 0x1f) | ((kcf & KCF_ALT) ? 0x80 : 0));
            return TRUE;
        }
    }
    return FALSE;
}

/****************************************************************************/
/* Library management functions                                              */
/****************************************************************************/

struct KeymapBase * __g_lxa_keymap_InitLib ( register struct KeymapBase *kmb    __asm("d0"),
                                              register BPTR               seglist __asm("a0"),
                                              register struct ExecBase   *sysb    __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: InitLib() called\n");
    kmb->SegList = seglist;
    kmb->DefaultKeymap = &default_keymap;

    /* Initialize KeyMapResource */
    kmb->KeymapResource.kr_Node.ln_Type = NT_RESOURCE;
    kmb->KeymapResource.kr_Node.ln_Name = "keymap.resource";

    /* Initialize the list manually */
    kmb->KeymapResource.kr_List.lh_Head = (struct Node *)&kmb->KeymapResource.kr_List.lh_Tail;
    kmb->KeymapResource.kr_List.lh_Tail = NULL;
    kmb->KeymapResource.kr_List.lh_TailPred = (struct Node *)&kmb->KeymapResource.kr_List.lh_Head;

    return kmb;
}

struct KeymapBase * __g_lxa_keymap_OpenLib ( register struct KeymapBase *KeymapBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: OpenLib() called\n");
    KeymapBase->lib.lib_OpenCnt++;
    KeymapBase->lib.lib_Flags &= ~LIBF_DELEXP;
    return KeymapBase;
}

BPTR __g_lxa_keymap_CloseLib ( register struct KeymapBase *kmb __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: CloseLib() called\n");
    kmb->lib.lib_OpenCnt--;
    return (BPTR)0;
}

BPTR __g_lxa_keymap_ExpungeLib ( register struct KeymapBase *kmb __asm("a6"))
{
    return (BPTR)0;
}

ULONG __g_lxa_keymap_ExtFuncLib(void)
{
    PRIVATE_FUNCTION_ERROR("_keymap", "ExtFuncLib");
    return 0;
}

/****************************************************************************/
/* Main functions                                                            */
/****************************************************************************/

VOID _keymap_SetKeyMapDefault ( register struct KeymapBase *KeymapBase __asm("a6"),
                                register struct KeyMap     *keyMap     __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_keymap: SetKeyMapDefault() called keyMap=0x%08lx\n", keyMap);

    KeymapBase->DefaultKeymap = keyMap;
}

struct KeyMap * _keymap_AskKeyMapDefault ( register struct KeymapBase *KeymapBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_keymap: AskKeyMapDefault() called, returning 0x%08lx\n",
             KeymapBase->DefaultKeymap);
    return KeymapBase->DefaultKeymap;
}

WORD _keymap_MapRawKey ( register struct KeymapBase  *KeymapBase __asm("a6"),
                         register struct InputEvent  *event      __asm("a0"),
                         register STRPTR              buffer     __asm("a1"),
                         register LONG                length     __asm("d1"),
                         register struct KeyMap      *keyMap     __asm("a2"))
{
    struct KeyPress kp;
    const UBYTE *out = NULL;
    UBYTE ch;
    WORD len = 0, i;
    WORD buflen = (WORD)length;

    DPRINTF (LOG_DEBUG, "_keymap: MapRawKey() called class=%d code=0x%04x qual=0x%04x\n",
             event ? event->ie_Class : 0,
             event ? event->ie_Code : 0,
             event ? event->ie_Qualifier : 0);

    if (!keyMap)
        keyMap = KeymapBase->DefaultKeymap;

    if (!keyMap || !event || event->ie_Class != IECLASS_RAWKEY)
        return 0;

    if (!get_key_press(&kp, event->ie_Code, event->ie_Qualifier, keyMap))
        return 0;

    if (kp.type & KCF_NOP)
        return 0;

    if (kp.type & KCF_STRING)
    {
        const UBYTE *d = (const UBYTE *)kp.map;
        WORD idx = entry_index(kp.type, kp.kcf);

        len = d[idx * 2];
        out = d + d[idx * 2 + 1];
    }
    else if (kp.type & KCF_DEAD)
    {
        const UBYTE *d = (const UBYTE *)kp.map;
        WORD idx = entry_index(kp.type, kp.kcf);
        UBYTE flag = d[idx * 2];
        UBYTE val  = d[idx * 2 + 1];

        if (flag == DPF_DEAD)
            return 0;           /* a dead key itself produces nothing */

        if (flag & DPF_MOD)
        {
            WORD dk = 0;
            WORD v1 = dead_key_value(event->ie_Prev1DownCode, event->ie_Prev1DownQual, keyMap);

            if (v1 >= 0)
            {
                WORD fac = v1 >> DP_2DFACSHIFT;

                dk = v1 & DP_2DINDEXMASK;
                if (fac)
                {
                    WORD v2 = dead_key_value(event->ie_Prev2DownCode, event->ie_Prev2DownQual, keyMap);

                    dk *= fac;
                    if (v2 >= 0)
                        dk += v2 & DP_2DINDEXMASK;
                }
            }
            ch = d[val + dk];
        }
        else
        {
            ch = val;
        }
        out = &ch;
        len = 1;
    }
    else
    {
        if ((kp.kcf & KCF_CONTROL) && (kp.type & KC_VANILLA) == KC_VANILLA &&
            vanilla_control(kp.map, kp.kcf, &ch))
        {
            /* Ctrl on a vanilla key */
        }
        else
        {
            /* KC_VANILLA has no Ctrl entries: Ctrl is ignored here */
            WORD idx = entry_index(kp.type & KC_VANILLA, kp.kcf);
            if (idx > 3)
                idx &= 3;
            ch = (UBYTE)(kp.map >> (idx * 8));
        }
        out = &ch;
        len = 1;
    }

    if (len > buflen)
    {
        DPRINTF (LOG_DEBUG, "_keymap: MapRawKey() buffer overflow\n");
        return -1;
    }

    for (i = 0; i < len; i++)
        buffer[i] = out[i];

    return len;
}

/****************************************************************************/
/* MapANSI                                                                   */
/****************************************************************************/

/* a way to type one or more characters */
struct AnsiCand
{
    WORD  len;      /* characters of the input consumed (0 = none found) */
    WORD  dead;     /* dead key index needed before the key (0 = none) */
    UBYTE code;
    UBYTE qual;
    BOOL  ctrl;     /* found through the vanilla control rule */
};

static WORD qual_bits(UBYTE q)
{
    WORD n = 0;
    while (q)
    {
        n += q & 1;
        q >>= 1;
    }
    return n;
}

/* pairs of prefix dead keys a dead index needs: 1 single, 2 double */
static WORD dead_pairs(WORD dead, WORD max_dead)
{
    return dead == 0 ? 0 : (dead <= max_dead ? 1 : 2);
}

/* keys are scanned upwards; a later candidate replaces the current one
 * only if it is strictly better */
static void offer(struct AnsiCand *best, WORD len, WORD dead, WORD max_dead,
                  UBYTE code, UBYTE qual)
{
    if (best->len)
    {
        if (len < best->len)
            return;
        if (len == best->len)
        {
            WORD np = dead_pairs(dead, max_dead), bp = dead_pairs(best->dead, max_dead);

            if (np > bp)
                return;
            if (np == bp && qual_bits(qual) >= qual_bits(best->qual))
                return;
        }
    }
    best->len  = len;
    best->dead = dead;
    best->code = code;
    best->qual = qual;
    best->ctrl = FALSE;
}

LONG _keymap_MapANSI ( register struct KeymapBase *KeymapBase __asm("a6"),
                       register STRPTR             string     __asm("a0"),
                       register LONG               count      __asm("d0"),
                       register STRPTR             buffer     __asm("a1"),
                       register LONG               length     __asm("d1"),
                       register struct KeyMap     *keyMap     __asm("a2"))
{
    UBYTE dead_code[16], dead_qual[16];
    BOOL  dead_valid[16];
    WORD  max_dead = 0;
    BOOL  have_double = FALSE;
    const UBYTE *in = (const UBYTE *)string;
    UBYTE *out = (UBYTE *)buffer;
    LONG pairs = 0;
    UWORD code;
    WORD k;

    DPRINTF (LOG_DEBUG, "_keymap: MapANSI() called count=%ld length=%ld\n", count, length);

    if (!keyMap)
        keyMap = KeymapBase->DefaultKeymap;
    if (!keyMap)
        return -2;

    /* collect the dead keys: for each index the first key needing the
     * fewest qualifiers */
    for (k = 0; k < 16; k++)
        dead_valid[k] = FALSE;

    for (code = 0; code < NUM_KEYS; code++)
    {
        UBYTE type = key_type(keyMap, code);
        const UBYTE *d = (const UBYTE *)key_map(keyMap, code);

        if ((type & KCF_NOP) || !(type & KCF_DEAD))
            continue;

        for (k = 0; k < g_num_entries[type & 7]; k++)
        {
            if (d[k * 2] == DPF_DEAD)
            {
                WORD idx = d[k * 2 + 1] & DP_2DINDEXMASK;
                UBYTE q = g_entry_qual[type & 7][k];

                if (d[k * 2 + 1] >> DP_2DFACSHIFT)
                    have_double = TRUE;
                if (idx > max_dead)
                    max_dead = idx;
                if (!dead_valid[idx] || qual_bits(q) < qual_bits(dead_qual[idx]))
                {
                    dead_valid[idx] = TRUE;
                    dead_code[idx]  = (UBYTE)code;
                    dead_qual[idx]  = q;
                }
            }
        }
    }

    while (count > 0)
    {
        struct AnsiCand best;
        UBYTE c = *in;
        WORD need;

        if (length <= 0)
            return -1;

        best.len = 0;
        best.dead = 0;
        best.code = 0;
        best.qual = 0;
        best.ctrl = FALSE;

        for (code = 0; code < NUM_KEYS; code++)
        {
            UBYTE type = key_type(keyMap, code);
            ULONG map  = key_map(keyMap, code);
            WORD  n    = g_num_entries[type & 7];

            if (type & KCF_NOP)
                continue;

            if (type & KCF_STRING)
            {
                const UBYTE *d = (const UBYTE *)map;

                for (k = 0; k < n; k++)
                {
                    WORD slen = d[k * 2], i;
                    const UBYTE *s = d + d[k * 2 + 1];

                    if (slen == 0 || slen > count)
                        continue;
                    for (i = 0; i < slen && in[i] == s[i]; i++)
                        ;
                    if (i == slen)
                        offer(&best, slen, 0, max_dead, (UBYTE)code, g_entry_qual[type & 7][k]);
                }
            }
            else if (type & KCF_DEAD)
            {
                const UBYTE *d = (const UBYTE *)map;

                for (k = 0; k < n; k++)
                {
                    UBYTE q = g_entry_qual[type & 7][k];

                    if (d[k * 2] == 0)
                    {
                        if (d[k * 2 + 1] == c)
                            offer(&best, 1, 0, max_dead, (UBYTE)code, q);
                    }
                    else if (d[k * 2] & DPF_MOD)
                    {
                        const UBYTE *t = d + d[k * 2 + 1];
                        WORD size = max_dead + 1, i;

                        if (have_double)
                            size += max_dead * max_dead;

                        for (i = 0; i < size; i++)
                        {
                            if (t[i] != c)
                                continue;
                            if (i > 0)
                            {
                                if (i <= max_dead)
                                {
                                    if (!dead_valid[i])
                                        continue;
                                }
                                else
                                {
                                    WORD p1 = (i - 1) % max_dead + 1;
                                    WORD p2 = (i - p1) / max_dead;

                                    if (!dead_valid[p1] || !dead_valid[p2])
                                        continue;
                                }
                            }
                            offer(&best, 1, i, max_dead, (UBYTE)code, q);
                        }
                    }
                }
            }
            else
            {
                for (k = 0; k < n; k++)
                    if ((UBYTE)(map >> (k * 8)) == c)
                        offer(&best, 1, 0, max_dead, (UBYTE)code, g_entry_qual[type & 7][k]);

                /* Ctrl on a vanilla key: taken only if nothing better is known */
                if ((type & KC_VANILLA) == KC_VANILLA && (c & 0x60) == 0 &&
                    (best.len == 0 || best.dead != 0))
                {
                    UBYTE q = IEQUALIFIER_CONTROL | ((c & 0x80) ? IEQUALIFIER_LALT : 0);
                    UBYTE b0 = (UBYTE)map, b1 = (UBYTE)(map >> 8);
                    BOOL hit = FALSE;

                    if ((b0 & 0xc0) == 0x40 && (b0 & 0x1f) == (c & 0x1f))
                        hit = TRUE;
                    else if ((b1 & 0xc0) == 0x40 && (b1 & 0x1f) == (c & 0x1f))
                    {
                        hit = TRUE;
                        q |= IEQUALIFIER_LSHIFT;
                    }
                    if (hit)
                    {
                        best.len  = 1;
                        best.dead = 0;
                        best.code = (UBYTE)code;
                        best.qual = q;
                        best.ctrl = TRUE;
                    }
                }
            }
        }

        if (best.len == 0)
            return 0;

        need = 1 + dead_pairs(best.dead, max_dead);
        if (need > length)
            return -1;

        if (best.dead > max_dead)
        {
            WORD p1 = (best.dead - 1) % max_dead + 1;
            WORD p2 = (best.dead - p1) / max_dead;

            *out++ = dead_code[p2];
            *out++ = dead_qual[p2];
            *out++ = dead_code[p1];
            *out++ = dead_qual[p1];
        }
        else if (best.dead)
        {
            *out++ = dead_code[best.dead];
            *out++ = dead_qual[best.dead];
        }
        *out++ = best.code;
        *out++ = best.qual;

        length -= need;
        pairs  += need;
        count  -= best.len;
        in     += best.len;
    }

    return pairs;
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

extern APTR              __g_lxa_keymap_FuncTab [];
extern struct MyDataInit __g_lxa_keymap_DataTab;
extern struct InitTable  __g_lxa_keymap_InitTab;
extern APTR              __g_lxa_keymap_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                      // UWORD rt_MatchWord
    &ROMTag,                            // struct Resident *rt_MatchTag
    &__g_lxa_keymap_EndResident,        // APTR  rt_EndSkip
    RTF_AUTOINIT,                       // UBYTE rt_Flags
    VERSION,                            // UBYTE rt_Version
    NT_LIBRARY,                         // UBYTE rt_Type
    0,                                  // BYTE  rt_Pri
    &_g_keymap_ExLibName[0],            // char  *rt_Name
    &_g_keymap_ExLibID[0],              // char  *rt_IdString
    &__g_lxa_keymap_InitTab             // APTR  rt_Init
};

APTR __g_lxa_keymap_EndResident;
struct Resident *__lxa_keymap_ROMTag = &ROMTag;

struct InitTable __g_lxa_keymap_InitTab =
{
    (ULONG)               sizeof(struct KeymapBase),
    (APTR              *) &__g_lxa_keymap_FuncTab[0],
    (APTR)                &__g_lxa_keymap_DataTab,
    (APTR)                __g_lxa_keymap_InitLib
};

/* Function table */
APTR __g_lxa_keymap_FuncTab [] =
{
    __g_lxa_keymap_OpenLib,             // -6   (0x06) pos 0
    __g_lxa_keymap_CloseLib,            // -12  (0x0c) pos 1
    __g_lxa_keymap_ExpungeLib,          // -18  (0x12) pos 2
    __g_lxa_keymap_ExtFuncLib,          // -24  (0x18) pos 3
    _keymap_SetKeyMapDefault,           // -30  (0x1e) pos 4
    _keymap_AskKeyMapDefault,           // -36  (0x24) pos 5
    _keymap_MapRawKey,                  // -42  (0x2a) pos 6
    _keymap_MapANSI,                    // -48  (0x30) pos 7
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_keymap_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_keymap_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_keymap_ExLibID[0],
    (ULONG) 0
};
