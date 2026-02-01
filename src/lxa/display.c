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
#include "util.h"

#include <stdlib.h>
#include <string.h>

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
    int           width;
    int           height;
    int           depth;
    uint8_t      *pixels;         /* Chunky pixel buffer */
    uint32_t      palette[DISPLAY_MAX_COLORS];  /* Local palette if no screen */
    bool          dirty;
    bool          in_use;         /* Slot is active */
};

/* Rootless window tracking */
static display_window_t g_windows[MAX_ROOTLESS_WINDOWS];
static bool g_rootless_mode = false;

/* Global state */
static bool g_display_initialized = false;
static bool g_sdl_available = false;
static display_t *g_active_display = NULL;  /* Forward declaration for event routing */

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
    if (g_sdl_available)
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
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
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
        LPRINTF(LOG_INFO, "display: opened %dx%dx%d virtual display (no SDL2)\n",
                width, height, depth);
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
                    dst[x] = display->palette[src[x]];
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
#define EVENT_QUEUE_SIZE 32
static display_event_t g_event_queue[EVENT_QUEUE_SIZE];
static int g_event_queue_head = 0;
static int g_event_queue_tail = 0;
static int g_mouse_x = 0;
static int g_mouse_y = 0;

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
    if (!g_sdl_available)
    {
        return false;
    }

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
                g_mouse_x = event.motion.x;
                g_mouse_y = event.motion.y;
                disp_event.type = DISPLAY_EVENT_MOUSEMOVE;
                disp_event.mouse_x = event.motion.x;
                disp_event.mouse_y = event.motion.y;
                disp_event.qualifier = sdl_mod_to_qualifier(SDL_GetModState());
                queue_event(&disp_event);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                g_mouse_x = event.button.x;
                g_mouse_y = event.button.y;
                disp_event.type = DISPLAY_EVENT_MOUSEBUTTON;
                disp_event.mouse_x = event.button.x;
                disp_event.mouse_y = event.button.y;
                disp_event.button_down = (event.type == SDL_MOUSEBUTTONDOWN);
                disp_event.qualifier = sdl_mod_to_qualifier(SDL_GetModState());
                
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
    win->width = width;
    win->height = height;
    win->depth = depth;
    win->in_use = true;

    /* Allocate pixel buffer */
    win->pixels = calloc(width * height, sizeof(uint8_t));
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
    if (g_sdl_available)
    {
        const char *window_title = title ? title : "LXA Window";

        win->window = SDL_CreateWindow(
            window_title,
            (x >= 0) ? x : SDL_WINDOWPOS_CENTERED,
            (y >= 0) ? y : SDL_WINDOWPOS_CENTERED,
            width, height,
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
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
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
            width, height
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
        SDL_RenderSetLogicalSize(win->renderer, width, height);

        LPRINTF(LOG_INFO, "display: opened rootless window %dx%d '%s' (SDL ID %u)\n",
                width, height, window_title, win->sdl_window_id);
    }
    else
    {
        LPRINTF(LOG_INFO, "display: opened virtual window %dx%d (no SDL2)\n",
                width, height);
    }
#else
    LPRINTF(LOG_INFO, "display: opened virtual window %dx%d (no SDL2)\n",
            width, height);
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

#if HAS_SDL2
    if (g_sdl_available && window->window)
    {
        SDL_SetWindowPosition(window->window, x, y);
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

    /* Reallocate pixel buffer if size changed */
    if (width != window->width || height != window->height)
    {
        uint8_t *new_pixels = calloc(width * height, sizeof(uint8_t));
        if (!new_pixels)
        {
            LPRINTF(LOG_ERROR, "display: out of memory for resized window\n");
            return false;
        }

        /* Copy old pixels (as much as fits) */
        int copy_w = (width < window->width) ? width : window->width;
        int copy_h = (height < window->height) ? height : window->height;
        for (int row = 0; row < copy_h; row++)
        {
            memcpy(new_pixels + row * width,
                   window->pixels + row * window->width,
                   copy_w);
        }

        free(window->pixels);
        window->pixels = new_pixels;
        window->width = width;
        window->height = height;

#if HAS_SDL2
        if (g_sdl_available && window->window)
        {
            SDL_SetWindowSize(window->window, width, height);

            /* Recreate texture at new size */
            if (window->texture)
            {
                SDL_DestroyTexture(window->texture);
            }
            window->texture = SDL_CreateTexture(
                window->renderer,
                SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING,
                width, height
            );
            SDL_RenderSetLogicalSize(window->renderer, width, height);
        }
#endif
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
                    dst[x] = palette[src[x]];
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
