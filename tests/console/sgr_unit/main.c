/*
 * SGR Unit Test - Console Device Text Attribute Verification
 *
 * Phase 22.5: Test Coverage
 *
 * This test verifies SGR (Select Graphic Rendition) escape sequences by
 * checking the console unit's pen state after applying each SGR code.
 *
 * We read the public ConUnit fields (cu_FgPen, cu_BgPen, cu_DrawMode,
 * cu_AlgoStyle) via io_Unit after OpenDevice().  Validated against
 * AmigaOS 3.1 (Phase 220): SGR 7 sets INVERSVID in cu_DrawMode instead of
 * swapping the pens, SGR 1/3/4 set soft styles, SGR 22 restores the
 * default pen, and the window's RastPort is left untouched.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
#include <devices/conunit.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>


#define CSI "\x9b"   /* Amiga CSI (single byte) */

/* Console unit types */
#ifndef CONU_STANDARD
#define CONU_STANDARD 0
#endif

/* ConUnit structure (partial - we only need the pen fields) */
/* Full structure in devices/conunit.h but we inline the relevant fields */
struct ConUnitPens {
    /* We need to reach cu_FgPen and cu_BgPen which are after other fields */
    /* Offset calculation based on AROS conunit.h:
     * - cu_MP (MsgPort): ~34 bytes
     * - cu_Window (4 bytes)
     * - cu_XCP through cu_YCCP (14 WORDs = 28 bytes)
     * - cu_KeyMapStruct (variable, but we skip it)
     * - cu_TabStops (80 UWORDs = 160 bytes)
     * Total offset to cu_Mask is about 230 bytes
     */
    char padding[230];  /* Skip to cu_Mask */
    BYTE cu_Mask;
    BYTE cu_FgPen;
    BYTE cu_BgPen;
    BYTE cu_AOLPen;
    BYTE cu_DrawMode;
};

extern struct DOSBase *DOSBase;
extern struct GfxBase *GfxBase;

/* Global console I/O */
static struct IOStdReq *con_io = NULL;
static struct MsgPort *con_port = NULL;
static struct Window *test_win = NULL;
static struct IntuitionBase *IntuitionBase = NULL;

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(int n)
{
    char buf[16];
    int i = 0;
    if (n < 0) {
        print("-");
        n = -n;
    }
    if (n == 0) {
        print("0");
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0) {
        char c[2] = { buf[--i], '\0' };
        print(c);
    }
}

/*
 * Write to console device
 */
static LONG con_write(const char *str, LONG len)
{
    if (!con_io) return -1;

    con_io->io_Command = CMD_WRITE;
    con_io->io_Data = (APTR)str;
    con_io->io_Length = (len < 0) ? -1 : len;
    DoIO((struct IORequest *)con_io);

    return con_io->io_Actual;
}

/*
 * Write null-terminated string to console
 */
static LONG con_puts(const char *str)
{
    LONG result = con_write(str, -1);
    /* Multiple WaitTOF to ensure processing completes */
    WaitTOF();
    WaitTOF();
    return result;
}

/*
 * Send SGR sequence and write a character to force pen update
 * The console device only sets RastPort pens when actually drawing.
 */
static void send_sgr(const char *sgr)
{
    con_puts(sgr);
    /* Write a space character to force the pen update */
    con_puts(" ");
    /* Backspace to return cursor to original position */
    con_puts("\x08");
}

/*
 * Get the window's RastPort for checking pen state
 */
static struct RastPort *get_rast_port(void)
{
    if (test_win) {
        return test_win->RPort;
    }
    return NULL;
}

/*
 * Test that foreground pen matches expected value
 */
static void assert_fg_pen(int expected, const char *test_name)
{
    struct RastPort *rp = get_rast_port();
    int actual;

    tests_run++;

    if (!rp) {
        print("FAIL: ");
        print(test_name);
        print(" - no RastPort\n");
        tests_failed++;
        return;
    }

    actual = ((struct ConUnit *)con_io->io_Unit)->cu_FgPen;

    if (actual == expected) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected FgPen=");
        print_num(expected);
        print(" got ");
        print_num(actual);
        print("\n");
        tests_failed++;
    }
}

/*
 * Test that background pen matches expected value
 */
