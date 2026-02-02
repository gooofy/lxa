/*
 * test_inject.h - UI Test Infrastructure Helpers
 *
 * Phase 21: UI Testing Infrastructure
 *
 * This header provides functions for injecting input events and
 * capturing screen output from m68k test programs.
 */

#ifndef TEST_INJECT_H
#define TEST_INJECT_H

#include <exec/types.h>

/*
 * Emucall definitions for test infrastructure (4100-4199 range)
 */
#define EMU_CALL_TEST_INJECT_KEY     4100
#define EMU_CALL_TEST_INJECT_STRING  4101
#define EMU_CALL_TEST_INJECT_MOUSE   4102
#define EMU_CALL_TEST_CAPTURE_SCREEN 4110
#define EMU_CALL_TEST_CAPTURE_WINDOW 4111
#define EMU_CALL_TEST_COMPARE_SCREEN 4112
#define EMU_CALL_TEST_SET_HEADLESS   4120
#define EMU_CALL_TEST_GET_HEADLESS   4121
#define EMU_CALL_TEST_WAIT_IDLE      4122

/*
 * Display event types for mouse injection
 * Note: These must match display_event_type_t in display.h
 */
#define DISPLAY_EVENT_MOUSEBUTTON 1
#define DISPLAY_EVENT_MOUSEMOVE   2

/*
 * Amiga rawkey codes (from devices/rawkeycodes.h style)
 */
#define RAWKEY_BACKSPACE    0x41
#define RAWKEY_TAB          0x42
#define RAWKEY_RETURN       0x44
#define RAWKEY_ESC          0x45
#define RAWKEY_DEL          0x46
#define RAWKEY_SPACE        0x40
#define RAWKEY_UP           0x4C
#define RAWKEY_DOWN         0x4D
#define RAWKEY_RIGHT        0x4E
#define RAWKEY_LEFT         0x4F

/* Letter keys (unshifted = lowercase, shifted = uppercase) */
#define RAWKEY_A            0x20
#define RAWKEY_B            0x35
#define RAWKEY_C            0x33
#define RAWKEY_D            0x22
#define RAWKEY_E            0x12
#define RAWKEY_F            0x23
#define RAWKEY_G            0x24
#define RAWKEY_H            0x25
#define RAWKEY_I            0x17
#define RAWKEY_J            0x26
#define RAWKEY_K            0x27
#define RAWKEY_L            0x28
#define RAWKEY_M            0x37
#define RAWKEY_N            0x36
#define RAWKEY_O            0x18
#define RAWKEY_P            0x19
#define RAWKEY_Q            0x10
#define RAWKEY_R            0x13
#define RAWKEY_S            0x21
#define RAWKEY_T            0x14
#define RAWKEY_U            0x16
#define RAWKEY_V            0x34
#define RAWKEY_W            0x11
#define RAWKEY_X            0x32
#define RAWKEY_Y            0x15
#define RAWKEY_Z            0x31

/* Number keys (top row) */
#define RAWKEY_0            0x0A
#define RAWKEY_1            0x01
#define RAWKEY_2            0x02
#define RAWKEY_3            0x03
#define RAWKEY_4            0x04
#define RAWKEY_5            0x05
#define RAWKEY_6            0x06
#define RAWKEY_7            0x07
#define RAWKEY_8            0x08
#define RAWKEY_9            0x09

/*
 * Qualifier bits (from devices/inputevent.h)
 */
#define IEQUALIFIER_LSHIFT      0x0001
#define IEQUALIFIER_RSHIFT      0x0002
#define IEQUALIFIER_CAPSLOCK    0x0004
#define IEQUALIFIER_CONTROL     0x0008
#define IEQUALIFIER_LALT        0x0010
#define IEQUALIFIER_RALT        0x0020
#define IEQUALIFIER_LCOMMAND    0x0040
#define IEQUALIFIER_RCOMMAND    0x0080

/*
 * Mouse button masks
 */
#define MOUSE_LEFTBUTTON   0x01
#define MOUSE_RIGHTBUTTON  0x02
#define MOUSE_MIDDLEBUTTON 0x04

