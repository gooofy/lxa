/*
 * display.c - Host display subsystem using SDL2
 *
 * Phase 13: Graphics Foundation
 * Phase 15: Rootless Windowing Support
 *
 * This module provides the host-side display rendering for lxa.
 * It creates SDL2 windows and handles the conversion from Amiga
 * planar bitmap format to SDL surfaces.
 *
 * In rootless mode (Phase 15), each Amiga window gets its own SDL window,
 * allowing integration with the host desktop window manager.
 */

#include "display.h"
#include "config.h"
#include "rootless_layout.h"
#include "util.h"
#include "m68k.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <png.h>

/* SDL2 support is optional - check if available */
#ifdef SDL2_FOUND
#include <SDL.h>
#define HAS_SDL2 1
#else
#define HAS_SDL2 0
#endif

/* Display state structure */
struct display_t
{
#if HAS_SDL2
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
#endif
    int           width;
    int           height;
    int           depth;
    uint8_t      *pixels;       /* Chunky pixel buffer (8-bit indexed) */
    uint32_t      palette[DISPLAY_MAX_COLORS];  /* ARGB format for SDL */
    bool          dirty;        /* Needs refresh */
    
    /* Amiga screen bitmap info - for auto-sync from planar RAM */
    uint32_t      amiga_planes_ptr;  /* Pointer to BitMap.Planes[] array in emulated RAM */
    uint32_t      amiga_bpr;         /* Bytes per row in bitmap */
    uint32_t      amiga_depth;       /* Number of bitplanes */
};

/*
 * Phase 15: Rootless window structure
 * Each Amiga window gets its own SDL window in rootless mode.
 */
#define MAX_ROOTLESS_WINDOWS 64

struct display_window_t
{
#if HAS_SDL2
    SDL_Window   *window;
    SDL_Renderer *renderer;
    SDL_Texture  *texture;
    uint32_t      sdl_window_id;  /* For event routing */
#endif
    display_t    *screen;         /* Parent screen (for palette) */
    int           x, y;           /* Position on host desktop */
    int           host_x;
    int           host_y;
    int           width;
    int           height;
    int           logical_width;
    int           logical_height;
    int           depth;
    uint32_t      amiga_window_ptr;
    char          title[256];
    uint8_t      *pixels;         /* Chunky pixel buffer */
    uint32_t      palette[DISPLAY_MAX_COLORS];  /* Local palette if no screen */
    bool          dirty;
    bool          in_use;         /* Slot is active */
};

/* Rootless window tracking */
static display_window_t g_windows[MAX_ROOTLESS_WINDOWS];
static bool g_rootless_mode = false;

extern uint8_t g_ram[];

#define LXA_RAM_SIZE (10 * 1024 * 1024)

/* Global state */
static bool g_display_initialized = false;
static bool g_sdl_available = false;
static bool g_headless_mode = false;  /* Skip SDL window creation for automated testing */
static display_t *g_active_display = NULL;  /* Forward declaration for event routing */
#define EVENT_QUEUE_SIZE 32
static display_event_t g_event_queue[EVENT_QUEUE_SIZE];
static int g_event_queue_head = 0;
static int g_event_queue_tail = 0;
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static int g_last_buttons = 0;  /* Track button state for inject release detection */

typedef struct display_rect_t
{
    int x0;
    int y0;
    int x1;
    int y1;
} display_rect_t;

#define DISPLAY_MAX_VISIBLE_RECTS 64

static uint32_t display_palette_fallback_argb(uint8_t idx)
{
    static const uint32_t fallback_palette[8] = {
        0xFFAAAAAA,
        0xFFFFFFFF,
        0xFF000000,
        0xFF00AACC,
        0xFF2244CC,
        0xFF888888,
        0xFF666666,
        0xFFFF0000,
    };

    if (idx < 8)
        return fallback_palette[idx];

    return 0xFF000000 | ((uint32_t)idx << 16) | ((uint32_t)idx << 8) | idx;
}

static uint32_t display_palette_argb(const uint32_t *palette, uint8_t idx)
{
    uint32_t argb;

    if (!palette)
        return display_palette_fallback_argb(idx);

    argb = palette[idx];
    if (argb == 0)
        return display_palette_fallback_argb(idx);

    return argb;
}

#define DISPLAY_NODE_SUCC_OFFSET 0
#define DISPLAY_SCREEN_FIRSTWINDOW_OFFSET 4
#define DISPLAY_WINDOW_LEFTEDGE_OFFSET 4
#define DISPLAY_WINDOW_TOPEDGE_OFFSET 6
#define DISPLAY_WINDOW_WIDTH_OFFSET 8
#define DISPLAY_WINDOW_HEIGHT_OFFSET 10
#define DISPLAY_WINDOW_WSCREEN_OFFSET 46

static bool display_rect_intersection(const display_rect_t *a,
                                      const display_rect_t *b,
                                      display_rect_t *out)
{
    display_rect_t result;

    if (!a || !b || !out)
    {
        return false;
    }

    result.x0 = (a->x0 > b->x0) ? a->x0 : b->x0;
    result.y0 = (a->y0 > b->y0) ? a->y0 : b->y0;
    result.x1 = (a->x1 < b->x1) ? a->x1 : b->x1;
    result.y1 = (a->y1 < b->y1) ? a->y1 : b->y1;

    if (result.x0 > result.x1 || result.y0 > result.y1)
    {
        return false;
    }

    *out = result;
    return true;
}

static bool display_rect_append(display_rect_t *rects,
                                int *rect_count,
                                int max_rects,
                                int x0,
                                int y0,
                                int x1,
                                int y1)
{
    if (!rects || !rect_count || x0 > x1 || y0 > y1)
    {
        return true;
    }

    if (*rect_count >= max_rects)
    {
        return false;
    }

    rects[*rect_count].x0 = x0;
    rects[*rect_count].y0 = y0;
    rects[*rect_count].x1 = x1;
    rects[*rect_count].y1 = y1;
    (*rect_count)++;
    return true;
}

static void display_rect_subtract(display_rect_t *rects,
                                  int *rect_count,
                                  int max_rects,
                                  const display_rect_t *obscurer)
{
    display_rect_t next_rects[DISPLAY_MAX_VISIBLE_RECTS];
    int next_count = 0;

    if (!rects || !rect_count || !obscurer)
    {
        return;
    }

    for (int i = 0; i < *rect_count; i++)
    {
        display_rect_t intersection;
        display_rect_t current = rects[i];

        if (!display_rect_intersection(&current, obscurer, &intersection))
        {
            if (!display_rect_append(next_rects, &next_count, max_rects,
                                     current.x0, current.y0, current.x1, current.y1))
            {
                return;
            }
            continue;
        }

        if (!display_rect_append(next_rects, &next_count, max_rects,
                                 current.x0, current.y0,
                                 current.x1, intersection.y0 - 1) ||
            !display_rect_append(next_rects, &next_count, max_rects,
                                 current.x0, intersection.y1 + 1,
                                 current.x1, current.y1) ||
            !display_rect_append(next_rects, &next_count, max_rects,
                                 current.x0, intersection.y0,
                                 intersection.x0 - 1, intersection.y1) ||
            !display_rect_append(next_rects, &next_count, max_rects,
                                 intersection.x1 + 1, intersection.y0,
                                 current.x1, intersection.y1))
        {
            return;
        }
    }

    memcpy(rects, next_rects, sizeof(display_rect_t) * (size_t)next_count);
    *rect_count = next_count;
}

static void display_window_copy_screen_rect(display_window_t *window,
                                            const uint8_t *planes[8],
                                            uint32_t bytes_per_row,
                                            uint32_t bitmap_depth,
                                            int src_screen_width,
                                            int src_screen_height,
                                            const display_rect_t *rect)
{
    display_rect_t clipped;

    if (!window || !window->pixels || !rect)
    {
        return;
    }

    clipped = *rect;
    if (clipped.x0 < 0)
    {
        clipped.x0 = 0;
    }
    if (clipped.y0 < 0)
    {
        clipped.y0 = 0;
    }
    if (clipped.x1 >= src_screen_width)
    {
        clipped.x1 = src_screen_width - 1;
    }
    if (clipped.y1 >= src_screen_height)
    {
        clipped.y1 = src_screen_height - 1;
    }
    if (clipped.x0 > clipped.x1 || clipped.y0 > clipped.y1)
    {
        return;
    }

    for (int screen_y = clipped.y0; screen_y <= clipped.y1; screen_y++)
    {
        uint8_t *dst = window->pixels + (size_t)(screen_y - window->host_y) * (size_t)window->width;
        int src_row_offset = screen_y * (int)bytes_per_row;

        for (int screen_x = clipped.x0; screen_x <= clipped.x1; screen_x++)
        {
            int dst_x = screen_x - window->host_x;
            int byte_idx = screen_x / 8;
            int bit_idx = 7 - (screen_x % 8);
            uint8_t pixel = 0;

            for (uint32_t plane = 0; plane < bitmap_depth && plane < 8; plane++)
            {
                if (planes[plane])
                {
                    uint8_t plane_byte = planes[plane][src_row_offset + byte_idx];
                    if (plane_byte & (1 << bit_idx))
                    {
                        pixel |= (1 << plane);
                    }
                }
            }

            dst[dst_x] = pixel;
        }
    }
}

