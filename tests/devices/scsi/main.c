/*
 * Test for scsi.device opens, SCSI-direct commands, and expunge behavior.
 */

#include <string.h>
#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/ports.h>
#include <devices/newstyle.h>
#include <devices/scsidisk.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static LONG test_pass = 0;
static LONG test_fail = 0;

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

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0)
    {
        neg = TRUE;
        num = -num;
    }

    if (num == 0)
    {
        buf[i++] = '0';
    }
    else
    {
        char temp[16];
        int j = 0;

        while (num > 0)
        {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }

        if (neg)
        {
            buf[i++] = '-';
        }

        while (j > 0)
        {
            buf[i++] = temp[--j];
        }
    }

    buf[i] = '\0';
    print(buf);
}

static void test_ok(const char *name)
{
    print("  OK: ");
    print(name);
    print("\n");
    test_pass++;
}

static void test_fail_msg(const char *name)
{
    print("  FAIL: ");
    print(name);
    print("\n");
    test_fail++;
}

static void fill_cdb(UBYTE *cdb, ULONG length)
{
    ULONG i;

    for (i = 0; i < length; i++)
    {
        cdb[i] = 0;
    }
}

int main(void)
{
    struct MsgPort *port;
    struct IOStdReq *req;
    struct IOStdReq *req2;
    struct IOStdReq *reopen;
    struct NSDeviceQueryResult query;
    struct Device *device;
    struct Node *node;
    struct SCSICmd cmd;
    UBYTE inquiry_cdb[6];
    UBYTE inquiry_data[64];
    UBYTE sense_data[32];
    UBYTE mode_sense_cdb[6];
    UBYTE mode_data[64];
    UBYTE read_capacity_cdb[10];
    UBYTE capacity_data[8];
    UBYTE write10_cdb[10];
    UBYTE read10_cdb[10];
    UBYTE write_data[512];
    UBYTE read_data[512];
    UBYTE bad_cdb[6];
    LONG error;
    ULONG i;

    print("Testing scsi.device\n\n");

    port = CreateMsgPort();
    req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    req2 = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    reopen = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!port || !req || !req2 || !reopen)
    {
        print("FAIL: Cannot create IO resources\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"scsi.device", 100, (struct IORequest *)req, 0);
    if (error == HFERR_NoBoard)
        test_ok("OpenDevice rejects invalid board number");
    else
        test_fail_msg("OpenDevice rejects invalid board number");

    error = OpenDevice((STRPTR)"scsi.device", 88, (struct IORequest *)req, 0);
    if (error == HFERR_NoBoard)
        test_ok("OpenDevice rejects invalid target and LUN encoding");
    else
        test_fail_msg("OpenDevice rejects invalid target and LUN encoding");

    error = OpenDevice((STRPTR)"scsi.device", 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("OpenDevice unit 0 succeeds");
    else
    {
        test_fail_msg("OpenDevice unit 0 succeeds");
        print("    error=");
        print_num(error);
        print("\n");
        return 20;
    }

    if (req->io_Device != NULL && req->io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    error = OpenDevice((STRPTR)"scsi.device", 12, (struct IORequest *)req2, 0);
    if (error == 0)
        test_ok("OpenDevice second encoded unit succeeds");
    else
        test_fail_msg("OpenDevice second encoded unit succeeds");

    req->io_Command = NSCMD_DEVICEQUERY;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &query;
    req->io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        req->io_Actual == sizeof(query) &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports scsi capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports scsi capabilities");

    fill_cdb(inquiry_cdb, sizeof(inquiry_cdb));
    inquiry_cdb[0] = 0x12;
    inquiry_cdb[4] = 36;
    cmd.scsi_Data = (UWORD *)inquiry_data;
    cmd.scsi_Length = sizeof(inquiry_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = inquiry_cdb;
    cmd.scsi_CmdLength = sizeof(inquiry_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Flags = SCSIF_AUTOSENSE | SCSIF_READ;
    cmd.scsi_Status = 0;
    cmd.scsi_SenseData = sense_data;
    cmd.scsi_SenseLength = sizeof(sense_data);
    cmd.scsi_SenseActual = 0;

    req->io_Command = HD_SCSICMD;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &cmd;
    req->io_Length = sizeof(cmd);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        cmd.scsi_Status == 0 &&
        cmd.scsi_Actual == 36 &&
        inquiry_data[2] == 2 &&
        inquiry_data[4] == 31)
        test_ok("HD_SCSICMD INQUIRY returns hosted descriptor");
    else
        test_fail_msg("HD_SCSICMD INQUIRY returns hosted descriptor");

    fill_cdb(mode_sense_cdb, sizeof(mode_sense_cdb));
    mode_sense_cdb[0] = 0x1a;
    mode_sense_cdb[2] = 0x03;
    mode_sense_cdb[4] = sizeof(mode_data);
    cmd.scsi_Data = (UWORD *)mode_data;
    cmd.scsi_Length = sizeof(mode_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = mode_sense_cdb;
    cmd.scsi_CmdLength = sizeof(mode_sense_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Status = 0;
    cmd.scsi_SenseActual = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && cmd.scsi_Status == 0 && mode_data[0] != 0)
        test_ok("HD_SCSICMD MODE SENSE returns geometry page");
    else
        test_fail_msg("HD_SCSICMD MODE SENSE returns geometry page");

    fill_cdb(read_capacity_cdb, sizeof(read_capacity_cdb));
    read_capacity_cdb[0] = 0x25;
    cmd.scsi_Data = (UWORD *)capacity_data;
    cmd.scsi_Length = sizeof(capacity_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = read_capacity_cdb;
    cmd.scsi_CmdLength = sizeof(read_capacity_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Status = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        cmd.scsi_Status == 0 &&
        capacity_data[3] == 0xff &&
        capacity_data[7] == 0x00)
        test_ok("HD_SCSICMD READ CAPACITY reports block geometry");
    else
        test_fail_msg("HD_SCSICMD READ CAPACITY reports block geometry");

    for (i = 0; i < sizeof(write_data); i++)
    {
        write_data[i] = (UBYTE)i;
        read_data[i] = 0;
    }

    fill_cdb(write10_cdb, sizeof(write10_cdb));
    write10_cdb[0] = 0x2a;
    write10_cdb[7] = 0;
    write10_cdb[8] = 1;
    cmd.scsi_Data = (UWORD *)write_data;
    cmd.scsi_Length = sizeof(write_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = write10_cdb;
    cmd.scsi_CmdLength = sizeof(write10_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Flags = SCSIF_WRITE;
    cmd.scsi_Status = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && cmd.scsi_Status == 0 && cmd.scsi_Actual == sizeof(write_data))
        test_ok("HD_SCSICMD WRITE(10) stores a logical block");
    else
        test_fail_msg("HD_SCSICMD WRITE(10) stores a logical block");

    fill_cdb(read10_cdb, sizeof(read10_cdb));
    read10_cdb[0] = 0x28;
    read10_cdb[7] = 0;
    read10_cdb[8] = 1;
    cmd.scsi_Data = (UWORD *)read_data;
    cmd.scsi_Length = sizeof(read_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = read10_cdb;
    cmd.scsi_CmdLength = sizeof(read10_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Flags = SCSIF_READ;
    cmd.scsi_Status = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && cmd.scsi_Status == 0 && memcmp(write_data, read_data, sizeof(write_data)) == 0)
        test_ok("HD_SCSICMD READ(10) returns previously written block");
    else
        test_fail_msg("HD_SCSICMD READ(10) returns previously written block");

    fill_cdb(bad_cdb, sizeof(bad_cdb));
    bad_cdb[0] = 0xff;
    cmd.scsi_Data = (UWORD *)read_data;
    cmd.scsi_Length = sizeof(read_data);
    cmd.scsi_Actual = 0;
    cmd.scsi_Command = bad_cdb;
    cmd.scsi_CmdLength = sizeof(bad_cdb);
    cmd.scsi_CmdActual = 0;
    cmd.scsi_Flags = SCSIF_AUTOSENSE | SCSIF_READ;
    cmd.scsi_Status = 0;
    cmd.scsi_SenseActual = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == HFERR_BadStatus &&
        cmd.scsi_Status == 2 &&
        cmd.scsi_SenseActual != 0 &&
        sense_data[2] == 5)
        test_ok("HD_SCSICMD unsupported opcode returns CHECK CONDITION with autosense");
    else
        test_fail_msg("HD_SCSICMD unsupported opcode returns CHECK CONDITION with autosense");

    device = req->io_Device;
    node = FindName(&SysBase->DeviceList, (CONST_STRPTR)"scsi.device");
    if (node == &device->dd_Library.lib_Node)
        test_ok("scsi.device is present in DeviceList before expunge");
    else
        test_fail_msg("scsi.device is present in DeviceList before expunge");

    RemDevice(device);
    if ((device->dd_Library.lib_Flags & LIBF_DELEXP) != 0 &&
        FindName(&SysBase->DeviceList, (CONST_STRPTR)"scsi.device") == NULL)
        test_ok("Expunge unlinks device and defers while opens remain");
    else
        test_fail_msg("Expunge unlinks device and defers while opens remain");

    error = OpenDevice((STRPTR)"scsi.device", 0, (struct IORequest *)reopen, 0);
    if (error == IOERR_OPENFAIL)
        test_ok("Deferred expunge blocks new opens");
    else
    {
        test_fail_msg("Deferred expunge blocks new opens");
        if (error == 0)
            CloseDevice((struct IORequest *)reopen);
    }

    CloseDevice((struct IORequest *)req2);
    if (device->dd_Library.lib_OpenCnt == 1 && (device->dd_Library.lib_Flags & LIBF_DELEXP) != 0)
        test_ok("CloseDevice keeps deferred expunge pending");
    else
        test_fail_msg("CloseDevice keeps deferred expunge pending");

    CloseDevice((struct IORequest *)req);
    if (FindName(&SysBase->DeviceList, (CONST_STRPTR)"scsi.device") == NULL &&
        device->dd_Library.lib_OpenCnt == 0 &&
        (device->dd_Library.lib_Flags & LIBF_DELEXP) == 0)
        test_ok("Final close completes deferred expunge");
    else
        test_fail_msg("Final close completes deferred expunge");

    DeleteIORequest((struct IORequest *)reopen);
    DeleteIORequest((struct IORequest *)req2);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);

    print("\n");
    if (test_fail == 0)
    {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    }

    print("FAIL: ");
    print_num(test_fail);
    print(" of ");
    print_num(test_pass + test_fail);
    print(" tests failed\n");
    return 20;
}
