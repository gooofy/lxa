#ifndef HAVE_DISPLAY_H
#define HAVE_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Phase 13: Graphics Foundation - Host Display Subsystem
 *
 * This module provides the host-side display abstraction using SDL2.
 * It manages the host window/surface that displays the Amiga graphics.
 *
 * The display system supports:
 * - Single window for screen rendering
 * - Palette management (OCS/ECS style - up to 256 colors)
 * - Rectangle updates for efficient blitting
 * - Event polling integration with the main emulator loop
 */

/* Maximum supported screen dimensions */
#define DISPLAY_MAX_WIDTH   1920
#define DISPLAY_MAX_HEIGHT  1200
#define DISPLAY_MAX_DEPTH   8       /* Max 256 colors (AGA compatibility) */
#define DISPLAY_MAX_COLORS  256

/* Display handle (opaque) */
typedef struct display_t display_t;

/*
 * Initialize the display subsystem.
 * Must be called before any other display functions.
 * Returns true on success, false on failure.
 */
bool display_init(void);

/*
 * Shutdown the display subsystem.
 * Closes all windows and frees resources.
 */
void display_shutdown(void);

/*
 * Open a display window.
 * @param width   Width in pixels
 * @param height  Height in pixels  
 * @param depth   Bit depth (1-8 for planar)
 * @param title   Window title (can be NULL)
 * @return Display handle, or NULL on failure
 */
display_t *display_open(int width, int height, int depth, const char *title);

/*
 * Close a display window.
 * @param display  Display handle from display_open()
 */
void display_close(display_t *display);

/*
 * Set a palette entry.
 * @param display  Display handle
 * @param index    Color index (0-255)
 * @param r        Red component (0-255)
 * @param g        Green component (0-255)
 * @param b        Blue component (0-255)
 */
void display_set_color(display_t *display, int index, uint8_t r, uint8_t g, uint8_t b);

/*
 * Set multiple palette entries (RGB4 format - 4 bits per component).
 * @param display  Display handle
 * @param start    Starting color index
 * @param count    Number of colors to set
 * @param colors   Array of RGB4 values (12-bit: 0xRGB)
 */
void display_set_palette_rgb4(display_t *display, int start, int count, const uint16_t *colors);

/*
 * Set multiple palette entries (RGB32 format - 8 bits per component).
 * @param display  Display handle
 * @param start    Starting color index
 * @param count    Number of colors to set
 * @param colors   Array of RGB32 values packed as R,G,B triplets (8-bit each)
 */
void display_set_palette_rgb32(display_t *display, int start, int count, const uint32_t *colors);

/*
 * Update a rectangular region from planar bitmap data.
 * Converts planar Amiga bitmap format to chunky for SDL.
 *
 * @param display     Display handle
 * @param x, y        Top-left corner of region
 * @param width       Width of region
 * @param height      Height of region
 * @param planes      Array of plane pointers (depth planes)
 * @param bytes_per_row  Bytes per row in each plane
 * @param depth       Number of bit planes
 */
void display_update_planar(display_t *display, int x, int y, int width, int height,
                           const uint8_t **planes, int bytes_per_row, int depth);

/*
 * Update a rectangular region from chunky (8-bit indexed) pixel data.
 *
 * @param display     Display handle
 * @param x, y        Top-left corner of region  
 * @param width       Width of region
 * @param height      Height of region
 * @param pixels      Pixel data (8-bit indexed)
 * @param pitch       Bytes per row in pixel data
 */
void display_update_chunky(display_t *display, int x, int y, int width, int height,
                           const uint8_t *pixels, int pitch);

/*
 * Refresh the display - present the back buffer to screen.
 * Call this after updating regions to make changes visible.
 *
 * @param display  Display handle
 */
void display_refresh(display_t *display);

/*
 * Refresh all open displays.
 * Called from main loop at VBlank time to update all screens and windows.
 */
void display_refresh_all(void);

/*
 * Set the Amiga bitmap info for automatic screen refresh.
 * Called after opening a screen to enable auto-sync from planar RAM.
 *
 * @param display     Display handle
 * @param planes_ptr  Address of BitMap.Planes[] array in emulated RAM
 * @param bpr_depth   Packed value: (bytes_per_row << 16) | depth
 */
