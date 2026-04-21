/*
 * lxa_api.h - Host-side API for lxa emulator
 *
 * This header provides a C API for controlling the lxa emulator from
 * host-side programs, primarily for automated testing.
 *
 * Example usage:
 *
 *   lxa_config_t config = {
 *       .rom_path = "path/to/lxa.rom",
 *       .sys_drive = "path/to/sys",
 *       .headless = true,
 *   };
 *   
 *   if (lxa_init(&config) != 0) return 1;
 *   
 *   lxa_load_program("MyProgram", "");
 *   
 *   // Run until window opens
 *   while (lxa_get_window_count() == 0 && lxa_is_running()) {
 *       lxa_run_cycles(10000);
 *   }
 *   
 *   // Inject mouse click
 *   lxa_inject_mouse_click(100, 50, LXA_MOUSE_LEFT);
 *   
 *   // Click close gadget
 *   lxa_click_close_gadget(0);
 *   
 *   // Run until program exits
 *   while (lxa_is_running()) {
 *       lxa_run_cycles(10000);
 *   }
 *   
 *   lxa_shutdown();
 */

#ifndef LXA_API_H
#define LXA_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configuration structure for lxa initialization
 */
typedef struct lxa_config {
    const char *rom_path;       /* Path to lxa.rom file (required) */
    const char *sys_drive;      /* Path to SYS: drive (optional, defaults to ".") */
    const char *config_path;    /* Path to config.ini (optional) */
    bool headless;              /* Run without SDL display windows */
    bool rootless;              /* Track individual windows for testing (default: true) */
    bool verbose;               /* Enable verbose logging */
} lxa_config_t;

/*
 * Initialize the lxa emulator.
 * Must be called before any other lxa_* functions.
 *
 * @param config  Configuration options
 * @return 0 on success, non-zero on failure
 */
int lxa_init(const lxa_config_t *config);

/*
 * Shutdown the lxa emulator.
 * Frees all resources and closes displays.
 */
void lxa_shutdown(void);

/*
 * Load a program to run.
 * The program is loaded but execution doesn't start until lxa_run_cycles().
 *
 * @param program  Program path (Amiga-style path like "SYS:MyProgram")
 * @param args     Command line arguments (can be NULL or empty)
 * @return 0 on success, non-zero on failure
 */
int lxa_load_program(const char *program, const char *args);

/*
 * Execute a number of CPU cycles.
 * This is the main emulation driver function.
 *
 * @param cycles  Number of m68k cycles to execute
 * @return 0 if still running, non-zero if program has exited
 */
int lxa_run_cycles(int cycles);

/*
 * Flush the display by converting Amiga planar bitmap data to chunky pixels.
 *
 * This forces an immediate planar-to-chunky conversion so that
 * display_read_pixel() returns up-to-date values reflecting the
 * current state of the Amiga screen bitmap.
 *
 * Normally, this conversion happens at the start of each VBlank in
 * lxa_run_cycles().  After modifying the planar bitmap (e.g. via
 * RectFill/complement), call this before reading pixels to avoid
 * stale data.
 */
void lxa_flush_display(void);

/*
 * Check if the emulator is still running a program.
 *
 * @return true if a program is running, false if exited
 */
bool lxa_is_running(void);

/*
 * Get the exit code from the last program.
 *
 * @return Exit code (0 = success)
 */
int lxa_get_exit_code(void);

/*
 * Run the emulator until it exits or timeout.
 * This is a convenience function that calls lxa_run_cycles() in a loop.
 *
 * @param timeout_ms  Maximum time to run in milliseconds (0 = no timeout)
 * @return Program exit code, or -1 on timeout
 */
int lxa_run_until_exit(int timeout_ms);

/* ========== VFS/Drive/Assign API ========== */

