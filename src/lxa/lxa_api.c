/*
 * lxa_api.c - Host-side API implementation for lxa emulator
 *
 * This file implements the lxa_api.h interface for test drivers.
 * It wraps the core lxa functionality to provide a clean API.
 */

#include "lxa_api.h"
#include "display.h"
#include "vfs.h"
#include "config.h"
#include "m68k.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <linux/limits.h>

/* External declarations for lxa.c globals and functions */
extern uint8_t g_ram[];
extern uint8_t g_rom[];
extern bool g_running;
extern bool g_verbose;
extern int g_rv;
extern char *g_loadfile;

#define MAX_ARGS_LEN 4096
extern char g_args[MAX_ARGS_LEN];
extern int g_args_len;

/* Forward declarations for internal lxa.c functions we need */
extern bool _load_rom_map(const char *rom_path);
extern void sigalrm_handler(int sig);
extern int _timer_check_expired(void);
extern void lxa_set_console_output_hook(void (*hook)(const char *data, int len));

/* Configuration constants from lxa.c */
#define RAM_SIZE (10 * 1024 * 1024)
#define ROM_SIZE (512 * 1024)
#define ROM_START 0xf80000
#define TIMER_INTERVAL_US 20000

/* API state */
static bool g_api_initialized = false;
static char g_output_buffer[64 * 1024];
static int g_output_len = 0;

/* Output capture hook - called from lxa.c _dos_write */
void lxa_api_capture_output(const char *data, int len)
{
    if (g_output_len + len < (int)sizeof(g_output_buffer) - 1) {
        memcpy(g_output_buffer + g_output_len, data, len);
        g_output_len += len;
        g_output_buffer[g_output_len] = '\0';
    }
}

int lxa_init(const lxa_config_t *config)
{
    if (g_api_initialized) {
        fprintf(stderr, "lxa_init: already initialized\n");
        return -1;
    }

    if (!config || !config->rom_path) {
        fprintf(stderr, "lxa_init: rom_path is required\n");
        return -1;
    }

    /* Set verbose mode */
    g_verbose = config->verbose;

    /* Initialize utilities */
    util_init();

    /* Set up VFS */
    if (!config->config_path) {
        vfs_setup_environment();
    } else {
        config_load(config->config_path);
    }

    /* Set up SYS: drive */
    if (config->sys_drive) {
        vfs_add_drive("SYS", config->sys_drive);
    } else if (!vfs_has_sys_drive()) {
        vfs_add_drive("SYS", ".");
    }

    vfs_setup_dynamic_drives();
    vfs_setup_default_assigns();

    /* Load ROM */
    FILE *romf = fopen(config->rom_path, "r");
    if (!romf) {
        fprintf(stderr, "lxa_init: failed to open ROM: %s\n", config->rom_path);
        return -1;
    }
    if (fread(g_rom, ROM_SIZE, 1, romf) != 1) {
        fprintf(stderr, "lxa_init: failed to read ROM\n");
        fclose(romf);
        return -1;
    }
    fclose(romf);

    if (!_load_rom_map(config->rom_path)) {
        fprintf(stderr, "lxa_init: failed to load ROM map\n");
        return -1;
    }

    /* Set up initial memory image */
    uint32_t initial_sp = RAM_SIZE - 1;
    uint32_t reset_vector = ROM_START + 2;
    m68k_write_memory_32(0, initial_sp);
    m68k_write_memory_32(4, reset_vector);

    /* Initialize CPU */
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68030);
    m68k_pulse_reset();

    /* Set headless mode if requested */
    if (config->headless) {
        display_set_headless(true);
    }

    /* Enable rootless mode for window tracking (default true for test drivers) */
    /* This allows lxa_get_window_count() to track individual Amiga windows */
    /* Note: headless does NOT force rootless - tests can opt out for pixel verification */
    config_set_rootless_mode(config->rootless);

    /* Initialize display subsystem */
    if (!display_init()) {
        fprintf(stderr, "lxa_init: display_init failed\n");
        /* Continue anyway - might be headless */
    }

    /* Set up timer for preemptive multitasking */
    struct sigaction sa;
    sa.sa_handler = sigalrm_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0) {
        perror("sigaction");
        return -1;
    }

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIMER_INTERVAL_US;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIMER_INTERVAL_US;
    if (setitimer(ITIMER_REAL, &timer, NULL) < 0) {
        perror("setitimer");
        return -1;
    }

    g_running = true;
    g_api_initialized = true;
    g_output_len = 0;

    /* Install console output capture hook */
    lxa_set_console_output_hook(lxa_api_capture_output);

    return 0;
}

