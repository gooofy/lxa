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

static ULONG call_config_board(APTR board, struct ConfigDev *config_dev)
{
    register struct ExpansionBase *base __asm("a6") = ExpansionBase;
    register APTR board_reg __asm("a0") = board;
    register struct ConfigDev *config_reg __asm("a1") = config_dev;
    register ULONG result __asm("d0");

    __asm volatile (
        "jsr -60(a6)"
        : "=d"(result), "+a"(board_reg), "+a"(config_reg)
        : "a"(base)
        : "d1", "cc", "memory");

    return result;
}

static ULONG call_config_chain(APTR base_addr)
{
    register struct ExpansionBase *base __asm("a6") = ExpansionBase;
    register APTR addr_reg __asm("a0") = base_addr;
    register ULONG result __asm("d0");

    __asm volatile (
        "jsr -66(a6)"
        : "=d"(result), "+a"(addr_reg)
        : "a"(base)
        : "d1", "a1", "cc", "memory");

    return result;
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
    struct ConfigDev board_c;
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

    slot = call_alloc_expansion_mem(0, 0);
    expect_ulong(slot, (ULONG)-1, "AllocExpansionMem rejects zero-sized requests");

    slot = call_alloc_expansion_mem(E_MEMORYSLOTS + 1, 0);
    expect_ulong(slot, (ULONG)-1, "AllocExpansionMem rejects oversized requests");

    slot = call_alloc_expansion_mem(2, 0);
    expect_ulong(slot, (E_MEMORYBASE >> E_SLOTSHIFT), "AllocExpansionMem allocates from first free slot");

    slot2 = call_alloc_expansion_mem(2, 0);
    expect_ulong(slot2, (E_MEMORYBASE >> E_SLOTSHIFT) + 2, "AllocExpansionMem advances past occupied range");

    FreeExpansionMem(slot, 2);
    slot = call_alloc_expansion_mem(2, 0);
    expect_ulong(slot, (E_MEMORYBASE >> E_SLOTSHIFT), "FreeExpansionMem makes freed range reusable");

    FreeExpansionMem(slot2, 2);
    FreeExpansionMem(slot, 2);
    FreeExpansionMem(slot, 2);
    FreeExpansionMem((ULONG)-1, 1);
    FreeExpansionMem(1, 1);

    slot = call_alloc_expansion_mem(64, 32);
    expect_ulong(slot, (E_MEMORYBASE >> E_SLOTSHIFT), "AllocExpansionMem honors slot offset for 4MB-style allocation");

    slot2 = call_alloc_expansion_mem(1, 0);
    expect_ulong(slot2, (ULONG)-1, "AllocExpansionMem fails when expansion space is exhausted");
    FreeExpansionMem(slot, 64);

    slot = call_alloc_board_mem(ERT_ZORROII | 0x07);
    expect_ulong(slot, (E_MEMORYBASE >> E_SLOTSHIFT), "AllocBoardMem uses board-specific 4MB alignment rule");
    FreeBoardMem(slot, 0x07);

    slot = call_alloc_board_mem(ERT_ZORROII);
    expect_ulong(slot, (E_MEMORYBASE >> E_SLOTSHIFT), "AllocBoardMem accepts full er_Type values");
    FreeBoardMem(slot, ERT_ZORROII);

    result = call_config_board((APTR)E_EXPANSIONBASE, NULL);
    expect_ulong(result, EE_NOBOARD, "ConfigBoard rejects NULL ConfigDev pointers");

    init_config_dev(&board_a, ERT_ZORROII | 0x02, 0, 0);
    result = call_config_board((APTR)E_EXPANSIONBASE, &board_a);
    expect_ulong(result, EE_OK, "ConfigBoard configures a Zorro II board");
    expect_ulong((ULONG)board_a.cd_BoardAddr, EC_MEMADDR(E_MEMORYBASE >> E_SLOTSHIFT), "ConfigBoard assigns board address from slot number");
    expect_ulong(board_a.cd_BoardSize, 2 * E_SLOTSIZE, "ConfigBoard derives board size from er_Type when missing");
    expect_ulong(board_a.cd_SlotAddr, E_MEMORYBASE >> E_SLOTSHIFT, "ConfigBoard records allocated slot address");
    expect_ulong(board_a.cd_SlotSize, 2, "ConfigBoard records allocated slot count");
    expect_true((board_a.cd_Flags & CDF_CONFIGME) != 0, "ConfigBoard marks configured boards as CONFIGME");

    result = call_config_board((APTR)E_EXPANSIONBASE, &board_a);
    expect_ulong(result, EE_OK, "ConfigBoard is idempotent for already configured boards");
    FreeBoardMem(board_a.cd_SlotAddr, board_a.cd_Rom.er_Type);

    init_config_dev(&board_a, ERT_ZORROII | 0x01, 0, 3 * E_SLOTSIZE);
    result = call_config_board((APTR)E_EXPANSIONBASE, &board_a);
    expect_ulong(result, EE_OK, "ConfigBoard accepts explicit board sizes");
    expect_ulong(board_a.cd_BoardSize, 3 * E_SLOTSIZE, "ConfigBoard preserves explicit board size values");
    expect_ulong(board_a.cd_SlotSize, 3, "ConfigBoard rounds explicit board size to slot count");
    FreeExpansionMem(board_a.cd_SlotAddr, board_a.cd_SlotSize);

    init_config_dev(&board_a, ERT_ZORROII | 0x07, 0, 0);
    result = call_config_board((APTR)E_EXPANSIONBASE, &board_a);
    expect_ulong(result, EE_OK, "ConfigBoard can consume the full expansion space");

    init_config_dev(&board_b, ERT_ZORROII | 0x01, 0, 0);
    result = call_config_board((APTR)E_EXPANSIONBASE, &board_b);
    expect_ulong(result, EE_NOEXPANSION, "ConfigBoard reports exhaustion when no slots remain");
    expect_true((board_b.cd_Flags & CDF_SHUTUP) != 0, "ConfigBoard shuts up failing boards when allowed");
    expect_true((ExpansionBase->Flags & EBF_SHORTMEM) != 0, "ConfigBoard raises the short-memory flag on allocation failure");

    init_config_dev(&board_c, ERT_ZORROII | 0x01, ERFF_NOSHUTUP, 0);
    result = call_config_board((APTR)E_EXPANSIONBASE, &board_c);
    expect_ulong(result, EE_NOEXPANSION, "ConfigBoard still fails when NOSHUTUP boards cannot be placed");
    expect_true((board_c.cd_Flags & CDF_SHUTUP) == 0, "ConfigBoard respects the NOSHUTUP flag on failure");
    FreeBoardMem(board_a.cd_SlotAddr, board_a.cd_Rom.er_Type);

    init_config_dev(&board_a, ERT_ZORROII | 0x02, 0, 0);
    init_config_dev(&board_b, ERT_ZORROII | 0x01, 0, 0);
    init_config_dev(&board_c, ERT_ZORROII | 0x01, 0, 0);
    board_c.cd_BoardAddr = (APTR)0x12340000;
    board_c.cd_SlotSize = 1;
    board_c.cd_SlotAddr = 0x1234;

    AddConfigDev(&board_a);
    AddConfigDev(&board_b);
    AddConfigDev(&board_c);

    result = call_config_chain((APTR)E_EXPANSIONBASE);
    expect_ulong(result, EE_OK, "ConfigChain configures each unconfigured board on the list");
    expect_ulong(board_a.cd_SlotAddr, E_MEMORYBASE >> E_SLOTSHIFT, "ConfigChain configures the first board in list order");
    expect_ulong(board_b.cd_SlotAddr, (E_MEMORYBASE >> E_SLOTSHIFT) + 2, "ConfigChain allocates later boards after earlier ones");
    expect_ulong((ULONG)board_c.cd_BoardAddr, 0x12340000, "ConfigChain leaves already configured boards untouched");

    RemConfigDev(&board_a);
    RemConfigDev(&board_b);
    RemConfigDev(&board_c);
    FreeBoardMem(board_a.cd_SlotAddr, board_a.cd_Rom.er_Type);
    FreeBoardMem(board_b.cd_SlotAddr, board_b.cd_Rom.er_Type);

    init_config_dev(&board_a, ERT_ZORROII | 0x07, 0, 0);
    init_config_dev(&board_b, ERT_ZORROII | 0x01, 0, 0);
    AddConfigDev(&board_a);
    AddConfigDev(&board_b);

    result = call_config_chain((APTR)E_EXPANSIONBASE);
    expect_ulong(result, EE_NOEXPANSION, "ConfigChain returns a failure code when a board cannot be configured");
    expect_true((board_b.cd_Flags & CDF_SHUTUP) != 0, "ConfigChain propagates ConfigBoard shutdown behavior");

    RemConfigDev(&board_a);
    RemConfigDev(&board_b);
    FreeBoardMem(board_a.cd_SlotAddr, board_a.cd_Rom.er_Type);

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
