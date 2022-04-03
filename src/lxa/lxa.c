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
#include <linux/limits.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "m68k.h"

#define DEBUG       true
#define LXA_LOG_FILENAME "lxa.log"
#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_ERROR 2

#define dprintf(lvl, ...) do { if (lvl || (DEBUG && g_debug)) printf(__VA_ARGS__); fprintf (g_logf, __VA_ARGS__); fflush (g_logf);} while (0)

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define ROM_PATH    "../rom/my.rom"
#define ROM_SIZE    256 * 1024
#define ROM_START   0xf80000
#define ROM_END     ROM_START + ROM_SIZE - 1

#define CUSTOM_START 0xdff000
#define CUSTOM_END   0xdfffff

#define CUSTOM_REG_INTENA   0x09a

#define EMU_CALL_LPUTC         1
#define EMU_CALL_STOP          2
#define EMU_CALL_TRACE         3
#define EMU_CALL_LPUTS         4
#define EMU_CALL_EXCEPTION     5
#define EMU_CALL_WAIT          6
#define EMU_CALL_MONITOR       8
#define EMU_CALL_LOADFILE      9
#define EMU_CALL_DOS_OPEN   1000
#define EMU_CALL_DOS_READ   1001
#define EMU_CALL_DOS_SEEK   1002
#define EMU_CALL_DOS_CLOSE  1003
#define EMU_CALL_DOS_INPUT  1004
#define EMU_CALL_DOS_OUTPUT 1005
#define EMU_CALL_DOS_WRITE  1006

#define AMIGA_SYSROOT "/home/guenter/media/emu/amiga/FS-UAE/hdd/system/"

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
static bool     g_debug                        = FALSE;
static bool     g_verbose                      = FALSE;
static bool     g_trace                        = FALSE;
static bool     g_stepping                     = FALSE;
static int      g_trace_buf[TRACE_BUF_ENTRIES];
static int      g_trace_buf_idx                = 0;
static bool     g_running                      = TRUE;
static FILE    *g_logf                         = NULL;
static char    *g_loadfile                     = NULL;
static uint32_t g_breakpoints[MAX_BREAKPOINTS];
static int      g_num_breakpoints              = 0;

// interrupts
#define INTENA_MASTER 0x4000
#define INTENA_VBLANK 0x0020

static uint16_t g_intena  = INTENA_MASTER | INTENA_VBLANK;

#define ENDIAN_SWAP_16(data) ( (((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00) )
#define ENDIAN_SWAP_32(data) ( (((data) >> 24) & 0x000000FF) | (((data) >>  8) & 0x0000FF00) | \
                               (((data) <<  8) & 0x00FF0000) | (((data) << 24) & 0xFF000000) )
#define ENDIAN_SWAP_64(data) ( (((data) & 0x00000000000000ffLL) << 56) | \
                               (((data) & 0x000000000000ff00LL) << 40) | \
                               (((data) & 0x0000000000ff0000LL) << 24) | \
                               (((data) & 0x00000000ff000000LL) << 8)  | \
                               (((data) & 0x000000ff00000000LL) >> 8)  | \
                               (((data) & 0x0000ff0000000000LL) >> 24) | \
                               (((data) & 0x00ff000000000000LL) >> 40) | \
                               (((data) & 0xff00000000000000LL) >> 56))

static void _debug(uint32_t pc);

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
            dprintf (LOG_ERROR, "lxa: mread8 illegal read from addr 0 detected\n");
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
    //    dprintf(LOG_DEBUG, "READ8   at 0x%08x -> 0x%02x\n", address, b);
    return b;
}

unsigned int m68k_read_memory_16 (unsigned int address)
{
    uint16_t w= (mread8(address)<<8) | mread8(address+1);

    //if (g_trace)
    //    dprintf(LOG_DEBUG, "READ16  at 0x%08x -> 0x%04x\n", address, w);
    return w;
}

unsigned int m68k_read_memory_32 (unsigned int address)
{
    uint32_t l = (mread8(address)<<24) | (mread8(address+1)<<16) | (mread8(address+2)<<8) | mread8(address+3);
    // if (g_trace)
    //     dprintf("READ32  at 0x%08x -> 0x%08x\n", address, l);
    return l;
}

