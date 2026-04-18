#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdint.h>
#include <linux/limits.h>
#include <sys/xattr.h>

/* Define HAVE_XATTR for Linux extended attribute support */
#define HAVE_XATTR 1

#include <readline/readline.h>
#include <readline/history.h>

#include "m68k.h"
#include "emucalls.h"
#include "util.h"
#include "vfs.h"
#include "config.h"
#include "display.h"

static float ffp_to_host_float(uint32_t raw);
static uint32_t host_float_to_ffp(float value);
static float host_float_from_bits(uint32_t bits);
static uint32_t host_float_to_bits(float value);
static float host_float_from_m68k_register(int reg);
static void host_float_to_m68k_register(float value, int reg);
static double host_double_from_words(uint32_t hi, uint32_t lo);
static void host_double_to_words(double value, uint32_t *hi, uint32_t *lo);
static double host_double_from_m68k_registers(int hi_reg, int lo_reg);
static void host_double_to_m68k_registers(double value, int hi_reg, int lo_reg);
static void host_double_to_m68k_memory(uint32_t addr, double value);
static int32_t clamp_double_to_long(double value);
static int32_t clamp_float_to_long(float value);
static int32_t compare_double_values(double left, double right);
static int32_t compare_float_values(float left, float right);
static double host_safe_double_divide(double dividend, double divisor);
static float host_safe_float_divide(float dividend, float divisor);
static int errno2Amiga(void);

#ifdef SDL2_FOUND
#include <SDL.h>
#endif

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define DEFAULT_ROM_PATH "../rom/lxa.rom"

#define ROM_SIZE    512 * 1024
#define ROM_START   0xf80000
#define ROM_END     ROM_START + ROM_SIZE - 1

#define CUSTOM_START 0xdff000
#define CUSTOM_END   0xdfffff

/* Extended ROM area (A3000/A4000 extended Kickstart) - return 0 for reads */
#define EXTROM_START 0xf00000
#define EXTROM_END   0xf7ffff

/* Zorro-II expansion bus area - return 0 for reads */
#define ZORRO2_START 0x200000
#define ZORRO2_END   0x9fffff
#define ZORRO2_AUTOCONFIG_START 0xe80000
#define ZORRO2_AUTOCONFIG_END   0xefffff

/* CIA memory ranges
 * CIA-A at 0xBFE001 (active on odd addresses): keyboard, mouse buttons, disk, parallel data
 * CIA-B at 0xBFD000 (active on even addresses): serial, disk motor control
 */
#define CIA_START 0xbfd000
#define CIA_END   0xbfffff

/* Slow RAM / Ranger RAM area (A500 trapdoor, A2000 expansion)
 * 0x00C00000-0x00D7FFFF: Main Slow RAM area (1.5MB)
 * 0x00E00000-0x00E7FFFF: Additional Ranger/expansion RAM (512KB)
 * Some applications (like GFA Basic) probe or use these memory areas.
 * We treat them as non-existent (read returns 0, writes are ignored).
 */
#define SLOWRAM_START    0xc00000
#define SLOWRAM_END      0xd7ffff
#define RANGER_START     0xe00000
#define RANGER_END       0xe7ffff

/* Phase 31: Extended custom chip register definitions */
#define CUSTOM_REG_INTENA   0x09a
#define CUSTOM_REG_INTREQ   0x09c
#define CUSTOM_REG_DMACON   0x096
#define CUSTOM_REG_DMACONR  0x002
#define CUSTOM_REG_VPOSR    0x004  /* Read vertical position (Agnus) */
#define CUSTOM_REG_VHPOSR   0x006  /* Read vert/horiz position */

/* Blitter register offsets (from custom chip base) */
#define CUSTOM_REG_BLTCON0  0x040
#define CUSTOM_REG_BLTCON1  0x042
#define CUSTOM_REG_BLTAFWM  0x044
#define CUSTOM_REG_BLTALWM  0x046
#define CUSTOM_REG_BLTCPTH  0x048
#define CUSTOM_REG_BLTCPTL  0x04a
#define CUSTOM_REG_BLTBPTH  0x04c
#define CUSTOM_REG_BLTBPTL  0x04e
#define CUSTOM_REG_BLTAPTH  0x050
#define CUSTOM_REG_BLTAPTL  0x052
#define CUSTOM_REG_BLTDPTH  0x054
#define CUSTOM_REG_BLTDPTL  0x056
#define CUSTOM_REG_BLTSIZE  0x058
#define CUSTOM_REG_BLTCON0L 0x05b  /* ECS: low byte of BLTCON0 */
#define CUSTOM_REG_BLTSIZV  0x05c  /* ECS: vertical size */
#define CUSTOM_REG_BLTSIZH  0x05e  /* ECS: horizontal size (triggers blit) */
#define CUSTOM_REG_BLTCMOD  0x060
#define CUSTOM_REG_BLTBMOD  0x062
#define CUSTOM_REG_BLTAMOD  0x064
#define CUSTOM_REG_BLTDMOD  0x066
#define CUSTOM_REG_BLTCDAT  0x070
#define CUSTOM_REG_BLTBDAT  0x072
#define CUSTOM_REG_BLTADAT  0x074
#define CUSTOM_REG_DENISEID 0x07c  /* Denise ID register */
#define CUSTOM_REG_ADKCON   0x09e  /* Audio/disk control */
#define CUSTOM_REG_ADKCONR  0x010  /* Audio/disk control read */
#define CUSTOM_REG_POTGO    0x034  /* Pot port control */
#define CUSTOM_REG_POTINP   0x016  /* Pot port read */
#define CUSTOM_REG_JOY0DAT  0x00a  /* Joystick 0 data */
#define CUSTOM_REG_JOY1DAT  0x00c  /* Joystick 1 data */
#define CUSTOM_REG_SERDATR  0x018  /* Serial port data and status read */
#define CUSTOM_REG_INTENAR  0x01c  /* Interrupt enable bits read */
#define CUSTOM_REG_INTREQR  0x01e  /* Interrupt request bits read */
#define CUSTOM_REG_COLOR00  0x180  /* Start of color registers */
#define CUSTOM_REG_COP1LC   0x080  /* Copper 1 location */
#define CUSTOM_REG_COP2LC   0x084  /* Copper 2 location */
#define CUSTOM_REG_COPJMP1  0x088  /* Copper jump 1 */
#define CUSTOM_REG_COPJMP2  0x08a  /* Copper jump 2 */

/* Default SYS: drive - use sys/ subdirectory if present, otherwise current directory */
#define DEFAULT_AMIGA_SYSROOT "sys"

#define MODE_OLDFILE        1005
#define MODE_NEWFILE        1006
#define MODE_READWRITE      1004

#define SHARED_LOCK         -2
#define EXCLUSIVE_LOCK      -1

#define CHANGE_LOCK         0
#define CHANGE_FH           1

#define OFFSET_BEGINNING    -1
#define OFFSET_CURRENT       0
#define OFFSET_END           1

#define LINK_HARD            0
#define LINK_SOFT            1

#define FILE_KIND_REGULAR    42
#define FILE_KIND_CONSOLE    23

#define TD_SECTOR_HOST             512
#define TDERR_NOTSPECIFIED_HOST    20
#define TDERR_BADSECPREAMBLE_HOST  22
#define TDERR_TOOFEWSECS_HOST      26
#define TDERR_WRITEPROT_HOST       28
#define TDERR_DISKCHANGED_HOST     29
#define TDERR_SEEKERROR_HOST       30
#define TDERR_NOMEM_HOST           31
#define FILE_KIND_CON        99   /* CON:/RAW: window (m68k-side handling) */

#define TRACE_BUF_ENTRIES 1000000
#define MAX_BREAKPOINTS       16

/* Core emulator state - exported for lxa_api.c */
uint8_t  g_ram[RAM_SIZE];
uint8_t  g_rom[ROM_SIZE];
bool     g_verbose                       = FALSE;
static bool     g_trace                         = FALSE;
static bool     g_stepping                      = FALSE;
static uint32_t g_next_pc                       = 0;
static int      g_trace_buf[TRACE_BUF_ENTRIES];
static int      g_trace_buf_idx                 = 0;

/*
 * g_debug_active: fast-path gate for cpu_instr_callback().
 * When FALSE, the callback only checks for PC=0 (safety net).
 * When TRUE, full debugging (trace buffer, breakpoints, tracing, stepping).
 */

static bool     g_debug_active                  = FALSE;
bool     g_running                       = TRUE;
char    *g_loadfile                      = NULL;

/* Output capture callback for test drivers */
static void (*g_console_output_hook)(const char *data, int len) = NULL;

#define MAX_ARGS_LEN 4096
char     g_args[MAX_ARGS_LEN]            = {0};
int      g_args_len                      = 0;

static uint32_t g_breakpoints[MAX_BREAKPOINTS];
static int      g_num_breakpoints               = 0;
int      g_rv                            = 0;
static char    *g_sysroot                       = NULL;

/*
 * Hardware blitter emulation state.
 * Shadows the Amiga custom chip blitter registers.
 * Writing to BLTSIZE (or BLTSIZH for ECS) triggers the blit.
 */
static struct {
    uint16_t con0;       /* BLTCON0: shift A (15:12), DMA enables (11:8), minterm (7:0) */
    uint16_t con1;       /* BLTCON1: shift B (15:12), fill/line/direction flags */
    uint16_t afwm;       /* First word mask for channel A */
    uint16_t alwm;       /* Last word mask for channel A */
    uint32_t cpt;        /* Channel C pointer */
    uint32_t bpt;        /* Channel B pointer */
    uint32_t apt;        /* Channel A pointer */
    uint32_t dpt;        /* Channel D (destination) pointer */
    int16_t  cmod;       /* Channel C modulo (signed) */
    int16_t  bmod;       /* Channel B modulo (signed) */
    int16_t  amod;       /* Channel A modulo (signed) */
    int16_t  dmod;       /* Channel D modulo (signed) */
    uint16_t cdat;       /* Channel C data register */
    uint16_t bdat;       /* Channel B data register */
    uint16_t adat;       /* Channel A data register */
    uint16_t sizv;       /* ECS: vertical size (for BLTSIZH trigger) */
} g_blitter = {0};

typedef struct map_sym_s map_sym_t;

struct map_sym_s
{
    map_sym_t *next;
    uint32_t   offset;
    char      *name;
    bool       owns_name;
};

static map_sym_t *_g_map      = NULL;

typedef struct pending_bp_s pending_bp_t;

struct pending_bp_s
{
    pending_bp_t *next;
    char          name[256];
};

static pending_bp_t *_g_pending_bps = NULL;

// interrupts
#define INTENA_MASTER 0x4000
#define INTENA_VBLANK 0x0020

uint16_t g_intena  = INTENA_MASTER | INTENA_VBLANK;
uint16_t g_intreq  = 0;

/* DMA control shadow register.
 * Start with all common DMA channels enabled + master bit:
 *   DMAF_SETCLR (0x8000) is not stored — it's the set/clear direction flag.
 *   DMAF_MASTER  = 0x0200  (DMA master enable)
 *   DMAF_RASTER  = 0x0100  (bitplane DMA)
 *   DMAF_BLITTER = 0x0040  (blitter DMA)
 *   DMAF_SPRITE  = 0x0020  (sprite DMA)
 *   DMAF_DISK    = 0x0010  (disk DMA)
 *   DMAF_COPPER  = 0x0080  (copper DMA)
 * Programs read DMACONR to check whether DMA channels are available.
 * Bit 14 (BLTBUSY) is 0 = blitter is idle (our blitter executes synchronously).
 */
uint16_t g_dmacon = 0x03F0;  /* MASTER|RASTER|COPPER|BLITTER|SPRITE|DISK + audio */

/*
 * Phase 6.5: Timer-driven preemptive multitasking
 *
 * We use a host-side timer (setitimer) to generate periodic interrupts
 * at ~50Hz (PAL VBlank rate). The SIGALRM handler sets a pending interrupt
 * flag which is checked in the main emulation loop.
 *
 * This allows tasks to be preempted even when they don't explicitly call
 * Wait() or other blocking functions.
 */

/* Pending interrupt flags (one bit per level 1-7) - exported for lxa_api.c */
volatile sig_atomic_t g_pending_irq = 0;

/* Last input event for IDCMP handling (Phase 14) */
static display_event_t g_last_event = {0};

/* Timer frequency in microseconds (50Hz = 20000us = 20ms) */
#define TIMER_INTERVAL_US 20000

/* SIGALRM handler - sets Level 3 interrupt pending - exported for lxa_api.c */
void sigalrm_handler(int sig)
{
    (void)sig;
    g_pending_irq |= (1 << 3);  /* Level 3 = VBlank */
}

// ExecBase offsets for task list checking
#define EXECBASE_THISTASK    276
#define EXECBASE_TASKREADY   406
#define EXECBASE_TASKWAIT    420   // 406 + 14 (sizeof(List) = 14 bytes)

/*
 * Phase 3: Lock management
 * 
 * We maintain a table of open directory handles (DIR*) on the host side.
 * Each lock has:
 *   - A unique lock_id (used as BPTR on Amiga side)
 *   - The resolved Linux path
 *   - A DIR* handle for directory iteration (for ExNext)
 *   - Reference count for DupLock
 */
#define MAX_LOCKS 256
#define MAX_RECORD_LOCKS 256

typedef struct lock_entry_s {
    bool in_use;
    char linux_path[PATH_MAX];
    char amiga_path[PATH_MAX]; /* Original Amiga path for NameFromLock */
    DIR *dir;           /* For directory iteration */
    int refcount;       /* For DupLock */
    bool is_dir;        /* True if lock is on a directory */
    int32_t access_mode;
} lock_entry_t;

static lock_entry_t g_locks[MAX_LOCKS];

typedef struct record_lock_entry_s {
    bool in_use;
    dev_t dev;
    ino_t ino;
    uint32_t owner_fh68k;
    uint64_t offset;
    uint64_t length;
    bool exclusive;
} record_lock_entry_t;

static record_lock_entry_t g_record_locks[MAX_RECORD_LOCKS];

/*
 * Phase 45: Async Timer Queue
 *
 * We maintain a list of pending timer requests from timer.device.
 * Each entry stores the m68k IORequest pointer and the absolute wake time.
 * The main loop checks this queue and completes requests when they expire.
 */
#define MAX_TIMER_REQUESTS 64

typedef struct timer_request_s {
    bool in_use;
    bool expired;             /* Set to true when timer has expired */
    uint32_t ioreq_ptr;       /* m68k pointer to IORequest */
    uint64_t wake_time_us;    /* Absolute wake time in microseconds since epoch */
} timer_request_t;

static timer_request_t g_timer_queue[MAX_TIMER_REQUESTS];

#ifdef SDL2_FOUND
#define AUDIO_HOST_CHANNELS 4

typedef struct audio_channel_state_s {
    bool active;
    uint8_t channel;
    uint16_t volume;
    uint16_t period;
    uint32_t length;
    uint32_t cycles;
} audio_channel_state_t;

static audio_channel_state_t g_audio_channels[AUDIO_HOST_CHANNELS];
static SDL_AudioDeviceID g_audio_device;
static bool g_audio_initialized = false;
#endif

#define MAX_NOTIFY_REQUESTS 128

typedef struct notify_entry_s {
    bool in_use;
    uint32_t notify68k;
    char linux_path[PATH_MAX];
    bool pending;
    bool existed;
    time_t mtime;
    off_t size;
    ino_t ino;
    dev_t dev;
} notify_entry_t;

static notify_entry_t g_notify_requests[MAX_NOTIFY_REQUESTS];
static char g_console_input_queue[1024];
static int g_console_input_head = 0;
static int g_console_input_tail = 0;

static bool lxa_host_console_input_empty(void)
{
    return g_console_input_head == g_console_input_tail;
}

static bool lxa_host_console_input_full(void)
{
    return ((g_console_input_head + 1) % (int)sizeof(g_console_input_queue)) == g_console_input_tail;
}

static bool lxa_host_console_input_push(char ch)
{
    if (lxa_host_console_input_full())
        return false;

    g_console_input_queue[g_console_input_head] = ch;
    g_console_input_head = (g_console_input_head + 1) % (int)sizeof(g_console_input_queue);
    return true;
}

static int lxa_host_console_input_pop(void)
{
    int ch;

    if (lxa_host_console_input_empty())
        return -1;

    ch = (unsigned char)g_console_input_queue[g_console_input_tail];
    g_console_input_tail = (g_console_input_tail + 1) % (int)sizeof(g_console_input_queue);
    return ch;
}

static int lxa_host_console_rawkey_to_char(int rawkey, int qualifier)
{
    const bool shift = (qualifier & 0x0003) != 0;

    switch (rawkey & 0x7f)
    {
        case 0x00: return shift ? '~' : '`';
        case 0x01: return shift ? '!' : '1';
        case 0x02: return shift ? '@' : '2';
        case 0x03: return shift ? '#' : '3';
        case 0x04: return shift ? '$' : '4';
        case 0x05: return shift ? '%' : '5';
        case 0x06: return shift ? '^' : '6';
        case 0x07: return shift ? '&' : '7';
        case 0x08: return shift ? '*' : '8';
        case 0x09: return shift ? '(' : '9';
        case 0x0a: return shift ? ')' : '0';
        case 0x0b: return shift ? '_' : '-';
        case 0x0c: return shift ? '+' : '=';
        case 0x0d: return shift ? '|' : '\\';
        case 0x10: return shift ? 'Q' : 'q';
        case 0x11: return shift ? 'W' : 'w';
        case 0x12: return shift ? 'E' : 'e';
        case 0x13: return shift ? 'R' : 'r';
        case 0x14: return shift ? 'T' : 't';
        case 0x15: return shift ? 'Y' : 'y';
        case 0x16: return shift ? 'U' : 'u';
        case 0x17: return shift ? 'I' : 'i';
        case 0x18: return shift ? 'O' : 'o';
        case 0x19: return shift ? 'P' : 'p';
        case 0x1a: return shift ? '{' : '[';
        case 0x1b: return shift ? '}' : ']';
        case 0x20: return shift ? 'A' : 'a';
        case 0x21: return shift ? 'S' : 's';
        case 0x22: return shift ? 'D' : 'd';
        case 0x23: return shift ? 'F' : 'f';
        case 0x24: return shift ? 'G' : 'g';
        case 0x25: return shift ? 'H' : 'h';
        case 0x26: return shift ? 'J' : 'j';
        case 0x27: return shift ? 'K' : 'k';
        case 0x28: return shift ? 'L' : 'l';
        case 0x29: return shift ? ':' : ';';
        case 0x2a: return shift ? '"' : '\'';
        case 0x31: return shift ? 'Z' : 'z';
        case 0x32: return shift ? 'X' : 'x';
        case 0x33: return shift ? 'C' : 'c';
        case 0x34: return shift ? 'V' : 'v';
        case 0x35: return shift ? 'B' : 'b';
        case 0x36: return shift ? 'N' : 'n';
        case 0x37: return shift ? 'M' : 'm';
        case 0x38: return shift ? '<' : ',';
        case 0x39: return shift ? '>' : '.';
        case 0x3a: return shift ? '?' : '/';
        case 0x40: return ' ';
        case 0x41: return '\b';
        case 0x42: return '\t';
        case 0x44: return '\r';
        case 0x45: return 0x1b;
        case 0x46: return 0x7f;
    }

    return -1;
}

bool lxa_host_console_enqueue_char(char ch)
{
    return lxa_host_console_input_push(ch);
}

bool lxa_host_console_enqueue_rawkey(int rawkey, int qualifier, bool down)
{
    int ch;

    if (!down)
        return true;

    ch = lxa_host_console_rawkey_to_char(rawkey, qualifier);
    if (ch < 0)
        return false;

    return lxa_host_console_input_push((char)ch);
}

void lxa_reset_host_state(void)
{
    _g_map = NULL;
    _g_pending_bps = NULL;

    g_trace = FALSE;
    g_stepping = FALSE;
    g_next_pc = 0;
    memset(g_trace_buf, 0, sizeof(g_trace_buf));
    g_trace_buf_idx = 0;
    g_debug_active = FALSE;
    g_running = TRUE;
    g_loadfile = NULL;
    g_console_output_hook = NULL;
    memset(g_args, 0, sizeof(g_args));
    g_args_len = 0;
    memset(g_breakpoints, 0, sizeof(g_breakpoints));
    g_num_breakpoints = 0;
    g_rv = 0;
    g_sysroot = NULL;
    g_last_event = (display_event_t){0};
    memset(g_locks, 0, sizeof(g_locks));
    memset(g_record_locks, 0, sizeof(g_record_locks));
    memset(g_timer_queue, 0, sizeof(g_timer_queue));
    memset(g_notify_requests, 0, sizeof(g_notify_requests));
    memset(g_console_input_queue, 0, sizeof(g_console_input_queue));
    g_console_input_head = 0;
    g_console_input_tail = 0;
    g_pending_irq = 0;
#ifdef SDL2_FOUND
    memset(g_audio_channels, 0, sizeof(g_audio_channels));
    g_audio_device = 0;
    g_audio_initialized = false;
#endif
}

static int _linux_path_to_amiga(const char *linux_path, char *amiga_buf, size_t bufsize);

/* Get current time in microseconds since epoch */
static uint64_t _timer_get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

static void __attribute__((unused)) _audio_init(void)
{
#ifdef SDL2_FOUND
    SDL_AudioSpec want;

    if (g_audio_initialized)
    {
        return;
    }

    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO))
    {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        {
            DPRINTF(LOG_WARNING, "lxa: SDL audio init failed: %s\n", SDL_GetError());
            return;
        }
    }

    memset(&want, 0, sizeof(want));
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;

    g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (g_audio_device == 0)
    {
        DPRINTF(LOG_WARNING, "lxa: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);
    g_audio_initialized = true;
#endif
}

static void __attribute__((unused)) _audio_shutdown(void)
{
#ifdef SDL2_FOUND
    if (g_audio_initialized)
    {
        SDL_ClearQueuedAudio(g_audio_device);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        g_audio_initialized = false;
    }
#endif

    return;
}

static void _audio_queue_fragment(uint32_t packed,
                                  uint32_t data_ptr,
                                  uint32_t length,
                                  uint32_t period,
                                  uint32_t volume)
{
#ifdef SDL2_FOUND
    uint8_t channel = packed & 0xff;
    uint32_t cycles = packed >> 8;
    int16_t mixbuf[4096];
    uint32_t count;
    uint32_t i;

    if (channel >= AUDIO_HOST_CHANNELS)
    {
        return;
    }

    _audio_init();

    if (!g_audio_initialized)
    {
        return;
    }

    if (length > 4096)
    {
        length = 4096;
    }

    count = length;
    if (count == 0)
    {
        return;
    }

    if (volume > 64)
    {
        volume = 64;
    }

    for (i = 0; i < count; i++)
    {
        int8_t sample = (int8_t)m68k_read_memory_8(data_ptr + i);
        mixbuf[i] = (int16_t)((sample * (int32_t)volume * 256) / 64);
    }

    SDL_ClearQueuedAudio(g_audio_device);
    SDL_QueueAudio(g_audio_device, mixbuf, count * sizeof(int16_t));

    g_audio_channels[channel].active = true;
    g_audio_channels[channel].channel = channel;
    g_audio_channels[channel].volume = (uint16_t)volume;
    g_audio_channels[channel].period = (uint16_t)period;
    g_audio_channels[channel].length = count;
    g_audio_channels[channel].cycles = cycles;
#else
#endif

    (void)packed;
    (void)data_ptr;
    (void)length;
    (void)period;
    (void)volume;
}

/* Add a timer request to the queue.
 * The delay is specified as seconds + microseconds from NOW.
 */
static bool _timer_add_request(uint32_t ioreq_ptr, uint32_t delay_secs, uint32_t delay_micros)
{
    /* Compute absolute wake time by adding delay to current host time */
    uint64_t now = _timer_get_time_us();
    uint64_t delay_us = (uint64_t)delay_secs * 1000000ULL + (uint64_t)delay_micros;
    uint64_t wake_time_us = now + delay_us;
    
    DPRINTF(LOG_DEBUG, "lxa: _timer_add_request: ioreq=0x%08x delay=%u.%06u now=%" PRIu64 " wake=%" PRIu64 "\n",
            ioreq_ptr, delay_secs, delay_micros, now, wake_time_us);
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (!g_timer_queue[i].in_use) {
            g_timer_queue[i].in_use = true;
            g_timer_queue[i].expired = false;
            g_timer_queue[i].ioreq_ptr = ioreq_ptr;
            g_timer_queue[i].wake_time_us = wake_time_us;
            return true;
        }
    }
    
    DPRINTF(LOG_ERROR, "lxa: _timer_add_request: queue full!\n");
    return false;
}

/* Remove a timer request from the queue */
static bool _timer_remove_request(uint32_t ioreq_ptr)
{
    DPRINTF(LOG_DEBUG, "lxa: _timer_remove_request: ioreq=0x%08x\n", ioreq_ptr);
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && g_timer_queue[i].ioreq_ptr == ioreq_ptr) {
            g_timer_queue[i].in_use = false;
            return true;
        }
    }
    return false;
}

/* Check and mark expired timer requests - exported for lxa_api.c
 * This is called from the main emulation loop.
 * It only marks timers as expired - the actual ReplyMsg() is done from the
 * m68k side via the _timer_VBlankHook() function.
 */
int _timer_check_expired(void)
{
    uint64_t now = _timer_get_time_us();
    int expired_count = 0;
    
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && !g_timer_queue[i].expired) {
            uint64_t wake_time = g_timer_queue[i].wake_time_us;
            DPRINTF(LOG_DEBUG, "lxa: _timer_check_expired: [%d] now=%" PRIu64 " wake=%" PRIu64 " diff=%lld\n", 
                    i, now, wake_time, (long long)(wake_time - now));
            
            if (wake_time <= now) {
                DPRINTF(LOG_DEBUG, "lxa: _timer_check_expired: ioreq=0x%08x marking as expired\n", 
                        g_timer_queue[i].ioreq_ptr);
            
                /* Mark as expired - it will be retrieved by EMU_CALL_TIMER_GET_EXPIRED */
                g_timer_queue[i].expired = true;
                expired_count++;
            }
        }
    }
    
    return expired_count;
}

/* Get the next expired timer request.
 * Returns the m68k IORequest pointer and marks the entry as no longer in use.
 * Returns 0 if no expired timers are available.
 */
static uint32_t _timer_get_expired(void)
{
    for (int i = 0; i < MAX_TIMER_REQUESTS; i++) {
        if (g_timer_queue[i].in_use && g_timer_queue[i].expired) {
            uint32_t ioreq = g_timer_queue[i].ioreq_ptr;
            
            DPRINTF(LOG_DEBUG, "lxa: _timer_get_expired: returning ioreq=0x%08x\n", ioreq);
            
            /* Mark as no longer in use */
            g_timer_queue[i].in_use = false;
            g_timer_queue[i].expired = false;
            
            return ioreq;
        }
    }
    
    return 0;
}

/* Amiga DOS protection flag bits */
#define FIBF_DELETE     (1<<0)  /* File is deletable */
#define FIBF_EXECUTE    (1<<1)  /* File is executable */
#define FIBF_WRITE      (1<<2)  /* File is writeable */
#define FIBF_READ       (1<<3)  /* File is readable */
#define FIBF_ARCHIVE    (1<<4)  /* File has been archived */
#define FIBF_PURE       (1<<5)  /* Pure (re-entrant) */
#define FIBF_SCRIPT     (1<<6)  /* Script file */

/* Amiga DOS file types */
#define ST_ROOT         1
#define ST_USERDIR      2
#define ST_SOFTLINK     3
#define ST_LINKDIR      4
#define ST_FILE        -3
#define ST_LINKFILE    -4

/* Amiga lock modes */
#define ACCESS_READ    -2  /* SHARED_LOCK */
#define ACCESS_WRITE   -1  /* EXCLUSIVE_LOCK */

/* Amiga error codes */
#define ERROR_NO_FREE_STORE      103
#define ERROR_BAD_NUMBER         115
#define ERROR_REQUIRED_ARG_MISSING 116
#define ERROR_OBJECT_IN_USE      202
#define ERROR_OBJECT_EXISTS      203
#define ERROR_DIR_NOT_FOUND      204
#define ERROR_OBJECT_NOT_FOUND   205
#define ERROR_BAD_STREAM_NAME    206
#define ERROR_OBJECT_TOO_LARGE   207
#define ERROR_ACTION_NOT_KNOWN   209
#define ERROR_INVALID_LOCK       211
#define ERROR_OBJECT_WRONG_TYPE  212
#define ERROR_DISK_NOT_VALIDATED 213
#define ERROR_DISK_WRITE_PROTECTED 214
#define ERROR_RENAME_ACROSS_DEVICES 215
#define ERROR_DIRECTORY_NOT_EMPTY 216
#define ERROR_TOO_MANY_LEVELS    217
#define ERROR_SEEK_ERROR         219
#define ERROR_DISK_FULL          221
#define ERROR_DELETE_PROTECTED   222
#define ERROR_WRITE_PROTECTED    223
#define ERROR_READ_PROTECTED     224
#define ERROR_NOT_A_DOS_DISK     225
#define ERROR_NO_MORE_ENTRIES    232
#define ERROR_RECORD_NOT_LOCKED  240
#define ERROR_LOCK_COLLISION     241
#define ERROR_LOCK_TIMEOUT       242
#define ERROR_BUFFER_OVERFLOW    303

#define DVP_DVP_PORT     0
#define DVP_DVP_LOCK     4
#define DVP_DVP_FLAGS    8
#define DVP_DVP_DEVNODE  12

#define DVPF_UNLOCK      (1UL << 0)
#define DVPF_ASSIGN      (1UL << 1)

#define NRF_SEND_MESSAGE 1
#define NRF_SEND_SIGNAL  2
#define NRF_NOTIFY_INITIAL 16

#define AMIGA_EPOCH_OFFSET 252460800

/* Allocate a new lock entry */
static int _lock_alloc(void)
{
    for (int i = 1; i < MAX_LOCKS; i++) {
        if (!g_locks[i].in_use) {
            memset(&g_locks[i], 0, sizeof(lock_entry_t));
            g_locks[i].in_use = true;
            g_locks[i].refcount = 1;
            return i;
        }
    }
    return 0; /* No free slot */
}

/* Free a lock entry */
static void _lock_free(int lock_id)
{
    if (lock_id <= 0 || lock_id >= MAX_LOCKS) return;
    if (!g_locks[lock_id].in_use) return;
    
    if (g_locks[lock_id].dir) {
        closedir(g_locks[lock_id].dir);
        g_locks[lock_id].dir = NULL;
    }
    g_locks[lock_id].in_use = false;
}

/* Get lock entry */
static lock_entry_t *_lock_get(int lock_id)
{
    if (lock_id <= 0 || lock_id >= MAX_LOCKS) return NULL;
    if (!g_locks[lock_id].in_use) return NULL;
    return &g_locks[lock_id];
}

static uint64_t _record_lock_end(uint64_t offset, uint64_t length)
{
    if (length > UINT64_MAX - offset)
    {
        return UINT64_MAX;
    }

    return offset + length;
}

static bool _record_locks_overlap(uint64_t offset_a, uint64_t length_a,
                                  uint64_t offset_b, uint64_t length_b)
{
    uint64_t end_a;
    uint64_t end_b;

    if (length_a == 0 || length_b == 0)
    {
        return false;
    }

    end_a = _record_lock_end(offset_a, length_a);
    end_b = _record_lock_end(offset_b, length_b);

    return offset_a < end_b && offset_b < end_a;
}

static int _record_lock_alloc(void)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        if (!g_record_locks[i].in_use)
        {
            memset(&g_record_locks[i], 0, sizeof(g_record_locks[i]));
            g_record_locks[i].in_use = true;
            return i;
        }
    }

    return -1;
}

static int _record_lock_find_conflict(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                                      uint64_t offset, uint64_t length, bool exclusive)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        record_lock_entry_t *entry = &g_record_locks[i];

        if (!entry->in_use)
        {
            continue;
        }

        if (entry->dev != dev || entry->ino != ino)
        {
            continue;
        }

        if (entry->owner_fh68k == owner_fh68k)
        {
            continue;
        }

        if (!_record_locks_overlap(entry->offset, entry->length, offset, length))
        {
            continue;
        }

        if (entry->exclusive || exclusive)
        {
            return i;
        }
    }

    return -1;
}

static bool _record_lock_add(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                             uint64_t offset, uint64_t length, bool exclusive)
{
    int slot = _record_lock_alloc();

    if (slot < 0)
    {
        return false;
    }

    g_record_locks[slot].dev = dev;
    g_record_locks[slot].ino = ino;
    g_record_locks[slot].owner_fh68k = owner_fh68k;
    g_record_locks[slot].offset = offset;
    g_record_locks[slot].length = length;
    g_record_locks[slot].exclusive = exclusive;

    return true;
}

static bool _record_lock_remove(dev_t dev, ino_t ino, uint32_t owner_fh68k,
                                 uint64_t offset, uint64_t length)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        record_lock_entry_t *entry = &g_record_locks[i];

        if (!entry->in_use)
        {
            continue;
        }

        if (entry->dev != dev || entry->ino != ino)
        {
            continue;
        }

        if (entry->owner_fh68k != owner_fh68k)
        {
            continue;
        }

        if (entry->offset != offset || entry->length != length)
        {
            continue;
        }

        entry->in_use = false;
        return true;
    }

    return false;
}

