/*
 * test_inject.h - UI Test Infrastructure Helpers
 *
 * Phase 21: UI Testing Infrastructure
 * Phase 39b: Enhanced Application Testing Infrastructure
 *
 * This header provides functions for injecting input events and
 * capturing screen output from m68k test programs, as well as
 * validation functions for testing application rendering.
 */

#ifndef TEST_INJECT_H
#define TEST_INJECT_H

#include <exec/types.h>

/*
 * Emucall definitions for test infrastructure (4100-4199 range)
 */
#define EMU_CALL_TEST_INJECT_KEY        4100
#define EMU_CALL_TEST_INJECT_STRING     4101
#define EMU_CALL_TEST_INJECT_MOUSE      4102
#define EMU_CALL_TEST_CAPTURE_SCREEN    4110
#define EMU_CALL_TEST_CAPTURE_WINDOW    4111
#define EMU_CALL_TEST_COMPARE_SCREEN    4112
/* Phase 39b: Validation emucalls */
#define EMU_CALL_TEST_GET_SCREEN_DIMS   4113
#define EMU_CALL_TEST_GET_CONTENT       4114
#define EMU_CALL_TEST_GET_REGION        4115
#define EMU_CALL_TEST_GET_WIN_COUNT     4116
#define EMU_CALL_TEST_GET_WIN_DIMS      4117
#define EMU_CALL_TEST_GET_WIN_CONTENT   4118
#define EMU_CALL_TEST_SET_HEADLESS      4120
#define EMU_CALL_TEST_GET_HEADLESS      4121
#define EMU_CALL_TEST_WAIT_IDLE         4122

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

/*
 * ============================================================================
 * Phase 39b: Validation Functions
 * ============================================================================
 */

/*
 * Get the dimensions of the active screen/display.
 *
 * @param width   Output: screen width (can be NULL)
 * @param height  Output: screen height (can be NULL)
 * @param depth   Output: screen depth (can be NULL)
 * @return TRUE if screen is active, FALSE otherwise
 */
static inline BOOL test_get_screen_dimensions(WORD *width, WORD *height, WORD *depth)
{
    ULONG result;
    ULONG dims_out;
    __asm volatile (
        "move.l %2, d0\n"
        "illegal\n"
        "move.l d0, %0\n"
        "move.l d1, %1\n"
        : "=r" (result), "=r" (dims_out)
        : "i" (EMU_CALL_TEST_GET_SCREEN_DIMS)
        : "d0", "d1"
    );
    if (result)
    {
        /* dims_out: high word = width, low word = height */
        /* result: high word = depth (if >0) */
        if (width)  *width  = (WORD)((dims_out >> 16) & 0xFFFF);
        if (height) *height = (WORD)(dims_out & 0xFFFF);
        if (depth)  *depth  = (WORD)((result >> 16) & 0xFFFF);
    }
    return (BOOL)(result != 0);
}

/*
 * Get the number of non-background pixels on the active screen.
 * Useful for detecting if anything has been rendered.
 *
 * @return Number of non-background pixels, or -1 if no screen
 */
static inline LONG test_get_content_pixels(void)
{
    LONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_GET_CONTENT)
        : "d0"
    );
    return result;
}

/*
 * Get the number of non-background pixels in a screen region.
 *
 * @param x, y        Top-left corner of region
 * @param width       Width of region
 * @param height      Height of region
 * @return Number of non-background pixels in region, or -1 on error
 */
static inline LONG test_get_region_content(WORD x, WORD y, WORD width, WORD height)
{
    LONG result;
    ULONG pos = ((ULONG)(UWORD)x << 16) | (ULONG)(UWORD)y;
    ULONG dims = ((ULONG)(UWORD)width << 16) | (ULONG)(UWORD)height;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "move.l %3, d2\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_GET_REGION),
          "r" (pos),
          "r" (dims)
        : "d0", "d1", "d2"
    );
    return result;
}

/*
 * Get the number of open windows.
 *
 * @return Number of open windows
 */
static inline LONG test_get_window_count(void)
{
    LONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_GET_WIN_COUNT)
        : "d0"
    );
    return result;
}

/*
 * Get the dimensions of a window by index.
 *
 * @param index   Window index (0-based)
 * @param width   Output: window width (can be NULL)
 * @param height  Output: window height (can be NULL)
 * @return TRUE if window exists, FALSE otherwise
 */
