/*
 * lxa ramdrive.device implementation
 *
 * In-memory disk units sized from their mounted DOS device node, with the
 * trackdisk-style command set and KillRAD() of AmigaOS 3.1.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/trackdisk.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "ramdrive"
#define EXDEVVER   " 40.1 (2026/03/17)"

#define RAMDRIVE_UNIT_COUNT    16
#define RAMDRIVE_NAME_MAX      32

char __aligned _g_ramdrive_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_ramdrive_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_ramdrive_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_ramdrive_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/*
 * A unit is created by the first OpenDevice() of its number, which needs a
 * DOS device node for "ramdrive.device"/unit (what "Mount RAD:" creates):
 * the unit size comes from the node's DosEnvec and KillRAD() returns the
 * node's name.  The unit (and its data) then lives until KillRAD(); after
 * KillRAD() the unit stays known but reports TDERR_DiskChanged.  This is
 * the AmigaOS 3.1 (ramdrive.device 39.35) behaviour, verified on the
 * reference in Phase 220.
 */
struct RamdriveUnit
{
    struct Unit ru_Unit;
    UBYTE *ru_Data;
    ULONG ru_Size;
    ULONG ru_ChangeCount;
    BOOL ru_WriteProtected;
    char ru_Name[RAMDRIVE_NAME_MAX];
};

struct RamdriveBase
{
    struct Device rb_Device;
    BPTR rb_SegList;
    struct RamdriveUnit *rb_Units[RAMDRIVE_UNIT_COUNT];
};

static void ramdrive_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static struct RamdriveUnit *ramdrive_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct RamdriveUnit *)ioreq->io_Unit;
}

static BOOL ramdrive_bstr_equal(BSTR bstr, const char *s)
{
    const UBYTE *b = (const UBYTE *)BADDR(bstr);
    ULONG len;
    ULONG i;

    if (!b)
    {
        return FALSE;
    }

    len = b[0];
    /* MakeDosNode() counts the terminating NUL in the BSTR length */
    if (len > 0 && b[len] == '\0')
    {
        len--;
    }
    for (i = 0; i < len; i++)
    {
        if (!s[i] || s[i] != (char)b[i + 1])
        {
            return FALSE;
        }
    }

    return s[len] == '\0';
}

/* find the DOS device node mounted for ramdrive.device/unit_number */
static struct DosList *ramdrive_find_dos_node(ULONG unit_number)
{
    struct DosLibrary *dosbase;
    struct RootNode *root;
    struct DosInfo *info;
    struct DosList *dl;

    dosbase = (struct DosLibrary *)FindName(&SysBase->LibList, (CONST_STRPTR)"dos.library");
    if (!dosbase || !(root = dosbase->dl_Root) || !(info = (struct DosInfo *)BADDR(root->rn_Info)))
    {
        return NULL;
    }

    for (dl = (struct DosList *)BADDR(info->di_DevInfo); dl; dl = (struct DosList *)BADDR(dl->dol_Next))
    {
        struct FileSysStartupMsg *fssm;

        if (dl->dol_Type != DLT_DEVICE)
        {
            continue;
        }

        /* dol_Startup may also hold a small integer for handlers */
        if ((ULONG)dl->dol_misc.dol_handler.dol_Startup < 0x400)
        {
            continue;
        }

        fssm = (struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);
        if (fssm->fssm_Unit == unit_number &&
            ramdrive_bstr_equal(fssm->fssm_Device, "ramdrive.device") &&
            fssm->fssm_Environ)
        {
            return dl;
        }
    }

    return NULL;
}

static struct RamdriveUnit *ramdrive_create_unit(ULONG unit_number)
{
    struct RamdriveUnit *unit;
    struct DosList *dl;
    struct FileSysStartupMsg *fssm;
    struct DosEnvec *de;
    const UBYTE *name;
    ULONG size;
    ULONG len;
    ULONG i;

    dl = ramdrive_find_dos_node(unit_number);
    if (!dl)
    {
        return NULL;
    }

    fssm = (struct FileSysStartupMsg *)BADDR(dl->dol_misc.dol_handler.dol_Startup);
    de = (struct DosEnvec *)BADDR(fssm->fssm_Environ);
    if (de->de_HighCyl < de->de_LowCyl)
    {
        return NULL;
    }

    size = (de->de_HighCyl - de->de_LowCyl + 1) * de->de_Surfaces * de->de_BlocksPerTrack *
           de->de_SizeBlock * 4;
    if (size == 0)
    {
        return NULL;
    }