static bool _dos_lock_info_from_fh(uint32_t fh68k, dev_t *dev_out, ino_t *ino_out)
{
    int fd;
    int kind;
    struct stat st;

    if (fh68k == 0 || !dev_out || !ino_out)
    {
        return false;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (kind != FILE_KIND_REGULAR)
    {
        return false;
    }

    if (fstat(fd, &st) != 0)
    {
        return false;
    }

    *dev_out = st.st_dev;
    *ino_out = st.st_ino;
    return true;
}

static bool _dos_change_mode(uint32_t type, uint32_t object, int32_t new_mode, uint32_t err68k)
{
    dev_t dev;
    ino_t ino;
    lock_entry_t *lock;
    uint32_t fh68k;
    uint32_t lock_id;
    uint32_t err = 0;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_change_mode(): type=%u object=0x%08x new_mode=%d err=0x%08x\n",
            type, object, new_mode, err68k);

    if (type != CHANGE_LOCK && type != CHANGE_FH)
    {
        err = ERROR_BAD_NUMBER;
        goto fail;
    }

    if (new_mode != SHARED_LOCK && new_mode != EXCLUSIVE_LOCK)
    {
        err = ERROR_BAD_NUMBER;
        goto fail;
    }

    if (type == CHANGE_FH)
    {
        fh68k = object << 2;
        if (!_dos_lock_info_from_fh(fh68k, &dev, &ino))
        {
            err = ERROR_OBJECT_WRONG_TYPE;
            goto fail;
        }
    }
    else
    {
        lock_id = object;
        lock = _lock_get(lock_id);
        if (!lock)
        {
            err = ERROR_INVALID_LOCK;
            goto fail;
        }

        {
            struct stat st;
            if (stat(lock->linux_path, &st) != 0)
            {
                err = errno2Amiga();
                goto fail;
            }
            dev = st.st_dev;
            ino = st.st_ino;
        }
    }

    if (new_mode == EXCLUSIVE_LOCK)
    {
        for (int i = 0; i < MAX_LOCKS; i++)
        {
            lock_entry_t *entry = &g_locks[i];
            struct stat st;

            if (!entry->in_use)
            {
                continue;
            }

            if (type == CHANGE_LOCK && i == (int)object)
            {
                continue;
            }

            if (stat(entry->linux_path, &st) != 0)
            {
                continue;
            }

            if (st.st_dev == dev && st.st_ino == ino)
            {
                err = ERROR_OBJECT_IN_USE;
                goto fail;
            }
        }
    }

    if (type == CHANGE_FH)
    {
        DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): fh change accepted\n");
    }
    else
    {
        lock = _lock_get(object);
        if (!lock)
        {
            err = ERROR_INVALID_LOCK;
            goto fail;
        }

        lock->access_mode = new_mode;
        DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): lock %u access now %d\n", object, new_mode);
    }

    if (err68k)
    {
        m68k_write_memory_32(err68k, 0);
    }

    return true;

fail:
    if (err68k)
    {
        m68k_write_memory_32(err68k, err);
    }

    DPRINTF(LOG_DEBUG, "lxa: _dos_change_mode(): fail err=%u\n", err);
    return false;
}

static void _record_lock_release_all(uint32_t owner_fh68k)
{
    for (int i = 0; i < MAX_RECORD_LOCKS; i++)
    {
        if (g_record_locks[i].in_use && g_record_locks[i].owner_fh68k == owner_fh68k)
        {
            g_record_locks[i].in_use = false;
        }
    }
}

/*
 * Check if a List is empty by examining its lh_Head pointer.
 * An empty List has lh_Head pointing to &lh_Tail (list_addr + 4).
 * A non-empty List has lh_Head pointing to the first Node, whose
 * ln_Succ is not NULL.
 */
static bool is_list_empty(uint32_t list_addr)
{
    uint32_t lh_Head = m68k_read_memory_32(list_addr);      // lh_Head at offset 0
    uint32_t lh_Tail_addr = list_addr + 4;                  // &lh_Tail is at offset 4
    return lh_Head == lh_Tail_addr;
}

/*
 * Check if there are other tasks running besides the current one.
 * Returns true if there are tasks in TaskReady or TaskWait.
 */
static bool other_tasks_running(void)
{
    uint32_t sysbase = m68k_read_memory_32(4);  // SysBase at address 4
    
    bool ready_empty = is_list_empty(sysbase + EXECBASE_TASKREADY);
    bool wait_empty = is_list_empty(sysbase + EXECBASE_TASKWAIT);
    
    DPRINTF(LOG_DEBUG, "*** other_tasks_running: sysbase=0x%08x, TaskReady empty=%d, TaskWait empty=%d\n",
            sysbase, ready_empty, wait_empty);
    
    if (!ready_empty)
        return true;
    if (!wait_empty)
        return true;
    
    return false;
}

static void _debug(uint32_t pc);

#define HEXDUMP_COLS 16

static bool isprintable (char c)
{
    return (c>=32) & (c<127);
}

void hexdump (int lvl, uint32_t offset, uint32_t len)
{
    uint32_t i, j;

    for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
    {
        /* print offset */
        if (i % HEXDUMP_COLS == 0)
        {
            LPRINTF(lvl, "0x%06x: ", offset+i);
        }

        /* print hex data */
        if (i < len)
        {
            char c = m68k_read_memory_8 (offset + i);
            LPRINTF(lvl, "%02x ", 0xFF & c);
        }
        else /* end of block, just aligning for ASCII dump */
        {
            LPRINTF(lvl, "   ");
        }

        /* print ASCII dump */
        if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
        {
            for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
            {
                char c = m68k_read_memory_8 (offset + j);

                if (j >= len) /* end of block, not really printing */
                {
                    LPRINTF(lvl, " ");
                }
                else if (isprintable(c)) /* printable char */
                {
                    LPRINTF(lvl, "%c", c);
                }
                else /* other char */
                {
                    LPRINTF(lvl, ".");
                }
            }
            LPRINTF(lvl, "\n");
        }
    }
}

static bool _symtab_add_owned (char *name, uint32_t offset, bool owns_name)
{
    map_sym_t *m = malloc (sizeof (*m));
    if (!m)
    {
        fprintf (stderr, "OOM\n");
        return false;
    }
    m->next   = NULL;
    m->offset = offset;
    m->name   = name;
    m->owns_name = owns_name;

    // add to sorted map list

    map_sym_t *prev = _g_map;

    while (prev && prev->next && prev->next->offset < offset)
        prev = prev->next;

    if (!prev || prev->offset > offset)
    {
        m->next = _g_map;
        _g_map = m;
    }
    else
    {
        m->next = prev->next;
        prev->next = m;
    }
    return true;
}

static bool _symtab_add (char *name, uint32_t offset)
{
    return _symtab_add_owned(name, offset, false);
}

static uint32_t _find_symbol (const char *name)
{
    for (map_sym_t *sym = _g_map; sym; sym=sym->next)
    {
        if (!(strcmp(sym->name, name)))
            return sym->offset;
    }

    return 0;
}

/* Phase 31: Helper function to get custom chip register name for logging */
static const char *_custom_reg_name(uint16_t reg) __attribute__((unused));
static const char *_custom_reg_name(uint16_t reg)
{
    switch (reg) {
        case CUSTOM_REG_DMACONR: return "DMACONR";
        case CUSTOM_REG_VPOSR: return "VPOSR";
        case CUSTOM_REG_VHPOSR: return "VHPOSR";
        case CUSTOM_REG_ADKCONR: return "ADKCONR";
        case CUSTOM_REG_POTINP: return "POTINP";
        case CUSTOM_REG_JOY0DAT: return "JOY0DAT";
        case CUSTOM_REG_JOY1DAT: return "JOY1DAT";
        case CUSTOM_REG_DENISEID: return "DENISEID";
        case CUSTOM_REG_BLTCON0: return "BLTCON0";
        case CUSTOM_REG_BLTCON1: return "BLTCON1";
        case CUSTOM_REG_BLTAFWM: return "BLTAFWM";
        case CUSTOM_REG_BLTALWM: return "BLTALWM";
        case CUSTOM_REG_BLTCPTH: return "BLTCPTH";
        case CUSTOM_REG_BLTCPTL: return "BLTCPTL";
        case CUSTOM_REG_BLTBPTH: return "BLTBPTH";
        case CUSTOM_REG_BLTBPTL: return "BLTBPTL";
        case CUSTOM_REG_BLTAPTH: return "BLTAPTH";
        case CUSTOM_REG_BLTAPTL: return "BLTAPTL";
        case CUSTOM_REG_BLTDPTH: return "BLTDPTH";
        case CUSTOM_REG_BLTDPTL: return "BLTDPTL";
        case CUSTOM_REG_BLTSIZE: return "BLTSIZE";
        case CUSTOM_REG_BLTSIZV: return "BLTSIZV";
        case CUSTOM_REG_BLTSIZH: return "BLTSIZH";
        case CUSTOM_REG_BLTCMOD: return "BLTCMOD";
        case CUSTOM_REG_BLTBMOD: return "BLTBMOD";
        case CUSTOM_REG_BLTAMOD: return "BLTAMOD";
        case CUSTOM_REG_BLTDMOD: return "BLTDMOD";
        case CUSTOM_REG_BLTCDAT: return "BLTCDAT";
        case CUSTOM_REG_BLTBDAT: return "BLTBDAT";
        case CUSTOM_REG_BLTADAT: return "BLTADAT";
        case CUSTOM_REG_DMACON: return "DMACON";
        case CUSTOM_REG_INTENA: return "INTENA";
        case CUSTOM_REG_INTREQ: return "INTREQ";
        case CUSTOM_REG_ADKCON: return "ADKCON";
        case CUSTOM_REG_POTGO: return "POTGO";
        case CUSTOM_REG_COP1LC: return "COP1LC";
        case CUSTOM_REG_COP2LC: return "COP2LC";
        case CUSTOM_REG_COPJMP1: return "COPJMP1";
        case CUSTOM_REG_COPJMP2: return "COPJMP2";
        default:
            if (reg >= CUSTOM_REG_COLOR00 && reg < CUSTOM_REG_COLOR00 + 64) {
                return "COLORxx";
            }
            return "UNKNOWN";
    }
}

static inline uint8_t mread8 (uint32_t address)
{
    /* Note: Reading from address 0 is valid - it's chip RAM on Amiga.
     * The previous NULL pointer check was removed as it caused false
     * positives with applications that legitimately read from low memory.
     */

    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return g_ram[addr];
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        uint32_t addr = address - ROM_START;
        return g_rom[addr];
    }
    else if ((address >= EXTROM_START) && (address <= EXTROM_END))
    {
        /* Extended ROM area (A3000/A4000) - return 0 to indicate no extended ROM */
        return 0;
    }
    else if ((address >= 0x00A00000) && (address < CUSTOM_START))
    {
        /* RAM overflow area (10MB - just before custom chips at 0xDFF000) - some apps allocate
         * to end of RAM and overflow into what would be expansion RAM, slow RAM, or CIA areas.
         * Return 0 for reads in this area. */
        DPRINTF (LOG_DEBUG, "lxa: mread8 RAM overflow area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= 0x01000000) && (address <= 0x0FFFFFFF))
    {
        /* 16MB-256MB range - Zorro-III expansion area, return 0xFF (no expansion)
         * This covers addresses 0x01000000-0x0FFFFFFF which are used for:
         * - Zorro-III memory expansion
         * - Fast RAM on accelerator cards
         * Returning 0xFF indicates no memory/expansion present.
         */
        return 0xFF;
    }
    else if ((address >= ZORRO2_AUTOCONFIG_START) && (address <= ZORRO2_AUTOCONFIG_END))
    {
        /* Zorro-II autoconfig space - return 0 (no boards) */
        return 0;
    }
    else if ((address >= SLOWRAM_START) && (address <= SLOWRAM_END))
    {
        /* Slow RAM / Ranger RAM area - return 0 (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mread8 Slow RAM area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= RANGER_START) && (address <= RANGER_END))
    {
        /* Ranger/expansion RAM area - return 0 (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mread8 Ranger RAM area 0x%08x -> 0x00\n", address);
        return 0;
    }
    else if ((address >= CIA_START) && (address <= CIA_END))
    {
        /* CIA chip area - return sensible defaults
         * 0xFF = all bits high = no buttons pressed, no keys, inactive signals
         */
        DPRINTF (LOG_DEBUG, "lxa: mread8 CIA area 0x%08x -> 0xFF\n", address);
        return 0xff;
    }
    else if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        /* Phase 31: Custom chip area reads (Denise, Agnus, Paula) */
        uint16_t reg = address - CUSTOM_START;
        uint8_t result = 0;
        
        /* Return sensible defaults for common read registers */
        switch (reg & ~1) {  /* Use even address for word-aligned registers */
            case CUSTOM_REG_VPOSR:    /* Vertical position - return 0 (line 0, PAL long frame) */
                result = (reg & 1) ? 0x00 : 0x00;
                break;
            case CUSTOM_REG_VHPOSR:   /* Horiz/vert position - return 0 */
                result = 0;
                break;
            case CUSTOM_REG_JOY0DAT:  /* Joystick 0 - no movement */
                result = 0;
                break;
            case CUSTOM_REG_JOY1DAT:  /* Joystick 1 - no movement */
                result = 0;
                break;
            case CUSTOM_REG_DMACONR:  /* DMA control read - return shadow register */
                /* Bit 14 (BLTBUSY) is always 0 since our blitter executes synchronously */
                result = (reg & 1) ? (g_dmacon & 0xFF) : ((g_dmacon >> 8) & 0xFF);
                break;
            case CUSTOM_REG_DENISEID: /* Denise ID - return 0xFC for ECS Denise */
                result = (reg & 1) ? 0xFC : 0x00;
                break;
            case CUSTOM_REG_POTINP:   /* Pot port read - return 0xFF (no pots) */
                result = 0xFF;
                break;
            case CUSTOM_REG_SERDATR:  /* Serial port data and status read */
                /* Bit 13 (TBE) = Transmit Buffer Empty - always set (ready to transmit)
                 * Bit 12 (TSRE) = Transmit Shift Reg Empty - always set
                 * Bit 14 (RBF) = Receive Buffer Full - always 0 (no data received)
                 * Bit 11 (RBF) = not used
                 * Lower bits = received data byte (0)
                 */
                {
                    uint16_t serdatr = 0x3000;  /* TBE + TSRE set */
                    result = (reg & 1) ? (serdatr & 0xFF) : ((serdatr >> 8) & 0xFF);
                }
                break;
            case CUSTOM_REG_INTENAR:  /* Interrupt enable bits read */
                result = (reg & 1) ? (g_intena & 0xFF) : ((g_intena >> 8) & 0xFF);
                break;
            case CUSTOM_REG_INTREQR:  /* Interrupt request bits read */
                result = (reg & 1) ? (g_intreq & 0xFF) : ((g_intreq >> 8) & 0xFF);
                break;
            case CUSTOM_REG_ADKCONR:  /* Audio/disk control read - return 0 */
                result = 0;
                break;
            default:
                result = 0;
                break;
        }
        
        DPRINTF (LOG_DEBUG, "lxa: mread8 CUSTOM %s (0x%08x/0x%03x) -> 0x%02x\n",
                _custom_reg_name(reg & ~1), address, reg, result);
        return result;
    }
    else
    {
        /* 
         * Invalid address - print warning but don't enter debugger.
         * Some programs may read from invalid addresses intentionally
         * (e.g., checking for expansion boards, sentinel values, etc.)
         * Return 0 and let the program continue.
         */
        static int invalid_read_count = 0;
        if (invalid_read_count < 10) {
            printf("WARNING: mread8 at invalid address 0x%08x (PC=0x%08x)\n", 
                   address, m68k_get_reg(NULL, M68K_REG_PC));
            invalid_read_count++;
            if (invalid_read_count == 10) {
                printf("WARNING: suppressing further invalid read warnings\n");
            }
        }
    }

    return 0;
}

unsigned int m68k_read_memory_8 (unsigned int address)
{
    uint8_t b = mread8(address);
    //if (g_trace)
    //    DPRINTF(LOG_DEBUG, "READ8   at 0x%08x -> 0x%02x\n", address, b);
    return b;
}

unsigned int m68k_read_memory_16 (unsigned int address)
{
    uint16_t w= (mread8(address)<<8) | mread8(address+1);

    //if (g_trace)
    //    DPRINTF(LOG_DEBUG, "READ16  at 0x%08x -> 0x%04x\n", address, w);
    return w;
}

unsigned int m68k_read_memory_32 (unsigned int address)
{
    uint32_t l = (mread8(address)<<24) | (mread8(address+1)<<16) | (mread8(address+2)<<8) | mread8(address+3);
    // if (g_trace)
    //     DPRINTF("READ32  at 0x%08x -> 0x%08x\n", address, l);
    return l;
}

/*
 * _blitter_execute() - Software emulation of the Amiga hardware blitter.
 *
 * Called when BLTSIZE (OCS) or BLTSIZH (ECS) is written, triggering a blit.
 * Reads source data from channels A/B/C (DMA or data register), applies
 * barrel shift (A/B), first/last word masks (A), minterm logic function,
 * and writes result to channel D.
 *
 * Supports:
 *   - All 256 minterm logic functions
 *   - Channel A/B barrel shift (ASH/BSH, 0-15 bits)
 *   - First/last word masks (BLTAFWM/BLTALWM)
 *   - Per-channel DMA enable/disable (data register fallback)
 *   - Ascending and descending (reverse) blit direction
 *   - Inclusive-OR and exclusive-OR fill modes
 *   - Per-channel signed modulo
 *
 * Not supported (not needed for current apps):
 *   - Line draw mode (BLTCON1 bit 0)
 */
static void _blitter_execute (uint16_t width_words, uint16_t height)
{
    /* Zero means maximum */
    if (width_words == 0) width_words = 64;
    if (height == 0) height = 1024;

    uint16_t con0 = g_blitter.con0;
    uint16_t con1 = g_blitter.con1;
    uint8_t  minterm = con0 & 0xff;
    bool     use_a = (con0 & 0x0800) != 0;
    bool     use_b = (con0 & 0x0400) != 0;
    bool     use_c = (con0 & 0x0200) != 0;
    bool     use_d = (con0 & 0x0100) != 0;
    uint16_t ash   = (con0 >> 12) & 0xf;
    uint16_t bsh   = (con1 >> 12) & 0xf;
    bool     desc  = (con1 & 0x02) != 0;  /* Descending direction */
    bool     fill_or  = (con1 & 0x08) != 0;
    bool     fill_xor = (con1 & 0x10) != 0;
    bool     fill_carry_in = (con1 & 0x04) != 0;

    uint32_t apt = g_blitter.apt;
    uint32_t bpt = g_blitter.bpt;
    uint32_t cpt = g_blitter.cpt;
    uint32_t dpt = g_blitter.dpt;

    int16_t  amod = g_blitter.amod;
    int16_t  bmod = g_blitter.bmod;
    int16_t  cmod = g_blitter.cmod;
    int16_t  dmod = g_blitter.dmod;

    /* Barrel shifter state: holds previous word for cross-word shifting */
    uint16_t a_prev = 0;
    uint16_t b_prev = 0;

    DPRINTF (LOG_DEBUG, "lxa: BLITTER: con0=0x%04x con1=0x%04x size=%dx%d "
             "A=%d B=%d C=%d D=%d ash=%d bsh=%d desc=%d minterm=0x%02x\n",
             con0, con1, width_words, height,
             use_a, use_b, use_c, use_d, ash, bsh, desc, minterm);

    int ptr_step = desc ? -2 : 2;

    for (uint16_t row = 0; row < height; row++)
    {
        /* Reset barrel shifter at start of each row */
        a_prev = 0;
        b_prev = 0;

        /* Fill mode carry state (per-row) */
        bool fill_carry = fill_carry_in;

        for (uint16_t col = 0; col < width_words; col++)
        {
            /* --- Read source channels --- */

            uint16_t a_data;
            if (use_a)
            {
                if (apt < RAM_SIZE - 1)
                    a_data = (g_ram[apt] << 8) | g_ram[apt + 1];
                else
                    a_data = 0;
                apt += ptr_step;
            }
            else
            {
                a_data = g_blitter.adat;
            }

            uint16_t b_data;
            if (use_b)
            {
                if (bpt < RAM_SIZE - 1)
                    b_data = (g_ram[bpt] << 8) | g_ram[bpt + 1];
                else
                    b_data = 0;
                bpt += ptr_step;
            }
            else
            {
                b_data = g_blitter.bdat;
            }

            uint16_t c_data;
            if (use_c)
            {
                if (cpt < RAM_SIZE - 1)
                    c_data = (g_ram[cpt] << 8) | g_ram[cpt + 1];
                else
                    c_data = 0;
                cpt += ptr_step;
            }
            else
            {
                c_data = g_blitter.cdat;
            }

            /* --- Apply barrel shift to channels A and B --- */

            uint16_t a_shifted;
            if (ash == 0)
            {
                a_shifted = a_data;
            }
            else
            {
                /* Combine previous and current word, shift right by ash bits */
                uint32_t combined = ((uint32_t)a_prev << 16) | a_data;
                a_shifted = (combined >> ash) & 0xffff;
            }
            a_prev = a_data;

            uint16_t b_shifted;
            if (bsh == 0)
            {
                b_shifted = b_data;
            }
            else
            {
                uint32_t combined = ((uint32_t)b_prev << 16) | b_data;
                b_shifted = (combined >> bsh) & 0xffff;
            }
            b_prev = b_data;

            /* --- Apply first/last word masks to channel A --- */

            uint16_t a_masked = a_shifted;
            if (col == 0)
                a_masked &= g_blitter.afwm;
            if (col == width_words - 1)
                a_masked &= g_blitter.alwm;
            /* If only 1 word wide, both masks are applied (handled above) */

            /* --- Evaluate minterm logic function --- */
            /* For each bit: index = (A<<2)|(B<<1)|C, result = (minterm>>index)&1 */

            uint16_t result = 0;
            for (int bit = 15; bit >= 0; bit--)
            {
                uint16_t mask = 1 << bit;
                int a_bit = (a_masked & mask) ? 1 : 0;
                int b_bit = (b_shifted & mask) ? 1 : 0;
                int c_bit = (c_data & mask) ? 1 : 0;
                int idx = (a_bit << 2) | (b_bit << 1) | c_bit;
                if (minterm & (1 << idx))
                    result |= mask;
            }

            /* --- Apply fill mode (if enabled) --- */

            if (fill_or || fill_xor)
            {
                uint16_t filled = 0;
                /*
                 * Fill processes bits right-to-left within each word.
                 * When a set bit is encountered, fill_carry toggles.
                 * While carry is set, bits are filled (OR or XOR).
                 */
                for (int bit = 0; bit <= 15; bit++)
                {
                    uint16_t mask = 1 << bit;
                    bool src_bit = (result & mask) != 0;

                    if (src_bit)
                        fill_carry = !fill_carry;

                    if (fill_or)
                    {
                        if (fill_carry || src_bit)
                            filled |= mask;
                    }
                    else /* fill_xor */
                    {
                        if (fill_carry)
                            filled |= mask;
                    }
                }
                result = filled;
            }

            /* --- Write result to destination --- */

            if (use_d)
            {
                if (dpt < RAM_SIZE - 1)
                {
                    g_ram[dpt]     = (result >> 8) & 0xff;
                    g_ram[dpt + 1] = result & 0xff;
                }
                dpt += ptr_step;
            }
        }

        /* End of row: apply modulo to advance pointers */
        if (use_a) apt += (desc ? -amod : amod);
        if (use_b) bpt += (desc ? -bmod : bmod);
        if (use_c) cpt += (desc ? -cmod : cmod);
        if (use_d) dpt += (desc ? -dmod : dmod);
    }

    /* Update pointer registers with final values (real hardware does this) */
    g_blitter.apt = apt;
    g_blitter.bpt = bpt;
    g_blitter.cpt = cpt;
    g_blitter.dpt = dpt;

    DPRINTF (LOG_DEBUG, "lxa: BLITTER: done (%d words x %d rows = %d words total)\n",
             width_words, height, width_words * height);
}

static void _handle_custom_write (uint16_t reg, uint16_t value)
{

    switch (reg)
    {
        case CUSTOM_REG_INTENA:
        {
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: INTENA value=0x%04x\n", value);

            if (value & 0x8000)
            {
                /* Set bits */
                g_intena |= value & 0x7fff;

                /*
                 * When INTENA is re-enabled (especially MASTER bit), check if
                 * a VBlank interrupt is pending and arm it now.  This emulates
                 * real Amiga hardware behavior: when INTENA re-enables and an
                 * interrupt is already pending on Paula, it propagates to the
                 * CPU immediately.
                 *
                 * This fixes a timing issue where lxa_run_cycles sets
                 * g_pending_irq while INTENA MASTER is temporarily off
                 * (e.g. during Disable()/Enable() around Wait()), and the
                 * pending VBlank never gets armed because subsequent checks
                 * also find INTENA disabled at the wrong moment.
                 */
                if ((g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK) &&
                    (g_pending_irq & (1 << 3)))
                {
                    g_pending_irq &= ~(1 << 3);
                    m68k_set_irq(0);    /* Musashi edge-trigger: lower first */
                    m68k_set_irq(3);
                }
            }
            else
            {
                /* Clear bits */
                g_intena &= ~(value & 0x7fff);
            }

            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> INTENA=0x%04x\n", g_intena);

            break;
        }
        case CUSTOM_REG_DMACON:
        {
            /* DMA control - update shadow register like INTENA (set/clear semantics) */
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: DMACON value=0x%04x\n", value);
            if (value & 0x8000)
            {
                /* Set bits */
                g_dmacon |= value & 0x7fff;
            }
            else
            {
                /* Clear bits */
                g_dmacon &= ~(value & 0x7fff);
            }
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> DMACON=0x%04x\n", g_dmacon);
            break;
        }
        /* Phase 31: Additional custom chip registers (Denise, Agnus, Paula) - log and ignore */
        case CUSTOM_REG_INTREQ:
        {
            /* Interrupt request - update shadow register with set/clear semantics */
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: INTREQ value=0x%04x\n", value);
            if (value & 0x8000)
            {
                g_intreq |= value & 0x7fff;
            }
            else
            {
                g_intreq &= ~(value & 0x7fff);
            }
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> INTREQ=0x%04x\n", g_intreq);
            break;
        }
        case CUSTOM_REG_COPJMP1:
        case CUSTOM_REG_COPJMP2:
        case CUSTOM_REG_ADKCON:
        case CUSTOM_REG_POTGO:
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: %s value=0x%04x (ignored)\n",
                    _custom_reg_name(reg), value);
            break;

        /* --- Hardware blitter register writes --- */
        case CUSTOM_REG_BLTCON0:
            g_blitter.con0 = value;
            break;
        case CUSTOM_REG_BLTCON1:
            g_blitter.con1 = value;
            break;
        case CUSTOM_REG_BLTAFWM:
            g_blitter.afwm = value;
            break;
        case CUSTOM_REG_BLTALWM:
            g_blitter.alwm = value;
            break;
        case CUSTOM_REG_BLTCPTH:
            g_blitter.cpt = (g_blitter.cpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTCPTL:
            g_blitter.cpt = (g_blitter.cpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTBPTH:
            g_blitter.bpt = (g_blitter.bpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTBPTL:
            g_blitter.bpt = (g_blitter.bpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTAPTH:
            g_blitter.apt = (g_blitter.apt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTAPTL:
            g_blitter.apt = (g_blitter.apt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTDPTH:
            g_blitter.dpt = (g_blitter.dpt & 0x0000ffff) | ((uint32_t)value << 16);
            break;
        case CUSTOM_REG_BLTDPTL:
            g_blitter.dpt = (g_blitter.dpt & 0xffff0000) | value;
            break;
        case CUSTOM_REG_BLTSIZE:
        {
            /* OCS: Writing BLTSIZE triggers the blit.
             * Bits 15:6 = height (0=1024), bits 5:0 = width in words (0=64) */
            uint16_t h = (value >> 6) & 0x3ff;
            uint16_t w = value & 0x3f;
            _blitter_execute (w, h);
            break;
        }
        case CUSTOM_REG_BLTSIZV:
            /* ECS: Store vertical size, blit triggered by BLTSIZH */
            g_blitter.sizv = value & 0x7fff;
            break;
        case CUSTOM_REG_BLTSIZH:
        {
            /* ECS: Writing BLTSIZH triggers the blit */
            uint16_t w = value & 0x7ff;
            uint16_t h = g_blitter.sizv;
            _blitter_execute (w, h);
            break;
        }
        case CUSTOM_REG_BLTCMOD:
            g_blitter.cmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTBMOD:
            g_blitter.bmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTAMOD:
            g_blitter.amod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTDMOD:
            g_blitter.dmod = (int16_t)value;
            break;
        case CUSTOM_REG_BLTCDAT:
            g_blitter.cdat = value;
            break;
        case CUSTOM_REG_BLTBDAT:
            g_blitter.bdat = value;
            break;
        case CUSTOM_REG_BLTADAT:
            g_blitter.adat = value;
            break;
        default:
            /* Many apps write to custom chip registers - just log and ignore */
            if (reg >= CUSTOM_REG_COLOR00 && reg < CUSTOM_REG_COLOR00 + 64) {
                /* Color register write - ignore for now */
                DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: COLOR%02x value=0x%04x (ignored)\n",
                        (reg - CUSTOM_REG_COLOR00) / 2, value);
            } else {
                DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: %s (0x%03x) value=0x%04x (ignored)\n",
                        _custom_reg_name(reg), reg, value);
            }
    }
}

static inline void mwrite8 (uint32_t address, uint8_t value)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        g_ram[addr] = value;
    }
    else if ((address >= 0x00A00000) && (address < CUSTOM_START))
    {
        /* RAM overflow area (10MB - just before custom chips at 0xDFF000) - some apps allocate
         * to end of RAM and overflow into what would be expansion RAM, slow RAM, or CIA areas.
         * Silently ignore these writes. */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 RAM overflow area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= 0x01000000) && (address <= 0x0FFFFFFF))
    {
        /* Zorro-III expansion area writes - ignore (no expansion present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Zorro-III area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= SLOWRAM_START) && (address <= SLOWRAM_END))
    {
        /* Slow RAM area writes - ignore (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Slow RAM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= RANGER_START) && (address <= RANGER_END))
    {
        /* Ranger/expansion RAM area writes - ignore (no expansion memory present) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Ranger RAM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= ZORRO2_AUTOCONFIG_START) && (address <= ZORRO2_AUTOCONFIG_END))
    {
        /* Zorro-II autoconfig area writes - ignore (no expansion boards) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Zorro-II autoconfig area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= EXTROM_START) && (address <= EXTROM_END))
    {
        /* Extended ROM area writes - ignore (some apps probe this area) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 Extended ROM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        /* ROM area writes - ignore (ROM is read-only, some apps try to write for various reasons) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 ROM area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= CIA_START) && (address <= CIA_END))
    {
        /* CIA chip area - ignore writes, we don't emulate CIA hardware */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 CIA area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        /* Custom chip area - handle via custom write handler (byte writes are rare but possible) */
        DPRINTF (LOG_DEBUG, "lxa: mwrite8 custom area 0x%08x <- 0x%02x (ignored)\n", address, value);
    }
    else
    {
        printf("ERROR: mwrite8 at invalid address 0x%08x\n", address);
        _debug(m68k_get_reg(NULL, M68K_REG_PC));
        assert (false);
    }
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    // if (g_trace)
    //     DPRINTF("WRITE8  at 0x%08x -> 0x%02x\n", address, value);
    mwrite8 (address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        _handle_custom_write (address & 0xfff, value);
        return;
    }


    // if (g_trace)
    //     DPRINTF("WRITE16 at 0x%08x -> 0x%04x\n", address, value);
    mwrite8 (address  , (value >> 8) & 0xff);
    mwrite8 (address+1, value & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    // if (g_trace)
    //     DPRINTF("WRITE32 at 0x%08x -> 0x%08x\n", address, value);

    /*
     * Custom chip area: 32-bit writes must be handled as two 16-bit writes
     * (high word first, low word second) because the custom chip register
     * handler processes 16-bit register writes.  This is critical for
     * blitter pointer registers (BLTAPT, BLTBPT, BLTCPT, BLTDPT) which
     * are written as 32-bit values by apps but stored as high/low halves
     * in adjacent 16-bit registers.
     */
    if ((address >= CUSTOM_START) && (address <= CUSTOM_END))
    {
        _handle_custom_write ((address    ) & 0xfff, (value >> 16) & 0xffff);
        _handle_custom_write ((address + 2) & 0xfff, value & 0xffff);
        return;
    }

    mwrite8 (address  , (value >>24) & 0xff);
    mwrite8 (address+1, (value >>16) & 0xff);
    mwrite8 (address+2, (value >> 8) & 0xff);
    mwrite8 (address+3, value & 0xff);
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
    return m68k_read_memory_16 (address);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
    return m68k_read_memory_32 (address);
}

void make_hex(char* buff, unsigned int pc, unsigned int length)
{
    char* ptr = buff;

    for(;length>0;length -= 2)
    {
        sprintf(ptr, "%04x", m68k_read_disassembler_16(pc));
        pc += 2;
        ptr += 4;
        if(length > 2)
            *ptr++ = ' ';
    }
}

static void print68kstate(int lvl)
{
    uint32_t pc, sr, d0,d1,d2,d3,d4,d5,d6,d7;

    pc = m68k_get_reg (NULL, M68K_REG_PC);
    sr = m68k_get_reg (NULL, M68K_REG_SR);
    d0 = m68k_get_reg (NULL, M68K_REG_D0);
    d1 = m68k_get_reg (NULL, M68K_REG_D1);
    d2 = m68k_get_reg (NULL, M68K_REG_D2);
    d3 = m68k_get_reg (NULL, M68K_REG_D3);
    d4 = m68k_get_reg (NULL, M68K_REG_D4);
    d5 = m68k_get_reg (NULL, M68K_REG_D5);
    d6 = m68k_get_reg (NULL, M68K_REG_D6);
    d7 = m68k_get_reg (NULL, M68K_REG_D7);

    LPRINTF (lvl, "     pc=0x%08x sr=0x%04x\n     d0=0x%08x d1=0x%08x d2=0x%08x d3=0x%08x d4=0x%08x d5=0x%08x d6=0x%08x d7=0x%08x\n",
                  pc, sr, d0, d1, d2, d3, d4, d5, d6, d7);
    uint32_t a0,a1,a2,a3,a4,a5,a6,a7,usp,isp,msp;
    a0 = m68k_get_reg (NULL, M68K_REG_A0);
    a1 = m68k_get_reg (NULL, M68K_REG_A1);
    a2 = m68k_get_reg (NULL, M68K_REG_A2);
    a3 = m68k_get_reg (NULL, M68K_REG_A3);
    a4 = m68k_get_reg (NULL, M68K_REG_A4);
    a5 = m68k_get_reg (NULL, M68K_REG_A5);
    a6 = m68k_get_reg (NULL, M68K_REG_A6);
    a7 = m68k_get_reg (NULL, M68K_REG_A7);
    usp = m68k_get_reg (NULL, M68K_REG_USP);
    isp = m68k_get_reg (NULL, M68K_REG_ISP);
    msp = m68k_get_reg (NULL, M68K_REG_MSP);
    LPRINTF (lvl, "     a0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x a6=0x%08x a7=0x%08x\n     usp=0x%08x isp=0x%08x msp=0x%08x\n",
                  a0, a1, a2, a3, a4, a5, a6, a7, usp, isp, msp);
}

/*
 * _update_debug_active() - recalculate the g_debug_active fast-path flag.
 * Must be called whenever g_trace, g_stepping, g_next_pc, or
 * g_num_breakpoints changes.
 */
static void _update_debug_active(void)
{
    g_debug_active = g_trace || g_stepping || g_next_pc || g_num_breakpoints > 0;
}

/*
 * cpu_instr_callback() - called by Musashi before every M68K instruction.
 *
 * Performance-critical: when no debugging is active (g_debug_active==FALSE),
 * this reduces to a single branch + PC safety check.  The trace buffer,
 * breakpoint scanning, and full tracing are only performed when debugging
 * is explicitly enabled.
 */
void cpu_instr_callback(int pc)
{
    /* Always record PC in trace buffer for post-mortem debugging */
    g_trace_buf[g_trace_buf_idx] = pc;
    g_trace_buf_idx = (g_trace_buf_idx+1) % TRACE_BUF_ENTRIES;

    /* Fast path: no debugging active — just check for PC=0 crash */
    if (__builtin_expect(!g_debug_active, 1))
    {
        if (__builtin_expect(pc < 0x100, 0))
        {
            CPRINTF("*** WARNING: PC=0x%08x - invalid address!\n", pc);
            _debug(pc);
        }
        return;
    }

    /* Slow path: full debugging active */

    g_trace_buf[g_trace_buf_idx] = pc;
    g_trace_buf_idx = (g_trace_buf_idx+1) % TRACE_BUF_ENTRIES;

    /* Catch PC=0 bug */
    if (pc < 0x100) {
        CPRINTF("*** WARNING: PC=0x%08x - invalid address!\n", pc);
        _debug(pc);
    }

    if (g_trace)
    {
        static char buff[100];
        static char buff2[100];
        static unsigned int instr_size;
        print68kstate(LOG_DEBUG);
        instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68030);
        make_hex(buff2, pc, instr_size);
        DPRINTF(LOG_DEBUG, "E %08x: %-20s: %s\n", pc, buff2, buff);
    }

    if (g_stepping)
    {
        _debug(pc);
    }
    else
    {
        if (g_next_pc == pc)
        {
            g_next_pc = 0;
            _update_debug_active();
            _debug(pc);
        }
        else
        {
            for (int i = 0; i<g_num_breakpoints; i++)
            {
                if (g_breakpoints[i] == pc)
                    _debug(pc);
            }
        }
    }
}

