/*
 * lxa_dev_trackdisk.c - Trackdisk device stub implementation
 *
 * Provides a stub trackdisk.device for compatibility with applications
 * that attempt to use it. Since lxa maps floppy drives to directories
 * (WINE-like approach), actual disk I/O is not performed. This stub
 * allows OpenDevice() to succeed and returns sensible defaults for
 * status queries.
 *
 * Supported commands:
 * - TD_CHANGENUM:   Returns change counter (always 1 = disk present)
 * - TD_CHANGESTATE: Returns 0 (disk present)
 * - TD_PROTSTATUS:  Returns 0 (not write-protected)
 * - TD_GETDRIVETYPE: Returns DRIVE3_5
 * - TD_GETNUMTRACKS: Returns 160 (80 cylinders x 2 heads)
 * - TD_GETGEOMETRY: Returns standard DD 3.5" floppy geometry
 * - TD_MOTOR:       No-op (returns previous state = 0)
 * - CMD_RESET/CMD_STOP/CMD_START/CMD_FLUSH/CMD_UPDATE/CMD_CLEAR: No-op success
 *
 * Unsupported commands return IOERR_NOCMD.
 *
 * Phase 77: trackdisk.device Implementation
 */

#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/errors.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/trackdisk.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "trackdisk"
#define EXDEVVER   " 40.1 (2026/03/06)"

char __aligned _g_trackdisk_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_trackdisk_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_trackdisk_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT license.";

char __aligned _g_trackdisk_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

/* Trackdisk device base */
struct TrackdiskBase
{
    struct Device  td_Device;
    BPTR           td_SegList;
};

/* Per-unit data */
struct TDUnit
{
    struct Unit    tdu_Unit;
    ULONG          tdu_ChangeCount;    /* disk change counter */
    UBYTE          tdu_DiskIn;         /* 0=no disk, 1=disk present */
    UBYTE          tdu_MotorOn;        /* motor state */
    UBYTE          tdu_ProtStatus;     /* write protection */
    UBYTE          tdu_UnitNum;
};

#define NUMUNITS  4

static struct TDUnit *g_units[NUMUNITS];

/*
 * Device Init
 */
static struct Library * __g_lxa_trackdisk_InitDev ( register struct Library    *dev     __asm("d0"),
                                                     register BPTR              seglist __asm("a0"),
                                                     register struct ExecBase  *sysb    __asm("a6"))
{
    struct TrackdiskBase *tdbase = (struct TrackdiskBase *)dev;
    ULONG i;

    DPRINTF (LOG_DEBUG, "_trackdisk: InitDev() called\n");

    tdbase->td_SegList = seglist;

    for (i = 0; i < NUMUNITS; i++)
        g_units[i] = NULL;

    return dev;
}

/*
 * Device Open
 *
 * Unit 0-3 are valid floppy units. We lazily allocate per-unit data
 * and always report success (with a disk present).
 */
static void __g_lxa_trackdisk_Open ( register struct Library   *dev   __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"),
                                      register ULONG             unit  __asm("d0"),
                                      register ULONG             flags __asm("d1"))
{
    DPRINTF (LOG_DEBUG, "_trackdisk: Open() called, unit=%lu flags=0x%08lx\n", unit, flags);

    if (unit >= NUMUNITS)
    {
        DPRINTF (LOG_DEBUG, "_trackdisk: Open() bad unit %lu\n", unit);
        ioreq->io_Error = TDERR_BadUnitNum;
        ioreq->io_Unit = NULL;
        ioreq->io_Device = NULL;
        return;
    }

    /* Allocate unit if not already done */
    if (!g_units[unit])
    {
        struct TDUnit *tdu = (struct TDUnit *)AllocMem(sizeof(struct TDUnit), MEMF_PUBLIC | MEMF_CLEAR);
        if (!tdu)
        {
            ioreq->io_Error = TDERR_NoMem;
            ioreq->io_Unit = NULL;
            ioreq->io_Device = NULL;
            return;
        }
        tdu->tdu_ChangeCount = 1;   /* disk has been inserted once */
        tdu->tdu_DiskIn      = 1;   /* disk present */
        tdu->tdu_MotorOn     = 0;
        tdu->tdu_ProtStatus  = 0;   /* not write-protected */
        tdu->tdu_UnitNum     = (UBYTE)unit;
        g_units[unit] = tdu;
    }

    dev->lib_OpenCnt++;
    g_units[unit]->tdu_Unit.unit_OpenCnt++;

    ioreq->io_Error  = 0;
    ioreq->io_Unit   = (struct Unit *)g_units[unit];
    ioreq->io_Device = (struct Device *)dev;
}