/*
 * Inject a keyboard event into the event queue.
 *
 * @param rawkey     Amiga raw key code
 * @param qualifier  Qualifier bits (shift, ctrl, etc.)
 * @param down       TRUE for key press, FALSE for key release
 * @return TRUE on success
 */
static inline BOOL test_inject_key(UWORD rawkey, UWORD qualifier, BOOL down)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "move.l %3, d2\n"
        "move.l %4, d3\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_INJECT_KEY),
          "r" ((ULONG)rawkey),
          "r" ((ULONG)qualifier),
          "r" ((ULONG)down)
        : "d0", "d1", "d2", "d3"
    );
    return (BOOL)(result != 0);
}

/*
 * Inject a string as a sequence of key events.
 * Each character is converted to appropriate rawkey codes with
 * both key-down and key-up events.
 *
 * @param str  String to inject (ASCII characters)
 * @return TRUE on success
 */
static inline BOOL test_inject_string(const char *str)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, a0\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_INJECT_STRING),
          "r" (str)
        : "d0", "a0"
    );
    return (BOOL)(result != 0);
}

/*
 * Inject a mouse event into the event queue.
 *
 * @param x           Mouse X position
 * @param y           Mouse Y position
 * @param buttons     Button state (use MOUSE_* masks)
 * @param event_type  DISPLAY_EVENT_MOUSEMOVE or DISPLAY_EVENT_MOUSEBUTTON
 * @return TRUE on success
 */
static inline BOOL test_inject_mouse(WORD x, WORD y, UWORD buttons, UWORD event_type)
{
    ULONG result;
    ULONG pos = ((ULONG)(UWORD)x << 16) | (ULONG)(UWORD)y;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "move.l %3, d2\n"
        "move.l %4, d3\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_INJECT_MOUSE),
          "r" (pos),
          "r" ((ULONG)buttons),
          "r" ((ULONG)event_type)
        : "d0", "d1", "d2", "d3"
    );
    return (BOOL)(result != 0);
}

/*
 * Capture the current screen to a PPM file.
 *
 * @param filename  Output filename (host filesystem path)
 * @return TRUE on success
 */
static inline BOOL test_capture_screen(const char *filename)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, a0\n"
        "moveq  #0, d1\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_CAPTURE_SCREEN),
          "r" (filename)
        : "d0", "d1", "a0"
    );
    return (BOOL)(result != 0);
}

/*
 * Enable or disable headless mode.
 *
 * @param enable  TRUE to enable headless mode
 * @return Previous headless state
 */
static inline BOOL test_set_headless(BOOL enable)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_SET_HEADLESS),
          "r" ((ULONG)enable)
        : "d0", "d1"
    );
    return (BOOL)(result != 0);
}

/*
 * Check if headless mode is enabled.
 *
 * @return TRUE if headless mode is active
 */
static inline BOOL test_get_headless(void)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_GET_HEADLESS)
        : "d0"
    );
    return (BOOL)(result != 0);
}

/*
 * Wait for the event queue to be empty.
 *
 * @param timeout_ms  Timeout in milliseconds (0 = just check, don't wait)
 * @return TRUE if queue is empty, FALSE if timeout
 */
static inline BOOL test_wait_idle(ULONG timeout_ms)
{
    ULONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_WAIT_IDLE),
          "r" (timeout_ms)
        : "d0", "d1"
    );
    return (BOOL)(result != 0);
}

/*
 * Helper: Inject a key press and release.
 */
static inline BOOL test_inject_keypress(UWORD rawkey, UWORD qualifier)
{
    if (!test_inject_key(rawkey, qualifier, TRUE))
        return FALSE;
    return test_inject_key(rawkey, qualifier, FALSE);
}

/*
 * Helper: Inject Return key press.
 */
static inline BOOL test_inject_return(void)
{
    return test_inject_keypress(RAWKEY_RETURN, 0);
}

/*
 * Helper: Inject Backspace key press.
 */
static inline BOOL test_inject_backspace(void)
{
    return test_inject_keypress(RAWKEY_BACKSPACE, 0);
}

#endif /* TEST_INJECT_H */
