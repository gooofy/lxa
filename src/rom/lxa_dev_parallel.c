/*
 * lxa parallel.device implementation
 *
 * Hosted compatibility implementation with shared/exclusive opens,
 * configurable EOF/handshake flags, loopback-backed reads/writes, and
 * basic status/query semantics.
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

#include <devices/newstyle.h>
#include <devices/parallel.h>

#include "util.h"

#define VERSION  40
#define REVISION 1
#define EXDEVNAME "parallel"
#define EXDEVVER  " 40.1 (2026/03/17)"

#define PARALLEL_UNIT_COUNT    1
#define PARALLEL_BUFFER_SIZE   4096

char __aligned _g_parallel_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_parallel_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_parallel_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_parallel_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct ParallelConfig
{
    ULONG io_PExtFlags;
    UBYTE io_ParFlags;
    struct IOPArray io_PTermArray;
};

struct ParallelUnit
{
    struct Unit pu_Unit;
    struct ParallelUnit *pu_Next;
    struct IOExtPar *pu_PendingRead;
    BOOL pu_Shared;
};

struct ParallelBase
{
    struct Device pb_Device;
    BPTR pb_SegList;
    struct ParallelUnit *pb_Units;
    struct ParallelConfig pb_Config;
    UBYTE *pb_Buffer;
    ULONG pb_BufferCount;
    UBYTE pb_Status;
    BOOL pb_ExclusiveOpen;
    BOOL pb_Stopped;
};

static const UWORD parallel_supported_commands[] =
{
    CMD_CLEAR,
    CMD_FLUSH,
    CMD_READ,
    CMD_RESET,
    CMD_START,
    CMD_STOP,
    CMD_WRITE,
    PDCMD_QUERY,
    PDCMD_SETPARAMS,
    NSCMD_DEVICEQUERY,
    0
};

static void parallel_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static void parallel_set_default_config(struct ParallelBase *parallelbase)
{
    parallelbase->pb_Config.io_PExtFlags = 0;
    parallelbase->pb_Config.io_ParFlags = 0;
    parallelbase->pb_Config.io_PTermArray.PTermArray0 = 0;
    parallelbase->pb_Config.io_PTermArray.PTermArray1 = 0;
}

static void parallel_fill_request_config(struct ParallelBase *parallelbase,
                                         struct IOExtPar *io,
                                         BOOL shared)
{
    if (!parallelbase || !io)
    {
        return;
    }

    io->io_PExtFlags = parallelbase->pb_Config.io_PExtFlags;
    io->io_ParFlags = parallelbase->pb_Config.io_ParFlags | (shared ? PARF_SHARED : 0);
    io->io_PTermArray = parallelbase->pb_Config.io_PTermArray;
    io->io_Status = parallelbase->pb_Status;
}

static BOOL parallel_request_is_valid(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IOExtPar);
}

static struct ParallelUnit *parallel_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct ParallelUnit *)ioreq->io_Unit;
}

static BOOL parallel_has_pending_read(const struct ParallelBase *parallelbase)
{
    struct ParallelUnit *unit;

    for (unit = parallelbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_PendingRead)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void parallel_recompute_exclusive(struct ParallelBase *parallelbase)
{
    struct ParallelUnit *unit;

    parallelbase->pb_ExclusiveOpen = FALSE;

    for (unit = parallelbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (!unit->pu_Shared)
        {
            parallelbase->pb_ExclusiveOpen = TRUE;
            break;
        }
    }
}

static ULONG parallel_compute_write_length(const struct IOExtPar *io)
{
    const UBYTE *data = (const UBYTE *)io->IOPar.io_Data;
    ULONG length = io->IOPar.io_Length;

    if (length != (ULONG)-1)
    {
        return length;
    }

    length = 0;
    while (data[length] != 0)
    {
        length++;
    }

    return length;
}

static LONG parallel_append_buffer(struct ParallelBase *parallelbase,
                                   const UBYTE *data,
                                   ULONG length)
{
    ULONG available;

    if (length == 0)
    {
        return 0;
    }

    available = PARALLEL_BUFFER_SIZE - parallelbase->pb_BufferCount;
    if (length > available)
    {
        return ParErr_BufTooBig;
    }

    CopyMem((APTR)data,
            parallelbase->pb_Buffer + parallelbase->pb_BufferCount,
            length);
    parallelbase->pb_BufferCount += length;
    return 0;
}

static BOOL parallel_is_term_char(const struct IOExtPar *io, UBYTE value)
{
    const UBYTE *terms = (const UBYTE *)&io->io_PTermArray;
    int idx;

    if (!(io->io_ParFlags & PARF_EOFMODE))
    {
        return FALSE;
    }

    for (idx = 0; idx < 8; idx++)
    {
        if (terms[idx] == value)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static ULONG parallel_compute_read_length(struct IOExtPar *io,
                                          const UBYTE *buffer,
                                          ULONG available)
{
    ULONG requested = io->IOPar.io_Length;
    ULONG count = requested;

    if (requested == (ULONG)-1)
    {
        ULONG idx;

        if (!(io->io_ParFlags & PARF_EOFMODE))
        {
            return available;
        }

        for (idx = 0; idx < available; idx++)
        {
            if (parallel_is_term_char(io, buffer[idx]))
            {
                return idx + 1;
            }
        }

        return 0;
    }

    if (count > available)
    {
        count = available;
    }

    if (io->io_ParFlags & PARF_EOFMODE)
    {
        ULONG idx;

        for (idx = 0; idx < count; idx++)
        {
            if (parallel_is_term_char(io, buffer[idx]))
            {
                return idx + 1;
            }
        }
    }

    return count;
}

static LONG parallel_service_read(struct ParallelBase *parallelbase,
                                  struct IOExtPar *io)
{
    ULONG count;

    if (!io->IOPar.io_Data && io->IOPar.io_Length != 0)
    {
        return IOERR_BADADDRESS;
    }

    if (parallelbase->pb_BufferCount == 0)
    {
        io->IOPar.io_Actual = 0;
        return 1;
    }

    count = parallel_compute_read_length(io,
                                         parallelbase->pb_Buffer,
                                         parallelbase->pb_BufferCount);
    if (count == 0)
    {
        io->IOPar.io_Actual = 0;
        return 1;
    }

    CopyMem(parallelbase->pb_Buffer, io->IOPar.io_Data, count);
    io->IOPar.io_Actual = count;

    parallelbase->pb_BufferCount -= count;
    if (parallelbase->pb_BufferCount != 0)
    {
        CopyMem(parallelbase->pb_Buffer + count,
                parallelbase->pb_Buffer,
                parallelbase->pb_BufferCount);
    }

    return 0;
}

static void parallel_complete_pending_reads(struct ParallelBase *parallelbase)
{
    struct ParallelUnit *unit;

    for (unit = parallelbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_PendingRead)
        {
            LONG result = parallel_service_read(parallelbase, unit->pu_PendingRead);

            if (result == 0)
            {
                unit->pu_PendingRead->IOPar.io_Error = 0;
                parallel_reply_request((struct IORequest *)unit->pu_PendingRead);
                unit->pu_PendingRead = NULL;
            }
        }
    }
}

static void parallel_abort_all_pending_reads(struct ParallelBase *parallelbase,
                                             BYTE error)
{
    struct ParallelUnit *unit;

    for (unit = parallelbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_PendingRead)
        {
            unit->pu_PendingRead->IOPar.io_Error = error;
            unit->pu_PendingRead->IOPar.io_Actual = 0;
            parallel_reply_request((struct IORequest *)unit->pu_PendingRead);
            unit->pu_PendingRead = NULL;
        }
    }
}

static BPTR parallel_expunge_if_possible(struct ParallelBase *parallelbase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)parallelbase->pb_Device.dd_Library.lib_Node.ln_Name) ==
        &parallelbase->pb_Device.dd_Library.lib_Node)
    {
        Remove(&parallelbase->pb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_parallel: Expunge() removed device from DeviceList\n");
    }

    if (parallelbase->pb_Device.dd_Library.lib_OpenCnt != 0)
    {
        parallelbase->pb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_parallel: Expunge() deferred, open count=%u\n",
                (unsigned int)parallelbase->pb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    parallelbase->pb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

    if (parallelbase->pb_Buffer)
    {
        FreeMem(parallelbase->pb_Buffer, PARALLEL_BUFFER_SIZE);
        parallelbase->pb_Buffer = NULL;
    }

    return parallelbase->pb_SegList;
}

static struct Library * __g_lxa_parallel_InitDev(register struct Library *dev __asm("d0"),
                                                  register BPTR seglist __asm("a0"),
                                                  register struct ExecBase *sysb __asm("a6"))
{
    struct ParallelBase *parallelbase = (struct ParallelBase *)dev;

    (void)sysb;

    parallelbase->pb_SegList = seglist;
    parallelbase->pb_Units = NULL;
    parallelbase->pb_BufferCount = 0;
    parallelbase->pb_Status = IOPTF_PARSEL;
    parallelbase->pb_ExclusiveOpen = FALSE;
    parallelbase->pb_Stopped = FALSE;
    parallel_set_default_config(parallelbase);
    parallelbase->pb_Buffer = (UBYTE *)AllocMem(PARALLEL_BUFFER_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!parallelbase->pb_Buffer)
    {
        return NULL;
    }

    return dev;
}

static void __g_lxa_parallel_Open(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"),
                                  register ULONG unit __asm("d0"),
                                  register ULONG flags __asm("d1"))
{
    struct ParallelBase *parallelbase = (struct ParallelBase *)dev;
    struct IOExtPar *io = (struct IOExtPar *)ioreq;
    struct ParallelUnit *parallelunit;
    BOOL shared;

    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (unit >= PARALLEL_UNIT_COUNT || !parallel_request_is_valid(ioreq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    shared = (io->io_ParFlags & PARF_SHARED) != 0;
    if (parallelbase->pb_Device.dd_Library.lib_OpenCnt != 0 &&
        (!shared || parallelbase->pb_ExclusiveOpen))
    {
        ioreq->io_Error = ParErr_DevBusy;
        return;
    }

    parallelunit = (struct ParallelUnit *)AllocMem(sizeof(*parallelunit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!parallelunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    parallelunit->pu_Unit.unit_OpenCnt = 1;
    parallelunit->pu_Shared = shared;
    parallelunit->pu_PendingRead = NULL;
    parallelunit->pu_Next = parallelbase->pb_Units;
    parallelbase->pb_Units = parallelunit;

    ioreq->io_Device = (struct Device *)parallelbase;
    ioreq->io_Unit = (struct Unit *)parallelunit;
    parallel_fill_request_config(parallelbase, io, shared);
    parallelbase->pb_Device.dd_Library.lib_OpenCnt++;
    parallelbase->pb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    parallel_recompute_exclusive(parallelbase);
}

static BPTR __g_lxa_parallel_Close(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct ParallelBase *parallelbase = (struct ParallelBase *)dev;
    struct ParallelUnit *parallelunit = parallel_get_unit(ioreq);
    struct ParallelUnit **link;

    if (parallelunit)
    {
        if (parallelunit->pu_PendingRead)
        {
            parallelunit->pu_PendingRead->IOPar.io_Error = IOERR_ABORTED;
            parallelunit->pu_PendingRead->IOPar.io_Actual = 0;
            parallel_reply_request((struct IORequest *)parallelunit->pu_PendingRead);
            parallelunit->pu_PendingRead = NULL;
        }

        for (link = &parallelbase->pb_Units; *link; link = &(*link)->pu_Next)
        {
            if (*link == parallelunit)
            {
                *link = parallelunit->pu_Next;
                break;
            }
        }

        FreeMem(parallelunit, sizeof(*parallelunit));
        ioreq->io_Unit = NULL;
    }

    ioreq->io_Device = NULL;

    if (parallelbase->pb_Device.dd_Library.lib_OpenCnt > 0)
    {
        parallelbase->pb_Device.dd_Library.lib_OpenCnt--;
    }

    parallel_recompute_exclusive(parallelbase);

    if (parallelbase->pb_Device.dd_Library.lib_OpenCnt == 0 &&
        (parallelbase->pb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return parallel_expunge_if_possible(parallelbase);
    }

    return 0;
}

static BPTR __g_lxa_parallel_Expunge(register struct Library *dev __asm("a6"))
{
    return parallel_expunge_if_possible((struct ParallelBase *)dev);
}

static LONG parallel_apply_setparams(struct ParallelBase *parallelbase,
                                     struct IOExtPar *io,
                                     struct ParallelUnit *parallelunit)
{
    ULONG allowed_flags = PARF_SHARED | PARF_SLOWMODE | PARF_FASTMODE | PARF_ACKMODE | PARF_EOFMODE;
    BOOL shared;

    if (parallel_has_pending_read(parallelbase))
    {
        return ParErr_DevBusy;
    }

    if (io->io_PExtFlags != 0)
    {
        return ParErr_InvParam;
    }

    if ((io->io_ParFlags & ~allowed_flags) != 0)
    {
        return ParErr_InvParam;
    }

    shared = (io->io_ParFlags & PARF_SHARED) != 0;
    if (!shared && parallelbase->pb_Device.dd_Library.lib_OpenCnt > 1)
    {
        return ParErr_DevBusy;
    }

    if (parallelunit)
    {
        parallelunit->pu_Shared = shared;
    }

    parallelbase->pb_Config.io_PExtFlags = 0;
    parallelbase->pb_Config.io_ParFlags = io->io_ParFlags & (PARF_SLOWMODE | PARF_FASTMODE | PARF_ACKMODE | PARF_EOFMODE);
    parallelbase->pb_Config.io_PTermArray = io->io_PTermArray;
    parallel_recompute_exclusive(parallelbase);
    parallel_fill_request_config(parallelbase, io, shared);
    return 0;
}

static BPTR __g_lxa_parallel_BeginIO(register struct Library *dev __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct ParallelBase *parallelbase = (struct ParallelBase *)dev;
    struct ParallelUnit *parallelunit = parallel_get_unit(ioreq);
    struct IOExtPar *io = (struct IOExtPar *)ioreq;
    LONG result = 0;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    io->IOPar.io_Actual = 0;

    if (!parallelunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        parallel_reply_request(ioreq);
        return 0;
    }

    switch (ioreq->io_Command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct NSDeviceQueryResult *query = (struct NSDeviceQueryResult *)io->IOPar.io_Data;

            if (!query)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (io->IOPar.io_Length < sizeof(*query))
            {
                result = IOERR_BADLENGTH;
                break;
            }

            query->nsdqr_DevQueryFormat = 0;
            query->nsdqr_SizeAvailable = sizeof(*query);
            query->nsdqr_DeviceType = NSDEVTYPE_PARALLEL;
            query->nsdqr_DeviceSubType = 0;
            query->nsdqr_SupportedCommands = (APTR)parallel_supported_commands;
            io->IOPar.io_Actual = sizeof(*query);
            break;
        }

        case CMD_CLEAR:
            parallelbase->pb_BufferCount = 0;
            break;

        case CMD_FLUSH:
            parallelbase->pb_BufferCount = 0;
            parallel_abort_all_pending_reads(parallelbase, IOERR_ABORTED);
            break;

        case CMD_RESET:
            parallelbase->pb_BufferCount = 0;
            parallelbase->pb_Status = IOPTF_PARSEL;
            parallelbase->pb_Stopped = FALSE;
            parallel_set_default_config(parallelbase);
            parallel_fill_request_config(parallelbase, io, parallelunit->pu_Shared);
            break;

        case CMD_START:
            parallelbase->pb_Stopped = FALSE;
            parallel_complete_pending_reads(parallelbase);
            break;

        case CMD_STOP:
            parallelbase->pb_Stopped = TRUE;
            break;

        case CMD_WRITE:
        {
            ULONG length;

            if (!io->IOPar.io_Data && io->IOPar.io_Length != 0)
            {
                result = IOERR_BADADDRESS;
                break;
            }

            length = parallel_compute_write_length(io);
            result = parallel_append_buffer(parallelbase,
                                            (const UBYTE *)io->IOPar.io_Data,
                                            length);
            if (result == 0)
            {
                io->IOPar.io_Actual = length;
                parallelbase->pb_Status |= IOPTF_RWDIR;
                if (!parallelbase->pb_Stopped)
                {
                    parallel_complete_pending_reads(parallelbase);
                }
            }
            break;
        }

        case CMD_READ:
            parallelbase->pb_Status &= (UBYTE)~IOPTF_RWDIR;
            if (parallelbase->pb_Stopped)
            {
                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                parallelunit->pu_PendingRead = io;
                return 0;
            }

            result = parallel_service_read(parallelbase, io);
            if (result == 1)
            {
                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                parallelunit->pu_PendingRead = io;
                return 0;
            }
            break;

        case PDCMD_QUERY:
            io->io_Status = parallelbase->pb_Status;
            break;

        case PDCMD_SETPARAMS:
            result = parallel_apply_setparams(parallelbase, io, parallelunit);
            break;

        default:
            result = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = result;
    parallel_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_parallel_AbortIO(register struct Library *dev __asm("a6"),
                                      register struct IORequest *ioreq __asm("a1"))
{
    struct ParallelUnit *parallelunit = parallel_get_unit(ioreq);

    (void)dev;

    if (parallelunit && parallelunit->pu_PendingRead == (struct IOExtPar *)ioreq)
    {
        parallelunit->pu_PendingRead = NULL;
        ioreq->io_Error = IOERR_ABORTED;
        ((struct IOStdReq *)ioreq)->io_Actual = 0;
        ReplyMsg(&ioreq->io_Message);
        return 0;
    }

    return (ULONG)-1;
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

extern APTR              __g_lxa_parallel_FuncTab [];
extern struct MyDataInit __g_lxa_parallel_DataTab;
extern struct InitTable  __g_lxa_parallel_InitTab;
extern APTR              __g_lxa_parallel_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_parallel_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_parallel_ExDevName[0],
    &_g_parallel_ExDevID[0],
    &__g_lxa_parallel_InitTab
};

APTR __g_lxa_parallel_EndResident;
struct Resident *__lxa_parallel_ROMTag = &ROMTag;

struct InitTable __g_lxa_parallel_InitTab =
{
    (ULONG)sizeof(struct ParallelBase),
    (APTR *)&__g_lxa_parallel_FuncTab[0],
    (APTR)&__g_lxa_parallel_DataTab,
    (APTR)__g_lxa_parallel_InitDev
};

APTR __g_lxa_parallel_FuncTab [] =
{
    __g_lxa_parallel_Open,
    __g_lxa_parallel_Close,
    __g_lxa_parallel_Expunge,
    NULL,
    __g_lxa_parallel_BeginIO,
    __g_lxa_parallel_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_parallel_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_parallel_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_parallel_ExDevID[0],
    (ULONG)0
};
