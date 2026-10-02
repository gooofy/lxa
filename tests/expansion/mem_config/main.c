#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>
#include <libraries/expansionbase.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/expansion_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/expansion.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct ExpansionBase *ExpansionBase;

static LONG errors;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
    {
        len++;
    }

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG value)
{
    char buf[16];
    int i = 0;
    BOOL negative = FALSE;

    if (value < 0)
    {
        negative = TRUE;
        value = -value;
    }

    if (value == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        while (value > 0 && i < (int)sizeof(buf) - 1)
        {
            buf[i++] = '0' + (value % 10);
            value /= 10;
        }
    }

    if (negative)
    {
        buf[i++] = '-';
    }

    while (i-- > 0)
    {
        Write(Output(), &buf[i], 1);
    }
}

static void pass(const char *msg)
{
    print("OK: ");
    print(msg);
    print("\n");
}

static void fail(const char *msg)
{
    print("FAIL: ");
    print(msg);
    print("\n");
    errors++;
}

static void expect_true(BOOL condition, const char *msg)
{
    if (condition)
    {
        pass(msg);
    }
    else
    {
        fail(msg);
    }
}

static void expect_ulong(ULONG actual, ULONG expected, const char *msg)
{
    if (actual == expected)
    {
        pass(msg);
    }
    else
    {
        fail(msg);
    }
}

static ULONG call_alloc_board_mem(ULONG slot_spec)
{
    register struct ExpansionBase *base __asm("a6") = ExpansionBase;
    register ULONG result __asm("d0") = slot_spec;

    __asm volatile (
        "jsr -42(a6)"
        : "+d"(result)
        : "a"(base)
        : "a0", "a1", "d1", "cc", "memory");

    return result;
}

static ULONG call_alloc_expansion_mem(ULONG num_slots, ULONG slot_align)
{
    register struct ExpansionBase *base __asm("a6") = ExpansionBase;
    register ULONG result __asm("d0") = num_slots;
    register ULONG align __asm("d1") = slot_align;

    __asm volatile (
        "jsr -54(a6)"
        : "+d"(result), "+d"(align)
        : "a"(base)
        : "a0", "a1", "cc", "memory");

    return result;
}



static struct ConfigDev *call_alloc_config_dev(void)
{
    register struct ExpansionBase *base __asm("a6") = ExpansionBase;
    register struct ConfigDev *result __asm("d0");

    __asm volatile (
        "jsr -48(a6)"
        : "=r"(result)
        : "a"(base)
        : "a0", "a1", "d1", "cc", "memory");

    return result;
}

static void encode_expansion_byte(UBYTE *board, ULONG offset, UBYTE value)
{
    ULONG physical_offset = offset * 4;

    board[physical_offset + 2] = (UBYTE)(value << 4);
    board[physical_offset] = value;
}

static void init_expansion_rom_image(UBYTE *board,
                                     UBYTE er_type,
                                     UBYTE er_product,
                                     UBYTE er_flags,
                                     UWORD manufacturer,
                                     ULONG serial_number,
                                     UWORD diag_vec)
{
    ULONG i;
    struct ExpansionRom rom;
    UBYTE *rom_bytes = (UBYTE *)&rom;

    for (i = 0; i < 128; i++)
    {
        board[i] = 0;
    }

    for (i = 0; i < sizeof(rom); i++)
    {
        rom_bytes[i] = 0;
    }

    rom.er_Type = er_type;
    rom.er_Product = er_product;
    rom.er_Flags = er_flags;
    rom.er_Manufacturer = manufacturer;
    rom.er_SerialNumber = serial_number;
    rom.er_InitDiagVec = diag_vec;

    encode_expansion_byte(board, 0, rom.er_Type);
    for (i = 1; i < sizeof(rom); i++)
    {
        encode_expansion_byte(board, i, (UBYTE)~rom_bytes[i]);
    }
}

static void init_config_dev(struct ConfigDev *config_dev, UBYTE er_type, UBYTE er_flags, ULONG board_size)
{
    ULONG i;
    UBYTE *bytes = (UBYTE *)config_dev;

    for (i = 0; i < sizeof(*config_dev); i++)
    {
        bytes[i] = 0;
    }

    config_dev->cd_Node.ln_Type = NT_UNKNOWN;
    config_dev->cd_Rom.er_Type = er_type;
    config_dev->cd_Rom.er_Flags = er_flags;
    config_dev->cd_BoardSize = board_size;
}

