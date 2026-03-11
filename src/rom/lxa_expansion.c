#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <libraries/expansionbase.h>
#include <libraries/configregs.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXLIBNAME  "expansion"
#define EXLIBVER   " 40.1 (2022/03/20)"

char __aligned _g_expansion_ExLibName [] = EXLIBNAME ".library";
char __aligned _g_expansion_ExLibID   [] = EXLIBNAME EXLIBVER;
char __aligned _g_expansion_Copyright [] = "(C)opyright 2022 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_expansion_VERSTRING [] = "\0$VER: " EXLIBNAME EXLIBVER;

extern struct ExecBase      *SysBase;

struct LXAExpansionBase
{
    struct Library LibNode;
    UBYTE Flags;
    UBYTE eb_Private01;
    ULONG eb_Private02;
    ULONG eb_Private03;
    struct CurrentBinding eb_Private04;
    struct List BoardList;
    struct List MountList;
    UBYTE ExpansionSlots[(E_MEMORYSLOTS + 7) / 8];
};

static struct List *expansion_board_list(struct ExpansionBase *ExpansionBase)
{
    return &((struct LXAExpansionBase *)ExpansionBase)->BoardList;
}

static VOID expansion_lock_binding(void)
{
    Forbid();
}

static VOID expansion_unlock_binding(void)
{
    Permit();
}

static ULONG expansion_slots_needed(ULONG slot_spec)
{
    ULONG mem_bits = slot_spec & ERT_MEMMASK;

    if (mem_bits == 0)
    {
        return E_MEMORYSLOTS;
    }

    return 1UL << (mem_bits - 1);
}

static BOOL expansion_slot_is_allocated(struct ExpansionBase *ExpansionBase, ULONG slot)
{
    struct LXAExpansionBase *base = (struct LXAExpansionBase *)ExpansionBase;
    ULONG relative_slot;

    if (slot < (E_MEMORYBASE >> E_SLOTSHIFT) || slot >= ((E_MEMORYBASE >> E_SLOTSHIFT) + E_MEMORYSLOTS))
    {
        return TRUE;
    }

    relative_slot = slot - (E_MEMORYBASE >> E_SLOTSHIFT);
    return (base->ExpansionSlots[relative_slot >> 3] & (1U << (relative_slot & 7))) != 0;
}

static VOID expansion_set_slot_state(struct ExpansionBase *ExpansionBase, ULONG slot, BOOL allocated)
{
    struct LXAExpansionBase *base = (struct LXAExpansionBase *)ExpansionBase;
    ULONG relative_slot = slot - (E_MEMORYBASE >> E_SLOTSHIFT);
    UBYTE mask = (UBYTE)(1U << (relative_slot & 7));

    if (allocated)
    {
        base->ExpansionSlots[relative_slot >> 3] |= mask;
    }
    else
    {
        base->ExpansionSlots[relative_slot >> 3] &= (UBYTE)~mask;
    }
}

