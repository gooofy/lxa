#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>

#include "m68k.h"

#define DEBUG       true
#define LXA_LOG_FILENAME "lxa.log"
#define dprintf(...) do { if (DEBUG && g_debug) printf(__VA_ARGS__); } while (0)

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define ROM_PATH    "../rom/my.rom"
#define ROM_SIZE    256 * 1024
#define ROM_START   0xf80000
#define ROM_END     ROM_START + ROM_SIZE - 1

#define EMU_CALL_LPUTC         1
#define EMU_CALL_STOP          2
#define EMU_CALL_DOS_OPEN   1000
#define EMU_CALL_DOS_READ   1001
#define EMU_CALL_DOS_SEEK   1002
#define EMU_CALL_DOS_CLOSE  1003

#define AMIGA_SYSROOT "/home/guenter/media/emu/amiga/FS-UAE/hdd/system/"

#define MODE_OLDFILE        1005
#define MODE_NEWFILE        1006
#define MODE_READWRITE      1004

#define OFFSET_BEGINNING    -1
#define OFFSET_CURRENT       0
#define OFFSET_END           1

static uint8_t g_ram[RAM_SIZE];
static uint8_t g_rom[ROM_SIZE];
static bool    g_debug   = false;
static bool    g_running = true;
static FILE   *g_logf    = NULL;

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
#if 0
static void hexdump (uint32_t offset, uint32_t num_bytes)
{
    dprintf ("HEX: 0x%08x  ", offset);
    uint32_t cnt=0;
    uint32_t num_longs = num_bytes >> 2;
    while (cnt<num_longs)
    {
        uint32_t l = lpeek(offset);
        dprintf (" 0x%08x", l);
		offset +=4;
        cnt++;
    }
    dprintf ("\n");
}
#endif

static inline uint8_t mread8 (uint32_t address)
{
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
            assert (false);
        }
    }
}

unsigned int m68k_read_memory_8 (unsigned int address)
{
    uint8_t b = mread8(address);
    //dprintf("READ8   at 0x%08x -> 0x%02x\n", address, b);
    return b;
}

unsigned int m68k_read_memory_16 (unsigned int address)
{
    uint16_t w= (mread8(address)<<8) | mread8(address+1);

    //char buff[100];
    //m68k_disassemble(buff, address, M68K_CPU_TYPE_68000);
    //dprintf("READ16  at 0x%08x -> 0x%04x\n", address, w);
    return w;
}

