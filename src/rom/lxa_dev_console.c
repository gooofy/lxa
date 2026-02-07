#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/console.h>
#include <devices/conunit.h>

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <clib/graphics_protos.h>
#include <inline/graphics.h>

#include "util.h"

/* strlen is declared in util.h */

/* Forward declaration of console unit structure */
struct LxaConUnit;

/*
 * Console Device Base structure
 * Extended from Library to hold device-specific state for async I/O (Phase 45)
 */
#define MAX_CONSOLE_UNITS 8  /* Maximum number of console units we can track */

struct ConsoleDevBase {
    struct Library   cdb_Library;           /* Standard library structure */
    struct LxaConUnit *cdb_Units[MAX_CONSOLE_UNITS];  /* Active console units */
    UWORD            cdb_NumUnits;          /* Number of active units */
};

/* Console device commands (from devices/console.h) */
#ifndef CD_ASKKEYMAP
#define CD_ASKKEYMAP        (CMD_NONSTD + 0)
#define CD_SETKEYMAP        (CMD_NONSTD + 1)
#define CD_ASKDEFAULTKEYMAP (CMD_NONSTD + 2)
#define CD_SETDEFAULTKEYMAP (CMD_NONSTD + 3)
#endif

#ifndef CMD_CLEAR
#define CMD_CLEAR           CMD_RESET   /* Clear console (same as CMD_RESET for console.device) */
#endif

#define VERSION    40
#define REVISION   4
#define EXDEVNAME  "console"
#define EXDEVVER   " 40.4 (2025/01/17)"

char __aligned _g_console_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_console_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_console_Copyright [] = "(C)opyright 2022-2025 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_console_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase      *SysBase;
extern struct GfxBase       *GfxBase;

/* CSI parsing state */
#define CSI_BUF_LEN 64

/* Input buffer size */
#define INPUT_BUF_LEN 256

/* Our extended ConUnit with additional state */
struct LxaConUnit {
    struct ConUnit cu;           /* Standard ConUnit at the beginning */
    /* CSI parsing state */
    BOOL   csi_active;
    BOOL   esc_active;
    char   csi_buf[CSI_BUF_LEN];
    UWORD  csi_len;
    /* Input buffer */
    char   input_buf[INPUT_BUF_LEN];
    UWORD  input_head;           /* Write position */
    UWORD  input_tail;           /* Read position */
    BOOL   echo_enabled;         /* Echo typed characters */
    BOOL   line_mode;            /* Line-buffered (cooked) vs raw mode */
    /* Cursor state */
    BOOL   cursor_visible;       /* Whether cursor should be displayed */
    BOOL   cursor_drawn;         /* Whether cursor is currently rendered */
    /* Line editing state */
    UWORD  line_start_x;         /* X position where current input line started */
    UWORD  line_start_y;         /* Y position where current input line started */
    /* Mode flags (LNM, ASM, AWM) */
    BOOL   lf_mode;              /* LF mode: TRUE = LF acts as CR+LF */
    BOOL   autowrap_mode;        /* Auto-wrap mode: TRUE = wrap at right edge */
    BOOL   autoscroll_mode;      /* Auto-scroll mode: TRUE = scroll when at bottom */
    BOOL   shift_out;            /* SHIFT OUT active: set MSB on output characters */
    /* SGR (text attributes) state */
    BOOL   sgr_bold;             /* Bold/bright text */
    BOOL   sgr_faint;            /* Faint/dim text */
    BOOL   sgr_italic;           /* Italic text (usually shown as inverse on Amiga) */
    BOOL   sgr_underline;        /* Underline text */
    BOOL   sgr_inverse;          /* Inverse video */
    BOOL   sgr_concealed;        /* Concealed/hidden text */
    /* Saved colors for inverse mode toggle */
    BYTE   saved_fg;             /* Original foreground pen */
    BYTE   saved_bg;             /* Original background pen */
    /* Tab stops - 80 maximum (RKRM standard) */
    UWORD  tab_stops[80];
    /* Scrolling region (for DECSTBM and Amiga '{' sequence) */
    WORD   scroll_top;           /* Top line of scrolling region (0-based) */
    WORD   scroll_bottom;        /* Bottom line of scrolling region (0-based) */
    /* Unit mode */
    UWORD  unit_mode;            /* CONU_STANDARD, CONU_CHARMAP, or CONU_SNIPMAP */
    BOOL   use_keymap;           /* TRUE if using custom keymap (CONU_CHARMAP/SNIPMAP) */
    /* Pending async read request (Phase 45) */
    struct IOStdReq *pending_read;  /* Currently pending async read, or NULL */
};

/*
 * Rawkey to ASCII conversion table (unshifted)
 * Index by rawkey code (0x00-0x67), value is ASCII character
 * 0 = no printable character
 */
static const char rawkey_to_ascii_unshifted[128] = {
    /* 0x00-0x0F: top row (`, 1-9, 0, -, =, \, unassigned, KP0) */
    '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\', 0, '0',
    /* 0x10-0x1F: qwerty row (q-p, [, ], unassigned, KP1-3) */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, '1', '2', '3',
    /* 0x20-0x2F: asdf row (a-;, ', unassigned, unassigned, KP4-6) */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', 0, 0, '4', '5', '6',
    /* 0x30-0x3F: zxcv row (unassigned, z-/, unassigned, KP., KP7-9) */
    0, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '.', '7', '8', '9',
    /* 0x40-0x4F: space, backspace, tab, KP enter, return, esc, del, unassigned, up, down, right, left */
    ' ', '\b', '\t', '\r', '\r', 0x1B, 0x7F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x50-0x5F: F1-F10, unassigned... */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x60-0x6F: modifiers and more */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x70-0x7F: unused */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Rawkey to ASCII conversion table (shifted)
 */
static const char rawkey_to_ascii_shifted[128] = {
    /* 0x00-0x0F: top row (~, !-), _, +, |, unassigned, KP0) */
    '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '|', 0, '0',
    /* 0x10-0x1F: qwerty row (Q-P, {, }, unassigned, KP1-3) */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, '1', '2', '3',
    /* 0x20-0x2F: asdf row (A-:, ", unassigned, unassigned, KP4-6) */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', 0, 0, '4', '5', '6',
    /* 0x30-0x3F: zxcv row (unassigned, Z-?, unassigned, KP., KP7-9) */
    0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '.', '7', '8', '9',
    /* 0x40-0x4F: space, backspace, tab, KP enter, return, esc, del */
    ' ', '\b', '\t', '\r', '\r', 0x1B, 0x7F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x50-0x7F: rest unchanged */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Convert Amiga rawkey + qualifier to ASCII character
 * Returns 0 if no printable character
 */
static char rawkey_to_ascii(UWORD rawkey, UWORD qualifier)
{
    char c;
    BOOL shifted;
    
    /* Ignore key-up events (high bit set) */
    if (rawkey & 0x80) return 0;
    
    /* Bounds check */
    if (rawkey >= 128) return 0;
    
    /* Check shift state */
    shifted = (qualifier & (0x0001 | 0x0002)) != 0;  /* IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT */
    
    /* Check caps lock for letters */
    if (qualifier & 0x0004) {  /* IEQUALIFIER_CAPSLOCK */
        /* Caps lock only affects letters (a-z = 0x20-0x39) */
        if ((rawkey >= 0x10 && rawkey <= 0x19) ||  /* qwerty row */
            (rawkey >= 0x20 && rawkey <= 0x28) ||  /* asdf row */
            (rawkey >= 0x31 && rawkey <= 0x3A))    /* zxcv row */
        {
            shifted = !shifted;
        }
    }
    
    c = shifted ? rawkey_to_ascii_shifted[rawkey] : rawkey_to_ascii_unshifted[rawkey];
    
    /* Handle Control key - convert to control character */
    if (c && (qualifier & 0x0008)) {  /* IEQUALIFIER_CONTROL */
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 1;  /* Ctrl+A = 0x01, etc. */
        } else if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 1;
        } else if (c == '[') {
            c = 0x1B;  /* ESC */
        } else if (c == '\\') {
            c = 0x1C;
        } else if (c == ']') {
            c = 0x1D;
        } else if (c == '^' || c == '6') {
            c = 0x1E;
        } else if (c == '_' || c == '-') {
            c = 0x1F;
        }
    }
    
    return c;
}

/*
 * Forward declarations
 */
static void console_write_char(struct LxaConUnit *unit, char c);
static void console_process_csi(struct LxaConUnit *unit, char final);
static void console_scroll_up(struct LxaConUnit *unit);
static void console_scroll_down(struct LxaConUnit *unit);
static void console_newline(struct LxaConUnit *unit);
static void console_carriage_return(struct LxaConUnit *unit);
static void console_process_char(struct LxaConUnit *unit, char c);
static void console_draw_cursor(struct LxaConUnit *unit);
static void console_hide_cursor(struct LxaConUnit *unit);
static void console_clear_to_bol(struct LxaConUnit *unit);
static void console_clear_line(struct LxaConUnit *unit);
static void console_clear_to_bos(struct LxaConUnit *unit);
static void console_insert_line(struct LxaConUnit *unit);
static void console_delete_line(struct LxaConUnit *unit);
static void console_insert_char(struct LxaConUnit *unit, int n);
static void console_delete_char(struct LxaConUnit *unit, int n);
static void console_cursor_up(struct LxaConUnit *unit);
static void console_cursor_down(struct LxaConUnit *unit);
static void console_init_tab_stops(struct LxaConUnit *unit);
static void console_try_complete_pending_read(struct LxaConUnit *unit);

/*
 * Initialize a ConUnit for a window
 */
static struct LxaConUnit *console_create_unit(struct Window *window)
{
    struct LxaConUnit *unit;
    struct RastPort *rp;
    struct TextFont *font;
    
    if (!window) {
        LPRINTF(LOG_ERROR, "_console: console_create_unit() window is NULL\n");
        return NULL;
    }
    