static BOOL expansion_range_is_free(struct ExpansionBase *ExpansionBase, ULONG start_slot, ULONG num_slots)
{
    ULONG slot;

    for (slot = 0; slot < num_slots; slot++)
    {
        if (expansion_slot_is_allocated(ExpansionBase, start_slot + slot))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static VOID expansion_mark_range(struct ExpansionBase *ExpansionBase, ULONG start_slot, ULONG num_slots, BOOL allocated)
{
    ULONG slot;

    for (slot = 0; slot < num_slots; slot++)
    {
        expansion_set_slot_state(ExpansionBase, start_slot + slot, allocated);
    }
}

static ULONG expansion_alloc_slots(struct ExpansionBase *ExpansionBase, ULONG num_slots, ULONG slot_offset)
{
    ULONG base_slot = (E_MEMORYBASE >> E_SLOTSHIFT);
    ULONG last_start;
    ULONG start_slot;

    if (num_slots == 0 || num_slots > E_MEMORYSLOTS)
    {
        return (ULONG)-1;
    }

    last_start = base_slot + E_MEMORYSLOTS - num_slots;
    start_slot = base_slot;

    while (((start_slot - slot_offset) % num_slots) != 0)
    {
        start_slot++;
    }

    while (start_slot <= last_start)
    {
        if (expansion_range_is_free(ExpansionBase, start_slot, num_slots))
        {
            expansion_mark_range(ExpansionBase, start_slot, num_slots, TRUE);
            return start_slot;
        }

        start_slot += num_slots;
    }

    return (ULONG)-1;
}

static VOID expansion_free_slots(struct ExpansionBase *ExpansionBase, ULONG start_slot, ULONG num_slots)
{
    ULONG base_slot = (E_MEMORYBASE >> E_SLOTSHIFT);
    ULONG last_slot = base_slot + E_MEMORYSLOTS;
    ULONG slot;

    if (start_slot == (ULONG)-1 || num_slots == 0)
    {
        return;
    }

    if (start_slot < base_slot || start_slot + num_slots > last_slot)
    {
        LPRINTF(LOG_ERROR, "_expansion: free outside Zorro II memory range start=%lu num=%lu\n", start_slot, num_slots);
        return;
    }

    for (slot = 0; slot < num_slots; slot++)
    {
        if (!expansion_slot_is_allocated(ExpansionBase, start_slot + slot))
        {
            LPRINTF(LOG_ERROR, "_expansion: double free in expansion slot range start=%lu num=%lu\n", start_slot, num_slots);
            return;
        }
    }

    expansion_mark_range(ExpansionBase, start_slot, num_slots, FALSE);
}

static ULONG expansion_board_slot_offset(ULONG slot_spec)
{
    ULONG slots = expansion_slots_needed(slot_spec);

    if (slots == 64)
    {
        return 32;
    }

    return 0;
}

// libBase: ExpansionBase
// baseType: struct ExpansionBase *
// libname: expansion.library

struct ExpansionBase * __g_lxa_expansion_InitLib    ( register struct ExpansionBase *expansionb    __asm("d0"),
                                                      register BPTR               seglist __asm("a0"),
                                                      register struct ExecBase   *sysb    __asm("a6"))
{
    (void)seglist;
    (void)sysb;

    NEWLIST(expansion_board_list(expansionb));
    NEWLIST(&((struct LXAExpansionBase *)expansionb)->MountList);

    DPRINTF (LOG_DEBUG, "_expansion: InitLib() called.\n");
    return expansionb;
}

struct ExpansionBase * __g_lxa_expansion_OpenLib ( register struct ExpansionBase  *ExpansionBase __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_expansion: OpenLib() called\n");
    ExpansionBase->LibNode.lib_OpenCnt++;
    ExpansionBase->LibNode.lib_Flags &= ~LIBF_DELEXP;
    return ExpansionBase;
}

BPTR __g_lxa_expansion_CloseLib ( register struct ExpansionBase  *expansionb __asm("a6"))
{
    expansionb->LibNode.lib_OpenCnt--;
    return NULL;
}

BPTR __g_lxa_expansion_ExpungeLib ( register struct ExpansionBase  *expansionb      __asm("a6"))
{
    return NULL;
}

ULONG __g_lxa_expansion_ExtFuncLib(void)
{
    return NULL;
}

VOID _expansion_AddConfigDev ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register struct ConfigDev * configDev __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_expansion: AddConfigDev(configDev=0x%08lx)\n", (ULONG)configDev);

    if (configDev == NULL)
    {
        return;
    }

    expansion_lock_binding();
    AddTail(expansion_board_list(ExpansionBase), (struct Node *)configDev);
    expansion_unlock_binding();
}

BOOL _expansion_AddBootNode ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register LONG bootPri __asm("d0"),
                                                        register ULONG flags __asm("d1"),
                                                        register struct DeviceNode * deviceNode __asm("a0"),
                                                        register struct ConfigDev * configDev __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_expansion: AddBootNode() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

ULONG _expansion_AllocBoardMem ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                         register ULONG slotSpec __asm("d0"))
{
    ULONG start_slot;
    ULONG num_slots = expansion_slots_needed(slotSpec);
    ULONG slot_offset = expansion_board_slot_offset(slotSpec);

    DPRINTF (LOG_DEBUG, "_expansion: AllocBoardMem(slotSpec=0x%08lx)\n", slotSpec);

    expansion_lock_binding();
    start_slot = expansion_alloc_slots(ExpansionBase, num_slots, slot_offset);
    expansion_unlock_binding();

    return start_slot;
}