void display_set_amiga_bitmap(display_t *display, uint32_t planes_ptr, uint32_t bpr_depth);

/*
 * Get the active display (for VBlank refresh).
 * @return Active display handle, or NULL if no display is open
 */
display_t *display_get_active(void);

/*
 * Get the Amiga bitmap info for a display.
 * Returns false if no bitmap is configured.
 *
 * @param display     Display handle
 * @param planes_ptr  Output: address of BitMap.Planes[] array
 * @param bpr         Output: bytes per row
 * @param depth       Output: number of bitplanes
 * @return true if bitmap info is available, false otherwise
 */
bool display_get_amiga_bitmap(display_t *display, uint32_t *planes_ptr, uint32_t *bpr, uint32_t *depth);

/*
 * Get display dimensions.
 *
 * @param display  Display handle
 * @param width    Output: width (can be NULL)
 * @param height   Output: height (can be NULL)
 * @param depth    Output: depth (can be NULL)
 */
void display_get_size(display_t *display, int *width, int *height, int *depth);

/*
 * Process SDL events.
 * Should be called regularly from the main loop.
 * Returns true if quit was requested, false otherwise.
 */
bool display_poll_events(void);

/*
 * Check if SDL2 is available (was successfully initialized).
 * @return true if SDL2 is ready, false if graphics are disabled
 */
bool display_available(void);

/*
 * Input Event Types (for IDCMP integration)
 */
typedef enum {
    DISPLAY_EVENT_NONE = 0,
    DISPLAY_EVENT_MOUSEBUTTON = 1,
    DISPLAY_EVENT_MOUSEMOVE = 2,
    DISPLAY_EVENT_KEY = 3,
    DISPLAY_EVENT_CLOSEWINDOW = 4,
    DISPLAY_EVENT_QUIT = 5
} display_event_type_t;

/*
 * Input Event Structure
 * Stores the most recent input event for IDCMP processing
 */
typedef struct {
    display_event_type_t type;
    int mouse_x;          /* Mouse X position (screen-relative) */
    int mouse_y;          /* Mouse Y position (screen-relative) */
    int button_code;      /* Button code (IECODE_LBUTTON, etc) - for mouse events */
    int rawkey;           /* Raw key code - for key events */
    int qualifier;        /* Key/mouse qualifier bits */
    bool button_down;     /* true = pressed, false = released */
    display_t *window;    /* Display that received the event */
} display_event_t;

/*
 * Get the next pending input event.
 * @param event   Output: event data (if return is true)
 * @return true if an event was available, false if no events pending
 */
bool display_get_event(display_event_t *event);

/*
 * Get current mouse position.
 * @param x   Output: mouse X (can be NULL)
 * @param y   Output: mouse Y (can be NULL)
 */
void display_get_mouse_pos(int *x, int *y);

/*
 * Set the active display (for event routing).
 * @param display  Display to make active
 */
void display_set_active(display_t *display);

/*
 * Phase 15: Rootless Windowing Support
 *
 * In rootless mode, each Amiga window gets its own SDL window instead
 * of all windows being rendered within a single screen surface.
 * This allows integration with the host desktop's window manager.
 */

/* Window handle for rootless mode (opaque) */
typedef struct display_window_t display_window_t;

/*
 * Check if rootless mode is enabled.
 * @return true if rootless mode is active, false for traditional screen mode
 */
bool display_get_rootless_mode(void);

/*
 * Open a rootless window.
 * Creates an independent SDL window for an Amiga window.
 *
 * @param screen   Parent screen display (for palette sharing, can be NULL)
 * @param x        Initial X position on host desktop
 * @param y        Initial Y position on host desktop  
 * @param width    Window width in pixels
 * @param height   Window height in pixels
 * @param depth    Bit depth (uses screen palette if screen provided)
 * @param title    Window title (can be NULL)
 * @return Window handle, or NULL on failure
 */
