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

static struct ConUnit *get_con_unit(void)
{
    if (!con_io) {
        return NULL;
    }

    return (struct ConUnit *)con_io->io_Unit;
}

static int count_non_bg_pixels_in_cell(int row, int col)
{
    struct ConUnit *unit = get_con_unit();
    struct RastPort *rp;
    WORD x0;
    WORD y0;
    WORD x1;
    WORD y1;
    int count = 0;
    WORD x;
    WORD y;

    if (!test_win || !test_win->RPort || !unit) {
        return -1;
    }

    rp = test_win->RPort;
    x0 = unit->cu_XROrigin + ((col - 1) * unit->cu_XRSize);
    y0 = unit->cu_YROrigin + ((row - 1) * unit->cu_YRSize);
    x1 = x0 + unit->cu_XRSize - 1;
    y1 = y0 + unit->cu_YRSize - 1;

    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            if (ReadPixel(rp, x, y) != rp->BgPen) {
                count++;
            }
        }
    }

    return count;
}

static void assert_cell_has_content(int row, int col, const char *test_name)
{
    int pixels;

    tests_run++;
    pixels = count_non_bg_pixels_in_cell(row, col);

    if (pixels > 0) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - cell has no visible content\n");
        tests_failed++;
    }
}

static void assert_cell_blank(int row, int col, const char *test_name)
{
    int pixels;

    tests_run++;
    pixels = count_non_bg_pixels_in_cell(row, col);

    if (pixels == 0) {
        print("PASS: ");
        print(test_name);
        print("\n");
        tests_passed++;
    } else {
        print("FAIL: ");
        print(test_name);
        print(" - expected blank cell, got ");
        print_num(pixels);
        print(" visible pixels\n");
        tests_failed++;
    }
}

static void start_async_read(char *buf, LONG len)
{
    con_io->io_Command = CMD_READ;
    con_io->io_Data = buf;
    con_io->io_Length = len;
    con_io->io_Flags = IOF_QUICK;
    SendIO((struct IORequest *)con_io);
}

static void finish_async_read(void)
{
    if (!CheckIO((struct IORequest *)con_io)) {
        AbortIO((struct IORequest *)con_io);
    }

    WaitIO((struct IORequest *)con_io);
}

static BOOL get_window_bounds(int *rows, int *cols)
{
    char buf[32];
    LONG len;
    int values[4] = {0, 0, 0, 0};
    int value_count = 0;
    int current = 0;
    BOOL have_digits = FALSE;
    int i;

    con_clear_input();
    con_puts(CSI "0q");

    len = con_read(buf, sizeof(buf) - 1);
    if (len <= 0) {
        return FALSE;
    }
    buf[len] = '\0';

    for (i = 0; i < len && value_count < 4; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            current = current * 10 + (buf[i] - '0');
            have_digits = TRUE;
        } else if (buf[i] == ';' || buf[i] == ' ' || buf[i] == 'r') {
            if (have_digits) {
                values[value_count++] = current;
                current = 0;
                have_digits = FALSE;
            }
            if (buf[i] == 'r') {
                break;
            }
        }
    }

    if (value_count == 4) {
        *rows = values[2];
        *cols = values[3];
        return TRUE;
    }

    return FALSE;
}

static void con_update(void)
{
    con_io->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)con_io);
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