struct ConfigDev * _expansion_AllocConfigDev ( register struct ExpansionBase * ExpansionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_expansion: AllocConfigDev() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

APTR _expansion_AllocExpansionMem ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register ULONG numSlots __asm("d0"),
                                                        register ULONG slotAlign __asm("d1"))
{
    ULONG start_slot;

    DPRINTF (LOG_DEBUG, "_expansion: AllocExpansionMem(numSlots=%lu, slotAlign=%lu)\n", numSlots, slotAlign);

    expansion_lock_binding();
    start_slot = expansion_alloc_slots(ExpansionBase, numSlots, slotAlign);
    expansion_unlock_binding();

    return (APTR)start_slot;
}

ULONG _expansion_ConfigBoard ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                         register APTR board __asm("a0"),
                                                         register struct ConfigDev * configDev __asm("a1"))
{
    ULONG slot_spec;
    ULONG board_size;
    ULONG slot_count;
    ULONG start_slot;

    DPRINTF (LOG_DEBUG, "_expansion: ConfigBoard(board=0x%08lx, configDev=0x%08lx)\n", (ULONG)board, (ULONG)configDev);

    if (configDev == NULL)
    {
        return EE_NOBOARD;
    }

    slot_spec = configDev->cd_Rom.er_Type;
    board_size = configDev->cd_BoardSize;

    if (board_size == 0)
    {
        board_size = ERT_MEMNEEDED(slot_spec);
    }

    slot_count = (board_size + E_SLOTSIZE - 1) >> E_SLOTSHIFT;
    if (slot_count == 0)
    {
        slot_count = expansion_slots_needed(slot_spec);
    }

    expansion_lock_binding();

    if (configDev->cd_BoardAddr != NULL && configDev->cd_SlotSize != 0)
    {
        expansion_unlock_binding();
        return EE_OK;
    }

    start_slot = expansion_alloc_slots(ExpansionBase, slot_count, expansion_board_slot_offset(slot_spec));

    if (start_slot == (ULONG)-1)
    {
        ExpansionBase->Flags |= EBF_SHORTMEM;
        if ((configDev->cd_Rom.er_Flags & ERFF_NOSHUTUP) == 0)
        {
            configDev->cd_Flags |= CDF_SHUTUP;
        }
        expansion_unlock_binding();
        return EE_NOEXPANSION;
    }

    configDev->cd_BoardAddr = (APTR)EC_MEMADDR(start_slot);
    configDev->cd_BoardSize = board_size;
    configDev->cd_SlotAddr = (UWORD)start_slot;
    configDev->cd_SlotSize = (UWORD)slot_count;
    configDev->cd_Flags &= (UBYTE)~CDF_SHUTUP;
    configDev->cd_Flags |= CDF_CONFIGME;

    expansion_unlock_binding();
    return EE_OK;
}

ULONG _expansion_ConfigChain ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                         register APTR baseAddr __asm("a0"))
{
    struct ConfigDev *config_dev;
    ULONG result = EE_OK;

    DPRINTF (LOG_DEBUG, "_expansion: ConfigChain(baseAddr=0x%08lx)\n", (ULONG)baseAddr);

    expansion_lock_binding();
    config_dev = (struct ConfigDev *)expansion_board_list(ExpansionBase)->lh_Head;

    while (config_dev != NULL && config_dev->cd_Node.ln_Succ != NULL)
    {
        struct ConfigDev *next_config_dev = (struct ConfigDev *)config_dev->cd_Node.ln_Succ;

        if (config_dev->cd_BoardAddr == NULL || config_dev->cd_SlotSize == 0)
        {
            ULONG board_result;

            expansion_unlock_binding();
            board_result = _expansion_ConfigBoard(ExpansionBase, baseAddr, config_dev);
            expansion_lock_binding();

            if (board_result != EE_OK)
            {
                result = board_result;
            }
        }

        config_dev = next_config_dev;
    }

    expansion_unlock_binding();
    return result;
}

