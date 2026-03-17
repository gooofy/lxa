/*
 * lxa scsi.device implementation
 *
 * Hosted compatibility implementation for SCSI-direct commands.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <devices/newstyle.h>
#include <devices/scsidisk.h>

#include "util.h"

#define VERSION  40
#define REVISION 1
#define EXDEVNAME "scsi"
#define EXDEVVER  " 40.1 (2026/03/17)"

#define SCSI_BLOCK_SIZE   512UL
#define SCSI_BLOCK_COUNT  2048UL
#define SCSI_CYLINDERS    64UL
#define SCSI_HEADS        4UL
#define SCSI_SECTORS      8UL
#define SCSI_SENSE_SIZE   32U

char __aligned _g_scsi_ExDevName [] = EXDEVNAME ".device";
char __aligned _g_scsi_ExDevID   [] = EXDEVNAME EXDEVVER;
char __aligned _g_scsi_Copyright [] = "(C)opyright 2026 by G. Bartsch. Licensed under the MIT License.";

char __aligned _g_scsi_VERSTRING [] = "\0$VER: " EXDEVNAME EXDEVVER;

extern struct ExecBase *SysBase;

struct ScsiUnit
{
    struct Unit su_Unit;
    struct ScsiUnit *su_Next;
    ULONG su_UnitNumber;
    UBYTE *su_Storage;
    ULONG su_BlockCount;
    UWORD su_BlockSize;
    UBYTE su_Sense[SCSI_SENSE_SIZE];
    UWORD su_SenseLength;
};

struct ScsiBase
{
    struct Device sb_Device;
    BPTR sb_SegList;
    struct ScsiUnit *sb_Units;
};

static const UWORD scsi_supported_commands[] =
{
    CMD_CLEAR,
    CMD_FLUSH,
    CMD_RESET,
    CMD_START,
    CMD_STOP,
    HD_SCSICMD,
    NSCMD_DEVICEQUERY,
    0
};

static void scsi_reply_request(struct IORequest *ioreq)
{
    if (!(ioreq->io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->io_Message);
    }
}

static ULONG scsi_read_be32(const UBYTE *data)
{
    return ((ULONG)data[0] << 24) |
           ((ULONG)data[1] << 16) |
           ((ULONG)data[2] << 8) |
           (ULONG)data[3];
}

static UWORD scsi_read_be16(const UBYTE *data)
{
    return (UWORD)(((UWORD)data[0] << 8) | (UWORD)data[1]);
}

static void scsi_write_be32(UBYTE *data, ULONG value)
{
    data[0] = (UBYTE)(value >> 24);
    data[1] = (UBYTE)(value >> 16);
    data[2] = (UBYTE)(value >> 8);
    data[3] = (UBYTE)value;
}

static void scsi_write_be24(UBYTE *data, ULONG value)
{
    data[0] = (UBYTE)(value >> 16);
    data[1] = (UBYTE)(value >> 8);
    data[2] = (UBYTE)value;
}

static struct ScsiUnit *scsi_get_unit(struct IORequest *ioreq)
{
    if (!ioreq)
    {
        return NULL;
    }

    return (struct ScsiUnit *)ioreq->io_Unit;
}

static BOOL scsi_unit_number_valid(ULONG unit)
{
    ULONG board = unit / 100;
    ULONG lun = (unit / 10) % 10;
    ULONG target = unit % 10;

    if (board != 0)
    {
        return FALSE;
    }

    return lun <= 7 && target <= 7;
}

static struct ScsiUnit *scsi_find_unit(struct ScsiBase *scsibase, ULONG unit)
{
    struct ScsiUnit *scan;

    for (scan = scsibase->sb_Units; scan; scan = scan->su_Next)
    {
        if (scan->su_UnitNumber == unit)
        {
            return scan;
        }
    }

    return NULL;
}

static void scsi_clear_sense(struct ScsiUnit *unit)
{
    if (!unit)
    {
        return;
    }

    memset(unit->su_Sense, 0, sizeof(unit->su_Sense));
    unit->su_SenseLength = 0;
}

static void scsi_set_sense(struct ScsiUnit *unit,
                           UBYTE sense_key,
                           UBYTE asc,
                           UBYTE ascq)
{
    if (!unit)
    {
        return;
    }

    memset(unit->su_Sense, 0, sizeof(unit->su_Sense));
    unit->su_Sense[0] = 0x70;
    unit->su_Sense[2] = sense_key;
    unit->su_Sense[7] = 10;
    unit->su_Sense[12] = asc;
    unit->su_Sense[13] = ascq;
    unit->su_SenseLength = 18;
}

static void scsi_copy_autosense(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UWORD max_length;
    UWORD copy_length;

    if (!unit || !cmd)
    {
        return;
    }

    cmd->scsi_SenseActual = 0;

    if (!(cmd->scsi_Flags & (SCSIF_AUTOSENSE | SCSIF_OLDAUTOSENSE)))
    {
        return;
    }

    if (!cmd->scsi_SenseData)
    {
        return;
    }

    max_length = (cmd->scsi_Flags & SCSIF_OLDAUTOSENSE) ? 4 : cmd->scsi_SenseLength;
    if (max_length == 0)
    {
        return;
    }

    copy_length = unit->su_SenseLength;
    if (copy_length == 0)
    {
        return;
    }

    if (copy_length > max_length)
    {
        copy_length = max_length;
    }

    CopyMem(unit->su_Sense, cmd->scsi_SenseData, copy_length);
    cmd->scsi_SenseActual = copy_length;
}

static LONG scsi_fail_command(struct ScsiUnit *unit,
                              struct SCSICmd *cmd,
                              UBYTE sense_key,
                              UBYTE asc,
                              UBYTE ascq)
{
    scsi_set_sense(unit, sense_key, asc, ascq);

    cmd->scsi_Status = 2;
    cmd->scsi_CmdActual = 0;
    cmd->scsi_Actual = 0;
    scsi_copy_autosense(unit, cmd);
    return HFERR_BadStatus;
}

static LONG scsi_validate_scsi_request(struct IOStdReq *io, struct SCSICmd **cmd_out)
{
    struct SCSICmd *cmd;

    if (!io || !cmd_out)
    {
        return IOERR_BADADDRESS;
    }

    if (!io->io_Data)
    {
        return IOERR_BADADDRESS;
    }

    if (io->io_Length < sizeof(struct SCSICmd))
    {
        return IOERR_BADLENGTH;
    }

    cmd = (struct SCSICmd *)io->io_Data;
    if (!cmd->scsi_Command)
    {
        return IOERR_BADADDRESS;
    }

    if (cmd->scsi_CmdLength == 0)
    {
        return IOERR_BADLENGTH;
    }

    *cmd_out = cmd;
    return 0;
}

static struct ScsiUnit *scsi_create_unit(struct ScsiBase *scsibase, ULONG unit_number)
{
    struct ScsiUnit *unit;

    unit = (struct ScsiUnit *)AllocMem(sizeof(*unit), MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit)
    {
        return NULL;
    }

    unit->su_Storage = (UBYTE *)AllocMem(SCSI_BLOCK_COUNT * SCSI_BLOCK_SIZE,
                                         MEMF_PUBLIC | MEMF_CLEAR);
    if (!unit->su_Storage)
    {
        FreeMem(unit, sizeof(*unit));
        return NULL;
    }

    unit->su_UnitNumber = unit_number;
    unit->su_BlockCount = SCSI_BLOCK_COUNT;
    unit->su_BlockSize = SCSI_BLOCK_SIZE;
    scsi_clear_sense(unit);

    unit->su_Next = scsibase->sb_Units;
    scsibase->sb_Units = unit;
    return unit;
}

static struct ScsiUnit *scsi_get_or_create_unit(struct ScsiBase *scsibase, ULONG unit_number)
{
    struct ScsiUnit *unit;

    unit = scsi_find_unit(scsibase, unit_number);
    if (unit)
    {
        return unit;
    }

    return scsi_create_unit(scsibase, unit_number);
}

static void scsi_free_units(struct ScsiBase *scsibase)
{
    struct ScsiUnit *unit = scsibase->sb_Units;

    while (unit)
    {
        struct ScsiUnit *next = unit->su_Next;

        if (unit->su_Storage)
        {
            FreeMem(unit->su_Storage, unit->su_BlockCount * unit->su_BlockSize);
        }

        FreeMem(unit, sizeof(*unit));
        unit = next;
    }

    scsibase->sb_Units = NULL;
}

static BPTR scsi_expunge_if_possible(struct ScsiBase *scsibase)
{
    if (FindName(&SysBase->DeviceList,
                 (CONST_STRPTR)scsibase->sb_Device.dd_Library.lib_Node.ln_Name) ==
        &scsibase->sb_Device.dd_Library.lib_Node)
    {
        Remove(&scsibase->sb_Device.dd_Library.lib_Node);
    }

    if (scsibase->sb_Device.dd_Library.lib_OpenCnt != 0)
    {
        scsibase->sb_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    scsibase->sb_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;
    scsi_free_units(scsibase);
    return scsibase->sb_SegList;
}

static LONG scsi_copy_out(struct SCSICmd *cmd,
                          const UBYTE *data,
                          ULONG available,
                          ULONG requested)
{
    ULONG actual = available;

    if (requested < actual)
    {
        actual = requested;
    }

    if (cmd->scsi_Length < actual)
    {
        actual = cmd->scsi_Length;
    }

    if (actual != 0 && !cmd->scsi_Data)
    {
        return IOERR_BADADDRESS;
    }

    if (actual != 0)
    {
        CopyMem((APTR)data, cmd->scsi_Data, actual);
    }

    cmd->scsi_Actual = actual;
    cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    cmd->scsi_Status = 0;
    return 0;
}

static LONG scsi_handle_inquiry(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UBYTE response[36];
    UBYTE *cdb = cmd->scsi_Command;

    (void)unit;

    if (cmd->scsi_CmdLength < 6)
    {
        return IOERR_BADLENGTH;
    }

    if ((cdb[1] & 1) != 0 || cdb[2] != 0)
    {
        return scsi_fail_command(unit, cmd, 0x05, 0x24, 0x00);
    }

    memset(response, 0, sizeof(response));
    response[0] = 0x00;
    response[2] = 0x02;
    response[3] = 0x02;
    response[4] = 31;
    CopyMem((APTR)"LXA     ", response + 8, 8);
    CopyMem((APTR)"Virtual SCSI    ", response + 16, 16);
    CopyMem((APTR)"1.0 ", response + 32, 4);

    scsi_clear_sense(unit);
    return scsi_copy_out(cmd, response, sizeof(response), cdb[4]);
}

static LONG scsi_handle_request_sense(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UBYTE response[SCSI_SENSE_SIZE];
    UBYTE *cdb = cmd->scsi_Command;
    UWORD sense_length;

    if (cmd->scsi_CmdLength < 6)
    {
        return IOERR_BADLENGTH;
    }

    memset(response, 0, sizeof(response));
    if (unit->su_SenseLength != 0)
    {
        CopyMem(unit->su_Sense, response, unit->su_SenseLength);
        sense_length = unit->su_SenseLength;
    }
    else
    {
        response[0] = 0x70;
        response[7] = 10;
        sense_length = 18;
    }

    return scsi_copy_out(cmd, response, sense_length, cdb[4]);
}

static LONG scsi_handle_mode_sense(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UBYTE response[64];
    UBYTE *cdb = cmd->scsi_Command;
    UBYTE page_code;
    BOOL disable_block_desc;
    ULONG total = 4;

    if (cmd->scsi_CmdLength < 6)
    {
        return IOERR_BADLENGTH;
    }

    memset(response, 0, sizeof(response));
    page_code = cdb[2] & 0x3f;
    disable_block_desc = (cdb[1] & 0x08) != 0;

    if (!disable_block_desc)
    {
        response[3] = 8;
        scsi_write_be24(response + 5, unit->su_BlockCount);
        scsi_write_be24(response + 9, unit->su_BlockSize);
        total += 8;
    }

    switch (page_code)
    {
        case 0x00:
            response[total + 0] = 0x00;
            response[total + 1] = 2;
            response[total + 2] = 0x03;
            response[total + 3] = 0x04;
            total += 4;
            break;

        case 0x03:
            response[total + 0] = 0x03;
            response[total + 1] = 0x16;
            response[total + 3] = 0x01;
            response[total + 10] = (UBYTE)(SCSI_SECTORS >> 8);
            response[total + 11] = (UBYTE)SCSI_SECTORS;
            response[total + 12] = (UBYTE)(SCSI_BLOCK_SIZE >> 8);
            response[total + 13] = (UBYTE)SCSI_BLOCK_SIZE;
            response[total + 15] = 0x01;
            response[total + 20] = 0x80;
            total += 24;
            break;

        case 0x04:
            response[total + 0] = 0x04;
            response[total + 1] = 0x16;
            scsi_write_be32(response + total + 2, SCSI_CYLINDERS);
            response[total + 5] = (UBYTE)SCSI_HEADS;
            scsi_write_be32(response + total + 13, SCSI_CYLINDERS);
            response[total + 20] = 0x15;
            response[total + 21] = 0x18;
            total += 24;
            break;

        case 0x3f:
            response[total + 0] = 0x03;
            response[total + 1] = 0x16;
            response[total + 3] = 0x01;
            response[total + 10] = (UBYTE)(SCSI_SECTORS >> 8);
            response[total + 11] = (UBYTE)SCSI_SECTORS;
            response[total + 12] = (UBYTE)(SCSI_BLOCK_SIZE >> 8);
            response[total + 13] = (UBYTE)SCSI_BLOCK_SIZE;
            response[total + 15] = 0x01;
            response[total + 20] = 0x80;
            total += 24;

            response[total + 0] = 0x04;
            response[total + 1] = 0x16;
            scsi_write_be32(response + total + 2, SCSI_CYLINDERS);
            response[total + 5] = (UBYTE)SCSI_HEADS;
            scsi_write_be32(response + total + 13, SCSI_CYLINDERS);
            response[total + 20] = 0x15;
            response[total + 21] = 0x18;
            total += 24;
            break;

        default:
            return scsi_fail_command(unit, cmd, 0x05, 0x24, 0x00);
    }

    response[0] = (UBYTE)(total - 1);
    scsi_clear_sense(unit);
    return scsi_copy_out(cmd, response, total, cdb[4]);
}

static LONG scsi_handle_read_capacity(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UBYTE response[8];
    UBYTE *cdb = cmd->scsi_Command;

    if (cmd->scsi_CmdLength < 10)
    {
        return IOERR_BADLENGTH;
    }

    if ((cdb[8] & 1) == 0 && scsi_read_be32(cdb + 2) != 0)
    {
        return scsi_fail_command(unit, cmd, 0x05, 0x24, 0x00);
    }

    scsi_write_be32(response + 0, unit->su_BlockCount - 1);
    scsi_write_be32(response + 4, unit->su_BlockSize);
    scsi_clear_sense(unit);
    return scsi_copy_out(cmd, response, sizeof(response), sizeof(response));
}

static LONG scsi_transfer_blocks(struct ScsiUnit *unit,
                                 struct SCSICmd *cmd,
                                 ULONG lba,
                                 ULONG blocks,
                                 BOOL write)
{
    ULONG byte_count;
    UBYTE *storage;

    if (blocks == 0)
    {
        cmd->scsi_Actual = 0;
        cmd->scsi_CmdActual = cmd->scsi_CmdLength;
        cmd->scsi_Status = 0;
        scsi_clear_sense(unit);
        return 0;
    }

    if (lba >= unit->su_BlockCount || blocks > (unit->su_BlockCount - lba))
    {
        return scsi_fail_command(unit, cmd, 0x05, 0x21, 0x00);
    }

    byte_count = blocks * unit->su_BlockSize;
    if (!cmd->scsi_Data)
    {
        return IOERR_BADADDRESS;
    }

    if (cmd->scsi_Length < byte_count)
    {
        return scsi_fail_command(unit, cmd, 0x05, 0x24, 0x00);
    }

    storage = unit->su_Storage + (lba * unit->su_BlockSize);
    if (write)
    {
        CopyMem(cmd->scsi_Data, storage, byte_count);
    }
    else
    {
        CopyMem(storage, cmd->scsi_Data, byte_count);
    }

    cmd->scsi_Actual = byte_count;
    cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    cmd->scsi_Status = 0;
    scsi_clear_sense(unit);
    return 0;
}

static LONG scsi_handle_read_write_6(struct ScsiUnit *unit,
                                     struct SCSICmd *cmd,
                                     BOOL write)
{
    UBYTE *cdb = cmd->scsi_Command;
    ULONG lba;
    ULONG blocks;

    if (cmd->scsi_CmdLength < 6)
    {
        return IOERR_BADLENGTH;
    }

    lba = ((ULONG)(cdb[1] & 0x1f) << 16) |
          ((ULONG)cdb[2] << 8) |
          (ULONG)cdb[3];
    blocks = cdb[4] ? (ULONG)cdb[4] : 256UL;
    return scsi_transfer_blocks(unit, cmd, lba, blocks, write);
}

static LONG scsi_handle_read_write_10(struct ScsiUnit *unit,
                                      struct SCSICmd *cmd,
                                      BOOL write)
{
    UBYTE *cdb = cmd->scsi_Command;

    if (cmd->scsi_CmdLength < 10)
    {
        return IOERR_BADLENGTH;
    }

    return scsi_transfer_blocks(unit,
                                cmd,
                                scsi_read_be32(cdb + 2),
                                scsi_read_be16(cdb + 7),
                                write);
}

static LONG scsi_handle_read_write_12(struct ScsiUnit *unit,
                                      struct SCSICmd *cmd,
                                      BOOL write)
{
    UBYTE *cdb = cmd->scsi_Command;

    if (cmd->scsi_CmdLength < 12)
    {
        return IOERR_BADLENGTH;
    }

    return scsi_transfer_blocks(unit,
                                cmd,
                                scsi_read_be32(cdb + 2),
                                scsi_read_be32(cdb + 6),
                                write);
}

static LONG scsi_handle_scsi_command(struct ScsiUnit *unit, struct SCSICmd *cmd)
{
    UBYTE opcode = cmd->scsi_Command[0];

    cmd->scsi_Actual = 0;
    cmd->scsi_CmdActual = 0;
    cmd->scsi_Status = 0;
    cmd->scsi_SenseActual = 0;

    switch (opcode)
    {
        case 0x00:
            cmd->scsi_CmdActual = cmd->scsi_CmdLength;
            scsi_clear_sense(unit);
            return 0;

        case 0x03:
            return scsi_handle_request_sense(unit, cmd);

        case 0x08:
            return scsi_handle_read_write_6(unit, cmd, FALSE);

        case 0x0a:
            return scsi_handle_read_write_6(unit, cmd, TRUE);

        case 0x12:
            return scsi_handle_inquiry(unit, cmd);

        case 0x1a:
            return scsi_handle_mode_sense(unit, cmd);

        case 0x1d:
        case 0x35:
            cmd->scsi_CmdActual = cmd->scsi_CmdLength;
            scsi_clear_sense(unit);
            return 0;

        case 0x25:
            return scsi_handle_read_capacity(unit, cmd);

        case 0x28:
            return scsi_handle_read_write_10(unit, cmd, FALSE);

        case 0x2a:
            return scsi_handle_read_write_10(unit, cmd, TRUE);

        case 0xa8:
            return scsi_handle_read_write_12(unit, cmd, FALSE);

        case 0xaa:
            return scsi_handle_read_write_12(unit, cmd, TRUE);

        default:
            return scsi_fail_command(unit, cmd, 0x05, 0x24, 0x00);
    }
}

static struct Library *__g_lxa_scsi_InitDev(register struct Library *dev __asm("d0"),
                                            register BPTR seglist __asm("a0"),
                                            register struct ExecBase *sysb __asm("a6"))
{
    struct ScsiBase *scsibase = (struct ScsiBase *)dev;

    (void)sysb;

    scsibase->sb_SegList = seglist;
    scsibase->sb_Units = NULL;
    return dev;
}

static void __g_lxa_scsi_Open(register struct Library *dev __asm("a6"),
                              register struct IORequest *ioreq __asm("a1"),
                              register ULONG unit __asm("d0"),
                              register ULONG flags __asm("d1"))
{
    struct ScsiBase *scsibase = (struct ScsiBase *)dev;
    struct ScsiUnit *scsiunit;

    (void)flags;

    ioreq->io_Error = 0;
    ioreq->io_Unit = NULL;
    ioreq->io_Device = NULL;

    if (dev->lib_Flags & LIBF_DELEXP)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    if (!scsi_unit_number_valid(unit))
    {
        ioreq->io_Error = HFERR_NoBoard;
        return;
    }

    scsiunit = scsi_get_or_create_unit(scsibase, unit);
    if (!scsiunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }

    dev->lib_OpenCnt++;
    scsiunit->su_Unit.unit_OpenCnt++;
    ioreq->io_Device = (struct Device *)scsibase;
    ioreq->io_Unit = (struct Unit *)scsiunit;
}

static BPTR __g_lxa_scsi_Close(register struct Library *dev __asm("a6"),
                               register struct IORequest *ioreq __asm("a1"))
{
    struct ScsiBase *scsibase = (struct ScsiBase *)dev;
    struct ScsiUnit *scsiunit = scsi_get_unit(ioreq);

    if (scsiunit && scsiunit->su_Unit.unit_OpenCnt != 0)
    {
        scsiunit->su_Unit.unit_OpenCnt--;
    }

    if (dev->lib_OpenCnt != 0)
    {
        dev->lib_OpenCnt--;
    }

    ioreq->io_Unit = NULL;
    ioreq->io_Device = NULL;

    if (scsibase->sb_Device.dd_Library.lib_OpenCnt == 0 &&
        (scsibase->sb_Device.dd_Library.lib_Flags & LIBF_DELEXP))
    {
        return scsi_expunge_if_possible(scsibase);
    }

    return 0;
}

static BPTR __g_lxa_scsi_Expunge(register struct Library *dev __asm("a6"))
{
    return scsi_expunge_if_possible((struct ScsiBase *)dev);
}

static BPTR __g_lxa_scsi_BeginIO(register struct Library *dev __asm("a6"),
                                 register struct IORequest *ioreq __asm("a1"))
{
    struct ScsiUnit *scsiunit = scsi_get_unit(ioreq);
    struct IOStdReq *io = (struct IOStdReq *)ioreq;
    LONG result = 0;

    (void)dev;

    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->io_Error = 0;
    io->io_Actual = 0;

    if (!scsiunit)
    {
        ioreq->io_Error = IOERR_OPENFAIL;
        scsi_reply_request(ioreq);
        return 0;
    }

    switch (ioreq->io_Command)
    {
        case NSCMD_DEVICEQUERY:
        {
            struct NSDeviceQueryResult *query = (struct NSDeviceQueryResult *)io->io_Data;

            if (!query)
            {
                result = IOERR_BADADDRESS;
                break;
            }
            if (io->io_Length < sizeof(*query))
            {
                result = IOERR_BADLENGTH;
                break;
            }

            query->nsdqr_DevQueryFormat = 0;
            query->nsdqr_SizeAvailable = sizeof(*query);
            query->nsdqr_DeviceType = NSDEVTYPE_UNKNOWN;
            query->nsdqr_DeviceSubType = 0;
            query->nsdqr_SupportedCommands = (APTR)scsi_supported_commands;
            io->io_Actual = sizeof(*query);
            break;
        }

        case CMD_CLEAR:
        case CMD_FLUSH:
        case CMD_RESET:
            scsi_clear_sense(scsiunit);
            break;

        case CMD_START:
        case CMD_STOP:
            break;

        case HD_SCSICMD:
        {
            struct SCSICmd *cmd;

            result = scsi_validate_scsi_request(io, &cmd);
            if (result == 0)
            {
                result = scsi_handle_scsi_command(scsiunit, cmd);
            }
            break;
        }

        default:
            result = IOERR_NOCMD;
            break;
    }

    ioreq->io_Error = result;
    scsi_reply_request(ioreq);
    return 0;
}

static ULONG __g_lxa_scsi_AbortIO(register struct Library *dev __asm("a6"),
                                  register struct IORequest *ioreq __asm("a1"))
{
    (void)dev;
    (void)ioreq;
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

extern APTR __g_lxa_scsi_FuncTab [];
extern struct MyDataInit __g_lxa_scsi_DataTab;
extern struct InitTable __g_lxa_scsi_InitTab;
extern APTR __g_lxa_scsi_EndResident;

static struct Resident __aligned ROMTag =
{
    RTC_MATCHWORD,
    &ROMTag,
    &__g_lxa_scsi_EndResident,
    RTF_AUTOINIT,
    VERSION,
    NT_DEVICE,
    0,
    &_g_scsi_ExDevName[0],
    &_g_scsi_ExDevID[0],
    &__g_lxa_scsi_InitTab
};

APTR __g_lxa_scsi_EndResident;
struct Resident *__lxa_scsi_ROMTag = &ROMTag;

struct InitTable __g_lxa_scsi_InitTab =
{
    (ULONG)sizeof(struct ScsiBase),
    (APTR *)&__g_lxa_scsi_FuncTab[0],
    (APTR)&__g_lxa_scsi_DataTab,
    (APTR)__g_lxa_scsi_InitDev
};

APTR __g_lxa_scsi_FuncTab [] =
{
    __g_lxa_scsi_Open,
    __g_lxa_scsi_Close,
    __g_lxa_scsi_Expunge,
    0,
    __g_lxa_scsi_BeginIO,
    __g_lxa_scsi_AbortIO,
    (APTR)((LONG)-1)
};

struct MyDataInit __g_lxa_scsi_DataTab =
{
    INITBYTE(OFFSET(Node, ln_Type), NT_DEVICE),
    0x80, (UBYTE)(ULONG)OFFSET(Node, ln_Name), (ULONG)&_g_scsi_ExDevName[0],
    INITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED | LIBF_CHANGED),
    INITWORD(OFFSET(Library, lib_Version), VERSION),
    INITWORD(OFFSET(Library, lib_Revision), REVISION),
    0x80, (UBYTE)(ULONG)OFFSET(Library, lib_IdString), (ULONG)&_g_scsi_ExDevID[0],
    (ULONG)0
};
