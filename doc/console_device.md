# Console Device Documentation

This document describes the console.device implementation in lxa, its supported features, and known differences from the original AmigaOS console device.

## Overview

The console device provides a text-based I/O interface for Amiga applications. It translates keyboard input into an ANSI byte stream and interprets ANSI/CSI escape sequences for output to a window.

In lxa, the console device:
- Attaches to an Intuition window
- Provides line-buffered (cooked) and character-at-a-time (raw) input modes
- Supports ANSI/CSI escape sequences for cursor control, text attributes, and colors
- Handles keyboard input via IDCMP events

## Opening the Console Device

```c
struct MsgPort *con_port;
struct IOStdReq *con_io;
struct Window *window;

/* Create message port for replies */
con_port = CreateMsgPort();

/* Create I/O request */
con_io = (struct IOStdReq *)CreateIORequest(con_port, sizeof(struct IOStdReq));

/* Open a window first */
window = OpenWindow(&newwindow);

/* Attach console to window */
con_io->io_Data = (APTR)window;
con_io->io_Length = sizeof(struct Window);

/* Open console device */
if (OpenDevice("console.device", CONU_STANDARD, (struct IORequest *)con_io, 0) == 0) {
    /* Success - console is now attached to window */
}
```

### Console Unit Types

| Unit | Value | Description |
|------|-------|-------------|
| CONU_LIBRARY | -1 | Library mode only (for RawKeyConvert). No window required. |
| CONU_STANDARD | 0 | Standard console with SMART_REFRESH window. |
| CONU_CHARMAP | 1 | Character-mapped console with SIMPLE_REFRESH (V36+). |
| CONU_SNIPMAP | 3 | Character-mapped with copy/paste support (V36+). |

**lxa Status**: Only CONU_STANDARD (0) is fully implemented.

## Device Commands

### CMD_WRITE

Writes text to the console, interpreting escape sequences.

```c
con_io->io_Command = CMD_WRITE;
con_io->io_Data = (APTR)"Hello, World!\n";
con_io->io_Length = -1;  /* -1 for null-terminated strings */
DoIO((struct IORequest *)con_io);
```

### CMD_READ

Reads input from the console. Returns when:
- In line mode: A complete line (terminated by Return) is available
- In raw mode: Any character is available

```c
char buffer[256];
con_io->io_Command = CMD_READ;
con_io->io_Data = buffer;
con_io->io_Length = sizeof(buffer);
DoIO((struct IORequest *)con_io);
/* con_io->io_Actual contains number of bytes read */
```

**Note**: A read request may return fewer bytes than requested. Check `io_Actual` for the actual count.

### CMD_CLEAR

Clears the console's input buffer (not the display).

```c
con_io->io_Command = CMD_CLEAR;
DoIO((struct IORequest *)con_io);
```

### CD_ASKKEYMAP / CD_SETKEYMAP

Get/set the keymap for this console unit.