    unit = (struct LxaConUnit *)AllocMem(sizeof(struct LxaConUnit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit) {
        LPRINTF(LOG_ERROR, "_console: console_create_unit() failed to allocate memory\n");
        return NULL;
    }
    
    /* Initialize the message port */
    unit->cu.cu_MP.mp_Flags = PA_SIGNAL;
    unit->cu.cu_MP.mp_SigBit = SIGB_SINGLE;
    unit->cu.cu_MP.mp_SigTask = FindTask(NULL);
    /* Initialize the message list (NewList inline) */
    unit->cu.cu_MP.mp_MsgList.lh_Head = (struct Node *)&unit->cu.cu_MP.mp_MsgList.lh_Tail;
    unit->cu.cu_MP.mp_MsgList.lh_Tail = NULL;
    unit->cu.cu_MP.mp_MsgList.lh_TailPred = (struct Node *)&unit->cu.cu_MP.mp_MsgList.lh_Head;
    
    /* Store the window pointer */
    unit->cu.cu_Window = window;
    
    /* Get the window's RastPort for font info */
    rp = window->RPort;
    if (rp) {
        font = rp->Font;
        if (font) {
            unit->cu.cu_XRSize = font->tf_XSize;
            unit->cu.cu_YRSize = font->tf_YSize;
        } else {
            /* Default to 8x8 font */
            unit->cu.cu_XRSize = 8;
            unit->cu.cu_YRSize = 8;
        }
    } else {
        unit->cu.cu_XRSize = 8;
        unit->cu.cu_YRSize = 8;
    }
    
    /* Calculate raster area - use inner window area (excluding borders) */
    /* Window border offsets: left=4, top=11 (title bar), right=4, bottom=2 typically */
    unit->cu.cu_XROrigin = window->BorderLeft;
    unit->cu.cu_YROrigin = window->BorderTop;
    unit->cu.cu_XRExtant = window->Width - window->BorderRight - 1;
    unit->cu.cu_YRExtant = window->Height - window->BorderBottom - 1;
    
    /* Calculate max character positions */
    WORD width = unit->cu.cu_XRExtant - unit->cu.cu_XROrigin + 1;
    WORD height = unit->cu.cu_YRExtant - unit->cu.cu_YROrigin + 1;
    unit->cu.cu_XMax = (width / unit->cu.cu_XRSize) - 1;
    unit->cu.cu_YMax = (height / unit->cu.cu_YRSize) - 1;
    
    /* Initialize cursor position to top-left */
    unit->cu.cu_XCP = 0;
    unit->cu.cu_YCP = 0;
    unit->cu.cu_XCCP = 0;
    unit->cu.cu_YCCP = 0;
    
    /* Initialize colors */
    unit->cu.cu_FgPen = 1;  /* Default foreground (usually white/black) */
    unit->cu.cu_BgPen = 0;  /* Default background */
    unit->cu.cu_DrawMode = JAM2;  /* Draw both foreground and background */
    
    /* Initialize CSI parsing state */
    unit->csi_active = FALSE;
    unit->esc_active = FALSE;
    unit->csi_len = 0;
    
    /* Initialize input buffer */
    unit->input_head = 0;
    unit->input_tail = 0;
    unit->echo_enabled = TRUE;   /* Echo by default */
    unit->line_mode = TRUE;      /* Line-buffered by default */
    
    /* Initialize cursor state */
    unit->cursor_visible = TRUE;  /* Cursor visible by default */
    unit->cursor_drawn = FALSE;
    
    /* Initialize line editing state */
    unit->line_start_x = 0;
    unit->line_start_y = 0;
    
    /* Initialize mode flags */
    unit->lf_mode = FALSE;        /* LF mode off by default */
    unit->autowrap_mode = TRUE;   /* Auto-wrap on by default */
    unit->autoscroll_mode = TRUE; /* Auto-scroll on by default */
    unit->shift_out = FALSE;      /* SHIFT OUT off by default */
    
    /* Initialize SGR state */
    unit->sgr_bold = FALSE;
    unit->sgr_faint = FALSE;
    unit->sgr_italic = FALSE;
    unit->sgr_underline = FALSE;
    unit->sgr_inverse = FALSE;
    unit->sgr_concealed = FALSE;
    unit->saved_fg = 1;
    unit->saved_bg = 0;
    
    /* Initialize tab stops (every 8 columns) */
    console_init_tab_stops(unit);
    
    /* Initialize scrolling region (full screen by default) */
    unit->scroll_top = 0;
    unit->scroll_bottom = unit->cu.cu_YMax;
    
    /* Initialize unit mode (will be set in OpenDevice) */
    unit->unit_mode = CONU_STANDARD;
    unit->use_keymap = FALSE;
    
    /* Initialize pending read (Phase 45: async I/O) */
    unit->pending_read = NULL;
    
    LPRINTF(LOG_INFO, "_console: Created ConUnit for window 0x%08lx: XMax=%d YMax=%d XRSize=%d YRSize=%d\n",
            (ULONG)window, unit->cu.cu_XMax, unit->cu.cu_YMax, unit->cu.cu_XRSize, unit->cu.cu_YRSize);
    LPRINTF(LOG_INFO, "_console:   Raster area: origin=(%d,%d) extant=(%d,%d)\n",
            unit->cu.cu_XROrigin, unit->cu.cu_YROrigin, unit->cu.cu_XRExtant, unit->cu.cu_YRExtant);
    
    return unit;
}

/*
 * Destroy a ConUnit
 */
static void console_destroy_unit(struct LxaConUnit *unit)
{
    if (unit) {
        FreeMem(unit, sizeof(struct LxaConUnit));
    }
}

/*
 * Input buffer management
 */
static BOOL input_buf_empty(struct LxaConUnit *unit)
{
    return unit->input_head == unit->input_tail;
}

static BOOL input_buf_full(struct LxaConUnit *unit)
{
    return ((unit->input_head + 1) % INPUT_BUF_LEN) == unit->input_tail;
}

static void input_buf_put(struct LxaConUnit *unit, char c)
{
    if (!input_buf_full(unit)) {
        unit->input_buf[unit->input_head] = c;
        unit->input_head = (unit->input_head + 1) % INPUT_BUF_LEN;
    }
}

static char input_buf_get(struct LxaConUnit *unit)
{
    char c;
    if (input_buf_empty(unit)) return 0;
    c = unit->input_buf[unit->input_tail];
    unit->input_tail = (unit->input_tail + 1) % INPUT_BUF_LEN;
    return c;
}

static UWORD input_buf_count(struct LxaConUnit *unit) __attribute__((unused));
static UWORD input_buf_count(struct LxaConUnit *unit)
{
    if (unit->input_head >= unit->input_tail) {
        return unit->input_head - unit->input_tail;
    } else {
        return INPUT_BUF_LEN - unit->input_tail + unit->input_head;
    }
}

/*
 * Check if there's a complete line in the buffer (for line mode)
 */
static BOOL input_has_line(struct LxaConUnit *unit)
{
    UWORD i = unit->input_tail;
    while (i != unit->input_head) {
        if (unit->input_buf[i] == '\r' || unit->input_buf[i] == '\n') {
            return TRUE;
        }
        i = (i + 1) % INPUT_BUF_LEN;
    }
    return FALSE;
}

/*
 * Put a string into the input buffer
 * Used for CSI sequences from special keys
 */
static void input_buf_put_string(struct LxaConUnit *unit, const char *str)
{
    while (*str) {
        input_buf_put(unit, *str++);
    }
}

/* Amiga rawkey codes for special keys */
#define RAWKEY_UP       0x4C
#define RAWKEY_DOWN     0x4D
#define RAWKEY_RIGHT    0x4E
#define RAWKEY_LEFT     0x4F
#define RAWKEY_F1       0x50
#define RAWKEY_F2       0x51
#define RAWKEY_F3       0x52
#define RAWKEY_F4       0x53
#define RAWKEY_F5       0x54
#define RAWKEY_F6       0x55
#define RAWKEY_F7       0x56
#define RAWKEY_F8       0x57
#define RAWKEY_F9       0x58
#define RAWKEY_F10      0x59
#define RAWKEY_HELP     0x5F
#define RAWKEY_DEL      0x46

/* 101-key keyboard extended keys - use NDK definitions if available */
#ifndef RAWKEY_F11
#define RAWKEY_F11      0x4B  /* Not on classic keyboards */
#endif
#ifndef RAWKEY_F12
#define RAWKEY_F12      0x6F  /* Not on classic keyboards */
#endif
#ifndef RAWKEY_INSERT
#define RAWKEY_INSERT   0x47  /* Insert key */
#endif
#ifndef RAWKEY_PAGEUP
#define RAWKEY_PAGEUP   0x48  /* Page Up */
#endif
#ifndef RAWKEY_PAGEDOWN
#define RAWKEY_PAGEDOWN 0x49  /* Page Down */
#endif
#ifndef RAWKEY_HOME
#define RAWKEY_HOME     0x70  /* Home key */
#endif
#ifndef RAWKEY_END
#define RAWKEY_END      0x71  /* End key */
#endif

/* Qualifier masks */
#define IEQUALIFIER_LSHIFT  0x0001
#define IEQUALIFIER_RSHIFT  0x0002
#define IEQUALIFIER_SHIFT   (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)

/*
 * Handle special key and generate CSI sequence
 * Returns TRUE if the key was handled as a special key
 */
static BOOL handle_special_key(struct LxaConUnit *unit, UWORD rawkey, UWORD qualifier)
{
    BOOL shifted = (qualifier & IEQUALIFIER_SHIFT) != 0;
    char seq[8];
    
    switch (rawkey) {
        /* Arrow keys - RKRM documented sequences */
        case RAWKEY_UP:
            if (shifted) {
                /* Shifted Up: CSI T (scroll up) */
                seq[0] = 0x9B;
                seq[1] = 'T';
                seq[2] = '\0';
            } else {
                /* Unshifted Up: CSI A */
                seq[0] = 0x9B;
                seq[1] = 'A';
                seq[2] = '\0';
            }
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_DOWN:
            if (shifted) {
                /* Shifted Down: CSI S (scroll down) */
                seq[0] = 0x9B;
                seq[1] = 'S';
                seq[2] = '\0';
            } else {
                /* Unshifted Down: CSI B */
                seq[0] = 0x9B;
                seq[1] = 'B';
                seq[2] = '\0';
            }
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_RIGHT:
            if (shifted) {
                /* Shifted Right: CSI <space>@ (word right) */
                seq[0] = 0x9B;
                seq[1] = ' ';
                seq[2] = '@';
                seq[3] = '\0';
            } else {
                /* Unshifted Right: CSI C */
                seq[0] = 0x9B;
                seq[1] = 'C';
                seq[2] = '\0';
            }
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_LEFT:
            if (shifted) {
                /* Shifted Left: CSI <space>A (word left) */
                seq[0] = 0x9B;
                seq[1] = ' ';
                seq[2] = 'A';
                seq[3] = '\0';
            } else {
                /* Unshifted Left: CSI D */
                seq[0] = 0x9B;
                seq[1] = 'D';
                seq[2] = '\0';
            }
            input_buf_put_string(unit, seq);
            return TRUE;
            
        /* Function keys F1-F10 */
        case RAWKEY_F1:
        case RAWKEY_F2:
        case RAWKEY_F3:
        case RAWKEY_F4:
        case RAWKEY_F5:
        case RAWKEY_F6:
        case RAWKEY_F7:
        case RAWKEY_F8:
        case RAWKEY_F9:
        case RAWKEY_F10:
            {
                /* F1 = 0~, F2 = 1~, ..., F10 = 9~ (unshifted)
                   F1 = 10~, F2 = 11~, ..., F10 = 19~ (shifted) */
                int fnum = rawkey - RAWKEY_F1;  /* 0-9 */
                seq[0] = 0x9B;
                if (shifted) {
                    seq[1] = '1';
                    seq[2] = '0' + fnum;
                    seq[3] = '~';
                    seq[4] = '\0';
                } else {
                    seq[1] = '0' + fnum;
                    seq[2] = '~';
                    seq[3] = '\0';
                }
                input_buf_put_string(unit, seq);
            }
            return TRUE;
            
        /* Help key */
        case RAWKEY_HELP:
            /* Help: CSI ?~ */
            seq[0] = 0x9B;
            seq[1] = '?';
            seq[2] = '~';
            seq[3] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        /* Delete key - outputs DEL character */
        case RAWKEY_DEL:
            input_buf_put(unit, 0x7F);
            return TRUE;
        
        /* F11, F12 (101-key keyboards) */
        case RAWKEY_F11:
            /* F11 = 20~ (unshifted), 30~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '3';
                seq[2] = '0';
            } else {
                seq[1] = '2';
                seq[2] = '0';
            }
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_F12:
            /* F12 = 21~ (unshifted), 31~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '3';
                seq[2] = '1';
            } else {
                seq[1] = '2';
                seq[2] = '1';
            }
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        /* 101-key extended keys */
        case RAWKEY_INSERT:
            /* Insert: CSI 40~ (unshifted), CSI 50~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '5';
            } else {
                seq[1] = '4';
            }
            seq[2] = '0';
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_PAGEUP:
            /* Page Up: CSI 41~ (unshifted), CSI 51~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '5';
            } else {
                seq[1] = '4';
            }
            seq[2] = '1';
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_PAGEDOWN:
            /* Page Down: CSI 42~ (unshifted), CSI 52~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '5';
            } else {
                seq[1] = '4';
            }
            seq[2] = '2';
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_HOME:
            /* Home: CSI 44~ (unshifted), CSI 54~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '5';
            } else {
                seq[1] = '4';
            }
            seq[2] = '4';
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        case RAWKEY_END:
            /* End: CSI 45~ (unshifted), CSI 55~ (shifted) */
            seq[0] = 0x9B;
            if (shifted) {
                seq[1] = '5';
            } else {
                seq[1] = '4';
            }
            seq[2] = '5';
            seq[3] = '~';
            seq[4] = '\0';
            input_buf_put_string(unit, seq);
            return TRUE;
            
        default:
            return FALSE;
    }
}

/*
 * Process keyboard input from IDCMP
 * Called when reading from console to check for new input
 */
static void console_process_input(struct LxaConUnit *unit)
{
    struct Window *window;
    struct MsgPort *port;
    struct IntuiMessage *imsg;
    BOOL redraw_cursor = FALSE;
    
    if (!unit || !unit->cu.cu_Window) {
        return;
    }
    
    window = unit->cu.cu_Window;
    port = window->UserPort;
    
    if (!port) {
        return;
    }
    
    /* Hide cursor while processing input */
    console_hide_cursor(unit);
    
    /* Process all pending IDCMP messages */
    while ((imsg = (struct IntuiMessage *)GetMsg(port)) != NULL) {
        if (imsg->Class == IDCMP_RAWKEY) {
            UWORD rawkey = imsg->Code;
            UWORD qualifier = imsg->Qualifier;
            
            /* Only process key-down events */
            if (!(rawkey & 0x80)) {
                DPRINTF(LOG_DEBUG, "_console: RAWKEY 0x%02x qual=0x%04x\n",
                        rawkey, qualifier);
                
                /* First check for special keys (arrows, function keys, etc.)
                 * that generate CSI sequences instead of ASCII characters */
                if (handle_special_key(unit, rawkey, qualifier)) {
                    /* Special key handled - CSI sequence added to input buffer */
                    redraw_cursor = TRUE;
                    DPRINTF(LOG_DEBUG, "_console: Special key handled\n");
                } else {
                    /* Try normal ASCII conversion */
                    char c = rawkey_to_ascii(rawkey, qualifier);
                    
                    DPRINTF(LOG_DEBUG, "_console: ASCII conversion -> '%c' (0x%02x)\n",
                            (c >= 32 && c < 127) ? c : '.', (unsigned char)c);
                    
                    if (c) {
                        redraw_cursor = TRUE;
                        
                        /* Handle special characters */
                        if (c == '\b') {
                            /* Backspace - remove last character from buffer if possible */
                            BOOL can_backspace = FALSE;
                            
                            /* Only allow backspace if we have characters in our input buffer
                             * that are part of the current line (not past a newline) */
                            if (!input_buf_empty(unit)) {
                                UWORD prev = (unit->input_head + INPUT_BUF_LEN - 1) % INPUT_BUF_LEN;
                                if (unit->input_buf[prev] != '\r' && 
                                    unit->input_buf[prev] != '\n') {
                                    unit->input_head = prev;
                                    can_backspace = TRUE;
                                }
                            }
                            
                            if (can_backspace && unit->echo_enabled) {
                                /* Echo backspace: move cursor back, space, move back again */
                                console_process_char(unit, '\b');
                                console_process_char(unit, ' ');
                                console_process_char(unit, '\b');
                            }
                        } else {
                            /* Add character to buffer */
                            input_buf_put(unit, c);
                            
                            /* Echo if enabled */
                            if (unit->echo_enabled) {
                                if (c == '\r') {
                                    console_process_char(unit, '\r');
                                    console_process_char(unit, '\n');
                                } else if (c >= 32 || c == '\t') {
                                    console_process_char(unit, c);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        /* Reply to the message (free it) */
        ReplyMsg((struct Message *)imsg);
    }
    
    /* Try to complete any pending async read now that we have new input */
    console_try_complete_pending_read(unit);
    
    /* Redraw cursor and refresh display */
    if (redraw_cursor || unit->cursor_visible) {
        console_draw_cursor(unit);
        WaitTOF();
    }
}

/*
 * Try to complete a pending async read request (Phase 45: Async I/O)
 * Called after input is processed to check if we can satisfy a pending read.
 */
static void console_try_complete_pending_read(struct LxaConUnit *unit)
{
    struct IOStdReq *iostd;
    char *data;
    LONG len;
    LONG count = 0;
    
    if (!unit || !unit->pending_read) {
        return;
    }
    
    /* Check if we have enough data to satisfy the read */
    if (unit->line_mode) {
        /* In line mode, need a complete line */
        if (!input_has_line(unit)) {
            return;  /* Not ready yet */
        }
    } else {
        /* In raw mode, any data is enough */
        if (input_buf_empty(unit)) {
            return;  /* Not ready yet */
        }
    }
    
    /* We have data - complete the pending read */
    iostd = unit->pending_read;
    unit->pending_read = NULL;  /* Clear before completing */
    
    DPRINTF(LOG_DEBUG, "_console: completing pending read, iostd=0x%08lx\n", (ULONG)iostd);
    
    /* Hide cursor after reading is done */
    console_hide_cursor(unit);
    
    /* Copy available characters to buffer */
    data = (char *)iostd->io_Data;
    len = iostd->io_Length;
    
    while (count < len && !input_buf_empty(unit)) {
        char c = input_buf_get(unit);
        data[count++] = c;
        
        /* In line mode, stop at newline */
        if (unit->line_mode && (c == '\r' || c == '\n')) {
            break;
        }
    }
    
    iostd->io_Actual = count;
    iostd->io_Error = 0;
    
    DPRINTF(LOG_DEBUG, "_console: async read completed, %ld bytes\n", count);
    
    /* Complete the request via ReplyMsg */
    ReplyMsg(&iostd->io_Message);
}

/*
 * VBlank Hook for Console Device (Phase 45: Async I/O)
 * 
 * Called from the VBlank interrupt handler to process pending console reads.
 * This function:
 * 1. Looks up the console device from the device list
 * 2. Iterates through all registered console units
 * 3. For units with pending async reads, calls console_process_input()
 * 4. console_process_input() will call console_try_complete_pending_read()
 *    to complete the request if data is available
 * 
 * This must be called from the m68k side (not host) so that ReplyMsg() properly
 * signals the task through the Exec scheduler.
 */
VOID _console_VBlankHook(void)
{
    int i;
    struct LxaConUnit *unit;
    struct ConsoleDevBase *cdb = NULL;
    struct Node *node;
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    
    /* Look up console device from device list via SysBase */
    /* This avoids using static variables which don't work in ROM code */
    Forbid();
    for (node = SysBase->DeviceList.lh_Head; node->ln_Succ; node = node->ln_Succ) {
        if (node->ln_Name && strcmp(node->ln_Name, "console.device") == 0) {
            cdb = (struct ConsoleDevBase *)node;
            break;
        }
    }
    Permit();
    
    if (!cdb || cdb->cdb_NumUnits == 0) {
        return;  /* Console device not found or no units open */
    }
    
    /* Iterate through all registered units */
    for (i = 0; i < MAX_CONSOLE_UNITS; i++) {
        unit = cdb->cdb_Units[i];
        if (unit && unit->pending_read) {
            /* This unit has a pending async read - process input */
            console_process_input(unit);
        }
    }
}

/*
 * Write a single character to the console at current cursor position
 */
static void console_write_char(struct LxaConUnit *unit, char c)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x, y;
    char str[2];
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) {
        return;
    }
    
    /* Hide cursor before modifying the display */
    console_hide_cursor(unit);
    
    /* Calculate pixel position from character position */
    x = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize) + unit->cu.cu_YRSize - 1;  /* Baseline */
    
    /* Set up rastport for drawing */
    SetAPen(rp, unit->cu.cu_FgPen);
    SetBPen(rp, unit->cu.cu_BgPen);
    SetDrMd(rp, unit->cu.cu_DrawMode);
    
    /* Move to position and draw character */
    Move(rp, x, y);
    str[0] = c;
    str[1] = '\0';
    Text(rp, (CONST_STRPTR)str, 1);
    
    /* Advance cursor */
    unit->cu.cu_XCP++;
    if (unit->cu.cu_XCP > unit->cu.cu_XMax) {
        /* Wrap to next line */
        console_newline(unit);
    }
}

/*
 * Handle newline - move cursor down, scroll if needed
 */
static void console_newline(struct LxaConUnit *unit)
{
    unit->cu.cu_XCP = 0;
    unit->cu.cu_YCP++;
    
    if (unit->cu.cu_YCP > unit->cu.cu_YMax) {
        /* Need to scroll */
        console_scroll_up(unit);
        unit->cu.cu_YCP = unit->cu.cu_YMax;
    }
}

/*
 * Handle carriage return - move cursor to start of line
 */
static void console_carriage_return(struct LxaConUnit *unit)
{
    unit->cu.cu_XCP = 0;
}

/*
 * Scroll the console up by one line
 */
static void console_scroll_up(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    /* Calculate scrolling region bounds in pixels */
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->scroll_top * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = unit->cu.cu_YROrigin + ((unit->scroll_bottom + 1) * unit->cu.cu_YRSize) - 1;
    
    /* Use ScrollRaster to scroll content up within the scrolling region */
    /* ScrollRaster(rp, dx, dy, x1, y1, x2, y2) moves content by (dx, dy) pixels */
    /* We want to move up by one character height */
    ScrollRaster(rp, 0, unit->cu.cu_YRSize, x1, y1, x2, y2);
    
    DPRINTF(LOG_DEBUG, "_console: scroll_up() scrolled region [%d-%d] by %d pixels\n", 
            unit->scroll_top, unit->scroll_bottom, unit->cu.cu_YRSize);
}

/*
 * Scroll the console down by one line
 */
static void console_scroll_down(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    /* Calculate scrolling region bounds in pixels */
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->scroll_top * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = unit->cu.cu_YROrigin + ((unit->scroll_bottom + 1) * unit->cu.cu_YRSize) - 1;
    
    /* Scroll down by one character height (negative dy) */
    ScrollRaster(rp, 0, -unit->cu.cu_YRSize, x1, y1, x2, y2);
    
    DPRINTF(LOG_DEBUG, "_console: scroll_down() scrolled region [%d-%d] by %d pixels\n",
            unit->scroll_top, unit->scroll_bottom, unit->cu.cu_YRSize);
}

/*
 * Move cursor up one line (INDEX reverse)
 */
static void console_cursor_up(struct LxaConUnit *unit)
{
    if (unit->cu.cu_YCP > unit->scroll_top) {
        /* Within scrolling region, just move cursor up */
        unit->cu.cu_YCP--;
    } else if (unit->cu.cu_YCP == unit->scroll_top) {
        /* At top of scrolling region, scroll down */
        console_scroll_down(unit);
    } else if (unit->cu.cu_YCP > 0) {
        /* Above scrolling region, just move cursor up */
        unit->cu.cu_YCP--;
    }
}

/*
 * Move cursor down one line (INDEX)
 */
static void console_cursor_down(struct LxaConUnit *unit)
{
    if (unit->cu.cu_YCP < unit->scroll_bottom) {
        /* Within scrolling region, just move cursor down */
        unit->cu.cu_YCP++;
    } else if (unit->cu.cu_YCP == unit->scroll_bottom) {
        /* At bottom of scrolling region, scroll up if auto-scroll is enabled */
        if (unit->autoscroll_mode) {
            console_scroll_up(unit);
        }
    } else if (unit->cu.cu_YCP < unit->cu.cu_YMax) {
        /* Below scrolling region, just move cursor down */
        unit->cu.cu_YCP++;
    } else if (unit->autoscroll_mode) {
        /* At bottom of screen, scroll up */
        console_scroll_up(unit);
    }
}

/*
 * Initialize tab stops (every 8 columns)
 */
static void console_init_tab_stops(struct LxaConUnit *unit)
{
    int i;
    for (i = 0; i < 80 && (i * 8) <= unit->cu.cu_XMax; i++) {
        unit->tab_stops[i] = i * 8;
    }
    /* Mark end of tab stops */
    if (i < 80) {
        unit->tab_stops[i] = (UWORD)-1;
    }
}

/*
 * Clear from cursor to end of line
 */
static void console_clear_to_eol(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    x1 = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    SetAPen(rp, unit->cu.cu_BgPen);
    RectFill(rp, x1, y1, x2, y2);
}

/*
 * Clear from start of line to cursor
 */
static void console_clear_to_bol(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XROrigin + ((unit->cu.cu_XCP + 1) * unit->cu.cu_XRSize) - 1;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    SetAPen(rp, unit->cu.cu_BgPen);
    RectFill(rp, x1, y1, x2, y2);
}

/*
 * Clear entire line
 */
static void console_clear_line(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    SetAPen(rp, unit->cu.cu_BgPen);
    RectFill(rp, x1, y1, x2, y2);
}

/*
 * Clear from cursor to end of screen (mode 0)
 */
static void console_clear_to_eos(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    /* First clear from cursor to end of line */
    console_clear_to_eol(unit);
    
    /* Then clear rest of display */
    if (unit->cu.cu_YCP < unit->cu.cu_YMax) {
        WORD x1 = unit->cu.cu_XROrigin;
        WORD y1 = unit->cu.cu_YROrigin + ((unit->cu.cu_YCP + 1) * unit->cu.cu_YRSize);
        WORD x2 = unit->cu.cu_XRExtant;
        WORD y2 = unit->cu.cu_YRExtant;
        
        SetAPen(rp, unit->cu.cu_BgPen);
        RectFill(rp, x1, y1, x2, y2);
    }
}

/*
 * Clear from start of screen to cursor (mode 1)
 */
static void console_clear_to_bos(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    /* First clear lines above cursor */
    if (unit->cu.cu_YCP > 0) {
        WORD x1 = unit->cu.cu_XROrigin;
        WORD y1 = unit->cu.cu_YROrigin;
        WORD x2 = unit->cu.cu_XRExtant;
        WORD y2 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize) - 1;
        
        SetAPen(rp, unit->cu.cu_BgPen);
        RectFill(rp, x1, y1, x2, y2);
    }
    
    /* Then clear from start of line to cursor */
    console_clear_to_bol(unit);
}

/*
 * Insert line - insert blank line at cursor, move lines below down
 */
static void console_insert_line(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = unit->cu.cu_YRExtant;
    
    /* Scroll down from cursor position to bottom */
    SetAPen(rp, unit->cu.cu_BgPen);
    ScrollRaster(rp, 0, -unit->cu.cu_YRSize, x1, y1, x2, y2);
}

/*
 * Delete line - remove line at cursor, move lines below up
 */
static void console_delete_line(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = unit->cu.cu_YRExtant;
    
    /* Scroll up from cursor position to bottom */
    SetAPen(rp, unit->cu.cu_BgPen);
    ScrollRaster(rp, 0, unit->cu.cu_YRSize, x1, y1, x2, y2);
}

/*
 * Insert characters - insert n blank spaces at cursor, shift rest of line right
 */
static void console_insert_char(struct LxaConUnit *unit, int n)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window || n <= 0) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    x1 = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    /* Scroll right by n characters */
    SetAPen(rp, unit->cu.cu_BgPen);
    ScrollRaster(rp, -(n * unit->cu.cu_XRSize), 0, x1, y1, x2, y2);
}

/*
 * Delete characters - delete n characters at cursor, shift rest of line left
 */
static void console_delete_char(struct LxaConUnit *unit, int n)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window || n <= 0) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp || !rp->BitMap) return;
    
    x1 = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = unit->cu.cu_XRExtant;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    /* Scroll left by n characters */
    SetAPen(rp, unit->cu.cu_BgPen);
    ScrollRaster(rp, n * unit->cu.cu_XRSize, 0, x1, y1, x2, y2);
}