static void display_window_remove_front_window_overlaps(display_window_t *window,
                                                        display_rect_t *rects,
                                                        int *rect_count,
                                                        int max_rects)
{
    uint32_t target_window_ptr;
    uint32_t screen_ptr;
    uint32_t front_window_ptr;

    if (!window || !rects || !rect_count)
    {
        return;
    }

    target_window_ptr = window->amiga_window_ptr;
    if (!target_window_ptr)
    {
        return;
    }

    screen_ptr = m68k_read_memory_32(target_window_ptr + DISPLAY_WINDOW_WSCREEN_OFFSET);
    if (!screen_ptr)
    {
        return;
    }

    front_window_ptr = m68k_read_memory_32(screen_ptr + DISPLAY_SCREEN_FIRSTWINDOW_OFFSET);
    while (front_window_ptr && front_window_ptr != target_window_ptr)
    {
        int obscurer_left = (int16_t)m68k_read_memory_16(front_window_ptr + DISPLAY_WINDOW_LEFTEDGE_OFFSET);
        int obscurer_top = (int16_t)m68k_read_memory_16(front_window_ptr + DISPLAY_WINDOW_TOPEDGE_OFFSET);
        int obscurer_width = (int16_t)m68k_read_memory_16(front_window_ptr + DISPLAY_WINDOW_WIDTH_OFFSET);
        int obscurer_height = (int16_t)m68k_read_memory_16(front_window_ptr + DISPLAY_WINDOW_HEIGHT_OFFSET);
        display_rect_t obscurer;

        if (obscurer_width > 0 && obscurer_height > 0)
        {
            obscurer.x0 = obscurer_left;
            obscurer.y0 = obscurer_top;
            obscurer.x1 = obscurer_left + obscurer_width - 1;
            obscurer.y1 = obscurer_top + obscurer_height - 1;
            display_rect_subtract(rects, rect_count, max_rects, &obscurer);
            if (*rect_count == 0)
            {
                return;
            }
        }

        front_window_ptr = m68k_read_memory_32(front_window_ptr + DISPLAY_NODE_SUCC_OFFSET);
    }
}

static void display_event_set_rootless_coords(display_event_t *event,
                                              uint32_t sdl_window_id,
                                              int local_x,
                                              int local_y)
{
    display_window_t *window;

    if (!event)
    {
        return;
    }

    event->mouse_x = local_x;
    event->mouse_y = local_y;

    window = display_window_from_sdl_id(sdl_window_id);
    if (!window)
    {
        return;
    }

    rootless_layout_screen_coords(window->x,
                                  window->y,
                                  local_x,
                                  local_y,
                                  &event->mouse_x,
                                  &event->mouse_y);
}

#if HAS_SDL2
static uint32_t display_renderer_flags(void)
{
    /* The emulator already paces redraws from its own VBlank timer.
     * Letting SDL block on the host compositor adds avoidable latency,
     * especially in rootless mode where both screen and window presents can stack. */
    return SDL_RENDERER_ACCELERATED;
}
#endif

static void display_window_sync_from_screen(display_window_t *window)
{
    uint32_t planes_ptr;
    uint32_t bpr;
    uint32_t depth;
    int screen_width;
    int screen_height;
    int screen_depth;
    const uint8_t *planes[8] = {0};
    display_rect_t visible_rects[DISPLAY_MAX_VISIBLE_RECTS];
    int visible_rect_count = 0;

    if (!window || !window->in_use || !window->screen || !window->pixels)
    {
        return;
    }

    if (!display_get_amiga_bitmap(window->screen, &planes_ptr, &bpr, &depth))
    {
        return;
    }

    display_get_size(window->screen, &screen_width, &screen_height, &screen_depth);

    if (screen_width <= 0 || screen_height <= 0 || bpr == 0 || depth == 0)
    {
        return;
    }

    for (uint32_t plane = 0; plane < depth && plane < 8; plane++)
    {
        uint32_t plane_addr = m68k_read_memory_32(planes_ptr + plane * 4);
        if (plane_addr && plane_addr < LXA_RAM_SIZE)
        {
            planes[plane] = &g_ram[plane_addr];
        }
    }

    visible_rects[0].x0 = window->host_x;
    visible_rects[0].y0 = window->host_y;
    visible_rects[0].x1 = window->host_x + window->width - 1;
    visible_rects[0].y1 = window->host_y + window->height - 1;
    visible_rect_count = 1;

    display_window_remove_front_window_overlaps(window,
                                                visible_rects,
                                                &visible_rect_count,
                                                DISPLAY_MAX_VISIBLE_RECTS);

    for (int i = 0; i < visible_rect_count; i++)
    {
        display_window_copy_screen_rect(window,
                                        planes,
                                        bpr,
                                        depth,
                                        screen_width,
                                        screen_height,
                                        &visible_rects[i]);
    }

    window->dirty = true;
}

/*
 * Initialize the display subsystem.
 */
bool display_init(void)
{
    if (g_display_initialized)
    {
        return true;
    }

    /* Load rootless mode setting from config */
    g_rootless_mode = config_get_rootless_mode();
    if (g_rootless_mode)
    {
        LPRINTF(LOG_INFO, "display: Rootless mode enabled\n");
    }

    /* Initialize rootless window slots */
    memset(g_windows, 0, sizeof(g_windows));
    g_active_display = NULL;
    memset(g_event_queue, 0, sizeof(g_event_queue));
    g_event_queue_head = 0;
    g_event_queue_tail = 0;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_last_buttons = 0;

#if HAS_SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        LPRINTF(LOG_WARNING, "display: SDL_Init failed: %s\n", SDL_GetError());
        LPRINTF(LOG_WARNING, "display: Graphics support disabled\n");
        g_sdl_available = false;
    }
    else
    {
        LPRINTF(LOG_INFO, "display: SDL2 initialized\n");
        g_sdl_available = true;
    }
#else
    LPRINTF(LOG_INFO, "display: Built without SDL2 support\n");
    g_sdl_available = false;
#endif

    g_display_initialized = true;
    return true;
}

/*
 * Shutdown the display subsystem.
 */
void display_shutdown(void)
{
    if (!g_display_initialized)
    {
        return;
    }

#if HAS_SDL2
    if (g_sdl_available)
    {
        SDL_Quit();
    }
#endif

    g_display_initialized = false;
    g_sdl_available = false;
    g_active_display = NULL;
    memset(g_event_queue, 0, sizeof(g_event_queue));
    g_event_queue_head = 0;
    g_event_queue_tail = 0;
    g_mouse_x = 0;
    g_mouse_y = 0;
    g_last_buttons = 0;
}

/*
 * Check if display is available.
 */
bool display_available(void)
{
    return g_sdl_available;
}

/*
 * Open a display window.
 */
display_t *display_open(int width, int height, int depth, const char *title)
{
    display_t *display;

    if (!g_display_initialized)
    {
        LPRINTF(LOG_ERROR, "display: display_open called before display_init\n");
        return NULL;
    }

    if (width <= 0 || width > DISPLAY_MAX_WIDTH ||
        height <= 0 || height > DISPLAY_MAX_HEIGHT ||
        depth <= 0 || depth > DISPLAY_MAX_DEPTH)
    {
        LPRINTF(LOG_ERROR, "display: invalid dimensions %dx%dx%d\n",
                width, height, depth);
        return NULL;
    }

    display = calloc(1, sizeof(display_t));
    if (!display)
    {
        LPRINTF(LOG_ERROR, "display: out of memory\n");
        return NULL;
    }

    display->width = width;
    display->height = height;
    display->depth = depth;

    /* Allocate pixel buffer */
    display->pixels = calloc(width * height, sizeof(uint8_t));
    if (!display->pixels)
    {
        LPRINTF(LOG_ERROR, "display: out of memory for pixel buffer\n");
        free(display);
        return NULL;
    }

    /* Initialize default palette (grayscale) */
    for (int i = 0; i < DISPLAY_MAX_COLORS; i++)
    {
        uint8_t gray = (i * 255) / (DISPLAY_MAX_COLORS - 1);
        display->palette[i] = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
    }

    /* Set up Amiga-style default colors for low depths */
    if (depth <= 4)
    {
        /* Color 0: Background (light gray/blue) */
        display->palette[0] = 0xFF9999BB;
        /* Color 1: Black */
        display->palette[1] = 0xFF000000;
        /* Color 2: White */
        display->palette[2] = 0xFFFFFFFF;
        /* Color 3: Blue (Amiga highlight) */
        display->palette[3] = 0xFF0055AA;
    }

#if HAS_SDL2
    if (g_sdl_available && !g_headless_mode && !g_rootless_mode)
    {
        const char *window_title = title ? title : "LXA Display";

        display->window = SDL_CreateWindow(
            window_title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (!display->window)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateWindow failed: %s\n",
                    SDL_GetError());
            free(display->pixels);
            free(display);
            return NULL;
        }

        display->renderer = SDL_CreateRenderer(
            display->window, -1,
            display_renderer_flags()
        );

        if (!display->renderer)
        {
            /* Fall back to software renderer */
            display->renderer = SDL_CreateRenderer(display->window, -1, 0);
        }

        if (!display->renderer)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateRenderer failed: %s\n",
                    SDL_GetError());
            SDL_DestroyWindow(display->window);
            free(display->pixels);
            free(display);
            return NULL;
        }

        /* Create streaming texture for pixel updates */
        display->texture = SDL_CreateTexture(
            display->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );

        if (!display->texture)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateTexture failed: %s\n",
                    SDL_GetError());
            SDL_DestroyRenderer(display->renderer);
            SDL_DestroyWindow(display->window);
            free(display->pixels);
            free(display);
            return NULL;
        }

        /* Set logical size for proper scaling */
        SDL_RenderSetLogicalSize(display->renderer, width, height);

        LPRINTF(LOG_INFO, "display: opened %dx%dx%d window '%s'\n",
                width, height, depth, window_title);
    }
    else
    {
        LPRINTF(LOG_INFO, "display: opened %dx%dx%d virtual display (%s)\n",
                width, height, depth,
                g_headless_mode ? "headless" :
                (g_rootless_mode ? "rootless backing store" : "no SDL2"));
    }
#else
    LPRINTF(LOG_INFO, "display: opened %dx%dx%d virtual display (no SDL2)\n",
            width, height, depth);
#endif

    display->dirty = true;
    
    /* Set this as the active display for event routing */
    g_active_display = display;
    
    return display;
}

