#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include "m68k.h"

#define DEBUG       true

#define dprintf(...) do { if (DEBUG && g_debug) printf(__VA_ARGS__); } while (0)

#define RAM_START   0x000000
#define RAM_SIZE    10 * 1024 * 1024
#define RAM_END     RAM_START + RAM_SIZE - 1

#define ROM_PATH    "../rom/my.rom"
#define ROM_SIZE    256 * 1024
#define ROM_START   0xf80000
#define ROM_END     ROM_START + ROM_SIZE - 1

static uint8_t g_ram[RAM_SIZE];
static uint8_t g_rom[ROM_SIZE];
static bool    g_debug=false;

#if 0
#define CHIPMEM_ADDRESS 0x000000
#define CHIPMEM_SIZE    2 * 1024 * 1024
#define FASTMEM_ADDRESS 0x200000
#define FASTMEM_SIZE    8 * 1024 * 1024

#define RTC_MATCHWORD 0x4AFC

#define STACK_SIZE	64 * 1024

// code to be emulated
#define M68K_CODE "\x20\x3c\xde\xad\xbe\xef" // move.l #-559038737,d0

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

static uc_engine *g_uc;

#if 0
static uint8_t peek (uint32_t addr)
{
	uint8_t b;
	uc_mem_read (g_uc, addr, &b, 1);
	return b;
}
#endif

static uint16_t dpeek (uint32_t addr)
{
	uint16_t w;
	uc_mem_read (g_uc, addr, &w, 2);
	return ENDIAN_SWAP_16(w);
}

static uint32_t lpeek (uint32_t addr)
{
	uint32_t l;
	uc_mem_read (g_uc, addr, &l, 4);
	return ENDIAN_SWAP_32(l);
}

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



static void hook_lxcall(uc_engine *uc, uint32_t intno, void *user_data)
{
	dprintf ("TRAP  intno=%d\n", intno);
    if (intno == 35)
	{
		uint32_t fn, c;
		uc_reg_read(g_uc, UC_M68K_REG_D0, &fn);

        switch (fn)
        {
            case 1:
            {
                print68kstate(g_uc);
                uc_reg_read(g_uc, UC_M68K_REG_D1, &c);
                printf ("%c", c); fflush (stdout);

                uint32_t r_pc = lpeek(0x00008c);
                uc_reg_read (g_uc, UC_M68K_REG_PC, &r_pc);
                r_pc += 2;
                uc_reg_write(g_uc, UC_M68K_REG_PC, &r_pc);

                break;
            }
            case 2:
                printf ("*** emulator stop via lxcall.\n");
                uc_emu_stop(uc);
                break;
            default:
                printf ("*** error: undefinded lxcall #%d\n", fn);
        }

    }
    if (intno == 61)
    {
        printf ("\n\n *** ERROR: unsupported exception was thrown!\n\n");
        uc_emu_stop(uc);
    }
}

static void hook_mem(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data)
{
    switch(type) {
        default: break;
        case UC_MEM_READ:
                 dprintf("READ  at 0x%08lx, data size = %u\n", address, size);
                 break;
        case UC_MEM_WRITE:
                 dprintf("WRITE at 0x%08lx, data size = %u, data value = 0x%08lx\n", address, size, value);
                 break;
    }
}

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    dprintf("INSTR at 0x%08lx code=0x%04x\n", address, dpeek(address));
    print68kstate(g_uc);
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
    dprintf("READ8   at 0x%08x -> 0x%02x\n", address, b);
    return b;
}

unsigned int m68k_read_memory_16 (unsigned int address)
{
    uint16_t w= (mread8(address)<<8) | mread8(address+1);

    //char buff[100];
    //m68k_disassemble(buff, address, M68K_CPU_TYPE_68000);
    dprintf("READ16  at 0x%08x -> 0x%04x\n", address, w);
    return w;
}

unsigned int m68k_read_memory_32 (unsigned int address)
{
    uint32_t l = (mread8(address)<<24) | (mread8(address+1)<<16) | (mread8(address+2)<<8) | mread8(address+3);
    dprintf("READ32  at 0x%08x -> 0x%08x\n", address, l);
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
    dprintf("WRITE8  at 0x%08x -> 0x%02x\n", address, value);
    mwrite8 (address, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    dprintf("WRITE16 at 0x%08x -> 0x%04x\n", address, value);
    mwrite8 (address  , (value >> 8) & 0xff);
    mwrite8 (address+1, value & 0xff);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    dprintf("WRITE32 at 0x%08x -> 0x%08x\n", address, value);
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
        printf("E %03x: %-20s: %s\n", pc, buff2, buff);
        print68kstate();
        fflush(stdout);
    }
}