/*
 * Clear entire display
 */
static void console_clear_display(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    
    if (!unit || !unit->cu.cu_Window) return;
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    SetAPen(rp, unit->cu.cu_BgPen);
    RectFill(rp, unit->cu.cu_XROrigin, unit->cu.cu_YROrigin, 
             unit->cu.cu_XRExtant, unit->cu.cu_YRExtant);
    
    /* Reset cursor to home */
    unit->cu.cu_XCP = 0;
    unit->cu.cu_YCP = 0;
}

/*
 * Draw cursor at current position (XOR block cursor)
 */
static void console_draw_cursor(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    if (!unit->cursor_visible) return;
    if (unit->cursor_drawn) return;  /* Already drawn */
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    /* Calculate pixel rectangle for cursor */
    x1 = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = x1 + unit->cu.cu_XRSize - 1;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    /* Draw cursor as XOR block (COMPLEMENT mode) */
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);  /* Will be XORed */
    RectFill(rp, x1, y1, x2, y2);
    SetDrMd(rp, unit->cu.cu_DrawMode);  /* Restore draw mode */
    
    unit->cursor_drawn = TRUE;
}

/*
 * Hide cursor (erase it from screen)
 */
static void console_hide_cursor(struct LxaConUnit *unit)
{
    struct Window *window;
    struct RastPort *rp;
    WORD x1, y1, x2, y2;
    
    if (!unit || !unit->cu.cu_Window) return;
    if (!unit->cursor_drawn) return;  /* Not drawn */
    
    window = unit->cu.cu_Window;
    rp = window->RPort;
    if (!rp) return;
    
    /* Calculate pixel rectangle for cursor */
    x1 = unit->cu.cu_XROrigin + (unit->cu.cu_XCP * unit->cu.cu_XRSize);
    y1 = unit->cu.cu_YROrigin + (unit->cu.cu_YCP * unit->cu.cu_YRSize);
    x2 = x1 + unit->cu.cu_XRSize - 1;
    y2 = y1 + unit->cu.cu_YRSize - 1;
    
    /* Erase cursor with XOR (same as draw) */
    SetDrMd(rp, COMPLEMENT);
    SetAPen(rp, 1);
    RectFill(rp, x1, y1, x2, y2);
    SetDrMd(rp, unit->cu.cu_DrawMode);
    
    unit->cursor_drawn = FALSE;
}