/*
 * Close a display window.
 */
void display_close(display_t *display)
{
    if (!display)
    {
        return;
    }

    /* Clear active display if this is the one being closed */
    if (g_active_display == display)
    {
        g_active_display = NULL;
    }

#if HAS_SDL2
    if (g_sdl_available)
    {
        if (display->texture)
        {
            SDL_DestroyTexture(display->texture);
        }
        if (display->renderer)
        {
            SDL_DestroyRenderer(display->renderer);
        }
        if (display->window)
        {
            SDL_DestroyWindow(display->window);
        }
    }
#endif

    free(display->pixels);
    free(display);
}

/*
 * Set a palette entry.
 */
void display_set_color(display_t *display, int index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!display || index < 0 || index >= DISPLAY_MAX_COLORS)
    {
        return;
    }

    if (index < 8)
    {
        LPRINTF(LOG_INFO,
                "display: set_color display=%p active=%p idx=%d rgb=(%u,%u,%u)\n",
                (void *)display,
                (void *)g_active_display,
                index,
                (unsigned)r,
                (unsigned)g,
                (unsigned)b);
    }

    /* Store as ARGB */
    display->palette[index] = 0xFF000000 | ((uint32_t)r << 16) |
                              ((uint32_t)g << 8) | (uint32_t)b;
    display->dirty = true;
}

/*
 * Set palette from RGB4 values (4 bits per component, OCS/ECS style).
 */
void display_set_palette_rgb4(display_t *display, int start, int count,
                              const uint16_t *colors)
{
    if (!display || !colors || start < 0)
    {
        return;
    }

    for (int i = 0; i < count && (start + i) < DISPLAY_MAX_COLORS; i++)
    {
        uint16_t rgb4 = colors[i];
        /* Extract 4-bit components and expand to 8-bit */
        uint8_t r = ((rgb4 >> 8) & 0x0F) * 17;  /* 0-15 -> 0-255 */
        uint8_t g = ((rgb4 >> 4) & 0x0F) * 17;
        uint8_t b = (rgb4 & 0x0F) * 17;
        display->palette[start + i] = 0xFF000000 | ((uint32_t)r << 16) |
                                      ((uint32_t)g << 8) | (uint32_t)b;
    }
    display->dirty = true;
}

/*
 * Set palette from RGB32 values (8 bits per component, AGA style).
 */
void display_set_palette_rgb32(display_t *display, int start, int count,
                               const uint32_t *colors)
{
    if (!display || !colors || start < 0)
    {
        return;
    }

    for (int i = 0; i < count && (start + i) < DISPLAY_MAX_COLORS; i++)
    {
        /* Colors are already in a suitable format, just need to repack */
        /* Assume input is 0x00RRGGBB, we need 0xFFRRGGBB (add alpha) */
        display->palette[start + i] = 0xFF000000 | (colors[i] & 0x00FFFFFF);
    }
    display->dirty = true;
}

/*
 * Update display from planar bitmap data.
 * Converts Amiga planar format to chunky 8-bit indexed.
 */
void display_update_planar(display_t *display, int x, int y, int width, int height,
                           const uint8_t **planes, int bytes_per_row, int depth)
{
    if (!display || !planes || x < 0 || y < 0 || width <= 0 || height <= 0)
    {
        return;
    }

    /* Clamp to display bounds */
    if (x + width > display->width)
    {
        width = display->width - x;
    }
    if (y + height > display->height)
    {
        height = display->height - y;
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }

    /* Convert planar to chunky */
    for (int row = 0; row < height; row++)
    {
        uint8_t *dst = display->pixels + (y + row) * display->width + x;
        int src_row_offset = row * bytes_per_row;

        for (int col = 0; col < width; col++)
        {
            int byte_idx = col / 8;
            int bit_idx = 7 - (col % 8);  /* Amiga: MSB is leftmost pixel */
            uint8_t pixel = 0;

            /* Combine bits from all planes */
            for (int plane = 0; plane < depth && plane < 8; plane++)
            {
                if (planes[plane])
                {
                    uint8_t plane_byte = planes[plane][src_row_offset + byte_idx];
                    if (plane_byte & (1 << bit_idx))
                    {
                        pixel |= (1 << plane);
                    }
                }
            }
            dst[col] = pixel;
        }
    }

    display->dirty = true;
}

/*
 * Update display from chunky pixel data.
 */
void display_update_chunky(display_t *display, int x, int y, int width, int height,
                           const uint8_t *pixels, int pitch)
{
    if (!display || !pixels || x < 0 || y < 0 || width <= 0 || height <= 0)
    {
        return;
    }

    /* Clamp to display bounds */
    if (x + width > display->width)
    {
        width = display->width - x;
    }
    if (y + height > display->height)
    {
        height = display->height - y;
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }

    /* Copy pixel data */
    for (int row = 0; row < height; row++)
    {
        uint8_t *dst = display->pixels + (y + row) * display->width + x;
        const uint8_t *src = pixels + row * pitch;
        memcpy(dst, src, width);
    }

    display->dirty = true;
}

/*
 * Refresh the display - convert indexed pixels to ARGB and present.
 */
void display_refresh(display_t *display)
{
    if (!display)
    {
        return;
    }

#if HAS_SDL2
    if (g_sdl_available && display->texture)
    {
        uint32_t *tex_pixels;
        int tex_pitch;

        if (SDL_LockTexture(display->texture, NULL, (void **)&tex_pixels, &tex_pitch) == 0)
        {
            /* Convert indexed pixels to ARGB using palette */
            for (int y = 0; y < display->height; y++)
            {
                uint32_t *dst = (uint32_t *)((uint8_t *)tex_pixels + y * tex_pitch);
                uint8_t *src = display->pixels + y * display->width;

                for (int x = 0; x < display->width; x++)
                {
                    dst[x] = display_palette_argb(display->palette, src[x]);
                }
            }

            SDL_UnlockTexture(display->texture);
        }

        SDL_RenderClear(display->renderer);
        SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
        SDL_RenderPresent(display->renderer);
    }
#endif

    display->dirty = false;
}

/*
 * Get display dimensions.
 */
void display_get_size(display_t *display, int *width, int *height, int *depth)
{
    if (!display)
    {
        if (width) *width = 0;
        if (height) *height = 0;
        if (depth) *depth = 0;
        return;
    }

    if (width) *width = display->width;
    if (height) *height = display->height;
    if (depth) *depth = display->depth;
}

/*
 * Input event queue - simple circular buffer for pending events
 */

/*
 * Add an event to the queue
 */
static void queue_event(const display_event_t *event)
{
    int next_head = (g_event_queue_head + 1) % EVENT_QUEUE_SIZE;
    if (next_head != g_event_queue_tail)  /* Don't overflow */
    {
        g_event_queue[g_event_queue_head] = *event;
        g_event_queue_head = next_head;
        DPRINTF(LOG_DEBUG, "display: queue_event type=%d code=0x%x, head=%d tail=%d\n", 
                event->type, event->button_code, g_event_queue_head, g_event_queue_tail);
    }
    else
    {
        LPRINTF(LOG_WARNING, "display: queue_event OVERFLOW! type=%d rawkey=0x%02x head=%d tail=%d\n",
                event->type, event->rawkey, g_event_queue_head, g_event_queue_tail);
    }
}

/*
 * Get the next event from the queue
 */
bool display_get_event(display_event_t *event)
{
    if (g_event_queue_tail == g_event_queue_head)
    {
        return false;  /* Queue empty */
    }
    
    if (event)
    {
        *event = g_event_queue[g_event_queue_tail];
    }
    DPRINTF(LOG_DEBUG, "display: get_event type=%d rawkey=0x%02x btn=0x%02x head=%d tail=%d\n",
            g_event_queue[g_event_queue_tail].type, g_event_queue[g_event_queue_tail].rawkey,
            g_event_queue[g_event_queue_tail].button_code,
            g_event_queue_head, g_event_queue_tail);
    g_event_queue_tail = (g_event_queue_tail + 1) % EVENT_QUEUE_SIZE;
    return true;
}

/*
 * Get current mouse position
 */
void display_get_mouse_pos(int *x, int *y)
{
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
}

/*
 * Convert SDL keycode to Amiga rawkey code
 * This is a simplified mapping for common keys
 */