static inline BOOL test_get_window_dimensions(LONG index, WORD *width, WORD *height)
{
    ULONG result;
    ULONG dims_out;
    __asm volatile (
        "move.l %2, d0\n"
        "move.l %3, d1\n"
        "illegal\n"
        "move.l d0, %0\n"
        "move.l d1, %1\n"
        : "=r" (result), "=r" (dims_out)
        : "i" (EMU_CALL_TEST_GET_WIN_DIMS),
          "r" ((ULONG)index)
        : "d0", "d1"
    );
    if (result)
    {
        if (width)  *width  = (WORD)((dims_out >> 16) & 0xFFFF);
        if (height) *height = (WORD)(dims_out & 0xFFFF);
    }
    return (BOOL)(result != 0);
}

/*
 * Get the number of non-background pixels in a window.
 *
 * @param index  Window index (0-based)
 * @return Number of non-background pixels, or -1 if window not found
 */
static inline LONG test_get_window_content(LONG index)
{
    LONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, d1\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_GET_WIN_CONTENT),
          "r" ((ULONG)index)
        : "d0", "d1"
    );
    return result;
}

/*
 * Compare the active screen to a reference PPM file.
 *
 * @param filename  Path to reference PPM file
 * @return Similarity percentage (0-100), or -1 on error
 */
static inline LONG test_compare_screen(const char *filename)
{
    LONG result;
    __asm volatile (
        "move.l %1, d0\n"
        "move.l %2, a0\n"
        "illegal\n"
        "move.l d0, %0\n"
        : "=r" (result)
        : "i" (EMU_CALL_TEST_COMPARE_SCREEN),
          "r" (filename)
        : "d0", "a0"
    );
    return result;
}

/*
 * Validation helper: Check if screen dimensions are valid.
 * Useful for detecting issues like the GFA Basic 21px screen bug.
 *
 * @param min_width   Minimum expected width
 * @param min_height  Minimum expected height
 * @return TRUE if dimensions meet requirements
 */
static inline BOOL test_validate_screen_size(WORD min_width, WORD min_height)
{
    WORD width, height, depth;
    if (!test_get_screen_dimensions(&width, &height, &depth))
        return FALSE;
    return (width >= min_width && height >= min_height);
}

/*
 * Validation helper: Check if screen has meaningful content.
 * Returns FALSE if screen appears empty (< threshold pixels).
 *
 * @param min_pixels  Minimum non-background pixels to consider "has content"
 * @return TRUE if screen has content above threshold
 */
static inline BOOL test_validate_screen_has_content(LONG min_pixels)
{
    LONG content = test_get_content_pixels();
    return (content >= min_pixels);
}

/*
 * ============================================================================
 * Phase 56+: Interactive Testing Helpers
 * ============================================================================
 */

/*
 * Helper: Perform a complete mouse click (press + release) at a position.
 * Waits for event processing between press and release.
 *
 * @param x, y       Screen coordinates to click
 * @param button     MOUSE_LEFTBUTTON, MOUSE_RIGHTBUTTON, or MOUSE_MIDDLEBUTTON
 * @return TRUE on success
 */
