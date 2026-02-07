/*
 * Test for Console CSI Escape Sequence Translation
 *
 * Tests Amiga CSI (0x9B) escape sequences being translated to ANSI.
 * Uses Write() to console to ensure sequences go through translation.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#include <stdio.h>
#include <string.h>

#define CSI "\x9b"   /* Amiga CSI (single byte) */

/* Write a string directly to console (triggers CSI translation) */
static void con_write(const char *s)
{
    Write(Output(), (APTR)s, strlen(s));
}

int main(int argc, char **argv)
{

    /* Test 1: Cursor positioning */
    con_write("Test 1: Cursor Home\n");
    con_write(CSI "H");
    con_write("\n");

    /* Test 2: Absolute position */
    con_write("Test 2: Position 5;10\n");
    con_write(CSI "5;10H");
    con_write("\n");

    /* Test 3: Cursor movement */
    con_write("Test 3: Up 2\n");
    con_write(CSI "2A");
    con_write("\n");

    con_write("Test 4: Down 3\n");
    con_write(CSI "3B");
    con_write("\n");

    con_write("Test 5: Forward 5\n");
    con_write(CSI "5C");
    con_write("\n");

    con_write("Test 6: Backward 4\n");
    con_write(CSI "4D");
    con_write("\n");

    /* Test 7: Erase functions */
    con_write("Test 7: Erase to end of line\n");
    con_write(CSI "K");
    con_write("\n");

    con_write("Test 8: Erase to end of display\n");
    con_write(CSI "J");
    con_write("\n");

    con_write("Test 9: Erase entire line\n");
    con_write(CSI "2K");
    con_write("\n");

    con_write("Test 10: Clear screen\n");
    con_write(CSI "2J");
    con_write("\n");

    /* Test 11: Insert/delete */
    con_write("Test 11: Insert line\n");
    con_write(CSI "L");
    con_write("\n");

    con_write("Test 12: Delete line\n");
    con_write(CSI "M");
    con_write("\n");

    con_write("Test 13: Insert 3 chars\n");
    con_write(CSI "3@");
    con_write("\n");

    con_write("Test 14: Delete 2 chars\n");
    con_write(CSI "2P");
    con_write("\n");

    /* Test 15: Scrolling */
    con_write("Test 15: Scroll up 2\n");
    con_write(CSI "2S");
    con_write("\n");

    con_write("Test 16: Scroll down 1\n");
    con_write(CSI "1T");
    con_write("\n");

    /* Test 17: SGR - Text attributes */
    con_write("Test 17: Reset attributes\n");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 18: Bold\n");
    con_write(CSI "1m");
    con_write("BOLD");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 19: Underline\n");
    con_write(CSI "4m");
    con_write("UNDERLINE");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 20: Inverse\n");
    con_write(CSI "7m");
    con_write("INVERSE");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 21: Foreground color 31 (red)\n");
    con_write(CSI "31m");
    con_write("RED");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 22: Background color 44 (blue bg)\n");
    con_write(CSI "44m");
    con_write("BLUE BG");
    con_write(CSI "0m");
    con_write("\n");

    con_write("Test 23: Combined 1;33;44 (bold yellow on blue)\n");
    con_write(CSI "1;33;44m");
    con_write("BOLD YELLOW ON BLUE");
    con_write(CSI "0m");
    con_write("\n");

    /* Test 24: Cursor visibility */
    con_write("Test 24: Hide cursor\n");
    con_write(CSI "0 p");
    con_write("\n");

    con_write("Test 25: Show cursor\n");
    con_write(CSI " p");
    con_write("\n");

    /* Test 26: Next/prev line */
    con_write("Test 26: Next line 2\n");
    con_write(CSI "2E");
    con_write("\n");

    con_write("Test 27: Prev line 1\n");
    con_write(CSI "1F");
    con_write("\n");

    /* Test 28: Form feed (clear screen) */
    con_write("Test 28: Form feed clear\n");
    con_write("\x0c");
    con_write("\n");

    /* Test 29: ESC[ alternative CSI */
    con_write("Test 29: ESC[ cursor up\n");
    con_write("\x1b[2A");
    con_write("\n");

    con_write("PASS: csi_cursor all tests completed\n");
    return 0;
}