/*
 * Parse CSI parameters and return them as integers
 * Returns number of parameters found
 */
static int parse_csi_params(const char *buf, int len, int *params, int max_params)
{
    int count = 0;
    int value = 0;
    BOOL have_value = FALSE;
    
    for (int i = 0; i < len && count < max_params; i++) {
        char c = buf[i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            have_value = TRUE;
        } else if (c == ';') {
            params[count++] = have_value ? value : 0;
            value = 0;
            have_value = FALSE;
        }
    }
    
    /* Don't forget the last parameter */
    if (have_value || count > 0) {
        params[count++] = have_value ? value : 0;
    }
    
    return count;
}

/*
 * Process a complete CSI sequence
 */
static void console_process_csi(struct LxaConUnit *unit, char final)
{
    int params[8] = {0};
    int nparams;
    
    nparams = parse_csi_params(unit->csi_buf, unit->csi_len, params, 8);
    
    DPRINTF(LOG_DEBUG, "_console: CSI '%s' final='%c' nparams=%d params[0]=%d params[1]=%d\n",
            unit->csi_buf, final, nparams, params[0], params[1]);
    
    switch (final) {
        case 'H':  /* Cursor Position (CUP) - CSI row;col H */
        case 'f':  /* Horizontal and Vertical Position (HVP) - same as CUP */
        {
            int row = (nparams >= 1 && params[0] > 0) ? params[0] - 1 : 0;
            int col = (nparams >= 2 && params[1] > 0) ? params[1] - 1 : 0;
            
            /* Clamp to valid range */
            if (row > unit->cu.cu_YMax) row = unit->cu.cu_YMax;
            if (col > unit->cu.cu_XMax) col = unit->cu.cu_XMax;
            if (row < 0) row = 0;
            if (col < 0) col = 0;
            
            unit->cu.cu_YCP = row;
            unit->cu.cu_XCP = col;
            DPRINTF(LOG_DEBUG, "_console: CUP -> cursor at (%d, %d)\n", unit->cu.cu_XCP, unit->cu.cu_YCP);
            break;
        }
        
        case 'A':  /* Cursor Up (CUU) */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_YCP -= n;
            if (unit->cu.cu_YCP < 0) unit->cu.cu_YCP = 0;
            break;
        }
        
        case 'B':  /* Cursor Down (CUD) */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_YCP += n;
            if (unit->cu.cu_YCP > unit->cu.cu_YMax) unit->cu.cu_YCP = unit->cu.cu_YMax;
            break;
        }
        
        case 'C':  /* Cursor Forward (CUF) */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_XCP += n;
            if (unit->cu.cu_XCP > unit->cu.cu_XMax) unit->cu.cu_XCP = unit->cu.cu_XMax;
            break;
        }
        
        case 'D':  /* Cursor Backward (CUB) */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_XCP -= n;
            if (unit->cu.cu_XCP < 0) unit->cu.cu_XCP = 0;
            break;
        }
        
        case 'E':  /* Cursor Next Line (CNL) - move cursor to beginning of line n lines down */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_XCP = 0;
            unit->cu.cu_YCP += n;
            if (unit->cu.cu_YCP > unit->cu.cu_YMax) unit->cu.cu_YCP = unit->cu.cu_YMax;
            break;
        }
        
        case 'F':  /* Cursor Previous Line (CPL) - move cursor to beginning of line n lines up */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            unit->cu.cu_XCP = 0;
            unit->cu.cu_YCP -= n;
            if (unit->cu.cu_YCP < 0) unit->cu.cu_YCP = 0;
            break;
        }
        
        case 'G':  /* Cursor Horizontal Absolute (CHA) - move cursor to column n */
        {
            int col = (nparams >= 1 && params[0] > 0) ? params[0] - 1 : 0;
            if (col > unit->cu.cu_XMax) col = unit->cu.cu_XMax;
            if (col < 0) col = 0;
            unit->cu.cu_XCP = col;
            break;
        }
        
        case 'I':  /* Cursor Horizontal Tab (CHT) - move cursor to next tab stop n times */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                unit->cu.cu_XCP = ((unit->cu.cu_XCP / 8) + 1) * 8;
                if (unit->cu.cu_XCP > unit->cu.cu_XMax) {
                    unit->cu.cu_XCP = unit->cu.cu_XMax;
                    break;
                }
            }
            break;
        }
        
        case 'J':  /* Erase in Display (ED) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            switch (mode) {
                case 0:  /* Clear from cursor to end of screen */
                    console_clear_to_eos(unit);
                    break;
                case 1:  /* Clear from start of screen to cursor */
                    console_clear_to_bos(unit);
                    break;
                case 2:  /* Clear entire display */
                    console_clear_display(unit);
                    break;
            }
            break;
        }
        
        case 'K':  /* Erase in Line (EL) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            switch (mode) {
                case 0:  /* Clear from cursor to end of line */
                    console_clear_to_eol(unit);
                    break;
                case 1:  /* Clear from start of line to cursor */
                    console_clear_to_bol(unit);
                    break;
                case 2:  /* Clear entire line */
                    console_clear_line(unit);
                    break;
            }
            break;
        }
        
        case 'L':  /* Insert Line (IL) - insert n blank lines at cursor */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                console_insert_line(unit);
            }
            break;
        }
        
        case 'M':  /* Delete Line (DL) - delete n lines at cursor */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                console_delete_line(unit);
            }
            break;
        }
        
        case '@':  /* Insert Character (ICH) - insert n blank characters at cursor */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            console_insert_char(unit, n);
            break;
        }
        
        case 'P':  /* Delete Character (DCH) - delete n characters at cursor */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            console_delete_char(unit, n);
            break;
        }
        
        case 'S':  /* Scroll Up (SU) - scroll display up n lines */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                console_scroll_up(unit);
            }
            break;
        }
        
        case 'T':  /* Scroll Down (SD) - scroll display down n lines */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                console_scroll_down(unit);
            }
            break;
        }
        
        case 'W':  /* Cursor Tab Control (CTC) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            switch (mode) {
                case 0:  /* Set tab stop at current column */
                    {
                        int i;
                        for (i = 0; i < 80 && unit->tab_stops[i] != (UWORD)-1; i++) {
                            if (unit->tab_stops[i] == unit->cu.cu_XCP) break;  /* Already set */
                            if (unit->tab_stops[i] > unit->cu.cu_XCP) {
                                /* Insert here */
                                int j;
                                for (j = 78; j >= i; j--) {
                                    unit->tab_stops[j + 1] = unit->tab_stops[j];
                                }
                                unit->tab_stops[i] = unit->cu.cu_XCP;
                                break;
                            }
                        }
                        if (i < 80 && unit->tab_stops[i] == (UWORD)-1) {
                            unit->tab_stops[i] = unit->cu.cu_XCP;
                            if (i + 1 < 80) unit->tab_stops[i + 1] = (UWORD)-1;
                        }
                    }
                    break;
                case 2:  /* Clear tab stop at current column */
                    {
                        int i;
                        for (i = 0; i < 80 && unit->tab_stops[i] != (UWORD)-1; i++) {
                            if (unit->tab_stops[i] == unit->cu.cu_XCP) {
                                /* Remove this tab stop */
                                for (; i < 79 && unit->tab_stops[i] != (UWORD)-1; i++) {
                                    unit->tab_stops[i] = unit->tab_stops[i + 1];
                                }
                                break;
                            }
                        }
                    }
                    break;
                case 5:  /* Clear all tab stops */
                    unit->tab_stops[0] = (UWORD)-1;
                    break;
            }
            break;
        }
        
        case 'Z':  /* Cursor Backward Tab (CBT) - move cursor to previous tab stop n times */
        {
            int n = (nparams >= 1 && params[0] > 0) ? params[0] : 1;
            for (int i = 0; i < n; i++) {
                /* Move to previous tab stop (8 columns) */
                if (unit->cu.cu_XCP > 0) {
                    unit->cu.cu_XCP = ((unit->cu.cu_XCP - 1) / 8) * 8;
                }
            }
            break;
        }
        
        case 'n':  /* Device Status Report (DSR) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            if (mode == 6) {
                /* Report cursor position - respond with CSI row;col R */
                /* Put response in input buffer */
                char response[16];
                int len, j;
                len = 0;
                response[len++] = 0x9B;  /* CSI */
                /* Convert row+1 to string */
                if (unit->cu.cu_YCP + 1 >= 10) {
                    response[len++] = '0' + ((unit->cu.cu_YCP + 1) / 10);
                }
                response[len++] = '0' + ((unit->cu.cu_YCP + 1) % 10);
                response[len++] = ';';
                /* Convert col+1 to string */
                if (unit->cu.cu_XCP + 1 >= 10) {
                    response[len++] = '0' + ((unit->cu.cu_XCP + 1) / 10);
                }
                response[len++] = '0' + ((unit->cu.cu_XCP + 1) % 10);
                response[len++] = 'R';
                /* Put in input buffer */
                for (j = 0; j < len; j++) {
                    input_buf_put(unit, response[j]);
                }
            }
            break;
        }
        
        case 'h':  /* Set Mode (SM) */
        {
            /* Check for private mode prefix '?' */
            BOOL private_mode = (unit->csi_len > 0 && unit->csi_buf[0] == '?');
            int mode = (nparams >= 1) ? params[0] : 0;
            
            if (private_mode) {
                switch (mode) {
                    case 7:  /* Auto-wrap mode (AWM) */
                        unit->autowrap_mode = TRUE;
                        break;
                }
            } else {
                switch (mode) {
                    case 20:  /* Line Feed/New Line Mode (LNM) */
                        unit->lf_mode = TRUE;
                        break;
                }
            }
            /* ASM (Auto-scroll mode) - modes >1 are Amiga-specific */
            if (mode > 1 && !private_mode) {
                unit->autoscroll_mode = TRUE;
            }
            break;
        }
        
        case 'l':  /* Reset Mode (RM) */
        {
            /* Check for private mode prefix '?' */
            BOOL private_mode = (unit->csi_len > 0 && unit->csi_buf[0] == '?');
            int mode = (nparams >= 1) ? params[0] : 0;
            
            if (private_mode) {
                switch (mode) {
                    case 7:  /* Auto-wrap mode (AWM) */
                        unit->autowrap_mode = FALSE;
                        break;
                }
            } else {
                switch (mode) {
                    case 20:  /* Line Feed/New Line Mode (LNM) */
                        unit->lf_mode = FALSE;
                        break;
                }
            }
            /* ASM (Auto-scroll mode) - modes >1 are Amiga-specific */
            if (mode > 1 && !private_mode) {
                unit->autoscroll_mode = FALSE;
            }
            break;
        }
        
        case 'q':  /* Window Status Request (Amiga-specific) */
        {
            /* Various window status queries */
            int mode = (nparams >= 1) ? params[0] : 0;
            char response[32];
            int len = 0, j;
            
            response[len++] = 0x9B;  /* CSI */
            
            switch (mode) {
                case 0:  /* Report window bounds in characters */
                    /* Response: CSI 1;1;rows;cols r */
                    response[len++] = '1';
                    response[len++] = ';';
                    response[len++] = '1';
                    response[len++] = ';';
                    /* rows */
                    if (unit->cu.cu_YMax + 1 >= 10) {
                        response[len++] = '0' + ((unit->cu.cu_YMax + 1) / 10);
                    }
                    response[len++] = '0' + ((unit->cu.cu_YMax + 1) % 10);
                    response[len++] = ';';
                    /* cols */
                    if (unit->cu.cu_XMax + 1 >= 10) {
                        response[len++] = '0' + ((unit->cu.cu_XMax + 1) / 10);
                    }
                    response[len++] = '0' + ((unit->cu.cu_XMax + 1) % 10);
                    response[len++] = ' ';
                    response[len++] = 'r';
                    break;
                default:
                    len = 0;  /* Unknown query, no response */
                    break;
            }
            
            /* Put in input buffer */
            for (j = 0; j < len; j++) {
                input_buf_put(unit, response[j]);
            }
            break;
        }
        
        case 't':  /* Set page length (Amiga-specific) */
        {
            /* CSI n t - set page length to n lines */
            /* For now, we ignore this as we use the window size */
            break;
        }
        
        case 'u':  /* Set line length (Amiga-specific) */
        {
            /* CSI n u - set line length to n characters */
            /* For now, we ignore this as we use the window size */
            break;
        }
        
        case 'x':  /* Set left offset (Amiga-specific) */
        {
            /* CSI n x - set left offset to n pixels */
            /* For now, we ignore this */
            break;
        }
        
        case 'y':  /* Set top offset (Amiga-specific) */
        {
            /* CSI n y - set top offset to n pixels */
            /* For now, we ignore this */
            break;
        }
        
        case 'm':  /* Select Graphic Rendition (SGR) */
        {
            for (int i = 0; i < nparams || nparams == 0; i++) {
                int p = (nparams > 0) ? params[i] : 0;
                switch (p) {
                    case 0:   /* Reset all attributes */
                        unit->cu.cu_FgPen = 1;
                        unit->cu.cu_BgPen = 0;
                        unit->cu.cu_DrawMode = JAM2;
                        unit->sgr_bold = FALSE;
                        unit->sgr_faint = FALSE;
                        unit->sgr_italic = FALSE;
                        unit->sgr_underline = FALSE;
                        unit->sgr_inverse = FALSE;
                        unit->sgr_concealed = FALSE;
                        unit->saved_fg = 1;
                        unit->saved_bg = 0;
                        break;
                    case 1:   /* Bold/bright */
                        unit->sgr_bold = TRUE;
                        unit->sgr_faint = FALSE;
                        /* Amiga: bold often shown with pen 3 (bright color) */
                        if (unit->cu.cu_FgPen < 4) {
                            unit->cu.cu_FgPen |= 0x01;  /* Make it brighter */
                        }
                        break;
                    case 2:   /* Faint/dim */
                        unit->sgr_faint = TRUE;
                        unit->sgr_bold = FALSE;
                        break;
                    case 3:   /* Italic (Amiga shows as inverse or different style) */
                        unit->sgr_italic = TRUE;
                        break;
                    case 4:   /* Underline */
                        unit->sgr_underline = TRUE;
                        /* Amiga: underline often shown with pen 3 */
                        break;
                    case 7:   /* Inverse video */
                        if (!unit->sgr_inverse) {
                            unit->sgr_inverse = TRUE;
                            unit->saved_fg = unit->cu.cu_FgPen;
                            unit->saved_bg = unit->cu.cu_BgPen;
                            unit->cu.cu_FgPen = unit->saved_bg;
                            unit->cu.cu_BgPen = unit->saved_fg;
                        }
                        break;
                    case 8:   /* Concealed/hidden */
                        unit->sgr_concealed = TRUE;
                        break;
                    case 21:  /* Not bold (double underline in some terminals) */
                    case 22:  /* Normal intensity (not bold, not faint) */
                        unit->sgr_bold = FALSE;
                        unit->sgr_faint = FALSE;
                        break;
                    case 23:  /* Not italic */
                        unit->sgr_italic = FALSE;
                        break;
                    case 24:  /* Not underlined */
                        unit->sgr_underline = FALSE;
                        break;
                    case 27:  /* Not inverse */
                        if (unit->sgr_inverse) {
                            unit->sgr_inverse = FALSE;
                            unit->cu.cu_FgPen = unit->saved_fg;
                            unit->cu.cu_BgPen = unit->saved_bg;
                        }
                        break;
                    case 28:  /* Not concealed */
                        unit->sgr_concealed = FALSE;
                        break;
                    case 30: case 31: case 32: case 33:
                    case 34: case 35: case 36: case 37:
                        /* Foreground colors 0-7 */
                        unit->cu.cu_FgPen = p - 30;
                        unit->saved_fg = unit->cu.cu_FgPen;
                        if (unit->sgr_inverse) {
                            unit->cu.cu_BgPen = p - 30;
                        }
                        break;
                    case 39:  /* Default foreground */
                        unit->cu.cu_FgPen = 1;
                        unit->saved_fg = 1;
                        if (unit->sgr_inverse) {
                            unit->cu.cu_BgPen = 1;
                        }
                        break;
                    case 40: case 41: case 42: case 43:
                    case 44: case 45: case 46: case 47:
                        /* Background colors 0-7 */
                        unit->cu.cu_BgPen = p - 40;
                        unit->saved_bg = unit->cu.cu_BgPen;
                        if (unit->sgr_inverse) {
                            unit->cu.cu_FgPen = p - 40;
                        }
                        break;
                    case 49:  /* Default background */
                        unit->cu.cu_BgPen = 0;
                        unit->saved_bg = 0;
                        if (unit->sgr_inverse) {
                            unit->cu.cu_FgPen = 0;
                        }
                        break;
                }
                if (nparams == 0) break;  /* Exit after handling reset with no params */
            }
            break;
        }
        
        case 'p':  /* Cursor visibility (Amiga-specific) */
        {
            /* CSI 0 p = hide cursor, CSI p or CSI 1 p = show cursor */
            int mode = (nparams >= 1) ? params[0] : 1;
            if (mode == 0) {
                console_hide_cursor(unit);
                unit->cursor_visible = FALSE;
            } else {
                unit->cursor_visible = TRUE;
                /* Cursor will be drawn on next refresh */
            }
            DPRINTF(LOG_DEBUG, "_console: cursor visibility = %d\n", unit->cursor_visible);
            break;
        }
        
        case 'r':  /* Set Top and Bottom Margins (DECSTBM) - CSI top;bottom r */
        {
            int top = (nparams >= 1 && params[0] > 0) ? params[0] - 1 : 0;
            int bottom = (nparams >= 2 && params[1] > 0) ? params[1] - 1 : unit->cu.cu_YMax;
            
            /* Validate and clamp to screen boundaries */
            if (top < 0) top = 0;
            if (top > unit->cu.cu_YMax) top = unit->cu.cu_YMax;
            if (bottom < 0) bottom = 0;
            if (bottom > unit->cu.cu_YMax) bottom = unit->cu.cu_YMax;
            
            /* Top must be less than bottom */
            if (top >= bottom) {
                /* Invalid region, reset to full screen */
                unit->scroll_top = 0;
                unit->scroll_bottom = unit->cu.cu_YMax;
            } else {
                unit->scroll_top = top;
                unit->scroll_bottom = bottom;
            }
            
            /* Move cursor to home position (top-left of screen, not scrolling region) */
            unit->cu.cu_XCP = 0;
            unit->cu.cu_YCP = 0;
            
            DPRINTF(LOG_DEBUG, "_console: DECSTBM - set scrolling region to lines %d-%d\n", 
                    unit->scroll_top, unit->scroll_bottom);
            break;
        }
        
        case '{':  /* Set scrolling region (Amiga-specific) - CSI n { */
        {
            /* This sets how many lines to use for the scrolling region from the top */
            int lines = (nparams >= 1 && params[0] > 0) ? params[0] : unit->cu.cu_YMax + 1;
            
            /* Clamp to screen size */
            if (lines < 1) lines = 1;
            if (lines > unit->cu.cu_YMax + 1) lines = unit->cu.cu_YMax + 1;
            
            /* Set scrolling region from top to specified line */
            unit->scroll_top = 0;
            unit->scroll_bottom = lines - 1;
            
            DPRINTF(LOG_DEBUG, "_console: Amiga scrolling region - set to %d lines (0-%d)\n",
                    lines, unit->scroll_bottom);
            break;
        }
        
        default:
            DPRINTF(LOG_DEBUG, "_console: Unhandled CSI sequence '%s%c'\n", unit->csi_buf, final);
            break;
    }
}