static void assert_bg_pen(int expected, const char *test_name)
{
    struct RastPort *rp = get_rast_port();
    int actual;

    tests_run++;

    if (!rp) {
        print("FAIL: ");
        print(test_name);
        print(" - no RastPort\n");
        tests_failed++;
        return;
    }

    actual = ((struct ConUnit *)con_io->io_Unit)->cu_BgPen;

    if (actual == expected) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected BgPen=");
        print_num(expected);
        print(" got ");
        print_num(actual);
        print("\n");
        tests_failed++;
    }
}

/*
 * Test that both pens match expected values
 */
static void assert_pens(int expected_fg, int expected_bg, const char *test_name)
{
    struct RastPort *rp = get_rast_port();
    int actual_fg, actual_bg;

    tests_run++;

    if (!rp) {
        print("FAIL: ");
        print(test_name);
        print(" - no RastPort\n");
        tests_failed++;
        return;
    }

    actual_fg = ((struct ConUnit *)con_io->io_Unit)->cu_FgPen;
    actual_bg = ((struct ConUnit *)con_io->io_Unit)->cu_BgPen;

    if (actual_fg == expected_fg && actual_bg == expected_bg) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected (fg=");
        print_num(expected_fg);
        print(",bg=");
        print_num(expected_bg);
        print(") got (fg=");
        print_num(actual_fg);
        print(",bg=");
        print_num(actual_bg);
        print(")\n");
        tests_failed++;
    }
}

static BYTE initial_rp_fg, initial_rp_bg, initial_rp_dm;

static struct ConUnit *get_con_unit(void)
{
    if (!con_io) {
        return NULL;
    }

    return (struct ConUnit *)con_io->io_Unit;
}

static void assert_mode(int expected_dm, int expected_style, const char *test_name)
{
    struct ConUnit *unit = get_con_unit();
    int dm = unit ? unit->cu_DrawMode : -1;
    int style = unit ? unit->cu_AlgoStyle : -1;

    tests_run++;
    if (dm == expected_dm && style == expected_style) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected (dm=");
        print_num(expected_dm);
        print(",style=");
        print_num(expected_style);
        print(") got (dm=");
        print_num(dm);
        print(",style=");
        print_num(style);
        print(")\n");
        tests_failed++;
    }
}

static void assert_window_rastport_untouched(const char *test_name)
{
    struct RastPort *rp = get_rast_port();

    tests_run++;
    if (rp && rp->FgPen == initial_rp_fg && rp->BgPen == initial_rp_bg && rp->DrawMode == initial_rp_dm) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" fg ");
        print_num(initial_rp_fg); print("->"); print_num(rp ? rp->FgPen : -1);
        print(" bg ");
        print_num(initial_rp_bg); print("->"); print_num(rp ? rp->BgPen : -1);
        print(" dm ");
        print_num(initial_rp_dm); print("->"); print_num(rp ? rp->DrawMode : -1);
        print("\n");
        tests_failed++;
    }
}

/*
 * Test: Reset attributes (SGR 0)
 */
static void test_sgr_reset(void)
{
    /* First set some non-default colors */
    send_sgr(CSI "35;42m");  /* Magenta on green */

    /* Now reset */
    send_sgr(CSI "0m");
    assert_pens(1, 0, "SGR 0 resets to default (fg=1, bg=0)");
}

/*
 * Test: Foreground colors (SGR 30-37)
 */
static void test_sgr_foreground(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    /* Test each foreground color */
    send_sgr(CSI "30m");
    assert_fg_pen(0, "SGR 30 sets fg to pen 0 (black)");

    send_sgr(CSI "31m");
    assert_fg_pen(1, "SGR 31 sets fg to pen 1 (red)");

    send_sgr(CSI "32m");
    assert_fg_pen(2, "SGR 32 sets fg to pen 2 (green)");

    send_sgr(CSI "33m");
    assert_fg_pen(3, "SGR 33 sets fg to pen 3 (yellow)");

    send_sgr(CSI "34m");
    assert_fg_pen(4, "SGR 34 sets fg to pen 4 (blue)");

    send_sgr(CSI "35m");
    assert_fg_pen(5, "SGR 35 sets fg to pen 5 (magenta)");

    send_sgr(CSI "36m");
    assert_fg_pen(6, "SGR 36 sets fg to pen 6 (cyan)");

    send_sgr(CSI "37m");
    assert_fg_pen(7, "SGR 37 sets fg to pen 7 (white)");

    /* Reset to default foreground */
    send_sgr(CSI "39m");
    assert_fg_pen(1, "SGR 39 resets fg to default (pen 1)");
}