#if HAS_SDL2
static int sdl_key_to_rawkey(SDL_Keycode key)
{
    /* Amiga rawkey codes (from devices/rawkeycodes.h) */
    switch (key)
    {
        /* Letters */
        case SDLK_a: return 0x20;
        case SDLK_b: return 0x35;
        case SDLK_c: return 0x33;
        case SDLK_d: return 0x22;
        case SDLK_e: return 0x12;
        case SDLK_f: return 0x23;
        case SDLK_g: return 0x24;
        case SDLK_h: return 0x25;
        case SDLK_i: return 0x17;
        case SDLK_j: return 0x26;
        case SDLK_k: return 0x27;
        case SDLK_l: return 0x28;
        case SDLK_m: return 0x37;
        case SDLK_n: return 0x36;
        case SDLK_o: return 0x18;
        case SDLK_p: return 0x19;
        case SDLK_q: return 0x10;
        case SDLK_r: return 0x13;
        case SDLK_s: return 0x21;
        case SDLK_t: return 0x14;
        case SDLK_u: return 0x16;
        case SDLK_v: return 0x34;
        case SDLK_w: return 0x11;
        case SDLK_x: return 0x32;
        case SDLK_y: return 0x15;
        case SDLK_z: return 0x31;
        
        /* Numbers */
        case SDLK_0: return 0x0A;
        case SDLK_1: return 0x01;
        case SDLK_2: return 0x02;
        case SDLK_3: return 0x03;
        case SDLK_4: return 0x04;
        case SDLK_5: return 0x05;
        case SDLK_6: return 0x06;
        case SDLK_7: return 0x07;
        case SDLK_8: return 0x08;
        case SDLK_9: return 0x09;
        
        /* Function keys */
        case SDLK_F1: return 0x50;
        case SDLK_F2: return 0x51;
        case SDLK_F3: return 0x52;
        case SDLK_F4: return 0x53;
        case SDLK_F5: return 0x54;
        case SDLK_F6: return 0x55;
        case SDLK_F7: return 0x56;
        case SDLK_F8: return 0x57;
        case SDLK_F9: return 0x58;
        case SDLK_F10: return 0x59;
        
        /* Special keys */
        case SDLK_SPACE: return 0x40;
        case SDLK_BACKSPACE: return 0x41;
        case SDLK_TAB: return 0x42;
        case SDLK_RETURN: return 0x44;
        case SDLK_ESCAPE: return 0x45;
        case SDLK_DELETE: return 0x46;
        
        /* Cursor keys */
        case SDLK_UP: return 0x4C;
        case SDLK_DOWN: return 0x4D;
        case SDLK_RIGHT: return 0x4E;
        case SDLK_LEFT: return 0x4F;
        
        /* Modifiers */
        case SDLK_LSHIFT: return 0x60;
        case SDLK_RSHIFT: return 0x61;
        case SDLK_CAPSLOCK: return 0x62;
        case SDLK_LCTRL: return 0x63;
        case SDLK_LALT: return 0x64;
        case SDLK_RALT: return 0x65;
        case SDLK_LGUI: return 0x66;  /* Left Amiga */
        case SDLK_RGUI: return 0x67;  /* Right Amiga */
        
        default: return 0xFF;  /* Unknown key */
    }
}

/*
 * Convert SDL key modifiers to Amiga qualifier bits
 */
static int sdl_mod_to_qualifier(uint16_t mod)
{
    int qual = 0;
    
    /* IEQUALIFIER bits from devices/inputevent.h */
    if (mod & KMOD_LSHIFT) qual |= 0x0001;  /* IEQUALIFIER_LSHIFT */
    if (mod & KMOD_RSHIFT) qual |= 0x0002;  /* IEQUALIFIER_RSHIFT */
    if (mod & KMOD_CAPS)   qual |= 0x0004;  /* IEQUALIFIER_CAPSLOCK */
    if (mod & KMOD_CTRL)   qual |= 0x0008;  /* IEQUALIFIER_CONTROL */
    if (mod & KMOD_LALT)   qual |= 0x0010;  /* IEQUALIFIER_LALT */
    if (mod & KMOD_RALT)   qual |= 0x0020;  /* IEQUALIFIER_RALT */
    if (mod & KMOD_LGUI)   qual |= 0x0040;  /* IEQUALIFIER_LCOMMAND */
    if (mod & KMOD_RGUI)   qual |= 0x0080;  /* IEQUALIFIER_RCOMMAND */
    
    return qual;
}
#endif

/*
 * Process SDL events.
 * Returns true if quit was requested.
 * Also queues events for IDCMP processing.
 */
bool display_poll_events(void)
{
#if HAS_SDL2
    /* In headless mode, skip SDL event polling but don't return early -
     * there may be injected events in the queue that need to be processed */
    if (g_sdl_available && !g_headless_mode)
    {

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        display_event_t disp_event = {0};
        disp_event.window = g_active_display;
        
        switch (event.type)
        {
            case SDL_QUIT:
                disp_event.type = DISPLAY_EVENT_QUIT;
                queue_event(&disp_event);
                return true;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                {
                    disp_event.type = DISPLAY_EVENT_CLOSEWINDOW;
                    queue_event(&disp_event);
                }
                break;

            case SDL_MOUSEMOTION:
                disp_event.type = DISPLAY_EVENT_MOUSEMOVE;
                disp_event.qualifier = sdl_mod_to_qualifier(SDL_GetModState());

                if (g_rootless_mode)
                {
                    display_event_set_rootless_coords(&disp_event,
                                                      event.motion.windowID,
                                                      event.motion.x,
                                                      event.motion.y);
                }
                else
                {
                    disp_event.mouse_x = event.motion.x;
                    disp_event.mouse_y = event.motion.y;
                }

                g_mouse_x = disp_event.mouse_x;
                g_mouse_y = disp_event.mouse_y;
                queue_event(&disp_event);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                disp_event.type = DISPLAY_EVENT_MOUSEBUTTON;
                disp_event.button_down = (event.type == SDL_MOUSEBUTTONDOWN);
                disp_event.qualifier = sdl_mod_to_qualifier(SDL_GetModState());

                if (g_rootless_mode)
                {
                    display_event_set_rootless_coords(&disp_event,
                                                      event.button.windowID,
                                                      event.button.x,
                                                      event.button.y);
                }
                else
                {
                    disp_event.mouse_x = event.button.x;
                    disp_event.mouse_y = event.button.y;
                }

                g_mouse_x = disp_event.mouse_x;
                g_mouse_y = disp_event.mouse_y;
                
                /* Map SDL button to Amiga IECODE */
                /* IECODE_LBUTTON = 0x68, IECODE_RBUTTON = 0x69, IECODE_MBUTTON = 0x6A */
                /* IECODE_UP_PREFIX = 0x80 (added when button released) */
                switch (event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                        disp_event.button_code = 0x68;
                        break;
                    case SDL_BUTTON_RIGHT:
                        disp_event.button_code = 0x69;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        disp_event.button_code = 0x6A;
                        break;
                    default:
                        disp_event.button_code = 0x68;
                        break;
                }
                if (!disp_event.button_down)
                {
                    disp_event.button_code |= 0x80;  /* IECODE_UP_PREFIX */
                }
                
                /* Set qualifier bits for button state */
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    if (disp_event.button_down)
                        disp_event.qualifier |= 0x8000;  /* IEQUALIFIER_LEFTBUTTON */
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    if (disp_event.button_down)
                        disp_event.qualifier |= 0x4000;  /* IEQUALIFIER_RBUTTON */
                }
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                {
                    if (disp_event.button_down)
                        disp_event.qualifier |= 0x2000;  /* IEQUALIFIER_MIDBUTTON */
                }
                
                queue_event(&disp_event);
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                disp_event.type = DISPLAY_EVENT_KEY;
                disp_event.rawkey = sdl_key_to_rawkey(event.key.keysym.sym);
                disp_event.button_down = (event.type == SDL_KEYDOWN);
                disp_event.qualifier = sdl_mod_to_qualifier(event.key.keysym.mod);
                disp_event.mouse_x = g_mouse_x;
                disp_event.mouse_y = g_mouse_y;
                
                /* Add IECODE_UP_PREFIX for key release */
                if (!disp_event.button_down)
                {
                    disp_event.rawkey |= 0x80;
                }
                
                /* Check for escape to quit */
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    return true;
                }
                
                queue_event(&disp_event);
                break;
        }
    }
    }  /* end if (g_sdl_available && !g_headless_mode) */
#endif

    return false;
}

/*
 * Set the active display (for event routing)
 * Called when a display is opened or focused
 */
void display_set_active(display_t *display)
{
    g_active_display = display;
}

/*
 * ============================================================================
 * Phase 15: Rootless Windowing Implementation
 * ============================================================================
 */

/*
 * Check if rootless mode is enabled.
 */
bool display_get_rootless_mode(void)
{
    return g_rootless_mode;
}

/*
 * Find a free window slot.
 */
static display_window_t *find_free_window_slot(void)
{
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (!g_windows[i].in_use)
        {
            return &g_windows[i];
        }
    }
    return NULL;
}

static bool display_window_reallocate_backing_store(display_window_t *window,
                                                    int host_width,
                                                    int host_height)
{
    uint8_t *new_pixels;

    if (!window || !window->in_use)
    {
        return false;
    }

    if (host_width <= 0 || host_width > DISPLAY_MAX_WIDTH ||
        host_height <= 0 || host_height > DISPLAY_MAX_HEIGHT)
    {
        return false;
    }

    if (host_width == window->width && host_height == window->height)
    {
        return true;
    }

    new_pixels = calloc(host_width * host_height, sizeof(uint8_t));
    if (!new_pixels)
    {
        LPRINTF(LOG_ERROR, "display: out of memory for resized window\n");
        return false;
    }

    if (window->pixels)
    {
        int copy_w = (host_width < window->width) ? host_width : window->width;
        int copy_h = (host_height < window->height) ? host_height : window->height;

        for (int row = 0; row < copy_h; row++)
        {
            memcpy(new_pixels + row * host_width,
                   window->pixels + row * window->width,
                   copy_w);
        }

        free(window->pixels);
    }

    window->pixels = new_pixels;
    window->width = host_width;
    window->height = host_height;

#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        SDL_SetWindowSize(window->window, host_width, host_height);

        if (window->texture)
        {
            SDL_DestroyTexture(window->texture);
        }

        window->texture = SDL_CreateTexture(
            window->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            host_width, host_height
        );
        SDL_RenderSetLogicalSize(window->renderer, host_width, host_height);
    }
#endif

    window->dirty = true;
    return true;
}

static int display_window_host_width(display_window_t *window, int logical_width)
{
    int screen_width = 0;
    bool widen_for_host_menu = true;

    if (window && window->screen)
    {
        screen_width = window->screen->width;
    }

    return rootless_layout_host_width(screen_width,
                                      window ? window->x : 0,
                                      logical_width,
                                      widen_for_host_menu);
}

static int display_window_host_height(display_window_t *window, int logical_height)
{
    return rootless_layout_host_height(window ? window->y : 0,
                                       logical_height);
}

static bool display_window_apply_host_extent(display_window_t *window)
{
    int host_width;
    int host_height;

    if (!window || !window->in_use)
    {
        return false;
    }

    window->host_x = window->x;
    window->host_y = rootless_layout_host_origin_y(window->y);
    host_width = display_window_host_width(window, window->logical_width);
    host_height = display_window_host_height(window, window->logical_height);
    return display_window_reallocate_backing_store(window,
                                                   host_width,
                                                   host_height);
}

