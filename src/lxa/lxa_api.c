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
#include "lxa_copper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
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
extern void lxa_reset_host_state(void);
extern void (*g_text_hook)(const char *str, int len, int x, int y, void *userdata);
extern void *g_text_hook_userdata;

/* Configuration constants from lxa.c */
#define RAM_SIZE (10 * 1024 * 1024)
#define ROM_SIZE (512 * 1024)
#define ROM_START 0xf80000
#define TIMER_INTERVAL_US 20000
#define EXECBASE_LIBLIST             392
#define EXECBASE_TASKREADY           406
#define INTUITIONBASE_FIRSTSCREEN    60

#define NODE_SUCC_OFFSET             0
#define NODE_NAME_OFFSET             10

#define SCREEN_NEXTSCREEN_OFFSET     0
#define SCREEN_FIRSTWINDOW_OFFSET    4

#define WINDOW_LEFTEDGE_OFFSET      4
#define WINDOW_TOPEDGE_OFFSET       6
#define WINDOW_WIDTH_OFFSET         8
#define WINDOW_HEIGHT_OFFSET        10
#define WINDOW_TITLE_OFFSET         32
#define WINDOW_WSCREEN_OFFSET       46
#define WINDOW_FIRSTGADGET_OFFSET   62

#define GADGET_NEXT_OFFSET          0
#define GADGET_LEFTEDGE_OFFSET      4
#define GADGET_TOPEDGE_OFFSET       6
#define GADGET_WIDTH_OFFSET         8
#define GADGET_HEIGHT_OFFSET        10
#define GADGET_FLAGS_OFFSET         12
#define GADGET_ACTIVATION_OFFSET    14
#define GADGET_TYPE_OFFSET          16
#define GADGET_GADGETRENDER_OFFSET  18
#define GADGET_SELECTRENDER_OFFSET  22
#define GADGET_GADGETTEXT_OFFSET    26
#define GADGET_MUTUALEXCLUDE_OFFSET 30
#define GADGET_SPECIALINFO_OFFSET   34
#define GADGET_ID_OFFSET            38
#define GADGET_USERDATA_OFFSET      40

#define EXTGADGET_MOREFLAGS_OFFSET  44
#define EXTGADGET_BOUNDSLEFT_OFFSET 48
#define EXTGADGET_BOUNDSTOP_OFFSET  50
#define EXTGADGET_BOUNDSWIDTH_OFFSET 52
#define EXTGADGET_BOUNDSHEIGHT_OFFSET 54

#define GFLG_RELBOTTOM              0x0008
#define GFLG_RELRIGHT               0x0010
#define GFLG_RELWIDTH               0x0020
#define GFLG_RELHEIGHT              0x0040
#define GFLG_EXTENDED               0x8000
#define GMORE_BOUNDS                0x00000001UL

/* Phase 131: event log state — defined in lxa_events.c, shared with lxa_dispatch.c */
extern lxa_intui_event_t g_event_log[];
extern int               g_event_log_head;
extern int               g_event_log_count;
void lxa_reset_intui_events(void); /* defined in lxa_events.c */

static bool lxa_api_memory_string_equals(uint32_t addr, const char *str){
    size_t i;

    if (!addr || !str)
        return false;

    for (i = 0; str[i] != '\0'; i++)
    {
        if ((char)m68k_read_memory_8(addr + i) != str[i])
            return false;
    }

    return m68k_read_memory_8(addr + i) == 0;
}

static bool lxa_api_read_emulated_string(uint32_t addr, char *buffer, size_t buffer_len)
{
    size_t i;

    if (!buffer || buffer_len == 0)
        return false;

    buffer[0] = '\0';
    if (!addr)
        return false;

    for (i = 0; i + 1 < buffer_len; i++)
    {
        char c = (char)m68k_read_memory_8(addr + i);
        buffer[i] = c;
        if (c == '\0')
            return true;
    }

    buffer[buffer_len - 1] = '\0';
    return true;
}

static uint32_t lxa_api_find_library(const char *name)
{
    uint32_t sysbase;
    uint32_t list_addr;
    uint32_t node_addr;

    if (!name)
        return 0;

    sysbase = m68k_read_memory_32(4);
    if (sysbase == 0)
        return 0;

    list_addr = sysbase + EXECBASE_LIBLIST;
    node_addr = m68k_read_memory_32(list_addr);

    while (node_addr != list_addr + 4 && node_addr != 0)
    {
        uint32_t name_ptr = m68k_read_memory_32(node_addr + NODE_NAME_OFFSET);

        if (lxa_api_memory_string_equals(name_ptr, name))
            return node_addr;

        node_addr = m68k_read_memory_32(node_addr + NODE_SUCC_OFFSET);
    }

    return 0;
}