/*
 * Test: Background colors (SGR 40-47)
 */
static void test_sgr_background(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    /* Test each background color */
    send_sgr(CSI "40m");
    assert_bg_pen(0, "SGR 40 sets bg to pen 0 (black)");

    send_sgr(CSI "41m");
    assert_bg_pen(1, "SGR 41 sets bg to pen 1 (red)");

    send_sgr(CSI "42m");
    assert_bg_pen(2, "SGR 42 sets bg to pen 2 (green)");

    send_sgr(CSI "43m");
    assert_bg_pen(3, "SGR 43 sets bg to pen 3 (yellow)");

    send_sgr(CSI "44m");
    assert_bg_pen(4, "SGR 44 sets bg to pen 4 (blue)");

    send_sgr(CSI "45m");
    assert_bg_pen(5, "SGR 45 sets bg to pen 5 (magenta)");

    send_sgr(CSI "46m");
    assert_bg_pen(6, "SGR 46 sets bg to pen 6 (cyan)");

    send_sgr(CSI "47m");
    assert_bg_pen(7, "SGR 47 sets bg to pen 7 (white)");

    /* Reset to default background */
    send_sgr(CSI "49m");
    assert_bg_pen(0, "SGR 49 resets bg to default (pen 0)");
}

/*
 * Test: Combined foreground and background
 */
static void test_sgr_combined(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    /* Set both colors in one sequence */
    send_sgr(CSI "33;44m");
    assert_pens(3, 4, "SGR 33;44 sets fg=3 (yellow), bg=4 (blue)");

    /* Another combination */
    send_sgr(CSI "31;47m");
    assert_pens(1, 7, "SGR 31;47 sets fg=1 (red), bg=7 (white)");

    /* Reset and verify */
    send_sgr(CSI "0m");
    assert_pens(1, 0, "SGR 0 after combined resets to default");
}

/*
 * Test: Inverse video (SGR 7)
 */
static void test_sgr_inverse(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    /* Enable inverse: INVERSVID draw mode, pens unchanged */
    send_sgr(CSI "7m");
    assert_pens(1, 0, "SGR 7 keeps the pens (fg=1, bg=0)");
    assert_mode(JAM2 | INVERSVID, 0, "SGR 7 sets INVERSVID");

    /* Disable inverse */
    send_sgr(CSI "27m");
    assert_mode(JAM2, 0, "SGR 27 clears INVERSVID");

    /* Test inverse with colors */
    send_sgr(CSI "32;45m");  /* Green on magenta */
    send_sgr(CSI "7m");       /* Inverse */
    assert_pens(2, 5, "SGR 7 with colors keeps the pens (fg=2, bg=5)");
    assert_mode(JAM2 | INVERSVID, 0, "SGR 7 with colors sets INVERSVID");

    /* Reset */
    send_sgr(CSI "0m");
}

/*
 * Test: Bold attribute (SGR 1)
 * Note: On Amiga, bold typically modifies the foreground pen
 */
static void test_sgr_bold(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    send_sgr(CSI "32m");
    send_sgr(CSI "1m");
    assert_mode(JAM2, FSF_BOLD, "SGR 1 sets FSF_BOLD");
    assert_fg_pen(2, "SGR 1 keeps the pen");

    send_sgr(CSI "3;4m");
    assert_mode(JAM2, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED, "SGR 3;4 add italic and underline");

    send_sgr(CSI "23;24m");
    assert_mode(JAM2, FSF_BOLD, "SGR 23;24 clear italic and underline");

    /* Normal colour, not bold */
    send_sgr(CSI "22m");
    assert_mode(JAM2, 0, "SGR 22 clears FSF_BOLD");
    assert_fg_pen(1, "SGR 22 restores the default pen");

    /* Reset */
    send_sgr(CSI "0m");
}

/*
 * Test: Multiple attributes in sequence
 */
static void test_sgr_multiple(void)
{
    /* Reset first */
    send_sgr(CSI "0m");

    /* Set multiple attributes: bold, yellow fg, blue bg */
    send_sgr(CSI "1;33;44m");
    assert_pens(3, 4, "SGR 1;33;44 sets fg=3, bg=4");
    assert_mode(JAM2, FSF_BOLD, "SGR 1;33;44 sets FSF_BOLD");

    /* Reset and verify */
    send_sgr(CSI "0m");
    assert_pens(1, 0, "SGR 0 resets all attributes");
    assert_mode(JAM2, 0, "SGR 0 resets draw mode and style");
}

