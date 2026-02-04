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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
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
#define CUSTOM_REG_BLTCON0  0x040
#define CUSTOM_REG_BLTCON1  0x042
#define CUSTOM_REG_BLTSIZE  0x058
#define CUSTOM_REG_VPOSR    0x004  /* Read vertical position (Agnus) */
#define CUSTOM_REG_VHPOSR   0x006  /* Read vert/horiz position */
#define CUSTOM_REG_DENISEID 0x07c  /* Denise ID register */
#define CUSTOM_REG_ADKCON   0x09e  /* Audio/disk control */
#define CUSTOM_REG_ADKCONR  0x010  /* Audio/disk control read */
#define CUSTOM_REG_POTGO    0x034  /* Pot port control */
#define CUSTOM_REG_POTINP   0x016  /* Pot port read */
#define CUSTOM_REG_JOY0DAT  0x00a  /* Joystick 0 data */
#define CUSTOM_REG_JOY1DAT  0x00c  /* Joystick 1 data */
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

#define OFFSET_BEGINNING    -1
#define OFFSET_CURRENT       0
#define OFFSET_END           1

#define FILE_KIND_REGULAR    42
#define FILE_KIND_CONSOLE    23
#define FILE_KIND_CON        99   /* CON:/RAW: window (m68k-side handling) */

#define TRACE_BUF_ENTRIES 1000000
#define MAX_BREAKPOINTS       16

static uint8_t  g_ram[RAM_SIZE];
static uint8_t  g_rom[ROM_SIZE];
static bool     g_verbose                       = FALSE;
static bool     g_trace                         = FALSE;
static bool     g_stepping                      = FALSE;
static uint32_t g_next_pc                       = 0;
static int      g_trace_buf[TRACE_BUF_ENTRIES];
static int      g_trace_buf_idx                 = 0;
static bool     g_running                       = TRUE;
static char    *g_loadfile                      = NULL;

#define MAX_ARGS_LEN 4096
static char     g_args[MAX_ARGS_LEN]            = {0};
static int      g_args_len                      = 0;

static uint32_t g_breakpoints[MAX_BREAKPOINTS];
static int      g_num_breakpoints               = 0;
static int      g_rv                            = 0;
static char    *g_sysroot                       = NULL;

typedef struct map_sym_s map_sym_t;