/* Functions used only by main() - excluded from library build */
#ifndef LXA_LIBRARY_BUILD
/* Get the directory containing the running binary */
static bool get_binary_dir(char *buf, size_t buf_size)
{
    ssize_t len = readlink("/proc/self/exe", buf, buf_size - 1);
    if (len == -1)
        return false;
    
    buf[len] = '\0';
    
    /* Remove the binary name to get the directory */
    char *last_slash = strrchr(buf, '/');
    if (last_slash)
        *last_slash = '\0';
    
    return true;
}

/* Auto-detect ROM path - tries multiple locations like Wine does */
static const char *auto_detect_rom_path(void)
{
    static char path[PATH_MAX];
    static char binary_dir[PATH_MAX];
    struct stat st;
    const char *home = getenv("HOME");
    const char *lxa_prefix = getenv("LXA_PREFIX");
    bool have_binary_dir = get_binary_dir(binary_dir, sizeof(binary_dir));
    
    /* Try paths relative to the binary directory first */
    if (have_binary_dir)
    {
        static char resolved_path[PATH_MAX];
        const char *rel_to_binary[] = {
            "../share/lxa/lxa.rom",              /* FHS style: bin/lxa -> ../share/lxa/lxa.rom */
            "../rom/lxa.rom",                    /* Legacy: bin/lxa -> ../rom/lxa.rom */
            "../../target/rom/lxa.rom",          /* CMake build: host/bin/lxa -> ../../target/rom/lxa.rom */
        };
        
        for (size_t i = 0; i < sizeof(rel_to_binary)/sizeof(rel_to_binary[0]); i++) {
            snprintf(path, sizeof(path), "%s/%s", binary_dir, rel_to_binary[i]);
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                realpath(path, resolved_path);
                return resolved_path;
            }
        }
    }
    
    /* Try locations relative to current working directory */
    const char *try_paths[] = {
        "lxa.rom",                           /* Current directory */
        "src/rom/lxa.rom",                   /* Project root (dev mode) */
    };
    
    for (size_t i = 0; i < sizeof(try_paths)/sizeof(try_paths[0]); i++) {
        if (stat(try_paths[i], &st) == 0 && S_ISREG(st.st_mode)) {
            realpath(try_paths[i], path);
            return path;
        }
    }
    
    /* Try LXA_PREFIX/lxa.rom (if LXA_PREFIX is set) */
    if (lxa_prefix) {
        snprintf(path, sizeof(path), "%s/lxa.rom", lxa_prefix);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            return path;
        }
    }
    
    /* Try ~/.lxa/lxa.rom */
    if (home) {
        snprintf(path, sizeof(path), "%s/.lxa/lxa.rom", home);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            return path;
        }
    }
    
    /* Try system-wide locations */
    const char *system_paths[] = {
        "/usr/local/share/lxa/lxa.rom",
        "/usr/share/lxa/lxa.rom",
    };
    
    for (size_t i = 0; i < sizeof(system_paths)/sizeof(system_paths[0]); i++) {
        if (stat(system_paths[i], &st) == 0 && S_ISREG(st.st_mode)) {
            return system_paths[i];
        }
    }
    
    return NULL;
}
#endif /* !LXA_LIBRARY_BUILD (get_binary_dir, auto_detect_rom_path) */

static char *_mgetstr (uint32_t address)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return (char *) &g_ram[addr];
    }
    else if ((address >= ROM_START) && (address <= ROM_END))
    {
        uint32_t addr = address - ROM_START;
        return (char *) &g_rom[addr];
    }
    else
    {
        printf("ERROR: _mgetstr at invalid address 0x%08x\n", address);
        assert (false);
    }
    return NULL;
}

static void _dos_path2linux (const char *amiga_path, char *linux_path, int buf_len)
{
    // First try VFS resolution
    if (vfs_resolve_path (amiga_path, linux_path, buf_len))
    {
        DPRINTF (LOG_DEBUG, "lxa: _dos_path2linux: VFS resolved %s -> %s\n", amiga_path, linux_path);
        return;
    }
    
    // Fallback to legacy behavior for backward compatibility
    if (!strncasecmp (amiga_path, "NIL:", 4))
        snprintf (linux_path, buf_len, "/dev/null");
    else if (g_sysroot && !vfs_resolve_case_path (g_sysroot, amiga_path, linux_path, buf_len))
        snprintf (linux_path, buf_len, "%s/%s", g_sysroot, amiga_path);
    else if (!g_sysroot)
        snprintf (linux_path, buf_len, "%s", amiga_path);
    
    DPRINTF (LOG_DEBUG, "lxa: _dos_path2linux: legacy resolved %s -> %s\n", amiga_path, linux_path);
}

static int errno2Amiga (void)
{
    switch (errno)
    {
        case 0:         return 0;
        case ENOENT:    return ERROR_OBJECT_NOT_FOUND;
        case ENOTDIR:   return ERROR_DIR_NOT_FOUND;
        case EEXIST:    return ERROR_OBJECT_EXISTS;
        case EACCES:    return ERROR_READ_PROTECTED;
        case EPERM:     return ERROR_READ_PROTECTED;
        case ENOMEM:    return ERROR_NO_FREE_STORE;
        case ENOSPC:    return ERROR_DISK_FULL;
        case EISDIR:    return ERROR_OBJECT_WRONG_TYPE;
        case ENOTEMPTY: return ERROR_DIRECTORY_NOT_EMPTY;
        case EXDEV:     return ERROR_RENAME_ACROSS_DEVICES;
        case ENAMETOOLONG: return ERROR_BAD_STREAM_NAME;
        case EFBIG:     return ERROR_OBJECT_TOO_LARGE;
        case EROFS:     return ERROR_DISK_WRITE_PROTECTED;
        case EMFILE:    /* fall through */
        case ENFILE:    return ERROR_NO_FREE_STORE;
        case EBUSY:     return ERROR_OBJECT_IN_USE;
        case ESPIPE:    return ERROR_SEEK_ERROR;
        case ELOOP:     return ERROR_TOO_MANY_LEVELS;
        default:
            DPRINTF (LOG_WARNING, "lxa: errno2Amiga: unmapped errno %d (%s)\n",
                     errno, strerror(errno));
            return ERROR_ACTION_NOT_KNOWN;
    }
}

static void _dos_stdinout_fh (uint32_t fh68k, int is_input)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_stdinout_fh(): fh68k=0x%08x, is_input=%d\n", fh68k, is_input);
    m68k_write_memory_32 (fh68k+32, FILE_KIND_CONSOLE); // fh_Func3
    m68k_write_memory_32 (fh68k+36, is_input ? STDIN_FILENO : STDOUT_FILENO);     // fh_Args
}

static int _dos_open (uint32_t path68k, uint32_t accessMode, uint32_t fh68k)
{
    char *amiga_path = _mgetstr(path68k);
    char lxpath[PATH_MAX];

    DPRINTF (LOG_DEBUG, "lxa: _dos_open(): amiga_path=%s\n", amiga_path);

    if (!strncasecmp (amiga_path, "CONSOLE:", 8) || (amiga_path[0]=='*'))
    {
        _dos_stdinout_fh (fh68k, 0);
    }
    else if (!strncasecmp (amiga_path, "CON:", 4) || !strncasecmp (amiga_path, "RAW:", 4))
    {
        /* CON:/RAW: window - mark as console, m68k side will handle the rest */
        /* We set FILE_KIND_CON to indicate this needs special handling */
        m68k_write_memory_32 (fh68k+32, FILE_KIND_CON);     // fh_Func3 = FILE_KIND_CON
        m68k_write_memory_32 (fh68k+36, 0);                 // fh_Args = 0 (will be set later)
        /* Store the raw mode flag: RAW: = 1, CON: = 0 */
        m68k_write_memory_8 (fh68k+44, !strncasecmp(amiga_path, "RAW:", 4) ? 1 : 0);  // fh_Buf (unused field)
        DPRINTF (LOG_DEBUG, "lxa: _dos_open(): CON:/RAW: window, path=%s\n", amiga_path);
    }
    else
    {
        // FIXME: amiga paths are case insensitive!
        _dos_path2linux (amiga_path, lxpath, PATH_MAX);
        DPRINTF (LOG_DEBUG, "lxa: _dos_open(): lxpath=%s\n", lxpath);

        int flags = 0;
        mode_t mode = 0644;

        switch (accessMode)
        {
            case MODE_NEWFILE:
                flags = O_CREAT | O_TRUNC | O_RDWR;
                break;
            case MODE_OLDFILE:
                flags = O_RDONLY;  // Read-only for existing files
                break;
            case MODE_READWRITE:
                flags = O_CREAT | O_RDWR;
                break;
            default:
                assert(FALSE);
        }

        int fd = open (lxpath, flags, mode);
        int err = errno;

        LPRINTF (LOG_DEBUG, "lxa: _dos_open(): open() result: fd=%d, err=%d\n", fd, err);

        if (fd >= 0)
        {
            m68k_write_memory_32 (fh68k+36, fd);                // fh_Args
            m68k_write_memory_32 (fh68k+32, FILE_KIND_REGULAR); // fh_Func3
            return 0;  // Success
        }
        else
        {
            int amiga_err = errno2Amiga();
            m68k_write_memory_32 (fh68k+40, amiga_err);  // fh_Arg2 = error code
            return amiga_err;
        }
    }

    return 0;
}

static int _dos_read (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_read(): fh=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd = m68k_read_memory_32 (fh68k+36);
    int kind = m68k_read_memory_32 (fh68k+32);
    DPRINTF (LOG_DEBUG, "                  -> fd = %d\n", fd);

    void *buf = _mgetstr (buf68k);
    if (kind == FILE_KIND_CONSOLE && !lxa_host_console_input_empty())
    {
        uint8_t *dst = (uint8_t *)buf;
        uint32_t count = 0;

        while (count < len68k)
        {
            int ch = lxa_host_console_input_pop();

            if (ch < 0)
                break;

            dst[count++] = (uint8_t)ch;

            if (ch == '\r' || ch == '\n')
                break;
        }

        return (int)count;
    }

    ssize_t l = read (fd, buf, len68k);

    if (l<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return l;
}

static int _dos_seek (uint32_t fh68k, int32_t position, int32_t mode)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_seek(): fh=0x%08x, position=0x%08x, mode=%d\n", fh68k, position, mode);

    int fd   = m68k_read_memory_32 (fh68k+36);

    int whence = 0;
    switch (mode)
    {
        case OFFSET_BEGINNING: whence = SEEK_SET; break;
        case OFFSET_CURRENT  : whence = SEEK_CUR; break;
        case OFFSET_END      : whence = SEEK_END; break;
        default:
            assert(FALSE);
    }

    off_t o = lseek (fd, position, whence);

    if (o<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return o;
}

static int _dos_setfilesize(uint32_t fh68k, int32_t offset, int32_t mode)
{
    int fd = m68k_read_memory_32(fh68k + 36);
    off_t base;
    off_t new_size;

    DPRINTF(LOG_DEBUG, "lxa: _dos_setfilesize(): fh=0x%08x, offset=%d, mode=%d\n",
            fh68k, offset, mode);

    switch (mode)
    {
        case OFFSET_BEGINNING:
            base = 0;
            break;

        case OFFSET_CURRENT:
            base = lseek(fd, 0, SEEK_CUR);
            break;

        case OFFSET_END:
            base = lseek(fd, 0, SEEK_END);
            break;

        default:
            errno = EINVAL;
            m68k_write_memory_32(fh68k + 40, errno2Amiga());
            return -1;
    }

    if (base < 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    new_size = base + offset;
    if (new_size < 0)
    {
        errno = EINVAL;
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    if (ftruncate(fd, new_size) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return -1;
    }

    return (int)new_size;
}

static int trackdisk_errno_to_td_error(void)
{
    switch (errno)
    {
        case 0:
            return 0;
        case ENOENT:
        case ENODEV:
            return TDERR_DISKCHANGED_HOST;
        case EROFS:
        case EACCES:
        case EPERM:
            return TDERR_WRITEPROT_HOST;
        case ENOMEM:
            return TDERR_NOMEM_HOST;
        case EINVAL:
            return TDERR_BADSECPREAMBLE_HOST;
        case EIO:
            return TDERR_NOTSPECIFIED_HOST;
        default:
            return TDERR_NOTSPECIFIED_HOST;
    }
}

static int trackdisk_unit_path(uint32_t unit, char *path, size_t path_size)
{
    char drive_name[8];
    const char *drive_path;
    struct stat st;
    DIR *dir;
    struct dirent *de;

    snprintf(drive_name, sizeof(drive_name), "DF%u", unit);
    drive_path = vfs_get_drive_path(drive_name);
    if (!drive_path)
        return TDERR_DISKCHANGED_HOST;

    if (stat(drive_path, &st) != 0)
        return trackdisk_errno_to_td_error();

    if (S_ISREG(st.st_mode))
    {
        strncpy(path, drive_path, path_size - 1);
        path[path_size - 1] = '\0';
        return 0;
    }

    if (!S_ISDIR(st.st_mode))
        return TDERR_DISKCHANGED_HOST;

    dir = opendir(drive_path);
    if (!dir)
        return trackdisk_errno_to_td_error();

    while ((de = readdir(dir)) != NULL)
    {
        size_t len = strlen(de->d_name);
        if (len >= 4 && strcasecmp(de->d_name + len - 4, ".adf") == 0)
        {
            snprintf(path, path_size, "%s/%s", drive_path, de->d_name);
            closedir(dir);
            return 0;
        }
    }

    closedir(dir);
    return TDERR_DISKCHANGED_HOST;
}

static int trackdisk_check_bounds(off_t file_size, uint32_t offset, uint32_t length)
{
    uint64_t end = (uint64_t)offset + (uint64_t)length;

    if (end > (uint64_t)file_size)
        return TDERR_TOOFEWSECS_HOST;

    return 0;
}

static int _trackdisk_read(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    uint32_t i;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    error = trackdisk_check_bounds(st.st_size, offset, length);
    if (error != 0)
    {
        close(fd);
        return error;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    for (i = 0; i < length; i++)
    {
        unsigned char byte;
        ssize_t n = read(fd, &byte, 1);
        if (n != 1)
        {
            close(fd);
            return (n < 0) ? trackdisk_errno_to_td_error() : TDERR_TOOFEWSECS_HOST;
        }
        m68k_write_memory_8(data_ptr + i, byte);
    }

    close(fd);
    return 0;
}

static int _trackdisk_write(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    uint32_t i;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDWR);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    error = trackdisk_check_bounds(st.st_size, offset, length);
    if (error != 0)
    {
        close(fd);
        return error;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    for (i = 0; i < length; i++)
    {
        unsigned char byte = (unsigned char)m68k_read_memory_8(data_ptr + i);
        if (write(fd, &byte, 1) != 1)
        {
            error = trackdisk_errno_to_td_error();
            close(fd);
            return error;
        }
    }

    close(fd);
    return 0;
}

static int _trackdisk_format(uint32_t unit, uint32_t data_ptr, uint32_t length, uint32_t offset, uint32_t is_ext)
{
    return _trackdisk_write(unit, data_ptr, length, offset, is_ext);
}

static int _trackdisk_seek(uint32_t unit, uint32_t offset, uint32_t is_ext)
{
    char path[PATH_MAX];
    int fd;
    struct stat st;
    int error;
    (void)is_ext;

    error = trackdisk_unit_path(unit, path, sizeof(path));
    if (error != 0)
        return error;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return trackdisk_errno_to_td_error();

    if (fstat(fd, &st) != 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    if ((uint64_t)offset > (uint64_t)st.st_size)
    {
        close(fd);
        return TDERR_SEEKERROR_HOST;
    }

    if (lseek(fd, (off_t)offset, SEEK_SET) < 0)
    {
        error = trackdisk_errno_to_td_error();
        close(fd);
        return error;
    }

    close(fd);
    return 0;
}

/*
 * Phase 16: Console Device Enhancement
 *
 * This implements Amiga console CSI (Control Sequence Introducer) escape sequence
 * translation to ANSI terminal sequences. The Amiga uses 0x9B as CSI, while
 * standard ANSI terminals use ESC[ (0x1B 0x5B).
 *
 * Supported sequences:
 * - Cursor positioning: H (absolute), A/B/C/D (relative), E/F (line start)
 * - Screen/line control: J (erase display), K (erase line), L/M (insert/delete line)
 * - Character control: @ (insert), P (delete)
 * - Scrolling: S (scroll up), T (scroll down)
 * - Text attributes: m (SGR - colors, bold, underline, etc.)
 * - Cursor visibility: p
 * - Window queries: q (reports window size), n (cursor position)
 */

#define CSI_BUF_LEN 64
#define CSI_MAX_PARAMS 8
#define CSI "\e["

/* Console state for window size queries (used in DPRINTF when debug enabled) */
static int g_console_rows __attribute__((unused)) = 24;
static int g_console_cols __attribute__((unused)) = 80;

/* Parse CSI parameters from buffer, returns number of parameters found */
static int _csi_parse_params(const char *buf, int len, int *params, int max_params)
{
    int num_params = 0;
    int current = 0;
    bool has_digits = false;

    for (int i = 0; i < len && num_params < max_params; i++) {
        char c = buf[i];
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            has_digits = true;
        } else if (c == ';') {
            params[num_params++] = has_digits ? current : 1;
            current = 0;
            has_digits = false;
        } else if (c == ' ' || c == '?' || c == '>') {
            /* Skip intermediate characters */
            continue;
        }
    }
    /* Don't forget the last parameter */
    if (has_digits || num_params > 0) {
        params[num_params++] = has_digits ? current : 1;
    }
    return num_params;
}

/* Check if buffer contains a specific prefix (for sequences like "0 " in "0 p") */
static bool _csi_has_prefix(const char *buf, int len, const char *prefix)
{
    int plen = strlen(prefix);
    if (len < plen)
        return false;
    return strncmp(buf, prefix, plen) == 0;
}

/* Process a complete Amiga CSI sequence and emit ANSI equivalent */
static void _csi_process(const char *buf, int len, char final_byte)
{
    int params[CSI_MAX_PARAMS] = {0};
    int num_params = _csi_parse_params(buf, len, params, CSI_MAX_PARAMS);

    /* Default parameter is 1 for most movement commands */
    int p1 = (num_params >= 1 && params[0] > 0) ? params[0] : 1;
    int p2 = (num_params >= 2 && params[1] > 0) ? params[1] : 1;

    switch (final_byte) {
        /*
         * Cursor Movement
         */
        case 'H':  /* CUP - Cursor Position: CSI row;col H */
        case 'f':  /* HVP - Horizontal/Vertical Position (same as H) */
            if (num_params == 0) {
                /* Home position */
                fputs(CSI "H", stdout);
            } else {
                fprintf(stdout, CSI "%d;%dH", p1, p2);
            }
            break;

        case 'A':  /* CUU - Cursor Up */
            fprintf(stdout, CSI "%dA", p1);
            break;

        case 'B':  /* CUD - Cursor Down */
            fprintf(stdout, CSI "%dB", p1);
            break;

        case 'C':  /* CUF - Cursor Forward (Right) */
            fprintf(stdout, CSI "%dC", p1);
            break;

        case 'D':  /* CUB - Cursor Backward (Left) */
            fprintf(stdout, CSI "%dD", p1);
            break;

        case 'E':  /* CNL - Cursor Next Line */
            fprintf(stdout, CSI "%dE", p1);
            break;

        case 'F':  /* CPL - Cursor Preceding Line */
            fprintf(stdout, CSI "%dF", p1);
            break;

        case 'G':  /* CHA - Cursor Horizontal Absolute (column) */
            fprintf(stdout, CSI "%dG", p1);
            break;

        case 'I':  /* CHT - Cursor Horizontal Tab */
            for (int i = 0; i < p1; i++)
                fputc('\t', stdout);
            break;

        case 'Z':  /* CBT - Cursor Backward Tab */
            /* ANSI supports this directly */
            fprintf(stdout, CSI "%dZ", p1);
            break;

        /*
         * Erase Functions
         */
        case 'J':  /* ED - Erase in Display */
            /* 0 = cursor to end, 1 = start to cursor, 2 = entire display */
            if (num_params == 0 || params[0] == 0) {
                fputs(CSI "J", stdout);  /* Erase to end of display */
            } else {
                fprintf(stdout, CSI "%dJ", params[0]);
            }
            break;

        case 'K':  /* EL - Erase in Line */
            /* 0 = cursor to end, 1 = start to cursor, 2 = entire line */
            if (num_params == 0 || params[0] == 0) {
                fputs(CSI "K", stdout);  /* Erase to end of line */
            } else {
                fprintf(stdout, CSI "%dK", params[0]);
            }
            break;

        /*
         * Insert/Delete
         */
        case 'L':  /* IL - Insert Line(s) */
            fprintf(stdout, CSI "%dL", p1);
            break;

        case 'M':  /* DL - Delete Line(s) */
            fprintf(stdout, CSI "%dM", p1);
            break;

        case '@':  /* ICH - Insert Character(s) */
            fprintf(stdout, CSI "%d@", p1);
            break;

        case 'P':  /* DCH - Delete Character(s) */
            fprintf(stdout, CSI "%dP", p1);
            break;

        /*
         * Scrolling
         */
        case 'S':  /* SU - Scroll Up */
            fprintf(stdout, CSI "%dS", p1);
            break;

        case 'T':  /* SD - Scroll Down */
            fprintf(stdout, CSI "%dT", p1);
            break;

        /*
         * Text Attributes (SGR - Select Graphic Rendition)
         */
        case 'm':
            if (num_params == 0) {
                /* Reset all attributes */
                fputs(CSI "0m", stdout);
            } else {
                /* Build ANSI SGR sequence from Amiga parameters */
                fputs(CSI, stdout);
                for (int i = 0; i < num_params; i++) {
                    int attr = params[i];
                    if (i > 0) fputc(';', stdout);

                    switch (attr) {
                        /* Reset */
                        case 0:  fprintf(stdout, "0"); break;

                        /* Styles */
                        case 1:  fprintf(stdout, "1"); break;   /* Bold */
                        case 2:  fprintf(stdout, "2"); break;   /* Faint/Dim */
                        case 3:  fprintf(stdout, "3"); break;   /* Italic */
                        case 4:  fprintf(stdout, "4"); break;   /* Underline */
                        case 7:  fprintf(stdout, "7"); break;   /* Inverse/Reverse */
                        case 8:  fprintf(stdout, "8"); break;   /* Concealed/Hidden */

                        /* Style off (V36+) */
                        case 22: fprintf(stdout, "22"); break;  /* Normal intensity */
                        case 23: fprintf(stdout, "23"); break;  /* Italic off */
                        case 24: fprintf(stdout, "24"); break;  /* Underline off */
                        case 27: fprintf(stdout, "27"); break;  /* Inverse off */
                        case 28: fprintf(stdout, "28"); break;  /* Concealed off */

                        /* Foreground colors (30-37, 39=default) */
                        case 30: case 31: case 32: case 33:
                        case 34: case 35: case 36: case 37:
                        case 39:
                            fprintf(stdout, "%d", attr);
                            break;

                        /* Background colors (40-47, 49=default) */
                        case 40: case 41: case 42: case 43:
                        case 44: case 45: case 46: case 47:
                        case 49:
                            fprintf(stdout, "%d", attr);
                            break;

                        default:
                            /* Pass through unknown attributes */
                            fprintf(stdout, "%d", attr);
                            break;
                    }
                }
                fputs("m", stdout);
            }
            break;

        /*
         * Cursor Visibility
         */
        case 'p':
            if (_csi_has_prefix(buf, len, "0 ")) {
                /* Cursor invisible */
                fputs(CSI "?25l", stdout);
            } else if (_csi_has_prefix(buf, len, " ") || _csi_has_prefix(buf, len, "1 ")) {
                /* Cursor visible */
                fputs(CSI "?25h", stdout);
            }
            break;

        /*
         * Window Status/Queries
         */
        case 'q':
            if (_csi_has_prefix(buf, len, "0 ")) {
                /* Window Status Request - Amiga expects response:
                 * CSI 1;1;<bottom>;<right> r
                 * We can't inject input, so just log for now */
                DPRINTF(LOG_DEBUG, "lxa: Console window status request (rows=%d, cols=%d)\n",
                        g_console_rows, g_console_cols);
            }
            break;

        case 'n':
            if (num_params >= 1 && params[0] == 6) {
                /* Device Status Report (cursor position query)
                 * Amiga expects response: CSI row;col R
                 * We can't inject input, so just log for now */
                DPRINTF(LOG_DEBUG, "lxa: Console cursor position query\n");
            }
            break;

        /*
         * Tab Control
         */
        case 'W':
            if (_csi_has_prefix(buf, len, "0")) {
                /* Set tab at current position - not directly supported in ANSI */
                DPRINTF(LOG_DEBUG, "lxa: Set tab stop (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "2")) {
                /* Clear tab at current position */
                fputs(CSI "0g", stdout);
            } else if (_csi_has_prefix(buf, len, "5")) {
                /* Clear all tabs */
                fputs(CSI "3g", stdout);
            }
            break;

        /*
         * Mode Control
         */
        case 'h':  /* SM - Set Mode */
            if (_csi_has_prefix(buf, len, "20")) {
                /* Set LF mode (LF acts as CR+LF) - not directly translatable */
                DPRINTF(LOG_DEBUG, "lxa: Set LF mode (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "?7")) {
                /* Enable autowrap */
                fputs(CSI "?7h", stdout);
            }
            break;

        case 'l':  /* RM - Reset Mode */
            if (_csi_has_prefix(buf, len, "20")) {
                /* Reset LF mode */
                DPRINTF(LOG_DEBUG, "lxa: Reset LF mode (ignored)\n");
            } else if (_csi_has_prefix(buf, len, "?7")) {
                /* Disable autowrap */
                fputs(CSI "?7l", stdout);
            }
            break;

        /*
         * Raw Events (Amiga-specific, not translatable)
         */
        case '{':  /* Set Raw Events */
            DPRINTF(LOG_DEBUG, "lxa: Set raw events %d (ignored)\n", p1);
            break;

        case '}':  /* Reset Raw Events */
            DPRINTF(LOG_DEBUG, "lxa: Reset raw events %d (ignored)\n", p1);
            break;

        /*
         * Page/Window Setup (Amiga-specific)
         */
        case 't':  /* Set page length */
            DPRINTF(LOG_DEBUG, "lxa: Set page length %d (ignored)\n", p1);
            break;

        case 'u':  /* Set line length */
            DPRINTF(LOG_DEBUG, "lxa: Set line length %d (ignored)\n", p1);
            break;

        case 'x':  /* Set left offset */
            DPRINTF(LOG_DEBUG, "lxa: Set left offset %d (ignored)\n", p1);
            break;

        case 'y':  /* Set top offset */
            DPRINTF(LOG_DEBUG, "lxa: Set top offset %d (ignored)\n", p1);
            break;

        default:
            DPRINTF(LOG_DEBUG, "lxa: Unhandled CSI sequence: %.*s%c\n", len, buf, final_byte);
            break;
    }
}

/* Process special C0/C1 control characters */
static void _console_control_char(uint8_t c)
{
    switch (c) {
        case 0x07:  /* BEL - Bell */
            fputc('\a', stdout);
            break;
        case 0x08:  /* BS - Backspace */
            fputc('\b', stdout);
            break;
        case 0x09:  /* HT - Horizontal Tab */
            fputc('\t', stdout);
            break;
        case 0x0A:  /* LF - Line Feed */
            fputc('\n', stdout);
            break;
        case 0x0B:  /* VT - Vertical Tab (move up one line on Amiga) */
            fputs(CSI "A", stdout);
            break;
        case 0x0C:  /* FF - Form Feed (clear screen on Amiga) */
            fputs(CSI "2J" CSI "H", stdout);
            break;
        case 0x0D:  /* CR - Carriage Return */
            fputc('\r', stdout);
            break;
        case 0x84:  /* IND - Index (move down one line) */
            fputs(CSI "D", stdout);
            break;
        case 0x85:  /* NEL - Next Line (CR+LF) */
            fputs("\r\n", stdout);
            break;
        case 0x8D:  /* RI - Reverse Index (move up one line) */
            fputs(CSI "M", stdout);
            break;
        default:
            /* Pass through printable characters */
            if (c >= 0x20 && c < 0x7F) {
                fputc(c, stdout);
            } else if (c >= 0xA0) {
                /* High ASCII / Latin-1 */
                fputc(c, stdout);
            }
            break;
    }
}

static int _dos_write(uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_write(): fh68k=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd   = m68k_read_memory_32(fh68k + 36);
    int kind = m68k_read_memory_32(fh68k + 32);
    DPRINTF(LOG_DEBUG, "                  -> fd=%d, kind=%d\n", fd, kind);

    char *buf = _mgetstr(buf68k);
    ssize_t l = 0;

    switch (kind) {
        case FILE_KIND_REGULAR:
        {
            l = write(fd, buf, len68k);

            if (l < 0)
                m68k_write_memory_32(fh68k + 40, errno2Amiga()); // fh_Arg2

            break;
        }
        case FILE_KIND_CONSOLE:
        {
            static bool     bCSI = FALSE;
            static bool     bESC = FALSE;
            static char     csiBuf[CSI_BUF_LEN];
            static uint16_t csiBufLen = 0;

            l = len68k;

            if (g_console_output_hook && l > 0)
                g_console_output_hook(buf, (int)l);

            for (int i = 0; i < l; i++) {
                uint8_t uc = (uint8_t)buf[i];

                if (!bCSI && !bESC) {
                    if (uc == 0x9B) {
                        /* Amiga CSI (single byte) */
                        bCSI = TRUE;
                        csiBufLen = 0;
                        continue;
                    } else if (uc == 0x1B) {
                        /* ESC - might be start of ESC[ sequence */
                        bESC = TRUE;
                        continue;
                    } else {
                        /* Regular character or control code */
                        _console_control_char(uc);
                    }
                } else if (bESC) {
                    if (uc == '[') {
                        /* ESC[ is equivalent to 0x9B CSI */
                        bESC = FALSE;
                        bCSI = TRUE;
                        csiBufLen = 0;
                    } else if (uc == 'c') {
                        /* ESC c - Reset to Initial State */
                        fputs(CSI "!p" CSI "?3;4l" CSI "4l" CSI "0m", stdout);
                        bESC = FALSE;
                    } else {
                        /* Unknown ESC sequence, output both */
                        fputc(0x1B, stdout);
                        fputc(uc, stdout);
                        bESC = FALSE;
                    }
                } else if (bCSI) {
                    /*
                     * CSI sequence parsing:
                     * 0x30–0x3F (ASCII 0–9:;<=>?)                  parameter bytes
                     * 0x20–0x2F (ASCII space and !"#$%&'()*+,-./)  intermediate bytes
                     * 0x40–0x7E (ASCII @A–Z[\]^_`a–z{|}~)          final byte
                     */
                    if (uc >= 0x40 && uc <= 0x7E) {
                        /* Final byte - process the sequence */
                        csiBuf[csiBufLen] = '\0';
                        _csi_process(csiBuf, csiBufLen, uc);
                        bCSI = FALSE;
                        csiBufLen = 0;
                    } else if (uc >= 0x20 && uc <= 0x3F) {
                        /* Parameter or intermediate byte */
                        if (csiBufLen < CSI_BUF_LEN - 1)
                            csiBuf[csiBufLen++] = uc;
                    } else {
                        /* Invalid byte in CSI sequence - abort */
                        bCSI = FALSE;
                        csiBufLen = 0;
                        _console_control_char(uc);
                    }
                }
            }
            /* Flush stdout after each write to console to ensure output is visible immediately */
            fflush(stdout);
            break;
        }
        default:
            assert(FALSE);
    }

    return l;
}

static int _dos_flush (uint32_t fh68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_flush(): fh=0x%08x\n", fh68k);

    int fd   = m68k_read_memory_32 (fh68k+36);
    int kind = m68k_read_memory_32 (fh68k+32);

    if (kind == FILE_KIND_REGULAR)
    {
        if (fsync(fd) == 0)
            return 1;
        else
        {
            m68k_write_memory_32 (fh68k+40, errno2Amiga());
            return 0;
        }
    }
    else if (kind == FILE_KIND_CONSOLE)
    {
        /* For console, just flush stdout */
        fflush(stdout);
        return 1;
    }

    return 0;
}

static int _dos_close (uint32_t fh68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x\n", fh68k);

    int fd = m68k_read_memory_32 (fh68k+36);

    _record_lock_release_all(fh68k);

    close(fd);

    DPRINTF (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x done\n", fh68k);
    return 1;
}

/*
 * Phase 3: Lock and Examine API host-side implementation
 */

/* Convert Linux stat mode to Amiga protection bits */
static uint32_t _unix_mode_to_amiga(mode_t mode)
{
    uint32_t prot = 0;
    
    /* Note: Amiga protection bits are inverted - bit SET means permission denied */
    /* We set the bit to 1 if permission is NOT granted, 0 if granted */
    
    /* Owner/basic permissions (bits 0-3) */
    if (!(mode & S_IRUSR)) prot |= FIBF_READ;
    if (!(mode & S_IWUSR)) prot |= FIBF_WRITE;
    if (!(mode & S_IXUSR)) prot |= FIBF_EXECUTE;
    /* Note: delete permission is tied to write permission */
    if (!(mode & S_IWUSR)) prot |= FIBF_DELETE;
    
    /* Group permissions (bits 8-11) */
    if (!(mode & S_IRGRP)) prot |= (1 << 11);  /* FIBB_GRP_READ */
    if (!(mode & S_IWGRP)) prot |= (1 << 10);  /* FIBB_GRP_WRITE */
    if (!(mode & S_IXGRP)) prot |= (1 << 9);   /* FIBB_GRP_EXECUTE */
    if (!(mode & S_IWGRP)) prot |= (1 << 8);   /* FIBB_GRP_DELETE */
    
    /* Other permissions (bits 12-15) */
    if (!(mode & S_IROTH)) prot |= (1 << 15);  /* FIBB_OTR_READ */
    if (!(mode & S_IWOTH)) prot |= (1 << 14);  /* FIBB_OTR_WRITE */
    if (!(mode & S_IXOTH)) prot |= (1 << 13);  /* FIBB_OTR_EXECUTE */
    if (!(mode & S_IWOTH)) prot |= (1 << 12);  /* FIBB_OTR_DELETE */
    
    /* Amiga also has archive and script flags - leave them as 0 (not set) */
    
    return prot;
}

/* Convert Unix time_t to Amiga DateStamp */
static void _unix_timespec_to_datestamp(time_t unix_time, long unix_nsec, uint32_t ds68k)
{
    time_t amiga_time = unix_time - AMIGA_EPOCH_OFFSET;
    if (amiga_time < 0) amiga_time = 0;
    
    /* DateStamp format: days, minutes, ticks (50ths of a second) */
    uint32_t days = amiga_time / (24 * 60 * 60);
    uint32_t remaining = amiga_time % (24 * 60 * 60);
    uint32_t minutes = remaining / 60;
    uint32_t ticks = (remaining % 60) * 50 + (uint32_t)(unix_nsec / 20000000L);

    m68k_write_memory_32(ds68k + 0, days);
    m68k_write_memory_32(ds68k + 4, minutes);
    m68k_write_memory_32(ds68k + 8, ticks);
}

static time_t _datestamp68k_to_unix_time(uint32_t ds68k)
{
    uint32_t days = m68k_read_memory_32(ds68k + 0);
    uint32_t minutes = m68k_read_memory_32(ds68k + 4);
    uint32_t ticks = m68k_read_memory_32(ds68k + 8);
    uint64_t amiga_time = (uint64_t)days * 24ULL * 60ULL * 60ULL +
                          (uint64_t)minutes * 60ULL +
                          (uint64_t)ticks / 50ULL;

    return (time_t)(amiga_time + AMIGA_EPOCH_OFFSET);
}

static bool _resolve_amiga_path_host(const char *amiga_path, char *linux_path, size_t maxlen)
{
    if (vfs_resolve_path(amiga_path, linux_path, maxlen))
        return true;

    _dos_path2linux(amiga_path, linux_path, (int)maxlen);
    return linux_path[0] != '\0';
}

/* Lock a file or directory */
static uint32_t _dos_lock(uint32_t name68k, int32_t mode)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: amiga_path='%s'\n", amiga_path);
    
    /* Resolve the Amiga path to Linux path */
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        /* Try legacy fallback */
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: linux_path='%s'\n", linux_path);
    
    /* Check if the path exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock: stat failed: %s\n", strerror(errno));
        return 0; /* ERROR_OBJECT_NOT_FOUND will be set by caller */
    }
    
    /* Allocate a lock entry */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock: no free lock slots\n");
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    lock->access_mode = mode;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock: allocated lock_id=%d\n", lock_id);
    
    return lock_id;
}

/* Unlock a file or directory */
static void _dos_unlock(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_unlock(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return;
    
    lock->refcount--;
    if (lock->refcount <= 0) {
        _lock_free(lock_id);
    }
}

/* Duplicate a lock */
static uint32_t _dos_duplock(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplock(): lock_id=%d\n", lock_id);
    
    if (lock_id == 0) return 0;
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Create a new lock with the same path */
    int new_lock_id = _lock_alloc();
    if (new_lock_id == 0) return 0;
    
    lock_entry_t *new_lock = &g_locks[new_lock_id];
    strncpy(new_lock->linux_path, lock->linux_path, sizeof(new_lock->linux_path) - 1);
    strncpy(new_lock->amiga_path, lock->amiga_path, sizeof(new_lock->amiga_path) - 1);
    new_lock->is_dir = lock->is_dir;
    new_lock->dir = NULL;
    new_lock->access_mode = lock->access_mode;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplock(): new_lock_id=%d\n", new_lock_id);
    return new_lock_id;
}

static uint32_t _dos_lockrecord(uint32_t fh68k, uint32_t offset, uint32_t length,
                                uint32_t mode, uint32_t timeout)
{
    int kind;
    int fd;
    struct stat st;
    bool exclusive;
    bool immediate;
    uint64_t deadline_us = 0;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_lockrecord(): fh=0x%08x offset=%u length=%u mode=%u timeout=%u\n",
            fh68k, offset, length, mode, timeout);

    if (fh68k == 0)
    {
        return 0;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (mode > 3)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_BAD_NUMBER);
        return 0;
    }

    if (kind != FILE_KIND_REGULAR)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_OBJECT_WRONG_TYPE);
        return 0;
    }

    if (fstat(fd, &st) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return 0;
    }

    exclusive = (mode == 0 || mode == 1);
    immediate = (mode == 1 || mode == 3);

    if (!immediate)
    {
        deadline_us = _timer_get_time_us() + ((uint64_t)timeout * 20000ULL);
    }

    for (;;)
    {
        if (_record_lock_find_conflict(st.st_dev, st.st_ino, fh68k,
                                       offset, length, exclusive) < 0)
        {
            if (!_record_lock_add(st.st_dev, st.st_ino, fh68k,
                                  offset, length, exclusive))
            {
                m68k_write_memory_32(fh68k + 40, ERROR_NO_FREE_STORE);
                return 0;
            }

            m68k_write_memory_32(fh68k + 40, 0);
            return 1;
        }

        if (immediate)
        {
            m68k_write_memory_32(fh68k + 40, ERROR_LOCK_COLLISION);
            return 0;
        }

        if (timeout == 0 || _timer_get_time_us() >= deadline_us)
        {
            m68k_write_memory_32(fh68k + 40, ERROR_LOCK_TIMEOUT);
            return 0;
        }

        usleep(1000);
    }
}

static uint32_t _dos_unlockrecord(uint32_t fh68k, uint32_t offset, uint32_t length)
{
    int kind;
    int fd;
    struct stat st;

    DPRINTF(LOG_DEBUG,
            "lxa: _dos_unlockrecord(): fh=0x%08x offset=%u length=%u\n",
            fh68k, offset, length);

    if (fh68k == 0)
    {
        return 0;
    }

    kind = m68k_read_memory_32(fh68k + 32);
    fd = m68k_read_memory_32(fh68k + 36);

    if (kind != FILE_KIND_REGULAR)
    {
        m68k_write_memory_32(fh68k + 40, ERROR_OBJECT_WRONG_TYPE);
        return 0;
    }

    if (fstat(fd, &st) != 0)
    {
        m68k_write_memory_32(fh68k + 40, errno2Amiga());
        return 0;
    }

    if (!_record_lock_remove(st.st_dev, st.st_ino, fh68k, offset, length))
    {
        m68k_write_memory_32(fh68k + 40, ERROR_RECORD_NOT_LOCKED);
        return 0;
    }

    m68k_write_memory_32(fh68k + 40, 0);
    return 1;
}

/* FileInfoBlock offsets (from dos/dos.h) */
#define FIB_fib_DiskKey      0   /* LONG */
#define FIB_fib_DirEntryType 4   /* LONG */
#define FIB_fib_FileName     8   /* char[108] */
#define FIB_fib_Protection   116 /* LONG */
#define FIB_fib_EntryType    120 /* LONG (obsolete) */
#define FIB_fib_Size         124 /* LONG */
#define FIB_fib_NumBlocks    128 /* LONG */
#define FIB_fib_Date         132 /* struct DateStamp (12 bytes) */
#define FIB_fib_Comment      144 /* char[80] */
#define FIB_fib_OwnerUID     224 /* UWORD */
#define FIB_fib_OwnerGID     226 /* UWORD */
#define FIB_fib_Reserved     228 /* char[32] */
#define FIB_SIZE             260

static uint32_t _default_owner_info_from_stat(const struct stat *st)
{
    return (((uint32_t)st->st_uid & 0xffffU) << 16) | ((uint32_t)st->st_gid & 0xffffU);
}

static void _write_owner_info(uint32_t fib68k, uint32_t owner_info)
{
    m68k_write_memory_16(fib68k + FIB_fib_OwnerUID, (owner_info >> 16) & 0xffffU);
    m68k_write_memory_16(fib68k + FIB_fib_OwnerGID, owner_info & 0xffffU);
}

static uint32_t _read_owner_info(const char *linux_path, const struct stat *st)
{
    char owner_buf[32];
    char *end = NULL;
    unsigned long owner_info;

    memset(owner_buf, 0, sizeof(owner_buf));

#ifdef HAVE_XATTR
    {
        ssize_t len = getxattr(linux_path, "user.amiga.owner", owner_buf, sizeof(owner_buf) - 1);

        if (len > 0)
        {
            owner_buf[len] = '\0';
            owner_info = strtoul(owner_buf, &end, 16);
            if (end != owner_buf)
                return (uint32_t)owner_info;
        }
    }
#endif

    {
        char sidecar_path[PATH_MAX + 8];
        FILE *f;

        snprintf(sidecar_path, sizeof(sidecar_path), "%s.owner", linux_path);
        f = fopen(sidecar_path, "r");
        if (f)
        {
            if (fgets(owner_buf, sizeof(owner_buf), f))
            {
                owner_info = strtoul(owner_buf, &end, 16);
                fclose(f);
                if (end != owner_buf)
                    return (uint32_t)owner_info;
            }
            else
            {
                fclose(f);
            }
        }
    }

    return _default_owner_info_from_stat(st);
}

/* Read file comment from xattr or sidecar file */
static void _read_file_comment(const char *linux_path, uint32_t fib68k)
{
    char comment[80];
    memset(comment, 0, sizeof(comment));
    
    #ifdef HAVE_XATTR
    /* Try to read from extended attribute first */
    ssize_t len = getxattr(linux_path, "user.amiga.comment", comment, sizeof(comment) - 1);
    if (len > 0) {
        comment[len] = '\0';
        for (int i = 0; i < (int)len && i < 79; i++) {
            m68k_write_memory_8(fib68k + FIB_fib_Comment + i, comment[i]);
        }
        m68k_write_memory_8(fib68k + FIB_fib_Comment + ((len < 79) ? len : 79), 0);
        return;
    }
    #endif
    
    /* Fallback: try sidecar file */
    char sidecar_path[PATH_MAX + 10];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.comment", linux_path);
    
    FILE *f = fopen(sidecar_path, "r");
    if (f) {
        if (fgets(comment, sizeof(comment), f)) {
            /* Remove trailing newline if present */
            int clen = strlen(comment);
            if (clen > 0 && comment[clen - 1] == '\n') {
                comment[clen - 1] = '\0';
                clen--;
            }
            for (int i = 0; i < clen && i < 79; i++) {
                m68k_write_memory_8(fib68k + FIB_fib_Comment + i, comment[i]);
            }
            m68k_write_memory_8(fib68k + FIB_fib_Comment + ((clen < 79) ? clen : 79), 0);
        }
        fclose(f);
        return;
    }
    
    /* No comment found - already cleared to 0 by FIB clear */
    m68k_write_memory_8(fib68k + FIB_fib_Comment, 0);
}

/* Examine a lock (fill FileInfoBlock) */
static int _dos_examine(uint32_t lock_id, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): lock_id=%d, fib68k=0x%08x\n", lock_id, fib68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): invalid lock_id\n");
        return 0;
    }
    
    struct stat st;
    if (stat(lock->linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Get the filename from the path */
    const char *filename = strrchr(lock->linux_path, '/');
    filename = filename ? filename + 1 : lock->linux_path;
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename (null-terminated C string, NOT BSTR) */
    int namelen = strlen(filename);
    if (namelen > 107) namelen = 107;  /* Leave room for null terminator */
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, filename[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);  /* Null terminator */
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(lock->linux_path, fib68k);
    
    _write_owner_info(fib68k, _read_owner_info(lock->linux_path, &st));
    
    /* If this is a directory, open it for ExNext iteration */
    if (S_ISDIR(st.st_mode) && !lock->dir) {
        lock->dir = opendir(lock->linux_path);
        DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): opened dir handle for ExNext\n");
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examine(): success, type=%d, size=%ld\n", type, st.st_size);
    return 1;
}

/* Examine next entry in directory */
static int _dos_exnext(uint32_t lock_id, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): lock_id=%d, fib68k=0x%08x\n", lock_id, fib68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): invalid lock_id\n");
        return 0;
    }
    
    if (!lock->is_dir) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): not a directory\n");
        return 0;
    }
    
    /* Open directory if not already open */
    if (!lock->dir) {
        lock->dir = opendir(lock->linux_path);
        if (!lock->dir) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): opendir failed: %s\n", strerror(errno));
            return 0;
        }
    }
    
    /* Read next entry, skipping . and .. */
    struct dirent *de;
    while ((de = readdir(lock->dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        break;
    }
    
    if (!de) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): no more entries\n");
        closedir(lock->dir);
        lock->dir = NULL;
        return 0; /* ERROR_NO_MORE_ENTRIES */
    }
    
    /* Build full path to get stat info */
    char fullpath[PATH_MAX];
    int n = snprintf(fullpath, sizeof(fullpath), "%s/%s", lock->linux_path, de->d_name);
    if (n >= (int)sizeof(fullpath)) {
        /* Path too long, skip this entry */
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): path too long, skipping\n");
        return _dos_exnext(lock_id, fib68k);
    }
    
    struct stat st;
    if (stat(fullpath, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): stat failed for %s: %s\n", fullpath, strerror(errno));
        /* Skip this entry and try next */
        return _dos_exnext(lock_id, fib68k);
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename (null-terminated C string, NOT BSTR) */
    int namelen = strlen(de->d_name);
    if (namelen > 107) namelen = 107;  /* Leave room for null terminator */
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, de->d_name[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);  /* Null terminator */
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(fullpath, fib68k);
    
    _write_owner_info(fib68k, _read_owner_info(fullpath, &st));
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_exnext(): success, name=%s, type=%d\n", de->d_name, type);
    return 1;
}