**lxa Status**: Stub implementation (returns success but doesn't change keymap).

### CD_ASKDEFAULTKEYMAP / CD_SETDEFAULTKEYMAP

Get/set the system default keymap.

**lxa Status**: Stub implementation.

## Control Characters

| Character | Hex | Action |
|-----------|-----|--------|
| BELL | 0x07 | DisplayBeep (visual flash) |
| BACKSPACE | 0x08 | Move cursor left one column |
| TAB | 0x09 | Move to next tab stop |
| LINEFEED | 0x0A | Move down (with LNM: CR+LF) |
| VERTICAL TAB | 0x0B | Move up one line |
| FORMFEED | 0x0C | Clear window |
| CARRIAGE RETURN | 0x0D | Move to column 1 |
| SHIFT OUT | 0x0E | Set MSB on output characters |
| SHIFT IN | 0x0F | Undo SHIFT OUT |
| ESC | 0x1B | Start escape sequence |
| INDEX | 0x84 | Move down one line |
| NEXT LINE | 0x85 | CR + LF |
| HTS | 0x88 | Set tab at cursor position |
| REVERSE INDEX | 0x8D | Move up one line |
| CSI | 0x9B | Control Sequence Introducer |

## CSI Escape Sequences

The Control Sequence Introducer (CSI) can be either:
- Single byte: 0x9B
- Two bytes: ESC [ (0x1B 0x5B)

### Cursor Movement

| Sequence | Description |
|----------|-------------|
| CSI H | Cursor home (1,1) |
| CSI n;m H | Cursor position (row n, column m) |
| CSI n;m f | Same as CSI H |
| CSI n A | Cursor up n rows (default 1) |
| CSI n B | Cursor down n rows (default 1) |
| CSI n C | Cursor forward n columns (default 1) |
| CSI n D | Cursor backward n columns (default 1) |
| CSI n E | Cursor to start of nth next line |
| CSI n F | Cursor to start of nth previous line |
| CSI n G | Cursor to column n |
| CSI n I | Forward n tab stops |
| CSI n Z | Backward n tab stops |

### Erase Functions

| Sequence | Description |
|----------|-------------|
| CSI J | Erase from cursor to end of display |
| CSI 1 J | Erase from start of display to cursor |
| CSI 2 J | Erase entire display |
| CSI K | Erase from cursor to end of line |
| CSI 1 K | Erase from start of line to cursor |
| CSI 2 K | Erase entire line |

### Line/Character Operations

| Sequence | Description |
|----------|-------------|
| CSI n @ | Insert n characters (shift line right) |
| CSI n P | Delete n characters |
| CSI L | Insert line |
| CSI M | Delete line |
| CSI n S | Scroll up n lines |
| CSI n T | Scroll down n lines |

### Tab Control

| Sequence | Description |
|----------|-------------|
| CSI 0 W | Set tab at current position |
| CSI 2 W | Clear tab at current position |
| CSI 5 W | Clear all tabs |

Default tab stops are at columns 9, 17, 25, 33, 41, 49, 57, 65, 73.

### Mode Control

| Sequence | Description |
|----------|-------------|
| CSI 20 h | Set LF mode (LF acts as CR+LF) |
| CSI 20 l | Reset LF mode (LF only moves down) |
| CSI >1 h | Enable auto-scroll mode |
| CSI >1 l | Disable auto-scroll mode |
| CSI ?7 h | Enable auto-wrap mode |
| CSI ?7 l | Disable auto-wrap mode |

### SGR (Select Graphic Rendition)

Format: CSI n1;n2;...;nk m

| Parameter | Description |
|-----------|-------------|
| 0 | Reset all attributes to default |
| 1 | Bold (brightens color) |
| 2 | Faint/dim |
| 3 | Italic (tracked, displayed as normal) |
| 4 | Underline (tracked, displayed as normal) |
| 7 | Inverse video (swap fg/bg) |
| 8 | Concealed (tracked, displayed as normal) |
| 22 | Not bold/faint |
| 23 | Not italic |
| 24 | Not underlined |
| 27 | Not inverse |
| 28 | Not concealed |
| 30-37 | Foreground colors 0-7 |
| 39 | Default foreground |
| 40-47 | Background colors 0-7 |
| 49 | Default background |

**Amiga Color Mapping**:
- 30/40: Black (pen 0)
- 31/41: Red (pen 1)
- 32/42: Green (pen 2)
- 33/43: Yellow (pen 3)
- 34/44: Blue (pen 4)
- 35/45: Magenta (pen 5)
- 36/46: Cyan (pen 6)
- 37/47: White (pen 7)

### Cursor Visibility

| Sequence | Description |
|----------|-------------|
| CSI 0 p | Hide cursor |
| CSI p | Show cursor |
| CSI 1 p | Show cursor |

### Device Status Report

| Sequence | Description |
|----------|-------------|
| CSI 6 n | Request cursor position report |

Response is placed in input buffer: CSI row;col R

### Window Status

| Sequence | Description |
|----------|-------------|
| CSI 0 q | Request window bounds |

Response: CSI 1;1;rows;cols r

### Amiga-Specific Sequences

| Sequence | Description |
|----------|-------------|
| CSI n t | Set page length (ignored - uses window size) |
| CSI n u | Set line length (ignored - uses window size) |
| CSI n x | Set left offset (ignored) |
| CSI n y | Set top offset (ignored) |

## Special Key Sequences

When function keys, arrow keys, or special keys are pressed, the console device generates CSI sequences:

### Function Keys

| Key | Unshifted | Shifted |
|-----|-----------|---------|
| F1 | CSI 0~ | CSI 10~ |
| F2 | CSI 1~ | CSI 11~ |
| F3 | CSI 2~ | CSI 12~ |
| F4 | CSI 3~ | CSI 13~ |
| F5 | CSI 4~ | CSI 14~ |
| F6 | CSI 5~ | CSI 15~ |
| F7 | CSI 6~ | CSI 16~ |
| F8 | CSI 7~ | CSI 17~ |
| F9 | CSI 8~ | CSI 18~ |
| F10 | CSI 9~ | CSI 19~ |
| F11 | CSI 20~ | CSI 30~ |
| F12 | CSI 21~ | CSI 31~ |

### Arrow Keys

| Key | Unshifted | Shifted |
|-----|-----------|---------|
| Up | CSI A | CSI T |
| Down | CSI B | CSI S |
| Right | CSI C | CSI  A (space+A) |
| Left | CSI D | CSI  @ (space+@) |

### Other Special Keys

| Key | Unshifted | Shifted |
|-----|-----------|---------|
| Help | CSI ?~ | CSI ?~ |
| Insert | CSI 40~ | CSI 50~ |
| Page Up | CSI 41~ | CSI 51~ |
| Page Down | CSI 42~ | CSI 52~ |
| Home | CSI 44~ | CSI 54~ |
| End | CSI 45~ | CSI 55~ |
| Delete | 0x7F | 0x7F |

## CON: Handler

lxa also implements the CON: and RAW: DOS handlers for opening console windows via DOS:

```c
/* Open a CON: window */
BPTR fh = Open("CON:0/0/640/200/My Window/CLOSE", MODE_OLDFILE);
if (fh) {
    Write(fh, "Hello from CON:\n", 16);
    Close(fh);
}
```

### CON: Path Syntax

```
CON:[x]/[y]/[width]/[height]/[title]/[options]
RAW:[x]/[y]/[width]/[height]/[title]/[options]
```

- **x, y**: Window position (optional, use defaults if omitted)
- **width, height**: Window size (optional)
- **title**: Window title (optional)
- **options**: CLOSE, AUTO, WAIT, SCREEN, etc. (most ignored in lxa)

The difference between CON: and RAW: is the input mode:
- **CON:** Line-buffered input (cooked mode)
- **RAW:** Character-at-a-time input (raw mode)

## Known Limitations vs. Real AmigaOS

1. **CONU_CHARMAP/CONU_SNIPMAP**: Not implemented (only CONU_STANDARD works)

2. **Keymap Functions**: RawKeyConvert() and keymap switching are stub implementations

3. **Raw Events**: CSI { and CSI } sequences for setting raw input events are not implemented

4. **Text Styles**: Italic, underline, and concealed are tracked but displayed as normal text

5. **Global Background Color**: The >n SGR parameter for global background color is not implemented

6. **Copy/Paste**: No clipboard integration (CONU_SNIPMAP not supported)

7. **Window Resize**: Console does not automatically redraw on window resize

## Usage Examples

### Basic Console Write

```c
#include <devices/console.h>

void console_puts(struct IOStdReq *con, const char *str)
{
    con->io_Command = CMD_WRITE;
    con->io_Data = (APTR)str;
    con->io_Length = -1;
    DoIO((struct IORequest *)con);
}

/* Clear screen and home cursor */
console_puts(con, "\x9b" "2J" "\x9b" "H");

/* Print in red */
console_puts(con, "\x9b" "31mRed text\x9b" "0m\n");

/* Bold text */
console_puts(con, "\x9b" "1mBold\x9b" "0m\n");
```

### Reading with Position Query

```c
void get_cursor_pos(struct IOStdReq *con, int *row, int *col)
{
    char buf[32];
    
    /* Clear input buffer */
    con->io_Command = CMD_CLEAR;
    DoIO((struct IORequest *)con);
    
    /* Request cursor position */
    con->io_Command = CMD_WRITE;
    con->io_Data = "\x9b" "6n";
    con->io_Length = -1;
    DoIO((struct IORequest *)con);
    
    /* Read response */
    con->io_Command = CMD_READ;
    con->io_Data = buf;
    con->io_Length = sizeof(buf);
    DoIO((struct IORequest *)con);
    
    /* Parse response: CSI row;col R */
    /* ... parsing code ... */
}
```

## Testing

Console device functionality is tested by the following test programs in `tests/console/`:

- **csi_unit**: Comprehensive CSI sequence verification using Device Status Report
- **csi_cursor**: Visual output test for cursor and SGR sequences
- **input_inject**: Tests keyboard input injection
- **input_console**: Tests console input handling
- **kp2_test**: Tests line-mode read behavior (1-byte-at-a-time pattern)
- **con_handler**: Tests CON:/RAW: DOS handler

Run all console tests:
```bash
make -C tests/console run-tests
```

## References

- RKRM: Amiga ROM Kernel Reference Manual - Devices
- AROS console.device source code
- ANSI X3.64 / ECMA-48 terminal control standards