int op_illg(int level)
{
    uint32_t d0 = m68k_get_reg(NULL, M68K_REG_D0);
	dprintf ("ILLEGAL, d=%d\n", d0);

    switch (d0)
    {
        case 1:
        {
            uint32_t d1 = m68k_get_reg(NULL, M68K_REG_D1);
            //print68kstate(g_uc);
            //uc_reg_read(g_uc, UC_M68K_REG_D1, &c);
            printf ("%c", d1); fflush (stdout);

            //uint32_t r_pc = lpeek(0x00008c);
            //uc_reg_read (g_uc, UC_M68K_REG_PC, &r_pc);
            //r_pc += 2;
            //uc_reg_write(g_uc, UC_M68K_REG_PC, &r_pc);

            break;
        }
        case 2:
            printf ("*** emulator stop via lxcall.\n");
            m68k_end_timeslice();
            break;
        default:
            printf ("*** error: undefinded lxcall #%d\n", d0);
    }

    //if (intno == 35)
	//{
	//	uint32_t fn, c;
	//	uc_reg_read(g_uc, UC_M68K_REG_D0, &fn);


    //}
    //if (intno == 61)
    //{
    //    printf ("\n\n *** ERROR: unsupported exception was thrown!\n\n");
    //    uc_emu_stop(uc);
    //}

	return M68K_INT_ACK_AUTOVECTOR;
	//return M68K_INT_ACK_SPURIOUS;
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

    m68k_execute(300000);


#if 0
	uc_err err;
    static uint8_t rom[ROM_SIZE];
    FILE *romf;

	// Initialize emulator in 68k mode
	err = uc_open(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, &g_uc);
	if (err != UC_ERR_OK)
    {
	    printf("Failed on uc_open() with error returned: %u\n", err);
	    return -1;
	}

    // set up memory map

	uc_mem_map(g_uc, CHIPMEM_ADDRESS, CHIPMEM_SIZE, UC_PROT_ALL                );   // 0x000000 2MB   chip mem
	uc_mem_map(g_uc, FASTMEM_ADDRESS, FASTMEM_SIZE, UC_PROT_ALL                );   // 0x200000 8MB   fast mem
	uc_mem_map(g_uc, ROM_ADDRESS,     ROM_SIZE,     UC_PROT_READ | UC_PROT_EXEC);   // 0xf80000 256KB system rom


	// write machine code to be emulated to memory
	if (uc_mem_write(g_uc, ROM_ADDRESS, rom, ROM_SIZE))
    {
	    printf("Failed to write emulation code to memory, quit!\n");
	    return -1;
	}

	// init stack pointer
	uint32_t r_a7 = FASTMEM_ADDRESS; // 0x9FFF00;
	uc_reg_write(g_uc, UC_M68K_REG_A7, &r_a7);

    // no supervisor mode
    uint32_t sr;
    uc_reg_read(g_uc, UC_M68K_REG_SR, &sr);
    sr &= 0xdfff;
    uc_reg_write(g_uc, UC_M68K_REG_SR, &sr);
	// coldstart
	uint32_t coldstart = lpeek (ROM_ADDRESS+4);
    printf ("coldstart = 0x%08x\n", coldstart);
	hexdump (coldstart, 20);

    // lxcall handler
    uc_hook lxchook;
	uc_hook_add (g_uc, &lxchook, UC_HOOK_INTR, hook_lxcall, NULL, 1, 0);

	uc_hook hook1, hook2, hook3;
	uc_hook_add (g_uc, &hook1, UC_HOOK_MEM_READ, hook_mem, NULL, 1, 0);
	uc_hook_add (g_uc, &hook2, UC_HOOK_MEM_WRITE, hook_mem, NULL, 1, 0);
	uc_hook_add (g_uc, &hook3, UC_HOOK_CODE, hook_code, NULL, 1, 0);

	// emulate code in infinite time & unlimited instructions
	err=uc_emu_start(g_uc, coldstart, ROM_ADDRESS + ROM_SIZE-1, 0, 0);
	if (err)
    {
	    printf("Failed on uc_emu_start() with error returned %u: %s\n", err, uc_strerror(err));
	}

	uc_close(g_uc);
    #endif

	return 0;
}