/*
 * Open a rootless window.
 */
display_window_t *display_window_open(display_t *screen, int x, int y,
                                       int width, int height, int depth,
                                       const char *title)
{
    display_window_t *win;

    if (!g_display_initialized)
    {
        LPRINTF(LOG_ERROR, "display: display_window_open called before display_init\n");
        return NULL;
    }

    if (width <= 0 || width > DISPLAY_MAX_WIDTH ||
        height <= 0 || height > DISPLAY_MAX_HEIGHT ||
        depth <= 0 || depth > DISPLAY_MAX_DEPTH)
    {
        LPRINTF(LOG_ERROR, "display: invalid window dimensions %dx%dx%d\n",
                width, height, depth);
        return NULL;
    }

    win = find_free_window_slot();
    if (!win)
    {
        LPRINTF(LOG_ERROR, "display: no free window slots (max %d)\n",
                MAX_ROOTLESS_WINDOWS);
        return NULL;
    }

    memset(win, 0, sizeof(*win));
    win->screen = screen;
    win->x = x;
    win->y = y;
    win->host_x = x;
    win->host_y = rootless_layout_host_origin_y(y);
    win->logical_width = width;
    win->logical_height = height;
    win->width = display_window_host_width(win, width);
    win->height = display_window_host_height(win, height);
    win->depth = depth;

    /* Validate computed host dimensions against bounds.
     * The logical dimensions have already been checked, but the host
     * extent may exceed DISPLAY_MAX_WIDTH/HEIGHT after layout expansion. */
    if (win->width <= 0 || win->width > DISPLAY_MAX_WIDTH ||
        win->height <= 0 || win->height > DISPLAY_MAX_HEIGHT)
    {
        LPRINTF(LOG_ERROR, "display: computed host dimensions %dx%d exceed limits for window '%s'\n",
                win->width, win->height, title ? title : "(null)");
        win->in_use = false;
        return NULL;
    }

    win->in_use = true;
    win->amiga_window_ptr = 0;
    if (title)
    {
        snprintf(win->title, sizeof(win->title), "%s", title);
    }
    else
    {
        snprintf(win->title, sizeof(win->title), "%s", "LXA Window");
    }

    /* Allocate pixel buffer */
    win->pixels = calloc(win->width * win->height, sizeof(uint8_t));
    if (!win->pixels)
    {
        LPRINTF(LOG_ERROR, "display: out of memory for window pixel buffer\n");
        win->in_use = false;
        return NULL;
    }

    /* Copy palette from screen or initialize default */
    if (screen)
    {
        memcpy(win->palette, screen->palette, sizeof(win->palette));
    }
    else
    {
        /* Initialize default grayscale palette */
        for (int i = 0; i < DISPLAY_MAX_COLORS; i++)
        {
            uint8_t gray = (i * 255) / (DISPLAY_MAX_COLORS - 1);
            win->palette[i] = 0xFF000000 | (gray << 16) | (gray << 8) | gray;
        }
        /* Set Amiga-style defaults */
        if (depth <= 4)
        {
            win->palette[0] = 0xFF9999BB;  /* Background */
            win->palette[1] = 0xFF000000;  /* Black */
            win->palette[2] = 0xFFFFFFFF;  /* White */
            win->palette[3] = 0xFF0055AA;  /* Blue */
        }
    }

#if HAS_SDL2
    if (g_sdl_available && !g_headless_mode)
    {
        const char *window_title = title ? title : "LXA Window";

        win->window = SDL_CreateWindow(
            window_title,
            (win->host_x >= 0) ? win->host_x : SDL_WINDOWPOS_CENTERED,
            (win->host_y >= 0) ? win->host_y : SDL_WINDOWPOS_CENTERED,
            win->width, win->height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (!win->window)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateWindow failed: %s\n",
                    SDL_GetError());
            free(win->pixels);
            win->in_use = false;
            return NULL;
        }

        win->sdl_window_id = SDL_GetWindowID(win->window);

        win->renderer = SDL_CreateRenderer(
            win->window, -1,
            display_renderer_flags()
        );

        if (!win->renderer)
        {
            /* Fall back to software renderer */
            win->renderer = SDL_CreateRenderer(win->window, -1, 0);
        }

        if (!win->renderer)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateRenderer failed: %s\n",
                    SDL_GetError());
            SDL_DestroyWindow(win->window);
            free(win->pixels);
            win->in_use = false;
            return NULL;
        }

        /* Create streaming texture for pixel updates */
        win->texture = SDL_CreateTexture(
            win->renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            win->width, win->height
        );

        if (!win->texture)
        {
            LPRINTF(LOG_ERROR, "display: SDL_CreateTexture failed: %s\n",
                    SDL_GetError());
            SDL_DestroyRenderer(win->renderer);
            SDL_DestroyWindow(win->window);
            free(win->pixels);
            win->in_use = false;
            return NULL;
        }

        /* Set logical size for proper scaling */
        SDL_RenderSetLogicalSize(win->renderer, win->width, win->height);

        LPRINTF(LOG_INFO, "display: opened rootless window %dx%d '%s' (SDL ID %u)\n",
                win->width, win->height, window_title, win->sdl_window_id);
    }
    else
    {
        LPRINTF(LOG_INFO, "display: opened virtual window %dx%d (no SDL2)\n",
                win->width, win->height);
    }
#else
    LPRINTF(LOG_INFO, "display: opened virtual window %dx%d (no SDL2)\n",
            win->width, win->height);
#endif

    win->dirty = true;
    return win;
}

/*
 * Close a rootless window.
 */
void display_window_close(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return;
    }

#if HAS_SDL2
    if (g_sdl_available)
    {
        if (window->texture)
        {
            SDL_DestroyTexture(window->texture);
        }
        if (window->renderer)
        {
            SDL_DestroyRenderer(window->renderer);
        }
        if (window->window)
        {
            SDL_DestroyWindow(window->window);
        }
    }
#endif

    free(window->pixels);
    memset(window, 0, sizeof(*window));
    /* in_use is already false from memset */
}

/*
 * Move a rootless window.
 */
bool display_window_move(display_window_t *window, int x, int y)
{
    if (!window || !window->in_use)
    {
        return false;
    }

    window->x = x;
    window->y = y;

    if (!display_window_apply_host_extent(window))
    {
        return false;
    }

#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        SDL_SetWindowPosition(window->window, window->host_x, window->host_y);
    }
#endif

    return true;
}

/*
 * Resize a rootless window.
 */
bool display_window_size(display_window_t *window, int width, int height)
{
    if (!window || !window->in_use)
    {
        return false;
    }

    if (width <= 0 || width > DISPLAY_MAX_WIDTH ||
        height <= 0 || height > DISPLAY_MAX_HEIGHT)
    {
        return false;
    }

    window->logical_width = width;
    window->logical_height = height;

    if (!display_window_apply_host_extent(window))
    {
        return false;
    }

    window->dirty = true;
    return true;
}

/*
 * Bring a rootless window to front.
 */
bool display_window_to_front(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return false;
    }

#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        SDL_RaiseWindow(window->window);
    }
#endif

    return true;
}

/*
 * Send a rootless window to back.
 */
bool display_window_to_back(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return false;
    }

    /* SDL doesn't have a direct "send to back" function,
     * but we can minimize and restore, or just leave it */
#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        /* Lower the window in the stacking order */
        /* Note: SDL2 doesn't have SDL_LowerWindow, so this is a no-op
         * for now. In practice, clicking another window achieves this. */
    }
#endif

    return true;
}

/*
 * Set a rootless window title.
 */
bool display_window_set_title(display_window_t *window, const char *title)
{
    if (!window || !window->in_use)
    {
        return false;
    }

#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        SDL_SetWindowTitle(window->window, title ? title : "LXA Window");
    }
#endif

    snprintf(window->title, sizeof(window->title), "%s", title ? title : "LXA Window");

    return true;
}

bool display_window_attach_amiga_window(display_window_t *window, uint32_t amiga_window_ptr)
{
    if (!window || !window->in_use)
    {
        return false;
    }

    window->amiga_window_ptr = amiga_window_ptr;

    return true;
}

/*
 * Refresh a rootless window.
 */
void display_window_refresh(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return;
    }

#if HAS_SDL2
    if (g_sdl_available && window->texture)
    {
        uint32_t *tex_pixels;
        int tex_pitch;
        const uint32_t *palette;

        /* Use screen palette if available, otherwise local */
        if (window->screen)
        {
            palette = window->screen->palette;
        }
        else
        {
            palette = window->palette;
        }

        if (SDL_LockTexture(window->texture, NULL, (void **)&tex_pixels, &tex_pitch) == 0)
        {
            /* Convert indexed pixels to ARGB using palette */
            for (int y = 0; y < window->height; y++)
            {
                uint32_t *dst = (uint32_t *)((uint8_t *)tex_pixels + y * tex_pitch);
                uint8_t *src = window->pixels + y * window->width;

                for (int x = 0; x < window->width; x++)
                {
                    dst[x] = display_palette_argb(palette, src[x]);
                }
            }

            SDL_UnlockTexture(window->texture);
        }

        SDL_RenderClear(window->renderer);
        SDL_RenderCopy(window->renderer, window->texture, NULL, NULL);
        SDL_RenderPresent(window->renderer);
    }
#endif

    window->dirty = false;
}

/*
 * Update rootless window from planar bitmap data.
 */