/*
 * Add an AmigaDOS assign.
 * This creates a logical name that points to a directory.
 * Must be called after lxa_init() but before lxa_load_program().
 *
 * Example: lxa_add_assign("Cluster", "/path/to/cluster") allows
 * the program to access "Cluster:file" which resolves to /path/to/cluster/file
 *
 * @param name        Assign name (without colon, e.g. "Cluster")
 * @param linux_path  Host filesystem path to map
 * @return true on success
 */
bool lxa_add_assign(const char *name, const char *linux_path);

/**
 * Add a path to an existing AmigaDOS assign (multi-assign).
 * If the assign doesn't exist, it is created.
 *
 * @param name        Assign name (without colon)
 * @param linux_path  Host filesystem path to add
 * @return true on success
 */
bool lxa_add_assign_path(const char *name, const char *linux_path);

/*
 * Add an AmigaDOS drive.
 * This creates a volume that points to a directory.
 * Must be called after lxa_init() but before lxa_load_program().
 *
 * @param name        Drive name (without colon, e.g. "Work")
 * @param linux_path  Host filesystem path to map
 * @return true on success
 */
bool lxa_add_drive(const char *name, const char *linux_path);

/* ========== Event Injection API ========== */

/* Mouse button constants */
#define LXA_MOUSE_LEFT    0x01
#define LXA_MOUSE_RIGHT   0x02
#define LXA_MOUSE_MIDDLE  0x04

/* Event types */
#define LXA_EVENT_MOUSEMOVE   2
#define LXA_EVENT_MOUSEBUTTON 1

/*
 * Inject a mouse event.
 *
 * @param x           Screen X coordinate
 * @param y           Screen Y coordinate
 * @param buttons     Button state (LXA_MOUSE_* flags)
 * @param event_type  LXA_EVENT_MOUSEMOVE or LXA_EVENT_MOUSEBUTTON
 * @return true on success
 */
bool lxa_inject_mouse(int x, int y, int buttons, int event_type);

/*
 * Inject a mouse click (press + release) at a position.
 * Convenience function that injects button down, runs some cycles,
 * then injects button up.
 *
 * @param x       Screen X coordinate
 * @param y       Screen Y coordinate
 * @param button  Button to click (LXA_MOUSE_LEFT, etc.)
 * @return true on success
 */
bool lxa_inject_mouse_click(int x, int y, int button);

/*
 * Inject a right mouse button click (for menu access).
 * This is a convenience wrapper for lxa_inject_mouse_click with LXA_MOUSE_RIGHT.
 *
 * @param x  Screen X coordinate
 * @param y  Screen Y coordinate
 * @return true on success
 */
bool lxa_inject_rmb_click(int x, int y);

/*
 * Inject a mouse drag operation.
 * Moves from (start_x, start_y) to (end_x, end_y) while holding button down.
 *
 * @param start_x  Starting X coordinate
 * @param start_y  Starting Y coordinate
 * @param end_x    Ending X coordinate
 * @param end_y    Ending Y coordinate
 * @param button   Button to hold (LXA_MOUSE_LEFT, etc.)
 * @param steps    Number of intermediate steps (0 = direct, higher = smoother)
 * @return true on success
 */
bool lxa_inject_drag(int start_x, int start_y, int end_x, int end_y, int button, int steps);

/*
 * Inject a keyboard event.
 *
 * @param rawkey     Amiga raw key code
 * @param qualifier  Qualifier bits (shift, ctrl, etc.)
 * @param down       true for key press, false for key release
 * @return true on success
 */
bool lxa_inject_key(int rawkey, int qualifier, bool down);

/*
 * Inject a key press and release.
 *
 * @param rawkey     Amiga raw key code
 * @param qualifier  Qualifier bits
 * @return true on success
 */
bool lxa_inject_keypress(int rawkey, int qualifier);

/*
 * Inject a string as a sequence of key events.
 *
 * @param str  String to type (ASCII)
 * @return true on success
 */
bool lxa_inject_string(const char *str);

/* ========== State Query API ========== */

/*
 * Get the number of open windows.
 *
 * @return Number of open Amiga windows
 */