static bool lxa_api_window_matches_index(uint32_t window_ptr, int index)
{
    lxa_window_info_t tracked_info;
    char tracked_title[256];
    char emulated_title[256];
    uint32_t title_ptr;

    if (!window_ptr || !lxa_get_window_info(index, &tracked_info))
        return false;

    title_ptr = m68k_read_memory_32(window_ptr + WINDOW_TITLE_OFFSET);
    tracked_title[0] = '\0';
    emulated_title[0] = '\0';
    display_get_window_title(index, tracked_title, sizeof(tracked_title));
    lxa_api_read_emulated_string(title_ptr, emulated_title, sizeof(emulated_title));

    if ((int16_t)m68k_read_memory_16(window_ptr + WINDOW_LEFTEDGE_OFFSET) != tracked_info.x)
        return false;
    if ((int16_t)m68k_read_memory_16(window_ptr + WINDOW_TOPEDGE_OFFSET) != tracked_info.y)
        return false;
    if ((int16_t)m68k_read_memory_16(window_ptr + WINDOW_WIDTH_OFFSET) != tracked_info.width)
        return false;
    if ((int16_t)m68k_read_memory_16(window_ptr + WINDOW_HEIGHT_OFFSET) != tracked_info.height)
        return false;

    if (tracked_title[0] != '\0' && emulated_title[0] != '\0' && strcmp(tracked_title, emulated_title) != 0)
        return false;

    return true;
}

static uint32_t lxa_api_find_window_pointer_by_scan(int index)
{
    uint32_t intuition_base;
    uint32_t screen_ptr;
    int current_index = 0;

    intuition_base = lxa_api_find_library("intuition.library");
    if (!intuition_base)
        return 0;

    screen_ptr = m68k_read_memory_32(intuition_base + INTUITIONBASE_FIRSTSCREEN);
    while (screen_ptr)
    {
        uint32_t window_ptr = m68k_read_memory_32(screen_ptr + SCREEN_FIRSTWINDOW_OFFSET);

        while (window_ptr)
        {
            if (current_index == index)
                return window_ptr;
            if (lxa_api_window_matches_index(window_ptr, index))
                return window_ptr;

            current_index++;
            window_ptr = m68k_read_memory_32(window_ptr + NODE_SUCC_OFFSET);
        }

        screen_ptr = m68k_read_memory_32(screen_ptr + SCREEN_NEXTSCREEN_OFFSET);
    }

    return 0;
}

static uint32_t lxa_api_get_window_pointer(int index)
{
    uint32_t window_ptr = 0;

    if (display_get_window_emulated_pointer(index, &window_ptr) && window_ptr != 0)
        return window_ptr;

    return lxa_api_find_window_pointer_by_scan(index);
}

static void lxa_api_calculate_gadget_box(uint32_t window_ptr,
                                         uint32_t gadget_ptr,
                                         int *left,
                                         int *top,
                                         int *width,
                                         int *height)
{
    int calc_left;
    int calc_top;
    int calc_width;
    int calc_height;
    uint16_t flags;

    if (!window_ptr || !gadget_ptr)
    {
        if (left)
            *left = 0;
        if (top)
            *top = 0;
        if (width)
            *width = 0;
        if (height)
            *height = 0;
        return;
    }

    calc_left = (int16_t)m68k_read_memory_16(gadget_ptr + GADGET_LEFTEDGE_OFFSET);
    calc_top = (int16_t)m68k_read_memory_16(gadget_ptr + GADGET_TOPEDGE_OFFSET);
    calc_width = (int16_t)m68k_read_memory_16(gadget_ptr + GADGET_WIDTH_OFFSET);
    calc_height = (int16_t)m68k_read_memory_16(gadget_ptr + GADGET_HEIGHT_OFFSET);
    flags = (uint16_t)m68k_read_memory_16(gadget_ptr + GADGET_FLAGS_OFFSET);

    if ((flags & GFLG_EXTENDED) &&
        (m68k_read_memory_32(gadget_ptr + EXTGADGET_MOREFLAGS_OFFSET) & GMORE_BOUNDS))
    {
        calc_left = (int16_t)m68k_read_memory_16(gadget_ptr + EXTGADGET_BOUNDSLEFT_OFFSET);
        calc_top = (int16_t)m68k_read_memory_16(gadget_ptr + EXTGADGET_BOUNDSTOP_OFFSET);
        calc_width = (int16_t)m68k_read_memory_16(gadget_ptr + EXTGADGET_BOUNDSWIDTH_OFFSET);
        calc_height = (int16_t)m68k_read_memory_16(gadget_ptr + EXTGADGET_BOUNDSHEIGHT_OFFSET);
    }

    if (flags & (GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_RELWIDTH | GFLG_RELHEIGHT))
    {
        int container_width = (int16_t)m68k_read_memory_16(window_ptr + WINDOW_WIDTH_OFFSET);
        int container_height = (int16_t)m68k_read_memory_16(window_ptr + WINDOW_HEIGHT_OFFSET);

        if (flags & GFLG_RELRIGHT)
            calc_left += container_width - 1;
        if (flags & GFLG_RELBOTTOM)
            calc_top += container_height - 1;
        if (flags & GFLG_RELWIDTH)
            calc_width += container_width;
        if (flags & GFLG_RELHEIGHT)
            calc_height += container_height;
    }

    if (left)
        *left = calc_left;
    if (top)
        *top = calc_top;
    if (width)
        *width = calc_width;
    if (height)
        *height = calc_height;
}

