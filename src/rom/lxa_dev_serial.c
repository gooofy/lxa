/*
 * lxa serial.device implementation
 *
 * Hosted compatibility implementation with shared/exclusive opens,
 * configurable line parameters, loopback-backed reads/writes, and
 * basic query/break semantics.
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
#include <devices/serial.h>

#include "util.h"

#define VERSION    40
#define REVISION   1
#define EXDEVNAME  "serial"
#define EXDEVVER   " 40.1 (2026/03/17)"

#define SERIAL_UNIT_COUNT           1
#define SERIAL_DEFAULT_BAUD         9600
#define SERIAL_DEFAULT_BUFFER_SIZE  1024
#define SERIAL_DEFAULT_BREAK_MICROS 250000

char __aligned _g_serial_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_serial_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_serial_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_serial_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct SerialConfig
{
    ULONG io_CtlChar;
    ULONG io_RBufLen;
    ULONG io_ExtFlags;
    ULONG io_Baud;
    ULONG io_BrkTime;
    struct IOTArray io_TermArray;
    UBYTE io_ReadLen;
    UBYTE io_WriteLen;
    UBYTE io_StopBits;
    UBYTE io_SerFlags;
};

struct SerialUnit
{
    struct Unit su_Unit;
    struct SerialUnit *su_Next;
    struct IOExtSer *su_PendingRead;
    BOOL su_Shared;
};

struct SerialBase
{
    struct Device sb_Device;
    BPTR sb_SegList;
    struct SerialUnit *sb_Units;
    struct SerialConfig sb_Config;
    UBYTE *sb_ReadBuffer;
    ULONG sb_ReadCount;
    UWORD sb_Status;
    BOOL sb_ExclusiveOpen;
    BOOL sb_Stopped;
};

static const UWORD serial_supported_commands[] =
{
    CMD_CLEAR,
    CMD_FLUSH,
    CMD_READ,
    CMD_RESET,
    CMD_START,
    CMD_STOP,
    CMD_WRITE,
    SDCMD_QUERY,
    SDCMD_BREAK,
    SDCMD_SETPARAMS,
    NSCMD_DEVICEQUERY,
    0
};

static void serial_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static void serial_set_default_config(struct SerialBase *serialbase)
{
    serialbase->sb_Config.io_CtlChar = SER_DEFAULT_CTLCHAR;
    serialbase->sb_Config.io_RBufLen = SERIAL_DEFAULT_BUFFER_SIZE;
    serialbase->sb_Config.io_ExtFlags = 0;
    serialbase->sb_Config.io_Baud = SERIAL_DEFAULT_BAUD;
    serialbase->sb_Config.io_BrkTime = SERIAL_DEFAULT_BREAK_MICROS;
    serialbase->sb_Config.io_TermArray.TermArray0 = 0;
    serialbase->sb_Config.io_TermArray.TermArray1 = 0;
    serialbase->sb_Config.io_ReadLen = 8;
    serialbase->sb_Config.io_WriteLen = 8;
    serialbase->sb_Config.io_StopBits = 1;
    serialbase->sb_Config.io_SerFlags = 0;
}

static void serial_fill_request_config(struct SerialBase *serialbase,
                                       struct IOExtSer *io,
                                       BOOL shared)
{
    if (!serialbase || !io)
    {
        return;
    }

    io->io_CtlChar = serialbase->sb_Config.io_CtlChar;
    io->io_RBufLen = serialbase->sb_Config.io_RBufLen;
    io->io_ExtFlags = serialbase->sb_Config.io_ExtFlags;
    io->io_Baud = serialbase->sb_Config.io_Baud;
    io->io_BrkTime = serialbase->sb_Config.io_BrkTime;
    io->io_TermArray = serialbase->sb_Config.io_TermArray;
    io->io_ReadLen = serialbase->sb_Config.io_ReadLen;
    io->io_WriteLen = serialbase->sb_Config.io_WriteLen;
    io->io_StopBits = serialbase->sb_Config.io_StopBits;
    io->io_SerFlags = serialbase->sb_Config.io_SerFlags | (shared ? SERF_SHARED : 0);
    io->io_Status = serialbase->sb_Status;
}

static BOOL serial_request_is_valid(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IOExtSer);
}

static struct SerialUnit *serial_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct SerialUnit *)ioreq->io_Unit;
}

static BOOL serial_has_pending_read(const struct SerialBase *serialbase)
{
    struct SerialUnit *unit;

    for (unit = serialbase->sb_Units; unit; unit = unit->su_Next)
    {
        if (unit->su_PendingRead)
        {
            return TRUE;
        }
    }

    return FALSE;
}

static LONG serial_resize_buffer(struct SerialBase *serialbase, ULONG size, BOOL preserve)
{
    UBYTE *buffer;

    if (size < 64 || (size & 63) != 0)
    {
        return SerErr_InvParam;
    }

    buffer = (UBYTE *)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    if (!buffer)
    {
        return SerErr_BufErr;
    }

    if (preserve && serialbase->sb_ReadBuffer && serialbase->sb_ReadCount != 0)
    {
        ULONG copy = serialbase->sb_ReadCount;

        if (copy > size)
        {
            copy = size;
        }

        CopyMem(serialbase->sb_ReadBuffer, buffer, copy);
        serialbase->sb_ReadCount = copy;
    }
    else
    {
        serialbase->sb_ReadCount = 0;
    }

    if (serialbase->sb_ReadBuffer)
    {
        FreeMem(serialbase->sb_ReadBuffer, serialbase->sb_Config.io_RBufLen);
    }

    serialbase->sb_ReadBuffer = buffer;
    return 0;
}

static LONG serial_append_input(struct SerialBase *serialbase, const UBYTE *data, ULONG length)
{
    ULONG available;

    if (length == 0)
    {
        return 0;
    }

    available = serialbase->sb_Config.io_RBufLen - serialbase->sb_ReadCount;
    if (length > available)
    {
        return SerErr_BufOverflow;
    }

    CopyMem((APTR)data,
            serialbase->sb_ReadBuffer + serialbase->sb_ReadCount,
            length);
    serialbase->sb_ReadCount += length;
    return 0;
}

static BOOL serial_is_term_char(const struct IOExtSer *io, UBYTE value)
{
    const UBYTE *terms = (const UBYTE *)&io->io_TermArray;
    int idx;

    if (!(io->io_SerFlags & SERF_EOFMODE))
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

static ULONG serial_compute_read_length(struct IOExtSer *io,
                                        const UBYTE *buffer,
                                        ULONG available)
{
    ULONG count;

    count = io->IOSer.io_Length;
    if (count > available)
    {
        count = available;
    }

    if (io->io_SerFlags & SERF_EOFMODE)
    {
        ULONG idx;

        for (idx = 0; idx < count; idx++)
        {
            if (serial_is_term_char(io, buffer[idx]))
            {
                return idx + 1;
            }
        }
    }

    return count;
}

static LONG serial_service_read(struct SerialBase *serialbase,
                                struct IOExtSer *io)
{
    ULONG count;

    if (!io->IOSer.io_Data && io->IOSer.io_Length != 0)
    {
        return IOERR_BADADDRESS;
    }

    if (serialbase->sb_ReadCount == 0)
    {
        io->IOSer.io_Actual = 0;
        return 1;
    }

    count = serial_compute_read_length(io,
                                       serialbase->sb_ReadBuffer,
                                       serialbase->sb_ReadCount);
    if (count == 0)
    {
        io->IOSer.io_Actual = 0;
        return 1;
    }

    CopyMem(serialbase->sb_ReadBuffer, io->IOSer.io_Data, count);
    io->IOSer.io_Actual = count;

    serialbase->sb_ReadCount -= count;
    if (serialbase->sb_ReadCount != 0)
    {
        CopyMem(serialbase->sb_ReadBuffer + count,
                serialbase->sb_ReadBuffer,
                serialbase->sb_ReadCount);
    }

    return 0;
}

static void serial_complete_pending_reads(struct SerialBase *serialbase)
{
    struct SerialUnit *unit;

    for (unit = serialbase->sb_Units; unit; unit = unit->su_Next)
    {
        if (unit->su_PendingRead)
        {
            LONG result = serial_service_read(serialbase, unit->su_PendingRead);

            if (result == 0)
            {
                unit->su_PendingRead->IOSer.io_Error = 0;
                serial_reply_request((struct IORequest *)unit->su_PendingRead);
                unit->su_PendingRead = NULL;
            }
        }
    }
}

static void serial_abort_all_pending_reads(struct SerialBase *serialbase, BYTE error)
{
    struct SerialUnit *unit;

    for (unit = serialbase->sb_Units; unit; unit = unit->su_Next)
    {
        if (unit->su_PendingRead)
        {
            unit->su_PendingRead->IOSer.io_Error = error;
            unit->su_PendingRead->IOSer.io_Actual = 0;
            serial_reply_request((struct IORequest *)unit->su_PendingRead);
            unit->su_PendingRead = NULL;
        }
    }
}

static BPTR serial_expunge_if_possible(struct SerialBase *serialbase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)serialbase->sb_Device.dd_Library.lib_Node.ln_Name) ==
        &serialbase->sb_Device.dd_Library.lib_Node)
    {
        Remove(&serialbase->sb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_serial: Expunge() removed device from DeviceList\n");
    }

    if (serialbase->sb_Device.dd_Library.lib_OpenCnt != 0)
    {
        serialbase->sb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_serial: Expunge() deferred, open count=%u\n",
                (unsigned int)serialbase->sb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    serialbase->sb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

    if (serialbase->sb_ReadBuffer)
    {
        FreeMem(serialbase->sb_ReadBuffer, serialbase->sb_Config.io_RBufLen);
        serialbase->sb_ReadBuffer = NULL;
    }

    return serialbase->sb_SegList;
}

static struct Library * __g_lxa_serial_InitDev(register struct Library *dev __asm("d0"),
                                                register BPTR seglist __asm("a0"),
                                                register struct ExecBase *sysb __asm("a6"))
{
    struct SerialBase *serialbase = (struct SerialBase *)dev;
    LONG error;

    (void)sysb;

    serialbase->sb_SegList = seglist;
    serialbase->sb_Units = NULL;
    serialbase->sb_Status = 0;
    serialbase->sb_ExclusiveOpen = FALSE;
    serialbase->sb_Stopped = FALSE;
    serialbase->sb_ReadBuffer = NULL;
    serial_set_default_config(serialbase);
    error = serial_resize_buffer(serialbase, serialbase->sb_Config.io_RBufLen, FALSE);
    if (error != 0)
    {
        return NULL;
    }

    return dev;
}

static void __g_lxa_serial_Open(register struct Library *dev __asm("a6"),
                                register struct IORequest *ioreq __asm("a1"),
                                register ULONG unit __asm("d0"),
                                register ULONG flags __asm("d1"))
{
    struct SerialBase *serialbase = (struct SerialBase *)dev;
    struct IOExtSer *io = (struct IOExtSer *)ioreq;
    struct SerialUnit *serialunit;
    BOOL shared;

    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (unit >= SERIAL_UNIT_COUNT || !serial_request_is_valid(ioreq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    shared = (io->io_SerFlags & SERF_SHARED) != 0;
    if (serialbase->sb_Device.dd_Library.lib_OpenCnt != 0 &&
        (!shared || serialbase->sb_ExclusiveOpen))
    {
        ioreq->io_Error = SerErr_DevBusy;
        return;
    }

    serialunit = (struct SerialUnit *)AllocMem(sizeof(*serialunit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!serialunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    serialunit->su_Unit.unit_OpenCnt = 1;
    serialunit->su_Shared = shared;
    serialunit->su_PendingRead = NULL;
    serialunit->su_Next = serialbase->sb_Units;
    serialbase->sb_Units = serialunit;

    if (!shared)
    {
        serialbase->sb_ExclusiveOpen = TRUE;
    }

    ioreq->io_Device = (struct Device *)serialbase;
    ioreq->io_Unit = (struct Unit *)serialunit;
    serial_fill_request_config(serialbase, io, shared);
    serialbase->sb_Device.dd_Library.lib_OpenCnt++;
    serialbase->sb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_serial_Close(register struct Library *dev __asm("a6"),
                                 register struct IORequest *ioreq __asm("a1"))
{
    struct SerialBase *serialbase = (struct SerialBase *)dev;
    struct SerialUnit *serialunit = serial_get_unit(ioreq);
    struct SerialUnit **link;
    BOOL any_exclusive = FALSE;
    struct SerialUnit *scan;

    if (serialunit)
    {
        if (serialunit->su_PendingRead)
        {
            serialunit->su_PendingRead->IOSer.io_Error = IOERR_ABORTED;
            serialunit->su_PendingRead->IOSer.io_Actual = 0;
            serial_reply_request((struct IORequest *)serialunit->su_PendingRead);
            serialunit->su_PendingRead = NULL;
        }

        for (link = &serialbase->sb_Units; *link; link = &(*link)->su_Next)
        {
            if (*link == serialunit)
            {
                *link = serialunit->su_Next;
                break;
            }
        }

        FreeMem(serialunit, sizeof(*serialunit));
        ioreq->io_Unit = NULL;
    }

    for (scan = serialbase->sb_Units; scan; scan = scan->su_Next)
    {
        if (!scan->su_Shared)
        {
            any_exclusive = TRUE;
            break;
        }
    }

    serialbase->sb_ExclusiveOpen = any_exclusive;
    ioreq->io_Device = NULL;

    if (serialbase->sb_Device.dd_Library.lib_OpenCnt > 0)
    {
        serialbase->sb_Device.dd_Library.lib_OpenCnt--;
    }

    if (serialbase->sb_Device.dd_Library.lib_OpenCnt == 0 &&
        (serialbase->sb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return serial_expunge_if_possible(serialbase);
    }

    return 0;
}

static BPTR __g_lxa_serial_Expunge(register struct Library *dev __asm("a6"))
{
    return serial_expunge_if_possible((struct SerialBase *)dev);
}

static LONG serial_apply_setparams(struct SerialBase *serialbase,
                                   struct IOExtSer *io,
                                   struct SerialUnit *serialunit)
{
    ULONG new_flags;
    LONG error;

    if (serial_has_pending_read(serialbase))
    {
        UBYTE current_xdisabled = serialbase->sb_Config.io_SerFlags & SERF_XDISABLED;
        UBYTE requested_xdisabled = io->io_SerFlags & SERF_XDISABLED;

        if (current_xdisabled != requested_xdisabled)
        {
            serialbase->sb_Config.io_SerFlags &= (UBYTE)~SERF_XDISABLED;
            serialbase->sb_Config.io_SerFlags |= requested_xdisabled;
            serial_fill_request_config(serialbase, io, serialunit && serialunit->su_Shared);
            return SerErr_DevBusy;
        }

        return SerErr_DevBusy;
    }

    if (io->io_Baud < 112 || io->io_Baud > 292000)
    {
        return SerErr_InvParam;
    }

    if (io->io_ReadLen == 0 || io->io_ReadLen > 8 ||
        io->io_WriteLen == 0 || io->io_WriteLen > 8 ||
        io->io_StopBits > 2)
    {
        return SerErr_InvParam;
    }

    if (io->io_ExtFlags & ~(SEXTF_MSPON | SEXTF_MARK))
    {
        return SerErr_InvParam;
    }

    if (io->io_RBufLen != serialbase->sb_Config.io_RBufLen)
    {
        error = serial_resize_buffer(serialbase, io->io_RBufLen, FALSE);
        if (error != 0)
        {
            return error;
        }
        serialbase->sb_Config.io_RBufLen = io->io_RBufLen;
    }

    new_flags = io->io_SerFlags & (SERF_XDISABLED | SERF_EOFMODE | SERF_RAD_BOOGIE | SERF_7WIRE | SERF_PARTY_ODD | SERF_PARTY_ON | SERF_QUEUEDBRK);
    if (new_flags & SERF_RAD_BOOGIE)
    {
        new_flags |= SERF_XDISABLED;
    }

    serialbase->sb_Config.io_CtlChar = io->io_CtlChar;
    serialbase->sb_Config.io_ExtFlags = io->io_ExtFlags;
    serialbase->sb_Config.io_Baud = io->io_Baud;
    serialbase->sb_Config.io_BrkTime = io->io_BrkTime;
    serialbase->sb_Config.io_TermArray = io->io_TermArray;
    serialbase->sb_Config.io_ReadLen = io->io_ReadLen;
    serialbase->sb_Config.io_WriteLen = io->io_WriteLen;
    serialbase->sb_Config.io_StopBits = io->io_StopBits;
    serialbase->sb_Config.io_SerFlags = (UBYTE)new_flags;
    serial_fill_request_config(serialbase, io, serialunit && serialunit->su_Shared);
    return 0;
}

static BPTR __g_lxa_serial_BeginIO(register struct Library *dev __asm("a6"),
                                   register struct IORequest *ioreq __asm("a1"))
{
    struct SerialBase *serialbase = (struct SerialBase *)dev;
    struct SerialUnit *serialunit = serial_get_unit(ioreq);
    struct IOExtSer *io = (struct IOExtSer *)ioreq;
    LONG result = 0;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    io->IOSer.io_Actual = 0;

    if (!serialunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        serial_reply_request(ioreq);
        return 0;
    }

    switch (ioreq->io_Command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct NSDeviceQueryResult *query = (struct NSDeviceQueryResult *)io->IOSer.io_Data;

            if (!query)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (io->IOSer.io_Length < sizeof(*query))
            {
                result = IOERR_BADLENGTH;
                break;
            }

            query->nsdqr_DevQueryFormat = 0;
            query->nsdqr_SizeAvailable = sizeof(*query);
            query->nsdqr_DeviceType = NSDEVTYPE_SERIAL;
            query->nsdqr_DeviceSubType = 0;
            query->nsdqr_SupportedCommands = (APTR)serial_supported_commands;
            io->IOSer.io_Actual = sizeof(*query);
            break;
        }

        case CMD_CLEAR:
            serialbase->sb_ReadCount = 0;
            break;

        case CMD_FLUSH:
            serialbase->sb_ReadCount = 0;
            serial_abort_all_pending_reads(serialbase, IOERR_ABORTED);
            break;

        case CMD_RESET:
            serialbase->sb_ReadCount = 0;
            serialbase->sb_Status = 0;
            serialbase->sb_Stopped = FALSE;
            serial_set_default_config(serialbase);
            result = serial_resize_buffer(serialbase, serialbase->sb_Config.io_RBufLen, FALSE);
            if (result == 0)
            {
                serial_fill_request_config(serialbase, io, serialunit->su_Shared);
            }
            break;

        case CMD_START:
            serialbase->sb_Stopped = FALSE;
            serial_complete_pending_reads(serialbase);
            break;

        case CMD_STOP:
            serialbase->sb_Stopped = TRUE;
            break;

        case CMD_WRITE:
            if (!io->IOSer.io_Data && io->IOSer.io_Length != 0)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            result = serial_append_input(serialbase,
                                         (const UBYTE *)io->IOSer.io_Data,
                                         io->IOSer.io_Length);
            if (result == 0)
            {
                io->IOSer.io_Actual = io->IOSer.io_Length;
                if (!serialbase->sb_Stopped)
                {
                    serial_complete_pending_reads(serialbase);
                }
            }
            break;

        case CMD_READ:
            if (serialbase->sb_Stopped)
            {
                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                serialunit->su_PendingRead = io;
                return 0;
            }

            result = serial_service_read(serialbase, io);
            if (result == 1)
            {
                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                serialunit->su_PendingRead = io;
                return 0;
            }
            break;

        case SDCMD_QUERY:
            io->io_Status = serialbase->sb_Status;
            io->IOSer.io_Actual = serialbase->sb_ReadCount;
            serialbase->sb_Status &= (UWORD)~(IO_STATF_WROTEBREAK | IO_STATF_READBREAK);
            break;

        case SDCMD_BREAK:
            serialbase->sb_Status |= IO_STATF_WROTEBREAK;
            break;

        case SDCMD_SETPARAMS:
            result = serial_apply_setparams(serialbase, io, serialunit);
            break;

        default:
            result = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = result;
    serial_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_serial_AbortIO(register struct Library *dev __asm("a6"),
                                    register struct IORequest *ioreq __asm("a1"))
{
    struct SerialUnit *serialunit = serial_get_unit(ioreq);

    (void)dev;

    if (serialunit && serialunit->su_PendingRead == (struct IOExtSer *)ioreq)
    {
        serialunit->su_PendingRead = NULL;
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

extern APTR              __g_lxa_serial_FuncTab [];
extern struct MyDataInit __g_lxa_serial_DataTab;
extern struct InitTable  __g_lxa_serial_InitTab;
extern APTR              __g_lxa_serial_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_serial_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_serial_ExDevName[0],
    &_g_serial_ExDevID[0],
    &__g_lxa_serial_InitTab
};

APTR __g_lxa_serial_EndResident;
struct Resident *__lxa_serial_ROMTag = &ROMTag;

struct InitTable __g_lxa_serial_InitTab =
{
    (ULONG)sizeof(struct SerialBase),
    (APTR *)&__g_lxa_serial_FuncTab[0],
    (APTR)&__g_lxa_serial_DataTab,
    (APTR)__g_lxa_serial_InitDev
};

APTR __g_lxa_serial_FuncTab [] =
{
    __g_lxa_serial_Open,
    __g_lxa_serial_Close,
    __g_lxa_serial_Expunge,
    NULL,
    __g_lxa_serial_BeginIO,
    __g_lxa_serial_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_serial_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_serial_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_serial_ExDevID[0],
    (ULONG)0
};