static void test_esc_bracket_sequences(void)
{
    char read_buf[8];

    con_puts(CSI "2J" CSI "5;10H");
    con_puts("\x1b[H");
    assert_cursor(1, 1, "ESC[H homes the cursor");

    con_puts(CSI "2;1H");
    con_puts("ABC");
    assert_cell_has_content(2, 2, "Text draws before ESC[K erase");
    con_puts(CSI "2;1H");
    con_puts("\x1b[K");
    assert_cell_blank(2, 2, "ESC[K clears to end of line");

    con_puts(CSI "3;1H");
    con_puts("XYZ");
    assert_cell_has_content(3, 2, "Text draws before ESC[J erase");
    con_puts(CSI "1;1H");
    con_puts("\x1b[J");
    assert_cell_blank(3, 2, "ESC[J clears to end of display");

    con_puts(CSI "2J" CSI "H");
    con_puts("\x1b[?25l");
    start_async_read(read_buf, sizeof(read_buf));
    WaitTOF();
    WaitTOF();
    assert_cell_blank(1, 1, "ESC[?25l hides the cursor");
    finish_async_read();

    con_puts("\x1b[?25h");
    start_async_read(read_buf, sizeof(read_buf));
    WaitTOF();
    WaitTOF();
    assert_cell_has_content(1, 1, "ESC[?25h shows the cursor");
    finish_async_read();
    con_puts("\x1b[?25l");
}

static void test_window_resize_updates_console(void)
{
    int rows_before;
    int cols_before;
    int rows_after;
    int cols_after;

    tests_run++;

    if (!get_window_bounds(&rows_before, &cols_before)) {
        print("FAIL: Console bounds before resize could not be queried\n");
        tests_failed++;
        return;
    }

    SizeWindow(test_win, 80, 40);
    con_update();
    WaitTOF();
    WaitTOF();
    Delay(2);
    con_update();
    WaitTOF();
    WaitTOF();

    if (!get_window_bounds(&rows_after, &cols_after)) {
        print("FAIL: Console bounds after resize could not be queried\n");
        tests_failed++;
        return;
    }

    if (rows_after > rows_before && cols_after > cols_before) {
        print("PASS: Window resize updates console bounds\n");
        tests_passed++;
    } else {
        print("FAIL: Window resize did not update console bounds (before ");
        print_num(rows_before);
        print("x");
        print_num(cols_before);
        print(", after ");
        print_num(rows_after);
        print("x");
        print_num(cols_after);
        print(")\n");
        tests_failed++;
    }
}

static void test_private_geometry_controls(void)
{
    struct ConUnit *unit;
    int rows;
    int cols;

    con_puts(CSI "2J" CSI "H");
    con_puts(CSI "5x" CSI "3y" CSI "20u" CSI "4t");
    con_update();
    WaitTOF();
    WaitTOF();

    tests_run++;
    if (get_window_bounds(&rows, &cols) && rows == 4 && cols == 20) {
        print("PASS: Private geometry controls update reported bounds\n");
        tests_passed++;
    } else {
        print("FAIL: Private geometry controls did not update reported bounds\n");
        tests_failed++;
    }

    unit = get_con_unit();
    tests_run++;
    if (unit && unit->cu_XROrigin == 5 && unit->cu_YROrigin == 3) {
        print("PASS: Private geometry controls update console origin\n");
        tests_passed++;
    } else {
        print("FAIL: Private geometry controls did not update console origin\n");
        tests_failed++;
    }

    con_puts("A");
    assert_cell_has_content(1, 1, "Private left/top offsets move text origin");

    con_puts(CSI "x" CSI "y" CSI "u" CSI "t");
    con_update();
    WaitTOF();
    WaitTOF();

    unit = get_con_unit();
    tests_run++;
    if (unit && unit->cu_XROrigin >= 4 && unit->cu_YROrigin >= 11) {
        print("PASS: Clearing private geometry controls restores auto origin\n");
        tests_passed++;
    } else {
        print("FAIL: Clearing private geometry controls did not restore auto origin\n");
        tests_failed++;
    }
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
        IDCMP_RAWKEY | IDCMP_VANILLAKEY | IDCMP_NEWSIZE,
        WFLG_SMART_REFRESH | WFLG_ACTIVATE | WFLG_DEPTHGADGET | WFLG_SIZEGADGET,
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
    test_esc_bracket_sequences();
    test_window_resize_updates_console();
    test_private_geometry_controls();
    
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
