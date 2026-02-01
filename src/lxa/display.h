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
 * Set the active display for event routing.
 * @param display  Display to make active
 */
void display_set_active(display_t *display);

#endif /* HAVE_DISPLAY_H */