int lxa_get_window_count(void);

/*
 * Get non-background pixel count for a rootless window.
 *
 * @param index  Window index (0-based)
 * @return Number of non-background pixels, or -1 if unavailable
 */
int lxa_get_window_content(int index);

/*
 * Window information structure
 */
typedef struct lxa_window_info {
    int x, y;             /* Window position */
    int width, height;    /* Window dimensions */
    char title[256];      /* Window title */
} lxa_window_info_t;

typedef struct lxa_gadget_info {
    int left;
    int top;
    int width;
    int height;
    uint16_t flags;
    uint16_t activation;
    uint16_t gadget_type;
    uint16_t gadget_id;
    uint32_t user_data;
} lxa_gadget_info_t;

/*
 * Get information about a window.
 *
 * @param index  Window index (0-based)
 * @param info   Output: window information
 * @return true if window exists, false otherwise
 */
bool lxa_get_window_info(int index, lxa_window_info_t *info);

/*
 * Wait until window content becomes non-empty.
 *
 * @param index       Window index (0-based)
 * @param timeout_ms  Maximum time to wait
 * @return true if content is visible, false on timeout
 */
bool lxa_wait_window_drawn(int index, int timeout_ms);

/*
 * Get the number of gadgets attached to a tracked Intuition window.
 *
 * @param window_index  Window index (0-based)
 * @return Number of gadgets, or -1 if unavailable
 */
int lxa_get_gadget_count(int window_index);

/*
 * Get gadget information by window/gadget index.
 * Gadget coordinates are screen-relative so they can be used directly for clicks.
 *
 * @param window_index  Window index (0-based)
 * @param gadget_index  Gadget index (0-based)
 * @param info          Output gadget information
 * @return true on success
 */
bool lxa_get_gadget_info(int window_index, int gadget_index, lxa_gadget_info_t *info);

/*
 * Click the close gadget of a window.
 * This injects a click at the close gadget position.
 *
 * @param window_index  Index of window to close (0 = first/active window)
 * @return true on success
 */
bool lxa_click_close_gadget(int window_index);

/*
 * Get screen dimensions.
 *
 * @param width   Output: screen width (can be NULL)
 * @param height  Output: screen height (can be NULL)
 * @param depth   Output: screen depth (can be NULL)
 * @return true if screen is active
 */
bool lxa_get_screen_dimensions(int *width, int *height, int *depth);

/*
 * Screen information structure (extended info beyond dimensions)
 */
typedef struct lxa_screen_info {
    int width;            /* Screen width in pixels */
    int height;           /* Screen height in pixels */
    int depth;            /* Screen bit depth */
    int view_modes;       /* View mode flags (interlace, hires, etc.) */
    char title[256];      /* Screen title */
    int num_colors;       /* Number of palette entries */
} lxa_screen_info_t;

/*
 * Get extended screen information.
 *
 * @param info  Output: screen information structure
 * @return true if screen is active
 */
bool lxa_get_screen_info(lxa_screen_info_t *info);

/*
 * Read a pixel color at screen coordinates.
 * Returns the palette index (pen) at the given position.
 *
 * @param x      Screen X coordinate
 * @param y      Screen Y coordinate
 * @param pen    Output: palette index (0-255)
 * @return true on success, false if coordinates out of bounds or no screen
 */
bool lxa_read_pixel(int x, int y, int *pen);

/*
 * Read pixel RGB color at screen coordinates.
 * Returns the actual RGB color after palette lookup.
 *
 * @param x      Screen X coordinate
 * @param y      Screen Y coordinate
 * @param r      Output: red component (0-255)
 * @param g      Output: green component (0-255)
 * @param b      Output: blue component (0-255)
 * @return true on success
 */
bool lxa_read_pixel_rgb(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);

/*
 * Read a palette entry RGB value by pen index from the active display.
 *
 * @param pen    Palette index (0-255)
 * @param r      Output: red component (0-255)
 * @param g      Output: green component (0-255)
 * @param b      Output: blue component (0-255)
 * @return true on success
 */