/*
 * Process a character for a console
 */
static void console_process_char(struct LxaConUnit *unit, char c)
{
    UBYTE uc = (UBYTE)c;
    
    if (!unit->csi_active && !unit->esc_active) {
        /* Not in escape sequence */
        if (uc == 0x9B) {
            /* Amiga CSI (single byte) */
            unit->csi_active = TRUE;
            unit->csi_len = 0;
            return;
        } else if (uc == 0x1B) {
            /* ESC - might be start of ESC[ sequence */
            unit->esc_active = TRUE;
            return;
        }
        
        /* Regular character or control code */
        switch (uc) {
            case '\n':  /* Line Feed (0x0A) */
                if (unit->lf_mode) {
                    /* LF mode: LF acts as CR+LF */
                    console_carriage_return(unit);
                }
                console_cursor_down(unit);
                break;
            case '\r':  /* Carriage Return (0x0D) */
                console_carriage_return(unit);
                break;
            case '\t':  /* Tab (0x09) */
                /* Move to next tab stop (every 8 characters) */
                unit->cu.cu_XCP = ((unit->cu.cu_XCP / 8) + 1) * 8;
                if (unit->cu.cu_XCP > unit->cu.cu_XMax) {
                    if (unit->autowrap_mode) {
                        console_newline(unit);
                    } else {
                        unit->cu.cu_XCP = unit->cu.cu_XMax;
                    }
                }
                break;
            case '\b':  /* Backspace (0x08) */
                if (unit->cu.cu_XCP > 0) {
                    unit->cu.cu_XCP--;
                }
                break;
            case '\a':  /* Bell (0x07) */
                /* TODO: Could trigger a visual or audio bell */
                break;
            case 0x0B:  /* Vertical Tab (VTAB) - move cursor up one line */
                console_cursor_up(unit);
                break;
            case '\f':  /* Form Feed (0x0C) - clear screen */
                console_clear_display(unit);
                break;
            case 0x0E:  /* SHIFT OUT - set MSB on output characters */
                unit->shift_out = TRUE;
                break;
            case 0x0F:  /* SHIFT IN - clear MSB setting */
                unit->shift_out = FALSE;
                break;
            case 0x84:  /* INDEX (IND) - move cursor down one line, scroll if needed */
                console_cursor_down(unit);
                break;
            case 0x85:  /* NEXT LINE (NEL) - CR + LF */
                console_carriage_return(unit);
                console_cursor_down(unit);
                break;
            case 0x88:  /* Horizontal Tab Set (HTS) - set tab stop at cursor position */
                {
                    int i;
                    /* Find a slot for this tab stop */
                    for (i = 0; i < 80 && unit->tab_stops[i] != (UWORD)-1; i++) {
                        if (unit->tab_stops[i] == unit->cu.cu_XCP) {
                            /* Already set */
                            break;
                        }
                        if (unit->tab_stops[i] > unit->cu.cu_XCP) {
                            /* Insert here - shift rest down */
                            int j;
                            for (j = 78; j >= i; j--) {
                                unit->tab_stops[j + 1] = unit->tab_stops[j];
                            }
                            unit->tab_stops[i] = unit->cu.cu_XCP;
                            break;
                        }
                    }
                    if (i < 80 && unit->tab_stops[i] == (UWORD)-1) {
                        unit->tab_stops[i] = unit->cu.cu_XCP;
                        if (i + 1 < 80) {
                            unit->tab_stops[i + 1] = (UWORD)-1;
                        }
                    }
                }
                break;
            case 0x8D:  /* Reverse Index (RI) - move cursor up one line, scroll if needed */
                console_cursor_up(unit);
                break;
            default:
                if (uc >= 32 && uc != 127) {
                    /* Printable character (32-126 and 128-255) */
                    char ch = c;
                    if (unit->shift_out) {
                        ch = (char)(uc | 0x80);  /* Set MSB */
                    }
                    console_write_char(unit, ch);
                }
                /* Ignore other control characters (0-31 and 127/DEL) */
                break;
        }
    } else if (unit->esc_active) {
        if (uc == '[') {
            /* ESC[ is equivalent to CSI */
            unit->esc_active = FALSE;
            unit->csi_active = TRUE;
            unit->csi_len = 0;
        } else if (uc == 'c') {
            /* ESC c - Reset to Initial State */
            unit->esc_active = FALSE;
            unit->cu.cu_FgPen = 1;
            unit->cu.cu_BgPen = 0;
            unit->cu.cu_DrawMode = JAM2;
        } else {
            /* Unknown ESC sequence, ignore */
            unit->esc_active = FALSE;
        }
    } else if (unit->csi_active) {
        /* CSI sequence parsing */
        if (uc >= 0x40 && uc <= 0x7E) {
            /* Final byte - process the sequence */
            unit->csi_buf[unit->csi_len] = '\0';
            console_process_csi(unit, c);
            unit->csi_active = FALSE;
            unit->csi_len = 0;
        } else if (uc >= 0x20 && uc <= 0x3F) {
            /* Parameter or intermediate byte */
            if (unit->csi_len < CSI_BUF_LEN - 1) {
                unit->csi_buf[unit->csi_len++] = c;
            }
        } else {
            /* Invalid byte in CSI sequence - abort */
            unit->csi_active = FALSE;
            unit->csi_len = 0;
        }
    }
}

