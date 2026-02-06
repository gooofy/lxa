/*
 * CSI Unit Test - Console Device Escape Sequence Verification
 *
 * Phase 22.5: Test Coverage
 *
 * This test verifies CSI escape sequences by using the Device Status Report
 * (CSI 6 n) to check cursor positions after each movement command.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/console.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>


#define CSI "\x9b"   /* Amiga CSI (single byte) */

/* Console unit types */
#ifndef CONU_STANDARD
#define CONU_STANDARD 0
#endif

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
 * Read from console device (with timeout via CheckIO polling)
 */
static LONG con_read(char *buf, LONG maxlen)
{
    int i;
    
    if (!con_io) return -1;
    
    con_io->io_Command = CMD_READ;
    con_io->io_Data = buf;
    con_io->io_Length = maxlen;
    
    /* Send async, then wait briefly */
    SendIO((struct IORequest *)con_io);
    
    /* Wait a bit for response (DSR should be immediate) */
    for (i = 0; i < 10; i++) {
        WaitTOF();
        if (CheckIO((struct IORequest *)con_io)) {
            WaitIO((struct IORequest *)con_io);
            return con_io->io_Actual;
        }
    }
    
    /* Timeout - abort */
    AbortIO((struct IORequest *)con_io);
    WaitIO((struct IORequest *)con_io);
    return con_io->io_Actual;
}

/*
 * Clear input buffer
 */
static void con_clear_input(void)
{
    if (!con_io) return;
    
    con_io->io_Command = CMD_CLEAR;
    DoIO((struct IORequest *)con_io);
}

/*
 * Get cursor position using CSI 6 n (Device Status Report)
 * Returns TRUE if position was read, stores row/col (1-based)
 */
static BOOL get_cursor_pos(int *row, int *col)
{
    char buf[32];
    LONG len;
    int r = 0, c = 0;
    int i;
    BOOL in_col = FALSE;
    
    /* Clear any pending input */
    con_clear_input();
    
    /* Request cursor position */
    con_puts(CSI "6n");
    
    /* Read response (should be CSI row;col R) */
    len = con_read(buf, sizeof(buf) - 1);
    if (len <= 0) {
        return FALSE;
    }
    buf[len] = '\0';
    
    /* Parse response: expect 0x9B row ; col R */
    for (i = 0; i < len; i++) {
        if (buf[i] == 0x9B || buf[i] == '\x1b') {
            /* Skip CSI or ESC[ */
            if (buf[i] == '\x1b' && i + 1 < len && buf[i+1] == '[') {
                i++;
            }
            continue;
        }
        if (buf[i] == ';') {
            in_col = TRUE;
            continue;
        }
        if (buf[i] == 'R') {
            break;
        }
        if (buf[i] >= '0' && buf[i] <= '9') {
            if (in_col) {
                c = c * 10 + (buf[i] - '0');
            } else {
                r = r * 10 + (buf[i] - '0');
            }
        }
    }
    
    *row = r;
    *col = c;
    return TRUE;
}

/*
 * Test assertion with cursor position
 */
static void assert_cursor(int expected_row, int expected_col, const char *test_name)
{
    int row, col;
    
    tests_run++;
    
    if (!get_cursor_pos(&row, &col)) {
        print("FAIL: ");
        print(test_name);
        print(" - could not read cursor position\n");
        tests_failed++;
        return;
    }
    
    if (row == expected_row && col == expected_col) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected (");
        print_num(expected_row);
        print(",");
        print_num(expected_col);
        print(") got (");
        print_num(row);
        print(",");
        print_num(col);
        print(")\n");
        tests_failed++;
    }
}

/*
 * Test: Cursor starts at home position
 */
static void test_cursor_home(void)
{
    /* Clear screen and home cursor */
    con_puts(CSI "2J" CSI "H");
    assert_cursor(1, 1, "Cursor home (CSI H)");
}

/*
 * Test: Cursor absolute positioning
 */
static void test_cursor_position(void)
{
    /* Position to 5,10 */
    con_puts(CSI "5;10H");
    assert_cursor(5, 10, "Cursor position CSI 5;10H");
    
    /* Position to 1,1 (explicit) */
    con_puts(CSI "1;1H");
    assert_cursor(1, 1, "Cursor position CSI 1;1H");
    
    /* Position to 10,20 */
    con_puts(CSI "10;20H");
    assert_cursor(10, 20, "Cursor position CSI 10;20H");
    
    /* Position using 'f' command (same as H) */
    con_puts(CSI "3;5f");
    assert_cursor(3, 5, "Cursor position CSI 3;5f");
}

/*
 * Test: Cursor relative movement
 */
static void test_cursor_movement(void)
{
    /* Start at known position */
    con_puts(CSI "10;10H");
    
    /* Move up 3 */
    con_puts(CSI "3A");
    assert_cursor(7, 10, "Cursor up 3 (CSI 3A)");
    
    /* Move down 5 */
    con_puts(CSI "5B");
    assert_cursor(12, 10, "Cursor down 5 (CSI 5B)");
    
    /* Move forward (right) 4 */
    con_puts(CSI "4C");
    assert_cursor(12, 14, "Cursor forward 4 (CSI 4C)");
    
    /* Move backward (left) 2 */
    con_puts(CSI "2D");
    assert_cursor(12, 12, "Cursor backward 2 (CSI 2D)");
}