bool lxa_get_palette_rgb(int pen, uint8_t *r, uint8_t *g, uint8_t *b);

/*
 * Check if the screen has content (non-background pixels).
 *
 * @return Number of non-background pixels, or -1 if no screen
 */
int lxa_get_content_pixels(void);

/* ========== Waiting API ========== */

/*
 * Manually trigger VBlank processing.
 * For test drivers that need immediate event processing without
 * waiting for the real-time SIGALRM timer.
 *
 * Call this after injecting events to ensure they're processed
 * on the next lxa_run_cycles() call.
 */
void lxa_trigger_vblank(void);

/*
 * Check whether all Exec tasks are in TS_WAIT state (idle).
 *
 * Returns true when the TaskReady list is empty, meaning every task
 * (including the currently running one) is blocked on WaitPort() or
 * Wait() and no task has pending work.  This is the signal that the
 * emulated system has processed all queued events and is waiting for
 * new input.
 *
 * Used by injection helpers to return early instead of burning
 * millions of cycles waiting for the app to finish processing.
 *
 * @return true if all tasks are idle
 */
bool lxa_is_idle(void);

/*
 * Run cycles with VBlank interrupts until idle or budget exhausted.
 *
 * Triggers VBlanks and runs cycles in a loop, returning early if all
 * Exec tasks enter TS_WAIT.  This replaces fixed-count cycle loops
 * with an adaptive approach that finishes as soon as the emulated
 * system has processed the pending event.
 *
 * @param max_iterations  Maximum VBlank+cycle iterations
 * @param cycles_per_iter Cycles to execute per iteration
 * @return Number of iterations actually executed (0 if already idle)
 */
int lxa_run_until_idle(int max_iterations, int cycles_per_iter);

/*
 * Wait until the event queue is empty.
 *
 * @param timeout_ms  Maximum time to wait (0 = don't wait, just check)
 * @return true if queue is empty, false on timeout
 */
bool lxa_wait_idle(int timeout_ms);

/*
 * Wait until N windows are open.
 *
 * @param count       Expected number of windows
 * @param timeout_ms  Maximum time to wait
 * @return true if windows opened, false on timeout
 */
bool lxa_wait_windows(int count, int timeout_ms);

/*
 * Wait until no windows are open (program exited).
 *
 * @param timeout_ms  Maximum time to wait
 * @return true if all windows closed, false on timeout
 */
bool lxa_wait_exit(int timeout_ms);

/* ========== Output Capture API ========== */

/*
 * Get the console output from the program.
 * Returns the accumulated stdout output.
 *
 * @param buffer  Output buffer
 * @param size    Buffer size
 * @return Number of bytes written to buffer
 */
int lxa_get_output(char *buffer, int size);

/*
 * Clear the output buffer.
 */
void lxa_clear_output(void);

/* ========== Phase 131: Window/Screen Event Log API ========== */

/*
 * Intuition event types recorded in the circular event log.
 */
#define LXA_INTUI_EVENT_OPEN_WINDOW    1
#define LXA_INTUI_EVENT_CLOSE_WINDOW   2
#define LXA_INTUI_EVENT_OPEN_SCREEN    3
#define LXA_INTUI_EVENT_CLOSE_SCREEN   4
#define LXA_INTUI_EVENT_OPEN_REQUESTER 5
#define LXA_INTUI_EVENT_CLOSE_REQUESTER 6

/*
 * Single Intuition event record.
 */
typedef struct lxa_intui_event {
    int      type;          /* LXA_INTUI_EVENT_* constant */
    int      window_index;  /* For window events: host window index; -1 if unknown */
    char     title[128];    /* Window/screen title (may be empty) */
    int      x, y;         /* Position (window events) */
    int      width, height; /* Dimensions (window events) */
} lxa_intui_event_t;

/*
 * Maximum events stored before the oldest is overwritten.
 */