/* InfoData offsets (from dos/dos.h) */
#define ID_id_NumSoftErrors   0   /* LONG */
#define ID_id_UnitNumber      4   /* LONG */
#define ID_id_DiskState       8   /* LONG */
#define ID_id_NumBlocks       12  /* LONG */
#define ID_id_NumBlocksUsed   16  /* LONG */
#define ID_id_BytesPerBlock   20  /* LONG */
#define ID_id_DiskType        24  /* LONG */
#define ID_id_VolumeNode      28  /* BPTR */
#define ID_id_InUse           32  /* LONG */
#define ID_SIZE               36

#define ID_VALIDATED        82   /* ID_VALIDATED */
#define ID_DOS_DISK    0x444F5300  /* 'DOS\0' */

/* Get volume info */
static int _dos_info(uint32_t lock_id, uint32_t infodata68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_info(): lock_id=%d, infodata68k=0x%08x\n", lock_id, infodata68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_info(): invalid lock_id\n");
        return 0;
    }
    
    struct statvfs vfs;
    if (statvfs(lock->linux_path, &vfs) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_info(): statvfs failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Fill InfoData structure */
    m68k_write_memory_32(infodata68k + ID_id_NumSoftErrors, 0);
    m68k_write_memory_32(infodata68k + ID_id_UnitNumber, 0);
    m68k_write_memory_32(infodata68k + ID_id_DiskState, ID_VALIDATED);
    m68k_write_memory_32(infodata68k + ID_id_NumBlocks, vfs.f_blocks);
    m68k_write_memory_32(infodata68k + ID_id_NumBlocksUsed, vfs.f_blocks - vfs.f_bfree);
    m68k_write_memory_32(infodata68k + ID_id_BytesPerBlock, vfs.f_bsize);
    m68k_write_memory_32(infodata68k + ID_id_DiskType, ID_DOS_DISK);
    m68k_write_memory_32(infodata68k + ID_id_VolumeNode, 0);
    m68k_write_memory_32(infodata68k + ID_id_InUse, 0);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_info(): success, blocks=%lu, used=%lu\n", 
            vfs.f_blocks, vfs.f_blocks - vfs.f_bfree);
    return 1;
}

/* Check if two locks refer to the same object */
static int _dos_samelock(uint32_t lock1_id, uint32_t lock2_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_samelock(): lock1=%d, lock2=%d\n", lock1_id, lock2_id);
    
    if (lock1_id == lock2_id) return 0; /* LOCK_SAME */
    
    lock_entry_t *lock1 = _lock_get(lock1_id);
    lock_entry_t *lock2 = _lock_get(lock2_id);
    
    if (!lock1 || !lock2) return -1; /* LOCK_DIFFERENT */
    
    struct stat st1, st2;
    if (stat(lock1->linux_path, &st1) != 0) return -1;
    if (stat(lock2->linux_path, &st2) != 0) return -1;
    
    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
        return 0; /* LOCK_SAME */
    }
    
    if (st1.st_dev == st2.st_dev) {
        return 1; /* LOCK_SAME_VOLUME */
    }
    
    return -1; /* LOCK_DIFFERENT */
}

/* Get parent directory of a lock */
static uint32_t _dos_parentdir(uint32_t lock_id)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentdir(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Find parent path (linux) */
    char parent_path[PATH_MAX];
    strncpy(parent_path, lock->linux_path, sizeof(parent_path) - 1);
    
    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash || last_slash == parent_path) {
        /* Already at root */
        return 0;
    }
    *last_slash = '\0';
    
    /* Find parent path (amiga) */
    char parent_amiga[PATH_MAX];
    strncpy(parent_amiga, lock->amiga_path, sizeof(parent_amiga) - 1);
    
    /* Amiga paths use / or : as separators */
    char *last_sep = strrchr(parent_amiga, '/');
    char *colon = strchr(parent_amiga, ':');
    if (last_sep) {
        *last_sep = '\0';
    } else if (colon && colon[1] != '\0') {
        /* Path is like "SYS:dir" - parent is "SYS:" */
        colon[1] = '\0';
    }
    
    /* Create lock for parent */
    int new_lock_id = _lock_alloc();
    if (new_lock_id == 0) return 0;
    
    lock_entry_t *new_lock = &g_locks[new_lock_id];
    strncpy(new_lock->linux_path, parent_path, sizeof(new_lock->linux_path) - 1);
    strncpy(new_lock->amiga_path, parent_amiga, sizeof(new_lock->amiga_path) - 1);
    new_lock->is_dir = true;
    new_lock->dir = NULL;
    new_lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentdir(): parent=%s, amiga=%s, new_lock_id=%d\n", parent_path, parent_amiga, new_lock_id);
    return new_lock_id;
}

/* Create a directory */
static uint32_t _dos_createdir(uint32_t name68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): amiga_path=%s\n", amiga_path);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    if (mkdir(linux_path, 0755) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): mkdir failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Return a lock on the new directory */
    int lock_id = _lock_alloc();
    if (lock_id == 0) return 0;
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
    lock->is_dir = true;
    lock->dir = NULL;
    lock->access_mode = EXCLUSIVE_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_createdir(): success, lock_id=%d\n", lock_id);
    return lock_id;
}

/* Delete a file or directory */
static int _dos_deletefile(uint32_t name68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): amiga_path=%s\n", amiga_path);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    struct stat st;
    if (lstat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    int result;
    if (S_ISDIR(st.st_mode)) {
        result = rmdir(linux_path);
    } else {
        result = unlink(linux_path);
    }
    
    if (result != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): delete failed: %s\n", strerror(errno));
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        char sidecar[PATH_MAX + 32];
        if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", linux_path) < (int)sizeof(sidecar)) {
            unlink(sidecar);
        }
    }

    DPRINTF(LOG_DEBUG, "lxa: _dos_deletefile(): success\n");
    return 1;
}

/* Rename a file or directory */
static int _dos_rename(uint32_t old68k, uint32_t new68k)
{
    char *old_amiga = _mgetstr(old68k);
    char *new_amiga = _mgetstr(new68k);
    char old_linux[PATH_MAX], new_linux[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): old=%s, new=%s\n", old_amiga, new_amiga);
    
    if (!vfs_resolve_path(old_amiga, old_linux, sizeof(old_linux))) {
        _dos_path2linux(old_amiga, old_linux, sizeof(old_linux));
    }
    if (!vfs_resolve_path(new_amiga, new_linux, sizeof(new_linux))) {
        _dos_path2linux(new_amiga, new_linux, sizeof(new_linux));
    }
    
    if (rename(old_linux, new_linux) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): rename failed: %s\n", strerror(errno));
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_rename(): success\n");
    return 1;
}

/* Get path name from lock */
static int _dos_namefromlock(uint32_t lock_id, uint32_t buf68k, uint32_t buflen)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromlock(): lock_id=%d\n", lock_id);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) return 0;
    
    /* Return the Amiga path stored in the lock */
    const char *path = lock->amiga_path;
    size_t len = strlen(path);
    if (len >= buflen) len = buflen - 1;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromlock(): returning '%s'\n", path);
    
    for (size_t i = 0; i < len; i++) {
        m68k_write_memory_8(buf68k + i, path[i]);
    }
    m68k_write_memory_8(buf68k + len, '\0');
    
    return 1;
}

/*
 * Phase 4: Metadata operations
 */

/* Amiga protection bits from dos/dos.h (inverted logic - bit SET = protection denied) 
 * 
 * Layout:
 *   Bits 0-3:  Basic owner RWED (Read, Write, Execute, Delete)
 *   Bits 4-7:  Special flags (Archive, Pure, Script, Hold)
 *   Bits 8-11: Group RWED
 *   Bits 12-15: Other RWED
 */
#define FIBB_DELETE       0   /* Owner: delete */
#define FIBB_EXECUTE      1   /* Owner: execute */
#define FIBB_WRITE        2   /* Owner: write */
#define FIBB_READ         3   /* Owner: read */
#define FIBB_ARCHIVE      4   /* Archived bit */
#define FIBB_PURE         5   /* Pure bit (reentrant executable) */
#define FIBB_SCRIPT       6   /* Script bit */
#define FIBB_HOLD         7   /* Hold bit */
#define FIBB_GRP_DELETE   8   /* Group: delete */
#define FIBB_GRP_EXECUTE  9   /* Group: execute */
#define FIBB_GRP_WRITE    10  /* Group: write */
#define FIBB_GRP_READ     11  /* Group: read */
#define FIBB_OTR_DELETE   12  /* Other: delete */
#define FIBB_OTR_EXECUTE  13  /* Other: execute */
#define FIBB_OTR_WRITE    14  /* Other: write */
#define FIBB_OTR_READ     15  /* Other: read */

/* Set file protection bits (Amiga -> Linux mapping) */
static int _dos_setprotection(uint32_t name68k, uint32_t protect)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): amiga_path=%s, protect=0x%08x\n", amiga_path, protect);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    /* Amiga protection bits are inverted (bit SET = permission denied)
     * Map to Unix permissions:
     * - Basic owner bits (0-3) map to user rwx
     * - Group bits (8-11) map to group rwx  
     * - Other bits (12-15) map to other rwx
     * - Delete bit controls write permission
     */
    mode_t mode = 0;
    
    /* Owner permissions (inverted Amiga bits - bit clear means permission granted) */
    if (!(protect & (1 << FIBB_READ)))    mode |= S_IRUSR;
    if (!(protect & (1 << FIBB_WRITE)))   mode |= S_IWUSR;
    if (!(protect & (1 << FIBB_EXECUTE))) mode |= S_IXUSR;
    
    /* Group permissions */
    if (!(protect & (1 << FIBB_GRP_READ)))    mode |= S_IRGRP;
    if (!(protect & (1 << FIBB_GRP_WRITE)))   mode |= S_IWGRP;
    if (!(protect & (1 << FIBB_GRP_EXECUTE))) mode |= S_IXGRP;
    
    /* Other permissions */
    if (!(protect & (1 << FIBB_OTR_READ)))    mode |= S_IROTH;
    if (!(protect & (1 << FIBB_OTR_WRITE)))   mode |= S_IWOTH;
    if (!(protect & (1 << FIBB_OTR_EXECUTE))) mode |= S_IXOTH;
    
    /* If no permissions set, default to rw-r--r-- */
    if (mode == 0) mode = 0644;
    
    if (chmod(linux_path, mode) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): chmod failed: %s\n", strerror(errno));
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setprotection(): success, mode=0%03o\n", mode);
    return 1;
}

/* Set file comment (stored as xattr or sidecar file) */
static int _dos_setcomment(uint32_t name68k, uint32_t comment68k)
{
    char *amiga_path = _mgetstr(name68k);
    char *comment = _mgetstr(comment68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): amiga_path=%s, comment=%s\n", amiga_path, comment);
    
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    /* Check if file exists first */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): file not found: %s\n", linux_path);
        return 0;
    }
    
    /* Try to use extended attributes first (Linux native) */
    #ifdef HAVE_XATTR
    if (comment && strlen(comment) > 0) {
        if (setxattr(linux_path, "user.amiga.comment", comment, strlen(comment), 0) == 0) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): set xattr success\n");
            return 1;
        }
    } else {
        /* Empty comment = remove xattr */
        removexattr(linux_path, "user.amiga.comment");
        return 1;
    }
    #endif
    
    /* Fallback: use sidecar file (.comment extension) */
    char sidecar_path[PATH_MAX + 10];
    snprintf(sidecar_path, sizeof(sidecar_path), "%s.comment", linux_path);
    
    if (comment && strlen(comment) > 0) {
        FILE *f = fopen(sidecar_path, "w");
        if (!f) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): failed to create sidecar: %s\n", strerror(errno));
            return 0;
        }
        fprintf(f, "%s", comment);
        fclose(f);
        DPRINTF(LOG_DEBUG, "lxa: _dos_setcomment(): sidecar created\n");
    } else {
        /* Empty comment = delete sidecar */
        unlink(sidecar_path);
    }
    
    return 1;
}

static int _dos_setowner(uint32_t name68k, uint32_t owner68k, uint32_t err68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    struct stat st;
    char owner_buf[32];

    DPRINTF(LOG_DEBUG, "lxa: _dos_setowner(): amiga_path=%s, owner=0x%08x\n",
            amiga_path ? amiga_path : "NULL", owner68k);

    if (!amiga_path)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    if (stat(linux_path, &st) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

#ifdef HAVE_XATTR
    snprintf(owner_buf, sizeof(owner_buf), "%08x", owner68k);
    if (setxattr(linux_path, "user.amiga.owner", owner_buf, strlen(owner_buf), 0) == 0)
    {
        m68k_write_memory_32(err68k, 0);
        return 1;
    }
#endif

    {
        char sidecar_path[PATH_MAX + 8];
        FILE *f;

        snprintf(sidecar_path, sizeof(sidecar_path), "%s.owner", linux_path);
        f = fopen(sidecar_path, "w");
        if (!f)
        {
            m68k_write_memory_32(err68k, errno2Amiga());
            return 0;
        }

        snprintf(owner_buf, sizeof(owner_buf), "%08x", owner68k);
        fprintf(f, "%s", owner_buf);
        fclose(f);
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

static int _dos_setfiledate(uint32_t name68k, uint32_t date68k, uint32_t err68k)
{
    char *amiga_path = _mgetstr(name68k);
    char linux_path[PATH_MAX];
    struct stat st;
    struct timespec times[2];
    time_t unix_time;

    DPRINTF(LOG_DEBUG, "lxa: _dos_setfiledate(): amiga_path=%s, date=0x%08x\n",
            amiga_path ? amiga_path : "NULL", date68k);

    if (!amiga_path || !date68k)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        return 0;
    }

    if (stat(linux_path, &st) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    unix_time = _datestamp68k_to_unix_time(date68k);
    times[0].tv_sec = st.st_atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = unix_time;
    times[1].tv_nsec = (long)((m68k_read_memory_32(date68k + 8) % 50U) * 20000000U);

    if (utimensat(AT_FDCWD, linux_path, times, 0) != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

static int _dos_readlink(uint32_t path68k, uint32_t buffer68k, uint32_t size, uint32_t err68k)
{
    char *amiga_path = _mgetstr(path68k);
    char linux_path[PATH_MAX];
    char target[PATH_MAX];
    char sidecar[PATH_MAX + 32];
    ssize_t len;

    DPRINTF(LOG_DEBUG, "lxa: _dos_readlink(): amiga_path=%s, size=%u\n",
            amiga_path ? amiga_path : "NULL", size);

    if (!amiga_path || !buffer68k || size == 0)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return -1;
    }

    if (!_resolve_amiga_path_host(amiga_path, linux_path, sizeof(linux_path)))
    {
        const char *slash = strrchr(amiga_path, '/');
        if (slash)
        {
            char parent_amiga[PATH_MAX];
            char parent_linux[PATH_MAX];
            size_t parent_len = (size_t)(slash - amiga_path);
            if (parent_len >= sizeof(parent_amiga))
                parent_len = sizeof(parent_amiga) - 1;
            memcpy(parent_amiga, amiga_path, parent_len);
            parent_amiga[parent_len] = '\0';

            if (!_resolve_amiga_path_host(parent_amiga, parent_linux, sizeof(parent_linux)))
            {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
                return -1;
            }

            if (snprintf(linux_path, sizeof(linux_path), "%s/%s", parent_linux, slash + 1) >= (int)sizeof(linux_path))
            {
                m68k_write_memory_32(err68k, ERROR_BAD_STREAM_NAME);
                return -1;
            }
        }
        else
        {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            return -1;
        }
    }

    len = readlink(linux_path, target, sizeof(target) - 1);
    if (len < 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return -1;
    }

    target[len] = '\0';

    if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", linux_path) < (int)sizeof(sidecar)) {
        FILE *f = fopen(sidecar, "r");
        if (f) {
            len = fread(target, 1, sizeof(target) - 1, f);
            fclose(f);
            while (len > 0 && (target[len - 1] == '\n' || target[len - 1] == '\r'))
                len--;
            target[len] = '\0';
        }
    }

    if ((size_t)len >= size)
    {
        m68k_write_memory_32(err68k, ERROR_BUFFER_OVERFLOW);
        return -2;
    }

    for (ssize_t i = 0; i <= len; i++)
        m68k_write_memory_8(buffer68k + (uint32_t)i, (uint8_t)target[i]);

    m68k_write_memory_32(err68k, 0);
    return (int)len;
}

static int _dos_makelink(uint32_t name68k, uint32_t dest_param, int32_t soft, uint32_t err68k)
{
    char *name = _mgetstr(name68k);
    char name_linux[PATH_MAX];
    int result;

    DPRINTF(LOG_DEBUG, "lxa: _dos_makelink(): name=%s, dest=0x%08x, soft=%d\n",
            name ? name : "NULL", dest_param, soft);

    if (!name)
    {
        m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
        return 0;
    }

    if (!_resolve_amiga_path_host(name, name_linux, sizeof(name_linux)))
    {
        const char *slash = strrchr(name, '/');
        if (slash)
        {
            char parent_amiga[PATH_MAX];
            char parent_linux[PATH_MAX];
            size_t parent_len = (size_t)(slash - name);
            if (parent_len >= sizeof(parent_amiga))
                parent_len = sizeof(parent_amiga) - 1;
            memcpy(parent_amiga, name, parent_len);
            parent_amiga[parent_len] = '\0';

            if (!_resolve_amiga_path_host(parent_amiga, parent_linux, sizeof(parent_linux)))
            {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
                return 0;
            }

            if (snprintf(name_linux, sizeof(name_linux), "%s/%s", parent_linux, slash + 1) >= (int)sizeof(name_linux))
            {
                m68k_write_memory_32(err68k, ERROR_BAD_STREAM_NAME);
                return 0;
            }
        }
        else
        {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            return 0;
        }
    }

    if (soft == LINK_SOFT)
    {
        char *dest_name = _mgetstr(dest_param);
        char dest_linux[PATH_MAX];
        char sidecar[PATH_MAX + 32];

        if (!dest_name)
        {
            m68k_write_memory_32(err68k, ERROR_REQUIRED_ARG_MISSING);
            return 0;
        }

        if (!_resolve_amiga_path_host(dest_name, dest_linux, sizeof(dest_linux)))
        {
            strncpy(dest_linux, dest_name, sizeof(dest_linux) - 1);
            dest_linux[sizeof(dest_linux) - 1] = '\0';
        }

        result = symlink(dest_linux, name_linux);
        if (result == 0)
        {
            if (snprintf(sidecar, sizeof(sidecar), "%s.amiga.readlink", name_linux) < (int)sizeof(sidecar)) {
                FILE *f = fopen(sidecar, "w");
                if (f) {
                    fwrite(dest_name, 1, strlen(dest_name), f);
                    fclose(f);
                }
            }
        }
    }
    else
    {
        uint32_t lock_id = dest_param;
        lock_entry_t *lock = _lock_get(lock_id);

        if (!lock)
        {
            m68k_write_memory_32(err68k, ERROR_INVALID_LOCK);
            return 0;
        }

        result = link(lock->linux_path, name_linux);
    }

    if (result != 0)
    {
        m68k_write_memory_32(err68k, errno2Amiga());
        return 0;
    }

    m68k_write_memory_32(err68k, 0);
    return 1;
}

/*
 * Phase 7: Assignment System
 *
 * These functions handle assign operations. Assigns are managed by the VFS
 * layer, and these functions bridge the m68k emucalls to VFS.
 */

/* Add or replace an assign */
static int _dos_assign_add(uint32_t name68k, uint32_t path68k, uint32_t type)
{
    char *name = _mgetstr(name68k);
    char *path = _mgetstr(path68k);
    char linux_path[PATH_MAX];
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): name=%s, path=%s, type=%d\n", 
            name ? name : "NULL", path ? path : "NULL", type);
    
    if (!name || strlen(name) == 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): invalid name\n");
        return 0;
    }
    
    /* Determine if we received a Linux path or an Amiga path.
     * Linux paths start with '/' while Amiga paths contain ':'
     * Note: NameFromLock currently returns Linux paths, while
     * AssignLate/AssignPath pass Amiga paths directly.
     */
    if (path && strlen(path) > 0) {
        if (path[0] == '/') {
            /* Already a Linux path - use it directly */
            strncpy(linux_path, path, sizeof(linux_path));
            linux_path[sizeof(linux_path) - 1] = '\0';
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): using Linux path directly: %s\n", linux_path);
        } else {
            /* Amiga path - resolve to Linux */
            if (!vfs_resolve_path(path, linux_path, sizeof(linux_path))) {
                _dos_path2linux(path, linux_path, sizeof(linux_path));
            }
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): resolved Amiga path to: %s\n", linux_path);
        }
        
        /* Verify the path exists */
        struct stat st;
        if (stat(linux_path, &st) != 0) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): path not found: %s (errno=%d)\n", linux_path, errno);
            return 0;
        }
        
        /* For directory assigns, ensure it's actually a directory */
        if (!S_ISDIR(st.st_mode)) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): not a directory: %s\n", linux_path);
            return 0;
        }
    } else {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): empty path\n");
        linux_path[0] = '\0';
    }
    
    if (type == 3) {
        if (vfs_assign_add_path(name, linux_path)) {
            DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): add-path success\n");
            return 1;
        }

        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): add-path failed\n");
        return 0;
    }

    /* type: 0 = ASSIGN_LOCK, 1 = ASSIGN_LATE, 2 = ASSIGN_PATH */
    assign_type_t atype = (type <= 2) ? (assign_type_t)type : ASSIGN_LOCK;

    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): calling vfs_assign_add(name=%s, linux_path=%s, atype=%d)\n",
            name, linux_path, atype);

    if (vfs_assign_add(name, linux_path, atype)) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): success\n");
        return 1;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_add(): vfs_assign_add failed\n");
    return 0;
}