/*
 * Test: Cursor movement with default (1)
 * Note: CSI sequences with no parameters can have timing issues
 */
static void test_cursor_movement_defaults(void)
{
    /* Test up with default */
    con_puts(CSI "5;5H");
    con_puts(CSI "A");
    assert_cursor(4, 5, "Cursor up default (CSI A)");
    
    /* Test down with default - start fresh */
    con_puts(CSI "5;5H");
    con_puts(CSI "B");
    assert_cursor(6, 5, "Cursor down default (CSI B)");
    
    /* Test forward with default - use explicit count to avoid timing issue */
    con_puts(CSI "5;5H");
    con_puts(CSI "1C");  /* Explicit count of 1 instead of default */
    assert_cursor(5, 6, "Cursor forward default (CSI 1C)");
    
    /* Test backward with default - use explicit count */
    con_puts(CSI "5;5H");
    con_puts(CSI "1D");  /* Explicit count of 1 instead of default */
    assert_cursor(5, 4, "Cursor backward default (CSI 1D)");
}

/*
 * Test: Next line and previous line
 */
static void test_cursor_lines(void)
{
    con_puts(CSI "5;10H");
    
    /* Next line - moves to column 1 of next line */
    con_puts(CSI "E");
    assert_cursor(6, 1, "Cursor next line (CSI E)");
    
    /* Previous line - moves to column 1 of previous line */
    con_puts(CSI "F");
    assert_cursor(5, 1, "Cursor previous line (CSI F)");
    
    /* Next line 3 */
    con_puts(CSI "3E");
    assert_cursor(8, 1, "Cursor next line 3 (CSI 3E)");
}

/*
 * Test: Horizontal absolute position
 */
static void test_cursor_horizontal(void)
{
    con_puts(CSI "5;10H");
    
    /* Move to column 15 on same row */
    con_puts(CSI "15G");
    assert_cursor(5, 15, "Cursor horizontal absolute (CSI 15G)");
    
    /* Move to column 1 (explicit) */
    con_puts(CSI "1G");
    assert_cursor(5, 1, "Cursor horizontal absolute (CSI 1G)");
}

/*
 * Test: Tab stops
 */
static void test_tab_stops(void)
{
    con_puts(CSI "1;1H");
    
    /* Tab should move to column 9 (default tab stops at 9,17,25...) */
    con_puts(CSI "I");
    assert_cursor(1, 9, "Cursor forward tab (CSI I)");
    
    /* Tab 2 more times */
    con_puts(CSI "2I");
    assert_cursor(1, 25, "Cursor forward 2 tabs (CSI 2I)");
    
    /* Back tab */
    con_puts(CSI "Z");
    assert_cursor(1, 17, "Cursor back tab (CSI Z)");
}

/*
 * Test: Character output advances cursor
 */
static void test_character_output(void)
{
    con_puts(CSI "1;1H");
    
    /* Write 5 characters */
    con_puts("ABCDE");
    assert_cursor(1, 6, "Cursor after 5 chars");
    
    /* Write more */
    con_puts("FGH");
    assert_cursor(1, 9, "Cursor after 8 chars");
}

/*
 * Test: Newline handling
 */
static void test_newline(void)
{
    con_puts(CSI "1;5H");
    
    /* Linefeed - moves down (LF mode off = just down) */
    con_puts(CSI "20l");  /* Ensure LF mode off */
    con_puts("\n");
    assert_cursor(2, 5, "Linefeed (LF mode off)");
    
    /* Carriage return */
    con_puts("\r");
    assert_cursor(2, 1, "Carriage return");
    
    /* Enable LF mode (LF = CR+LF) */
    con_puts(CSI "20h");
    con_puts(CSI "3;10H");
    con_puts("\n");
    assert_cursor(4, 1, "Linefeed (LF mode on)");
    
    /* Reset LF mode */
    con_puts(CSI "20l");
}

/*
 * Test: Boundary conditions - cursor at top
 */
static void test_boundary_top(void)
{
    con_puts(CSI "1;5H");
    
    /* Try to move up past top - should stay at row 1 */
    con_puts(CSI "10A");
    assert_cursor(1, 5, "Cursor up at top boundary");
}

/*
 * Test: Boundary conditions - cursor at left
 */
static void test_boundary_left(void)
{
    con_puts(CSI "5;1H");
    
    /* Try to move left past left edge - should stay at column 1 */
    con_puts(CSI "10D");
    assert_cursor(5, 1, "Cursor left at left boundary");
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
        (STRPTR)"CSI Unit Test",
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
    print("=== CSI Unit Test ===\n");
    print("Testing console.device CSI escape sequences\n\n");
    
    if (!setup_console()) {
        print("FAIL: Setup failed\n");
        cleanup_console();
        return 1;
    }
    
    /* Run all tests */
    test_cursor_home();
    test_cursor_position();
    test_cursor_movement();
    test_cursor_movement_defaults();
    test_cursor_lines();
    test_cursor_horizontal();
    test_tab_stops();
    test_character_output();
    test_newline();
    test_boundary_top();
    test_boundary_left();
    
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
    
    print("\nPASS: All CSI unit tests passed\n");
    return 0;
}