display_window_t *display_window_open(display_t *screen, int x, int y,
                                       int width, int height, int depth,
                                       const char *title);

/*
 * Close a rootless window.
 * @param window  Window handle from display_window_open()
 */
void display_window_close(display_window_t *window);

/*
 * Move a rootless window.
 * @param window  Window handle
 * @param x       New X position
 * @param y       New Y position
 * @return true on success
 */
bool display_window_move(display_window_t *window, int x, int y);

/*
 * Resize a rootless window.
 * @param window  Window handle
 * @param width   New width
 * @param height  New height
 * @return true on success
 */
bool display_window_size(display_window_t *window, int width, int height);

/*
 * Bring a rootless window to front.
 * @param window  Window handle
 * @return true on success
 */
bool display_window_to_front(display_window_t *window);

/*
 * Send a rootless window to back.
 * @param window  Window handle
 * @return true on success
 */
bool display_window_to_back(display_window_t *window);

/*
 * Set a rootless window title.
 * @param window  Window handle
 * @param title   New title
 * @return true on success
 */
bool display_window_set_title(display_window_t *window, const char *title);

/*
 * Refresh a rootless window - update from its pixel buffer.
 * @param window  Window handle
 */
void display_window_refresh(display_window_t *window);

/*
 * Update rootless window from planar bitmap data.
 *
 * @param window      Window handle
 * @param x, y        Top-left corner of region
 * @param width       Width of region
 * @param height      Height of region
 * @param planes      Array of plane pointers (depth planes)
 * @param bytes_per_row  Bytes per row in each plane
 * @param depth       Number of bit planes
 */
void display_window_update_planar(display_window_t *window, int x, int y, 
                                   int width, int height,
                                   const uint8_t **planes, int bytes_per_row, int depth);

/*
 * Get the screen associated with a rootless window.
 * @param window  Window handle
 * @return Parent screen, or NULL
 */
display_t *display_window_get_screen(display_window_t *window);

/*
 * Look up a window by its SDL window ID.
 * Used for routing SDL events to the correct Amiga window.
 * @param sdl_window_id  SDL window ID from event
 * @return Window handle, or NULL if not found
 */
display_window_t *display_window_from_sdl_id(uint32_t sdl_window_id);

/*
 * Get the SDL window ID for a display window.
 * @param window  Window handle
 * @return SDL window ID, or 0 if invalid
 */
uint32_t display_window_get_sdl_id(display_window_t *window);

/*
 * Phase 21: UI Testing Infrastructure
 *
 * These functions allow automated testing by injecting input events
 * and capturing screen output programmatically.
 */

/*
 * Inject a keyboard event into the event queue.
 * This creates a synthetic key press or release event.
 *
 * @param rawkey     Amiga raw key code (0x00-0x7F for press, 0x80+ for release)
 * @param qualifier  Amiga qualifier bits (shift, ctrl, alt, etc.)
 * @param down       true for key press, false for key release
 * @return true on success
 */
bool display_inject_key(int rawkey, int qualifier, bool down);

/*
 * Convert an ASCII character to an Amiga rawkey code.
 * Returns 0xFF if no mapping exists.
 */
int ascii_to_rawkey(char c, bool *need_shift);

/*
 * Inject a string as a sequence of key events.
 * Each character is converted to appropriate rawkey codes.
 * Both key-down and key-up events are generated for each character.
 *
 * @param str  String to inject (ASCII characters)
 * @return true on success
 */
bool display_inject_string(const char *str);

/*
 * Inject a mouse event into the event queue.
 *
 * @param x           Mouse X position
 * @param y           Mouse Y position
 * @param buttons     Button state (bit 0 = left, bit 1 = right, bit 2 = middle)
 * @param event_type  DISPLAY_EVENT_MOUSEMOVE or DISPLAY_EVENT_MOUSEBUTTON
 * @return true on success
 */
bool display_inject_mouse(int x, int y, int buttons, display_event_type_t event_type);

/*
 * Set headless mode (no actual window rendering).
 * In headless mode, display operations succeed but no SDL window is shown.
 * Useful for automated testing without a display.
 *
 * @param enable  true to enable headless mode, false to disable
 * @return Previous headless state
 */
