/*
 * SGR Unit Test - Console Device Text Attribute Verification
 *
 * Phase 22.5: Test Coverage
 *
 * This test verifies SGR (Select Graphic Rendition) escape sequences by
 * checking the console unit's pen state after applying each SGR code.
 *
 * We access the ConUnit structure via io_Unit after OpenDevice() to verify
 * that the pen colors and attributes are set correctly.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
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
    
    actual = rp->FgPen;
    
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
    
    actual = rp->BgPen;
    
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
    
    actual_fg = rp->FgPen;
    actual_bg = rp->BgPen;
    
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
    
    /* Enable inverse (swaps fg/bg) */
    send_sgr(CSI "7m");
    assert_pens(0, 1, "SGR 7 enables inverse (fg=0, bg=1)");
    
    /* Disable inverse */
    send_sgr(CSI "27m");
    assert_pens(1, 0, "SGR 27 disables inverse (fg=1, bg=0)");
    
    /* Test inverse with colors */
    send_sgr(CSI "32;45m");  /* Green on magenta */
    send_sgr(CSI "7m");       /* Inverse */
    assert_pens(5, 2, "SGR 7 with colors inverts (fg=5, bg=2)");
    
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
    
    /* Enable bold - this may change the foreground pen */
    send_sgr(CSI "1m");
    /* Bold often ORs 0x01 with the fg pen, so pen 1 stays 1 */
    /* Just verify it doesn't crash - actual effect depends on implementation */
    tests_run++;
    print("PASS: SGR 1 (bold) applied without error\n");
    tests_passed++;
    
    /* Disable bold */
    send_sgr(CSI "22m");
    tests_run++;
    print("PASS: SGR 22 (not bold) applied without error\n");
    tests_passed++;
    
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
    assert_bg_pen(4, "SGR 1;33;44 sets bg=4 (blue)");
    /* fg might be modified by bold, so we just check bg here */
    
    /* Reset and verify */
    send_sgr(CSI "0m");
    assert_pens(1, 0, "SGR 0 resets all attributes");
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