static struct Library * __g_lxa_console_InitDev  ( register struct Library    *dev     __asm("d0"),
                                                          register BPTR               seglist __asm("a0"),
                                                          register struct ExecBase   *SysBase __asm("a6"))
{
    struct ConsoleDevBase *cdb = (struct ConsoleDevBase *)dev;
    int i;
    
    DPRINTF (LOG_DEBUG, "_console: InitDev() called, cdb=0x%08lx.\n", (ULONG)cdb);
    
    /* Initialize the unit tracking array (Phase 45: async I/O) */
    for (i = 0; i < MAX_CONSOLE_UNITS; i++) {
        cdb->cdb_Units[i] = NULL;
    }
    cdb->cdb_NumUnits = 0;
    
    return dev;
}

/* IDCMP_RAWKEY constant - 1<<10 = 0x400 */
#define IDCMP_RAWKEY_FLAG 0x0400

static void __g_lxa_console_Open ( register struct Library   *dev   __asm("a6"),
                                            register struct IORequest *ioreq __asm("a1"),
                                            register ULONG            unitn  __asm("d0"),
                                            register ULONG            flags  __asm("d1"))
{
    struct ConsoleDevBase *cdb = (struct ConsoleDevBase *)dev;
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    struct Window *window = (struct Window *)iostd->io_Data;
    struct LxaConUnit *unit = NULL;
    int i;
    
    LPRINTF (LOG_INFO, "_console: Open() called, unitn=%ld, flags=0x%08lx, io_Data(Window)=0x%08lx\n", 
             unitn, flags, (ULONG)window);
    
    /* Create a ConUnit for this console if a window was provided */
    if (window != NULL) {
        unit = console_create_unit(window);
        if (!unit) {
            ioreq->io_Error = IOERR_OPENFAIL;
            return;
        }
        
        /* Set unit mode based on unitn parameter */
        switch (unitn) {
            case CONU_STANDARD:
                unit->unit_mode = CONU_STANDARD;
                unit->use_keymap = FALSE;
                LPRINTF(LOG_INFO, "_console: Opened as CONU_STANDARD (unmapped console)\n");
                break;
            case CONU_CHARMAP:
                unit->unit_mode = CONU_CHARMAP;
                unit->use_keymap = TRUE;
                /* Copy keymap from cu_KeyMapStruct if provided, otherwise use default */
                LPRINTF(LOG_INFO, "_console: Opened as CONU_CHARMAP (character map console)\n");
                break;
            case CONU_SNIPMAP:
                unit->unit_mode = CONU_SNIPMAP;
                unit->use_keymap = TRUE;
                LPRINTF(LOG_INFO, "_console: Opened as CONU_SNIPMAP (snipmap console)\n");
                break;
            default:
                /* Unknown unit number, default to CONU_STANDARD */
                unit->unit_mode = CONU_STANDARD;
                unit->use_keymap = FALSE;
                LPRINTF(LOG_WARNING, "_console: Unknown unitn %ld, defaulting to CONU_STANDARD\n", unitn);
                break;
        }
        
        /* Ensure the window has IDCMP_RAWKEY flag set so we receive keyboard events */
        if (!(window->IDCMPFlags & IDCMP_RAWKEY_FLAG)) {
            LPRINTF(LOG_INFO, "_console: Adding IDCMP_RAWKEY to window flags 0x%08lx\n",
                    (ULONG)window->IDCMPFlags);
            /* Use ModifyIDCMP to add the flag - this also ensures UserPort exists */
            struct IntuitionBase *IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
            if (IntuitionBase) {
                ModifyIDCMP(window, window->IDCMPFlags | IDCMP_RAWKEY_FLAG);
                CloseLibrary((struct Library *)IntuitionBase);
            }
        }
        
        LPRINTF(LOG_INFO, "_console: Window now has IDCMPFlags=0x%08lx, UserPort=0x%08lx\n",
                (ULONG)window->IDCMPFlags, (ULONG)window->UserPort);
        
        /* Register unit in tracking array for VBlank hook (Phase 45: async I/O) */
        for (i = 0; i < MAX_CONSOLE_UNITS; i++) {
            if (cdb->cdb_Units[i] == NULL) {
                cdb->cdb_Units[i] = unit;
                cdb->cdb_NumUnits++;
                DPRINTF(LOG_DEBUG, "_console: Registered unit 0x%08lx in slot %d\n", (ULONG)unit, i);
                break;
            }
        }
        if (i >= MAX_CONSOLE_UNITS) {
            LPRINTF(LOG_WARNING, "_console: No free slots for unit tracking (max %d)\n", MAX_CONSOLE_UNITS);
        }
    }
    
    /* Accept the open request */
    ioreq->io_Error = 0;
    ioreq->io_Device = (struct Device *)dev;
    ioreq->io_Unit = (struct Unit *)unit;  /* Store our ConUnit */
    
    /* Mark device as open */
    dev->lib_OpenCnt++;
    dev->lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_console_Close( register struct Library   *dev   __asm("a6"),
                                          register struct IORequest *ioreq __asm("a1"))
{
    struct ConsoleDevBase *cdb = (struct ConsoleDevBase *)dev;
    struct LxaConUnit *unit = (struct LxaConUnit *)ioreq->io_Unit;
    int i;
    
    DPRINTF (LOG_DEBUG, "_console: Close() called, unit=0x%08lx\n", (ULONG)unit);
    
    if (unit) {
        /* Unregister unit from tracking array (Phase 45: async I/O) */
        for (i = 0; i < MAX_CONSOLE_UNITS; i++) {
            if (cdb->cdb_Units[i] == unit) {
                cdb->cdb_Units[i] = NULL;
                cdb->cdb_NumUnits--;
                DPRINTF(LOG_DEBUG, "_console: Unregistered unit 0x%08lx from slot %d\n", (ULONG)unit, i);
                break;
            }
        }
        
        console_destroy_unit(unit);
    }
    
    dev->lib_OpenCnt--;
    return 0;
}

static BPTR __g_lxa_console_Expunge ( register struct Library   *dev   __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_console: Expunge() called - not expunging ROM device\n");
    return 0;
}

static BPTR __g_lxa_console_BeginIO ( register struct Library   *dev   __asm("a6"),
                                             register struct IORequest *ioreq __asm("a1"))
{
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    struct LxaConUnit *unit = (struct LxaConUnit *)ioreq->io_Unit;
    
    DPRINTF (LOG_DEBUG, "_console: BeginIO() cmd=%d, len=%ld, data=0x%08lx, unit=0x%08lx\n", 
             ioreq->io_Command, iostd->io_Length, (ULONG)iostd->io_Data, (ULONG)unit);
    
    ioreq->io_Error = 0;
    iostd->io_Actual = 0;
    
    switch (ioreq->io_Command)
    {
        case CMD_READ:
            DPRINTF (LOG_DEBUG, "_console: CMD_READ len=%ld line_mode=%d quick=%d\n", 
                     iostd->io_Length, unit ? unit->line_mode : -1,
                     (ioreq->io_Flags & IOF_QUICK) ? 1 : 0);
            
            if (unit && unit->cu.cu_Window) {
                /* Read from window's IDCMP keyboard input */
                char *data = (char *)iostd->io_Data;
                LONG len = iostd->io_Length;
                LONG count = 0;
                BOOL data_available = FALSE;
                
                /* Record line start position for backspace limits.
                 * This is where the user's input starts - they cannot backspace
                 * past this point. This matches authentic Amiga behavior where
                 * you cannot backspace over program-output text. */
                unit->line_start_x = unit->cu.cu_XCP;
                unit->line_start_y = unit->cu.cu_YCP;
                
                /* Draw cursor to show we're ready for input */
                console_draw_cursor(unit);
                
                /* Process any pending IDCMP input first */
                console_process_input(unit);
                
                /* Check if we have enough data to satisfy the request immediately */
                if (unit->line_mode) {
                    /* In line mode, need a complete line OR existing buffer data */
                    data_available = input_has_line(unit) || !input_buf_empty(unit);
                } else {
                    /* In raw mode, any data is enough */
                    data_available = !input_buf_empty(unit);
                }
                
                if (data_available) {
                    /* Data is available - complete immediately */
                    console_hide_cursor(unit);
                    
                    /* Copy available characters to buffer */
                    while (count < len && !input_buf_empty(unit)) {
                        char c = input_buf_get(unit);
                        data[count++] = c;
                        
                        /* In line mode, stop at newline */
                        if (unit->line_mode && (c == '\r' || c == '\n')) {
                            break;
                        }
                    }
                    
                    iostd->io_Actual = count;
                    ioreq->io_Error = 0;
                    DPRINTF(LOG_DEBUG, "_console: CMD_READ completed immediately, %ld bytes\n", count);
                } else {
                    /* No data available yet - queue as async request */
                    DPRINTF(LOG_DEBUG, "_console: CMD_READ queuing async read\n");
                    
                    /* Only one pending read at a time per unit */
                    if (unit->pending_read != NULL) {
                        LPRINTF(LOG_WARNING, "_console: CMD_READ already has pending read, aborting previous\n");
                        /* Complete previous with 0 bytes */
                        unit->pending_read->io_Actual = 0;
                        unit->pending_read->io_Error = 0;
                        ReplyMsg(&unit->pending_read->io_Message);
                    }
                    
                    unit->pending_read = iostd;
                    
                    /* Clear IOF_QUICK to indicate we're going async */
                    ioreq->io_Flags &= ~IOF_QUICK;
                    
                    /* Don't reply yet - will be completed when input arrives */
                    return 0;
                }
            } else {
                /* No window - fall back to host emucall */
                LONG result = emucall2(EMU_CALL_CON_READ, (ULONG)iostd->io_Data, iostd->io_Length);
                if (result >= 0)
                {
                    iostd->io_Actual = result;
                    ioreq->io_Error = 0;
                }
                else
                {
                    /* No input available yet */
                    iostd->io_Actual = 0;
                    ioreq->io_Error = 0;
                }
            }
            break;
            
        case CMD_WRITE:
            DPRINTF (LOG_DEBUG, "_console: CMD_WRITE len=%ld\n", iostd->io_Length);
            
            if (unit && unit->cu.cu_Window) {
                /* Write to the window */
                char *data = (char *)iostd->io_Data;
                LONG len = iostd->io_Length;
                
                /* Handle io_Length == -1: null-terminated string */
                if (len == -1) {
                    len = strlen(data);
                }
                
                /* Process each character */
                for (LONG i = 0; i < len; i++) {
                    console_process_char(unit, data[i]);
                }
                
                /* Trigger display refresh via WaitTOF */
                WaitTOF();
                
                iostd->io_Actual = len;
            } else {
                /* No window - just log to debug output */
                char *data = (char *)iostd->io_Data;
                LONG len = iostd->io_Length;
                
                /* Handle io_Length == -1: null-terminated string */
                if (len == -1) {
                    len = strlen(data);
                }
                
                LPRINTF(LOG_INFO, "_console: WRITE (no window): '");
                for (LONG i = 0; i < len && i < 256; i++) {
                    char c = data[i];
                    if (c >= 32 && c < 127) {
                        lputc(LOG_INFO, c);
                    } else if (c == '\n') {
                        lputs(LOG_INFO, "\\n");
                    } else if (c == '\r') {
                        lputs(LOG_INFO, "\\r");
                    } else {
                        LPRINTF(LOG_INFO, "\\x%02x", (unsigned char)c);
                    }
                }
                LPRINTF(LOG_INFO, "'\n");
                iostd->io_Actual = len;
            }
            break;
            
        case CMD_CLEAR:
            /* Clear the input buffer (NOT the display!) */
            DPRINTF(LOG_DEBUG, "_console: CMD_CLEAR (clearing input buffer)\n");
            if (unit) {
                /* Reset the input ring buffer */
                unit->input_head = 0;
                unit->input_tail = 0;
            }
            break;
            
        case CD_ASKKEYMAP:
        case CD_ASKDEFAULTKEYMAP:
            /* Return the current/default keymap */
            /* Both commands return the same keymap since we don't support per-unit keymaps yet */
            DPRINTF(LOG_DEBUG, "_console: CD_ASKKEYMAP/CD_ASKDEFAULTKEYMAP io_Data=0x%08lx io_Length=%ld\n",
                    (ULONG)iostd->io_Data, (LONG)iostd->io_Length);
            if (iostd->io_Data && iostd->io_Length >= (LONG)sizeof(struct KeyMap)) {
                struct Library *KeymapBase = OpenLibrary((STRPTR)"keymap.library", 0);
                if (KeymapBase) {
                    /* Call AskKeyMapDefault from keymap.library (LVO -36) */
                    register struct KeyMap *_result __asm("d0");
                    register struct Library *_a6 __asm("a6") = KeymapBase;
                    __asm volatile (
                        "jsr %1@(-36)"
                        : "=r" (_result)
                        : "a" (_a6)
                        : "cc", "memory"
                    );
                    if (_result) {
                        /* Copy the KeyMap structure to user's buffer */
                        struct KeyMap *destMap = (struct KeyMap *)iostd->io_Data;
                        destMap->km_LoKeyMapTypes = _result->km_LoKeyMapTypes;
                        destMap->km_LoKeyMap = _result->km_LoKeyMap;
                        destMap->km_LoCapsable = _result->km_LoCapsable;
                        destMap->km_LoRepeatable = _result->km_LoRepeatable;
                        destMap->km_HiKeyMapTypes = _result->km_HiKeyMapTypes;
                        destMap->km_HiKeyMap = _result->km_HiKeyMap;
                        destMap->km_HiCapsable = _result->km_HiCapsable;
                        destMap->km_HiRepeatable = _result->km_HiRepeatable;
                        ioreq->io_Error = 0;
                        iostd->io_Actual = sizeof(struct KeyMap);
                        DPRINTF(LOG_DEBUG, "_console: CD_ASKKEYMAP returned keymap at 0x%08lx\n", (ULONG)_result);
                    } else {
                        LPRINTF(LOG_WARNING, "_console: CD_ASKKEYMAP - AskKeyMapDefault returned NULL\n");
                        ioreq->io_Error = IOERR_OPENFAIL;
                    }
                    CloseLibrary(KeymapBase);
                } else {
                    LPRINTF(LOG_WARNING, "_console: CD_ASKKEYMAP - couldn't open keymap.library\n");
                    ioreq->io_Error = IOERR_OPENFAIL;
                }
            } else {
                DPRINTF(LOG_DEBUG, "_console: CD_ASKKEYMAP - invalid buffer\n");
                ioreq->io_Error = IOERR_BADLENGTH;
            }
            break;
            
        case CD_SETKEYMAP:
            /* Set the keymap for this unit */
            /* For now, just accept and ignore - we use the default system keymap */
            DPRINTF(LOG_DEBUG, "_console: CD_SETKEYMAP\n");
            ioreq->io_Error = 0;
            break;
            
        case CD_SETDEFAULTKEYMAP:
            /* Set the default keymap */
            /* For now, just accept and ignore */
            DPRINTF(LOG_DEBUG, "_console: CD_SETDEFAULTKEYMAP\n");
            ioreq->io_Error = 0;
            break;
            
        default:
            /* Unknown or unimplemented command */
            DPRINTF (LOG_DEBUG, "_console: BeginIO unknown cmd=%d\n", ioreq->io_Command);
            ioreq->io_Error = 0;
            break;
    }
    
    /* If quick I/O was requested and we completed, return immediately */
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        /* Need to reply to the message */
        ReplyMsg(&ioreq->io_Message);
    }
    
    return 0;
}