/*
 * Device Close
 */
static BPTR __g_lxa_trackdisk_Close ( register struct Library   *dev   __asm("a6"),
                                       register struct IORequest *ioreq __asm("a1"))
{
    struct TDUnit *tdu = (struct TDUnit *)ioreq->io_Unit;

    DPRINTF (LOG_DEBUG, "_trackdisk: Close() called\n");

    if (tdu)
        tdu->tdu_Unit.unit_OpenCnt--;

    dev->lib_OpenCnt--;

    ioreq->io_Unit   = NULL;
    ioreq->io_Device = NULL;

    return 0;
}

/*
 * Device Expunge
 */
static BPTR __g_lxa_trackdisk_Expunge ( register struct Library *dev __asm("a6"))
{
    DPRINTF (LOG_DEBUG, "_trackdisk: Expunge() stub called\n");
    return 0;
}

/*
 * Device BeginIO
 *
 * Handle trackdisk commands. Status queries are handled synchronously.
 * I/O commands (read/write/format) are not supported in this stub.
 */
static BPTR __g_lxa_trackdisk_BeginIO ( register struct Library   *dev   __asm("a6"),
                                         register struct IORequest *ioreq __asm("a1"))
{
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    struct TDUnit *tdu = (struct TDUnit *)ioreq->io_Unit;
    UWORD command = ioreq->io_Command;

    DPRINTF (LOG_DEBUG, "_trackdisk: BeginIO() command=%u\n", command);

    /* Strip extended command flag for dispatch */
    UWORD base_cmd = command & ~TDF_EXTCOM;

    ioreq->io_Error = 0;

    switch (base_cmd)
    {
        case CMD_RESET:
        case CMD_STOP:
        case CMD_START:
        case CMD_FLUSH:
        case CMD_UPDATE:
        case CMD_CLEAR:
            /* No-op success */
            break;

        case TD_MOTOR:
        {
            /* Return previous motor state, accept new state */
            ULONG prev = tdu ? tdu->tdu_MotorOn : 0;
            if (tdu && io->io_Length <= 1)
                tdu->tdu_MotorOn = (UBYTE)io->io_Length;
            io->io_Actual = prev;
            break;
        }

        case TD_CHANGENUM:
            /* Return disk change counter */
            io->io_Actual = tdu ? tdu->tdu_ChangeCount : 0;
            break;

        case TD_CHANGESTATE:
            /* 0 = disk present, non-zero = no disk */
            io->io_Actual = (tdu && tdu->tdu_DiskIn) ? 0 : 1;
            break;

        case TD_PROTSTATUS:
            /* 0 = not protected, non-zero = protected */
            io->io_Actual = tdu ? tdu->tdu_ProtStatus : 0;
            break;

        case TD_GETDRIVETYPE:
            io->io_Actual = DRIVE3_5;
            break;

        case TD_GETNUMTRACKS:
            io->io_Actual = 160;  /* 80 cylinders x 2 heads */
            break;

        case TD_GETGEOMETRY:
        {
            struct DriveGeometry *dg = (struct DriveGeometry *)io->io_Data;
            if (!dg || io->io_Length < sizeof(struct DriveGeometry))
            {
                ioreq->io_Error = TDERR_NotSpecified;
                break;
            }
            dg->dg_SectorSize   = TD_SECTOR;   /* 512 */
            dg->dg_TotalSectors = 1760;         /* 80 * 2 * 11 (DD) */
            dg->dg_Cylinders    = 80;
            dg->dg_CylSectors   = 22;           /* 2 heads * 11 sectors */
            dg->dg_Heads        = 2;
            dg->dg_TrackSectors = NUMSECS;      /* 11 (DD) */
            dg->dg_BufMemType   = MEMF_PUBLIC;
            dg->dg_DeviceType   = DG_DIRECT_ACCESS;
            dg->dg_Flags        = DGF_REMOVABLE;
            dg->dg_Reserved     = 0;
            break;
        }

        case TD_SEEK:
            /* Seek is a no-op in our stub (directories don't have tracks) */
            break;

        case TD_ADDCHANGEINT:
            /* We don't support disk change notifications - just hold the request */
            /* Don't reply - the request stays pending until TD_REMCHANGEINT */
            return 0;

        case TD_REMCHANGEINT:
            /* Reply to the add-change-int request */
            break;

        case TD_EJECT:
            /* No-op - can't eject a directory-mapped disk */
            break;

        case CMD_READ:
        case CMD_WRITE:
        case TD_FORMAT:
        case TD_RAWREAD:
        case TD_RAWWRITE:
            /* Actual I/O not supported in stub - directories use DOS, not device I/O */
            DPRINTF (LOG_DEBUG, "_trackdisk: I/O command %u not supported (use DOS)\n", base_cmd);
            ioreq->io_Error = TDERR_NotSpecified;
            break;

        default:
            ioreq->io_Error = IOERR_NOCMD;
            break;
    }

    /* Reply if not quick */
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }

    return 0;
}