static inline BOOL test_mouse_click(WORD x, WORD y, UWORD button)
{
    /* Move to position first */
    if (!test_inject_mouse(x, y, 0, DISPLAY_EVENT_MOUSEMOVE))
        return FALSE;
    
    /* Wait for move to be processed */
    /* We use a simple delay since WaitTOF would require graphics.library */
    {
        volatile LONG i;
        for (i = 0; i < 10000; i++) ;  /* Short spin delay */
    }
    
    /* Press button */
    if (!test_inject_mouse(x, y, button, DISPLAY_EVENT_MOUSEBUTTON))
        return FALSE;
    
    /* Wait for press to be processed */
    {
        volatile LONG i;
        for (i = 0; i < 50000; i++) ;  /* Longer delay for button press */
    }
    
    /* Release button */
    if (!test_inject_mouse(x, y, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return FALSE;
    
    /* Wait for release to be processed */
    {
        volatile LONG i;
        for (i = 0; i < 10000; i++) ;
    }
    
    return TRUE;
}

/*
 * Helper: Perform a right-click menu drag operation.
 * Press RMB at start position, move to end position, release.
 * This is how Amiga menus work - hold RMB, drag to item, release.
 *
 * @param startX, startY  Where to press RMB (menu bar area)
 * @param endX, endY      Where to release RMB (menu item)
 * @return TRUE on success
 */
static inline BOOL test_menu_select(WORD startX, WORD startY, WORD endX, WORD endY)
{
    /* Move to menu bar */
    if (!test_inject_mouse(startX, startY, 0, DISPLAY_EVENT_MOUSEMOVE))
        return FALSE;
    
    /* Short delay */
    {
        volatile LONG i;
        for (i = 0; i < 10000; i++) ;
    }
    
    /* Press right mouse button (enters menu mode) */
    if (!test_inject_mouse(startX, startY, MOUSE_RIGHTBUTTON, DISPLAY_EVENT_MOUSEBUTTON))
        return FALSE;
    
    /* Wait for menu to appear */
    {
        volatile LONG i;
        for (i = 0; i < 100000; i++) ;  /* Longer delay for menu rendering */
    }
    
    /* Move to menu item (while RMB still held) */
    if (!test_inject_mouse(endX, endY, MOUSE_RIGHTBUTTON, DISPLAY_EVENT_MOUSEMOVE))
        return FALSE;
    
    /* Wait for item to highlight */
    {
        volatile LONG i;
        for (i = 0; i < 50000; i++) ;
    }
    
    /* Release right mouse button (selects item) */
    if (!test_inject_mouse(endX, endY, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return FALSE;
    
    /* Wait for selection to be processed */
    {
        volatile LONG i;
        for (i = 0; i < 20000; i++) ;
    }
    
    return TRUE;
}

/*
 * Helper: Check if a region has changed by comparing pixel counts.
 * Saves the current pixel count for a region, useful for detecting UI updates.
 *
 * @param x, y, w, h  Region to check
 * @param baseline    Previous pixel count to compare against
 * @param threshold   Minimum difference to consider "changed"
 * @return TRUE if region has changed (|current - baseline| >= threshold)
 */
static inline BOOL test_region_changed(WORD x, WORD y, WORD w, WORD h, 
                                        LONG baseline, LONG threshold)
{
    LONG current = test_get_region_content(x, y, w, h);
    if (current < 0)
        return FALSE;  /* Error getting region content */
    
    LONG diff = current - baseline;
    if (diff < 0) diff = -diff;  /* abs() */
    
    return (diff >= threshold);
}

/*
 * Helper: Wait for a region to have content (with timeout).
 * Useful for waiting for windows/menus to appear.
 *
 * @param x, y, w, h   Region to check
 * @param min_pixels   Minimum pixels to consider "has content"
 * @param max_loops    Maximum spin loops to wait (0 = no limit)
 * @return TRUE if region has content, FALSE if timeout
 */
static inline BOOL test_wait_for_region_content(WORD x, WORD y, WORD w, WORD h,
                                                 LONG min_pixels, ULONG max_loops)
{
    ULONG loops = 0;
    
    while (max_loops == 0 || loops < max_loops)
    {
        LONG content = test_get_region_content(x, y, w, h);
        if (content >= min_pixels)
            return TRUE;
        
        /* Small spin delay */
        {
            volatile LONG i;
            for (i = 0; i < 1000; i++) ;
        }
        
        loops++;
    }
    
    return FALSE;
}

/*
 * Helper: Get the approximate center of a gadget given its geometry.
 * Useful for calculating click coordinates.
 */
static inline void test_gadget_center(WORD gadX, WORD gadY, WORD gadW, WORD gadH,
                                       WORD winX, WORD winY,
                                       WORD *outX, WORD *outY)
{
    /* Convert gadget-relative coordinates to screen coordinates */
    *outX = winX + gadX + (gadW / 2);
    *outY = winY + gadY + (gadH / 2);
}

/*
 * Helper: Drain all pending IDCMP messages from a port.
 * Useful for clearing the queue before testing specific interactions.
 *
 * @param port  The message port to drain
 * @return Number of messages drained
 *
 * Note: This requires the caller to have exec.library available.
 * Caller should #include <clib/exec_protos.h> and use their SysBase.
 */
#define TEST_DRAIN_PORT(port, count) \
    do { \
        struct Message *_msg; \
        (count) = 0; \
        while ((_msg = GetMsg(port)) != NULL) { \
            ReplyMsg(_msg); \
            (count)++; \
        } \
    } while(0)

#endif /* TEST_INJECT_H */