static ULONG __g_lxa_console_AbortIO ( register struct Library   *dev   __asm("a6"),
                                              register struct IORequest *ioreq __asm("a1"))
{
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    struct LxaConUnit *unit = (struct LxaConUnit *)ioreq->io_Unit;
    
    DPRINTF (LOG_DEBUG, "_console: AbortIO() called, ioreq=0x%08lx, unit=0x%08lx\n", (ULONG)ioreq, (ULONG)unit);
    
    /* Check if this is a pending async read we can abort */
    if (unit && unit->pending_read == iostd) {
        DPRINTF(LOG_DEBUG, "_console: AbortIO() aborting pending read\n");
        
        unit->pending_read = NULL;
        
        /* Hide cursor */
        console_hide_cursor(unit);
        
        /* Mark as aborted */
        ioreq->io_Error = IOERR_ABORTED;
        iostd->io_Actual = 0;
        
        /* Complete the request */
        ReplyMsg(&ioreq->io_Message);
        
        return 0;  /* Success */
    }
    
    return IOERR_NOCMD;  /* Not a pending request we know about */
}

/*
 * CDInputHandler - console input handler for input.device
 * This function is called by input.device to process input events.
 * For now, we just pass through the events unmodified.
 */
static struct InputEvent *__g_lxa_console_CDInputHandler (
    register struct Library       *dev    __asm("a6"),
    register struct InputEvent    *events __asm("a0"),
    register struct Library       *cdihdata __asm("a1"))
{
    DPRINTF(LOG_DEBUG, "_console: CDInputHandler() called\n");
    /* For now, just pass through events unmodified */
    /* In a full implementation, this would handle console-specific input processing */
    return events;
}

