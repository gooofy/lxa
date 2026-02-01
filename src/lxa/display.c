/*
 * display.c - Host display subsystem using SDL2
 *
 * Phase 13: Graphics Foundation
 *
 * This module provides the host-side display rendering for lxa.
 * It creates SDL2 windows and handles the conversion from Amiga
 * planar bitmap format to SDL surfaces.
 */

#include "display.h"
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

/* Global state */
static bool g_display_initialized = false;
static bool g_sdl_available = false;

/*
 * Initialize the display subsystem.
 */
bool display_init(void)
{
    if (g_display_initialized)
    {
        return true;
    }

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
 * Process SDL events.
 * Returns true if quit was requested.
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
        switch (event.type)
        {
            case SDL_QUIT:
                return true;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    return true;
                }
                break;

            /* TODO: Handle keyboard and mouse events for Intuition input */
        }
    }
#endif

    return false;
}