static uint32_t lxa_api_get_window_gadget(int window_index, int gadget_index)
{
    uint32_t window_ptr;
    uint32_t gadget_ptr;
    uint32_t screen_ptr;
    int current_index = 0;

    if (gadget_index < 0)
        return 0;

    window_ptr = lxa_api_get_window_pointer(window_index);
    if (!window_ptr)
        return 0;

    gadget_ptr = m68k_read_memory_32(window_ptr + WINDOW_FIRSTGADGET_OFFSET);
    while (gadget_ptr)
    {
        if (current_index == gadget_index)
            return gadget_ptr;
        current_index++;
        gadget_ptr = m68k_read_memory_32(gadget_ptr + GADGET_NEXT_OFFSET);
    }

    screen_ptr = m68k_read_memory_32(window_ptr + WINDOW_WSCREEN_OFFSET);
    if (screen_ptr)
        gadget_ptr = m68k_read_memory_32(screen_ptr + 326);

    while (gadget_ptr)
    {
        if (current_index == gadget_index)
            return gadget_ptr;
        current_index++;
        gadget_ptr = m68k_read_memory_32(gadget_ptr + GADGET_NEXT_OFFSET);
    }

    return 0;
}

/* API state */
static bool g_api_initialized = false;
static char g_output_buffer[64 * 1024];
static int g_output_len = 0;

/*
 * Phase 106: Track whether the planar-to-chunky pixel buffer is stale.
 * In headless mode the VBlank handler skips the expensive conversion,
 * so the display pixel buffer drifts out of date.  Any function that
 * reads pixels (lxa_read_pixel, lxa_read_pixel_rgb, lxa_get_content_pixels)
 * checks this flag and calls lxa_flush_display() once on demand, keeping
 * per-pixel overhead near zero while still returning correct data.
 */
static bool s_display_dirty = true;

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
    extern volatile sig_atomic_t g_pending_irq;

    if (g_api_initialized) {
        fprintf(stderr, "lxa_init: already initialized\n");
        return -1;
    }

    if (!config || !config->rom_path) {
        fprintf(stderr, "lxa_init: rom_path is required\n");
        return -1;
    }

    /* Reset profiling counters for fresh run */
    lxa_profile_reset();

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
    memset(g_ram, 0, RAM_SIZE);
    uint32_t initial_sp = RAM_SIZE - 1;
    uint32_t reset_vector = ROM_START + 2;
    m68k_write_memory_32(0, initial_sp);
    m68k_write_memory_32(4, reset_vector);

    /* Initialize CPU */
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68030);
    m68k_pulse_reset();
    g_pending_irq = 0;

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
    s_display_dirty = true;

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

    config_reset();
    vfs_reset();
    util_shutdown();
    lxa_reset_host_state();
    lxa_profile_reset();

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

    /* Reset event log so each program run starts with a clean log */
    lxa_reset_intui_events();

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
static int s_cycles_since_auto_vblank = 0;

/*
 * Automatic VBlank cadence based on emulated cycle count.
 *
 * On a PAL Amiga at 7.09 MHz, a VBlank fires every ~141,800 cycles
 * (7,093,790 Hz / 50 Hz). We use a slightly lower threshold to ensure
 * timer completions and scheduling happen frequently enough even when
 * LOG_LEVEL suppresses DPRINTF overhead that would otherwise slow down
 * execution and naturally space out VBlanks.
 */
/*
 * Phase 106: Raised from 100K to 500K.  The lower value caused up to
 * 10 spurious auto-VBlanks per 1M-cycle inject_string step, each
 * triggering a full planar-to-chunky conversion in non-headless mode
 * and unnecessary VBlank ISR overhead in all modes.  500K still fires
 * roughly every ~70 ms of emulated time (7 MHz / 500K ≈ 14 Hz),
 * which is frequent enough for timer completions and input delivery.
 */
#define AUTO_VBLANK_CYCLES 500000

