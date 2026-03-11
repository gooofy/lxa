#include <exec/types.h>
//#include <exec/memory.h>
//#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <libraries/expansionbase.h>

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

VOID _expansion_AllocBoardMem ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register ULONG slotSpec __asm("d0"))
{
    DPRINTF (LOG_ERROR, "_expansion: AllocBoardMem() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_expansion: AllocExpansionMem() unimplemented STUB called.\n");
    assert(FALSE);
    return NULL;
}

VOID _expansion_ConfigBoard ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register APTR board __asm("a0"),
                                                        register struct ConfigDev * configDev __asm("a1"))
{
    DPRINTF (LOG_ERROR, "_expansion: ConfigBoard() unimplemented STUB called.\n");
    assert(FALSE);
}

VOID _expansion_ConfigChain ( register struct ExpansionBase * ExpansionBase __asm("a6"),
                                                        register APTR baseAddr __asm("a0"))
{
    DPRINTF (LOG_ERROR, "_expansion: ConfigChain() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_expansion: FreeBoardMem() unimplemented STUB called.\n");
    assert(FALSE);
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
    DPRINTF (LOG_ERROR, "_expansion: FreeExpansionMem() unimplemented STUB called.\n");
    assert(FALSE);
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
    (ULONG)               sizeof(struct ExpansionBase),        // LibBaseSize
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