#define LXA_EVENT_LOG_SIZE 256

/*
 * Push one event into the circular log.
 * Called internally from the dispatch layer; safe to call from any context.
 */
void lxa_push_intui_event(int type, int window_index, const char *title,
                          int x, int y, int w, int h);

/*
 * Drain all pending events from the log.
 * Copies up to max_count events into the caller-supplied array and
 * returns the number of events copied.  Draining clears the log.
 *
 * @param events     Caller-supplied output array
 * @param max_count  Size of the array
 * @return Number of events copied (0 if log is empty)
 */
int lxa_drain_intui_events(lxa_intui_event_t *events, int max_count);

/*
 * Return the number of events currently pending in the log
 * (without draining them).
 */
int lxa_intui_event_count(void);

/* ========== Phase 131: Menu Introspection API ========== */

/*
 * Opaque handle for a menu strip traversal context.
 * Obtained via lxa_get_menu_strip(); freed via lxa_free_menu_strip().
 */
typedef struct lxa_menu_strip lxa_menu_strip_t;

/*
 * Per-menu-item information returned by lxa_get_menu_info().
 */
typedef struct lxa_menu_info {
    char     name[64];      /* Menu or item title (NUL-terminated) */
    bool     enabled;       /* True if ITEMENABLED is set (or it is a top-level menu) */
    bool     checked;       /* True if CHECKED flag is set */
    bool     has_submenu;   /* True if item has a sub-item chain */
    int      x, y;         /* Screen-relative position of the bounding box */
    int      width, height; /* Bounding box dimensions */
} lxa_menu_info_t;

/*
 * Obtain a snapshot of the MenuStrip attached to a tracked window.
 *
 * The returned handle is valid until lxa_free_menu_strip() is called or
 * lxa_shutdown() is called.  It is safe to call from the host side at
 * any point after the app has opened its menus via SetMenuStrip().
 *
 * @param window_index  Index into the tracked rootless window list (0-based)
 * @return Opaque handle, or NULL if the window has no menu strip.
 */
lxa_menu_strip_t *lxa_get_menu_strip(int window_index);

/*
 * Return the number of top-level menus in a strip.
 *
 * @param strip  Handle from lxa_get_menu_strip()
 * @return Number of menus, or 0 if strip is NULL.
 */
int lxa_get_menu_count(lxa_menu_strip_t *strip);

/*
 * Return the number of items under a top-level menu.
 *
 * @param strip     Handle from lxa_get_menu_strip()
 * @param menu_idx  0-based menu index
 * @return Number of items, or 0 on error.
 */
int lxa_get_item_count(lxa_menu_strip_t *strip, int menu_idx);

/*
 * Return information about a menu or item.
 *
 * Pass item_idx = -1 to query a top-level menu title.
 * Pass sub_idx  = -1 (or omit) to query an item (not a sub-item).
 *
 * @param strip     Handle from lxa_get_menu_strip()
 * @param menu_idx  0-based menu index
 * @param item_idx  0-based item index, or -1 for the menu title itself
 * @param sub_idx   0-based sub-item index, or -1 for the item itself
 * @param out       Output structure
 * @return true on success, false if indices are out of range.
 */
bool lxa_get_menu_info(lxa_menu_strip_t *strip,
                       int menu_idx, int item_idx, int sub_idx,
                       lxa_menu_info_t *out);

/*
 * Release the menu strip handle.
 * Must be called when finished with the handle to avoid leaking memory.
 *
 * @param strip  Handle from lxa_get_menu_strip() (may be NULL)
 */
void lxa_free_menu_strip(lxa_menu_strip_t *strip);

/* ========== Phase 130: Text() interception hook. ========== */

