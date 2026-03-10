#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <devices/inputevent.h>
#include <devices/keymap.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/keymap_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/keymap.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Library *KeymapBase;

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

#define PACK4(b0, b1, b2, b3) \
    ((((ULONG)(UBYTE)(b0)) << 24) | (((ULONG)(UBYTE)(b1)) << 16) | \
     (((ULONG)(UBYTE)(b2)) << 8) | ((ULONG)(UBYTE)(b3)))

static UBYTE g_lo_types[64];
static ULONG g_lo_map[64];
static UBYTE g_lo_capsable[8];
static UBYTE g_lo_repeatable[8];
static UBYTE g_hi_types[0x38];
static ULONG g_hi_map[0x38];
static UBYTE g_hi_capsable[7];
static UBYTE g_hi_repeatable[7];
static UBYTE g_dead_descr[] = { DPF_DEAD, 1 };
static UBYTE g_deadable_a_descr[] = { DPF_MOD, 2, 'a', 0xE1 };
static UBYTE g_f1_descr[] = { 3, 2, 0x9B, '0', '~' };
static struct KeyMap g_custom_keymap;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + 15;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }

    do
    {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);

    if (neg)
        *--p = '-';

    print(p);
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

static void init_custom_keymap(void)
{
    LONG i;

    for (i = 0; i < 64; i++)
    {
        g_lo_types[i] = KCF_NOP;
        g_lo_map[i] = 0;
    }

    for (i = 0; i < 0x38; i++)
    {
        g_hi_types[i] = KCF_NOP;
        g_hi_map[i] = 0;
    }

    for (i = 0; i < 8; i++)
    {
        g_lo_capsable[i] = 0;
        g_lo_repeatable[i] = 0xFF;
        if (i < 7)
        {
            g_hi_capsable[i] = 0;
            g_hi_repeatable[i] = 0xFF;
        }
    }

    g_lo_types[0x20] = KC_VANILLA;
    g_lo_map[0x20] = PACK4(0, 0, 'A', 'a');

    g_lo_types[0x23] = KCF_DEAD | KC_NOQUAL;
    g_lo_map[0x23] = (ULONG)g_dead_descr;

    g_lo_types[0x24] = KCF_DEAD | KC_NOQUAL;
    g_lo_map[0x24] = (ULONG)g_deadable_a_descr;

    g_hi_types[RAWKEY_TAB - 0x40] = KC_NOQUAL;
    g_hi_map[RAWKEY_TAB - 0x40] = PACK4('\t', '\t', '\t', '\t');

    g_hi_types[RAWKEY_F1 - 0x40] = KCF_STRING | KC_NOQUAL;
    g_hi_map[RAWKEY_F1 - 0x40] = (ULONG)g_f1_descr;

    g_custom_keymap.km_LoKeyMapTypes = g_lo_types;
    g_custom_keymap.km_LoKeyMap = g_lo_map;
    g_custom_keymap.km_LoCapsable = g_lo_capsable;
    g_custom_keymap.km_LoRepeatable = g_lo_repeatable;
    g_custom_keymap.km_HiKeyMapTypes = g_hi_types;
    g_custom_keymap.km_HiKeyMap = g_hi_map;
    g_custom_keymap.km_HiCapsable = g_hi_capsable;
    g_custom_keymap.km_HiRepeatable = g_hi_repeatable;
}

static void test_ask_and_set_default(void)
{
    struct KeyMap custom_map;
    struct KeyMap *original;
    struct KeyMap *current;

    print("Test 1: AskKeyMapDefault / SetKeyMapDefault\n");

    original = AskKeyMapDefault();
    if (original != NULL)
        test_ok("AskKeyMapDefault returns non-NULL default");
    else
        test_fail_msg("AskKeyMapDefault returns non-NULL default");

    custom_map = *original;
    SetKeyMapDefault(&custom_map);
    current = AskKeyMapDefault();
    if (current == &custom_map)
        test_ok("SetKeyMapDefault installs supplied keymap pointer");
    else
        test_fail_msg("SetKeyMapDefault installs supplied keymap pointer");

    SetKeyMapDefault(NULL);
    current = AskKeyMapDefault();
    if (current == NULL)
        test_ok("SetKeyMapDefault accepts NULL keymap");
    else
        test_fail_msg("SetKeyMapDefault accepts NULL keymap");

    SetKeyMapDefault(original);
    current = AskKeyMapDefault();
    if (current == original)
        test_ok("SetKeyMapDefault restores original keymap");
    else
        test_fail_msg("SetKeyMapDefault restores original keymap");
}

static void init_raw_event(struct InputEvent *event, UWORD code, UWORD qual)
{
    event->ie_NextEvent = NULL;
    event->ie_Class = IECLASS_RAWKEY;
    event->ie_SubClass = 0;
    event->ie_Code = code;
    event->ie_Qualifier = qual;
    event->ie_Prev1DownCode = 0;
    event->ie_Prev1DownQual = 0;
    event->ie_Prev2DownCode = 0;
    event->ie_Prev2DownQual = 0;
}