/* Remove an assign */
static int _dos_assign_remove(uint32_t name68k)
{
    char *name = _mgetstr(name68k);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_remove(): name=%s\n", name ? name : "NULL");
    
    if (!name || strlen(name) == 0) {
        return 0;
    }
    
    if (vfs_assign_remove(name)) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_assign_remove(): success\n");
        return 1;
    }
    
    return 0;
}

/* List assigns - returns count and writes packed strings to buffer */
static int _dos_assign_list(uint32_t buf68k, uint32_t buflen)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_list(): buf=0x%08x, buflen=%d\n", buf68k, buflen);
    
    /* Get list of assigns from VFS */
    const char *names[64];
    const char *paths[64];
    char amiga_paths[64][PATH_MAX];
    int count = vfs_assign_list(names, paths, 64);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_assign_list(): got %d assigns\n", count);
    
    if (buf68k == 0 || buflen == 0) {
        /* Just return the count */
        return count;
    }
    
    /* Write assigns to buffer as: name\0path\0name\0path\0...\0\0 */
    uint32_t offset = 0;
    int written = 0;
    
    for (int i = 0; i < count && offset < buflen - 2; i++) {
        size_t name_len = strlen(names[i]);
        const char *path = paths[i];
        size_t path_len;

        if (path && path[0] != '\0') {
            if (_linux_path_to_amiga(path, amiga_paths[i], sizeof(amiga_paths[i]))) {
                path = amiga_paths[i];
            }
        }

        path_len = path ? strlen(path) : 0;
        
        if (offset + name_len + 1 + path_len + 1 >= buflen - 1) {
            break; /* No more space */
        }
        
        /* Write name */
        for (size_t j = 0; j < name_len; j++) {
            m68k_write_memory_8(buf68k + offset++, names[i][j]);
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        /* Write path */
        if (path) {
            for (size_t j = 0; j < path_len; j++) {
                m68k_write_memory_8(buf68k + offset++, path[j]);
            }
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        written++;
    }
    
    /* Terminate with extra NUL */
    m68k_write_memory_8(buf68k + offset, '\0');
    
    return written;
}

static uint32_t _dos_make_lock_for_path(const char *linux_path, const char *amiga_path)
{
    struct stat st;
    int lock_id;
    lock_entry_t *lock;

    if (!linux_path || stat(linux_path, &st) != 0) {
        return 0;
    }

    lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }

    lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    lock->linux_path[sizeof(lock->linux_path) - 1] = '\0';

    if (amiga_path && amiga_path[0] != '\0') {
        strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);
        lock->amiga_path[sizeof(lock->amiga_path) - 1] = '\0';
    } else {
        if (!_linux_path_to_amiga(linux_path, lock->amiga_path, sizeof(lock->amiga_path))) {
            strncpy(lock->amiga_path, linux_path, sizeof(lock->amiga_path) - 1);
            lock->amiga_path[sizeof(lock->amiga_path) - 1] = '\0';
        }
    }

    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    return (uint32_t)lock_id;
}

static int _dos_assign_remove_path(uint32_t name68k, uint32_t path68k)
{
    char *name = _mgetstr(name68k);
    char *path = _mgetstr(path68k);
    char linux_path[PATH_MAX];

    if (!name || !path || name[0] == '\0' || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/') {
        strncpy(linux_path, path, sizeof(linux_path) - 1);
        linux_path[sizeof(linux_path) - 1] = '\0';
    } else {
        _dos_path2linux(path, linux_path, sizeof(linux_path));
    }

    return vfs_assign_remove_path(name, linux_path) ? 1 : 0;
}

static int _dos_getdevproc(uint32_t name68k, uint32_t dp68k, uint32_t err68k)
{
    char *name = _mgetstr(name68k);
    char volume[64];
    const char *colon;
    uint32_t index = 0;
    uint32_t lock_id = 0;
    char amiga_root[PATH_MAX];
    char linux_path[PATH_MAX];
    uint32_t flags = 0;

    if (err68k) {
        m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
    }

    if (!name || !dp68k) {
        return 0;
    }

    colon = strchr(name, ':');
    if (!colon) {
        strncpy(volume, name, sizeof(volume) - 1);
        volume[sizeof(volume) - 1] = '\0';
    } else {
        size_t len = (size_t)(colon - name);
        if (len >= sizeof(volume)) {
            len = sizeof(volume) - 1;
        }
        memcpy(volume, name, len);
        volume[len] = '\0';
    }

    if (m68k_read_memory_32(dp68k + DVP_DVP_DEVNODE) != 0) {
        index = m68k_read_memory_32(dp68k + DVP_DVP_DEVNODE);
    }

    if (vfs_assign_exists(volume)) {
        const char *paths[64];
        int count = vfs_assign_get_paths(volume, paths, 64);

        if ((int)index >= count) {
            if (err68k) {
                m68k_write_memory_32(err68k, ERROR_NO_MORE_ENTRIES);
            }
            return 0;
        }

        if (_linux_path_to_amiga(paths[index], amiga_root, sizeof(amiga_root)) == 0) {
            snprintf(amiga_root, sizeof(amiga_root), "%s:", volume);
        }

        lock_id = _dos_make_lock_for_path(paths[index], amiga_root);
        flags |= DVPF_ASSIGN;
        flags |= DVPF_UNLOCK;
        m68k_write_memory_32(dp68k + DVP_DVP_DEVNODE, index + 1);
    } else {
        if (volume[0] == '\0') {
            if (err68k) {
                m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
            }
            return 0;
        }

        snprintf(amiga_root, sizeof(amiga_root), "%s:", volume);
        _dos_path2linux(amiga_root, linux_path, sizeof(linux_path));
        lock_id = _dos_make_lock_for_path(linux_path, amiga_root);
        m68k_write_memory_32(dp68k + DVP_DVP_DEVNODE, 0);
    }

    if (!lock_id) {
        if (err68k) {
            m68k_write_memory_32(err68k, ERROR_OBJECT_NOT_FOUND);
        }
        return 0;
    }

    m68k_write_memory_32(dp68k + DVP_DVP_PORT, 0);
    m68k_write_memory_32(dp68k + DVP_DVP_LOCK, lock_id);
    m68k_write_memory_32(dp68k + DVP_DVP_FLAGS, flags);

    if (err68k) {
        m68k_write_memory_32(err68k, 0);
    }

    return (int)dp68k;
}

static void _notify_snapshot(notify_entry_t *entry)
{
    struct stat st;

    if (stat(entry->linux_path, &st) == 0) {
        entry->existed = true;
        entry->mtime = st.st_mtime;
        entry->size = st.st_size;
        entry->ino = st.st_ino;
        entry->dev = st.st_dev;
    } else {
        entry->existed = false;
        entry->mtime = 0;
        entry->size = 0;
        entry->ino = 0;
        entry->dev = 0;
    }
}

static bool _notify_changed(notify_entry_t *entry)
{
    struct stat st;
    bool exists_now = (stat(entry->linux_path, &st) == 0);

    if (exists_now != entry->existed) {
        _notify_snapshot(entry);
        return true;
    }

    if (!exists_now) {
        return false;
    }

    if (st.st_mtime != entry->mtime ||
        st.st_size != entry->size ||
        st.st_ino != entry->ino ||
        st.st_dev != entry->dev) {
        _notify_snapshot(entry);
        return true;
    }

    return false;
}

static int _dos_notify_start(uint32_t notify68k, uint32_t fullname68k, uint32_t flags)
{
    char *full_name = _mgetstr(fullname68k);
    char linux_path[PATH_MAX];

    if (!notify68k || !full_name || full_name[0] == '\0') {
        return 0;
    }

    _dos_path2linux(full_name, linux_path, sizeof(linux_path));

    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (g_notify_requests[i].in_use && g_notify_requests[i].notify68k == notify68k) {
            g_notify_requests[i].pending = (flags & NRF_NOTIFY_INITIAL) != 0;
            strncpy(g_notify_requests[i].linux_path, linux_path, sizeof(g_notify_requests[i].linux_path) - 1);
            g_notify_requests[i].linux_path[sizeof(g_notify_requests[i].linux_path) - 1] = '\0';
            _notify_snapshot(&g_notify_requests[i]);
            return 1;
        }
    }

    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (!g_notify_requests[i].in_use) {
            g_notify_requests[i].in_use = true;
            g_notify_requests[i].notify68k = notify68k;
            g_notify_requests[i].pending = (flags & NRF_NOTIFY_INITIAL) != 0;
            strncpy(g_notify_requests[i].linux_path, linux_path, sizeof(g_notify_requests[i].linux_path) - 1);
            g_notify_requests[i].linux_path[sizeof(g_notify_requests[i].linux_path) - 1] = '\0';
            _notify_snapshot(&g_notify_requests[i]);
            return 1;
        }
    }

    return 0;
}

static void _dos_notify_end(uint32_t notify68k)
{
    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (g_notify_requests[i].in_use && g_notify_requests[i].notify68k == notify68k) {
            memset(&g_notify_requests[i], 0, sizeof(g_notify_requests[i]));
            break;
        }
    }
}

static uint32_t _dos_notify_poll(void)
{
    for (int i = 0; i < MAX_NOTIFY_REQUESTS; i++) {
        if (!g_notify_requests[i].in_use) {
            continue;
        }

        if (!g_notify_requests[i].pending && !_notify_changed(&g_notify_requests[i])) {
            continue;
        }

        g_notify_requests[i].pending = false;
        return g_notify_requests[i].notify68k;
    }

    return 0;
}

/*
 * Phase 10: File Handle Utilities
 */

/* Get Linux path from fd using /proc/self/fd symlink */
static int _get_path_from_fd(int fd, char *buf, size_t bufsize)
{
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);
    
    ssize_t len = readlink(proc_path, buf, bufsize - 1);
    if (len < 0) {
        DPRINTF(LOG_DEBUG, "lxa: _get_path_from_fd(): readlink failed for fd=%d: %s\n", fd, strerror(errno));
        return 0;
    }
    buf[len] = '\0';
    return 1;
}

/* Convert Linux path back to Amiga path (best effort) */
static int _linux_path_to_amiga(const char *linux_path, char *amiga_buf, size_t bufsize)
{
    if (vfs_path_to_amiga(linux_path, amiga_buf, bufsize)) {
        return 1;
    }
    
    /* No matching assign found - use the Linux path as-is */
    strncpy(amiga_buf, linux_path, bufsize - 1);
    amiga_buf[bufsize - 1] = '\0';
    return 1;
}

/* DupLockFromFH - Get a lock from an open file handle */
static uint32_t _dos_duplockfromfh(uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): fh=0x%08x\n", fh68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): fd=%d, kind=%d\n", fd, kind);
    
    if (kind == FILE_KIND_CONSOLE) {
        /* Console doesn't have a meaningful lock */
        return 0;
    }
    
    /* Get the path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): linux_path='%s'\n", linux_path);
    
    /* Get the directory part of the path */
    char *last_slash = strrchr(linux_path, '/');
    if (last_slash && last_slash != linux_path) {
        *last_slash = '\0';  /* Truncate to directory */
    }
    
    /* Check if the path exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): stat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Allocate a lock entry */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    
    /* Try to build an Amiga path */
    char amiga_path[PATH_MAX];
    _linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path));
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);

    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplockfromfh(): returning lock_id=%d\n", lock_id);
    return lock_id;
}

/* ExamineFH - Examine an open file handle */
static int _dos_examinefh(uint32_t fh68k, uint32_t fib68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fh=0x%08x, fib=0x%08x\n", fh68k, fib68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fd=%d\n", fd);
    
    /* Get file info via fstat */
    struct stat st;
    if (fstat(fd, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): fstat failed: %s\n", strerror(errno));
        return 0;
    }
    
    /* Clear the FIB first */
    for (int i = 0; i < FIB_SIZE; i++) {
        m68k_write_memory_8(fib68k + i, 0);
    }
    
    /* Get the filename from the fd */
    char linux_path[PATH_MAX];
    const char *filename = "";
    if (_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        const char *last_slash = strrchr(linux_path, '/');
        filename = last_slash ? last_slash + 1 : linux_path;
    }
    
    /* Fill the FileInfoBlock */
    m68k_write_memory_32(fib68k + FIB_fib_DiskKey, (uint32_t)st.st_ino);
    
    int32_t type = S_ISDIR(st.st_mode) ? ST_USERDIR : ST_FILE;
    m68k_write_memory_32(fib68k + FIB_fib_DirEntryType, type);
    m68k_write_memory_32(fib68k + FIB_fib_EntryType, type);
    
    /* Write filename */
    int namelen = strlen(filename);
    if (namelen > 107) namelen = 107;
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + i, filename[i]);
    }
    m68k_write_memory_8(fib68k + FIB_fib_FileName + namelen, 0);
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_timespec_to_datestamp(st.st_mtime, st.st_mtim.tv_nsec, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    if (linux_path[0]) {
        _read_file_comment(linux_path, fib68k);
    }
    
    _write_owner_info(fib68k, _read_owner_info(linux_path, &st));
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_examinefh(): success, type=%d, size=%ld\n", type, (long)st.st_size);
    return 1;
}

/* NameFromFH - Get the full path name from a file handle */
static int _dos_namefromfh(uint32_t fh68k, uint32_t buf68k, uint32_t len)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): fh=0x%08x, buf=0x%08x, len=%d\n", fh68k, buf68k, len);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): fd=%d, kind=%d\n", fd, kind);
    
    if (kind == FILE_KIND_CONSOLE) {
        /* Console doesn't have a meaningful path */
        const char *console = "CONSOLE:";
        int clen = strlen(console);
        if ((uint32_t)clen >= len) {
            return 0;
        }
        for (int i = 0; i <= clen; i++) {
            m68k_write_memory_8(buf68k + i, console[i]);
        }
        return 1;
    }
    
    /* Get the Linux path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    /* Convert to Amiga path */
    char amiga_path[PATH_MAX];
    if (!_linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path))) {
        return 0;
    }
    
    /* Write to m68k buffer */
    size_t path_len = strlen(amiga_path);
    if (path_len >= len) {
        return 0;  /* Buffer too small */
    }
    
    for (size_t i = 0; i <= path_len; i++) {
        m68k_write_memory_8(buf68k + i, amiga_path[i]);
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_namefromfh(): returning '%s'\n", amiga_path);
    return 1;
}

/* ParentOfFH - Get a lock on the parent directory of an open file */
static uint32_t _dos_parentoffh(uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentoffh(): fh=0x%08x\n", fh68k);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */
    
    if (kind == FILE_KIND_CONSOLE) {
        return 0;
    }
    
    /* Get the Linux path from the fd */
    char linux_path[PATH_MAX];
    if (!_get_path_from_fd(fd, linux_path, sizeof(linux_path))) {
        return 0;
    }
    
    /* Get the parent directory */
    char *last_slash = strrchr(linux_path, '/');
    if (!last_slash) {
        return 0;
    }
    
    if (last_slash == linux_path) {
        /* Parent is root */
        linux_path[1] = '\0';
    } else {
        *last_slash = '\0';
    }
    
    /* Check if parent exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        return 0;
    }
    
    /* Allocate a lock for the parent */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    
    char amiga_path[PATH_MAX];
    _linux_path_to_amiga(linux_path, amiga_path, sizeof(amiga_path));
    strncpy(lock->amiga_path, amiga_path, sizeof(lock->amiga_path) - 1);

    lock->is_dir = TRUE;
    lock->dir = NULL;
    lock->access_mode = SHARED_LOCK;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentoffh(): returning lock_id=%d\n", lock_id);
    return lock_id;
}

/* OpenFromLock - Convert a lock to a file handle (lock is consumed) */
static int _dos_openfromlock(uint32_t lock_id, uint32_t fh68k)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): lock_id=%d, fh=0x%08x\n", lock_id, fh68k);
    
    lock_entry_t *lock = _lock_get(lock_id);
    if (!lock) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): invalid lock_id\n");
        return 0;
    }
    
    /* Can only open files, not directories */
    if (lock->is_dir) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): cannot open directory\n");
        return 0;
    }
    
    /* Open the file for reading */
    int fd = open(lock->linux_path, O_RDONLY);
    if (fd < 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): open failed: %s\n", strerror(errno));
        m68k_write_memory_32(fh68k + 40, errno2Amiga());  /* fh_Arg2 */
        return 0;
    }
    
    /* Set up the file handle */
    m68k_write_memory_32(fh68k + 36, fd);                /* fh_Args */
    m68k_write_memory_32(fh68k + 32, FILE_KIND_REGULAR); /* fh_Func3 */
    
    /* Free the lock - it's consumed */
    _lock_free(lock_id);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_openfromlock(): success, fd=%d\n", fd);
    return 1;
}

/* WaitForChar - Check if input is available on a file handle */
static int _dos_waitforchar(uint32_t fh68k, uint32_t timeout_us)
{
    DPRINTF(LOG_DEBUG, "lxa: _dos_waitforchar(): fh=0x%08x, timeout=%u us\n", fh68k, timeout_us);
    
    int fd = m68k_read_memory_32(fh68k + 36);   /* fh_Args */
    int kind = m68k_read_memory_32(fh68k + 32); /* fh_Func3 */

    if (kind == FILE_KIND_CONSOLE && !lxa_host_console_input_empty())
        return 1;
    
    /* Use select() to check for available input */
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    struct timeval tv;
    tv.tv_sec = timeout_us / 1000000;
    tv.tv_usec = timeout_us % 1000000;
    
    int result = select(fd + 1, &readfds, NULL, NULL, &tv);
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_waitforchar(): select returned %d\n", result);
    
    return (result > 0) ? 1 : 0;
}

static void _debug_add_bp (uint32_t addr)
{
    if (g_num_breakpoints < MAX_BREAKPOINTS)
    {
        g_breakpoints[g_num_breakpoints++] = addr;
        _update_debug_active();
    }
}

static bool _debug_toggle_bp (uint32_t addr)
{
    for (int i=0; i<g_num_breakpoints; i++)
    {
        if (addr==g_breakpoints[i])
        {
            for (int j=i; j<g_num_breakpoints-1; j++)
                g_breakpoints[j] = g_breakpoints[j+1];
            g_num_breakpoints--;
            _update_debug_active();

            return false;
        }
    }

    _debug_add_bp (addr);
    return true;
}

#define NUM_M68K_REGS 27

static char *_m68k_regnames[NUM_M68K_REGS] = {
    "d0",
    "d1",
    "d2",
    "d3",
    "d4",
    "d5",
    "d6",
    "d7",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    "pc",
    "sr",
    "sp",
    "usp",
    "isp",
    "msp",
    "sfc",
    "dfc",
    "vbr",
    "cacr",
    "caar"
};

static uint32_t _debug_parse_addr(const char *buf)
{
    uint32_t addr = _find_symbol (buf);
    if (addr)
        return addr;
    int n = sscanf (buf, "%x", &addr);
    if (n==1)
    {
        return addr;
    }
    else
    {
        for (int i=0; i<NUM_M68K_REGS; i++)
        {
            if (!strcmp(buf, _m68k_regnames[i]))
                return m68k_get_reg (NULL, i);
        }
    }
    return 0;
}

/* stolen from newlib */

#define YEAR_BASE    1900

#define _DAYS_IN_MONTH(x) ((x == 1) ? days_in_feb : __month_lengths[0][x])