int lxa_run_cycles(int cycles)
{
    if (!g_api_initialized || !g_running) return 1;

    /* Check for pending timer interrupts */
    extern volatile sig_atomic_t g_pending_irq;
    extern uint16_t g_intena;
    
    if (g_pending_irq & (1 << 3)) {
        s_vblank_count++;
        s_cycles_since_auto_vblank = 0;
        /* VBlank - poll events and refresh display */
        if (display_poll_events()) {
            g_running = false;
            return 1;
        }

        /* Phase 114: Run one pass of the copper list per VBlank. */
        copper_run_frame();

        /* 
         * Update display from Amiga's planar bitmap if configured.
         * This converts the planar data in emulated RAM to chunky pixels
         * so that display_refresh_all() can present them via SDL.
         *
         * Phase 58: This was missing - display_refresh_all() only converts
         * the existing pixel buffer to SDL, but didn't update the pixel
         * buffer from the Amiga's planar bitmap first.
         *
         * Phase 106: Skip the expensive planar-to-chunky conversion and
         * display refresh in headless mode.  Tests that need pixel data
         * call lxa_flush_display() or lxa_capture_window() explicitly,
         * both of which always perform the conversion regardless of this
         * skip.  This eliminates ~150 full-screen conversions per typical
         * test SetUp.
         */
        if (!display_get_headless()) {
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
        } else {
            /* Headless: pixel buffer is now stale.  Functions that
             * read pixels (lxa_read_pixel, etc.) will auto-flush on
             * the first access after this point. */
            s_display_dirty = true;
        }

        /* Check timer queue */
        _timer_check_expired();

        /* Trigger interrupt if enabled */
        #define INTENA_MASTER 0x4000
        #define INTENA_VBLANK 0x0020
        if ((g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK)) {
            g_pending_irq &= ~(1 << 3);
            /*
             * Lower the interrupt line first, then raise it again.
             * Musashi treats same-level interrupts as edge-triggered:
             * once the CPU acknowledges a level-3 interrupt, it won't
             * re-trigger at the same level unless the line is lowered
             * first.  Without this edge, a VBlank triggered while the
             * previous ISR is still active (IPL=3) would be lost
             * because the CPU considers it already acknowledged.
             */
            m68k_set_irq(0);
            m68k_set_irq(3);
        } else {
            DPRINTF(LOG_DEBUG, "lxa_run_cycles: VBlank pending but INTENA blocked, g_intena=0x%04x\n",
                    g_intena);
        }
    } else {
        /*
         * Auto-VBlank: if enough emulated cycles have passed without a
         * SIGALRM-triggered VBlank, synthesize one.  This ensures that
         * timer completions, input delivery, and task scheduling happen at
         * a realistic cadence regardless of host-side wall-clock speed.
         */
        s_cycles_since_auto_vblank += cycles;
        if (s_cycles_since_auto_vblank >= AUTO_VBLANK_CYCLES) {
            s_cycles_since_auto_vblank = 0;
            g_pending_irq |= (1 << 3);
            /* Process will happen on the next lxa_run_cycles call */
        }

        m68k_set_irq(0);
    }

    /* Execute CPU cycles */
    m68k_execute(cycles);

    if (!g_running) {
        DPRINTF(LOG_DEBUG, "lxa_run_cycles: g_running became false during m68k_execute\n");
    }
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

bool lxa_is_idle(void)
{
    if (!g_api_initialized) return false;

    /*
     * Check whether the TaskReady list is empty.  When it is, no Exec
     * task has pending work — every task (including the app under test)
     * is in TS_WAIT, blocked on WaitPort() or similar.  This means the
     * emulated system has finished processing the most recent event and
     * is waiting for new input.
     *
     * We read the list directly from emulated memory to avoid coupling
     * with ROM internals beyond the well-defined ExecBase layout.
     */
    uint32_t sysbase = m68k_read_memory_32(4);
    if (sysbase == 0) return false;

    uint32_t list_addr = sysbase + EXECBASE_TASKREADY;
    uint32_t lh_Head = m68k_read_memory_32(list_addr);
    uint32_t lh_Tail_addr = list_addr + 4;

    return lh_Head == lh_Tail_addr;
}

int lxa_run_until_idle(int max_iterations, int cycles_per_iter)
{
    if (!g_api_initialized || !g_running) return 0;

    for (int i = 0; i < max_iterations; i++) {
        lxa_trigger_vblank();
        if (lxa_run_cycles(cycles_per_iter) != 0)
            return i + 1;

        if (lxa_is_idle())
            return i + 1;
    }

    return max_iterations;
}

void lxa_flush_display(void)
{
    /* Force an immediate planar-to-chunky conversion so that
     * display_read_pixel() returns current data.
     *
     * This duplicates the conversion logic from lxa_run_cycles()
     * but can be called at any time (e.g. after event processing
     * has modified the screen bitmap). */
    display_t *disp = display_get_active();
    uint32_t planes_ptr, bpr, depth;
    if (disp && display_get_amiga_bitmap(disp, &planes_ptr, &bpr, &depth))
    {
        int w, h, d;
        display_get_size(disp, &w, &h, &d);

        const uint8_t *planes[8] = {0};
        for (uint32_t i = 0; i < depth && i < 8; i++)
        {
            uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
            if (plane_addr && plane_addr < RAM_SIZE)
            {
                planes[i] = (const uint8_t *)&g_ram[plane_addr];
            }
        }

        display_update_planar(disp, 0, 0, w, h, planes, bpr, depth);
    }

    /* Also sync rootless windows from their screen bitmaps so that
     * headless capture (display_capture_window) sees current pixel data. */
    display_refresh_all();

    s_display_dirty = false;
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
    
    /*
     * Phase 106: Use idle detection to return early.  The previous
     * fixed budget of 11 VBlanks x 50K cycles (550K total) was
     * chosen conservatively.  With idle detection we run the minimum
     * cycles needed: 1 move settle + 3 press settle + 3 release
     * settle, returning early from each batch as soon as the app
     * reaches WaitPort().
     */
    lxa_run_until_idle(1, 50000);

    /* Press button */
    if (!display_inject_mouse(x, y, button, DISPLAY_EVENT_MOUSEBUTTON))
        return false;

    lxa_run_until_idle(3, 50000);

    /* Release button */
    if (!display_inject_mouse(x, y, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return false;

    lxa_run_until_idle(3, 50000);

    return true;
}

bool lxa_inject_rmb_click(int x, int y)
{
    return lxa_inject_mouse_click(x, y, LXA_MOUSE_RIGHT);
}

bool lxa_inject_drag(int start_x, int start_y, int end_x, int end_y, int button, int steps)
{
    if (!g_api_initialized) return false;

    DPRINTF(LOG_DEBUG, "lxa_api: inject_drag (%d,%d)->(%d,%d) btn=0x%x steps=%d\n",
            start_x, start_y, end_x, end_y, button, steps);

    /* Move to start position */
    if (!display_inject_mouse(start_x, start_y, 0, DISPLAY_EVENT_MOUSEMOVE))
        return false;
    
    /*
     * Phase 106: Drag steps need generous cycle budgets because
     * Intuition's rendering happens in the input.device handler
     * (interrupt context), not in a normal task.  The CPU task
     * goes idle immediately, but the rendering work still needs
     * VBlank cycles to complete.  Idle detection is therefore
     * unreliable here — use fixed budgets instead.
     *
     * The original pre-Phase-106 budget was 500K per step.
     * We reduce to 3 VBlanks × 200K cycles each, which is still
     * significantly less overhead while keeping drag fidelity.
     */
    lxa_trigger_vblank();
    lxa_run_cycles(200000);

    /* Press button */
    if (!display_inject_mouse(start_x, start_y, button, DISPLAY_EVENT_MOUSEBUTTON))
        return false;
    
    /*
     * After a button press (especially RMB which enters menu mode),
     * the VBlank ISR triggers heavy rendering (menu bar + items).
     * Give enough VBlanks for the menu system to set up.
     */
    for (int i = 0; i < 5; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(200000);
    }

    /* Interpolate movement */
    if (steps < 1) steps = 1;
    
    for (int step = 1; step <= steps; step++) {
        int x = start_x + (end_x - start_x) * step / steps;
        int y = start_y + (end_y - start_y) * step / steps;
        
        bool inj = display_inject_mouse(x, y, button, DISPLAY_EVENT_MOUSEMOVE);
        if (!inj)
            return false;
        
        /* Each movement step needs VBlank processing for the
         * input handler to see the new coordinates.  Menu-mode
         * steps trigger _render_menu_item_chain (RectFill + Text
         * per item) inside ProcessInputEvents.  Give 10 VBlanks ×
         * 200K cycles (2M total) so the render completes before
         * the next event is injected. */
        for (int i = 0; i < 10; i++) {
            lxa_trigger_vblank();
            lxa_run_cycles(200000);
        }
    }

    /* Release button at end position */
    if (!display_inject_mouse(end_x, end_y, 0, DISPLAY_EVENT_MOUSEBUTTON))
        return false;
    
    /* Let the release event propagate — menu cleanup / window
     * resize finalization happen here. */
    for (int i = 0; i < 5; i++) {
        lxa_trigger_vblank();
        lxa_run_cycles(200000);
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
    if (!str) return false;
    
    /*
     * Phase 106: Use idle detection to dramatically reduce cycle waste.
     * Previously each character cost 2 × 500K = 1M cycles unconditionally.
     * Now we run up to 5 iterations of 50K cycles per key event but return
     * early as soon as the app goes idle (all tasks in TS_WAIT).
     *
     * For a typical string gadget keystroke the app needs ~20-30K cycles
     * to process the event and re-render, so idle detection typically
     * returns after 1-2 iterations instead of burning the full budget.
     */
    for (const char *p = str; *p; p++)
    {
        bool need_shift = false;
        int rawkey = ascii_to_rawkey(*p, &need_shift);
        if (rawkey == 0xFF) continue;
        
        int qualifier = need_shift ? 0x0001 : 0;
        
        /* Key down */
        display_inject_key(rawkey, qualifier, true);
        lxa_run_until_idle(5, 50000);
        
        /* Key up */
        display_inject_key(rawkey, qualifier, false);
        lxa_run_until_idle(5, 50000);
    }
    
    return true;
}

/* ========== State Query API ========== */

int lxa_get_window_count(void)
{
    if (!g_api_initialized) return 0;
    return display_get_window_count();
}

int lxa_get_window_content(int index)
{
    if (!g_api_initialized) return -1;
    lxa_flush_display();  /* sync planar→chunky before pixel query */
    return display_get_window_content(index);
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
    if (!display_get_window_title(index, info->title, sizeof(info->title))) {
        info->title[0] = '\0';
    }

    return true;
}

bool lxa_wait_window_drawn(int index, int timeout_ms)
{
    if (!g_api_initialized)
        return false;

    struct timeval start, now;
    int cycles_since_vblank = 0;
    const int cycles_per_vblank = 50000;

    gettimeofday(&start, NULL);

    while (g_running)
    {
        if (lxa_get_window_content(index) >= 0)
            return true;

        lxa_run_cycles(10000);
        cycles_since_vblank += 10000;

        if (cycles_since_vblank >= cycles_per_vblank)
        {
            lxa_trigger_vblank();
            cycles_since_vblank = 0;
        }

        if (timeout_ms > 0)
        {
            gettimeofday(&now, NULL);
            long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                           (now.tv_usec - start.tv_usec) / 1000;
            if (elapsed >= timeout_ms)
                return false;
        }
    }

    return lxa_get_window_content(index) >= 0;
}

int lxa_get_gadget_count(int window_index)
{
    uint32_t window_ptr;
    uint32_t gadget_ptr;
    uint32_t screen_ptr;
    int count = 0;

    if (!g_api_initialized)
        return -1;

    window_ptr = lxa_api_get_window_pointer(window_index);
    if (!window_ptr)
        return -1;

    gadget_ptr = m68k_read_memory_32(window_ptr + WINDOW_FIRSTGADGET_OFFSET);
    while (gadget_ptr)
    {
        count++;
        gadget_ptr = m68k_read_memory_32(gadget_ptr + GADGET_NEXT_OFFSET);
    }

    screen_ptr = m68k_read_memory_32(window_ptr + WINDOW_WSCREEN_OFFSET);
    if (screen_ptr)
    {
        gadget_ptr = m68k_read_memory_32(screen_ptr + 326);
        while (gadget_ptr)
        {
            count++;
            gadget_ptr = m68k_read_memory_32(gadget_ptr + GADGET_NEXT_OFFSET);
        }
    }

    return count;
}

bool lxa_get_gadget_info(int window_index, int gadget_index, lxa_gadget_info_t *info)
{
    uint32_t window_ptr;
    uint32_t gadget_ptr;
    int left;
    int top;
    int width;
    int height;

    if (!g_api_initialized || !info)
        return false;

    window_ptr = lxa_api_get_window_pointer(window_index);
    if (!window_ptr)
        return false;

    gadget_ptr = lxa_api_get_window_gadget(window_index, gadget_index);
    if (!gadget_ptr)
        return false;

    lxa_api_calculate_gadget_box(window_ptr, gadget_ptr, &left, &top, &width, &height);

    info->left = (int16_t)m68k_read_memory_16(window_ptr + WINDOW_LEFTEDGE_OFFSET) + left;
    info->top = (int16_t)m68k_read_memory_16(window_ptr + WINDOW_TOPEDGE_OFFSET) + top;
    info->width = width;
    info->height = height;
    info->flags = (uint16_t)m68k_read_memory_16(gadget_ptr + GADGET_FLAGS_OFFSET);
    info->activation = (uint16_t)m68k_read_memory_16(gadget_ptr + GADGET_ACTIVATION_OFFSET);
    info->gadget_type = (uint16_t)m68k_read_memory_16(gadget_ptr + GADGET_TYPE_OFFSET);
    info->gadget_id = (uint16_t)m68k_read_memory_16(gadget_ptr + GADGET_ID_OFFSET);
    info->user_data = m68k_read_memory_32(gadget_ptr + GADGET_USERDATA_OFFSET);

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
    if (s_display_dirty) lxa_flush_display();
    return display_read_pixel(x, y, pen);
}

bool lxa_read_pixel_rgb(int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!g_api_initialized) return false;
    if (s_display_dirty) lxa_flush_display();
    return display_read_pixel_rgb(x, y, r, g, b);
}

bool lxa_get_palette_rgb(int pen, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!g_api_initialized) return false;
    return display_get_palette_rgb(pen, r, g, b);
}

int lxa_get_content_pixels(void)
{
    if (!g_api_initialized) return -1;
    lxa_flush_display();  /* sync planar→chunky before pixel query */
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
    if (s_display_dirty) lxa_flush_display();
    
    display_t *disp = display_get_active();
    if (!disp) return false;
    
    return display_capture_screen(disp, filename);
}

bool lxa_capture_window(int window_index, const char *filename)
{
    if (!g_api_initialized || !filename)
        return false;
    if (s_display_dirty) lxa_flush_display();

    return display_capture_window_by_index(window_index, filename);
}

uint32_t lxa_peek32(uint32_t addr)
{
    return m68k_read_memory_32(addr);
}

uint16_t lxa_peek16(uint32_t addr)
{
    return m68k_read_memory_16(addr);
}

uint8_t lxa_peek8(uint32_t addr)
{
    return m68k_read_memory_8(addr);
}

/* ========== Phase 131: Window/Screen Event Log ========== */

/* Intuition struct offsets used for menu traversal */
#define MENU_NEXTMENU_OFFSET    0   /* struct Menu *NextMenu */
#define MENU_LEFTEDGE_OFFSET    4   /* WORD LeftEdge */
#define MENU_TOPEDGE_OFFSET     6   /* WORD TopEdge */
#define MENU_WIDTH_OFFSET       8   /* WORD Width */
#define MENU_HEIGHT_OFFSET      10  /* WORD Height */
#define MENU_FLAGS_OFFSET       12  /* UWORD Flags */
#define MENU_MENUNAME_OFFSET    14  /* BYTE *MenuName */
#define MENU_FIRSTITEM_OFFSET   18  /* struct MenuItem *FirstItem */

#define MENUITEM_NEXTITEM_OFFSET   0   /* struct MenuItem *NextItem */
#define MENUITEM_LEFTEDGE_OFFSET   4   /* WORD LeftEdge */
#define MENUITEM_TOPEDGE_OFFSET    6   /* WORD TopEdge */
#define MENUITEM_WIDTH_OFFSET      8   /* WORD Width */
#define MENUITEM_HEIGHT_OFFSET     10  /* WORD Height */
#define MENUITEM_FLAGS_OFFSET      12  /* UWORD Flags */
#define MENUITEM_EXCLUSION_OFFSET  14  /* LONG MutualExclude */
#define MENUITEM_ITEMFILL_OFFSET   18  /* APTR ItemFill */
#define MENUITEM_SELECTFILL_OFFSET 22  /* APTR SelectFill */
#define MENUITEM_COMMAND_OFFSET    26  /* BYTE Command */
#define MENUITEM_SUBITEM_OFFSET    28  /* struct MenuItem *SubItem */
#define MENUITEM_NEXTSELECT_OFFSET 32  /* UWORD NextSelect */

/* Amiga MenuItem Flags */
#define MENUITEM_FLAG_ITEMENABLED  0x0010  /* ITEMENABLED = bit 4 */
#define MENUITEM_FLAG_ITEMTEXT     0x0002
#define MENUITEM_FLAG_COMMSEQ      0x0004
#define MENUITEM_FLAG_MENUTOGGLE   0x0008
#define MENUITEM_FLAG_CHECKIT      0x0001
#define MENUITEM_FLAG_CHECKED      0x0100
#define MENUITEM_FLAG_HIGHCOMP     0x0040

/* IntuiText offsets (for reading item label when ITEMTEXT is set) */
#define INTUITEXT_ITEXT_OFFSET     12 /* UBYTE *IText within IntuiText struct */

/* lxa_push_intui_event and lxa_reset_intui_events are defined in lxa_events.c */

int lxa_drain_intui_events(lxa_intui_event_t *events, int max_count)
{
    if (!events || max_count <= 0)
        return 0;

    int n = g_event_log_count;
    if (n > max_count)
        n = max_count;

    /* Oldest entry is at (head - count + LXA_EVENT_LOG_SIZE) % LXA_EVENT_LOG_SIZE */
    int tail = (g_event_log_head - g_event_log_count + LXA_EVENT_LOG_SIZE * 2) % LXA_EVENT_LOG_SIZE;
    for (int i = 0; i < n; i++)
    {
        events[i] = g_event_log[(tail + i) % LXA_EVENT_LOG_SIZE];
    }

    g_event_log_count = 0;
    g_event_log_head  = 0;
    return n;
}

int lxa_intui_event_count(void)
{
    return g_event_log_count;
}

/* ========== Phase 131: Menu Introspection ========== */

/*
 * We snapshot the entire menu strip at query time and store an array
 * of (menu_ptr, item_ptr[], subitem_ptr[][]) 32-bit emulated addresses.
 * The strip handle owns this heap snapshot so the caller can query it
 * freely without worrying about concurrent ROM activity.
 */

#define MAX_MENUS     32
#define MAX_ITEMS     64
#define MAX_SUBITEMS  32

typedef struct
{
    uint32_t menu_ptr;                       /* emulated address of struct Menu */
    int      item_count;
    uint32_t item_ptrs[MAX_ITEMS];           /* emulated addresses of struct MenuItem */
    int      subitem_counts[MAX_ITEMS];
    uint32_t subitem_ptrs[MAX_ITEMS][MAX_SUBITEMS]; /* emulated addresses of sub MenuItem */
} lxa_menu_snapshot_t;

struct lxa_menu_strip
{
    int                  menu_count;
    lxa_menu_snapshot_t  menus[MAX_MENUS];
    int                  screen_x;  /* Screen LeftEdge for menu x offset */
};

/* Read a NUL-terminated string from emulated RAM into buf (max buf_len). */
static void lxa_read_emu_string(uint32_t addr, char *buf, int buf_len)
{
    buf[0] = '\0';
    if (!addr || buf_len <= 0)
        return;
    for (int i = 0; i + 1 < buf_len; i++)
    {
        char c = (char)m68k_read_memory_8(addr + i);
        buf[i] = c;
        if (!c)
            return;
    }
    buf[buf_len - 1] = '\0';
}

lxa_menu_strip_t *lxa_get_menu_strip(int window_index)
{
    if (!g_api_initialized)
        return NULL;

    /* Locate the emulated Window* for this host window index */
    uint32_t window_ptr = lxa_api_get_window_pointer(window_index);
    if (!window_ptr)
        return NULL;

    /* Window->MenuStrip is at offset 28 (standard Amiga struct, verified from NDK/AROS) */
    uint32_t strip_ptr = m68k_read_memory_32(window_ptr + 28);
    if (!strip_ptr)
        return NULL;

    lxa_menu_strip_t *handle = (lxa_menu_strip_t *)calloc(1, sizeof(lxa_menu_strip_t));
    if (!handle)
        return NULL;

    /* Walk the linked list of struct Menu */
    uint32_t menu_ptr = strip_ptr;
    int mi = 0;
    while (menu_ptr && mi < MAX_MENUS)
    {
        lxa_menu_snapshot_t *ms = &handle->menus[mi];
        ms->menu_ptr = menu_ptr;
        ms->item_count = 0;

        /* Walk FirstItem chain */
        uint32_t item_ptr = m68k_read_memory_32(menu_ptr + MENU_FIRSTITEM_OFFSET);
        int ii = 0;
        while (item_ptr && ii < MAX_ITEMS)
        {
            ms->item_ptrs[ii] = item_ptr;
            ms->subitem_counts[ii] = 0;

            /* Walk SubItem chain */
            uint32_t sub_ptr = m68k_read_memory_32(item_ptr + MENUITEM_SUBITEM_OFFSET);
            int si = 0;
            while (sub_ptr && si < MAX_SUBITEMS)
            {
                ms->subitem_ptrs[ii][si] = sub_ptr;
                si++;
                sub_ptr = m68k_read_memory_32(sub_ptr + MENUITEM_NEXTITEM_OFFSET);
            }
            ms->subitem_counts[ii] = si;

            ii++;
            item_ptr = m68k_read_memory_32(item_ptr + MENUITEM_NEXTITEM_OFFSET);
        }
        ms->item_count = ii;

        mi++;
        menu_ptr = m68k_read_memory_32(menu_ptr + MENU_NEXTMENU_OFFSET);
    }
    handle->menu_count = mi;

    return handle;
}

int lxa_get_menu_count(lxa_menu_strip_t *strip)
{
    if (!strip) return 0;
    return strip->menu_count;
}

int lxa_get_item_count(lxa_menu_strip_t *strip, int menu_idx)
{
    if (!strip || menu_idx < 0 || menu_idx >= strip->menu_count)
        return 0;
    return strip->menus[menu_idx].item_count;
}

bool lxa_get_menu_info(lxa_menu_strip_t *strip,
                       int menu_idx, int item_idx, int sub_idx,
                       lxa_menu_info_t *out)
{
    if (!strip || !out)
        return false;
    if (menu_idx < 0 || menu_idx >= strip->menu_count)
        return false;

    lxa_menu_snapshot_t *ms = &strip->menus[menu_idx];
    memset(out, 0, sizeof(*out));
    out->enabled = true;  /* menus are always enabled at top level */

    if (item_idx == -1)
    {
        /* Top-level menu title */
        uint32_t name_ptr = m68k_read_memory_32(ms->menu_ptr + MENU_MENUNAME_OFFSET);
        lxa_read_emu_string(name_ptr, out->name, sizeof(out->name));
        out->x      = (int16_t)m68k_read_memory_16(ms->menu_ptr + MENU_LEFTEDGE_OFFSET);
        out->y      = (int16_t)m68k_read_memory_16(ms->menu_ptr + MENU_TOPEDGE_OFFSET);
        out->width  = (int16_t)m68k_read_memory_16(ms->menu_ptr + MENU_WIDTH_OFFSET);
        out->height = (int16_t)m68k_read_memory_16(ms->menu_ptr + MENU_HEIGHT_OFFSET);
        return true;
    }

    if (item_idx < 0 || item_idx >= ms->item_count)
        return false;

    uint32_t item_ptr = ms->item_ptrs[item_idx];

    if (sub_idx == -1)
    {
        /* Menu item */
        uint16_t flags = m68k_read_memory_16(item_ptr + MENUITEM_FLAGS_OFFSET);
        out->enabled = (flags & MENUITEM_FLAG_ITEMENABLED) != 0;
        out->checked = (flags & MENUITEM_FLAG_CHECKED) != 0;
        out->has_submenu = (ms->subitem_counts[item_idx] > 0);
        out->x      = (int16_t)m68k_read_memory_16(item_ptr + MENUITEM_LEFTEDGE_OFFSET);
        out->y      = (int16_t)m68k_read_memory_16(item_ptr + MENUITEM_TOPEDGE_OFFSET);
        out->width  = (int16_t)m68k_read_memory_16(item_ptr + MENUITEM_WIDTH_OFFSET);
        out->height = (int16_t)m68k_read_memory_16(item_ptr + MENUITEM_HEIGHT_OFFSET);

        /* Read the item label — it may be an IntuiText or an image.
         * Only ITEMTEXT items have a readable string. */
        if (flags & MENUITEM_FLAG_ITEMTEXT)
        {
            uint32_t fill_ptr = m68k_read_memory_32(item_ptr + MENUITEM_ITEMFILL_OFFSET);
            if (fill_ptr)
            {
                uint32_t text_ptr = m68k_read_memory_32(fill_ptr + INTUITEXT_ITEXT_OFFSET);
                lxa_read_emu_string(text_ptr, out->name, sizeof(out->name));
            }
        }
        return true;
    }

    /* Sub-item */
    if (sub_idx < 0 || sub_idx >= ms->subitem_counts[item_idx])
        return false;

    uint32_t sub_ptr = ms->subitem_ptrs[item_idx][sub_idx];
    uint16_t flags = m68k_read_memory_16(sub_ptr + MENUITEM_FLAGS_OFFSET);
    out->enabled = (flags & MENUITEM_FLAG_ITEMENABLED) != 0;
    out->checked = (flags & MENUITEM_FLAG_CHECKED) != 0;
    out->has_submenu = false;
    out->x      = (int16_t)m68k_read_memory_16(sub_ptr + MENUITEM_LEFTEDGE_OFFSET);
    out->y      = (int16_t)m68k_read_memory_16(sub_ptr + MENUITEM_TOPEDGE_OFFSET);
    out->width  = (int16_t)m68k_read_memory_16(sub_ptr + MENUITEM_WIDTH_OFFSET);
    out->height = (int16_t)m68k_read_memory_16(sub_ptr + MENUITEM_HEIGHT_OFFSET);

    if (flags & MENUITEM_FLAG_ITEMTEXT)
    {
        uint32_t fill_ptr = m68k_read_memory_32(sub_ptr + MENUITEM_ITEMFILL_OFFSET);
        if (fill_ptr)
        {
            uint32_t text_ptr = m68k_read_memory_32(fill_ptr + INTUITEXT_ITEXT_OFFSET);
            lxa_read_emu_string(text_ptr, out->name, sizeof(out->name));
        }
    }
    return true;
}

void lxa_free_menu_strip(lxa_menu_strip_t *strip)
{
    free(strip);
}