void lxa_shutdown(void)
{
    if (!g_api_initialized) return;

    /* Stop timer */
    struct itimerval timer = {{0, 0}, {0, 0}};
    setitimer(ITIMER_REAL, &timer, NULL);

    /* Shutdown display */
    display_shutdown();

    g_api_initialized = false;
}

int lxa_load_program(const char *program, const char *args)
{
    if (!g_api_initialized) return -1;

    /* Store program path */
    static char loadfile_buf[PATH_MAX];
    strncpy(loadfile_buf, program, PATH_MAX - 1);
    loadfile_buf[PATH_MAX - 1] = '\0';
    g_loadfile = loadfile_buf;

    /* Store arguments */
    if (args && args[0]) {
        strncpy(g_args, args, sizeof(g_args) - 1);
        g_args[sizeof(g_args) - 1] = '\0';
        g_args_len = strlen(g_args);
    } else {
        g_args[0] = '\0';
        g_args_len = 0;
    }

    g_running = true;
    return 0;
}

/* ========== VFS/Drive/Assign API ========== */

bool lxa_add_assign(const char *name, const char *linux_path)
{
    if (!g_api_initialized) return false;
    if (!name || !linux_path) return false;
    return vfs_assign_add(name, linux_path, ASSIGN_LOCK);
}

bool lxa_add_assign_path(const char *name, const char *linux_path)
{
    if (!g_api_initialized) return false;
    if (!name || !linux_path) return false;
    return vfs_assign_add_path(name, linux_path);
}

bool lxa_add_drive(const char *name, const char *linux_path)
{
    if (!g_api_initialized) return false;
    if (!name || !linux_path) return false;
    vfs_add_drive(name, linux_path);
    return true;
}

static int s_vblank_count = 0;

int lxa_run_cycles(int cycles)
{
    if (!g_api_initialized || !g_running) return 1;

    /* Check for pending timer interrupts */
    extern volatile sig_atomic_t g_pending_irq;
    extern uint16_t g_intena;
    
    if (g_pending_irq & (1 << 3)) {
        s_vblank_count++;
        /* VBlank - poll events and refresh display */
        if (display_poll_events()) {
            g_running = false;
            return 1;
        }

        /* 
         * Update display from Amiga's planar bitmap if configured.
         * This converts the planar data in emulated RAM to chunky pixels
         * so that display_refresh_all() can present them via SDL.
         *
         * Phase 58: This was missing - display_refresh_all() only converts
         * the existing pixel buffer to SDL, but didn't update the pixel
         * buffer from the Amiga's planar bitmap first.
         */
        display_t *disp = display_get_active();
        uint32_t planes_ptr, bpr, depth;
        if (disp && display_get_amiga_bitmap(disp, &planes_ptr, &bpr, &depth))
        {
            int w, h, d;
            display_get_size(disp, &w, &h, &d);

            /* Read plane pointers from m68k memory */
            const uint8_t *planes[8] = {0};
            for (uint32_t i = 0; i < depth && i < 8; i++)
            {
                uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
                if (plane_addr && plane_addr < RAM_SIZE)
                {
                    planes[i] = (const uint8_t *)&g_ram[plane_addr];
                }
            }

            /* Update display from planar data */
            display_update_planar(disp, 0, 0, w, h, planes, bpr, depth);
        }

        /* Refresh displays (now with updated pixel data) */
        display_refresh_all();

        /* Check timer queue */
        _timer_check_expired();

        /* Trigger interrupt if enabled */
        #define INTENA_MASTER 0x4000
        #define INTENA_VBLANK 0x0020
        if ((g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK)) {
            g_pending_irq &= ~(1 << 3);
            m68k_set_irq(3);
        }
    } else {
        m68k_set_irq(0);
    }

    /* Execute CPU cycles */
    m68k_execute(cycles);

    return g_running ? 0 : 1;
}

int lxa_get_vblank_count(void)
{
    return s_vblank_count;
}

void lxa_trigger_vblank(void)
{
    /* Manually trigger VBlank processing for test drivers that need
     * event processing without waiting for real-time SIGALRM */
    extern volatile sig_atomic_t g_pending_irq;
    g_pending_irq |= (1 << 3);  /* Level 3 = VBlank */
}

bool lxa_is_running(void)
{
    return g_api_initialized && g_running;
}

int lxa_get_exit_code(void)
{
    return g_rv;
}

