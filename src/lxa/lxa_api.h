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
 * Window information structure
 */
typedef struct lxa_window_info {
    int x, y;             /* Window position */
    int width, height;    /* Window dimensions */
    char title[256];      /* Window title */
} lxa_window_info_t;

/*
 * Get information about a window.
 *
 * @param index  Window index (0-based)
 * @param info   Output: window information
 * @return true if window exists, false otherwise
 */
bool lxa_get_window_info(int index, lxa_window_info_t *info);

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

/*
 * Capture the screen to a file.
 *
 * @param filename  Output filename (PPM format)
 * @return true on success
 */
bool lxa_capture_screen(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* LXA_API_H */