struct ConfigDev * _expansion_FindConfigDev ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register const struct ConfigDev * oldConfigDev __asm("a0"),
                                                        register LONG manufacturer __asm("d0"),
                                                        register LONG product __asm("d1"))
{
    struct ConfigDev *config_dev;

    DPRINTF (LOG_DEBUG, "_expansion: FindConfigDev(oldConfigDev=0x%08lx, manufacturer=%ld, product=%ld)\n",
             (ULONG)oldConfigDev, manufacturer, product);

    expansion_lock_binding();

    if (oldConfigDev == NULL)
    {
        config_dev = (struct ConfigDev *)expansion_board_list(ExpansionBase)->lh_Head;
    }
    else
    {
        config_dev = (struct ConfigDev *)oldConfigDev->cd_Node.ln_Succ;
    }

    while (config_dev != NULL && config_dev->cd_Node.ln_Succ != NULL)
    {
        if ((manufacturer == -1 || config_dev->cd_Rom.er_Manufacturer == manufacturer) &&
            (product == -1 || config_dev->cd_Rom.er_Product == product))
        {
            break;
        }

        config_dev = (struct ConfigDev *)config_dev->cd_Node.ln_Succ;
    }

    if (config_dev != NULL && config_dev->cd_Node.ln_Succ == NULL)
    {
        config_dev = NULL;
    }

    expansion_unlock_binding();
    return config_dev;
}

VOID _expansion_FreeBoardMem ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register ULONG startSlot __asm("d0"),
                                                        register ULONG slotSpec __asm("d1"))
{
    ULONG num_slots = expansion_slots_needed(slotSpec);

    DPRINTF (LOG_DEBUG, "_expansion: FreeBoardMem(startSlot=%lu, slotSpec=0x%08lx)\n", startSlot, slotSpec);

    expansion_lock_binding();
    expansion_free_slots(ExpansionBase, startSlot, num_slots);
    expansion_unlock_binding();
}

VOID _expansion_FreeConfigDev ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register struct ConfigDev * configDev __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_expansion: FreeConfigDev() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_FreeExpansionMem ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register ULONG startSlot __asm("d0"),
                                                        register ULONG numSlots __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_expansion: FreeExpansionMem(startSlot=%lu, numSlots=%lu)\n", startSlot, numSlots);

    expansion_lock_binding();
    expansion_free_slots(ExpansionBase, startSlot, numSlots);
    expansion_unlock_binding();
}

UBYTE _expansion_ReadExpansionByte ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register const APTR board __asm("a0"),
                                                        register ULONG offset __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_expansion: ReadExpansionByte() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

VOID _expansion_ReadExpansionRom ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register const APTR board __asm("a0"),
                                                        register struct ConfigDev * configDev __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_expansion: ReadExpansionRom() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_RemConfigDev ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register struct ConfigDev * configDev __asm("a0"))
{
    DPRINTF (LOG_DEBUG, "_expansion: RemConfigDev(configDev=0x%08lx)\n", (ULONG)configDev);

    (void)ExpansionBase;

    if (configDev == NULL)
    {
        return;
    }

    expansion_lock_binding();
    Remove((struct Node *)configDev);
    expansion_unlock_binding();
}