static const int16_t _DAYS_BEFORE_MONTH[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

#define _DAYS_IN_YEAR(year) (isleap(year+YEAR_BASE) ? 366 : 365)

static inline int isleap (int y)
{
    // This routine must return exactly 0 or 1, because the result is used to index on __month_lengths[].
    // The order of checks below is the fastest for a random year.
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

int op_illg(int level)
{
    uint32_t d0 = m68k_get_reg(NULL, M68K_REG_D0);
    //DPRINTF (LOG_INFO, "ILLEGAL, d=%d\n", d0);

    switch (d0)
    {
        case EMU_CALL_LPUTC:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t ch  = m68k_get_reg(NULL, M68K_REG_D2);

            lputc (lvl, ch);

            break;
        }

        case EMU_CALL_LPUTS:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t s68k = m68k_get_reg(NULL, M68K_REG_D2);

            char *s = _mgetstr (s68k);

            lputs (lvl, s);

            break;
        }

        case EMU_CALL_STOP:
        {
            g_rv = m68k_get_reg(NULL, M68K_REG_D1);
            fprintf(stderr, "EMU_CALL_STOP called, rv=%d, current PC=0x%08x\n", 
                     g_rv, m68k_get_reg(NULL, M68K_REG_PC));
            DPRINTF (LOG_DEBUG, "EMU_CALL_STOP called, rv=%d, current PC=0x%08x\n", 
                     g_rv, m68k_get_reg(NULL, M68K_REG_PC));
            
            /*
             * Check if there are other tasks running. If so, we need to
             * properly terminate the current task via RemTask(NULL) and
             * let the scheduler continue running other tasks.
             * 
             * If no other tasks are running, we can exit immediately.
             */
            if (other_tasks_running())
            {
                DPRINTF (LOG_DEBUG, "other tasks running, calling RemTask(NULL) instead of stopping\n");
                
                /*
                 * Set up the CPU to call RemTask(NULL):
                 * - A6 = SysBase
                 * - A1 = NULL (remove current task)
                 * 
                 * The library jump table contains:
                 *   offset -288: JMP instruction (0x4ef9) + 32-bit address
                 * 
                 * We jump directly to the RemTask entry point. Since RemTask(NULL)
                 * doesn't return when removing the current task, we don't need to
                 * set up a return address.
                 */
                uint32_t sysbase = m68k_read_memory_32(4);
                
                /* The function address is at offset +2 from the jump table entry 
                 * (skip the JMP instruction opcode) */
                uint32_t remtask_addr = m68k_read_memory_32(sysbase - 288 + 2);
                
                DPRINTF (LOG_DEBUG, "setting PC=0x%08x (RemTask), A6=0x%08x, A1=0\n", remtask_addr, sysbase);
                
                m68k_set_reg(M68K_REG_A6, sysbase);
                m68k_set_reg(M68K_REG_A1, 0);  // NULL = remove current task
                m68k_set_reg(M68K_REG_PC, remtask_addr);
                
                /* Don't end timeslice or set g_running to FALSE - let it continue */
            }
            else
            {
                DPRINTF (LOG_DEBUG, "*** no other tasks, stopping emulator\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        case EMU_CALL_EXIT:
        {
            g_rv = m68k_get_reg(NULL, M68K_REG_D1);
            fprintf(stderr, "EMU_CALL_EXIT called, rv=%d\n", g_rv);
            DPRINTF (LOG_DEBUG, "*** emulator exit via libnix, rv=%d\n", g_rv);
            
            /*
             * Check if there are other tasks running. If so, we need to
             * properly terminate the current task via RemTask(NULL) and
             * let the scheduler continue running other tasks.
             * 
             * If no other tasks are running, we can exit immediately.
             */
            if (other_tasks_running())
            {
                DPRINTF (LOG_DEBUG, "*** other tasks running, calling RemTask(NULL) instead of exiting\n");
                
                /*
                 * Set up the CPU to call RemTask(NULL):
                 * - A6 = SysBase
                 * - A1 = NULL (remove current task)
                 * 
                 * The library jump table contains:
                 *   offset -288: JMP instruction (0x4ef9) + 32-bit address
                 * 
                 * We jump directly to the RemTask entry point. Since RemTask(NULL)
                 * doesn't return when removing the current task, we don't need to
                 * set up a return address.
                 */
                uint32_t sysbase = m68k_read_memory_32(4);
                /* The function address is at offset +2 from the jump table entry 
                 * (skip the JMP instruction opcode) */
                uint32_t remtask_addr = m68k_read_memory_32(sysbase - 288 + 2);
                
                DPRINTF (LOG_DEBUG, "*** calling RemTask at 0x%08x\n", remtask_addr);
                
                m68k_set_reg(M68K_REG_A6, sysbase);
                m68k_set_reg(M68K_REG_A1, 0);  // NULL = remove current task
                m68k_set_reg(M68K_REG_PC, remtask_addr);
                
                /* Don't end timeslice or set g_running to FALSE - let it continue */
            }
            else
            {
                DPRINTF (LOG_DEBUG, "*** no other tasks, exiting emulator\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        case EMU_CALL_TRACE:
            g_trace = m68k_get_reg(NULL, M68K_REG_D1);
            _update_debug_active();
            DPRINTF (LOG_DEBUG, "set emulator tracing to %d\n", g_trace);
            break;

        case EMU_CALL_EXCEPTION:
        {
            uint32_t excn = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t isp  = m68k_get_reg(NULL, M68K_REG_ISP);

            uint32_t d0 = m68k_read_memory_32 (isp);
            uint32_t d1 = m68k_read_memory_32 (isp+4);
            uint32_t pc = m68k_read_memory_32 (isp+10);

            m68k_set_reg (M68K_REG_D0, d0);
            m68k_set_reg (M68K_REG_D1, d1);

            LPRINTF (LOG_WARNING, "*** EXCEPTION CAUGHT: pc=0x%08x #%2d ", pc, excn);

            switch (excn)
            {
                case  2: LPRINTF (LOG_WARNING, "bus error\n"); break;
                case  3: LPRINTF (LOG_WARNING, "address error\n"); break;
                case  4: LPRINTF (LOG_WARNING, "illegal instruction\n"); break;
                case  5: LPRINTF (LOG_WARNING, "divide by zero\n"); break;
                case  6: LPRINTF (LOG_WARNING, "chk instruction\n"); break;
                case  7: LPRINTF (LOG_WARNING, "trapv instruction\n"); break;
                case  8: LPRINTF (LOG_WARNING, "privilege violation\n"); break;
                case  9: LPRINTF (LOG_WARNING, "trace\n"); break;
                case 10: LPRINTF (LOG_WARNING, "line 1010 emulator\n"); break;
                case 11: LPRINTF (LOG_WARNING, "line 1111 emulator\n"); break;
                case 32: LPRINTF (LOG_WARNING, "trap #0\n"); break;
                case 33: LPRINTF (LOG_WARNING, "trap #1\n"); break;
                case 34: LPRINTF (LOG_WARNING, "trap #2 (stack overflow)\n"); break;
                case 35: LPRINTF (LOG_WARNING, "trap #3\n"); break;
                case 36: LPRINTF (LOG_WARNING, "trap #4\n"); break;
                case 37: LPRINTF (LOG_WARNING, "trap #5\n"); break;
                case 38: LPRINTF (LOG_WARNING, "trap #6\n"); break;
                case 39: LPRINTF (LOG_WARNING, "trap #7\n"); break;
                case 40: LPRINTF (LOG_WARNING, "trap #8\n"); break;
                case 41: LPRINTF (LOG_WARNING, "trap #9\n"); break;
                case 42: LPRINTF (LOG_WARNING, "trap #10\n"); break;
                case 43: LPRINTF (LOG_WARNING, "trap #11\n"); break;
                case 44: LPRINTF (LOG_WARNING, "trap #12\n"); break;
                case 45: LPRINTF (LOG_WARNING, "trap #13\n"); break;
                case 46: LPRINTF (LOG_WARNING, "trap #14\n"); break;
                default: LPRINTF (LOG_WARNING, "???\n"); break;
            }

            hexdump (LOG_WARNING, isp, 16);

            /*
             * Phase 32: Don't halt on exceptions in multitasking scenarios.
             * 
             * Instead of entering the debugger and halting, we just log the exception
             * and let the exception handler return via RTE. In a real Amiga, this would
             * typically cause the crashing task to hang or loop, but other tasks can
             * continue running.
             *
             * This allows test programs (running as background processes) to detect
             * windows that were opened before the app crashed.
             *
             * For debugging, use -d flag which enables verbose output.
             *
             * Phase 54: Trap exceptions (vectors 32-46) are always fatal.
             * Traps like #2 (stack overflow) indicate the task cannot continue.
             * We must halt the emulator since returning would cause undefined behavior.
             */
            bool is_trap = (excn >= 32 && excn <= 46);
            
            if (g_debug || is_trap) {
                if (is_trap) {
                    LPRINTF (LOG_ERROR, "*** FATAL: Trap #%d - task cannot continue\n", excn - 32);
                    fprintf(stderr, "*** FATAL: Trap #%d at PC=0x%08x\n", excn - 32, pc);
                }
                _debug(pc);
                m68k_end_timeslice();
                g_running = FALSE;
            } else {
                LPRINTF (LOG_WARNING, "*** Exception in task - continuing (use -d to halt and debug)\n");
            }
            break;
        }

        case EMU_CALL_WAIT:
        {
            /*
             * Phase 6.5: Improved wait handling
             *
             * When the scheduler dispatch loop has no ready tasks, it calls
             * EMU_CALL_WAIT. We poll SDL events and sleep briefly.
             * 
             * We also ensure VBlank interrupt is set if pending, so that input
             * processing can happen when we return to the dispatch loop.
             */
            if (display_poll_events())
            {
                /* Quit requested via SDL */
                g_running = false;
            }
            
            /*
             * Check if ALL tasks have exited (no tasks ready, no tasks waiting,
             * and no current task). If so, the emulator should stop.
             * 
             * This handles the case where the last task exits via Exit()/RemTask()
             * rather than emu_stop(), leaving the Dispatch loop waiting forever.
             */
            if (!other_tasks_running())
            {
                uint32_t sysbase = m68k_read_memory_32(4);
                uint32_t thisTask = m68k_read_memory_32(sysbase + EXECBASE_THISTASK);
                
                if (thisTask == 0)
                {
                    fprintf(stderr, "*** EMU_CALL_WAIT: no tasks left, stopping emulator\n");
                    DPRINTF(LOG_DEBUG, "*** EMU_CALL_WAIT: no tasks left, stopping emulator\n");
                    m68k_end_timeslice();
                    g_running = false;
                    break;
                }
            }
            
            /* If VBlank is pending, set the IRQ now so it fires after we return */
            if ((g_pending_irq & (1 << 3)) && 
                (g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK))
            {
                g_pending_irq &= ~(1 << 3);
                m68k_set_irq(0);    /* Musashi edge-trigger: lower first */
                m68k_set_irq(3);
            }
            
            usleep(1000);  /* 1ms - avoid busy-waiting */
            break;
        }

        case EMU_CALL_DELAY:
        {
            /*
             * EMU_CALL_DELAY: Simple delay for the specified milliseconds.
             * Note: This is not used by the current Delay() implementation
             * but kept for potential future use.
             */
            uint32_t ms = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_DELAY %u ms\n", ms);
            usleep(ms * 1000);
            break;
        }

        case EMU_CALL_MONITOR:
        {
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_MONITOR\n");

            _debug(m68k_get_reg(NULL, M68K_REG_PC));

            break;
        }

        case EMU_CALL_LOADFILE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_LOADFILE d1=0x%08x\n", d1);

            for (int i=0; i<=strlen(g_loadfile); i++)
                m68k_write_memory_8 (d1++, g_loadfile[i]);

            break;
        }

        case EMU_CALL_GETARGS:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GETARGS d1=0x%08x, len=%d\n", d1, g_args_len);

            for (int i=0; i<=g_args_len; i++)
                m68k_write_memory_8 (d1++, g_args[i]);

            break;
        }

        case EMU_CALL_LOADED:
        {
            /* Handle pending breakpoints */
            for (pending_bp_t *pbp = _g_pending_bps; pbp; pbp=pbp->next)
            {
                uint32_t addr = _debug_parse_addr (pbp->name);
                if (!addr)
                {
                    fprintf (stderr, "*** error: failed to resolve pending breakpoint '%s'\n", pbp->name);
                    exit(23);
                }
                DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_LOADED pbp '%s' -> addr=0x%08lx\n", pbp->name, addr);
                _debug_add_bp (addr);
            }
            
            /*
             * Set up program-local assigns: Add the program's subdirectories to
             * standard assigns so programs can find their local config files.
             * This simulates what users typically did on real Amigas via startup
             * scripts: "Assign S: PROGDIR:S ADD"
             */
            {
                char amiga_progdir[PATH_MAX];
                char linux_progdir[PATH_MAX];
                char subdir[PATH_MAX];
                struct stat st;
                
                /* Extract the directory containing the loaded program (Amiga path) */
                strncpy(amiga_progdir, g_loadfile, sizeof(amiga_progdir) - 1);
                amiga_progdir[sizeof(amiga_progdir) - 1] = '\0';
                
                /* Find the last path separator (either / or after :) */
                char *last_slash = strrchr(amiga_progdir, '/');
                char *colon = strrchr(amiga_progdir, ':');
                
                /* If no slash, or colon is after the last slash, the file is directly in the volume/assign */
                if (last_slash && (!colon || last_slash > colon)) {
                    *last_slash = '\0';
                } else if (colon) {
                    /* Path is "VOLUME:filename" - directory is "VOLUME:" */
                    *(colon + 1) = '\0';
                } else {
                    /* No separator found - shouldn't happen, but handle gracefully */
                    amiga_progdir[0] = '\0';
                }
                
                /* Resolve Amiga path to Linux path */
                if (amiga_progdir[0] && vfs_resolve_path(amiga_progdir, linux_progdir, sizeof(linux_progdir))) {
                    /* Set PROGDIR: for this program (using the Linux path) */
                    vfs_set_progdir(linux_progdir);
                    
                    /* Standard Amiga directories to add to assigns */
                    struct {
                        const char *dir;
                        const char *assign;
                    } subdirs[] = {
                        {"s", "S"},
                        {"S", "S"},
                        {"libs", "LIBS"},
                        {"Libs", "LIBS"},
                        {"c", "C"},
                        {"C", "C"},
                        {"devs", "DEVS"},
                        {"Devs", "DEVS"},
                        {"l", "L"},
                        {"L", "L"},
                        {NULL, NULL}
                    };
                    
                    for (int i = 0; subdirs[i].dir; i++) {
                        snprintf(subdir, sizeof(subdir), "%s/%s", linux_progdir, subdirs[i].dir);
                        if (stat(subdir, &st) == 0 && S_ISDIR(st.st_mode)) {
                            /* Prepend this path to the assign (so it's searched first) */
                            vfs_assign_prepend_path(subdirs[i].assign, subdir);
                            DPRINTF(LOG_DEBUG, "lxa: Added %s to %s: assign\n", subdir, subdirs[i].assign);
                        }
                    }
                }
            }
            
            break;
        }

        case EMU_CALL_SYMBOL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);

            char *name = _mgetstr(d2);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_SYMBOL name=%s, offset=0x%08lx\n", name, d1);
            _symtab_add (name, d1);

            break;
        }

        case EMU_CALL_GETSYSTIME:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            struct timeval tv;
            gettimeofday(&tv, 0);
            struct tm t;
            localtime_r(&tv.tv_sec, &t);
            DPRINTF (LOG_DEBUG, "lxa: lxa: op_illg(): EMU_CALL_GETSYSTIME %02ld:%02ld:%02ld\n",
                     t.tm_hour, t.tm_min, t.tm_sec);

            time_t tim = t.tm_sec + t.tm_min * 60 + t.tm_hour * 3600;

            /* compute days in year */
            long days = t.tm_mday - 1;
            days += _DAYS_BEFORE_MONTH[t.tm_mon];
            if (t.tm_mon > 1 && isleap (t.tm_year+YEAR_BASE))
                days++;

            /* compute day of the year */
            t.tm_yday = days;

            // amiga's epoch starts at Jan 1st, 1978
            if (t.tm_year > 78)
            {
                for (int year = 78; year < t.tm_year; year++)
                    days += _DAYS_IN_YEAR (year);
            }

            /* compute total seconds */
            tim += (time_t) days * 24*60*60;

            m68k_write_memory_32 (d1,   tim);        // ULONG tv_secs
            m68k_write_memory_32 (d1+4, tv.tv_usec); // ULONG tv_micro

            break;
        }

        case EMU_CALL_DOS_OPEN:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OPEN name=0x%08x, accessMode=0x%08x, fh=0x%08x\n", d1, d2, d3);

            uint32_t res = _dos_open (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_READ:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_READ file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_read (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_SEEK:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SEEK file=0x%08x, position=0x%08x, mode=%d\n", d1, d2, d3);

            uint32_t res = _dos_seek (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_CLOSE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_CLOSE file=0x%08x\n", d1);

            uint32_t res = _dos_close (d1);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_INPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_INPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh, 1);
            break;
        }

        case EMU_CALL_DOS_OUTPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OUTPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh, 0);
            break;
        }

        case EMU_CALL_DOS_WRITE:
        {
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE\n");

            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_write (d1, d2, d3);

            //_debug(m68k_get_reg(NULL, M68K_REG_PC));

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        /*
         * Phase 3: Lock and Examine API emucalls
         */
        case EMU_CALL_DOS_LOCK:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            int32_t mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_LOCK name=0x%08x, mode=%d\n", name, mode);

            uint32_t res = _dos_lock(name, mode);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_UNLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_UNLOCK lock_id=%d\n", lock_id);

            _dos_unlock(lock_id);
            break;
        }

        case EMU_CALL_DOS_DUPLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DUPLOCK lock_id=%d\n", lock_id);

            uint32_t res = _dos_duplock(lock_id);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXAMINE:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXAMINE lock_id=%d, fib=0x%08x\n", lock_id, fib);

            uint32_t res = _dos_examine(lock_id, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXNEXT:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXNEXT lock_id=%d, fib=0x%08x\n", lock_id, fib);

            uint32_t res = _dos_exnext(lock_id, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_INFO:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t infodata = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_INFO lock_id=%d, infodata=0x%08x\n", lock_id, infodata);

            uint32_t res = _dos_info(lock_id, infodata);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SAMELOCK:
        {
            uint32_t lock1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t lock2 = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SAMELOCK lock1=%d, lock2=%d\n", lock1, lock2);

            int32_t res = _dos_samelock(lock1, lock2);
            m68k_set_reg(M68K_REG_D0, (uint32_t)res);
            break;
        }

        case EMU_CALL_DOS_PARENTDIR:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_PARENTDIR lock_id=%d\n", lock_id);

            uint32_t res = _dos_parentdir(lock_id);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_CREATEDIR:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_CREATEDIR name=0x%08x\n", name);

            uint32_t res = _dos_createdir(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_DELETE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DELETE name=0x%08x\n", name);

            uint32_t res = _dos_deletefile(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_RENAME:
        {
            uint32_t old_name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t new_name = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_RENAME old=0x%08x, new=0x%08x\n", old_name, new_name);

            uint32_t res = _dos_rename(old_name, new_name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NAMEFROMLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t len = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_NAMEFROMLOCK lock_id=%d, buf=0x%08x, len=%d\n", lock_id, buf, len);

            uint32_t res = _dos_namefromlock(lock_id, buf, len);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETPROTECTION:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t protect = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETPROTECTION name=0x%08x, protect=0x%08x\n", name, protect);

            uint32_t res = _dos_setprotection(name, protect);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETCOMMENT:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t comment = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETCOMMENT name=0x%08x, comment=0x%08x\n", name, comment);

            uint32_t res = _dos_setcomment(name, comment);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETOWNER:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t owner = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETOWNER name=0x%08x, owner=0x%08x, err=0x%08x\n",
                    name, owner, err);

            uint32_t res = _dos_setowner(name, owner, err);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_FLUSH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_FLUSH fh=0x%08x\n", fh);

            uint32_t res = _dos_flush(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        /* Phase 7: Assignment System */
        case EMU_CALL_DOS_ASSIGN_ADD:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t type = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_ADD name=0x%08x, path=0x%08x, type=%d\n", name, path, type);

            uint32_t res = _dos_assign_add(name, path, type);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_REMOVE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_REMOVE name=0x%08x\n", name);

            uint32_t res = _dos_assign_remove(name);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_LIST:
        {
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buflen = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_ASSIGN_LIST buf=0x%08x, buflen=%d\n", buf, buflen);

            uint32_t res = _dos_assign_list(buf, buflen);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_ASSIGN_REMOVE_PATH:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D2);

            uint32_t res = _dos_assign_remove_path(name, path);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_GETDEVPROC:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dp = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t errptr = m68k_get_reg(NULL, M68K_REG_D3);

            uint32_t res = _dos_getdevproc(name, dp, errptr);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_START:
        {
            uint32_t notify = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fullname = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t flags = m68k_get_reg(NULL, M68K_REG_D3);

            uint32_t res = _dos_notify_start(notify, fullname, flags);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_END:
        {
            uint32_t notify = m68k_get_reg(NULL, M68K_REG_D1);

            _dos_notify_end(notify);
            break;
        }

        case EMU_CALL_DOS_NOTIFY_POLL:
        {
            uint32_t res = _dos_notify_poll();
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        /*
         * Phase 10: File Handle Utilities
         */
        case EMU_CALL_DOS_DUPLOCKFROMFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_DUPLOCKFROMFH fh=0x%08x\n", fh);

            uint32_t res = _dos_duplockfromfh(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_EXAMINEFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fib = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_EXAMINEFH fh=0x%08x, fib=0x%08x\n", fh, fib);

            uint32_t res = _dos_examinefh(fh, fib);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_NAMEFROMFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buf = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t len = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_NAMEFROMFH fh=0x%08x, buf=0x%08x, len=%d\n", fh, buf, len);

            uint32_t res = _dos_namefromfh(fh, buf, len);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_PARENTOFFH:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_PARENTOFFH fh=0x%08x\n", fh);

            uint32_t res = _dos_parentoffh(fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_OPENFROMLOCK:
        {
            uint32_t lock_id = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OPENFROMLOCK lock_id=%d, fh=0x%08x\n", lock_id, fh);

            uint32_t res = _dos_openfromlock(lock_id, fh);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_WAITFORCHAR:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t timeout = m68k_get_reg(NULL, M68K_REG_D2);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WAITFORCHAR fh=0x%08x, timeout=%u\n", fh, timeout);

            uint32_t res = _dos_waitforchar(fh, timeout);
            m68k_set_reg(M68K_REG_D0, res);
            break;
        }

        case EMU_CALL_DOS_SETFILESIZE:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            int32_t offset = (int32_t)m68k_get_reg(NULL, M68K_REG_D2);
            int32_t mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETFILESIZE fh=0x%08x, offset=%d, mode=%d\n",
                    fh, offset, mode);

            m68k_set_reg(M68K_REG_D0, _dos_setfilesize(fh, offset, mode));
            break;
        }

        case EMU_CALL_DOS_SETFILEDATE:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t date = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SETFILEDATE name=0x%08x, date=0x%08x, err=0x%08x\n",
                    name, date, err);

            m68k_set_reg(M68K_REG_D0, _dos_setfiledate(name, date, err));
            break;
        }

        case EMU_CALL_DOS_READLINK:
        {
            uint32_t path = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buffer = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t size = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_READLINK path=0x%08x, buffer=0x%08x, size=%u, err=0x%08x\n",
                    path, buffer, size, err);

            m68k_set_reg(M68K_REG_D0, _dos_readlink(path, buffer, size, err));
            break;
        }

        case EMU_CALL_DOS_MAKELINK:
        {
            uint32_t name = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dest = m68k_get_reg(NULL, M68K_REG_D2);
            int32_t soft = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_MAKELINK name=0x%08x, dest=0x%08x, soft=%d, err=0x%08x\n",
                    name, dest, soft, err);

            m68k_set_reg(M68K_REG_D0, _dos_makelink(name, dest, soft, err));
            break;
        }

        case EMU_CALL_DOS_LOCKRECORD:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t mode = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t timeout = m68k_get_reg(NULL, M68K_REG_D5);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_LOCKRECORD fh=0x%08x offset=%u length=%u mode=%u timeout=%u\n",
                    fh, offset, length, mode, timeout);

            m68k_set_reg(M68K_REG_D0, _dos_lockrecord(fh, offset, length, mode, timeout));
            break;
        }

        case EMU_CALL_DOS_UNLOCKRECORD:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_UNLOCKRECORD fh=0x%08x offset=%u length=%u\n",
                    fh, offset, length);

            m68k_set_reg(M68K_REG_D0, _dos_unlockrecord(fh, offset, length));
            break;
        }

        case EMU_CALL_DOS_CHANGEMODE:
        {
            uint32_t type = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t object = m68k_get_reg(NULL, M68K_REG_D2);
            int32_t new_mode = (int32_t)m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t err = m68k_get_reg(NULL, M68K_REG_D4);

            DPRINTF(LOG_DEBUG,
                    "lxa: op_illg(): EMU_CALL_DOS_CHANGEMODE type=%u object=0x%08x new_mode=%d err=0x%08x\n",
                    type, object, new_mode, err);

            m68k_set_reg(M68K_REG_D0, _dos_change_mode(type, object, new_mode, err));
            break;
        }

        case EMU_CALL_TRACKDISK_READ:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_read(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_WRITE:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_write(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_FORMAT:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D5);

            m68k_set_reg(M68K_REG_D0, _trackdisk_format(unit, data, length, offset, is_ext));
            break;
        }

        case EMU_CALL_TRACKDISK_SEEK:
        {
            uint32_t unit = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t offset = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t is_ext = m68k_get_reg(NULL, M68K_REG_D3);

            m68k_set_reg(M68K_REG_D0, _trackdisk_seek(unit, offset, is_ext));
            break;
        }

        /*
         * Graphics Library emucalls (2000-2999)
         * Phase 13: Graphics Foundation
         */

        case EMU_CALL_GFX_INIT:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_INIT\n");
            bool success = display_init();
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_GFX_SHUTDOWN:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SHUTDOWN\n");
            display_shutdown();
            break;
        }

        case EMU_CALL_GFX_OPEN_DISPLAY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            int width = (int)d1;
            int height = (int)d2;
            int depth = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_OPEN_DISPLAY %dx%dx%d\n",
                    width, height, depth);

            display_t *display = display_open(width, height, depth, "LXA Amiga Display");
            /* Store display handle as a host pointer - we'll use the pointer address as a handle */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)display);
            break;
        }

        case EMU_CALL_GFX_CLOSE_DISPLAY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_CLOSE_DISPLAY handle=0x%08x\n", d1);

            display_close(display);
            break;
        }

        case EMU_CALL_GFX_REFRESH:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_REFRESH handle=0x%08x\n", d1);

            display_refresh(display);
            break;
        }

        case EMU_CALL_GFX_SET_COLOR:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *display = (display_t *)(uintptr_t)d1;
            int index = (int)(d2 & 0xFF);
            uint8_t r = (d3 >> 16) & 0xFF;
            uint8_t g = (d3 >> 8) & 0xFF;
            uint8_t b = d3 & 0xFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_COLOR handle=0x%08x, idx=%d, rgb=0x%06x\n",
                    d1, index, d3 & 0xFFFFFF);

            display_set_color(display, index, r, g, b);
            break;
        }

        case EMU_CALL_GFX_SET_PALETTE4:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int start = (int)d2;
            int count = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_PALETTE4 handle=0x%08x, start=%d, count=%d, colors=0x%08x\n",
                    d1, start, count, a0);

            if (a0 && count > 0)
            {
                /* Read RGB4 values from m68k memory */
                uint16_t *colors = malloc(count * sizeof(uint16_t));
                if (colors)
                {
                    for (int i = 0; i < count; i++)
                    {
                        colors[i] = m68k_read_memory_16(a0 + i * 2);
                    }
                    display_set_palette_rgb4(display, start, count, colors);
                    free(colors);
                }
            }
            break;
        }

        case EMU_CALL_GFX_SET_PALETTE32:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int start = (int)d2;
            int count = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_SET_PALETTE32 handle=0x%08x, start=%d, count=%d, colors=0x%08x\n",
                    d1, start, count, a0);

            if (a0 && count > 0)
            {
                /* Read RGB32 values from m68k memory */
                uint32_t *colors = malloc(count * sizeof(uint32_t));
                if (colors)
                {
                    for (int i = 0; i < count; i++)
                    {
                        colors[i] = m68k_read_memory_32(a0 + i * 4);
                    }
                    display_set_palette_rgb32(display, start, count, colors);
                    free(colors);
                }
            }
            break;
        }

        case EMU_CALL_GFX_WRITE_PIXEL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            uint8_t pen = (uint8_t)d4;

            /* Write pixel directly to display pixel buffer */
            display_update_chunky(display, x, y, 1, 1, &pen, 1);
            break;
        }

        case EMU_CALL_GFX_READ_PIXEL:
        {
            /* TODO: Implement pixel read - requires access to display internal buffer */
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_READ_PIXEL (not implemented)\n");
            m68k_set_reg(M68K_REG_D0, 0);
            break;
        }

        case EMU_CALL_GFX_RECT_FILL:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x1 = (int)d2;
            int y1 = (int)d3;
            int x2 = (int)d4;
            int y2 = (int)d5;
            uint8_t pen = (uint8_t)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_RECT_FILL handle=0x%08x, (%d,%d)-(%d,%d), pen=%d\n",
                    d1, x1, y1, x2, y2, pen);

            /* Create filled rectangle in a temporary buffer and blit it */
            int width = x2 - x1 + 1;
            int height = y2 - y1 + 1;
            if (width > 0 && height > 0)
            {
                uint8_t *pixels = malloc(width * height);
                if (pixels)
                {
                    memset(pixels, pen, width * height);
                    display_update_chunky(display, x1, y1, width, height, pixels, width);
                    free(pixels);
                }
            }
            break;
        }

        case EMU_CALL_GFX_DRAW_LINE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x1 = (int)d2;
            int y1 = (int)d3;
            int x2 = (int)d4;
            int y2 = (int)d5;
            uint8_t pen = (uint8_t)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_DRAW_LINE handle=0x%08x, (%d,%d)-(%d,%d), pen=%d\n",
                    d1, x1, y1, x2, y2, pen);

            /* Bresenham's line algorithm */
            int dx = abs(x2 - x1);
            int dy = -abs(y2 - y1);
            int sx = x1 < x2 ? 1 : -1;
            int sy = y1 < y2 ? 1 : -1;
            int err = dx + dy;

            while (1)
            {
                display_update_chunky(display, x1, y1, 1, 1, &pen, 1);
                if (x1 == x2 && y1 == y2)
                    break;
                int e2 = 2 * err;
                if (e2 >= dy)
                {
                    err += dy;
                    x1 += sx;
                }
                if (e2 <= dx)
                {
                    err += dx;
                    y1 += sy;
                }
            }
            break;
        }

        case EMU_CALL_GFX_UPDATE_PLANAR:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            uint32_t d7 = m68k_get_reg(NULL, M68K_REG_D7);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            int width = (int)d4;
            int height = (int)d5;
            int bytes_per_row = (int)d6;
            int depth = (int)d7;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_UPDATE_PLANAR handle=0x%08x, (%d,%d) %dx%d, bpr=%d, depth=%d, planes=0x%08x\n",
                    d1, x, y, width, height, bytes_per_row, depth, a0);

            if (a0 && depth > 0 && depth <= 8 && width > 0 && height > 0)
            {
                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {NULL};
                uint8_t *plane_data[8] = {NULL};
                int valid_planes = 0;

                for (int i = 0; i < depth; i++)
                {
                    uint32_t plane_ptr = m68k_read_memory_32(a0 + i * 4);
                    if (plane_ptr)
                    {
                        /* Map m68k plane pointer to host memory */
                        plane_data[i] = malloc(bytes_per_row * height);
                        if (plane_data[i])
                        {
                            /* Copy plane data from m68k memory */
                            for (int row = 0; row < height; row++)
                            {
                                for (int col = 0; col < bytes_per_row; col++)
                                {
                                    plane_data[i][row * bytes_per_row + col] =
                                        m68k_read_memory_8(plane_ptr + row * bytes_per_row + col);
                                }
                            }
                            planes[i] = plane_data[i];
                            valid_planes++;
                        }
                    }
                }

                /* Update display with planar data */
                if (valid_planes > 0)
                {
                    display_update_planar(display, x, y, width, height, planes, bytes_per_row, depth);
                }

                /* Free temporary plane buffers */
                for (int i = 0; i < depth; i++)
                {
                    free(plane_data[i]);
                }
            }
            break;
        }

        case EMU_CALL_GFX_UPDATE_CHUNKY:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t d5 = m68k_get_reg(NULL, M68K_REG_D5);
            uint32_t d6 = m68k_get_reg(NULL, M68K_REG_D6);
            uint32_t a0 = m68k_get_reg(NULL, M68K_REG_A0);
            display_t *display = (display_t *)(uintptr_t)d1;
            int x = (int)d2;
            int y = (int)d3;
            int width = (int)d4;
            int height = (int)d5;
            int pitch = (int)d6;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_UPDATE_CHUNKY handle=0x%08x, (%d,%d) %dx%d, pitch=%d, pixels=0x%08x\n",
                    d1, x, y, width, height, pitch, a0);

            if (a0 && width > 0 && height > 0)
            {
                /* Read pixel data from m68k memory */
                uint8_t *pixels = malloc(width * height);
                if (pixels)
                {
                    for (int row = 0; row < height; row++)
                    {
                        for (int col = 0; col < width; col++)
                        {
                            pixels[row * width + col] = m68k_read_memory_8(a0 + row * pitch + col);
                        }
                    }
                    display_update_chunky(display, x, y, width, height, pixels, width);
                    free(pixels);
                }
            }
            break;
        }

        case EMU_CALL_GFX_BLT_BITMAP:
        {
            /*
             * Phase 112: native host-C implementation of graphics.library
             * BltBitMap(). Argument struct (28 bytes) lives in m68k memory,
             * pointer in D1. Returns planesAffected mask in D0.
             *
             * Layout (matches struct LxaBltBitMapArgs in lxa_graphics.c):
             *   +0   ULONG  srcBM (m68k addr of struct BitMap)
             *   +4   WORD   xSrc
             *   +6   WORD   ySrc
             *   +8   ULONG  destBM
             *   +12  WORD   xDest
             *   +14  WORD   yDest
             *   +16  WORD   xSize
             *   +18  WORD   ySize
             *   +20  UBYTE  minterm
             *   +21  UBYTE  planeMask
             *   +22  UWORD  pixelMaskBpr
             *   +24  ULONG  pixelMask (m68k addr or 0)
             *
             * struct BitMap layout (offsets within m68k memory):
             *   +0   UWORD  BytesPerRow
             *   +2   UWORD  Rows
             *   +4   UBYTE  Flags
             *   +5   UBYTE  Depth
             *   +6   UWORD  pad
             *   +8   PLANEPTR Planes[8]   (each is m68k addr, 32 bits)
             */
            uint32_t args_ptr = m68k_get_reg(NULL, M68K_REG_D1);
            if (!args_ptr) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Sign-extend 16-bit fields */
            #define READ_W(addr) ((int16_t)m68k_read_memory_16(addr))

            uint32_t srcBM         = m68k_read_memory_32(args_ptr + 0);
            int      xSrc          = READ_W(args_ptr + 4);
            int      ySrc          = READ_W(args_ptr + 6);
            uint32_t destBM        = m68k_read_memory_32(args_ptr + 8);
            int      xDest         = READ_W(args_ptr + 12);
            int      yDest         = READ_W(args_ptr + 14);
            int      xSize         = READ_W(args_ptr + 16);
            int      ySize         = READ_W(args_ptr + 18);
            uint8_t  minterm       = m68k_read_memory_8(args_ptr + 20);
            uint8_t  planeMask     = m68k_read_memory_8(args_ptr + 21);
            uint16_t pixelMaskBpr  = m68k_read_memory_16(args_ptr + 22);
            uint32_t pixelMaskAddr = m68k_read_memory_32(args_ptr + 24);

            #undef READ_W

            if (!srcBM || !destBM || xSize <= 0 || ySize <= 0 || planeMask == 0) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Read BitMap headers */
            uint16_t srcBpr   = m68k_read_memory_16(srcBM + 0);
            uint16_t srcRows  = m68k_read_memory_16(srcBM + 2);
            uint8_t  srcDepth = m68k_read_memory_8 (srcBM + 5);
            uint16_t dstBpr   = m68k_read_memory_16(destBM + 0);
            uint16_t dstRows  = m68k_read_memory_16(destBM + 2);
            uint8_t  dstDepth = m68k_read_memory_8 (destBM + 5);

            int srcWidth  = srcBpr * 8;
            int dstWidth  = dstBpr * 8;
            int srcHeight = srcRows;
            int dstHeight = dstRows;

            /* Clip - same algorithm as m68k BltBitMapCore */
            int sx = xSrc, sy = ySrc, dx = xDest, dy = yDest;
            int aw = xSize, ah = ySize;

            if (sx < 0) { dx -= sx; aw += sx; sx = 0; }
            if (sy < 0) { dy -= sy; ah += sy; sy = 0; }
            if (dx < 0) { sx -= dx; aw += dx; dx = 0; }
            if (dy < 0) { sy -= dy; ah += dy; dy = 0; }

            if (sx + aw > srcWidth)  aw = srcWidth  - sx;
            if (sy + ah > srcHeight) ah = srcHeight - sy;
            if (dx + aw > dstWidth)  aw = dstWidth  - dx;
            if (dy + ah > dstHeight) ah = dstHeight - dy;

            if (aw <= 0 || ah <= 0) {
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            int depth = srcDepth < dstDepth ? srcDepth : dstDepth;

            /*
             * Read source plane addresses, allocate temp src plane row
             * buffers (we need stable host-side bytes for arbitrary x-shift
             * blits). We read src row data once per row into a small
             * stack buffer to avoid per-bit m68k_read_memory_8 calls in
             * the hot loop.
             *
             * For each plane:
             *   1. If planeMask bit clear -> skip
             *   2. If destPlane is NULL or 0xFFFFFFFF -> skip
             *   3. Determine row direction and column direction (overlap)
             *   4. For each row:
             *        - Read src row bytes [src_byte_start..src_byte_end] from m68k
             *        - Read dst row bytes [dst_byte_start..dst_byte_end] from m68k
             *          (only the bytes we'll modify)
             *        - Apply minterm + pixelMask + plane mask using bit-shifted src
             *        - Write modified dst row bytes back to m68k
             *
             * Common-case fast path: minterm==0xC0, no pixelMask, source
             * not equal to destination plane (no overlap), same x-byte
             * alignment -> straight memcpy after reading src and writing
             * dst directly. We don't bother with that explicit fast path
             * here; per-byte processing in C is already 100x faster than
             * m68k-emulated per-bit, and that's enough.
             */
            uint32_t srcPlanes[8] = {0};
            uint32_t dstPlanes[8] = {0};
            for (int p = 0; p < depth; p++) {
                srcPlanes[p] = m68k_read_memory_32(srcBM  + 8 + p * 4);
                dstPlanes[p] = m68k_read_memory_32(destBM + 8 + p * 4);
            }

            int planesAffected = 0;
            uint8_t srcRow[1024];   /* enough for 8192 px wide */
            uint8_t dstRow[1024];
            uint8_t pmRow[1024];

            for (int p = 0; p < depth; p++) {
                if (!(planeMask & (1u << p)))
                    continue;
                uint32_t srcPlane = srcPlanes[p];
                uint32_t dstPlane = dstPlanes[p];
                if (!dstPlane || dstPlane == 0xFFFFFFFFu)
                    continue;

                planesAffected++;

                /*
                 * Determine if src/dst overlap on this plane.
                 * If srcPlane==dstPlane and the rectangles overlap, we
                 * need to choose row/column scan direction so that we
                 * read each cell before we overwrite it.
                 */
                int overlap = 0;
                if (srcPlane && srcPlane != 0xFFFFFFFFu && srcPlane == dstPlane) {
                    int sx0=sx, sy0=sy, sx1=sx+aw-1, sy1=sy+ah-1;
                    int dx0=dx, dy0=dy, dx1=dx+aw-1, dy1=dy+ah-1;
                    if (!(sx1 < dx0 || dx1 < sx0 || sy1 < dy0 || dy1 < sy0))
                        overlap = 1;
                }

                int row_start, row_end, row_step;
                if (overlap && dy > sy) {
                    row_start = ah - 1; row_end = -1; row_step = -1;
                } else {
                    row_start = 0; row_end = ah; row_step = 1;
                }

                int col_reverse_default = (overlap && dy == sy && dx > sx);

                /* Bytes per row on the source - we read enough to cover
                 * sx..sx+aw-1 PLUS one extra for the shift carry. */
                int src_byte_start = sx >> 3;
                int src_byte_end_excl = ((sx + aw + 7) >> 3); /* exclusive */
                int src_byte_count = src_byte_end_excl - src_byte_start;
                int dst_byte_start = dx >> 3;
                int dst_byte_end_excl = ((dx + aw + 7) >> 3);
                int dst_byte_count = dst_byte_end_excl - dst_byte_start;

                if (src_byte_count > (int)sizeof(srcRow)) src_byte_count = sizeof(srcRow);
                if (dst_byte_count > (int)sizeof(dstRow)) dst_byte_count = sizeof(dstRow);

                int src_shift = sx & 7;     /* leftmost bit of source rect */
                int dst_shift = dx & 7;     /* leftmost bit of dest rect */

                /*
                 * For each output bit (col in 0..aw-1):
                 *   src bit  = bit at (sx+col, srcY) in source plane
                 *   dst bit  = bit at (dx+col, destY) in dest plane
                 *   new dst bit = ApplyMinterm(src bit, dst bit, minterm)
                 *
                 * We process this byte-by-output-byte, building a window
                 * over the source bytes and shifting on the fly. This is
                 * O(aw/8) per row not O(aw) per row.
                 *
                 * To keep the code straightforward and bug-free for this
                 * first cut, we process bit-by-bit within each row but
                 * read the source/dest rows ONCE up front. That gives us
                 * a ~50x speedup over the m68k per-bit version because
                 * each m68k_read_memory_8/write_memory_8 is much cheaper
                 * here than a full m68k cpu cycle round-trip in the
                 * emulator. Future optimization: unroll into byte-aligned
                 * fast paths for minterm 0xC0.
                 */
                for (int row = row_start; row != row_end; row += row_step) {
                    int srcY = sy + row;
                    int destY = dy + row;
                    int col_reverse = col_reverse_default;

                    /* Read src row bytes. Special-case NULL source plane
                     * (treat as all-0s) and 0xFFFFFFFF source plane (treat
                     * as all-1s) - these encode the GadTools/Image
                     * "planeonoff" feature where a plane is logically
                     * constant rather than backed by memory. */
                    if (!srcPlane) {
                        for (int b = 0; b < src_byte_count; b++) srcRow[b] = 0x00;
                    } else if (srcPlane == 0xFFFFFFFFu) {
                        for (int b = 0; b < src_byte_count; b++) srcRow[b] = 0xFF;
                    } else {
                        uint32_t src_row_addr = srcPlane + (uint32_t)srcY * srcBpr + src_byte_start;
                        for (int b = 0; b < src_byte_count; b++)
                            srcRow[b] = m68k_read_memory_8(src_row_addr + b);
                    }

                    /* Read dst row bytes (we need them for minterm cases
                     * that depend on dest, and for preserving bits we
                     * don't touch within partial-byte edges) */
                    uint32_t dst_row_addr = dstPlane + (uint32_t)destY * dstBpr + dst_byte_start;
                    for (int b = 0; b < dst_byte_count; b++)
                        dstRow[b] = m68k_read_memory_8(dst_row_addr + b);

                    /* Read pixel mask row if present */
                    if (pixelMaskAddr) {
                        int pm_byte_start = sx >> 3;
                        int pm_byte_count = src_byte_count;
                        uint32_t pm_addr = pixelMaskAddr + (uint32_t)srcY * pixelMaskBpr + pm_byte_start;
                        for (int b = 0; b < pm_byte_count && b < (int)sizeof(pmRow); b++)
                            pmRow[b] = m68k_read_memory_8(pm_addr + b);
                    }

                    /* Bit loop */
                    int col_first, col_last, col_step_dir;
                    if (col_reverse) {
                        col_first = aw - 1; col_last = -1; col_step_dir = -1;
                    } else {
                        col_first = 0; col_last = aw; col_step_dir = 1;
                    }

                    for (int col = col_first; col != col_last; col += col_step_dir) {
                        int s_bit_idx = src_shift + col;        /* in srcRow buffer */
                        int d_bit_idx = dst_shift + col;        /* in dstRow buffer */
                        int s_byte = s_bit_idx >> 3;
                        int s_bit  = 7 - (s_bit_idx & 7);
                        int d_byte = d_bit_idx >> 3;
                        int d_bit  = 7 - (d_bit_idx & 7);

                        /* Pixel mask check */
                        if (pixelMaskAddr) {
                            int pm_idx = s_bit_idx;
                            int pm_byte = pm_idx >> 3;
                            int pm_b    = 7 - (pm_idx & 7);
                            if (!((pmRow[pm_byte] >> pm_b) & 1))
                                continue;
                        }

                        uint8_t srcBit = (srcRow[s_byte] >> s_bit) & 1;
                        uint8_t dstBit = (dstRow[d_byte] >> d_bit) & 1;
                        uint8_t newBit = 0;
                        if ((minterm & 0x10) && !srcBit && !dstBit) newBit = 1;
                        if ((minterm & 0x20) && !srcBit &&  dstBit) newBit = 1;
                        if ((minterm & 0x40) &&  srcBit && !dstBit) newBit = 1;
                        if ((minterm & 0x80) &&  srcBit &&  dstBit) newBit = 1;

                        if (newBit)
                            dstRow[d_byte] |=  (uint8_t)(1 << d_bit);
                        else
                            dstRow[d_byte] &= (uint8_t)~(1 << d_bit);
                    }

                    /* Write modified dst row back */
                    for (int b = 0; b < dst_byte_count; b++)
                        m68k_write_memory_8(dst_row_addr + b, dstRow[b]);
                }
            }

            m68k_set_reg(M68K_REG_D0, (uint32_t)planesAffected);
            break;
        }

        case EMU_CALL_GFX_GET_SIZE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *display = (display_t *)(uintptr_t)d1;
            int width, height, depth;

            display_get_size(display, &width, &height, &depth);

            /* Pack into D0: high word = width, low word = height; D1 = depth */
            m68k_set_reg(M68K_REG_D0, ((uint32_t)width << 16) | (uint32_t)height);
            m68k_set_reg(M68K_REG_D1, (uint32_t)depth);
            break;
        }

        case EMU_CALL_GFX_AVAILABLE:
        {
            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_GFX_AVAILABLE\n");
            m68k_set_reg(M68K_REG_D0, display_available() ? 1 : 0);
            break;
        }

        case EMU_CALL_GFX_POLL_EVENTS:
        {
            bool quit = display_poll_events();
            m68k_set_reg(M68K_REG_D0, quit ? 1 : 0);
            if (quit)
            {
                DPRINTF(LOG_INFO, "lxa: display quit requested\n");
                m68k_end_timeslice();
                g_running = FALSE;
            }
            break;
        }

        /*
         * Intuition Library emucalls (3000-3999)
         * Phase 13.5: Screen Management
         */

        case EMU_CALL_INT_OPEN_SCREEN:
        {
            /* d1: (width << 16) | height, d2: depth, d3: title_ptr */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t width = (d1 >> 16) & 0xFFFF;
            uint32_t height = d1 & 0xFFFF;
            uint32_t depth = d2;
            uint32_t title_ptr = d3;

            char title[128] = "LXA Screen";
            if (title_ptr != 0)
            {
                /* Read title string from m68k memory */
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_OPEN_SCREEN %dx%dx%d '%s'\n",
                    width, height, depth, title);

            /* Initialize display subsystem if not already done */
            display_init();

            /* Open the display window */
            display_t *disp = display_open(width, height, depth, title);

            /* Return display handle (pointer cast to uint32_t) */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)disp);
            break;
        }

        case EMU_CALL_INT_CLOSE_SCREEN:
        {
            /* d1: display_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_t *disp = (display_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_CLOSE_SCREEN handle=0x%08x\n", d1);

            if (disp)
            {
                display_close(disp);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_REFRESH_SCREEN:
        {
            /* d1: display_handle, d2: planes_ptr, d3: (bpr << 16) | depth */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *disp = (display_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_REFRESH_SCREEN handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (disp && planes_ptr)
            {
                int w, h, d;
                display_get_size(disp, &w, &h, &d);

                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {0};
                for (uint32_t i = 0; i < depth && i < 8; i++)
                {
                    uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
                    if (plane_addr)
                    {
                        planes[i] = (const uint8_t *)&g_ram[plane_addr];
                    }
                }

                /* Update display from planar data */
                display_update_planar(disp, 0, 0, w, h, planes, bpr, depth);

                /* Refresh display */
                display_refresh(disp);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_SET_SCREEN_BITMAP:
        {
            /* d1: display_handle, d2: planes_ptr (address of BitMap.Planes[] array)
             * d3: (bpr << 16) | depth
             * 
             * This tells the host where the Amiga screen's bitmap lives so that
             * display_refresh_all() can automatically convert planar data to the
             * SDL display at VBlank time.
             */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_t *disp = (display_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: op_illg(): EMU_CALL_INT_SET_SCREEN_BITMAP handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (disp)
            {
                display_set_amiga_bitmap(disp, planes_ptr, (bpr << 16) | depth);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        /*
         * Input handling emucalls (Phase 14)
         */
        
        case EMU_CALL_INT_POLL_INPUT:
        {
            /* Poll for next input event
             * Returns event type in D0:
             *   0 = no event
             *   1 = mouse button
             *   2 = mouse move
             *   3 = key
             *   4 = close window
             *   5 = quit
             */
            
            /* First, poll SDL for new events (this populates the event queue) */
            display_poll_events();
            
            display_event_t event;
            bool got_event = display_get_event(&event);
            if (got_event)
            {
                /* Store event data in static vars for subsequent GET calls */
                g_last_event = event;
                m68k_set_reg(M68K_REG_D0, (uint32_t)event.type);
                DPRINTF(LOG_DEBUG, "lxa: POLL_INPUT: got event type=%d btn=0x%02x pos=(%d,%d)\n", event.type, event.button_code, event.mouse_x, event.mouse_y);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
            }
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_POS:
        {
            /* Returns (x << 16) | y from the last polled event.
             * We use g_last_event.mouse_x/y rather than display_get_mouse_pos()
             * to ensure the position corresponds to the event that was dequeued,
             * not the current mouse position (which may have changed). */
            int x = g_last_event.mouse_x;
            int y = g_last_event.mouse_y;
            m68k_set_reg(M68K_REG_D0, ((uint32_t)x << 16) | ((uint32_t)y & 0xFFFF));
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_BTN:
        {
            /* Returns button_code | (qualifier << 8) from last event.
             * The ROM extracts: code = result & 0xFF, qualifier = (result >> 8) & 0xFFFF.
             * This matches the packing used by EMU_CALL_INT_GET_KEY. */
            uint32_t result = ((uint32_t)g_last_event.qualifier << 8) |
                             ((uint32_t)g_last_event.button_code & 0xFF);
            m68k_set_reg(M68K_REG_D0, result);
            break;
        }

        case EMU_CALL_INT_GET_KEY:
        {
            /* Returns rawkey | (qualifier << 16) from last event */
            uint32_t result = ((uint32_t)g_last_event.qualifier << 16) | 
                             ((uint32_t)g_last_event.rawkey & 0xFFFF);
            m68k_set_reg(M68K_REG_D0, result);
            break;
        }

        case EMU_CALL_INT_GET_EVENT_WIN:
        {
            /* Returns display handle for window that received the event */
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)g_last_event.window);
            break;
        }

        /*
         * Rootless windowing emucalls (Phase 15)
         */

        case EMU_CALL_INT_OPEN_WINDOW:
        {
            /* d1: screen_handle, d2: (x << 16) | y, d3: (w << 16) | h, d4: title_ptr */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t d4 = m68k_get_reg(NULL, M68K_REG_D4);
            display_t *screen = (display_t *)(uintptr_t)d1;
            int x = (int16_t)((d2 >> 16) & 0xFFFF);
            int y = (int16_t)(d2 & 0xFFFF);
            int w = (int16_t)((d3 >> 16) & 0xFFFF);
            int h = (int16_t)(d3 & 0xFFFF);
            uint32_t title_ptr = d4;
            char title[128] = "LXA Window";
            if (title_ptr != 0)
            {
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_OPEN_WINDOW x=%d, y=%d, w=%d, h=%d '%s'\n",
                    x, y, w, h, title);

            /* Reject negative or zero window dimensions that result from
             * signed WORD overflow on the ROM side. */
            if (w <= 0 || h <= 0)
            {
                LPRINTF(LOG_WARNING, "lxa: EMU_CALL_INT_OPEN_WINDOW rejected invalid dimensions %dx%d\n", w, h);
                m68k_set_reg(M68K_REG_D0, 0);
                break;
            }

            /* Get depth from screen if available */
            int depth = 2;  /* Default */
            if (screen)
            {
                int sw, sh, sd;
                display_get_size(screen, &sw, &sh, &sd);
                depth = sd;
            }

            display_window_t *win = display_window_open(screen, x, y, w, h, depth, title);
            m68k_set_reg(M68K_REG_D0, (uint32_t)(uintptr_t)win);
            break;
        }

        case EMU_CALL_INT_CLOSE_WINDOW:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_CLOSE_WINDOW handle=0x%08x\n", d1);

            if (win)
            {
                display_window_close(win);
            }
            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_MOVE_WINDOW:
        {
            /* d1: window_handle, d2: (x << 16) | y */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            int x = (int16_t)((d2 >> 16) & 0xFFFF);
            int y = (int16_t)(d2 & 0xFFFF);

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_MOVE_WINDOW handle=0x%08x, x=%d, y=%d\n",
                    d1, x, y);

            bool success = display_window_move(win, x, y);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_SIZE_WINDOW:
        {
            /* d1: window_handle, d2: width, d3: height
             * Called from _intuition_SizeWindow via emucall3(). */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            int w = (int)d2;
            int h = (int)d3;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_SIZE_WINDOW handle=0x%08x, w=%d, h=%d\n",
                    d1, w, h);

            bool success = display_window_size(win, w, h);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_WINDOW_TOFRONT:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_WINDOW_TOFRONT handle=0x%08x\n", d1);

            bool success = display_window_to_front(win);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_WINDOW_TOBACK:
        {
            /* d1: window_handle */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_WINDOW_TOBACK handle=0x%08x\n", d1);

            bool success = display_window_to_back(win);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_SET_TITLE:
        {
            /* d1: window_handle, d2: title_ptr */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            uint32_t title_ptr = d2;

            char title[128] = "";
            if (title_ptr != 0)
            {
                int i;
                for (i = 0; i < 127; i++)
                {
                    char c = (char)m68k_read_memory_8(title_ptr + i);
                    if (c == 0)
                        break;
                    title[i] = c;
                }
                title[i] = 0;
            }

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_SET_TITLE handle=0x%08x, title='%s'\n",
                    d1, title);

            bool success = display_window_set_title(win, title);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_REFRESH_WINDOW:
        {
            /* d1: window_handle, d2: planes_ptr, d3: (bpr << 16) | depth */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            uint32_t planes_ptr = d2;
            uint32_t bpr = (d3 >> 16) & 0xFFFF;
            uint32_t depth = d3 & 0xFFFF;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_REFRESH_WINDOW handle=0x%08x, planes=0x%08x, bpr=%d, depth=%d\n",
                    d1, planes_ptr, bpr, depth);

            if (win && planes_ptr)
            {
                /* Read plane pointers from m68k memory */
                const uint8_t *planes[8] = {0};
                for (uint32_t i = 0; i < depth && i < 8; i++)
                {
                    uint32_t plane_addr = m68k_read_memory_32(planes_ptr + i * 4);
                    if (plane_addr)
                    {
                        planes[i] = (const uint8_t *)&g_ram[plane_addr];
                    }
                }

                /* Get window size - we need to figure out the dimensions somehow */
                /* For now, use bpr * 8 as width estimate and read from window */
                int w = bpr * 8;
                int h = 200;  /* Default, ideally this would come from the window struct */

                display_window_update_planar(win, 0, 0, w, h, planes, bpr, depth);
                display_window_refresh(win);
            }

            m68k_set_reg(M68K_REG_D0, 1);  /* Success */
            break;
        }

        case EMU_CALL_INT_GET_ROOTLESS:
        {
            /* Returns true if rootless mode is enabled */
            bool rootless = display_get_rootless_mode();
            m68k_set_reg(M68K_REG_D0, rootless ? 1 : 0);
            break;
        }

        case EMU_CALL_INT_ATTACH_WINDOW:
        {
            /* d1: window_handle, d2: emulated Window* */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;

            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_INT_ATTACH_WINDOW handle=0x%08x, window=0x%08x\n",
                    d1, d2);

            m68k_set_reg(M68K_REG_D0,
                         display_window_attach_amiga_window(win, d2) ? 1 : 0);
            break;
        }

        /*
         * Timer device emucalls (Phase 45)
         * These manage the host-side timer queue for async TR_ADDREQUEST.
         */
        case EMU_CALL_TIMER_ADD:
        {
            /* Add a timer request to the host-side queue.
             * d1 = ioreq pointer (68k)
             * d2 = delay seconds
             * d3 = delay microseconds
             * Returns: d0 = 1 on success, 0 on failure
             */
            uint32_t ioreq = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t secs = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t micros = m68k_get_reg(NULL, M68K_REG_D3);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_ADD ioreq=0x%08x delay=%u.%06u\n",
                    ioreq, secs, micros);
            
            bool success = _timer_add_request(ioreq, secs, micros);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }
        
        case EMU_CALL_TIMER_REMOVE:
        {
            /* Remove a timer request from the queue (for AbortIO).
             * d1 = ioreq pointer (68k)
             * Returns: d0 = 1 if found and removed, 0 if not found
             */
            uint32_t ioreq = m68k_get_reg(NULL, M68K_REG_D1);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_REMOVE ioreq=0x%08x\n", ioreq);
            
            bool success = _timer_remove_request(ioreq);
            m68k_set_reg(M68K_REG_D0, success ? 1 : 0);
            break;
        }
        
        case EMU_CALL_TIMER_CHECK:
        {
            /* Check for expired timers and mark them.
             * This is called from the m68k side (e.g., in VBlank handler).
             * Returns: d0 = number of expired timers marked
             */
            int count = _timer_check_expired();
            m68k_set_reg(M68K_REG_D0, (uint32_t)count);
            break;
        }

        case EMU_CALL_TIMER_GET_EXPIRED:
        {
            /* Get the next expired timer IORequest.
             * Returns: d0 = ioreq pointer (m68k), or 0 if none available
             */
            uint32_t ioreq = _timer_get_expired();
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_TIMER_GET_EXPIRED -> 0x%08x\n", ioreq);
            m68k_set_reg(M68K_REG_D0, ioreq);
            break;
        }

        case EMU_CALL_AUDIO_PLAY:
        {
            uint32_t packed = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t data_ptr = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t length = m68k_get_reg(NULL, M68K_REG_D3);
            uint32_t period = m68k_get_reg(NULL, M68K_REG_D4);
            uint32_t volume = m68k_get_reg(NULL, M68K_REG_D5);

            _audio_queue_fragment(packed, data_ptr, length, period, volume);
            m68k_set_reg(M68K_REG_D0, 1);
            break;
        }

        /*
         * Console device emucalls
         */
        case EMU_CALL_CON_READ:
        {
            /* Read from console input.
             * d1 = buffer address (68k memory)
             * d2 = max bytes to read
             * Returns: d0 = bytes read, or -1 if no data available yet
             */
            uint32_t buf68k = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t maxlen = m68k_get_reg(NULL, M68K_REG_D2);
            
            DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_CON_READ buf=0x%08x maxlen=%d\n", buf68k, maxlen);
            
            /* For now, read from stdin (non-blocking) */
            /* Check if input is available using select() */
            fd_set readfds;
            struct timeval tv = {0, 0};  /* No timeout - non-blocking */
            
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            
            int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
            
            if (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds))
            {
                /* Input is available */
                char buf[256];
                int to_read = (maxlen > sizeof(buf)) ? sizeof(buf) : maxlen;
                ssize_t n = read(STDIN_FILENO, buf, to_read);
                
                if (n > 0)
                {
                    /* Copy to 68k memory, converting LF to CR for Amiga compatibility */
                    for (int i = 0; i < n; i++)
                    {
                        char c = buf[i];
                        if (c == '\n')
                            c = '\r';  /* Convert LF to CR */
                        m68k_write_memory_8(buf68k + i, c);
                    }
                    /* Log what was read */
                    DPRINTF(LOG_DEBUG, "lxa: EMU_CALL_CON_READ -> %zd bytes: 0x%02x '%c'\n", 
                            n, (unsigned char)buf[0], 
                            (buf[0] >= 32 && buf[0] < 127) ? buf[0] : '.');
                    m68k_set_reg(M68K_REG_D0, (uint32_t)n);
                }
                else if (n == 0)
                {
                    /* EOF */
                    m68k_set_reg(M68K_REG_D0, 0);
                }
                else
                {
                    /* Error */
                    m68k_set_reg(M68K_REG_D0, (uint32_t)-1);
                }
            }
            else
            {
                /* No input available */
                m68k_set_reg(M68K_REG_D0, (uint32_t)-1);
            }
            break;
        }

        case EMU_CALL_CON_INPUT_READY:
        {
            /* Check if console input is ready (non-blocking).
             * Returns: d0 = 1 if input ready, 0 if not
             */
            fd_set readfds;
            struct timeval tv = {0, 0};
            
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            
            int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);
            
            m68k_set_reg(M68K_REG_D0, (ready > 0 && FD_ISSET(STDIN_FILENO, &readfds)) ? 1 : 0);
            break;
        }

        /*
         * Test Infrastructure emucalls (4100-4199)
         * These provide automated testing capabilities for UI and input handling.
         */
        case EMU_CALL_TEST_INJECT_KEY:
        {
            /* Inject a keyboard event into the event queue.
             * Input:  d1 = rawkey code
             *         d2 = qualifier bits
             *         d3 = key down (1) or up (0)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t rawkey = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t qualifier = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t down = m68k_get_reg(NULL, M68K_REG_D3);
            
            bool result = display_inject_key((int)rawkey, (int)qualifier, down != 0);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_INJECT_STRING:
        {
            /* Inject a string as a sequence of key events.
             * Input:  a0 = pointer to null-terminated string
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t str_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            char str[1024];
            
            /* Copy string from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(str) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(str_ptr + i);
                if (c == '\0')
                    break;
                str[i] = c;
            }
            str[i] = '\0';
            
            bool result = display_inject_string(str);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_INJECT_MOUSE:
        {
            /* Inject a mouse event into the event queue.
             * Input:  d1 = (x << 16) | y
             *         d2 = button state (bit 0=left, bit 1=right, bit 2=middle)
             *         d3 = event type (DISPLAY_EVENT_MOUSEMOVE=2, DISPLAY_EVENT_MOUSEBUTTON=3)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t pos = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t buttons = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t event_type = m68k_get_reg(NULL, M68K_REG_D3);
            
            int x = (int)(pos >> 16);
            int y = (int)(pos & 0xFFFF);
            
            bool result = display_inject_mouse(x, y, (int)buttons, (display_event_type_t)event_type);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_CAPTURE_SCREEN:
        {
            /* Capture the display to a file.
             * Input:  a0 = pointer to filename string
             *         d1 = display handle (0 for default)
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            /* uint32_t display_handle = m68k_get_reg(NULL, M68K_REG_D1); */
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            /* Use default display for now */
            bool result = display_capture_screen(NULL, filename);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_CAPTURE_WINDOW:
        {
            /* Capture a window to a file.
             * Input:  a0 = pointer to filename string
             *         d1 = window handle
             * Output: d0 = 1 on success, 0 on failure
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            uint32_t window_handle = m68k_get_reg(NULL, M68K_REG_D1);
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            bool result = display_capture_window((display_window_t *)(uintptr_t)window_handle, filename);
            m68k_set_reg(M68K_REG_D0, result ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_COMPARE_SCREEN:
        {
            /* Compare screen to a reference image.
             * Input:  a0 = pointer to reference filename
             * Output: d0 = similarity percentage (0-100), or -1 on error
             */
            uint32_t filename_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            char filename[256];
            
            /* Copy filename from 68k memory */
            int i;
            for (i = 0; i < (int)sizeof(filename) - 1; i++)
            {
                char c = (char)m68k_read_memory_8(filename_ptr + i);
                if (c == '\0')
                    break;
                filename[i] = c;
            }
            filename[i] = '\0';
            
            int similarity = display_compare_to_reference(filename);
            m68k_set_reg(M68K_REG_D0, (uint32_t)similarity);
            break;
        }

        /* Phase 39b: Validation emucalls */
        case 4113:  /* EMU_CALL_TEST_GET_SCREEN_DIMS */
        {
            /* Get active screen dimensions.
             * Output: d0 = (depth << 16) | 1 on success, 0 on failure
             *         d1 = (width << 16) | height
             */
            int width, height, depth;
            if (display_get_active_dimensions(&width, &height, &depth))
            {
                m68k_set_reg(M68K_REG_D0, ((uint32_t)depth << 16) | 1);
                m68k_set_reg(M68K_REG_D1, ((uint32_t)width << 16) | (uint16_t)height);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
                m68k_set_reg(M68K_REG_D1, 0);
            }
            break;
        }

        case 4114:  /* EMU_CALL_TEST_GET_CONTENT */
        {
            /* Get number of non-background pixels on active screen.
             * Output: d0 = pixel count, or -1 if no screen
             */
            int content = display_get_content_pixels();
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case 4115:  /* EMU_CALL_TEST_GET_REGION */
        {
            /* Get non-background pixels in a screen region.
             * Input:  d1 = (x << 16) | y
             *         d2 = (width << 16) | height
             * Output: d0 = pixel count, or -1 on error
             */
            uint32_t pos = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t dims = m68k_get_reg(NULL, M68K_REG_D2);
            int x = (int16_t)((pos >> 16) & 0xFFFF);
            int y = (int16_t)(pos & 0xFFFF);
            int width = (int16_t)((dims >> 16) & 0xFFFF);
            int height = (int16_t)(dims & 0xFFFF);
            
            int content = display_get_region_content(x, y, width, height);
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case 4116:  /* EMU_CALL_TEST_GET_WIN_COUNT */
        {
            /* Get number of open windows.
             * Output: d0 = window count
             */
            int count = display_get_window_count();
            m68k_set_reg(M68K_REG_D0, (uint32_t)count);
            break;
        }

        case 4117:  /* EMU_CALL_TEST_GET_WIN_DIMS */
        {
            /* Get window dimensions by index.
             * Input:  d1 = window index
             * Output: d0 = 1 on success, 0 on failure
             *         d1 = (width << 16) | height
             */
            int index = (int)m68k_get_reg(NULL, M68K_REG_D1);
            int width, height;
            
            if (display_get_window_dimensions(index, &width, &height))
            {
                m68k_set_reg(M68K_REG_D0, 1);
                m68k_set_reg(M68K_REG_D1, ((uint32_t)width << 16) | (uint16_t)height);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
                m68k_set_reg(M68K_REG_D1, 0);
            }
            break;
        }

        case 4118:  /* EMU_CALL_TEST_GET_WIN_CONTENT */
        {
            /* Get non-background pixels in a window.
             * Input:  d1 = window index
             * Output: d0 = pixel count, or -1 on error
             */
            int index = (int)m68k_get_reg(NULL, M68K_REG_D1);
            int content = display_get_window_content(index);
            m68k_set_reg(M68K_REG_D0, (uint32_t)content);
            break;
        }

        case EMU_CALL_TEST_SET_HEADLESS:
        {
            /* Set headless mode (no window rendering).
             * Input:  d1 = enable (1) or disable (0)
             * Output: d0 = previous headless state
             */
            uint32_t enable = m68k_get_reg(NULL, M68K_REG_D1);
            
            bool previous = display_set_headless(enable != 0);
            m68k_set_reg(M68K_REG_D0, previous ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_GET_HEADLESS:
        {
            /* Get current headless mode state.
             * Output: d0 = 1 if headless, 0 if not
             */
            bool is_headless = display_get_headless();
            m68k_set_reg(M68K_REG_D0, is_headless ? 1 : 0);
            break;
        }

        case EMU_CALL_TEST_WAIT_IDLE:
        {
            /* Wait for event queue to be empty.
             * Input:  d1 = timeout in milliseconds (0 = just check, don't wait)
             * Output: d0 = 1 if queue is empty, 0 if timeout
             */
            uint32_t timeout_ms = m68k_get_reg(NULL, M68K_REG_D1);
            
            if (timeout_ms == 0)
            {
                /* Just check, don't wait */
                m68k_set_reg(M68K_REG_D0, display_event_queue_empty() ? 1 : 0);
            }
            else
            {
                /* Wait with timeout using gettimeofday */
                struct timeval start_tv, now_tv;
                gettimeofday(&start_tv, NULL);
                
                while (!display_event_queue_empty())
                {
                    gettimeofday(&now_tv, NULL);
                    uint32_t elapsed_ms = (uint32_t)((now_tv.tv_sec - start_tv.tv_sec) * 1000 +
                                                     (now_tv.tv_usec - start_tv.tv_usec) / 1000);
                    if (elapsed_ms >= timeout_ms)
                    {
                        m68k_set_reg(M68K_REG_D0, 0);  /* Timeout */
                        break;
                    }
                    usleep(1000);  /* 1ms delay to avoid busy-waiting */
                }
                if (display_event_queue_empty())
                {
                    m68k_set_reg(M68K_REG_D0, 1);  /* Success */
                }
            }
            break;
        }

        /*
         * Phase 61: IEEE Double Precision Math emucalls (5000-5011)
         * 
         * These handlers bridge the m68k mathieeedoubbas.library to host native double.
         * Double values are passed as two 32-bit values (d1=hi, d2=lo).
         * For two-operand functions: d1:d2=left, d3:d4=right.
         * Results that are doubles are returned in d0:d1 (hi:lo).
         */
        
        case EMU_CALL_IEEEDP_FIX:
        {
            /* IEEEDPFix: Convert double to integer (truncate toward zero)
             * Input:  d1:d2 = IEEE double (hi:lo)
             * Output: d0 = LONG integer
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            int32_t result = clamp_double_to_long(value);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FIX(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_FLT:
        {
            /* IEEEDPFlt: Convert integer to double
             * Input:  d1 = LONG integer
             * Output: d0:d1 = IEEE double (hi:lo)
             */
            int32_t integer = (int32_t)m68k_get_reg(NULL, M68K_REG_D1);
            
            double value = (double)integer;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FLT(%d) = %f\n", integer, value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_CMP:
        {
            /* IEEEDPCmp: Compare two doubles
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0 = -1 (left < right), 0 (equal), +1 (left > right)
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            int32_t result = compare_double_values(left, right);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_CMP(%f, %f) = %d\n", left, right, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_TST:
        {
            /* IEEEDPTst: Test double against zero
             * Input:  d1:d2 = IEEE double
             * Output: d0 = -1 (negative), 0 (zero), +1 (positive)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            int32_t result = compare_double_values(value, 0.0);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TST(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }
        
        case EMU_CALL_IEEEDP_ABS:
        {
            /* IEEEDPAbs: Absolute value
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = |input|
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = fabs(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ABS -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_NEG:
        {
            /* IEEEDPNeg: Negate
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = -input
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = -value;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_NEG -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ADD:
        {
            /* IEEEDPAdd: Addition
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0:d1 = left + right
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = left + right;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ADD(%f, %f) = %f\n", left, right, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SUB:
        {
            /* IEEEDPSub: Subtraction
             * Input:  d1:d2 = left, d3:d4 = right
             * Output: d0:d1 = left - right
             */
            double left = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double right = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = left - right;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SUB(%f, %f) = %f\n", left, right, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_MUL:
        {
            /* IEEEDPMul: Multiplication
             * Input:  d1:d2 = factor1, d3:d4 = factor2
             * Output: d0:d1 = factor1 * factor2
             */
            double f1 = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double f2 = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = f1 * f2;
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_MUL(%f, %f) = %f\n", f1, f2, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_DIV:
        {
            /* IEEEDPDiv: Division
             * Input:  d1:d2 = dividend, d3:d4 = divisor
             * Output: d0:d1 = dividend / divisor
             */
            double dividend = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double divisor = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = host_safe_double_divide(dividend, divisor);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_DIV(%f, %f) = %f\n", dividend, divisor, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_FLOOR:
        {
            /* IEEEDPFloor: Largest integer not greater than input
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = floor(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = floor(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FLOOR -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_CEIL:
        {
            /* IEEEDPCeil: Smallest integer not less than input
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = ceil(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = ceil(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_CEIL -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }

        /*
         * Phase 77: IEEE Single Precision Basic Math (5100-5111)
         * mathieeesingbas.library handlers
         *
         * Single-precision floats are 32-bit IEEE 754, passed in one register.
         * One-operand: d1=float -> d0=result
         * Two-operand: d1=left, d2=right -> d0=result
         */

        case EMU_CALL_IEEESP_FIX:
        {
            /* IEEESPFix: Convert single float to integer (truncate toward zero)
             * Input:  d1 = IEEE single float
             * Output: d0 = LONG integer
             */
            float value = host_float_from_m68k_register(M68K_REG_D1);
            int32_t result = clamp_float_to_long(value);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FIX(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_FLT:
        {
            /* IEEESPFlt: Convert integer to single float
             * Input:  d1 = LONG integer
             * Output: d0 = IEEE single float
             */
            int32_t integer = (int32_t)m68k_get_reg(NULL, M68K_REG_D1);

            float value = (float)integer;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FLT(%d) = %f\n", integer, value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_CMP:
        {
            /* IEEESPCmp: Compare two single floats
             * Input:  d1 = left, d2 = right
             * Output: d0 = -1 (left < right), 0 (equal), +1 (left > right)
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            int32_t result = compare_float_values(left, right);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_CMP(%f, %f) = %d\n", left, right, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_TST:
        {
            /* IEEESPTst: Test single float against zero
             * Input:  d1 = IEEE single float
             * Output: d0 = -1 (negative), 0 (zero), +1 (positive)
             */
            float value = host_float_from_m68k_register(M68K_REG_D1);
            int32_t result = compare_float_values(value, 0.0f);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_TST(%f) = %d\n", value, result);
            m68k_set_reg(M68K_REG_D0, (uint32_t)result);
            break;
        }

        case EMU_CALL_IEEESP_ABS:
        {
            /* IEEESPAbs: Absolute value
             * Input:  d1 = IEEE single float
             * Output: d0 = |input|
             */
            float value = fabsf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_ABS -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_NEG:
        {
            /* IEEESPNeg: Negate
             * Input:  d1 = IEEE single float
             * Output: d0 = -input
             */
            float value = -host_float_from_m68k_register(M68K_REG_D1);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_NEG -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_ADD:
        {
            /* IEEESPAdd: Addition
             * Input:  d1 = left, d2 = right
             * Output: d0 = left + right
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            float result = left + right;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_ADD(%f, %f) = %f\n", left, right, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_SUB:
        {
            /* IEEESPSub: Subtraction
             * Input:  d1 = left, d2 = right
             * Output: d0 = left - right
             */
            float left = host_float_from_m68k_register(M68K_REG_D1);
            float right = host_float_from_m68k_register(M68K_REG_D2);
            float result = left - right;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_SUB(%f, %f) = %f\n", left, right, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_MUL:
        {
            /* IEEESPMul: Multiplication
             * Input:  d1 = factor1, d2 = factor2
             * Output: d0 = factor1 * factor2
             */
            float f1 = host_float_from_m68k_register(M68K_REG_D1);
            float f2 = host_float_from_m68k_register(M68K_REG_D2);
            float result = f1 * f2;

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_MUL(%f, %f) = %f\n", f1, f2, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_DIV:
        {
            /* IEEESPDiv: Division
             * Input:  d1 = dividend, d2 = divisor
             * Output: d0 = dividend / divisor
             */
            float dividend = host_float_from_m68k_register(M68K_REG_D1);
            float divisor = host_float_from_m68k_register(M68K_REG_D2);
            float result = host_safe_float_divide(dividend, divisor);

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_DIV(%f, %f) = %f\n", dividend, divisor, result);
            host_float_to_m68k_register(result, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_FLOOR:
        {
            /* IEEESPFloor: Largest integer not greater than input
             * Input:  d1 = IEEE single float
             * Output: d0 = floor(input) as IEEE single float
             */
            float value = floorf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_FLOOR -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_IEEESP_CEIL:
        {
            /* IEEESPCeil: Smallest integer not less than input
             * Input:  d1 = IEEE single float
             * Output: d0 = ceil(input) as IEEE single float
             */
            float value = ceilf(host_float_from_m68k_register(M68K_REG_D1));

            DPRINTF(LOG_DEBUG, "lxa: IEEESP_CEIL -> %f\n", value);
            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_FFP_ATAN:
        case EMU_CALL_FFP_SIN:
        case EMU_CALL_FFP_COS:
        case EMU_CALL_FFP_TAN:
        case EMU_CALL_FFP_SINH:
        case EMU_CALL_FFP_COSH:
        case EMU_CALL_FFP_TANH:
        case EMU_CALL_FFP_EXP:
        case EMU_CALL_FFP_LOG:
        case EMU_CALL_FFP_SQRT:
        case EMU_CALL_FFP_ASIN:
        case EMU_CALL_FFP_ACOS:
        case EMU_CALL_FFP_LOG10:
        case EMU_CALL_FFP_FLOOR:
        case EMU_CALL_FFP_CEIL:
        {
            uint32_t raw = m68k_get_reg(NULL, M68K_REG_D1);
            float input = ffp_to_host_float(raw);
            float result = 0.0f;

            switch (d0)
            {
                case EMU_CALL_FFP_ATAN:  result = atanf(input); break;
                case EMU_CALL_FFP_SIN:   result = sinf(input); break;
                case EMU_CALL_FFP_COS:   result = cosf(input); break;
                case EMU_CALL_FFP_TAN:   result = tanf(input); break;
                case EMU_CALL_FFP_SINH:  result = sinhf(input); break;
                case EMU_CALL_FFP_COSH:  result = coshf(input); break;
                case EMU_CALL_FFP_TANH:  result = tanhf(input); break;
                case EMU_CALL_FFP_EXP:   result = expf(input); break;
                case EMU_CALL_FFP_LOG:   result = logf(input); break;
                case EMU_CALL_FFP_SQRT:  result = sqrtf(input); break;
                case EMU_CALL_FFP_ASIN:  result = asinf(input); break;
                case EMU_CALL_FFP_ACOS:  result = acosf(input); break;
                case EMU_CALL_FFP_LOG10: result = log10f(input); break;
                case EMU_CALL_FFP_FLOOR: result = floorf(input); break;
                case EMU_CALL_FFP_CEIL:  result = ceilf(input); break;
            }

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(result));
            break;
        }

        case EMU_CALL_FFP_SINCOS:
        {
            uint32_t raw = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t cos_ptr = m68k_get_reg(NULL, M68K_REG_D2);
            float input = ffp_to_host_float(raw);
            float sin_result = sinf(input);
            float cos_result = cosf(input);

            if (cos_ptr != 0)
            {
                m68k_write_memory_32(cos_ptr, host_float_to_ffp(cos_result));
            }

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(sin_result));
            break;
        }

        case EMU_CALL_FFP_POW:
        {
            float base = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D1));
            float power = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D2));
            float result = powf(base, power);

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(result));
            break;
        }

        case EMU_CALL_FFP_TIEEE:
        {
            float value = ffp_to_host_float(m68k_get_reg(NULL, M68K_REG_D1));

            host_float_to_m68k_register(value, M68K_REG_D0);
            break;
        }

        case EMU_CALL_FFP_FIEEE:
        {
            float value = host_float_from_m68k_register(M68K_REG_D1);

            m68k_set_reg(M68K_REG_D0, host_float_to_ffp(value));
            break;
        }

        /*
         * Phase 61: IEEE Double Precision Transcendental Math (5020-5039)
         * mathieeedoubtrans.library handlers
         */
        
        case EMU_CALL_IEEEDP_ATAN:
        {
            /* IEEEDPAtan: Arc tangent
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = atan(input) in radians
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = atan(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ATAN(%f) -> %f\n", input, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SIN:
        {
            /* IEEEDPSin: Sine
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = sin(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = sin(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SIN -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_COS:
        {
            /* IEEEDPCos: Cosine
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = cos(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = cos(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_COS -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TAN:
        {
            /* IEEEDPTan: Tangent
             * Input:  d1:d2 = IEEE double (radians)
             * Output: d0:d1 = tan(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double result = tan(input);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TAN -> %f\n", result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SINCOS:
        {
            /* IEEEDPSincos: Sine and Cosine
             * Input:  d1:d2 = IEEE double (radians), a0 = pointer to store cos
             * Output: d0:d1 = sin(input), *a0 = cos(input)
             */
            double input = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double sin_result = sin(input);
            double cos_result = cos(input);
            uint32_t cos_ptr = m68k_get_reg(NULL, M68K_REG_A0);
            host_double_to_m68k_memory(cos_ptr, cos_result);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SINCOS -> sin=%f, cos=%f\n", sin_result, cos_result);
            host_double_to_m68k_registers(sin_result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SINH:
        {
            /* IEEEDPSinh: Hyperbolic sine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = sinh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = sinh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SINH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_COSH:
        {
            /* IEEEDPCosh: Hyperbolic cosine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = cosh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = cosh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_COSH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TANH:
        {
            /* IEEEDPTanh: Hyperbolic tangent
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = tanh(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = tanh(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TANH -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_EXP:
        {
            /* IEEEDPExp: Natural exponential (e^x)
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = exp(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = exp(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_EXP -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_LOG:
        {
            /* IEEEDPLog: Natural logarithm
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = log(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = log(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_LOG -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_POW:
        {
            /* IEEEDPPow: Power function (arg^exp)
             * Input:  d1:d2 = arg (base), d3:d4 = exp (exponent)
             * Output: d0:d1 = arg^exp
             */
            double base = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            double exponent = host_double_from_m68k_registers(M68K_REG_D3, M68K_REG_D4);
            double result = pow(base, exponent);

            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_POW(%f, %f) = %f\n", base, exponent, result);
            host_double_to_m68k_registers(result, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_SQRT:
        {
            /* IEEEDPSqrt: Square root
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = sqrt(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = sqrt(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_SQRT -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_TIEEE:
        {
            /* IEEEDPTieee: Convert double to IEEE single
             * Input:  d1:d2 = IEEE double
             * Output: d0 = IEEE single
             */
            double dval = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_TIEEE(%f) -> single\n", dval);
            host_float_to_m68k_register((float)dval, M68K_REG_D0);
            break;
        }
        
        case EMU_CALL_IEEEDP_FIEEE:
        {
            /* IEEEDPFieee: Convert IEEE single to double
             * Input:  d1 = IEEE single
             * Output: d0:d1 = IEEE double
             */
            double dval = (double)host_float_from_m68k_register(M68K_REG_D1);
            
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_FIEEE(single) -> %f\n", dval);
            host_double_to_m68k_registers(dval, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ASIN:
        {
            /* IEEEDPAsin: Arc sine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = asin(input) in radians
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = asin(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ASIN -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_ACOS:
        {
            /* IEEEDPAcos: Arc cosine
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = acos(input) in radians
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = acos(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_ACOS -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }
        
        case EMU_CALL_IEEEDP_LOG10:
        {
            /* IEEEDPLog10: Base-10 logarithm
             * Input:  d1:d2 = IEEE double
             * Output: d0:d1 = log10(input)
             */
            double value = host_double_from_m68k_registers(M68K_REG_D1, M68K_REG_D2);
            value = log10(value);
            DPRINTF(LOG_DEBUG, "lxa: IEEEDP_LOG10 -> %f\n", value);
            host_double_to_m68k_registers(value, M68K_REG_D0, M68K_REG_D1);
            break;
        }

        default:
        {
            /*
             * Undefined EMU_CALL - this happens when the PC jumps to
             * invalid/corrupt code. This typically occurs during the main
             * process's libnix exit sequence when stack corruption happens
             * due to multitasking timing issues.
             * 
             * When this happens during normal program exit (after the main
             * task finishes its work), we should exit gracefully rather than
             * crashing. The return value has already been set by g_rv from
             * a previous EMU_CALL_EXIT or EMU_CALL_STOP.
             */
            DPRINTF (LOG_WARNING, "*** undefined EMU_CALL #%d (corrupt PC at 0x%08x)\n", 
                     d0, m68k_get_reg(NULL, M68K_REG_PC));
            
            /*
             * Just exit the emulator. Any child tasks will be terminated
             * along with the emulator. This is acceptable because:
             * 1. The main task has already finished its work
             * 2. Child tasks that didn't get to cleanup are acceptable loss
             * 3. This prevents hangs from orphaned child tasks
             */
            m68k_end_timeslice();
            g_running = FALSE;
            break;
        }
    }

    return M68K_INT_ACK_AUTOVECTOR;
}

#define MAX_JITTER 1024

static float ffp_to_host_float(uint32_t raw)
{
    uint32_t mantissa;
    int exponent;
    float value;

    if (raw == 0)
    {
        return 0.0f;
    }

    mantissa = raw >> 8;
    exponent = (int)(raw & 0x7f) - 64;
    value = ldexpf((float)mantissa / 16777216.0f, exponent);

    if (raw & 0x80)
    {
        value = -value;
    }

    return value;
}

static float host_float_from_bits(uint32_t bits)
{
    union { float f; uint32_t u; } value;

    value.u = bits;
    return value.f;
}

static uint32_t host_float_to_bits(float value)
{
    union { float f; uint32_t u; } bits;

    bits.f = value;
    return bits.u;
}

static float host_float_from_m68k_register(int reg)
{
    return host_float_from_bits(m68k_get_reg(NULL, reg));
}

static void host_float_to_m68k_register(float value, int reg)
{
    m68k_set_reg(reg, host_float_to_bits(value));
}

static double host_double_from_words(uint32_t hi, uint32_t lo)
{
    uint64_t bits = ((uint64_t)hi << 32) | (uint64_t)lo;
    double value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void host_double_to_words(double value, uint32_t *hi, uint32_t *lo)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    *hi = (uint32_t)(bits >> 32);
    *lo = (uint32_t)bits;
}

static double host_double_from_m68k_registers(int hi_reg, int lo_reg)
{
    return host_double_from_words(m68k_get_reg(NULL, hi_reg), m68k_get_reg(NULL, lo_reg));
}

static void host_double_to_m68k_registers(double value, int hi_reg, int lo_reg)
{
    uint32_t hi;
    uint32_t lo;

    host_double_to_words(value, &hi, &lo);
    m68k_set_reg(hi_reg, hi);
    m68k_set_reg(lo_reg, lo);
}

static void host_double_to_m68k_memory(uint32_t addr, double value)
{
    uint32_t hi;
    uint32_t lo;

    if (addr == 0)
    {
        return;
    }

    host_double_to_words(value, &hi, &lo);
    m68k_write_memory_32(addr, hi);
    m68k_write_memory_32(addr + 4, lo);
}

static int32_t clamp_double_to_long(double value)
{
    if (value > 2147483647.0)
    {
        return 0x7FFFFFFF;
    }
    if (value < -2147483648.0)
    {
        return (int32_t)0x80000000;
    }

    return (int32_t)value;
}

static int32_t clamp_float_to_long(float value)
{
    if (value > 2147483647.0f)
    {
        return 0x7FFFFFFF;
    }
    if (value < -2147483648.0f)
    {
        return (int32_t)0x80000000;
    }

    return (int32_t)value;
}

static int32_t compare_double_values(double left, double right)
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }

    return 0;
}

static int32_t compare_float_values(float left, float right)
{
    if (left < right)
    {
        return -1;
    }
    if (left > right)
    {
        return 1;
    }

    return 0;
}

static double host_safe_double_divide(double dividend, double divisor)
{
    if (divisor == 0.0)
    {
        return (signbit(dividend) != signbit(divisor)) ? -HUGE_VAL : HUGE_VAL;
    }

    return dividend / divisor;
}

static float host_safe_float_divide(float dividend, float divisor)
{
    if (divisor == 0.0f)
    {
        return (dividend >= 0.0f) ? HUGE_VALF : -HUGE_VALF;
    }

    return dividend / divisor;
}

static uint32_t host_float_to_ffp(float value)
{
    float mantissa;
    int exponent;
    uint32_t sign = 0;
    uint32_t mantissa_bits;
    int encoded_exponent;

    if (value == 0.0f)
    {
        return 0;
    }

    if (isnan(value))
    {
        return 0;
    }

    if (signbit(value))
    {
        sign = 0x80;
        value = -value;
    }

    if (isinf(value))
    {
        return sign | 0xffffff7fU;
    }

    mantissa = frexpf(value, &exponent);
    mantissa_bits = (uint32_t)lrintf(mantissa * 16777216.0f);

    if (mantissa_bits >= 0x1000000U)
    {
        mantissa_bits >>= 1;
        exponent++;
    }

    encoded_exponent = exponent + 64;
    if (encoded_exponent <= 0)
    {
        return 0;
    }
    if (encoded_exponent > 0x7f)
    {
        return sign | 0xffffff7fU;
    }

    return (mantissa_bits << 8) | sign | (uint32_t)encoded_exponent;
}

static uint32_t _debug_print_diss (uint32_t pc, uint32_t curPC)
{
    static char buff[100];
    static char buff2[100];
    char *prefix="";

    if (pc==curPC)
    {
        prefix = "--->";
    }
    else
    {
        for (int i=0; i<g_num_breakpoints; i++)
        {
            if (pc==g_breakpoints[i])
                prefix = "[BP]";
        }
    }

    char *name="";
    for (map_sym_t *m=_g_map; m; m=m->next)
    {
        int32_t diff = pc-m->offset;
        if ( (diff>=0) && (diff<MAX_JITTER) )
            name = m->name;
    }

    uint32_t instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68030);
    make_hex(buff2, pc, instr_size);
    CPRINTF("%-5s0x%08x  %-20s: %-40s (%s)\n", prefix, pc, buff2, buff, name);
    return instr_size;
}

static void _debug_traceback (int n, uint32_t pcFinal)
{
    // dump last n instructions from trace buf:
    uint32_t pc;

    CPRINTF ("Traceback:\n\n");

    n--;

    if (n>TRACE_BUF_ENTRIES)
        n = TRACE_BUF_ENTRIES;

    int idx = g_trace_buf_idx - n ;
    if (idx<0) idx += TRACE_BUF_ENTRIES;
    for (int i = 0; i<n; i++)
    {
        pc = g_trace_buf[idx];
        _debug_print_diss(pc, pcFinal);
        idx = (idx+1) % TRACE_BUF_ENTRIES;
    }

    //pc = pcFinal;
    //_debug_print_diss(pc, "---> ");
    CPRINTF("\n");
}

static void _debug_machine_state (void)
{
    DPRINTF (LOG_INFO, "Machine state:\n\n");
    print68kstate(LOG_INFO);
    DPRINTF (LOG_INFO, "\n");
}

static uint32_t _debug_memdump (uint32_t addr)
{
    CPRINTF ("Memory dump:\n\n");

    hexdump (LOG_INFO, addr, 256);

    CPRINTF ("\n");

    return addr+256;
}

static uint32_t _debug_disassemble (int n, uint32_t pcCur)
{
    unsigned int instr_size;
    uint32_t pc = pcCur;

    for (int i = 0; i<n; i++)
    {
        instr_size = _debug_print_diss(pc, pcCur);
        pc += instr_size;
    }

    CPRINTF("\n");

    return pc;
}

static void _debug_help (void)
{
    CPRINTF ("Available commands:\n\n");
    CPRINTF ("h            - this help screen\n");
    CPRINTF ("c            - continue\n");
    CPRINTF ("r            - registers\n");
    CPRINTF ("s            - step\n");
    CPRINTF ("n            - next\n");
    CPRINTF ("b <addr/reg> - toggle breakpoint\n");
    CPRINTF ("t <num>      - traceback\n");
    CPRINTF ("m <addr/reg> - memory dump\n");
    CPRINTF ("d <addr/reg> - disassemble\n");
    CPRINTF ("S            - symboltable\n");
    CPRINTF ("\n");
}

static void _debug_print_symtab (void)
{
    for (map_sym_t *sym = _g_map; sym; sym=sym->next)
        CPRINTF ("0x%08x %s\n", sym->offset, sym->name);
    CPRINTF ("\n");
}


static void _debug(uint32_t pcFinal)
{
    static bool in_debug = FALSE;

    if (in_debug)
        return;
    in_debug = TRUE;

    CPRINTF("\n     *** LXA DEBUGGER ***\n\n");

    _debug_traceback (5, pcFinal);

    _debug_disassemble (5, pcFinal);

    _debug_machine_state ();

    // interactive debugger loop

    char* buf;
    uint32_t caddr = pcFinal;
    fputs(CSI "?25h", stdout);  // cursor visible
    while ((buf = readline(">> ")))
    {
        int l = strlen(buf);
        //CPRINTF ("buf l=%d (%s)\n", l, buf);
        if (!l)
            continue;

        add_history(buf);
        CPRINTF ("\n");

        switch (buf[0])
        {
            case 'h':
            case '?':
                _debug_help();
                break;
            case 'c':
                in_debug = FALSE;
                g_stepping = FALSE;
                _update_debug_active();
                return;
            case 's':
                in_debug = FALSE;
                g_stepping = TRUE;
                _update_debug_active();
                return;
            case 'n':
            {
                static char buff[100];
                in_debug   = FALSE;
                g_stepping = FALSE;
                uint32_t instr_size = m68k_disassemble(buff, pcFinal, M68K_CPU_TYPE_68030);
                g_next_pc = pcFinal+instr_size;
                _update_debug_active();
                return;
            }
            case 'b':
            {
                uint32_t addr = 0;
                if (strlen(buf)>2)
                    addr = _debug_parse_addr (&buf[2]);
                if (addr)
                {
                    if (_debug_toggle_bp (addr))
                        CPRINTF ("   added breakpoint at 0x%08x\n", addr);
                    else
                        CPRINTF ("   removed breakpoint at 0x%08x\n", addr);
                }
                else
                {
                    CPRINTF ("   *** error: failed to parse/resolve breakpoint address\n");
                }
                break;
            }
            case 'r':
                _debug_machine_state ();
                break;
            case 't':
            {
                int num;
                int n = sscanf (buf, "t %d", &num);
                if (n==1)
                    _debug_traceback (num, pcFinal);
                else
                    CPRINTF ("???\n");
                break;
            }
            case 'm':
            {
                uint32_t addr = caddr;
                if (strlen(buf)>2)
                    addr = _debug_parse_addr(&buf[2]);
                if (addr)
                {
                    caddr = _debug_memdump (addr);
                }
                else
                {
                    CPRINTF ("???\n");
                }
                break;
            }
            case 'd':
            {
                uint32_t addr = caddr;
                if (strlen(buf)>2)
                    addr = _debug_parse_addr(&buf[2]);
                if (addr)
                    caddr = _debug_disassemble (10, addr);
                else
                    CPRINTF ("???\n");
                break;
            }
            case 'S':
                _debug_print_symtab ();
                break;
            default:
                CPRINTF("*** unknown command error *** (%s)\n", buf);
        }

        free(buf);
    }
    assert(FALSE);
}

#define MAX_LINE_LEN 1024

/* Load ROM symbol map - exported for lxa_api.c */
bool _load_rom_map (const char *rom_path)
{
    char map_path[PATH_MAX];
    char line_buf[MAX_LINE_LEN];

    snprintf (map_path, PATH_MAX, "%s.map", rom_path);

    FILE *mapf = fopen (map_path, "r");
    if (!mapf)
    {
        fprintf (stderr, "failed to open %s\n", map_path);
        return false;
    }

    regex_t reegex;
    int value = regcomp (&reegex, "^[ ]*0x([0-9a-f]+)[ ]*([_a-zA-Z]+)", REG_EXTENDED);
    if (value)
    {
        fprintf (stderr, "*** internal error: regcomp failed\n");
        return false;
    }

    while (fgets (line_buf, MAX_LINE_LEN, mapf))
    {
        //printf("%s\n", line_buf);

        regmatch_t matches[5];

        value = regexec( &reegex, line_buf, 5, matches, 0);
        if (!value)
        {
            //printf ("   *** MATCH *** %d-%d %d-%d %d-%d %d-%d\n",
            //        (int)matches[0].rm_so, (int)matches[0].rm_eo,
            //        (int)matches[1].rm_so, (int)matches[1].rm_eo,
            //        (int)matches[2].rm_so, (int)matches[2].rm_eo,
            //        (int)matches[3].rm_so, (int)matches[3].rm_eo);


            uint32_t offset = 0;
            sscanf(&line_buf[matches[1].rm_so], "%x", &offset);
            int l = matches[2].rm_eo-matches[2].rm_so;
            char *name = malloc(l+1);
            if (!name)
            {
                fprintf (stderr, "OOM\n");
                return false;
            }
            name = strncpy (name, &line_buf[matches[2].rm_so], l);
            name[l]=0;
            //printf ("%-20s at 0x%x\n", name, offset);

            if (!_symtab_add_owned (name, offset, true))
                return false;

        }
    }

    fclose (mapf);

    return true;
}

/*
 * Set console output capture hook for test drivers.
 * When set, all console output will be passed to this callback.
 */
void lxa_set_console_output_hook(void (*hook)(const char *data, int len))
{
    g_console_output_hook = hook;
}

/* When building liblxa as a library, we don't include main() or print_usage() */
#ifndef LXA_LIBRARY_BUILD
static void print_usage(char *argv[])
{
    fprintf(stderr, "lxa - Linux Amiga Emulation Layer\n");
    fprintf(stderr, "usage: %s [ options ] [ program [ args... ] ]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -a <name=path>          replace assign (legacy alias for --assign)\n");
    fprintf(stderr, "    --assign <name=path>    replace assign target (e.g., Cluster=/path/to/Cluster)\n");
    fprintf(stderr, "    --assign-add <name=path> append path to multi-assign search list\n");
    fprintf(stderr, "    -b <addr|sym>  add breakpoint, examples: -b _start\n");
    fprintf(stderr, "    -c <config>    use config file (default: ~/.lxa/config.ini)\n");
    fprintf(stderr, "    -d             enable debug output\n");
    fprintf(stderr, "    -h, --help     display this help and exit\n");
    fprintf(stderr, "    -r <rom>       use kickstart ROM (auto-detected if not specified)\n");
    fprintf(stderr, "    -v             verbose mode\n");
    fprintf(stderr, "    -t             trace mode\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "If no program is specified, the interactive Amiga shell is launched.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Drives:\n");
    fprintf(stderr, "    SYS:   System drive (configured via config.ini)\n");
    fprintf(stderr, "    HOME:  User's home directory (automatic)\n");
    fprintf(stderr, "    CWD:   Current working directory (automatic)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "    %s                          # Launch interactive shell\n", argv[0]);
    fprintf(stderr, "    %s hello.world              # Run a program\n", argv[0]);
    fprintf(stderr, "    %s -r /path/to/lxa.rom prog # Specify ROM location\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Configuration: ~/.lxa/config.ini (created on first run)\n");
}

#define MAX_PENDING_ASSIGNS 32

typedef enum pending_assign_mode_e {
    PENDING_ASSIGN_REPLACE,
    PENDING_ASSIGN_APPEND
} pending_assign_mode_t;

typedef struct pending_assign_s {
    char name[256];
    char path[PATH_MAX];
    pending_assign_mode_t mode;
} pending_assign_t;

static const char *get_option_value(int argc, char **argv, int *optind,
                                    const char *current_arg, const char *option_name)
{
    size_t option_len = strlen(option_name);

    if (strncmp(current_arg, option_name, option_len) != 0) {
        return NULL;
    }

    if (current_arg[option_len] == '=') {
        return current_arg + option_len + 1;
    }

    if (current_arg[option_len] != '\0') {
        return NULL;
    }

    (*optind)++;
    if (*optind >= argc) {
        fprintf(stderr, "Error: %s requires name=path argument\n", option_name);
        exit(EXIT_FAILURE);
    }

    return argv[*optind];
}

static void queue_pending_assign(pending_assign_t *pending_assigns, int *num_pending_assigns,
                                 const char *spec, pending_assign_mode_t mode,
                                 const char *option_name)
{
    const char *eq = strchr(spec, '=');
    size_t name_len;

    if (!eq || eq == spec || eq[1] == '\0') {
        fprintf(stderr, "Error: %s argument must be in name=path format\n", option_name);
        exit(EXIT_FAILURE);
    }

    if (*num_pending_assigns >= MAX_PENDING_ASSIGNS) {
        fprintf(stderr, "Error: too many assign options (max %d)\n", MAX_PENDING_ASSIGNS);
        exit(EXIT_FAILURE);
    }

    name_len = (size_t)(eq - spec);
    if (name_len >= sizeof(pending_assigns[*num_pending_assigns].name)) {
        name_len = sizeof(pending_assigns[*num_pending_assigns].name) - 1;
    }

    strncpy(pending_assigns[*num_pending_assigns].name, spec, name_len);
    pending_assigns[*num_pending_assigns].name[name_len] = '\0';
    strncpy(pending_assigns[*num_pending_assigns].path, eq + 1,
            sizeof(pending_assigns[*num_pending_assigns].path) - 1);
    pending_assigns[*num_pending_assigns].path[sizeof(pending_assigns[*num_pending_assigns].path) - 1] = '\0';
    pending_assigns[*num_pending_assigns].mode = mode;
    (*num_pending_assigns)++;
}

int main(int argc, char **argv, char **envp)
{
    char *rom_path = NULL;
    char *config_path = NULL;
    int optind=0;

    /* Pending assigns from command line flags (applied after config is loaded) */
    pending_assign_t pending_assigns[MAX_PENDING_ASSIGNS];
    int num_pending_assigns = 0;

    vfs_init();

    // argument parsing
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++)
    {
        /* Handle --help first */
        if (strcmp(argv[optind], "--help") == 0)
        {
            print_usage(argv);
            exit(EXIT_SUCCESS);
        }

        if (strncmp(argv[optind], "--assign-add", strlen("--assign-add")) == 0)
        {
            const char *spec = get_option_value(argc, argv, &optind, argv[optind], "--assign-add");
            if (!spec)
            {
                print_usage(argv);
                exit(EXIT_FAILURE);
            }

            queue_pending_assign(pending_assigns, &num_pending_assigns, spec,
                                 PENDING_ASSIGN_APPEND, "--assign-add");
            continue;
        }

        if (strncmp(argv[optind], "--assign", strlen("--assign")) == 0)
        {
            const char *spec = get_option_value(argc, argv, &optind, argv[optind], "--assign");
            if (!spec)
            {
                print_usage(argv);
                exit(EXIT_FAILURE);
            }

            queue_pending_assign(pending_assigns, &num_pending_assigns, spec,
                                 PENDING_ASSIGN_REPLACE, "--assign");
            continue;
        }

        switch (argv[optind][1])
        {
            case 'a':
            {
                optind++;
                if (optind >= argc)
                {
                    fprintf(stderr, "Error: -a requires name=path argument\n");
                    exit(EXIT_FAILURE);
                }
                queue_pending_assign(pending_assigns, &num_pending_assigns, argv[optind],
                                     PENDING_ASSIGN_REPLACE, "-a");
                break;
            }
            case 'b':
            {
                optind++;
                pending_bp_t *pbp = malloc (sizeof (*pbp));
                strncpy (pbp->name, argv[optind], 256);
                pbp->next = _g_pending_bps;
                _g_pending_bps = pbp;
                break;
            }
            case 'c':
            {
                optind++;
                config_path = argv[optind];
                break;
            }
            case 'd':
                g_debug = true;
                break;
            case 'h':
                print_usage(argv);
                exit(EXIT_SUCCESS);
            case 'r':
            {
                optind++;
                rom_path = argv[optind];
                break;
            }
            case 'v':
                g_verbose = true;
                break;
            case 't':
                g_trace = true;
                _update_debug_active();
                break;
            default:
                print_usage(argv);
                exit(EXIT_FAILURE);
        }
    }

    /*
     * Phase 3: Automatic Environment Setup
     * 
     * Check if ~/.lxa exists. If not, create it with the default structure.
     */
    if (!config_path) {
        vfs_setup_environment();
    }

    if (config_path) {
        config_load(config_path);
    } else {
        const char *lxa_home = vfs_get_home_dir();
        if (lxa_home) {
            char default_cfg[PATH_MAX];
            snprintf(default_cfg, PATH_MAX, "%s/config.ini", lxa_home);
            config_load(default_cfg);
        }
    }

    if (!vfs_has_sys_drive()) {
        /* No SYS: drive configured - try sys/ subdirectory first, then current directory */
        struct stat st;
        if (stat(DEFAULT_AMIGA_SYSROOT, &st) == 0 && S_ISDIR(st.st_mode)) {
            vfs_add_drive("SYS", DEFAULT_AMIGA_SYSROOT);
        } else {
            vfs_add_drive("SYS", ".");
        }
    }

    /* Set up automatic HOME: and CWD: drives */
    vfs_setup_dynamic_drives();
    
    /* Set up default assigns (C:, S:, LIBS:, etc.) pointing to SYS: subdirectories */
    vfs_setup_default_assigns();

    /* Apply pending assigns from command line flags */
    for (int i = 0; i < num_pending_assigns; i++)
    {
        bool ok;

        if (pending_assigns[i].mode == PENDING_ASSIGN_APPEND)
        {
            ok = vfs_assign_add_path(pending_assigns[i].name, pending_assigns[i].path);
        }
        else
        {
            ok = vfs_assign_add(pending_assigns[i].name, pending_assigns[i].path, ASSIGN_LOCK);
        }

        if (!ok)
        {
            fprintf(stderr, "lxa: WARNING: Failed to %s assign %s: -> %s\n",
                    pending_assigns[i].mode == PENDING_ASSIGN_APPEND ? "append to" : "set",
                    pending_assigns[i].name, pending_assigns[i].path);
        }
        else
        {
            DPRINTF(LOG_DEBUG, "lxa: %s assign from command line: %s: -> %s\n",
                    pending_assigns[i].mode == PENDING_ASSIGN_APPEND ? "Appended to" : "Set",
                    pending_assigns[i].name, pending_assigns[i].path);
        }
    }

    if (!rom_path) {
        rom_path = (char *)config_get_rom_path();
        if (!rom_path) {
            rom_path = (char *)auto_detect_rom_path();
            if (!rom_path) {
                fprintf(stderr, "lxa: ERROR: Could not find lxa.rom\n");
                fprintf(stderr, "lxa: Searched in:\n");
                fprintf(stderr, "lxa:   - ../share/lxa/ (relative to binary)\n");
                fprintf(stderr, "lxa:   - Current directory\n");
                fprintf(stderr, "lxa:   - src/rom/ (relative to current)\n");
                fprintf(stderr, "lxa:   - LXA_PREFIX/ (if LXA_PREFIX env var is set)\n");
                fprintf(stderr, "lxa:   - ~/.lxa/\n");
                fprintf(stderr, "lxa:   - /usr/local/share/lxa/\n");
                fprintf(stderr, "lxa:   - /usr/share/lxa/\n");
                fprintf(stderr, "lxa: Please specify ROM path with -r option or install lxa.rom\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Default to interactive shell if no program specified */
    if (argc <= optind)
    {
        g_loadfile = "SYS:System/Shell";
        g_args[0] = '\0';
        g_args_len = 0;
    }
    else
    {
        g_loadfile = argv[optind];
    
        /* Store remaining arguments for the program */
        if (argc > optind + 1) {
            /* Build argument string from remaining arguments */
            g_args_len = 0;
            for (int i = optind + 1; i < argc && g_args_len < MAX_ARGS_LEN - 1; i++) {
                if (i > optind + 1) {
                    g_args[g_args_len++] = ' ';
                }
                int arg_len = strlen(argv[i]);
                int copy_len = (g_args_len + arg_len < MAX_ARGS_LEN - 1) ? arg_len : (MAX_ARGS_LEN - 1 - g_args_len);
                memcpy(g_args + g_args_len, argv[i], copy_len);
                g_args_len += copy_len;
            }
            g_args[g_args_len] = '\0';
        } else {
            g_args[0] = '\0';
            g_args_len = 0;
        }
    }
    
    g_sysroot = ".";

    util_init();

    DPRINTF (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       ROM_START          = 0x%08x\n", ROM_START         );
    DPRINTF (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       ROM_END            = 0x%08x\n", ROM_END           );
    DPRINTF (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       RAM_START          = 0x%08x\n", RAM_START         );
    DPRINTF (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       RAM_END            = 0x%08x\n", RAM_END           );

    // load rom code
    FILE *romf = fopen (rom_path, "r");
    if (!romf)
    {
        fprintf (stderr, "failed to open %s\n", rom_path);
        exit(1);
    }
    if (fread (g_rom, ROM_SIZE, 1, romf) != 1)
    {
        fprintf (stderr, "failed to read from %s\n", rom_path);
        exit(2);
    }
    fclose(romf);

    if (!_load_rom_map(rom_path))
        exit(3);


    // setup memory image

    uint32_t initial_sp   = RAM_END-1;
    uint32_t reset_vector = ROM_START+2;

    m68k_write_memory_32 (0, initial_sp);   // m68k initial SP
    m68k_write_memory_32 (4, reset_vector); // m68k reset vector

    uint32_t p = m68k_read_memory_32(ROM_START+4);

    LPRINTF (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa: initial_sp=0x%08x, reset_vector=0x%08x jumps to 0x%08x\n",
             initial_sp, reset_vector, p);

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68030);  /* Phase 31: Support 68030 MMU instructions for SysInfo */
    m68k_pulse_reset();

    /*
     * Initialise the display subsystem (SDL2) before the SIGALRM timer is
     * armed.  SDL_Init() performs X11/Wayland connection setup which involves
     * blocking system calls (connect, poll).  If SIGALRM fires during those
     * calls, SA_RESTART *should* be sufficient, but in practice some display
     * server round-trips fail or hang under repeated signal delivery.
     * Calling display_init() here — before any signals are armed — avoids
     * the race entirely and ensures SDL2 is ready by the time the first
     * EMU_CALL_INT_OPEN_SCREEN arrives.
     */
    display_init();

    /*
     * Phase 6.5: Set up timer-driven preemptive multitasking
     *
     * We use setitimer() to generate SIGALRM at ~50Hz (PAL VBlank rate).
     * The signal handler sets g_pending_irq which is checked each iteration.
     */
    struct sigaction sa;
    sa.sa_handler = sigalrm_handler;
    sa.sa_flags = SA_RESTART;  /* Restart interrupted syscalls */
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIMER_INTERVAL_US;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIMER_INTERVAL_US;
    if (setitimer(ITIMER_REAL, &timer, NULL) < 0)
    {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }

    DPRINTF(LOG_DEBUG, "lxa: Timer-driven scheduler enabled at %d Hz\n", 1000000 / TIMER_INTERVAL_US);

    while (g_running)
    {
        /*
         * Check for pending interrupts from the timer.
         * If INTENA allows VBlank interrupts and we have a pending Level 3,
         * trigger the interrupt.
         */
        if (g_pending_irq & (1 << 3))
        {
            /*
             * VBlank time - refresh all displays and poll SDL events.
             * This happens at 50Hz regardless of whether the Amiga has
             * interrupts enabled, ensuring the display stays updated.
             */
            if (display_poll_events())
            {
                g_running = false;  /* Quit requested */
                break;
            }

            (void)_dos_notify_poll();

            /*
             * Update display from Amiga's planar bitmap if configured.
             * This converts the planar data in emulated RAM to chunky pixels
             * so that display_refresh_all() can present them via SDL.
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

            display_refresh_all();

            /*
             * Phase 45: Check for expired timer requests.
             * This processes the host-side timer queue and signals completion
             * for any requests whose wake time has passed.
             */
            _timer_check_expired();

            if ((g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK))
            {
                /* Clear pending flag and trigger interrupt.
                 *
                 * Phase 108c: Pulse the IRQ line (lower then raise) so Musashi
                 * recognises a fresh edge.  Without this, consecutive VBlanks
                 * at the same level are silently ignored because Musashi treats
                 * same-level interrupts as edge-triggered.  This was already
                 * fixed in lxa_run_cycles() (Phase 105) but the main interactive
                 * loop was still missing the pulse, causing buttons in apps like
                 * SysInfo to appear unresponsive in SDL mode. */
                g_pending_irq &= ~(1 << 3);
                m68k_set_irq(0);
                m68k_set_irq(3);
            }
        }

        /*
         * Execute a batch of instructions. The batch size is chosen to be
         * small enough that we check for interrupts frequently (~1000 cycles
         * gives reasonable responsiveness while keeping overhead low).
         */
        m68k_execute(1000);
    }

    /* Stop the timer */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);

    _audio_shutdown();

    return g_rv;
}
#endif /* !LXA_LIBRARY_BUILD */
