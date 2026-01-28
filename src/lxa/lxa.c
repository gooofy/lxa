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
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <linux/limits.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "m68k.h"
#include "emucalls.h"
#include "util.h"
#include "vfs.h"
#include "config.h"

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define DEFAULT_ROM_PATH "../rom/lxa.rom"
//#define DEFAULT_ROM_PATH "../tinyrom/lxa.rom"

#define ROM_SIZE    256 * 1024
#define ROM_START   0xfc0000
#define ROM_END     ROM_START + ROM_SIZE - 1

#define CUSTOM_START 0xdff000
#define CUSTOM_END   0xdfffff

#define CUSTOM_REG_INTENA   0x09a

#define DEFAULT_AMIGA_SYSROOT "/home/guenter/media/emu/amiga/FS-UAE/hdd/system/"

#define MODE_OLDFILE        1005
#define MODE_NEWFILE        1006
#define MODE_READWRITE      1004

#define OFFSET_BEGINNING    -1
#define OFFSET_CURRENT       0
#define OFFSET_END           1

#define FILE_KIND_REGULAR    42
#define FILE_KIND_CONSOLE    23

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

// ExecBase offsets for task list checking
#define EXECBASE_THISTAK     276
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
    DIR *dir;           /* For directory iteration */
    int refcount;       /* For DupLock */
    bool is_dir;        /* True if lock is on a directory */
} lock_entry_t;

static lock_entry_t g_locks[MAX_LOCKS];

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

static inline uint8_t mread8 (uint32_t address)
{
    static bool startup = TRUE;

    if (!address)
    {
        if (startup)
        {
            startup = FALSE;
        }
        else
        {
            DPRINTF (LOG_ERROR, "lxa: mread8 illegal read from addr 0 detected\n");
            _debug(m68k_get_reg(NULL, M68K_REG_PC));
        }
    }

    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return g_ram[addr];
    }
    else
    {
        if ((address >= ROM_START) && (address <= ROM_END))
        {
            uint32_t addr = address - ROM_START;
            return g_rom[addr];
        }
        else
        {
            printf("ERROR: mread8 at invalid address 0x%08x\n", address);
            _debug(m68k_get_reg(NULL, M68K_REG_PC));
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
        default:
            printf("ERROR: _handle_custom_write: unsupport chip reg 0x%03x\n", reg);
            assert (false);
    }
}

