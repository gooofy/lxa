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

#define VERSION    40
#define REVISION   2
#define EXDEVNAME  "console"
#define EXDEVVER   " 40.2 (2025/01/15)"

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
static void console_newline(struct LxaConUnit *unit);
static void console_carriage_return(struct LxaConUnit *unit);
static void console_process_char(struct LxaConUnit *unit, char c);

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
 * Process keyboard input from IDCMP
 * Called when reading from console to check for new input
 */
static void console_process_input(struct LxaConUnit *unit)
{
    struct Window *window;
    struct MsgPort *port;
    struct IntuiMessage *imsg;
    
    if (!unit || !unit->cu.cu_Window) {
        return;
    }
    
    window = unit->cu.cu_Window;
    port = window->UserPort;
    
    if (!port) {
        return;
    }
    
    /* Process all pending IDCMP messages */
    while ((imsg = (struct IntuiMessage *)GetMsg(port)) != NULL) {
        if (imsg->Class == IDCMP_RAWKEY) {
            UWORD rawkey = imsg->Code;
            UWORD qualifier = imsg->Qualifier;
            
            /* Only process key-down events */
            if (!(rawkey & 0x80)) {
                char c = rawkey_to_ascii(rawkey, qualifier);
                
                DPRINTF(LOG_DEBUG, "_console: RAWKEY 0x%02x qual=0x%04x -> '%c' (0x%02x)\n",
                        rawkey, qualifier, (c >= 32 && c < 127) ? c : '.', (unsigned char)c);
                
                if (c) {
                    /* Handle special characters */
                    if (c == '\b') {
                        /* Backspace - remove last character from buffer if possible */
                        if (!input_buf_empty(unit)) {
                            /* Check if we can remove (not past a newline) */
                            UWORD prev = (unit->input_head + INPUT_BUF_LEN - 1) % INPUT_BUF_LEN;
                            if (prev != unit->input_tail && 
                                unit->input_buf[prev] != '\r' && 
                                unit->input_buf[prev] != '\n') {
                                unit->input_head = prev;
                                if (unit->echo_enabled) {
                                    /* Echo backspace: move cursor back, space, move back again */
                                    console_process_char(unit, '\b');
                                    console_process_char(unit, ' ');
                                    console_process_char(unit, '\b');
                                }
                            }
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
        
        /* Reply to the message (free it) */
        ReplyMsg((struct Message *)imsg);
    }
    
    /* Refresh display if we echoed anything */
    if (unit->echo_enabled) {
        WaitTOF();
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
    
    /* Source rectangle (everything except top line) */
    x1 = unit->cu.cu_XROrigin;
    y1 = unit->cu.cu_YROrigin;
    x2 = unit->cu.cu_XRExtant;
    y2 = unit->cu.cu_YRExtant;
    
    /* Use ScrollRaster to scroll content up */
    /* ScrollRaster(rp, dx, dy, x1, y1, x2, y2) moves content by (dx, dy) pixels */
    /* We want to move up by one character height */
    ScrollRaster(rp, 0, unit->cu.cu_YRSize, x1, y1, x2, y2);
    
    DPRINTF(LOG_DEBUG, "_console: scroll_up() scrolled by %d pixels\n", unit->cu.cu_YRSize);
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
        
        case 'J':  /* Erase in Display (ED) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            if (mode == 2) {
                /* Clear entire display */
                console_clear_display(unit);
            }
            /* mode 0 = clear from cursor to end, mode 1 = clear from start to cursor */
            /* TODO: implement modes 0 and 1 */
            break;
        }
        
        case 'K':  /* Erase in Line (EL) */
        {
            int mode = (nparams >= 1) ? params[0] : 0;
            if (mode == 0) {
                /* Clear from cursor to end of line */
                console_clear_to_eol(unit);
            }
            /* mode 1 = clear from start to cursor, mode 2 = clear entire line */
            /* TODO: implement modes 1 and 2 */
            break;
        }
        
        case 'm':  /* Select Graphic Rendition (SGR) */
        {
            for (int i = 0; i < nparams || nparams == 0; i++) {
                int p = (nparams > 0) ? params[i] : 0;
                switch (p) {
                    case 0:   /* Reset */
                        unit->cu.cu_FgPen = 1;
                        unit->cu.cu_BgPen = 0;
                        unit->cu.cu_DrawMode = JAM2;
                        break;
                    case 1:   /* Bold - use brighter color */
                        /* For Amiga, we could use a different pen */
                        break;
                    case 7:   /* Inverse */
                        {
                            BYTE tmp = unit->cu.cu_FgPen;
                            unit->cu.cu_FgPen = unit->cu.cu_BgPen;
                            unit->cu.cu_BgPen = tmp;
                        }
                        break;
                    case 30: case 31: case 32: case 33:
                    case 34: case 35: case 36: case 37:
                        /* Foreground colors 0-7 */
                        unit->cu.cu_FgPen = p - 30;
                        break;
                    case 39:  /* Default foreground */
                        unit->cu.cu_FgPen = 1;
                        break;
                    case 40: case 41: case 42: case 43:
                    case 44: case 45: case 46: case 47:
                        /* Background colors 0-7 */
                        unit->cu.cu_BgPen = p - 40;
                        break;
                    case 49:  /* Default background */
                        unit->cu.cu_BgPen = 0;
                        break;
                }
                if (nparams == 0) break;  /* Exit after handling reset with no params */
            }
            break;
        }
        
        case 'p':  /* Cursor visibility (Amiga-specific) */
        {
            /* CSI 0 p = hide cursor, CSI p or CSI 1 p = show cursor */
            /* We don't have cursor rendering yet, so just ignore */
            break;
        }
        
        case '{':  /* Set scrolling region (Amiga-specific) - CSI n { */
        {
            /* This sets how many lines to use for the scrolling region */
            /* For now, just ignore it */
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
            case '\n':  /* Line Feed */
                console_newline(unit);
                break;
            case '\r':  /* Carriage Return */
                console_carriage_return(unit);
                break;
            case '\t':  /* Tab */
                /* Move to next tab stop (every 8 characters) */
                unit->cu.cu_XCP = ((unit->cu.cu_XCP / 8) + 1) * 8;
                if (unit->cu.cu_XCP > unit->cu.cu_XMax) {
                    console_newline(unit);
                }
                break;
            case '\b':  /* Backspace */
                if (unit->cu.cu_XCP > 0) {
                    unit->cu.cu_XCP--;
                }
                break;
            case '\a':  /* Bell */
                /* TODO: Could trigger a visual or audio bell */
                break;
            case '\f':  /* Form Feed - clear screen */
                console_clear_display(unit);
                break;
            default:
                if (uc >= 32 && uc != 127) {
                    /* Printable character (32-126 and 128-255) */
                    console_write_char(unit, c);
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
    DPRINTF (LOG_DEBUG, "_console: InitDev() called.\n");
    return dev;
}

/* IDCMP_RAWKEY constant - 1<<10 = 0x400 */
#define IDCMP_RAWKEY_FLAG 0x0400

static void __g_lxa_console_Open ( register struct Library   *dev   __asm("a6"),
                                            register struct IORequest *ioreq __asm("a1"),
                                            register ULONG            unitn  __asm("d0"),
                                            register ULONG            flags  __asm("d1"))
{
    struct IOStdReq *iostd = (struct IOStdReq *)ioreq;
    struct Window *window = (struct Window *)iostd->io_Data;
    struct LxaConUnit *unit = NULL;
    
    LPRINTF (LOG_INFO, "_console: Open() called, unitn=%ld, flags=0x%08lx, io_Data(Window)=0x%08lx\n", 
             unitn, flags, (ULONG)window);
    
    /* Create a ConUnit for this console if a window was provided */
    if (window != NULL) {
        unit = console_create_unit(window);
        if (!unit) {
            ioreq->io_Error = IOERR_OPENFAIL;
            return;
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
    struct LxaConUnit *unit = (struct LxaConUnit *)ioreq->io_Unit;
    
    DPRINTF (LOG_DEBUG, "_console: Close() called, unit=0x%08lx\n", (ULONG)unit);
    
    if (unit) {
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
            DPRINTF (LOG_DEBUG, "_console: CMD_READ len=%ld\n", iostd->io_Length);
            
            if (unit && unit->cu.cu_Window) {
                /* Read from window's IDCMP keyboard input */
                char *data = (char *)iostd->io_Data;
                LONG len = iostd->io_Length;
                LONG count = 0;
                
                /* Process any pending input */
                console_process_input(unit);
                
                /* In line mode, wait for a complete line */
                if (unit->line_mode) {
                    while (!input_has_line(unit) && !input_buf_full(unit)) {
                        /* Wait for more input - yield to scheduler */
                        WaitTOF();
                        console_process_input(unit);
                    }
                }
                
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
                
                DPRINTF(LOG_DEBUG, "_console: CMD_READ returned %ld bytes\n", count);
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
    DPRINTF (LOG_DEBUG, "_console: AbortIO() called\n");
    return 0;  /* No pending I/O to abort */
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
    (ULONG)               sizeof(struct Library),     // LibBaseSize
    (APTR              *) &__g_lxa_console_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_console_DataTab,     // DataTable
    (APTR)                __g_lxa_console_InitDev       // InitLibFn
};

APTR __g_lxa_console_FuncTab [] =
{
    __g_lxa_console_Open,
    __g_lxa_console_Close,
    __g_lxa_console_Expunge,
    NULL,
    __g_lxa_console_BeginIO,
    __g_lxa_console_AbortIO,
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