static void _handle_custom_write (uint16_t reg, uint16_t value)
{

    switch (reg)
    {
        case CUSTOM_REG_INTENA:
        {
            dprintf (LOG_DEBUG, "lxa: _handle_custom_write: INTENA value=0x%04x\n", value);

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

            dprintf (LOG_DEBUG, "lxa: _handle_custom_write -> INTENA=0x%04x\n", g_intena);

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
        assert (false);
    }
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    // if (g_trace)
    //     dprintf("WRITE8  at 0x%08x -> 0x%02x\n", address, value);
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
    //     dprintf("WRITE16 at 0x%08x -> 0x%04x\n", address, value);
    mwrite8 (address  , (value >> 8) & 0xff);
    mwrite8 (address+1, value & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    // if (g_trace)
    //     dprintf("WRITE32 at 0x%08x -> 0x%08x\n", address, value);
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
            dprintf(lvl, "0x%06x: ", offset+i);
        }

        /* print hex data */
        if (i < len)
        {
            char c = m68k_read_memory_8 (offset + i);
            dprintf(lvl, "%02x ", 0xFF & c);
        }
        else /* end of block, just aligning for ASCII dump */
        {
            dprintf(lvl, "   ");
        }

        /* print ASCII dump */
        if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
        {
            for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
            {
                char c = m68k_read_memory_8 (offset + j);

                if (j >= len) /* end of block, not really printing */
                {
                    dprintf(lvl, " ");
                }
                else if (isprintable(c)) /* printable char */
                {
                    dprintf(lvl, "%c", c);
                }
                else /* other char */
                {
                    dprintf(lvl, ".");
                }
            }
            dprintf(lvl, "\n");
        }
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

    dprintf (lvl, "     pc=0x%08x sr=0x%04x\n     d0=0x%08x d1=0x%08x d2=0x%08x d3=0x%08x d4=0x%08x d5=0x%08x d6=0x%08x d7=0x%08x\n",
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
    dprintf (lvl, "     a0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x a6=0x%08x a7=0x%08x\n     usp=0x%08x isp=0x%08x msp=0x%08x\n",
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
        dprintf(LOG_DEBUG, "E %08x: %-20s: %s\n", pc, buff2, buff);
    }

    if (g_stepping)
    {
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
    // FIXME: pretty hard coded, for now
    if (!strncasecmp (amiga_path, "NIL:", 4))
        snprintf (linux_path, buf_len, "/dev/null");
    else
        snprintf (linux_path, buf_len, "%s/%s", AMIGA_SYSROOT, amiga_path);
}

static int errno2Amiga (void)
{
    // FIXME: do actual mapping!
    return errno;
}

static void _dos_stdinout_fh (uint32_t fh68k)
{
    dprintf (LOG_DEBUG, "lxa: _dos_stdinout_fh(): fh68k=0x%08x\n", fh68k);
    m68k_write_memory_32 (fh68k+36, STDOUT_FILENO);     // fh_Args
    m68k_write_memory_32 (fh68k+40, FILE_KIND_CONSOLE); // fh_Arg2
}

static int _dos_open (uint32_t path68k, uint32_t accessMode, uint32_t fh68k)
{
    char *amiga_path = _mgetstr(path68k);
    char lxpath[PATH_MAX];

    dprintf (LOG_DEBUG, "lxa: _dos_open(): amiga_path=%s\n", amiga_path);

    if (!strncasecmp (amiga_path, "CONSOLE:", 8) || (amiga_path[0]=='*'))
    {
        _dos_stdinout_fh (fh68k);
    }
    else
    {
        // FIXME: amiga paths are case insensitive!
        _dos_path2linux (amiga_path, lxpath, PATH_MAX);
        dprintf (LOG_DEBUG, "lxa: _dos_open(): lxpath=%s\n", lxpath);

        int flags = 0;
        mode_t mode = 0644;

        switch (accessMode)
        {
            case MODE_NEWFILE:
                flags = O_CREAT | O_TRUNC | O_RDWR;
                break;
            case MODE_OLDFILE:
                flags = O_RDWR;
                break;
            case MODE_READWRITE:
                flags = O_CREAT | O_RDWR;
                break;
            default:
                assert(FALSE);
        }

        int fd = open (lxpath, flags, mode);
        int err = errno;

        dprintf (LOG_DEBUG, "lxa: _dos_open(): open() result: fd=%d, err=%d\n", fd, err);

        m68k_write_memory_32 (fh68k+36, fd);                // fh_Args
        m68k_write_memory_32 (fh68k+40, FILE_KIND_REGULAR); // fh_Arg2
    }

	return errno2Amiga();
}

static int _dos_read (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    dprintf (LOG_DEBUG, "lxa: _dos_read(): fh=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

	int fd = m68k_read_memory_32 (fh68k+36);
    dprintf (LOG_DEBUG, "                  -> fd = %d\n", fd);

    void *buf = _mgetstr (buf68k);

    ssize_t l = read (fd, buf, len68k);

    if (l<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return l;
}

static int _dos_seek (uint32_t fh68k, int32_t position, int32_t mode)
{
    dprintf (LOG_DEBUG, "lxa: _dos_seek(): fh=0x%08x, position=0x%08x, mode=%d\n", fh68k, position, mode);

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

static int _dos_write (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    dprintf (LOG_DEBUG, "lxa: _dos_write(): fh68k=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

	int fd   = m68k_read_memory_32 (fh68k+36);
	int kind = m68k_read_memory_32 (fh68k+40);
    dprintf (LOG_DEBUG, "                  -> fd=%d, kind=%d\n", fd, kind);

    void *buf = _mgetstr (buf68k);

    ssize_t l = write (fd, buf, len68k);

    if (l<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return l;
}

static int _dos_close (uint32_t fh68k)
{
    dprintf (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x\n", fh68k);

	int fd = m68k_read_memory_32 (fh68k+36);

    close(fd);

    dprintf (LOG_DEBUG, "lxa: _dos_close(): fh=0x%08x done\n", fh68k);
    return 1;
}

int op_illg(int level)
{
    uint32_t d0 = m68k_get_reg(NULL, M68K_REG_D0);
	//dprintf ("ILLEGAL, d=%d\n", d0);

    switch (d0)
    {
        case EMU_CALL_LPUTC:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t ch  = m68k_get_reg(NULL, M68K_REG_D2);

            fprintf (g_logf, "%c", ch);
            if (ch==10)
                fflush (g_logf);

            if (lvl || g_debug)
            {
                printf ("%c", ch); fflush (stdout);
            }

            break;
        }

        case EMU_CALL_LPUTS:
        {
            uint32_t lvl = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t s68k = m68k_get_reg(NULL, M68K_REG_D2);

            char *s = _mgetstr (s68k);

            fprintf (g_logf, "%s", s);
            fflush (g_logf);

            if (lvl || g_debug)
            {
                printf ("%s", s); fflush (stdout);
            }

            break;
        }

        case EMU_CALL_STOP:
        {
            uint32_t rv = m68k_get_reg(NULL, M68K_REG_D1);
            if (rv)
                dprintf (LOG_ERROR, "*** emulator stop via lxcall, rv=%d\n", rv);
            else
                dprintf (LOG_DEBUG, "*** emulator stop via lxcall.\n");
            m68k_end_timeslice();
            g_running = FALSE;
            break;
        }

        case EMU_CALL_TRACE:
            g_trace = m68k_get_reg(NULL, M68K_REG_D1);
            dprintf (LOG_DEBUG, "set emulator tracing to %d\n", g_trace);
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

            dprintf (LOG_INFO, "*** EXCEPTION CAUGHT: pc=0x%08x #%2d ", pc, excn);

            switch (excn)
            {
                case  2: dprintf (LOG_INFO, "bus error\n"); break;
                case  3: dprintf (LOG_INFO, "address error\n"); break;
                case  4: dprintf (LOG_INFO, "illegal instruction\n"); break;
                case  5: dprintf (LOG_INFO, "divide by zero\n"); break;
                case  6: dprintf (LOG_INFO, "chk instruction\n"); break;
                case  7: dprintf (LOG_INFO, "trapv instruction\n"); break;
                case  8: dprintf (LOG_INFO, "privilege violation\n"); break;
                case  9: dprintf (LOG_INFO, "trace\n"); break;
                case 10: dprintf (LOG_INFO, "line 1010 emulator\n"); break;
                case 11: dprintf (LOG_INFO, "line 1111 emulator\n"); break;
                default: dprintf (LOG_INFO, "???\n"); break;
            }

            hexdump (LOG_INFO, isp, 16);

            _debug(pc);

            m68k_end_timeslice();
            g_running = FALSE;
            break;
        }

        case EMU_CALL_MONITOR:
        {
            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_MONITOR\n");

            _debug(m68k_get_reg(NULL, M68K_REG_PC));

            break;
        }

        case EMU_CALL_LOADFILE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_LOADFILE d1=0x%08x\n", d1);

            for (int i=0; i<=strlen(g_loadfile); i++)
                m68k_write_memory_8 (d1++, g_loadfile[i]);

            break;
        }

        case EMU_CALL_DOS_OPEN:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OPEN name=0x%08x, accessMode=0x%08x, fh=0x%08x\n", d1, d2, d3);

            uint32_t res = _dos_open (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_READ:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_READ file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_read (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_SEEK:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_SEEK file=0x%08x, position=0x%08x, mode=%d\n", d1, d2, d3);

            uint32_t res = _dos_seek (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_CLOSE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_CLOSE file=0x%08x\n", d1);

            uint32_t res = _dos_close (d1);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_INPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_INPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh);
            break;
        }

        case EMU_CALL_DOS_OUTPUT:
        {
            uint32_t fh = m68k_get_reg(NULL, M68K_REG_D1);
            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_OUTPUT, fh=0x%08x\n", fh);
            _dos_stdinout_fh (fh);
            break;
        }

        case EMU_CALL_DOS_WRITE:
        {
            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE\n");

            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf (LOG_DEBUG, "lxa: op_illg(): EMU_CALL_DOS_WRITE file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_write (d1, d2, d3);

            //_debug(m68k_get_reg(NULL, M68K_REG_PC));

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        default:
            dprintf (LOG_INFO, "*** error: undefined lxcall #%d\n", d0);
            _debug(m68k_get_reg(NULL, M68K_REG_PC));
            m68k_end_timeslice();
            g_running = FALSE;
    }

	return M68K_INT_ACK_AUTOVECTOR;
}

static void _debug_traceback (int n, uint32_t pcFinal)
{
    // dump last n instructions from trace buf:
    uint32_t pc;
    unsigned int instr_size;
    static char buff[100];
    static char buff2[100];

    dprintf (LOG_INFO, "Traceback:\n\n");

    n--;

    if (n>TRACE_BUF_ENTRIES)
        n = TRACE_BUF_ENTRIES;

    int idx = g_trace_buf_idx - n ;
    if (idx<0) idx += TRACE_BUF_ENTRIES;
    for (int i = 0; i<n; i++)
    {
        pc = g_trace_buf[idx];
        instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
        make_hex(buff2, pc, instr_size);
        dprintf(LOG_INFO, "     0x%08x  %-20s: %s\n", pc, buff2, buff);
        idx = (idx+1) % TRACE_BUF_ENTRIES;
    }

    pc = pcFinal;
    instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
    make_hex(buff2, pc, instr_size);
    dprintf(LOG_INFO, "---> 0x%08x  %-20s: %s\n\n", pc, buff2, buff);
}

static void _debug_machine_state (void)
{
    dprintf (LOG_INFO, "Machine state:\n\n");
    print68kstate(LOG_INFO);
    dprintf (LOG_INFO, "\n");
}

static void _debug_memdump (uint32_t addr)
{
    dprintf (LOG_INFO, "Memory dump:\n\n");

    hexdump (LOG_INFO, addr, 256);

    dprintf (LOG_INFO, "\n");
}

static void _debug_help (void)
{
    dprintf (LOG_INFO, "Available commands:\n\n");
    dprintf (LOG_INFO, "h        - this help screen\n");
    dprintf (LOG_INFO, "c        - continue\n");
    dprintf (LOG_INFO, "r        - registers\n");
    dprintf (LOG_INFO, "s        - step\n");
    dprintf (LOG_INFO, "t <num>  - traceback\n");
    dprintf (LOG_INFO, "m <addr> - memory dump\n");
    dprintf (LOG_INFO, "\n");
}

static void _debug(uint32_t pcFinal)
{
    static bool in_debug = FALSE;

    if (in_debug)
        return;
    in_debug = TRUE;

    dprintf(LOG_INFO, "\n\n     *** LXA DEBUGGER ***\n\n");

    _debug_traceback (5, pcFinal);

    _debug_machine_state ();

    // interactive debugger loop

    char* buf;
    while ((buf = readline(">> ")))
    {
        int l = strlen(buf);
        if (!l)
            continue;

        add_history(buf);
        dprintf (LOG_INFO, "\n");

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
                    dprintf (LOG_INFO, "???\n");
                break;
            }
            case 'm':
            {
                uint32_t addr;
                int n = sscanf (buf, "m %x", &addr);
                if (n==1)
                    _debug_memdump (addr);
                else
                    dprintf (LOG_INFO, "???\n");
                break;
            }
        }

        free(buf);
    }
    assert(FALSE);
}

static void print_usage(char *argv[])
{
    fprintf(stderr, "usage: %s [ options ] <loadfile>\n", argv[0]);
    fprintf(stderr, "    -b <addr>  add breakpoint\n");
    fprintf(stderr, "    -d         enable debug output\n");
    fprintf(stderr, "    -v         verbose\n");
}

int main(int argc, char **argv, char **envp)
{
	int optind=0;
    // argument parsing
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++)
    {
        switch (argv[optind][1])
        {
            case 'b':
            {
                uint32_t addr;
                optind++;
                if (sscanf (argv[optind], "%x", &addr) != 1)
                {
                    print_usage(argv);
                    exit(EXIT_FAILURE);
                }
                if (g_num_breakpoints < MAX_BREAKPOINTS)
                    g_breakpoints[g_num_breakpoints++] = addr;
                break;
            }
            case 'd':
				g_debug = true;
                break;
            case 'v':
				g_verbose = true;
                break;
			default:
                print_usage(argv);
                exit(EXIT_FAILURE);
        }
    }
	if (argc != optind+1)
	{
		print_usage(argv);
		exit(EXIT_FAILURE);
	}

    g_loadfile = argv[optind];

    // logging
    g_logf = fopen (LXA_LOG_FILENAME, "w");
    assert (g_logf);

    dprintf (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       ROM_START          = 0x%08x\n", ROM_START         );
    dprintf (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       ROM_END            = 0x%08x\n", ROM_END           );
    dprintf (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       RAM_START          = 0x%08x\n", RAM_START         );
    dprintf (g_verbose ? LOG_INFO : LOG_DEBUG, "lxa:       RAM_END            = 0x%08x\n", RAM_END           );

    // load rom code
    FILE *romf = fopen (ROM_PATH, "r");
    if (!romf)
    {
        fprintf (stderr, "failed to open %s\n", ROM_PATH);
        exit(1);
    }
    if (fread (g_rom, ROM_SIZE, 1, romf) != 1)
    {
        fprintf (stderr, "failed to read from %s\n", ROM_PATH);
        exit(2);
    }
    fclose(romf);

    // setup memory image

    m68k_write_memory_32 (0, RAM_END-1);   // initial SP
    m68k_write_memory_32 (4, ROM_START+2); // reset vector

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
            dprintf (LOG_DEBUG, "lxa: triggering IRQ #3...\n");
            m68k_set_irq(3);
        }
        m68k_execute(100000);
        if (!g_running)
            break;
    }

	return 0;
}