struct map_sym_s
{
    map_sym_t *next;
    uint32_t   offset;
    char      *name;
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

static uint16_t g_intena  = INTENA_MASTER | INTENA_VBLANK;

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

/* Pending interrupt flags (one bit per level 1-7) */
static volatile sig_atomic_t g_pending_irq = 0;

/* Last input event for IDCMP handling (Phase 14) */
static display_event_t g_last_event = {0};

/* Timer frequency in microseconds (50Hz = 20000us = 20ms) */
#define TIMER_INTERVAL_US 20000

/* SIGALRM handler - sets Level 3 interrupt pending */
static void sigalrm_handler(int sig)
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

typedef struct lock_entry_s {
    bool in_use;
    char linux_path[PATH_MAX];
    char amiga_path[PATH_MAX]; /* Original Amiga path for NameFromLock */
    DIR *dir;           /* For directory iteration */
    int refcount;       /* For DupLock */
    bool is_dir;        /* True if lock is on a directory */
} lock_entry_t;

static lock_entry_t g_locks[MAX_LOCKS];

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

/* Get current time in microseconds since epoch */
static uint64_t _timer_get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
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

/* Check and mark expired timer requests
 * This is called from the main emulation loop.
 * It only marks timers as expired - the actual ReplyMsg() is done from the
 * m68k side via the _timer_VBlankHook() function.
 */
static int _timer_check_expired(void)
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
#define ERROR_OBJECT_NOT_FOUND   205
#define ERROR_OBJECT_EXISTS      203
#define ERROR_DIR_NOT_FOUND      204
#define ERROR_BAD_STREAM_NAME    206
#define ERROR_OBJECT_IN_USE      202
#define ERROR_DELETE_PROTECTED   222
#define ERROR_WRITE_PROTECTED    223
#define ERROR_NO_MORE_ENTRIES    232
#define ERROR_NOT_A_DOS_DISK     225
#define ERROR_DISK_NOT_VALIDATED 213

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

static bool _symtab_add (char *name, uint32_t offset)
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
        case CUSTOM_REG_BLTSIZE: return "BLTSIZE";
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
            case CUSTOM_REG_DMACONR:  /* DMA control read - return 0 (no DMA) */
                result = 0;
                break;
            case CUSTOM_REG_DENISEID: /* Denise ID - return 0xFC for ECS Denise */
                result = (reg & 1) ? 0xFC : 0x00;
                break;
            case CUSTOM_REG_POTINP:   /* Pot port read - return 0xFF (no pots) */
                result = 0xFF;
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

static void _handle_custom_write (uint16_t reg, uint16_t value)
{

    switch (reg)
    {
        case CUSTOM_REG_INTENA:
        {
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: INTENA value=0x%04x\n", value);

            if (value & 0x8000)
            {
                // set
                g_intena |= value & 0x7fff;
            }
            else
            {
                // clear
                g_intena &= ~(value & 0x7fff);
            }

            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write -> INTENA=0x%04x\n", g_intena);

            break;
        }
        case CUSTOM_REG_DMACON:
        {
            /* DMA control - ignore for now, we don't do hardware DMA */
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: DMACON value=0x%04x (ignored)\n", value);
            break;
        }
        /* Phase 31: Additional custom chip registers (Denise, Agnus, Paula) - log and ignore */
        case CUSTOM_REG_COPJMP1:
        case CUSTOM_REG_COPJMP2:
        case CUSTOM_REG_ADKCON:
        case CUSTOM_REG_POTGO:
            DPRINTF (LOG_DEBUG, "lxa: _handle_custom_write: %s value=0x%04x (ignored)\n",
                    _custom_reg_name(reg), value);
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

void cpu_instr_callback(int pc)
{
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
    else
        snprintf (linux_path, buf_len, "%s/%s", g_sysroot, amiga_path);
    
    DPRINTF (LOG_DEBUG, "lxa: _dos_path2linux: legacy resolved %s -> %s\n", amiga_path, linux_path);
}

static int errno2Amiga (void)
{
    // FIXME: do actual mapping!
    return errno;
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
            m68k_write_memory_32 (fh68k+40, errno2Amiga());  // fh_Arg2 = error code
            return errno2Amiga();
        }
    }

    return 0;
}

static int _dos_read (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_read(): fh=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd = m68k_read_memory_32 (fh68k+36);
    DPRINTF (LOG_DEBUG, "                  -> fd = %d\n", fd);

    void *buf = _mgetstr (buf68k);

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
static void _unix_time_to_datestamp(time_t unix_time, uint32_t ds68k)
{
    /* Amiga epoch is Jan 1, 1978 */
    /* Unix epoch is Jan 1, 1970 */
    /* Difference is 8 years = 2922 days (including 2 leap years) */
    #define AMIGA_EPOCH_OFFSET 252460800  /* seconds between 1970 and 1978 */
    
    time_t amiga_time = unix_time - AMIGA_EPOCH_OFFSET;
    if (amiga_time < 0) amiga_time = 0;
    
    /* DateStamp format: days, minutes, ticks (50ths of a second) */
    uint32_t days = amiga_time / (24 * 60 * 60);
    uint32_t remaining = amiga_time % (24 * 60 * 60);
    uint32_t minutes = remaining / 60;
    uint32_t ticks = (remaining % 60) * 50;
    
    m68k_write_memory_32(ds68k + 0, days);
    m68k_write_memory_32(ds68k + 4, minutes);
    m68k_write_memory_32(ds68k + 8, ticks);
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
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_duplock(): new_lock_id=%d\n", new_lock_id);
    return new_lock_id;
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
    
    _unix_time_to_datestamp(st.st_mtime, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(lock->linux_path, fib68k);
    
    /* Owner info */
    m68k_write_memory_16(fib68k + FIB_fib_OwnerUID, st.st_uid);
    m68k_write_memory_16(fib68k + FIB_fib_OwnerGID, st.st_gid);
    
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
    
    _unix_time_to_datestamp(st.st_mtime, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    _read_file_comment(fullpath, fib68k);
    
    m68k_write_memory_16(fib68k + FIB_fib_OwnerUID, st.st_uid);
    m68k_write_memory_16(fib68k + FIB_fib_OwnerGID, st.st_gid);
    
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
    if (stat(linux_path, &st) != 0) {
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
        size_t path_len = paths[i] ? strlen(paths[i]) : 0;
        
        if (offset + name_len + 1 + path_len + 1 >= buflen - 1) {
            break; /* No more space */
        }
        
        /* Write name */
        for (size_t j = 0; j < name_len; j++) {
            m68k_write_memory_8(buf68k + offset++, names[i][j]);
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        /* Write path */
        if (paths[i]) {
            for (size_t j = 0; j < path_len; j++) {
                m68k_write_memory_8(buf68k + offset++, paths[i][j]);
            }
        }
        m68k_write_memory_8(buf68k + offset++, '\0');
        
        written++;
    }
    
    /* Terminate with extra NUL */
    m68k_write_memory_8(buf68k + offset, '\0');
    
    return written;
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
    /* Try to find a matching drive mapping */
    const char *names[64];
    const char *paths[64];
    int count = vfs_assign_list(names, paths, 64);
    
    for (int i = 0; i < count; i++) {
        if (!paths[i]) continue;
        size_t plen = strlen(paths[i]);
        
        /* Check if linux_path starts with this path */
        if (strncmp(linux_path, paths[i], plen) == 0) {
            const char *remainder = linux_path + plen;
            
            /* Skip leading slash in remainder */
            if (*remainder == '/') remainder++;
            
            /* Build Amiga path: DRIVE:remainder */
            int written = snprintf(amiga_buf, bufsize, "%s:%s", names[i], remainder);
            if (written < 0 || (size_t)written >= bufsize) {
                return 0;
            }
            
            /* Convert slashes to Amiga style */
            for (char *p = amiga_buf; *p; p++) {
                if (*p == '/') *p = '/';  /* Actually Amiga uses / too */
            }
            
            return 1;
        }
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
    
    _unix_time_to_datestamp(st.st_mtime, fib68k + FIB_fib_Date);
    
    /* Read file comment from xattr or sidecar */
    if (linux_path[0]) {
        _read_file_comment(linux_path, fib68k);
    }
    
    /* Owner info */
    m68k_write_memory_16(fib68k + FIB_fib_OwnerUID, st.st_uid);
    m68k_write_memory_16(fib68k + FIB_fib_OwnerGID, st.st_gid);
    
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
        g_breakpoints[g_num_breakpoints++] = addr;
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

            DPRINTF (LOG_INFO, "*** EXCEPTION CAUGHT: pc=0x%08x #%2d ", pc, excn);

            switch (excn)
            {
                case  2: DPRINTF (LOG_INFO, "bus error\n"); break;
                case  3: DPRINTF (LOG_INFO, "address error\n"); break;
                case  4: DPRINTF (LOG_INFO, "illegal instruction\n"); break;
                case  5: DPRINTF (LOG_INFO, "divide by zero\n"); break;
                case  6: DPRINTF (LOG_INFO, "chk instruction\n"); break;
                case  7: DPRINTF (LOG_INFO, "trapv instruction\n"); break;
                case  8: DPRINTF (LOG_INFO, "privilege violation\n"); break;
                case  9: DPRINTF (LOG_INFO, "trace\n"); break;
                case 10: DPRINTF (LOG_INFO, "line 1010 emulator\n"); break;
                case 11: DPRINTF (LOG_INFO, "line 1111 emulator\n"); break;
                case 32: DPRINTF (LOG_INFO, "trap #0\n"); break;
                case 33: DPRINTF (LOG_INFO, "trap #1\n"); break;
                case 34: DPRINTF (LOG_INFO, "trap #2 (stack overflow)\n"); break;
                case 35: DPRINTF (LOG_INFO, "trap #3\n"); break;
                case 36: DPRINTF (LOG_INFO, "trap #4\n"); break;
                case 37: DPRINTF (LOG_INFO, "trap #5\n"); break;
                case 38: DPRINTF (LOG_INFO, "trap #6\n"); break;
                case 39: DPRINTF (LOG_INFO, "trap #7\n"); break;
                case 40: DPRINTF (LOG_INFO, "trap #8\n"); break;
                case 41: DPRINTF (LOG_INFO, "trap #9\n"); break;
                case 42: DPRINTF (LOG_INFO, "trap #10\n"); break;
                case 43: DPRINTF (LOG_INFO, "trap #11\n"); break;
                case 44: DPRINTF (LOG_INFO, "trap #12\n"); break;
                case 45: DPRINTF (LOG_INFO, "trap #13\n"); break;
                case 46: DPRINTF (LOG_INFO, "trap #14\n"); break;
                default: DPRINTF (LOG_INFO, "???\n"); break;
            }

            hexdump (LOG_INFO, isp, 16);

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
                }
                _debug(pc);
                m68k_end_timeslice();
                g_running = FALSE;
            } else {
                DPRINTF (LOG_WARNING, "*** Exception in task - continuing (use -d to halt and debug)\n");
            }
            break;
        }

        case EMU_CALL_WAIT:
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
                m68k_set_irq(3);
            }
            
            usleep(1000);  /* 1ms - avoid busy-waiting */
            break;

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
                char progdir[PATH_MAX];
                char subdir[PATH_MAX];
                struct stat st;
                
                /* Extract the directory containing the loaded program */
                strncpy(progdir, g_loadfile, sizeof(progdir) - 1);
                progdir[sizeof(progdir) - 1] = '\0';
                
                char *last_slash = strrchr(progdir, '/');
                if (last_slash && last_slash != progdir) {
                    *last_slash = '\0';
                    
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
                        snprintf(subdir, sizeof(subdir), "%s/%s", progdir, subdirs[i].dir);
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
            if (display_get_event(&event))
            {
                /* Store event data in static vars for subsequent GET calls */
                g_last_event = event;
                m68k_set_reg(M68K_REG_D0, (uint32_t)event.type);
            }
            else
            {
                m68k_set_reg(M68K_REG_D0, 0);
            }
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_POS:
        {
            /* Returns (x << 16) | y */
            int x, y;
            display_get_mouse_pos(&x, &y);
            m68k_set_reg(M68K_REG_D0, ((uint32_t)x << 16) | ((uint32_t)y & 0xFFFF));
            break;
        }

        case EMU_CALL_INT_GET_MOUSE_BTN:
        {
            /* Returns button code from last event */
            m68k_set_reg(M68K_REG_D0, (uint32_t)g_last_event.button_code);
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
            int w = (d3 >> 16) & 0xFFFF;
            int h = d3 & 0xFFFF;
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
            /* d1: window_handle, d2: (w << 16) | h */
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            display_window_t *win = (display_window_t *)(uintptr_t)d1;
            int w = (d2 >> 16) & 0xFFFF;
            int h = d2 & 0xFFFF;

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
                return;
            case 's':
                in_debug = FALSE;
                g_stepping = TRUE;
                return;
            case 'n':
            {
                static char buff[100];
                in_debug   = FALSE;
                g_stepping = FALSE;
                uint32_t instr_size = m68k_disassemble(buff, pcFinal, M68K_CPU_TYPE_68030);
                g_next_pc = pcFinal+instr_size;
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

static bool _load_rom_map (const char *rom_path)
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

            if (!_symtab_add (name, offset))
                return false;

        }
    }

    fclose (mapf);

    return true;
}

static void print_usage(char *argv[])
{
    fprintf(stderr, "lxa - Linux Amiga Emulation Layer\n");
    fprintf(stderr, "usage: %s [ options ] [ program [ args... ] ]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
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

int main(int argc, char **argv, char **envp)
{
    char *rom_path = NULL;
    char *config_path = NULL;
    int optind=0;

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

        switch (argv[optind][1])
        {
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
                /* Clear pending flag and trigger interrupt */
                g_pending_irq &= ~(1 << 3);
                m68k_set_irq(3);
            }
        }
        else
        {
            /* No pending interrupt - clear IRQ line */
            m68k_set_irq(0);
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

    return g_rv;
}