unsigned int m68k_read_memory_32 (unsigned int address)
{
    uint32_t l = (mread8(address)<<24) | (mread8(address+1)<<16) | (mread8(address+2)<<8) | mread8(address+3);
    //dprintf("READ32  at 0x%08x -> 0x%08x\n", address, l);
    return l;
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
    //dprintf("WRITE8  at 0x%08x -> 0x%02x\n", address, value);
    mwrite8 (address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
   // dprintf("WRITE16 at 0x%08x -> 0x%04x\n", address, value);
    mwrite8 (address  , (value >> 8) & 0xff);
    mwrite8 (address+1, value & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    //dprintf("WRITE32 at 0x%08x -> 0x%08x\n", address, value);
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

#if 0
static void print68kstate(void)
{
    uint32_t d0,d1,d2,d3,d4,d5,d6,d7;

    d0 = m68k_get_reg (NULL, M68K_REG_D0);
    d1 = m68k_get_reg (NULL, M68K_REG_D1);
    d2 = m68k_get_reg (NULL, M68K_REG_D2);
    d3 = m68k_get_reg (NULL, M68K_REG_D3);
    d4 = m68k_get_reg (NULL, M68K_REG_D4);
    d5 = m68k_get_reg (NULL, M68K_REG_D5);
    d6 = m68k_get_reg (NULL, M68K_REG_D6);
    d7 = m68k_get_reg (NULL, M68K_REG_D7);

    dprintf ("      d0=0x%08x d1=0x%08x d2=0x%08x d3=0x%08x d4=0x%08x d5=0x%08x d6=0x%08x d7=0x%08x\n", d0, d1, d2, d3, d4, d5, d6, d7);
    uint32_t a0,a1,a2,a3,a4,a5,a6,a7;
    a0 = m68k_get_reg (NULL, M68K_REG_A0);
    a1 = m68k_get_reg (NULL, M68K_REG_A1);
    a2 = m68k_get_reg (NULL, M68K_REG_A2);
    a3 = m68k_get_reg (NULL, M68K_REG_A3);
    a4 = m68k_get_reg (NULL, M68K_REG_A4);
    a5 = m68k_get_reg (NULL, M68K_REG_A5);
    a6 = m68k_get_reg (NULL, M68K_REG_A6);
    a7 = m68k_get_reg (NULL, M68K_REG_A7);
    dprintf ("      a0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x a6=0x%08x a7=0x%08x\n", a0, a1, a2, a3, a4, a5, a6, a7);
#if 0
    uint32_t sr, pc;
    uc_reg_read(g_uc, UC_M68K_REG_SR, &sr);
    uc_reg_read(g_uc, UC_M68K_REG_PC, &pc);
    dprintf ("      sr = 0x%08x ", sr);
    if (sr &     1) dprintf ("C") ; else dprintf (".");
    if (sr &     2) dprintf ("V") ; else dprintf (".");
    if (sr &     4) dprintf ("Z") ; else dprintf (".");
    if (sr &     8) dprintf ("N") ; else dprintf (".");
    if (sr &    16) dprintf ("X") ; else dprintf (".");
    if (sr &  8192) dprintf ("S") ; else dprintf (".");
    if (sr & 32768) dprintf ("T") ; else dprintf (".");
    dprintf (" pc=0x%08x\n", pc);
    sr &= 0xdfff;
    uc_reg_write(g_uc, UC_M68K_REG_SR, &sr);
#endif
}
#endif

void cpu_instr_callback(int pc)
{
    if (DEBUG && g_debug)
    {
        static char buff[100];
        static char buff2[100];
        //static unsigned int pc;
        static unsigned int instr_size;
        //pc = m68k_get_reg(NULL, M68K_REG_PC);
        instr_size = m68k_disassemble(buff, pc, M68K_CPU_TYPE_68000);
        make_hex(buff2, pc, instr_size);
        //printf("E %03x: %-20s: %s\n", pc, buff2, buff);
        //print68kstate();
        //fflush(stdout);
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
    snprintf (linux_path, buf_len, "%s/%s", AMIGA_SYSROOT, amiga_path);
}

static int errno2Amiga (void)
{
    // FIXME: do actual mapping!
    return errno;
}

static int _dos_open (uint32_t path68k, uint32_t accessMode, uint32_t fh68k)
{
    char *amiga_path = _mgetstr(path68k);
    char lxpath[PATH_MAX];

    dprintf ("lxa: _dos_open(): amiga_path=%s\n", amiga_path);

    _dos_path2linux (amiga_path, lxpath, PATH_MAX);
    dprintf ("lxa: _dos_open(): lxpath=%s\n", lxpath);

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

    dprintf ("lxa: _dos_open(): open() result: fd=%d, err=%d\n", fd, err);

	m68k_write_memory_32 (fh68k+36, fd); // fh_Args

	return errno2Amiga();
}

static int _dos_read (uint32_t fh68k, uint32_t buf68k, uint32_t len68k)
{
    dprintf ("lxa: _dos_read(): fh=0x%08x, buf68k=0x%08x, len68k=%d\n", fh68k, buf68k, len68k);

	int fd = m68k_read_memory_32 (fh68k+36);
    dprintf ("                  -> fd = %d\n", fd);

    void *buf = _mgetstr (buf68k);

    ssize_t l = read (fd, buf, len68k);

    if (l<0)
        m68k_write_memory_32 (fh68k+40, errno2Amiga()); // fh_Arg2

    return l;
}

static int _dos_seek (uint32_t fh68k, int32_t position, int32_t mode)
{
    dprintf ("lxa: _dos_seek(): fh=0x%08x, position=0x%08x, mode=%d\n", fh68k, position, mode);

	int fd = m68k_read_memory_32 (fh68k+36);

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

static int _dos_close (uint32_t fh68k)
{
    dprintf ("lxa: _dos_close(): fh=0x%08x\n", fh68k);

	int fd = m68k_read_memory_32 (fh68k+36);

    close(fd);

    dprintf ("lxa: _dos_close(): fh=0x%08x done\n", fh68k);
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

            if (lvl || g_debug)
            {
                printf ("%c", ch); fflush (stdout);
            }

            break;
        }

        case EMU_CALL_STOP:
            printf ("*** emulator stop via lxcall.\n");
            m68k_end_timeslice();
            g_running = FALSE;
            break;

        case EMU_CALL_DOS_OPEN:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf ("lxa: op_illg(): EMU_CALL_DOS_OPEN name=0x%08x, accessMode=0x%08x, fh=0x%08x\n", d1, d2, d3);

            uint32_t res = _dos_open (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_READ:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf ("lxa: op_illg(): EMU_CALL_DOS_READ file=0x%08x, buffer=0x%08x, len=%d\n", d1, d2, d3);

            uint32_t res = _dos_read (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_SEEK:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            uint32_t d2 = m68k_get_reg(NULL, M68K_REG_D2);
            uint32_t d3 = m68k_get_reg(NULL, M68K_REG_D3);

            dprintf ("lxa: op_illg(): EMU_CALL_DOS_SEEK file=0x%08x, position=0x%08x, mode=%d\n", d1, d2, d3);

            uint32_t res = _dos_seek (d1, d2, d3);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        case EMU_CALL_DOS_CLOSE:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);

            dprintf ("lxa: op_illg(): EMU_CALL_DOS_CLOSE file=0x%08x\n", d1);

            uint32_t res = _dos_close (d1);

            m68k_set_reg(M68K_REG_D0, res);

            break;
        }

        default:
            printf ("*** error: undefined lxcall #%d\n", d0);
            m68k_end_timeslice();
            g_running = FALSE;
    }

	return M68K_INT_ACK_AUTOVECTOR;
}

static void print_usage(char *argv[])
{
    fprintf(stderr, "usage: %s [ options ]\n", argv[0]);
    fprintf(stderr, "    -d enable debug output\n");
}

int main(int argc, char **argv, char **envp)
{
	int optind=0;
    // argument parsing
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++)
    {
        switch (argv[optind][1])
        {
            case 'd':
				g_debug = true;
                break;
			default:
                print_usage(argv);
                exit(EXIT_FAILURE);
        }
    }
	if (argc != optind)
	{
		print_usage(argv);
		exit(EXIT_FAILURE);
	}

    // logging
    g_logf = fopen (LXA_LOG_FILENAME, "w");
    assert (g_logf);

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

    m68k_write_memory_32 (0, RAM_END-1);     // initial SP
    m68k_write_memory_32 (4, ROM_START+2); // reset vector

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();

    while (g_running)
    {
        m68k_execute(1000000);
    }

	return 0;
}
