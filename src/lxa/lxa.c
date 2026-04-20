#include "lxa_internal.h"
#include "lxa_memory.h"


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
#define CUSTOM_REG_COPCON   0x02e  /* Copper control (CDANG bit) */

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
bool     g_trace                         = FALSE;
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
void (*g_console_output_hook)(const char *data, int len) = NULL;

#define MAX_ARGS_LEN 4096
char     g_args[MAX_ARGS_LEN]            = {0};
int      g_args_len                      = 0;

static uint32_t g_breakpoints[MAX_BREAKPOINTS];
static int      g_num_breakpoints               = 0;
int      g_rv                            = 0;
char    *g_sysroot                       = NULL;

/*
 * Hardware blitter emulation state.
 * Shadows the Amiga custom chip blitter registers.
 * Writing to BLTSIZE (or BLTSIZH for ECS) triggers the blit.
 */

/*
 * Custom-chip color register shadow (COLOR00..COLOR31).
 * Updated by direct CPU writes to 0xDFF180..0xDFF1BE *and* by the
 * copper interpreter (Phase 114). Apps and tests can observe the
 * current palette via lxa_get_color() / EMU_CALL_GFX_GET_COLOR.
 */
uint16_t g_color_regs[32] = {0};

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

pending_bp_t *_g_pending_bps = NULL;

// interrupts

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
display_event_t g_last_event = {0};

/* Timer frequency in microseconds (50Hz = 20000us = 20ms) */
#define TIMER_INTERVAL_US 20000

/* SIGALRM handler - sets Level 3 interrupt pending - exported for lxa_api.c */
void sigalrm_handler(int sig)
{
    (void)sig;
    g_pending_irq |= (1 << 3);  /* Level 3 = VBlank */
}

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
static char g_console_input_queue[1024];
static int g_console_input_head = 0;
static int g_console_input_tail = 0;

bool lxa_host_console_input_empty(void)
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

int lxa_host_console_input_pop(void)
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
    memset(g_locks, 0, sizeof(lock_entry_t) * MAX_LOCKS);
    memset(g_record_locks, 0, sizeof(record_lock_entry_t) * MAX_RECORD_LOCKS);
    memset(g_timer_queue, 0, sizeof(timer_request_t) * MAX_TIMER_REQUESTS);
    memset(g_notify_requests, 0, sizeof(notify_entry_t) * MAX_NOTIFY_REQUESTS);
    memset(g_console_input_queue, 0, sizeof(g_console_input_queue));
    g_console_input_head = 0;
    g_console_input_tail = 0;
    g_pending_irq = 0;

    /* Reset chipset shadow state so a previous in-process run does not
     * leak custom-register values (DMACON, COP1LC, palette) into the
     * next coldstart. Without this, the next test would inherit a
     * stale copper pointer into freed chip RAM and crash on the next
     * VBlank. */
    g_dmacon = 0x03F0;
    memset(g_color_regs, 0, sizeof(g_color_regs));
    copper_reset();

#ifdef SDL2_FOUND
    memset(g_audio_channels, 0, sizeof(audio_channel_state_t) * AUDIO_HOST_CHANNELS);
    g_audio_device = 0;
    g_audio_initialized = false;
#endif
}

/* Get current time in microseconds since epoch */
void _debug(uint32_t pc);

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

bool _symtab_add (char *name, uint32_t offset)
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
void _update_debug_active(void)
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

void _debug_add_bp (uint32_t addr)
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

uint32_t _debug_parse_addr(const char *buf)
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


void _debug(uint32_t pcFinal)
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
             * Run one pass of the copper list (Phase 114).
             * Done per VBlank, before the planar→chunky conversion, so
             * any palette changes the copper performs take effect in
             * the frame currently being presented.
             */
            copper_run_frame();

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