void display_window_update_planar(display_window_t *window, int x, int y,
                                   int width, int height,
                                   const uint8_t **planes, int bytes_per_row, int depth)
{
    if (!window || !window->in_use || !planes ||
        x < 0 || y < 0 || width <= 0 || height <= 0)
    {
        return;
    }

    /* Clamp to window bounds */
    if (x + width > window->width)
    {
        width = window->width - x;
    }
    if (y + height > window->height)
    {
        height = window->height - y;
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }

    /* Convert planar to chunky */
    for (int row = 0; row < height; row++)
    {
        uint8_t *dst = window->pixels + (y + row) * window->width + x;
        int src_row_offset = row * bytes_per_row;

        for (int col = 0; col < width; col++)
        {
            int byte_idx = col / 8;
            int bit_idx = 7 - (col % 8);  /* Amiga: MSB is leftmost pixel */
            uint8_t pixel = 0;

            /* Combine bits from all planes */
            for (int plane = 0; plane < depth && plane < 8; plane++)
            {
                if (planes[plane])
                {
                    uint8_t plane_byte = planes[plane][src_row_offset + byte_idx];
                    if (plane_byte & (1 << bit_idx))
                    {
                        pixel |= (1 << plane);
                    }
                }
            }
            dst[col] = pixel;
        }
    }

    window->dirty = true;
}

/*
 * Get the screen associated with a rootless window.
 */
display_t *display_window_get_screen(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return NULL;
    }
    return window->screen;
}

/*
 * Look up a window by its SDL window ID.
 */
display_window_t *display_window_from_sdl_id(uint32_t sdl_window_id)
{
#if HAS_SDL2
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use && g_windows[i].sdl_window_id == sdl_window_id)
        {
            return &g_windows[i];
        }
    }
#else
    (void)sdl_window_id;
#endif
    return NULL;
}

/*
 * Get the SDL window ID for a display window.
 */
uint32_t display_window_get_sdl_id(display_window_t *window)
{
    if (!window || !window->in_use)
    {
        return 0;
    }
#if HAS_SDL2
    return window->sdl_window_id;
#else
    return 0;
#endif
}

/*
 * Refresh all open displays and rootless windows.
 * Called from main loop at VBlank time to ensure screen updates.
 */
void display_refresh_all(void)
{
    /* Refresh the active screen display */
    if (g_active_display)
    {
        display_refresh(g_active_display);
    }

#if HAS_SDL2
    /* Refresh all rootless windows */
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            display_window_sync_from_screen(&g_windows[i]);
            display_window_refresh(&g_windows[i]);
        }
    }
#endif
}

/*
 * Set the Amiga bitmap info for automatic screen refresh.
 * This stores the bitmap parameters for later use by display_refresh_all().
 * 
 * @param display     Display handle
 * @param planes_ptr  Address of BitMap.Planes[] array in emulated RAM
 * @param bpr_depth   Packed value: (bytes_per_row << 16) | depth
 */
void display_set_amiga_bitmap(display_t *display, uint32_t planes_ptr, uint32_t bpr_depth)
{
    if (!display)
        return;

    LPRINTF(LOG_INFO,
            "display: set_amiga_bitmap display=%p active=%p planes=0x%08x bpr=%u depth=%u\n",
            (void *)display,
            (void *)g_active_display,
            planes_ptr,
            (unsigned)((bpr_depth >> 16) & 0xFFFF),
            (unsigned)(bpr_depth & 0xFFFF));
    
    display->amiga_planes_ptr = planes_ptr;
    display->amiga_bpr = (bpr_depth >> 16) & 0xFFFF;
    display->amiga_depth = bpr_depth & 0xFFFF;
}

/*
 * Get the active display (for VBlank refresh).
 */
display_t *display_get_active(void)
{
    return g_active_display;
}

/*
 * Get the Amiga bitmap info for a display.
 */
bool display_get_amiga_bitmap(display_t *display, uint32_t *planes_ptr, uint32_t *bpr, uint32_t *depth)
{
    if (!display || display->amiga_planes_ptr == 0)
        return false;
    
    if (planes_ptr) *planes_ptr = display->amiga_planes_ptr;
    if (bpr) *bpr = display->amiga_bpr;
    if (depth) *depth = display->amiga_depth;
    return true;
}

/*
 * ============================================================================
 * Phase 21: UI Testing Infrastructure
 * ============================================================================
 */

/*
 * Convert ASCII character to Amiga rawkey code.
 * Returns 0xFF if no mapping exists.
 */
int ascii_to_rawkey(char c, bool *need_shift)
{
    *need_shift = false;
    
    /* Lowercase letters */
    if (c >= 'a' && c <= 'z')
    {
        static const int letter_codes[] = {
            0x20, /* a */ 0x35, /* b */ 0x33, /* c */ 0x22, /* d */
            0x12, /* e */ 0x23, /* f */ 0x24, /* g */ 0x25, /* h */
            0x17, /* i */ 0x26, /* j */ 0x27, /* k */ 0x28, /* l */
            0x37, /* m */ 0x36, /* n */ 0x18, /* o */ 0x19, /* p */
            0x10, /* q */ 0x13, /* r */ 0x21, /* s */ 0x14, /* t */
            0x16, /* u */ 0x34, /* v */ 0x11, /* w */ 0x32, /* x */
            0x15, /* y */ 0x31  /* z */
        };
        return letter_codes[c - 'a'];
    }
    
    /* Uppercase letters (same codes, but need shift) */
    if (c >= 'A' && c <= 'Z')
    {
        *need_shift = true;
        static const int letter_codes[] = {
            0x20, /* A */ 0x35, /* B */ 0x33, /* C */ 0x22, /* D */
            0x12, /* E */ 0x23, /* F */ 0x24, /* G */ 0x25, /* H */
            0x17, /* I */ 0x26, /* J */ 0x27, /* K */ 0x28, /* L */
            0x37, /* M */ 0x36, /* N */ 0x18, /* O */ 0x19, /* P */
            0x10, /* Q */ 0x13, /* R */ 0x21, /* S */ 0x14, /* T */
            0x16, /* U */ 0x34, /* V */ 0x11, /* W */ 0x32, /* X */
            0x15, /* Y */ 0x31  /* Z */
        };
        return letter_codes[c - 'A'];
    }
    
    /* Numbers */
    if (c >= '0' && c <= '9')
    {
        if (c == '0') return 0x0A;
        return 0x01 + (c - '1');  /* '1' = 0x01, '2' = 0x02, etc. */
    }
    
    /* Special characters */
    switch (c)
    {
        case ' ':  return 0x40;  /* Space */
        case '\n': return 0x44;  /* Return */
        case '\r': return 0x44;  /* Return */
        case '\t': return 0x42;  /* Tab */
        case '\b': return 0x41;  /* Backspace */
        case 0x7F: return 0x46;  /* Delete */
        case '-':  return 0x0B;  /* Minus */
        case '=':  return 0x0C;  /* Equals */
        case '[':  return 0x1A;  /* Left bracket */
        case ']':  return 0x1B;  /* Right bracket */
        case ';':  return 0x29;  /* Semicolon */
        case '\'': return 0x2A;  /* Apostrophe */
        case '`':  return 0x00;  /* Backtick */
        case '\\': return 0x0D;  /* Backslash */
        case ',':  return 0x38;  /* Comma */
        case '.':  return 0x39;  /* Period */
        case '/':  return 0x3A;  /* Slash */
        
        /* Shifted characters */
        case '!':  *need_shift = true; return 0x01;  /* Shift+1 */
        case '@':  *need_shift = true; return 0x02;  /* Shift+2 */
        case '#':  *need_shift = true; return 0x03;  /* Shift+3 */
        case '$':  *need_shift = true; return 0x04;  /* Shift+4 */
        case '%':  *need_shift = true; return 0x05;  /* Shift+5 */
        case '^':  *need_shift = true; return 0x06;  /* Shift+6 */
        case '&':  *need_shift = true; return 0x07;  /* Shift+7 */
        case '*':  *need_shift = true; return 0x08;  /* Shift+8 */
        case '(':  *need_shift = true; return 0x09;  /* Shift+9 */
        case ')':  *need_shift = true; return 0x0A;  /* Shift+0 */
        case '_':  *need_shift = true; return 0x0B;  /* Shift+- */
        case '+':  *need_shift = true; return 0x0C;  /* Shift+= */
        case '{':  *need_shift = true; return 0x1A;  /* Shift+[ */
        case '}':  *need_shift = true; return 0x1B;  /* Shift+] */
        case ':':  *need_shift = true; return 0x29;  /* Shift+; */
        case '"':  *need_shift = true; return 0x2A;  /* Shift+' */
        case '~':  *need_shift = true; return 0x00;  /* Shift+` */
        case '|':  *need_shift = true; return 0x0D;  /* Shift+\ */
        case '<':  *need_shift = true; return 0x38;  /* Shift+, */
        case '>':  *need_shift = true; return 0x39;  /* Shift+. */
        case '?':  *need_shift = true; return 0x3A;  /* Shift+/ */
        
        default:
            return 0xFF;  /* No mapping */
    }
}

/*
 * Inject a keyboard event into the event queue.
 */
bool display_inject_key(int rawkey, int qualifier, bool down)
{
    display_event_t event = {0};
    
    event.type = DISPLAY_EVENT_KEY;
    event.rawkey = down ? (rawkey & 0x7F) : (rawkey | 0x80);
    event.qualifier = qualifier;
    event.button_down = down;
    event.window = g_active_display;
    
    /* Get current mouse position */
    display_get_mouse_pos(&event.mouse_x, &event.mouse_y);
    
    /* Add to event queue */
    queue_event(&event);
    
    LPRINTF(LOG_DEBUG, "display: inject_key rawkey=0x%02x qual=0x%04x down=%d\n",
            rawkey, qualifier, down);

    lxa_host_console_enqueue_rawkey(rawkey, qualifier, down);

    return true;
}

/*
 * Inject a string as a sequence of key events.
 */