VOID _expansion_WriteExpansionByte ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register APTR board __asm("a0"),
                                                        register ULONG offset __asm("d0"),
                                                        register UBYTE byte __asm("d1"))
{
    DPRINTF (LOG_ERROR, "_expansion: WriteExpansionByte() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_ObtainConfigBinding ( register struct ExpansionBase * ExpansionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_expansion: ObtainConfigBinding() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_ReleaseConfigBinding ( register struct ExpansionBase * ExpansionBase __asm("a6"))
{
    DPRINTF (LOG_ERROR, "_expansion: ReleaseConfigBinding() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_SetCurrentBinding ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register struct CurrentBinding * currentBinding __asm("a0"),
                                                        register ULONG bindingSize __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_expansion: SetCurrentBinding() unimplemented STUB called.\n");
    assert(FALSE);
}

ULONG _expansion_GetCurrentBinding ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register const struct CurrentBinding * currentBinding __asm("a0"),
                                                        register ULONG bindingSize __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_expansion: GetCurrentBinding() unimplemented STUB called.\n");
    assert(FALSE);
    return 0;
}

struct DeviceNode * _expansion_MakeDosNode ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register const APTR parmPacket __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_expansion: MakeDosNode() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

BOOL _expansion_AddDosNode ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register LONG bootPri __asm("d0"),
                                                        register ULONG flags __asm("d1"),
                                                        register struct DeviceNode * deviceNode __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_expansion: AddDosNode() unimplemented STUB called.\n");
    assert(FALSE);
    return FALSE;
}

struct MyDataInit
{
    UWORD ln_Type_Init     ; UWORD ln_Type_Offset     ; UWORD ln_Type_Content     ;
    UBYTE ln_Name_Init     ; UBYTE ln_Name_Offset     ; ULONG ln_Name_Content     ;
    UWORD lib_Flags_Init   ; UWORD lib_Flags_Offset   ; UWORD lib_Flags_Content   ;
    UWORD lib_Version_Init ; UWORD lib_Version_Offset ; UWORD lib_Version_Content ;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG ENDMARK;
};

extern APTR              __g_lxa_expansion_FuncTab [];
extern struct MyDataInit __g_lxa_expansion_DataTab;
extern struct InitTable  __g_lxa_expansion_InitTab;
extern APTR              __g_lxa_expansion_EndResident;

static struct Resident __aligned ROMTag =
{                                 //                               offset
    RTC_MATCHWORD,                // UWORD rt_MatchWord;                0
    &ROMTag,                      // struct Resident *rt_MatchTag;      2
    &__g_lxa_expansion_EndResident, // APTR  rt_EndSkip;                  6
    RTF_AUTOINIT,                 // UBYTE rt_Flags;                    7
    VERSION,                      // UBYTE rt_Version;                  8
    NT_LIBRARY,                   // UBYTE rt_Type;                     9
    0,                            // BYTE  rt_Pri;                     10
    &_g_expansion_ExLibName[0],     // char  *rt_Name;                   14
    &_g_expansion_ExLibID[0],       // char  *rt_IdString;               18
    &__g_lxa_expansion_InitTab      // APTR  rt_Init;                    22
};

APTR __g_lxa_expansion_EndResident;
struct Resident *__lxa_expansion_ROMTag = &ROMTag;

struct InitTable __g_lxa_expansion_InitTab =
{
    (ULONG)               sizeof(struct LXAExpansionBase),     // LibBaseSize
    (APTR              *) &__g_lxa_expansion_FuncTab[0],  // FunctionTable
    (APTR)                &__g_lxa_expansion_DataTab,     // DataTable
    (APTR)                __g_lxa_expansion_InitLib       // InitLibFn
};

APTR __g_lxa_expansion_FuncTab [] =
{
    __g_lxa_expansion_OpenLib,
    __g_lxa_expansion_CloseLib,
    __g_lxa_expansion_ExpungeLib,
    __g_lxa_expansion_ExtFuncLib,
    _expansion_AddConfigDev, // offset = -30
    _expansion_AddBootNode, // offset = -36
    _expansion_AllocBoardMem, // offset = -42
    _expansion_AllocConfigDev, // offset = -48
    _expansion_AllocExpansionMem, // offset = -54
    _expansion_ConfigBoard, // offset = -60
    _expansion_ConfigChain, // offset = -66
    _expansion_FindConfigDev, // offset = -72
    _expansion_FreeBoardMem, // offset = -78
    _expansion_FreeConfigDev, // offset = -84
    _expansion_FreeExpansionMem, // offset = -90
    _expansion_ReadExpansionByte, // offset = -96
    _expansion_ReadExpansionRom, // offset = -102
    _expansion_RemConfigDev, // offset = -108
    _expansion_WriteExpansionByte, // offset = -114
    _expansion_ObtainConfigBinding, // offset = -120
    _expansion_ReleaseConfigBinding, // offset = -126
    _expansion_SetCurrentBinding, // offset = -132
    _expansion_GetCurrentBinding, // offset = -138
    _expansion_MakeDosNode, // offset = -144
    _expansion_AddDosNode, // offset = -150
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_expansion_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_LIBRARY),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_expansion_ExLibName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_expansion_ExLibID[0],
    (ULONG) 0
};