bool display_set_headless(bool enable);

/*
 * Check if headless mode is enabled.
 * @return true if headless mode is active
 */
bool display_get_headless(void);

/*
 * Capture the display to a PNG file.
 *
 * @param display   Display handle
 * @param filename  Output filename (PNG format)
 * @return true on success
 */
bool display_capture_screen(display_t *display, const char *filename);

/*
 * Capture a rootless window to a PNG file.
 *
 * @param window    Window handle
 * @param filename  Output filename (PNG format)
 * @return true on success
 */
bool display_capture_window(display_window_t *window, const char *filename);

/*
 * Check if the event queue is empty.
 * Useful for waiting until all injected events have been processed.
 *
 * @return true if no events are pending
 */
bool display_event_queue_empty(void);

/*
 * Phase 39b: Enhanced Application Testing Infrastructure
 *
 * These functions provide validation capabilities for testing
 * that applications render correctly, not just that they don't crash.
 */

/*
 * Get the dimensions of the active display.
 * @param width   Output: display width (can be NULL)
 * @param height  Output: display height (can be NULL)
 * @param depth   Output: display depth (can be NULL)
 * @return true if a display is active
 */
bool display_get_active_dimensions(int *width, int *height, int *depth);

/*
 * Check if the active display has non-empty content.
 * Returns the number of pixels that differ from the background color (index 0).
 * @return Number of non-background pixels, or -1 if no display
 */
int display_get_content_pixels(void);

/*
 * Check if a rectangular region of the display has content.
 * @param x, y        Top-left corner of region
 * @param width       Width of region to check
 * @param height      Height of region to check
 * @return Number of non-background pixels in region, or -1 on error
 */
int display_get_region_content(int x, int y, int width, int height);

/*
 * Get the number of open rootless windows.
 * @return Number of active windows
 */
int display_get_window_count(void);

/*
 * Get dimensions of a rootless window by index.
 * @param index   Window index (0-based)
 * @param width   Output: window width (can be NULL)
 * @param height  Output: window height (can be NULL)
 * @return true if window exists at index
 */
bool display_get_window_dimensions(int index, int *width, int *height);

/*
 * Check if a rootless window has content.
 * @param index  Window index (0-based)
 * @return Number of non-background pixels, or -1 if window not found
 */
int display_get_window_content(int index);

/*
 * Get window position by index.
 * @param index  Window index (0-based, in order of creation)
 * @param x      Output: window X position (can be NULL)
 * @param y      Output: window Y position (can be NULL)
 * @return true if window exists at index
 */
bool display_get_window_position(int index, int *x, int *y);

/*
 * Compare the active display to a reference PPM file.
 * @param reference_file  Path to reference PPM file
 * @return Similarity percentage (0-100), or -1 on error
 */
int display_compare_to_reference(const char *reference_file);

/*
 * Phase 57: Extended Screen and Pixel Query API
 */

/*
 * Read a pixel at screen coordinates.
 * Returns the palette index (pen) at the given position.
 *
 * @param x    X coordinate
 * @param y    Y coordinate
 * @param pen  Output: palette index (0-255)
 * @return true on success, false if out of bounds or no display
 */
bool display_read_pixel(int x, int y, int *pen);

/*
 * Read pixel RGB value at screen coordinates.
 * Looks up the palette color for the pixel.
 *
 * @param x    X coordinate
 * @param y    Y coordinate
 * @param r    Output: red component (0-255)
 * @param g    Output: green component (0-255)
 * @param b    Output: blue component (0-255)
 * @return true on success
 */
bool display_read_pixel_rgb(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b);

/*
 * Get extended screen information.
 *
 * @param width      Output: screen width (can be NULL)
 * @param height     Output: screen height (can be NULL)
 * @param depth      Output: screen depth (can be NULL)
 * @param num_colors Output: number of palette entries (can be NULL)
 * @return true if screen is active
 */
bool display_get_screen_info(int *width, int *height, int *depth, int *num_colors);

#endif /* HAVE_DISPLAY_H */