    unit = (struct RamdriveUnit *)AllocMem(sizeof(*unit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit)
    {
        return NULL;
    }

    unit->ru_Data = (UBYTE *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit->ru_Data)
    {
        FreeMem(unit, sizeof(*unit));
        return NULL;
    }

    unit->ru_Size = size;
    unit->ru_ChangeCount = 1;

    name = (const UBYTE *)BADDR(dl->dol_Name);
    len = name ? name[0] : 0;
    if (len > RAMDRIVE_NAME_MAX - 2)
    {
        len = RAMDRIVE_NAME_MAX - 2;
    }
    for (i = 0; i < len; i++)
    {
        unit->ru_Name[i] = (char)name[i + 1];
    }
    unit->ru_Name[len] = ':';
    unit->ru_Name[len + 1] = '\0';

    return unit;
}

static void ramdrive_free_unit(struct RamdriveUnit *unit)
{
    if (!unit)
    {
        return;
    }

    if (unit->ru_Data)
    {
        FreeMem(unit->ru_Data, unit->ru_Size);
    }

    FreeMem(unit, sizeof(*unit));
}

static LONG ramdrive_transfer(struct IORequest *ioreq,
                              struct RamdriveUnit *unit,
                              BOOL write)
{
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    ULONG offset;
    ULONG length;

    if (!io->io_Data && io->io_Length != 0)
    {
        return IOERR_BADADDRESS;
    }

    if (write && unit->ru_WriteProtected)
    {
        return TDERR_WriteProt;
    }

    /* killed unit (KillRAD) or a stale ETD change count */
    if (!unit->ru_Data ||
        ((ioreq->io_Command & TDF_EXTCOM) != 0 &&
         ((struct IOExtTD *)ioreq)->iotd_Count < unit->ru_ChangeCount))
    {
        return TDERR_DiskChanged;
    }

    offset = io->io_Offset;
    length = io->io_Length;
    if (offset > unit->ru_Size || length > (unit->ru_Size - offset))
    {
        return IOERR_BADLENGTH;
    }

    if (length != 0)
    {
        if (write)
        {
            CopyMem(io->io_Data, unit->ru_Data + offset, length);
        }
        else
        {
            CopyMem(unit->ru_Data + offset, io->io_Data, length);
        }
    }

    io->io_Actual = length;
    return 0;
}

static BPTR ramdrive_expunge_if_possible(struct RamdriveBase *ramdrivebase)
{
    ULONG unit;

    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)ramdrivebase->rb_Device.dd_Library.lib_Node.ln_Name) ==
        &ramdrivebase->rb_Device.dd_Library.lib_Node)
    {
        Remove(&ramdrivebase->rb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_ramdrive: Expunge() removed device from DeviceList\n");
    }

    if (ramdrivebase->rb_Device.dd_Library.lib_OpenCnt != 0)
    {
        ramdrivebase->rb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_ramdrive: Expunge() deferred, open count=%u\n",
                (unsigned int)ramdrivebase->rb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    ramdrivebase->rb_Device.dd_Library.lib_Flags &= (UBYTE)~LIBF_DELEXP;

    for (unit = 0; unit < RAMDRIVE_UNIT_COUNT; unit++)
    {
        ramdrive_free_unit(ramdrivebase->rb_Units[unit]);
        ramdrivebase->rb_Units[unit] = NULL;
    }

    return ramdrivebase->rb_SegList;
}

static STRPTR ramdrive_kill_unit(struct RamdriveBase *ramdrivebase, ULONG unit_number)
{
    struct RamdriveUnit *unit;

    if (!ramdrivebase || unit_number >= RAMDRIVE_UNIT_COUNT)
    {
        return NULL;
    }

    unit = ramdrivebase->rb_Units[unit_number];
    if (!unit || !unit->ru_Data)
    {
        return NULL;
    }

    FreeMem(unit->ru_Data, unit->ru_Size);
    unit->ru_Data = NULL;
    return (STRPTR)&unit->ru_Name[0];
}

static struct Library * __g_lxa_ramdrive_InitDev(register struct Library *dev __asm("d0"),
                                                  register BPTR seglist __asm("a0"),
                                                  register struct ExecBase *sysb __asm("a6"))
{
    struct RamdriveBase *ramdrivebase = (struct RamdriveBase *)dev;
    ULONG unit;

    (void)sysb;

    ramdrivebase->rb_SegList = seglist;
    for (unit = 0; unit < RAMDRIVE_UNIT_COUNT; unit++)
    {
        ramdrivebase->rb_Units[unit] = NULL;
    }

    return dev;
}

static void __g_lxa_ramdrive_Open(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"),
                                  register ULONG unit __asm("d0"),
                                  register ULONG flags __asm("d1"))
{
    struct RamdriveBase *ramdrivebase = (struct RamdriveBase *)dev;
    struct RamdriveUnit *ramunit;

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if ((dev->lib_Flags & LIBF_DELEXP) != 0 || unit >= RAMDRIVE_UNIT_COUNT)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    ramunit = ramdrivebase->rb_Units[unit];
    if (!ramunit)
    {
        ramunit = ramdrive_create_unit(unit);
        if (!ramunit)
        {
            ioreq->io_Error = IOERR_OPENFAIL;
            return;
        }
        ramdrivebase->rb_Units[unit] = ramunit;
    }

    /* OpenDevice() flags bit 0 write-protects the unit */
    ramunit->ru_WriteProtected = (flags & 1UL) != 0;

    ramunit->ru_Unit.unit_OpenCnt++;
    dev->lib_OpenCnt++;
    dev->lib_Flags &= (UBYTE)~LIBF_DELEXP;

    ioreq->io_Device = (struct Device *)ramdrivebase;
    ioreq->io_Unit = (struct Unit *)ramunit;
}

static BPTR __g_lxa_ramdrive_Close(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct RamdriveBase *ramdrivebase = (struct RamdriveBase *)dev;
    struct RamdriveUnit *ramunit = ramdrive_get_unit(ioreq);

    if (ramunit && ramunit->ru_Unit.unit_OpenCnt != 0)
    {
        ramunit->ru_Unit.unit_OpenCnt--;
    }

    if (ramdrivebase->rb_Device.dd_Library.lib_OpenCnt != 0)
    {
        ramdrivebase->rb_Device.dd_Library.lib_OpenCnt--;
    }

    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (ramdrivebase->rb_Device.dd_Library.lib_OpenCnt == 0 &&
        (ramdrivebase->rb_Device.dd_Library.lib_Flags & LIBF_DELEXP) != 0)
    {
        return ramdrive_expunge_if_possible(ramdrivebase);
    }

    return 0;
}

static BPTR __g_lxa_ramdrive_Expunge(register struct Library *dev __asm("a6"))
{
    return ramdrive_expunge_if_possible((struct RamdriveBase *)dev);
}

static BPTR __g_lxa_ramdrive_BeginIO(register struct Library *dev __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    (void)dev;

    struct RamdriveUnit *ramunit = ramdrive_get_unit(ioreq);
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    LONG result = 0;
    UWORD command = ioreq->io_Command & (UWORD)~TDF_EXTCOM;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;

    if (!ramunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        ramdrive_reply_request(ioreq);
        return 0;
    }

    /* only the commands below exist; io_Actual is only set where noted */
    switch (command)
    {
        case CMD_CLEAR:
        case CMD_UPDATE:
        case TD_REMOVE:
            break;

        case CMD_READ:
            result = ramdrive_transfer(ioreq, ramunit, FALSE);
            break;

        case CMD_WRITE:
        case TD_FORMAT:
            result = ramdrive_transfer(ioreq, ramunit, TRUE);
            break;

        case TD_CHANGENUM:
            io->io_Actual = ramunit->ru_ChangeCount;
            break;

        case TD_CHANGESTATE:
            io->io_Actual = 0;
            break;

        case TD_MOTOR:
            io->io_Actual = 1;
            break;

        case TD_PROTSTATUS:
            io->io_Actual = ramunit->ru_WriteProtected ? 1 : 0;
            break;

        default:
            result = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = result;
    ramdrive_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_ramdrive_AbortIO(register struct Library *dev __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    (void)dev;
    (void)ioreq;

    return (ULONG)-1;
}

static STRPTR __g_lxa_ramdrive_KillRAD0(register struct Library *dev __asm("a6"))
{
    return ramdrive_kill_unit((struct RamdriveBase *)dev, 0);
}

static STRPTR __g_lxa_ramdrive_KillRAD(register struct Library *dev __asm("a6"),
                                       register ULONG unit __asm("d0"))
{
    return ramdrive_kill_unit((struct RamdriveBase *)dev, unit);
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

extern APTR              __g_lxa_ramdrive_FuncTab [];
extern struct MyDataInit __g_lxa_ramdrive_DataTab;
extern struct InitTable  __g_lxa_ramdrive_InitTab;
extern APTR              __g_lxa_ramdrive_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_ramdrive_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_ramdrive_ExDevName[0],
    &_g_ramdrive_ExDevID[0],
    &__g_lxa_ramdrive_InitTab
};

APTR __g_lxa_ramdrive_EndResident;
struct Resident *__lxa_ramdrive_ROMTag = &ROMTag;

struct InitTable __g_lxa_ramdrive_InitTab =
{
    (ULONG)sizeof(struct RamdriveBase),
    (APTR *)&__g_lxa_ramdrive_FuncTab[0],
    (APTR)&__g_lxa_ramdrive_DataTab,
    (APTR)__g_lxa_ramdrive_InitDev
};

APTR __g_lxa_ramdrive_FuncTab [] =
{
    __g_lxa_ramdrive_Open,
    __g_lxa_ramdrive_Close,
    __g_lxa_ramdrive_Expunge,
    NULL,
    __g_lxa_ramdrive_BeginIO,
    __g_lxa_ramdrive_AbortIO,
    __g_lxa_ramdrive_KillRAD0,
    __g_lxa_ramdrive_KillRAD,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_ramdrive_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_ramdrive_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_ramdrive_ExDevID[0],
    (ULONG)0
};