bool display_inject_string(const char *str)
{
    if (!str)
        return false;
    
    LPRINTF(LOG_DEBUG, "display: inject_string '%s'\n", str);
    
    for (const char *p = str; *p; p++)
    {
        bool need_shift = false;
        int rawkey = ascii_to_rawkey(*p, &need_shift);
        
        if (rawkey == 0xFF)
        {
            LPRINTF(LOG_DEBUG, "display: inject_string: no mapping for '%c' (0x%02x)\n",
                    *p, (unsigned char)*p);
            continue;  /* Skip unmappable characters */
        }
        
        int qualifier = need_shift ? 0x0001 : 0;  /* IEQUALIFIER_LSHIFT */
        
        DPRINTF(LOG_DEBUG, "display: inject_string: char='%c' rawkey=0x%02x qual=0x%04x head=%d tail=%d\n",
                *p, rawkey, qualifier, g_event_queue_head, g_event_queue_tail);
        
        /* Key down */
        display_inject_key(rawkey, qualifier, true);
        
        /* Key up */
        display_inject_key(rawkey, qualifier, false);
    }
    
    return true;
}

/*
 * Inject a mouse event into the event queue.
 */
bool display_inject_mouse(int x, int y, int buttons, display_event_type_t event_type)
{
    display_event_t event = {0};
    
    event.type = event_type;
    event.mouse_x = x;
    event.mouse_y = y;
    event.window = g_active_display;
    
    if (event_type == DISPLAY_EVENT_MOUSEBUTTON)
    {
        /* Detect button changes by comparing with last state */
        int pressed = buttons & ~g_last_buttons;   /* Newly pressed buttons */
        int released = g_last_buttons & ~buttons;  /* Newly released buttons */
        
        LPRINTF(LOG_DEBUG, "display: inject_mouse button: last=0x%x cur=0x%x pressed=0x%x released=0x%x\n",
                g_last_buttons, buttons, pressed, released);
        
        if (pressed)
        {
            /* Button pressed */
            if (pressed & 0x01)
            {
                event.button_code = 0x68;  /* IECODE_LBUTTON */
                event.qualifier = 0x8000;   /* IEQUALIFIER_LEFTBUTTON */
            }
            else if (pressed & 0x02)
            {
                event.button_code = 0x69;  /* IECODE_RBUTTON */
                event.qualifier = 0x4000;   /* IEQUALIFIER_RBUTTON */
            }
            else if (pressed & 0x04)
            {
                event.button_code = 0x6A;  /* IECODE_MBUTTON */
                event.qualifier = 0x2000;   /* IEQUALIFIER_MIDBUTTON */
            }
            event.button_down = true;
        }
        else if (released)
        {
            /* Button released - add IECODE_UP_PREFIX (0x80) */
            if (released & 0x01)
            {
                event.button_code = 0x68 | 0x80;  /* IECODE_LBUTTON | UP */
            }
            else if (released & 0x02)
            {
                event.button_code = 0x69 | 0x80;  /* IECODE_RBUTTON | UP */
            }
            else if (released & 0x04)
            {
                event.button_code = 0x6A | 0x80;  /* IECODE_MBUTTON | UP */
            }
            event.button_down = false;
            /* Keep qualifier showing buttons still held */
            if (buttons & 0x01) event.qualifier |= 0x8000;
            if (buttons & 0x02) event.qualifier |= 0x4000;
            if (buttons & 0x04) event.qualifier |= 0x2000;
        }
        
        /* Update button state tracking */
        g_last_buttons = buttons;
    }
    
    queue_event(&event);
    
    /* Update internal mouse position */
    g_mouse_x = x;
    g_mouse_y = y;
    
    LPRINTF(LOG_DEBUG, "display: inject_mouse x=%d y=%d buttons=0x%x type=%d code=0x%x down=%d\n",
            x, y, buttons, event_type, event.button_code, event.button_down);
    
    return true;
}

/*
 * Set headless mode.
 */
bool display_set_headless(bool enable)
{
    bool previous = g_headless_mode;
    g_headless_mode = enable;
    
    LPRINTF(LOG_INFO, "display: headless mode %s\n", enable ? "enabled" : "disabled");
    
    return previous;
}

/*
 * Check if headless mode is enabled.
 */
bool display_get_headless(void)
{
    return g_headless_mode;
}

/*
 * Check if the event queue is empty.
 */
bool display_event_queue_empty(void)
{
    return g_event_queue_tail == g_event_queue_head;
}

static bool display_write_png_file(const char *filename,
                                   int width,
                                   int height,
                                   const uint8_t *pixels,
                                   const uint32_t *palette)
{
    FILE *f = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep row = NULL;
    bool success = false;

    if (!filename || width <= 0 || height <= 0 || !pixels || !palette)
        return false;

    f = fopen(filename, "wb");
    if (!f)
    {
        LPRINTF(LOG_ERROR, "display: failed to open '%s' for writing\n", filename);
        return false;
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
    {
        LPRINTF(LOG_ERROR, "display: failed to create PNG write struct\n");
        goto cleanup;
    }

    info = png_create_info_struct(png);
    if (!info)
    {
        LPRINTF(LOG_ERROR, "display: failed to create PNG info struct\n");
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(png)))
    {
        LPRINTF(LOG_ERROR, "display: PNG write failed for '%s'\n", filename);
        goto cleanup;
    }

    row = (png_bytep)malloc((size_t)width * 3u);
    if (!row)
    {
        LPRINTF(LOG_ERROR, "display: failed to allocate PNG row buffer\n");
        goto cleanup;
    }

    png_init_io(png, f);
    png_set_IHDR(png,
                 info,
                 width,
                 height,
                 8,
                 PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            size_t pixel_index = (size_t)y * (size_t)width + (size_t)x;
            uint8_t idx = pixels[pixel_index];
            uint32_t color = display_palette_argb(palette, idx);
            size_t rgb_index = (size_t)x * 3u;

            row[rgb_index] = (uint8_t)((color >> 16) & 0xFF);
            row[rgb_index + 1] = (uint8_t)((color >> 8) & 0xFF);
            row[rgb_index + 2] = (uint8_t)(color & 0xFF);
        }

        png_write_row(png, row);
    }

    png_write_end(png, NULL);
    success = true;

cleanup:
    if (row)
        free(row);
    if (png || info)
        png_destroy_write_struct(&png, &info);
    if (f)
        fclose(f);
    if (!success)
        remove(filename);

    return success;
}

static bool display_read_png_file(const char *filename,
                                  int *width,
                                  int *height,
                                  uint8_t **pixels)
{
    FILE *f = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *rows = NULL;
    uint8_t *data = NULL;
    png_byte header[8];
    int image_width;
    int image_height;
    png_size_t rowbytes;
    bool success = false;

    if (!filename || !width || !height || !pixels)
        return false;

    *width = 0;
    *height = 0;
    *pixels = NULL;

    f = fopen(filename, "rb");
    if (!f)
    {
        LPRINTF(LOG_WARNING, "display: cannot open reference file '%s'\n", filename);
        return false;
    }

    if (fread(header, 1, sizeof(header), f) != sizeof(header) ||
        png_sig_cmp(header, 0, sizeof(header)) != 0)
    {
        LPRINTF(LOG_WARNING, "display: invalid PNG header in '%s'\n", filename);
        goto cleanup;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
    {
        LPRINTF(LOG_WARNING, "display: failed to create PNG read struct\n");
        goto cleanup;
    }

    info = png_create_info_struct(png);
    if (!info)
    {
        LPRINTF(LOG_WARNING, "display: failed to create PNG info struct\n");
        goto cleanup;
    }

    if (setjmp(png_jmpbuf(png)))
    {
        LPRINTF(LOG_WARNING, "display: failed to decode PNG file '%s'\n", filename);
        goto cleanup;
    }

    png_init_io(png, f);
    png_set_sig_bytes(png, sizeof(header));
    png_read_info(png, info);

    image_width = (int)png_get_image_width(png, info);
    image_height = (int)png_get_image_height(png, info);

    if (png_get_bit_depth(png, info) == 16)
        png_set_strip_16(png);
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png, info) < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY ||
        png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (png_get_color_type(png, info) & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png);

    png_read_update_info(png, info);

    rowbytes = png_get_rowbytes(png, info);
    if (rowbytes != (png_size_t)image_width * 3u)
    {
        LPRINTF(LOG_WARNING, "display: unexpected PNG row size in '%s'\n", filename);
        goto cleanup;
    }

    data = (uint8_t *)malloc((size_t)image_height * (size_t)rowbytes);
    rows = (png_bytep *)malloc((size_t)image_height * sizeof(png_bytep));
    if (!data || !rows)
    {
        LPRINTF(LOG_WARNING, "display: failed to allocate PNG decode buffers\n");
        goto cleanup;
    }

    for (int y = 0; y < image_height; y++)
        rows[y] = data + ((size_t)y * (size_t)rowbytes);

    png_read_image(png, rows);
    png_read_end(png, NULL);

    *width = image_width;
    *height = image_height;
    *pixels = data;
    data = NULL;
    success = true;

cleanup:
    if (rows)
        free(rows);
    if (data)
        free(data);
    if (png || info)
        png_destroy_read_struct(&png, &info, NULL);
    if (f)
        fclose(f);

    return success;
}

/*
 * Capture the display to a PNG file.
 */
bool display_capture_screen(display_t *display, const char *filename)
{
    /* If no display specified, use the active display */
    if (!display)
        display = g_active_display;
    
    if (!display || !filename)
        return false;
    
    LPRINTF(LOG_INFO, "display: capturing screen to '%s'\n", filename);
    
    if (!display_write_png_file(filename,
                                display->width,
                                display->height,
                                display->pixels,
                                display->palette))
        return false;
    
    LPRINTF(LOG_INFO, "display: captured %dx%d screen to '%s'\n",
            display->width, display->height, filename);
    
    return true;
}

/*
 * Capture a rootless window to a PNG file.
 */