/*
 * RawKeyConvert - convert raw key codes to ASCII
 * This is essentially a wrapper around keymap.library's MapRawKey()
 */
static LONG __g_lxa_console_RawKeyConvert (
    register struct Library       *dev    __asm("a6"),
    register struct InputEvent    *events __asm("a0"),
    register STRPTR               buffer  __asm("a1"),
    register LONG                 length  __asm("d1"),
    register struct KeyMap        *keyMap __asm("a2"))
{
    struct Library *KeymapBase;
    LONG result = 0;
    
    DPRINTF(LOG_DEBUG, "_console: RawKeyConvert() called, length=%ld\n", length);
    
    /* Open keymap.library */
    KeymapBase = OpenLibrary((STRPTR)"keymap.library", 0);
    if (KeymapBase) {
        /* Call MapRawKey from keymap.library 
         * MapRawKey is at LVO -42 (offset -42 from library base) 
         * Signature: WORD MapRawKey(struct InputEvent *event, STRPTR buffer, LONG length, struct KeyMap *keyMap)
         * Registers: A0=event, A1=buffer, D1=length, A2=keyMap, A6=KeymapBase
         */
        register LONG _result __asm("d0");
        register struct Library *_a6 __asm("a6") = KeymapBase;
        register struct InputEvent *_a0 __asm("a0") = events;
        register STRPTR _a1 __asm("a1") = buffer;
        register LONG _d1 __asm("d1") = length;
        register struct KeyMap *_a2 __asm("a2") = keyMap;
        
        __asm volatile (
            "jsr %1@(-42)"
            : "=r" (_result)
            : "a" (_a6), "r" (_a0), "r" (_a1), "r" (_d1), "r" (_a2)
            : "cc", "memory"
        );
        
        result = _result;
        CloseLibrary(KeymapBase);
    } else {
        LPRINTF(LOG_WARNING, "_console: RawKeyConvert() - couldn't open keymap.library\n");
    }
    
    DPRINTF(LOG_DEBUG, "_console: RawKeyConvert() returning %ld\n", result);
    return result;
}

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

extern APTR              __g_lxa_console_FuncTab [];
extern struct MyDataInit __g_lxa_console_DataTab;
extern struct InitTable  __g_lxa_console_InitTab;
extern APTR              __g_lxa_console_EndResident;

static struct Resident __aligned ROMTag =
{                               //                               offset
    RTC_MATCHWORD,              // UWORD rt_MatchWord;                0
    &ROMTag,                    // struct Resident *rt_MatchTag;      2
    &__g_lxa_console_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,               // UBYTE rt_Flags;                    7
    VERSION,                    // UBYTE rt_Version;                  8
    NT_DEVICE,                  // UBYTE rt_Type;                     9
    0,                          // BYTE  rt_Pri;                     10
    &_g_console_ExDevName[0],     // char  *rt_Name;                   14
    &_g_console_ExDevID[0],       // char  *rt_IdString;               18
    &__g_lxa_console_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_console_EndResident;
struct Resident *__lxa_console_ROMTag = &ROMTag;

struct InitTable __g_lxa_console_InitTab =
{
    (ULONG)               sizeof(struct ConsoleDevBase),  // LibBaseSize (extended for Phase 45)
    (APTR              *) &__g_lxa_console_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_console_DataTab,     // DataTable
    (APTR)                __g_lxa_console_InitDev       // InitLibFn
};

APTR __g_lxa_console_FuncTab [] =
{
    __g_lxa_console_Open,             // -6  (LVO_Open)
    __g_lxa_console_Close,            // -12 (LVO_Close)
    __g_lxa_console_Expunge,          // -18 (LVO_Expunge)
    NULL,                             // -24 (Reserved)
    __g_lxa_console_BeginIO,          // -30 (LVO_BeginIO)
    __g_lxa_console_AbortIO,          // -36 (LVO_AbortIO)
    __g_lxa_console_CDInputHandler,   // -42 (CDInputHandler, bias=42)
    __g_lxa_console_RawKeyConvert,    // -48 (RawKeyConvert, bias=42+6)
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_console_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_console_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_console_ExDevID[0],
    (ULONG) 0
};