/*
 * Device AbortIO
 */
static ULONG __g_lxa_trackdisk_AbortIO ( register struct Library   *dev   __asm("a6"),
                                           register struct IORequest *ioreq __asm("a1"))
{
    DPRINTF (LOG_DEBUG, "_trackdisk: AbortIO() stub called\n");
    return IOERR_NOCMD;
}

/*
 * Device data tables and initialization
 */

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

extern APTR              __g_lxa_trackdisk_FuncTab [];
extern struct MyDataInit __g_lxa_trackdisk_DataTab;
extern struct InitTable  __g_lxa_trackdisk_InitTab;
extern APTR              __g_lxa_trackdisk_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,                     // UWORD rt_MatchWord
    &ROMTag,                           // struct Resident *rt_MatchTag
    &__g_lxa_trackdisk_EndResident,    // APTR  rt_EndSkip
    RTF_AUTOINIT,                      // UBYTE rt_Flags
    VERSION,                           // UBYTE rt_Version
    NT_DEVICE,                         // UBYTE rt_Type
    0,                                 // BYTE  rt_Pri
    &_g_trackdisk_ExDevName[0],        // char  *rt_Name
    &_g_trackdisk_ExDevID[0],          // char  *rt_IdString
    &__g_lxa_trackdisk_InitTab         // APTR  rt_Init
};

APTR __g_lxa_trackdisk_EndResident;
struct Resident *__lxa_trackdisk_ROMTag = &ROMTag;

struct InitTable __g_lxa_trackdisk_InitTab =
{
    (ULONG)               sizeof(struct TrackdiskBase),
    (APTR              *) &__g_lxa_trackdisk_FuncTab[0],
    (APTR)                &__g_lxa_trackdisk_DataTab,
    (APTR)                __g_lxa_trackdisk_InitDev
};

APTR __g_lxa_trackdisk_FuncTab [] =
{
    __g_lxa_trackdisk_Open,
    __g_lxa_trackdisk_Close,
    __g_lxa_trackdisk_Expunge,
    0,  /* Reserved */
    __g_lxa_trackdisk_BeginIO,
    __g_lxa_trackdisk_AbortIO,
    (APTR) ((LONG)-1)
};

struct MyDataInit __g_lxa_trackdisk_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,    ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE) (ULONG) OFFSET(Node,    ln_Name), (ULONG) &_g_trackdisk_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library, lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library, lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library, lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE) (ULONG) OFFSET(Library, lib_IdString), (ULONG) &_g_trackdisk_ExDevID[0],
    (ULONG) 0
};