bool display_capture_window(display_window_t *window, const char *filename)
{
    if (!window || !window->in_use || !filename)
        return false;
    
    LPRINTF(LOG_INFO, "display: capturing window to '%s'\n", filename);
    
    /* Get the palette from screen or window */
    const uint32_t *palette;
    if (window->screen)
    {
        palette = window->screen->palette;
    }
    else
    {
        palette = window->palette;
    }
    
    if (!display_write_png_file(filename,
                                window->width,
                                window->height,
                                window->pixels,
                                palette))
        return false;
    
    LPRINTF(LOG_INFO, "display: captured %dx%d window to '%s'\n",
            window->width, window->height, filename);
    
    return true;
}

bool display_capture_window_by_index(int index, const char *filename)
{
    int found = 0;

    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (!g_windows[i].in_use)
            continue;

        if (found == index)
            return display_capture_window(&g_windows[i], filename);

        found++;
    }

    return false;
}

/*
 * ============================================================================
 * Phase 39b: Enhanced Application Testing Infrastructure
 * ============================================================================
 */

/*
 * Get the dimensions of the active display.
 * Returns true if a display is active, false otherwise.
 */
bool display_get_active_dimensions(int *width, int *height, int *depth)
{
    if (!g_active_display)
        return false;
    
    if (width)  *width  = g_active_display->width;
    if (height) *height = g_active_display->height;
    if (depth)  *depth  = g_active_display->depth;
    
    return true;
}

/*
 * Check if the active display has non-empty content.
 * Returns the number of pixels that differ from the background color (index 0).
 * Returns -1 if no display is active.
 */
int display_get_content_pixels(void)
{
    if (!g_active_display || !g_active_display->pixels)
        return -1;
    
    int count = 0;
    int total = g_active_display->width * g_active_display->height;
    
    for (int i = 0; i < total; i++)
    {
        if (g_active_display->pixels[i] != 0)
            count++;
    }
    
    return count;
}

/*
 * Check if a rectangular region of the display has content.
 * Useful for validating that specific areas (like title bars) have been drawn.
 *
 * @param x, y        Top-left corner of region
 * @param width       Width of region to check
 * @param height      Height of region to check
 * @return Number of non-background pixels in region, or -1 on error
 */
int display_get_region_content(int x, int y, int width, int height)
{
    if (!g_active_display || !g_active_display->pixels)
        return -1;
    
    /* Clamp region to display bounds */
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > g_active_display->width)
        width = g_active_display->width - x;
    if (y + height > g_active_display->height)
        height = g_active_display->height - y;
    
    if (width <= 0 || height <= 0)
        return -1;
    
    int count = 0;
    for (int row = y; row < y + height; row++)
    {
        for (int col = x; col < x + width; col++)
        {
            if (g_active_display->pixels[row * g_active_display->width + col] != 0)
                count++;
        }
    }
    
    return count;
}

/*
 * Get the number of open rootless windows.
 */
int display_get_window_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
            count++;
    }
    return count;
}

/*
 * Get dimensions of a rootless window by index.
 * @param index   Window index (0-based)
 * @param width   Output: window width (can be NULL)
 * @param height  Output: window height (can be NULL)
 * @return true if window exists at index
 */
bool display_get_window_dimensions(int index, int *width, int *height)
{
    int found = 0;
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            if (found == index)
            {
                if (width)  *width  = g_windows[i].logical_width ? g_windows[i].logical_width : g_windows[i].width;
                if (height) *height = g_windows[i].logical_height ? g_windows[i].logical_height : g_windows[i].height;
                return true;
            }
            found++;
        }
    }
    return false;
}

/*
 * Check if a rootless window has content.
 * @param index  Window index (0-based)
 * @return Number of non-background pixels, or -1 if window not found
 */
int display_get_window_content(int index)
{
    int found = 0;
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            if (found == index)
            {
                display_window_t *win = &g_windows[i];
                if (!win->pixels)
                    return -1;
                
                int count = 0;
                int total = win->width * win->height;
                for (int j = 0; j < total; j++)
                {
                    if (win->pixels[j] != 0)
                        count++;
                }
                return count;
            }
            found++;
        }
    }
    return -1;
}

/*
 * Get window position by index.
 * @param index  Window index (0-based)
 * @param x      Output: window X position (can be NULL)
 * @param y      Output: window Y position (can be NULL)
 * @return true if window exists at index
 */
bool display_get_window_position(int index, int *x, int *y)
{
    int found = 0;
    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            if (found == index)
            {
                if (x) *x = g_windows[i].x;
                if (y) *y = g_windows[i].y;
                return true;
            }
            found++;
        }
    }
    return false;
}

bool display_get_window_title(int index, char *title, int title_len)
{
    int found = 0;

    if (!title || title_len <= 0)
        return false;

    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            if (found == index)
            {
                snprintf(title, title_len, "%s", g_windows[i].title);
                return true;
            }
            found++;
        }
    }

    return false;
}

bool display_get_window_emulated_pointer(int index, uint32_t *amiga_window_ptr)
{
    int found = 0;

    if (!amiga_window_ptr)
        return false;

    for (int i = 0; i < MAX_ROOTLESS_WINDOWS; i++)
    {
        if (g_windows[i].in_use)
        {
            if (found == index)
            {
                *amiga_window_ptr = g_windows[i].amiga_window_ptr;
                return true;
            }
            found++;
        }
    }

    return false;
}

/*
 * Compare the active display to a reference PNG file.
 * Returns a similarity percentage (0-100), or -1 on error.
 *
 * @param reference_file  Path to reference PNG file
 * @return Similarity percentage (0-100), or -1 on error
 */
int display_compare_to_reference(const char *reference_file)
{
    uint8_t *reference_pixels = NULL;
    int ref_width;
    int ref_height;

    if (!g_active_display || !g_active_display->pixels || !reference_file)
        return -1;

    if (!display_read_png_file(reference_file, &ref_width, &ref_height, &reference_pixels))
        return -1;
    
    /* Check dimensions match */
    if (ref_width != g_active_display->width || ref_height != g_active_display->height)
    {
        LPRINTF(LOG_WARNING, "display: reference dimensions (%dx%d) don't match display (%dx%d)\n",
                ref_width, ref_height, g_active_display->width, g_active_display->height);
        free(reference_pixels);
        return -1;
    }
    
    /* Compare pixels */
    int matching_pixels = 0;
    int total_pixels = ref_width * ref_height;
    
    for (int y = 0; y < ref_height; y++)
    {
        for (int x = 0; x < ref_width; x++)
        {
            size_t reference_offset = ((size_t)y * (size_t)ref_width + (size_t)x) * 3u;
            const uint8_t *ref_rgb = reference_pixels + reference_offset;
            
            /* Get current display pixel */
            uint8_t idx = g_active_display->pixels[y * ref_width + x];
            uint32_t color = g_active_display->palette[idx];
            uint8_t disp_r = (color >> 16) & 0xFF;
            uint8_t disp_g = (color >> 8) & 0xFF;
            uint8_t disp_b = color & 0xFF;
            
            /* Consider a match if colors are close (tolerance of 8) */
            int dr = (int)ref_rgb[0] - (int)disp_r;
            int dg = (int)ref_rgb[1] - (int)disp_g;
            int db = (int)ref_rgb[2] - (int)disp_b;
            
            if (dr < 0) dr = -dr;
            if (dg < 0) dg = -dg;
            if (db < 0) db = -db;
            
            if (dr <= 8 && dg <= 8 && db <= 8)
                matching_pixels++;
        }
    }

    free(reference_pixels);
    
    int similarity = (matching_pixels * 100) / total_pixels;
    LPRINTF(LOG_INFO, "display: comparison result: %d%% similarity (%d/%d pixels match)\n",
            similarity, matching_pixels, total_pixels);
    
    return similarity;
}

/*
 * Phase 57: Extended Screen and Pixel Query API
 */

/*
 * Read a pixel at screen coordinates.
 * Returns the palette index (pen) at the given position.
 */
bool display_read_pixel(int x, int y, int *pen)
{
    if (!g_active_display || !g_active_display->pixels || !pen)
        return false;
    
    if (x < 0 || x >= g_active_display->width ||
        y < 0 || y >= g_active_display->height)
        return false;
    
    *pen = g_active_display->pixels[y * g_active_display->width + x];
    return true;
}

/*
 * Read pixel RGB value at screen coordinates.
 * Looks up the palette color for the pixel.
 */
bool display_read_pixel_rgb(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!g_active_display || !g_active_display->pixels)
        return false;
    
    if (x < 0 || x >= g_active_display->width ||
        y < 0 || y >= g_active_display->height)
        return false;
    
    if (!r || !g || !b)
        return false;
    
    int idx = g_active_display->pixels[y * g_active_display->width + x];
    uint32_t argb = display_palette_argb(g_active_display->palette, (uint8_t)idx);
    
    /* ARGB format: 0xAARRGGBB */
    *r = (argb >> 16) & 0xFF;
    *g = (argb >> 8) & 0xFF;
    *b = argb & 0xFF;
    
    return true;
}

bool display_get_palette_rgb(int pen, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint32_t argb;

    if (!g_active_display || !r || !g || !b)
        return false;

    if (pen < 0 || pen >= DISPLAY_MAX_COLORS)
        return false;

    argb = display_palette_argb(g_active_display->palette, (uint8_t)pen);
    *r = (argb >> 16) & 0xFF;
    *g = (argb >> 8) & 0xFF;
    *b = argb & 0xFF;
    return true;
}

/*
 * Get extended screen information.
 */
bool display_get_screen_info(int *width, int *height, int *depth, int *num_colors)
{
    if (!g_active_display)
        return false;
    
    if (width)  *width = g_active_display->width;
    if (height) *height = g_active_display->height;
    if (depth)  *depth = g_active_display->depth;
    if (num_colors) *num_colors = 1 << g_active_display->depth;
    
    return true;
}