int lxa_run_until_exit(int timeout_ms)
{
    if (!g_api_initialized) return -1;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    int cycle_count = 0;

    while (g_running) {
        /* Periodically trigger VBlank to ensure input events and timer
         * callbacks are processed even when SIGALRM doesn't fire fast
         * enough (common in test/headless mode). */
        if (++cycle_count % 50 == 0) {
            lxa_trigger_vblank();
        }

        if (lxa_run_cycles(10000) != 0) break;

        if (timeout_ms > 0) {
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) {
                return -1;  /* Timeout */
            }
        }
    }

    return g_rv;
}

/* ========== Event Injection API ========== */

bool lxa_inject_mouse(int x, int y, int buttons, int event_type)
{
    if (!g_api_initialized) return false;
    return display_inject_mouse(x, y, buttons, (display_event_type_t)event_type);
}

bool lxa_inject_mouse_click(int x, int y, int button)
{
    if (!g_api_initialized) return false;

    /* Move to position */
    if (!display_inject_mouse(x, y, 0, DISPLAY_EVENT_MOUSEMOVE))
        return false;
    
    /* Trigger VBlank to ensure event is processed - need enough cycles for handler to complete */
    lxa_trigger_vblank();
    lxa_run_cycles(50000);

    /* Press button */
    if (!display_inject_mouse(x, y, button, DISPLAY_EVENT_MOUSEBUTTON))
        return false;

    /* Multiple VBlanks to ensure event goes through the full pipeline:
     * 1. Event queued
     * 2. VBlank polls events
     * 3. ProcessInputEvents processes the event
     * 4. IDCMP message is posted
     * 5. Task is signaled and scheduled
     * 6. Task runs and prints output
     */
    for (int i = 0; i < 5; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }

    /* Release button */
    if (!display_inject_mouse(x, y, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return false;

    for (int i = 0; i < 5; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(50000);
    }

    return true;
}

bool lxa_inject_rmb_click(int x, int y)
{
    return lxa_inject_mouse_click(x, y, LXA_MOUSE_RIGHT);
}

bool lxa_inject_drag(int start_x, int start_y, int end_x, int end_y, int button, int steps)
{
    if (!g_api_initialized) return false;

    /* Move to start position */
    if (!display_inject_mouse(start_x, start_y, 0, DISPLAY_EVENT_MOUSEMOVE))
        return false;
    
    lxa_trigger_vblank();
    lxa_run_cycles(500000);  /* Need enough cycles for IRQ handler to complete menu rendering */

    /* Press button */
    if (!display_inject_mouse(start_x, start_y, button, DISPLAY_EVENT_MOUSEBUTTON))
        return false;
    
    for (int i = 0; i < 3; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(500000);
    }

    /* Interpolate movement */
    if (steps < 1) steps = 1;
    
    for (int step = 1; step <= steps; step++) {
        int x = start_x + (end_x - start_x) * step / steps;
        int y = start_y + (end_y - start_y) * step / steps;
        
        if (!display_inject_mouse(x, y, button, DISPLAY_EVENT_MOUSEMOVE))
            return false;
        
        lxa_trigger_vblank();
        lxa_run_cycles(500000);
    }

    /* Release button at end position */
    if (!display_inject_mouse(end_x, end_y, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return false;
    
    for (int i = 0; i < 3; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(500000);
    }

    return true;
}

bool lxa_inject_key(int rawkey, int qualifier, bool down)
{
    if (!g_api_initialized) return false;
    return display_inject_key(rawkey, qualifier, down);
}

bool lxa_inject_keypress(int rawkey, int qualifier)
{
    if (!g_api_initialized) return false;
    if (!display_inject_key(rawkey, qualifier, true)) return false;
    lxa_run_cycles(2000);
    return display_inject_key(rawkey, qualifier, false);
}

bool lxa_inject_string(const char *str)
{
    if (!g_api_initialized) return false;
    return display_inject_string(str);
}

/* ========== State Query API ========== */

int lxa_get_window_count(void)
{
    if (!g_api_initialized) return 0;
    return display_get_window_count();
}

bool lxa_get_window_info(int index, lxa_window_info_t *info)
{
    if (!g_api_initialized || !info) return false;

    int w, h, x, y;
    if (!display_get_window_dimensions(index, &w, &h))
        return false;

    /* Get position */
    if (!display_get_window_position(index, &x, &y)) {
        x = 0;
        y = 0;
    }

    info->x = x;
    info->y = y;
    info->width = w;
    info->height = h;
    info->title[0] = '\0';  /* TODO: implement title query */

    return true;
}

bool lxa_click_close_gadget(int window_index)
{
    if (!g_api_initialized) return false;

    lxa_window_info_t info;
    if (!lxa_get_window_info(window_index, &info))
        return false;

    /* Close gadget is typically in the top-left corner of the window
     * Amiga window close gadget is usually at (4, 4) relative to window */
    int close_x = info.x + 8;
    int close_y = info.y + 4;

    return lxa_inject_mouse_click(close_x, close_y, LXA_MOUSE_LEFT);
}

bool lxa_get_screen_dimensions(int *width, int *height, int *depth)
{
    if (!g_api_initialized) return false;
    return display_get_active_dimensions(width, height, depth);
}

bool lxa_get_screen_info(lxa_screen_info_t *info)
{
    if (!g_api_initialized || !info) return false;
    
    int num_colors;
    if (!display_get_screen_info(&info->width, &info->height, &info->depth, &num_colors))
        return false;
    
    info->num_colors = num_colors;
    info->view_modes = 0;  /* TODO: implement view mode flags */
    info->title[0] = '\0';  /* TODO: implement screen title query */
    
    return true;
}

bool lxa_read_pixel(int x, int y, int *pen)
{
    if (!g_api_initialized) return false;
    return display_read_pixel(x, y, pen);
}

bool lxa_read_pixel_rgb(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!g_api_initialized) return false;
    return display_read_pixel_rgb(x, y, r, g, b);
}

int lxa_get_content_pixels(void)
{
    if (!g_api_initialized) return -1;
    return display_get_content_pixels();
}

/* ========== Waiting API ========== */

bool lxa_wait_idle(int timeout_ms)
{
    if (!g_api_initialized) return false;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    int cycles_since_vblank = 0;
    const int cycles_per_vblank = 50000;  /* Trigger VBlank every 50k cycles */

    while (!display_event_queue_empty()) {
        lxa_run_cycles(1000);
        cycles_since_vblank += 1000;

        /* Trigger VBlank periodically to process queued events */
        if (cycles_since_vblank >= cycles_per_vblank) {
            lxa_trigger_vblank();
            cycles_since_vblank = 0;
        }

        if (timeout_ms > 0) {
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) return false;
        }
    }

    return true;
}

bool lxa_wait_windows(int count, int timeout_ms)
{
    if (!g_api_initialized) return false;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    int cycles_since_vblank = 0;
    const int cycles_per_vblank = 50000;  /* Trigger VBlank every 50k cycles */

    while (lxa_get_window_count() < count && g_running) {
        lxa_run_cycles(10000);
        cycles_since_vblank += 10000;
        
        /* Trigger VBlank periodically to process events and let Intuition work */
        if (cycles_since_vblank >= cycles_per_vblank) {
            lxa_trigger_vblank();
            cycles_since_vblank = 0;
        }

        if (timeout_ms > 0) {
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) return false;
        }
    }

    return lxa_get_window_count() >= count;
}

bool lxa_wait_exit(int timeout_ms)
{
    if (!g_api_initialized) return false;

    struct timeval start, now;
    gettimeofday(&start, NULL);

    int cycles_since_vblank = 0;
    const int cycles_per_vblank = 50000;  /* Trigger VBlank every 50k cycles */

    while (g_running) {
        lxa_run_cycles(10000);
        cycles_since_vblank += 10000;

        /* Trigger VBlank periodically - needed for timer.device processing,
         * Intuition event delivery, and task scheduling during exit */
        if (cycles_since_vblank >= cycles_per_vblank) {
            lxa_trigger_vblank();
            cycles_since_vblank = 0;
        }

        if (timeout_ms > 0) {
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                          (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms) return false;
        }
    }

    return true;
}

/* ========== Output Capture API ========== */

int lxa_get_output(char *buffer, int size)
{
    if (!buffer || size <= 0) return 0;

    int copy_len = (g_output_len < size - 1) ? g_output_len : size - 1;
    memcpy(buffer, g_output_buffer, copy_len);
    buffer[copy_len] = '\0';

    return copy_len;
}

void lxa_clear_output(void)
{
    g_output_len = 0;
    g_output_buffer[0] = '\0';
}

bool lxa_capture_screen(const char *filename)
{
    if (!g_api_initialized) return false;
    
    display_t *disp = display_get_active();
    if (!disp) return false;
    
    return display_capture_screen(disp, filename);
}