int main(void)
{
    struct ConfigDev board_a;
    struct ConfigDev board_b;
    struct ConfigDev *allocated_config_dev;
    UBYTE board_image[128];
    ULONG slot;
    ULONG slot2;
    ULONG result;

    print("Testing expansion.library memory/config helpers\n");

    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    if (ExpansionBase == NULL)
    {
        print("FAIL: OpenLibrary(expansion.library) failed\n");
        return 20;
    }

    allocated_config_dev = call_alloc_config_dev();
    expect_true(allocated_config_dev != NULL, "AllocConfigDev allocates a ConfigDev structure");
    if (allocated_config_dev != NULL)
    {
        expect_ulong((ULONG)allocated_config_dev->cd_BoardAddr, 0, "AllocConfigDev clears the board address");
        expect_ulong(allocated_config_dev->cd_BoardSize, 0, "AllocConfigDev clears the board size");
        expect_ulong(allocated_config_dev->cd_Rom.er_Type, 0, "AllocConfigDev clears the ROM type byte");
        FreeConfigDev(allocated_config_dev);
    }

    /* Slot numbers depend on the boards of the machine: check only
     * machine-independent properties of the allocator. */
    slot = call_alloc_expansion_mem(E_MEMORYSLOTS + 1, 0);
    expect_ulong(slot, (ULONG)-1, "AllocExpansionMem rejects oversized requests");

    slot = call_alloc_expansion_mem(2, 0);
    expect_true(slot != (ULONG)-1, "AllocExpansionMem allocates two slots");

    slot2 = call_alloc_expansion_mem(2, 0);
    expect_true(slot2 != (ULONG)-1 && (slot2 >= slot + 2 || slot2 + 2 <= slot),
                "AllocExpansionMem does not hand out an occupied range");

    FreeExpansionMem(slot, 2);
    result = call_alloc_expansion_mem(2, 0);
    expect_ulong(result, slot, "FreeExpansionMem makes freed range reusable");

    FreeExpansionMem(slot2, 2);
    FreeExpansionMem(result, 2);

    slot = call_alloc_expansion_mem(64, 32);
    expect_true(slot != (ULONG)-1 && (slot % 32) == 0, "AllocExpansionMem honors the slot alignment");

    slot2 = call_alloc_expansion_mem(1, 0);
    expect_true(slot2 != (ULONG)-1 && (slot2 < slot || slot2 >= slot + 64),
                "AllocExpansionMem allocates outside an aligned 4MB-style range");
    FreeExpansionMem(slot2, 1);
    FreeExpansionMem(slot, 64);

    slot = call_alloc_board_mem(ERT_ZORROII | 0x07);
    expect_true(slot != (ULONG)-1, "AllocBoardMem allocates a 4MB board");
    FreeBoardMem(slot, 0x07);

    slot = call_alloc_board_mem(ERT_ZORROII);
    expect_true(slot != (ULONG)-1, "AllocBoardMem accepts full er_Type values");
    FreeBoardMem(slot, ERT_ZORROII);

    init_expansion_rom_image(board_image,
                             ERT_ZORROII | 0x02,
                             0x34,
                             0,
                             0x07db,
                             0x12345678,
                             0x0040);
    expect_ulong(ReadExpansionByte(board_image, EROFFSET(er_Type)), ERT_ZORROII | 0x02,
                 "ReadExpansionByte decodes logical bytes from nybble-wide board storage");
    WriteExpansionByte(board_image, ECOFFSET(ec_Interrupt), 0xa5);
    expect_ulong(ReadExpansionByte(board_image, ECOFFSET(ec_Interrupt)), 0xa5,
                 "WriteExpansionByte stores data in the layout expected by ReadExpansionByte");

    /* AmigaOS 3.1 fills only cd_Rom - plus, reading one byte too many,
     * the top byte of cd_BoardAddr with the inverted ec_Interrupt byte;
     * cd_BoardSize is left alone (reference-verified) */
    init_config_dev(&board_a, 0, 0, 0x5555);
    board_a.cd_BoardAddr = (APTR)0x4444;
    ReadExpansionRom(board_image, &board_a);
    expect_ulong(board_a.cd_Rom.er_Type, ERT_ZORROII | 0x02, "ReadExpansionRom restores the ROM type byte");
    expect_ulong(board_a.cd_Rom.er_Product, 0x34, "ReadExpansionRom decodes the product byte");
    expect_ulong(board_a.cd_Rom.er_Manufacturer, 0x07db, "ReadExpansionRom decodes the manufacturer field");
    expect_ulong(board_a.cd_Rom.er_SerialNumber, 0x12345678, "ReadExpansionRom decodes the serial number field");
    expect_ulong((ULONG)board_a.cd_BoardAddr, 0x5a004444, "ReadExpansionRom reads the byte after cd_Rom into cd_BoardAddr");
    expect_ulong(board_a.cd_BoardSize, 0x5555, "ReadExpansionRom leaves cd_BoardSize alone");

    init_expansion_rom_image(board_image,
                             ERT_ZORROII | 0x01,
                             1,
                             0,
                             0,
                             0,
                             0);
    init_config_dev(&board_b, 0, 0, 0xdeadbeef);
    board_b.cd_BoardAddr = (APTR)0x1234;
    ReadExpansionRom(board_image, &board_b);
    expect_ulong(board_b.cd_Rom.er_Type, ERT_ZORROII | 0x01, "ReadExpansionRom copies the ROM of any board");
    expect_ulong((ULONG)board_b.cd_BoardAddr, 0xff001234, "ReadExpansionRom keeps the low bytes of cd_BoardAddr");
    expect_ulong(board_b.cd_BoardSize, 0xdeadbeef, "ReadExpansionRom keeps cd_BoardSize for any board");

    CloseLibrary((struct Library *)ExpansionBase);

    if (errors == 0)
    {
        print("PASS: expansion memory/config helper tests passed\n");
        return 0;
    }

    print("FAIL: ");
    print_num(errors);
    print(" expansion memory/config helper test(s) failed\n");
    return 20;
}
