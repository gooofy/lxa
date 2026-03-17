/*
 * lxa printer.device implementation
 *
 * Hosted compatibility implementation with buffered writes, basic printer
 * status queries, limited printer command translation, and stop/start queueing.
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
#include <devices/printer.h>

#include "util.h"

#define VERSION  40
#define REVISION 1
#define EXDEVNAME "printer"
#define EXDEVVER  " 40.1 (2026/03/17)"

#define PRINTER_UNIT_COUNT   1
#define PRINTER_BUFFER_SIZE  8192
#define PRINTER_STATUS_READY 0x04

char __aligned _g_printer_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_printer_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_printer_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_printer_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct PrinterUnit
{
    struct Unit pu_Unit;
    struct PrinterUnit *pu_Next;
    struct IORequest *pu_Pending;
};

struct PrinterBase
{
    struct Device pb_Device;
    BPTR pb_SegList;
    struct PrinterUnit *pb_Units;
    UBYTE *pb_Buffer;
    ULONG pb_BufferCount;
    BOOL pb_Stopped;
    UBYTE pb_Status[2];
};

static const UWORD printer_supported_commands[] =
{
    CMD_FLUSH,
    CMD_RESET,
    CMD_START,
    CMD_STOP,
    CMD_WRITE,
    PRD_QUERY,
    PRD_RAWWRITE,
    PRD_PRTCOMMAND,
    PRD_DUMPRPORT,
    PRD_DUMPRPORTTAGS,
    NSCMD_DEVICEQUERY,
    0
};

static void printer_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static BOOL printer_request_is_valid(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IOStdReq);
}

static BOOL printer_request_has_iostd(const struct IORequest *ioreq)
{
    return printer_request_is_valid(ioreq);
}

static BOOL printer_request_has_prtcmd(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IOPrtCmdReq);
}

static BOOL printer_request_has_dumprport(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IODRPReq);
}

static BOOL printer_request_has_dumprporttags(const struct IORequest *ioreq)
{
    return ioreq && ioreq->io_Message.mn_Length >= sizeof(struct IODRPTagsReq);
}

static struct PrinterUnit *printer_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct PrinterUnit *)ioreq->io_Unit;
}

static ULONG printer_compute_write_length(const struct IOStdReq *io)
{
    const UBYTE *data = (const UBYTE *)io->io_Data;
    ULONG length = io->io_Length;

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

static LONG printer_append_buffer(struct PrinterBase *printerbase,
                                  const UBYTE *data,
                                  ULONG length)
{
    ULONG available;

    if (length == 0)
    {
        return 0;
    }

    available = PRINTER_BUFFER_SIZE - printerbase->pb_BufferCount;
    if (length > available)
    {
        return PDERR_BUFFERMEMORY;
    }

    CopyMem((APTR)data,
            printerbase->pb_Buffer + printerbase->pb_BufferCount,
            length);
    printerbase->pb_BufferCount += length;
    return 0;
}

static LONG printer_append_prtcommand(struct PrinterBase *printerbase,
                                      const struct IOPrtCmdReq *io)
{
    static const UBYTE cmd_ris[] = { 0x1b, 'c' };
    static const UBYTE cmd_rin[] = { 0x1b, '#', '1' };
    static const UBYTE cmd_ind[] = { 0x1b, 'D' };
    static const UBYTE cmd_nel[] = { 0x1b, 'E' };
    static const UBYTE cmd_ri[]  = { 0x1b, 'M' };
    static const UBYTE cmd_sgr0[] = { 0x1b, '[', '0', 'm' };
    static const UBYTE cmd_sgr1[] = { 0x1b, '[', '1', 'm' };
    static const UBYTE cmd_sgr22[] = { 0x1b, '[', '2', '2', 'm' };
    static const UBYTE cmd_cam[] = { 0x1b, '#', '3' };
    const UBYTE *data = NULL;
    ULONG length = 0;

    switch (io->io_PrtCommand)
    {
        case aRIS:
            data = cmd_ris;
            length = sizeof(cmd_ris);
            break;

        case aRIN:
            data = cmd_rin;
            length = sizeof(cmd_rin);
            break;

        case aIND:
            data = cmd_ind;
            length = sizeof(cmd_ind);
            break;

        case aNEL:
            data = cmd_nel;
            length = sizeof(cmd_nel);
            break;

        case aRI:
            data = cmd_ri;
            length = sizeof(cmd_ri);
            break;

        case aSGR0:
            data = cmd_sgr0;
            length = sizeof(cmd_sgr0);
            break;

        case aSGR1:
            data = cmd_sgr1;
            length = sizeof(cmd_sgr1);
            break;

        case aSGR22:
            data = cmd_sgr22;
            length = sizeof(cmd_sgr22);
            break;

        case aCAM:
            data = cmd_cam;
            length = sizeof(cmd_cam);
            break;

        default:
            return -1;
    }

    return printer_append_buffer(printerbase, data, length);
}

static void printer_abort_request(struct IORequest *ioreq, BYTE error)
{
    if (printer_request_has_iostd(ioreq))
    {
        struct IOStdReq *iostd = (struct IOStdReq *)ioreq;

        iostd->io_Actual = 0;
    }

    ioreq->io_Error = error;
    printer_reply_request(ioreq);
}

static void printer_abort_all_pending(struct PrinterBase *printerbase, BYTE error)
{
    struct PrinterUnit *unit;

    for (unit = printerbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_Pending)
        {
            struct IORequest *pending = unit->pu_Pending;

            unit->pu_Pending = NULL;
            printer_abort_request(pending, error);
        }
    }
}

static LONG printer_execute_request(struct PrinterBase *printerbase,
                                    struct IORequest *ioreq,
                                    BOOL allow_pending);

static void printer_complete_pending(struct PrinterBase *printerbase)
{
    struct PrinterUnit *unit;

    for (unit = printerbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_Pending)
        {
            struct IORequest *pending = unit->pu_Pending;

            unit->pu_Pending = NULL;
            pending->io_Error = printer_execute_request(printerbase, pending, FALSE);
            printer_reply_request(pending);
        }
    }
}

static BPTR printer_expunge_if_possible(struct PrinterBase *printerbase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)printerbase->pb_Device.dd_Library.lib_Node.ln_Name) ==
        &printerbase->pb_Device.dd_Library.lib_Node)
    {
        Remove(&printerbase->pb_Device.dd_Library.lib_Node);
        DPRINTF(LOG_DEBUG, "_printer: Expunge() removed device from DeviceList\n");
    }

    if (printerbase->pb_Device.dd_Library.lib_OpenCnt != 0)
    {
        printerbase->pb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        DPRINTF(LOG_DEBUG, "_printer: Expunge() deferred, open count=%u\n",
                (unsigned int)printerbase->pb_Device.dd_Library.lib_OpenCnt);
        return 0;
    }

    printerbase->pb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

    if (printerbase->pb_Buffer)
    {
        FreeMem(printerbase->pb_Buffer, PRINTER_BUFFER_SIZE);
        printerbase->pb_Buffer = NULL;
    }

    return printerbase->pb_SegList;
}

static struct Library * __g_lxa_printer_InitDev(register struct Library *dev __asm("d0"),
                                                register BPTR seglist __asm("a0"),
                                                register struct ExecBase *sysb __asm("a6"))
{
    struct PrinterBase *printerbase = (struct PrinterBase *)dev;

    (void)sysb;

    printerbase->pb_SegList = seglist;
    printerbase->pb_Units = NULL;
    printerbase->pb_BufferCount = 0;
    printerbase->pb_Stopped = FALSE;
    printerbase->pb_Status[0] = PRINTER_STATUS_READY;
    printerbase->pb_Status[1] = 0;
    printerbase->pb_Buffer = (UBYTE *)AllocMem(PRINTER_BUFFER_SIZE, MEMF_PUBLIC | MEMF_CLEAR);
    if (!printerbase->pb_Buffer)
    {
        return NULL;
    }

    return dev;
}

static void __g_lxa_printer_Open(register struct Library *dev __asm("a6"),
                                 register struct IORequest *ioreq __asm("a1"),
                                 register ULONG unit __asm("d0"),
                                 register ULONG flags __asm("d1"))
{
    struct PrinterBase *printerbase = (struct PrinterBase *)dev;
    struct PrinterUnit *printerunit;

    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    if (unit >= PRINTER_UNIT_COUNT || !printer_request_is_valid(ioreq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    printerunit = (struct PrinterUnit *)AllocMem(sizeof(*printerunit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!printerunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    printerunit->pu_Unit.unit_OpenCnt = 1;
    printerunit->pu_Pending = NULL;
    printerunit->pu_Next = printerbase->pb_Units;
    printerbase->pb_Units = printerunit;

    ioreq->io_Device = (struct Device *)printerbase;
    ioreq->io_Unit = (struct Unit *)printerunit;
    printerbase->pb_Device.dd_Library.lib_OpenCnt++;
    printerbase->pb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
}

static BPTR __g_lxa_printer_Close(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"))
{
    struct PrinterBase *printerbase = (struct PrinterBase *)dev;
    struct PrinterUnit *printerunit = printer_get_unit(ioreq);
    struct PrinterUnit **link;

    if (printerunit)
    {
        if (printerunit->pu_Pending)
        {
            struct IORequest *pending = printerunit->pu_Pending;

            printerunit->pu_Pending = NULL;
            printer_abort_request(pending, IOERR_ABORTED);
        }

        for (link = &printerbase->pb_Units; *link; link = &(*link)->pu_Next)
        {
            if (*link == printerunit)
            {
                *link = printerunit->pu_Next;
                break;
            }
        }

        FreeMem(printerunit, sizeof(*printerunit));
        ioreq->io_Unit = NULL;
    }

    ioreq->io_Device = NULL;

    if (printerbase->pb_Device.dd_Library.lib_OpenCnt > 0)
    {
        printerbase->pb_Device.dd_Library.lib_OpenCnt--;
    }

    if (printerbase->pb_Device.dd_Library.lib_OpenCnt == 0 &&
        (printerbase->pb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return printer_expunge_if_possible(printerbase);
    }

    return 0;
}

static BPTR __g_lxa_printer_Expunge(register struct Library *dev __asm("a6"))
{
    return printer_expunge_if_possible((struct PrinterBase *)dev);
}

static LONG printer_execute_request(struct PrinterBase *printerbase,
                                    struct IORequest *ioreq,
                                    BOOL allow_pending)
{
    struct PrinterUnit *printerunit = printer_get_unit(ioreq);

    if (!printerunit)
    {
        return IOERR_OPENFAIL;
    }

    switch (ioreq->io_Command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct IOStdReq *io;
            struct NSDeviceQueryResult *query;

            if (!printer_request_has_iostd(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            io = (struct IOStdReq *)ioreq;
            query = (struct NSDeviceQueryResult *)io->io_Data;

            if (!query)
            {
                return IOERR_BADADDRESS;
            }
            if (io->io_Length < sizeof(*query))
            {
                return IOERR_BADLENGTH;
            }

            query->nsdqr_DevQueryFormat = 0;
            query->nsdqr_SizeAvailable = sizeof(*query);
            query->nsdqr_DeviceType = NSDEVTYPE_PRINTER;
            query->nsdqr_DeviceSubType = 0;
            query->nsdqr_SupportedCommands = (APTR)printer_supported_commands;
            io->io_Actual = sizeof(*query);
            return 0;
        }

        case CMD_FLUSH:
            printerbase->pb_BufferCount = 0;
            printer_abort_all_pending(printerbase, IOERR_ABORTED);
            return 0;

        case CMD_RESET:
            printerbase->pb_BufferCount = 0;
            printerbase->pb_Stopped = FALSE;
            printer_abort_all_pending(printerbase, IOERR_ABORTED);
            printerbase->pb_Status[0] = PRINTER_STATUS_READY;
            printerbase->pb_Status[1] = 0;
            return 0;

        case CMD_START:
            printerbase->pb_Stopped = FALSE;
            printer_complete_pending(printerbase);
            return 0;

        case CMD_STOP:
            printerbase->pb_Stopped = TRUE;
            return 0;

        case CMD_WRITE:
        case PRD_RAWWRITE:
        {
            struct IOStdReq *io;
            ULONG length;

            if (!printer_request_has_iostd(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            if (printerbase->pb_Stopped && allow_pending)
            {
                if (printerunit->pu_Pending)
                {
                    return IOERR_UNITBUSY;
                }

                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                printerunit->pu_Pending = ioreq;
                return 1;
            }

            io = (struct IOStdReq *)ioreq;
            if (!io->io_Data && io->io_Length != 0)
            {
                return IOERR_BADADDRESS;
            }

            length = (ioreq->io_Command == CMD_WRITE) ? printer_compute_write_length(io) : io->io_Length;
            io->io_Actual = 0;
            if (length == 0)
            {
                return 0;
            }

            io->io_Error = printer_append_buffer(printerbase,
                                                 (const UBYTE *)io->io_Data,
                                                 length);
            if (io->io_Error == 0)
            {
                io->io_Actual = length;
            }
            return io->io_Error;
        }

        case PRD_QUERY:
        {
            struct IOStdReq *io;
            UBYTE *status;

            if (!printer_request_has_iostd(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            io = (struct IOStdReq *)ioreq;
            status = (UBYTE *)io->io_Data;
            if (!status)
            {
                return IOERR_BADADDRESS;
            }
            if (io->io_Length < 2)
            {
                return IOERR_BADLENGTH;
            }

            status[0] = printerbase->pb_Status[0];
            status[1] = printerbase->pb_Status[1];
            io->io_Actual = 1;
            return 0;
        }

        case PRD_PRTCOMMAND:
        {
            struct IOPrtCmdReq *io;

            if (!printer_request_has_prtcmd(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            if (printerbase->pb_Stopped && allow_pending)
            {
                if (printerunit->pu_Pending)
                {
                    return IOERR_UNITBUSY;
                }

                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                printerunit->pu_Pending = ioreq;
                return 1;
            }

            io = (struct IOPrtCmdReq *)ioreq;
            return printer_append_prtcommand(printerbase, io);
        }

        case PRD_DUMPRPORT:
        {
            struct IODRPReq *io;

            if (!printer_request_has_dumprport(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            if (printerbase->pb_Stopped && allow_pending)
            {
                if (printerunit->pu_Pending)
                {
                    return IOERR_UNITBUSY;
                }

                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                printerunit->pu_Pending = ioreq;
                return 1;
            }

            io = (struct IODRPReq *)ioreq;
            if (io->io_Special & SPECIAL_NOPRINT)
            {
                if (io->io_DestCols <= 0)
                {
                    io->io_DestCols = io->io_SrcWidth;
                }
                if (io->io_DestRows <= 0)
                {
                    io->io_DestRows = io->io_SrcHeight;
                }
                return 0;
            }

            return PDERR_NOTGRAPHICS;
        }

        case PRD_DUMPRPORTTAGS:
        {
            struct IODRPTagsReq *io;

            if (!printer_request_has_dumprporttags(ioreq))
            {
                return IOERR_BADLENGTH;
            }

            if (printerbase->pb_Stopped && allow_pending)
            {
                if (printerunit->pu_Pending)
                {
                    return IOERR_UNITBUSY;
                }

                ioreq->io_Flags &= (UBYTE)~IOF_QUICK;
                printerunit->pu_Pending = ioreq;
                return 1;
            }

            io = (struct IODRPTagsReq *)ioreq;
            if (io->io_Special & SPECIAL_NOPRINT)
            {
                if (io->io_DestCols <= 0)
                {
                    io->io_DestCols = io->io_SrcWidth;
                }
                if (io->io_DestRows <= 0)
                {
                    io->io_DestRows = io->io_SrcHeight;
                }
                return 0;
            }

            return PDERR_NOTGRAPHICS;
        }

        default:
            return IOERR_NOCMD;
    }
}

static BPTR __g_lxa_printer_BeginIO(register struct Library *dev __asm("a6"),
                                    register struct IORequest *ioreq __asm("a1"))
{
    struct PrinterBase *printerbase = (struct PrinterBase *)dev;
    LONG result;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    if (printer_request_has_iostd(ioreq))
    {
        ((struct IOStdReq *)ioreq)->io_Actual = 0;
    }

    result = printer_execute_request(printerbase, ioreq, TRUE);
    if (result == 1)
    {
        return 0;
    }

    ioreq->io_Error = result;
    printer_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_printer_AbortIO(register struct Library *dev __asm("a6"),
                                     register struct IORequest *ioreq __asm("a1"))
{
    struct PrinterBase *printerbase = (struct PrinterBase *)dev;
    struct PrinterUnit *unit;

    (void)dev;

    for (unit = printerbase->pb_Units; unit; unit = unit->pu_Next)
    {
        if (unit->pu_Pending == ioreq)
        {
            unit->pu_Pending = NULL;
            printer_abort_request(ioreq, IOERR_ABORTED);
            return 0;
        }
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

extern APTR              __g_lxa_printer_FuncTab [];
extern struct MyDataInit __g_lxa_printer_DataTab;
extern struct InitTable  __g_lxa_printer_InitTab;
extern APTR              __g_lxa_printer_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_printer_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_printer_ExDevName[0],
    &_g_printer_ExDevID[0],
    &__g_lxa_printer_InitTab
};

APTR __g_lxa_printer_EndResident;
struct Resident *__lxa_printer_ROMTag = &ROMTag;

struct InitTable __g_lxa_printer_InitTab =
{
    (ULONG)sizeof(struct PrinterBase),
    (APTR *)&__g_lxa_printer_FuncTab[0],
    (APTR)&__g_lxa_printer_DataTab,
    (APTR)__g_lxa_printer_InitDev
};

APTR __g_lxa_printer_FuncTab [] =
{
    __g_lxa_printer_Open,
    __g_lxa_printer_Close,
    __g_lxa_printer_Expunge,
    NULL,
    __g_lxa_printer_BeginIO,
    __g_lxa_printer_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_printer_DataTab =
{
    /* ln_Type      */ INITBYTE(OFFSET(Node,         ln_Type),      NT_DEVICE),
    /* ln_Name      */ 0x80, (UBYTE)(ULONG)OFFSET(Node,    ln_Name), (ULONG)&_g_printer_ExDevName[0],
    /* lib_Flags    */ INITBYTE(OFFSET(Library,      lib_Flags),    LIBF_SUMUSED|LIBF_CHANGED),
    /* lib_Version  */ INITWORD(OFFSET(Library,      lib_Version),  VERSION),
    /* lib_Revision */ INITWORD(OFFSET(Library,      lib_Revision), REVISION),
    /* lib_IdString */ 0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_printer_ExDevID[0],
    (ULONG)0
};