static void test_maprawkey(void)
{
    struct InputEvent event;
    char buffer[8];
    WORD actual;

    print("\nTest 2: MapRawKey\n");

    init_raw_event(&event, 0x20, 0);
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == 'a'))
        test_ok("MapRawKey maps raw A to lowercase character");
    else
        test_fail_msg("MapRawKey maps raw A to lowercase character");

    init_raw_event(&event, 0x20, IEQUALIFIER_LSHIFT);
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == 'A'))
        test_ok("MapRawKey maps shifted raw A to uppercase character");
    else
        test_fail_msg("MapRawKey maps shifted raw A to uppercase character");

    init_raw_event(&event, 0x20, IEQUALIFIER_CONTROL);
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if ((actual == 1) && ((UBYTE)buffer[0] == 0x01))
        test_ok("MapRawKey maps control-A to control code");
    else
        test_fail_msg("MapRawKey maps control-A to control code");

    init_raw_event(&event, RAWKEY_F1, 0);
    actual = MapRawKey(&event, buffer, 2, &g_custom_keymap);
    if (actual == -1)
        test_ok("MapRawKey reports overflow for undersized string buffer");
    else
        test_fail_msg("MapRawKey reports overflow for undersized string buffer");

    init_raw_event(&event, IECODE_UP_PREFIX | 0x20, 0);
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if (actual == 0)
        test_ok("MapRawKey ignores key-up events");
    else
        test_fail_msg("MapRawKey ignores key-up events");
}

static void test_maprawkey_dead(void)
{
    struct InputEvent event;
    char buffer[8];
    WORD actual;

    print("\nTest 3: MapRawKey dead-key handling\n");

    init_raw_event(&event, 0x23, 0);
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if (actual == 0)
        test_ok("MapRawKey keeps dead key silent when pressed alone");
    else
        test_fail_msg("MapRawKey keeps dead key silent when pressed alone");

    init_raw_event(&event, 0x24, 0);
    event.ie_Prev1DownCode = 0x23;
    event.ie_Prev1DownQual = 0;
    actual = MapRawKey(&event, buffer, sizeof(buffer), &g_custom_keymap);
    if ((actual == 1) && ((UBYTE)buffer[0] == 0xE1))
        test_ok("MapRawKey combines dead key with deadable key output");
    else
        test_fail_msg("MapRawKey combines dead key with deadable key output");
}

static void test_mapansi(void)
{
    UBYTE buffer[12];
    LONG actual;

    print("\nTest 4: MapANSI\n");

    actual = MapANSI("a", 1, (STRPTR)buffer, 1, &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == 0x20) && (buffer[1] == 0))
        test_ok("MapANSI encodes lowercase A to raw key and qualifier");
    else
        test_fail_msg("MapANSI encodes lowercase A to raw key and qualifier");

    actual = MapANSI("A", 1, (STRPTR)buffer, 1, &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == 0x20) && (buffer[1] == IEQUALIFIER_LSHIFT))
        test_ok("MapANSI encodes uppercase A with shift qualifier");
    else
        test_fail_msg("MapANSI encodes uppercase A with shift qualifier");

    actual = MapANSI("\t", 1, (STRPTR)buffer, 1, &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == RAWKEY_TAB) && (buffer[1] == 0))
        test_ok("MapANSI encodes tab to raw tab key");
    else
        test_fail_msg("MapANSI encodes tab to raw tab key");

    actual = MapANSI("\2330~", 3, (STRPTR)buffer, 1, &g_custom_keymap);
    if ((actual == 1) && (buffer[0] == RAWKEY_F1) && (buffer[1] == 0))
        test_ok("MapANSI encodes F1 escape sequence");
    else
        test_fail_msg("MapANSI encodes F1 escape sequence");

    actual = MapANSI("\341", 1, (STRPTR)buffer, 3, &g_custom_keymap);
    if ((actual == 2) &&
        (buffer[0] == 0x23) && (buffer[1] == 0) &&
        (buffer[2] == 0x24) && (buffer[3] == 0))
        test_ok("MapANSI emits dead-key prefix for deadable output");
    else
        test_fail_msg("MapANSI emits dead-key prefix for deadable output");

    actual = MapANSI("a", 1, (STRPTR)buffer, 0, &g_custom_keymap);
    if (actual == -1)
        test_ok("MapANSI reports overflow when no WORD slots are available");
    else
        test_fail_msg("MapANSI reports overflow when no WORD slots are available");

    actual = MapANSI("\377", 1, (STRPTR)buffer, 3, &g_custom_keymap);
    if (actual == 0)
        test_ok("MapANSI returns zero for unmappable character");
    else
        test_fail_msg("MapANSI returns zero for unmappable character");
}

static void test_keymap_structure(void)
{
    struct KeyMap *keymap;

    print("\nTest 5: KeyMap structure\n");

    keymap = AskKeyMapDefault();
    if (sizeof(struct KeyMap) == 32)
        test_ok("KeyMap structure size matches NDK layout");
    else
        test_fail_msg("KeyMap structure size matches NDK layout");

    if (keymap && keymap->km_LoKeyMapTypes && keymap->km_LoKeyMap &&
        keymap->km_HiKeyMapTypes && keymap->km_HiKeyMap)
        test_ok("KeyMap structure exposes low/high table pointers");
    else
        test_fail_msg("KeyMap structure exposes low/high table pointers");
}

int main(void)
{
    out = Output();

    print("Keymap Library Tests\n");
    print("===================\n\n");

    KeymapBase = OpenLibrary("keymap.library", 37);
    if (!KeymapBase)
    {
        print("FAIL: Could not open keymap.library\n");
        return 20;
    }

    init_custom_keymap();

    test_ask_and_set_default();
    test_maprawkey();
    test_maprawkey_dead();
    test_mapansi();
    test_keymap_structure();

    CloseLibrary(KeymapBase);

    print("\nChecks passed: ");
    print_num(test_pass);
    print("\nChecks failed: ");
    print_num(test_fail);
    print("\n");

    if (test_fail == 0)
    {
        print("PASS: Keymap library tests passed\n");
        return 0;
    }

    print("FAIL: Keymap library tests failed\n");
    return 20;
}