/*
 * Phase 130: Text() interception hook.
 *
 * Register a callback that is invoked by _graphics_Text() before each string
 * is rendered.  The callback receives a copy of the raw string bytes, the
 * character count, the RastPort pen position (cp_x / cp_y) at the time of
 * the call, and the opaque userdata pointer supplied here.
 *
 * The hook is called from within the emulator's main dispatch loop, so it must
 * not call any lxa_run_* or lxa_inject_* functions.  It may safely append to
 * a host-side container (std::vector, std::string, etc.).
 *
 * Call lxa_clear_text_hook() in TearDown() to avoid dangling pointers.
 *
 * Example (C++):
 *   std::vector<std::string> log;
 *   lxa_set_text_hook([](const char *s, int n, int, int, void *ud) {
 *       ((std::vector<std::string>*)ud)->push_back(std::string(s, n));
 *   }, &log);
 *   // ... run emulator ...
 *   lxa_clear_text_hook();
 *
 * @param hook      Callback function (NULL disables the hook)
 * @param userdata  Opaque pointer forwarded to the callback
 */
typedef void (*lxa_text_hook_t)(const char *str, int len, int x, int y, void *userdata);
void lxa_set_text_hook(lxa_text_hook_t hook, void *userdata);

/*
 * Unregister the text interception hook (equivalent to lxa_set_text_hook(NULL, NULL)).
 */
void lxa_clear_text_hook(void);

/*
 * Capture the screen to a file.
 *
 * @param filename  Output filename (PNG format)
 * @return true on success
 */
bool lxa_capture_screen(const char *filename);

/*
 * Capture a tracked rootless window to a file.
 *
 * @param window_index  Window index (0-based)
 * @param filename      Output filename (PNG format)
 * @return true on success
 */
bool lxa_capture_window(int window_index, const char *filename);

/*
 * Read a 32-bit value from emulated memory.
 * @param addr  Address in emulated memory space
 * @return 32-bit value at address
 */
uint32_t lxa_peek32(uint32_t addr);

/*
 * Read a 16-bit value from emulated memory.
 * @param addr  Address in emulated memory space
 * @return 16-bit value at address
 */
uint16_t lxa_peek16(uint32_t addr);

/*
 * Read an 8-bit value from emulated memory.
 * @param addr  Address in emulated memory space
 * @return 8-bit value at address
 */
uint8_t lxa_peek8(uint32_t addr);

/* ========== Profiling API ========== */

/*
 * Maximum number of distinct EMU_CALL IDs tracked by the profiler.
 * Covers the full range 0–5999 used in emucalls.h.
 */
#define LXA_PROFILE_MAX_EMUCALL  6000

/*
 * Per-call profiling record.
 */
typedef struct lxa_profile_entry {
    int      emucall_id;        /* EMU_CALL_* constant */
    uint64_t call_count;        /* Number of invocations */
    uint64_t total_ns;          /* Total wall-clock nanoseconds spent */
} lxa_profile_entry_t;

/*
 * Reset all profiling counters.
 * Safe to call at any time (including before lxa_init).
 */
void lxa_profile_reset(void);

/*
 * Get profiling data for all EMU_CALL IDs that were called at least once.
 *
 * @param entries   Caller-supplied array of lxa_profile_entry_t
 * @param max_count Size of the entries array
 * @return Number of entries filled (entries with call_count > 0)
 */
int lxa_profile_get(lxa_profile_entry_t *entries, int max_count);

/*
 * Write profiling data as JSON to a file.
 * The JSON is an array of objects:
 *   [{"id": 1000, "name": "EMU_CALL_DOS_OPEN", "calls": 42, "total_ns": 12345}, ...]
 * sorted by total_ns descending.
 *
 * @param path  Output file path (creates or truncates)
 * @return true on success
 */
bool lxa_profile_write_json(const char *path);

/*
 * Return the canonical name string for an EMU_CALL_* constant, or
 * "EMU_CALL_UNKNOWN_<id>" if the id is not recognized.
 *
 * @param emucall_id  EMU_CALL_* constant
 * @param buf         Caller-supplied buffer
 * @param buf_size    Size of buf
 */
void lxa_profile_emucall_name(int emucall_id, char *buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif /* LXA_API_H */