static inline void mwrite8 (uint32_t address, uint8_t value)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        g_ram[addr] = value;
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

    if (g_trace)
    {
        static char buff[100];
        static char buff2[100];
        static unsigned int instr_size;
        print68kstate(LOG_DEBUG);
        instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
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

static char *_mgetstr (uint32_t address)
{
    if ((address >= RAM_START) && (address <= RAM_END))
    {
        uint32_t addr = address - RAM_START;
        return (char *) &g_ram[addr];
    }
    else
    {
        if ((address >= ROM_START) && (address <= ROM_END))
        {
            uint32_t addr = address - ROM_START;
            return (char *) &g_rom[addr];
        }
        else
        {
            printf("ERROR: _mgetstr at invalid address 0x%08x\n", address);
            assert (false);
        }
    }
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

static void _dos_stdinout_fh (uint32_t fh68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_stdinout_fh(): fh68k=0x%08x\n", fh68k);
    m68k_write_memory_32 (fh68k+32, FILE_KIND_CONSOLE); // fh_Func3
    m68k_write_memory_32 (fh68k+36, STDOUT_FILENO);     // fh_Args
}

static int _dos_open (uint32_t path68k, uint32_t accessMode, uint32_t fh68k)
{
    char *amiga_path = _mgetstr(path68k);
    char lxpath[PATH_MAX];

    DPRINTF (LOG_DEBUG, "lxa: _dos_open(): amiga_path=%s\n", amiga_path);

    if (!strncasecmp (amiga_path, "CONSOLE:", 8) || (amiga_path[0]=='*'))
    {
        _dos_stdinout_fh (fh68k);
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

#define CSI_BUF_LEN 32
#define CSI "\e["

static int _dos_write (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    DPRINTF (LOG_DEBUG, "lxa: _dos_write(): fh68k=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

    int fd   = m68k_read_memory_32 (fh68k+36);
    int kind = m68k_read_memory_32 (fh68k+32);
    DPRINTF (LOG_DEBUG, "                  -> fd=%d, kind=%d\n", fd, kind);

    char *buf = _mgetstr (buf68k);
    ssize_t l = 0;

    switch (kind)
    {
        case FILE_KIND_REGULAR:
        {
            l = write (fd, buf, len68k);

            if (l<0)
                m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

            break;
        }
        case FILE_KIND_CONSOLE:
        {
            static bool     bCSI = FALSE;
            static char     csiBuf[CSI_BUF_LEN];
            static uint16_t csiBufLen=0;

            l = len68k;

            for (int i =0; i<l; i++)
            {
                char c = buf[i];

                uint8_t uc = (uint8_t) c;
                if (!bCSI)
                {
                    if (uc==0x9b)
                    {
                        bCSI = TRUE;
                        continue;
                    }
                }
                else
                {
                    /*
                    0x30–0x3F (ASCII 0–9:;<=>?)                  parameter bytes
                    0x20–0x2F (ASCII space and !\"#$%&'()*+,-./) intermediate bytes
                    0x40–0x7E (ASCII @A–Z[\]^_`a–z{|}~)          final byte
                    */
                    if (uc>=0x40)
                    {
                        //printf ("CSI seq detected: %s%c csiBufLen=%d\n", csiBuf, c, csiBufLen);

                        switch (c)
                        {
                            case 'p': // csr on/off
                                if (csiBufLen == 2)
                                {
                                    switch (csiBuf[0])
                                    {
                                        case '0':
                                            fputs(CSI "?25l", stdout); // cursor invisible
                                            break;
                                        case '1':
                                            fputs(CSI "?25h", stdout);  // cursor visible
                                            break;
                                    }
                                }
                                break;
                            case 'm': // presentation
                                if (csiBufLen == 1)
                                {
                                    switch (csiBuf[0])
                                    {
                                        case '0': fputs(CSI "30m", stdout); /*s2 = UI_TEXT_STYLE_TEXT*/; break;
                                    }
                                }
                                else
                                {
                                    if (csiBufLen == 2)
                                    {
                                        uint8_t color = (csiBuf[0]-'0')*10+(csiBuf[1]-'0');
                                        //printf ("setting color %d\n", color);
                                        switch (color)
                                        {
                                            case 30: fputs (CSI "30m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_0;*/ break;
                                            case 31: fputs (CSI "31m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_1;*/ break;
                                            case 32: fputs (CSI "32m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_2;*/ break;
                                            case 33: fputs (CSI "33m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_3;*/ break;
                                            case 34: fputs (CSI "34m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_4;*/ break;
                                            case 35: fputs (CSI "35m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_5;*/ break;
                                            case 36: fputs (CSI "36m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_6;*/ break;
                                            case 37: fputs (CSI "37m", stdout); /*s2 = UI_TEXT_STYLE_ANSI_7;*/ break;
                                        }
                                    }
                                }
                                break;
                        }

                        bCSI = FALSE;
                        csiBufLen = 0;
                    }
                    else
                    {
                        if (csiBufLen<CSI_BUF_LEN)
                            csiBuf[csiBufLen++] = c;
                    }
                    continue;
                }
                //printf ("non-CSI c=0x%02x\n", c);

                fputc (c, stdout);
            }
            break;
        }
        default:
            assert(FALSE);
    }

    return l;
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
    
    /* Note: Amiga protection bits are inverted - 0 means permission granted */
    /* We set the bit to 0 if permission is granted, 1 if denied */
    
    if (!(mode & S_IRUSR)) prot |= FIBF_READ;
    if (!(mode & S_IWUSR)) prot |= FIBF_WRITE;
    if (!(mode & S_IXUSR)) prot |= FIBF_EXECUTE;
    
    /* Amiga also has archive and script flags - leave them as 0 (granted) */
    
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
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock(): amiga_path=%s, mode=%d\n", amiga_path, mode);
    
    /* Resolve the Amiga path to Linux path */
    if (!vfs_resolve_path(amiga_path, linux_path, sizeof(linux_path))) {
        /* Try legacy fallback */
        _dos_path2linux(amiga_path, linux_path, sizeof(linux_path));
    }
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock(): linux_path=%s\n", linux_path);
    
    /* Check if the path exists */
    struct stat st;
    if (stat(linux_path, &st) != 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock(): stat failed: %s\n", strerror(errno));
        return 0; /* ERROR_OBJECT_NOT_FOUND will be set by caller */
    }
    
    /* Allocate a lock entry */
    int lock_id = _lock_alloc();
    if (lock_id == 0) {
        DPRINTF(LOG_DEBUG, "lxa: _dos_lock(): no free lock slots\n");
        return 0;
    }
    
    lock_entry_t *lock = &g_locks[lock_id];
    strncpy(lock->linux_path, linux_path, sizeof(lock->linux_path) - 1);
    lock->is_dir = S_ISDIR(st.st_mode);
    lock->dir = NULL;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_lock(): allocated lock_id=%d, is_dir=%d\n", lock_id, lock->is_dir);
    
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
    
    /* Write filename (BSTR format - length byte followed by string) */
    int namelen = strlen(filename);
    if (namelen > 106) namelen = 106;
    m68k_write_memory_8(fib68k + FIB_fib_FileName, namelen);
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + 1 + i, filename[i]);
    }
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_time_to_datestamp(st.st_mtime, fib68k + FIB_fib_Date);
    
    /* No comment support on Linux filesystem */
    m68k_write_memory_8(fib68k + FIB_fib_Comment, 0);
    
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
    
    /* Write filename (BSTR format) */
    int namelen = strlen(de->d_name);
    if (namelen > 106) namelen = 106;
    m68k_write_memory_8(fib68k + FIB_fib_FileName, namelen);
    for (int i = 0; i < namelen; i++) {
        m68k_write_memory_8(fib68k + FIB_fib_FileName + 1 + i, de->d_name[i]);
    }
    
    m68k_write_memory_32(fib68k + FIB_fib_Protection, _unix_mode_to_amiga(st.st_mode));
    m68k_write_memory_32(fib68k + FIB_fib_Size, st.st_size);
    m68k_write_memory_32(fib68k + FIB_fib_NumBlocks, (st.st_size + 511) / 512);
    
    _unix_time_to_datestamp(st.st_mtime, fib68k + FIB_fib_Date);
    
    m68k_write_memory_8(fib68k + FIB_fib_Comment, 0);
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
    
    /* Find parent path */
    char parent_path[PATH_MAX];
    strncpy(parent_path, lock->linux_path, sizeof(parent_path) - 1);
    
    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash || last_slash == parent_path) {
        /* Already at root */
        return 0;
    }
    *last_slash = '\0';
    
    /* Create lock for parent */
    int new_lock_id = _lock_alloc();
    if (new_lock_id == 0) return 0;
    
    lock_entry_t *new_lock = &g_locks[new_lock_id];
    strncpy(new_lock->linux_path, parent_path, sizeof(new_lock->linux_path) - 1);
    new_lock->is_dir = true;
    new_lock->dir = NULL;
    
    DPRINTF(LOG_DEBUG, "lxa: _dos_parentdir(): parent=%s, new_lock_id=%d\n", parent_path, new_lock_id);
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
    
    /* For now, just return the linux path - ideally we'd convert back to Amiga format */
    const char *path = lock->linux_path;
    size_t len = strlen(path);
    if (len >= buflen) len = buflen - 1;
    
    for (size_t i = 0; i < len; i++) {
        m68k_write_memory_8(buf68k + i, path[i]);
    }
    m68k_write_memory_8(buf68k + len, '\0');
    
    return 1;
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
            CPRINTF ("*** EMU_CALL_STOP called, rv=%d\n", g_rv);
            
            /*
             * Check if there are other tasks running. If so, we need to
             * properly terminate the current task via RemTask(NULL) and
             * let the scheduler continue running other tasks.
             * 
             * If no other tasks are running, we can exit immediately.
             */
            if (other_tasks_running())
            {
                CPRINTF ("*** other tasks running, calling RemTask(NULL) instead of stopping\n");
                
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
                default: DPRINTF (LOG_INFO, "???\n"); break;
            }

            hexdump (LOG_INFO, isp, 16);

            _debug(pc);

            m68k_end_timeslice();
            g_running = FALSE;
            break;
        }

        case EMU_CALL_WAIT:
            usleep (10000);
            break;

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

        case EMU_CALL_LOADED:
        {
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
            _dos_stdinout_fh (fh);
            break;
        }

        case EMU_CALL_DOS_OUTPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            DPRINTF (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OUTPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh);
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

    uint32_t instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
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
                uint32_t instr_size = m68k_disassemble(buff, pcFinal, M68K_CPU_TYPE_68000);
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
    fprintf(stderr, "usage: %s [ options ] <loadfile>\n", argv[0]);
    fprintf(stderr, "    -b <addr|sym>  add breakpoint, examples:\n");
    fprintf(stderr, "                     b _start\n");
    fprintf(stderr, "                     b __acs_main\n");
    fprintf(stderr, "    -c <config>    use config file, default: ~/.lxa/config.ini\n");
    fprintf(stderr, "    -d             enable debug output\n");
    fprintf(stderr, "    -r <rom>       use kickstart <rom>, default: %s\n", DEFAULT_ROM_PATH);
    fprintf(stderr, "    -s <sysroot>   set AmigaOS system root, default: %s\n", DEFAULT_AMIGA_SYSROOT);
    fprintf(stderr, "    -v             verbose\n");
    fprintf(stderr, "    -t             trace\n");
    //fprintf(stderr, "    -S             print symtab\n");
}

int main(int argc, char **argv, char **envp)
{
    char *rom_path = NULL;
    char *sysroot = NULL;
    char *config_path = NULL;
    int optind=0;

    vfs_init();

    // argument parsing
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++)
    {
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
            case 'r':
            {
                optind++;
                rom_path = argv[optind];
                break;
            }
            case 's':
            {
                optind++;
                sysroot = argv[optind];
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
     * If no config file is specified and no sysroot is set, check if
     * ~/.lxa exists. If not, create it with the default structure.
     */
    if (!config_path && !sysroot) {
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

    if (sysroot) {
        vfs_add_drive("SYS", sysroot);
    } else if (!vfs_has_sys_drive()) {
        /* No SYS: drive configured - use default */
        vfs_add_drive("SYS", DEFAULT_AMIGA_SYSROOT);
    }

    if (!rom_path) {
        rom_path = (char *)config_get_rom_path();
        if (!rom_path) rom_path = DEFAULT_ROM_PATH;
    }

    if (argc != optind+1)
    {
        print_usage(argv);
        exit(EXIT_FAILURE);
    }

    g_loadfile = argv[optind];
    g_sysroot = sysroot ? sysroot : DEFAULT_AMIGA_SYSROOT;

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
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();

    while (TRUE)
    {
        m68k_set_irq(0);
        m68k_execute(100000);
        if (!g_running)
            break;
        if ( (g_intena & INTENA_MASTER) && (g_intena & INTENA_VBLANK))
        {
            //DPRINTF (LOG_DEBUG, "lxa: triggering IRQ #3...\n");
            m68k_set_irq(3);
        }
        m68k_execute(100000);
        if (!g_running)
            break;
    }

    return g_rv;
}