/*
 * Test: Default reset (no parameter = 0)
 */
static void test_sgr_default_reset(void)
{
    /* Set some colors */
    send_sgr(CSI "34;43m");  /* Blue on yellow */

    /* Reset using bare m (no parameter) */
    send_sgr(CSI "m");
    assert_pens(1, 0, "CSI m (bare) resets to default");
}

static void test_esc_bracket_sgr_sequences(void)
{
    send_sgr(CSI "0m");

    con_puts("\x1b[31;44m ");
    con_puts("\x08");
    assert_pens(1, 4, "ESC[31;44m sets fg/bg pens");

    con_puts("\x1b[7m ");
    con_puts("\x08");
    assert_mode(JAM2 | INVERSVID, 0, "ESC[7m enables inverse video");

    con_puts("\x1b[27m ");
    con_puts("\x08");
    assert_mode(JAM2, 0, "ESC[27m disables inverse video");

    con_puts("\x1b[39;49m ");
    con_puts("\x08");
    assert_pens(1, 0, "ESC[39;49m restores default pens");

    assert_window_rastport_untouched("console output leaves the window RastPort pens alone");
}

/*
 * Open console device on test window
 */
static BOOL setup_console(void)
{
    struct NewWindow nw = {
        0, 0,          /* Left, Top */
        400, 200,      /* Width, Height */
        0, 1,          /* Detail, Block pens */
        IDCMP_RAWKEY | IDCMP_VANILLAKEY,
        WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DEPTHGADGET,
        NULL, NULL,
        (STRPTR)"SGR Unit Test",
        NULL, NULL,
        0, 0, 0, 0,
        WBENCHSCREEN
    };

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    if (!IntuitionBase) {
        print("FAIL: Cannot open intuition.library\n");
        return FALSE;
    }

    test_win = OpenWindow(&nw);
    if (!test_win) {
        print("FAIL: Cannot open test window\n");
        return FALSE;
    }

    con_port = CreateMsgPort();
    if (!con_port) {
        print("FAIL: Cannot create message port\n");
        return FALSE;
    }

    con_io = (struct IOStdReq *)CreateIORequest(con_port, sizeof(struct IOStdReq));
    if (!con_io) {
        print("FAIL: Cannot create IO request\n");
        return FALSE;
    }

    con_io->io_Data = (APTR)test_win;
    con_io->io_Length = sizeof(struct Window);

    if (OpenDevice((STRPTR)"console.device", CONU_STANDARD, (struct IORequest *)con_io, 0) != 0) {
        print("FAIL: Cannot open console.device\n");
        return FALSE;
    }
    initial_rp_fg = test_win->RPort->FgPen;
    initial_rp_bg = test_win->RPort->BgPen;
    initial_rp_dm = test_win->RPort->DrawMode;

    return TRUE;
}

/*
 * Cleanup console device
 */
static void cleanup_console(void)
{
    if (con_io) {
        CloseDevice((struct IORequest *)con_io);
        DeleteIORequest((struct IORequest *)con_io);
    }

    if (con_port) {
        DeleteMsgPort(con_port);
    }

    if (test_win) {
        CloseWindow(test_win);
    }

    if (IntuitionBase) {
        CloseLibrary((struct Library *)IntuitionBase);
    }
}

int main(void)
{
    print("=== SGR Unit Test ===\n");
    print("Testing console.device SGR escape sequences\n\n");

    if (!setup_console()) {
        print("FAIL: Setup failed\n");
        cleanup_console();
        return 1;
    }

    /* Run all tests */
    test_sgr_reset();
    test_sgr_foreground();
    test_sgr_background();
    test_sgr_combined();
    test_sgr_inverse();
    test_sgr_bold();
    test_sgr_multiple();
    test_sgr_default_reset();
    test_esc_bracket_sgr_sequences();

    /* Summary */
    print("\n=== Test Summary ===\n");
    print("Tests run: ");
    print_num(tests_run);
    print("\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\n");
    print("Failed: ");
    print_num(tests_failed);
    print("\n");

    cleanup_console();

    if (tests_failed > 0) {
        print("\nFAIL: Some tests failed\n");
        return 1;
    }

    print("\nPASS: All SGR unit tests passed\n");
    return 0;
}
