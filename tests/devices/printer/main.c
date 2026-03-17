/*
 * Test for printer.device opens, buffered writes, status queries, and queued I/O.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/printer.h>
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

int main(void)
{
    struct MsgPort *port;
    struct IOStdReq *req;
    struct IOStdReq *raw_req;
    struct IOStdReq *small_req;
    struct IOPrtCmdReq *cmd_req;
    struct IODRPReq *dump_req;
    struct IODRPTagsReq *tags_req;
    struct NSDeviceQueryResult query;
    UBYTE status[2] = { 0xff, 0xff };
    UBYTE write_buf[7] = { 'P', 'R', 'I', 'N', 'T', '\n', 0 };
    LONG error;

    SysBase = *(struct ExecBase **)4;
    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 0);
    if (!DOSBase)
    {
        return 20;
    }

    print("Testing printer.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    raw_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    small_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    cmd_req = (struct IOPrtCmdReq *)CreateIORequest(port, sizeof(struct IOPrtCmdReq));
    dump_req = (struct IODRPReq *)CreateIORequest(port, sizeof(struct IODRPReq));
    tags_req = (struct IODRPTagsReq *)CreateIORequest(port, sizeof(struct IODRPTagsReq));
    if (!req || !raw_req || !small_req || !cmd_req || !dump_req || !tags_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"printer.device", 1, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)req);
    }

    small_req->io_Message.mn_Length = sizeof(struct Message);
    error = OpenDevice((STRPTR)"printer.device", 0, (struct IORequest *)small_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)small_req);
    }

    error = OpenDevice((STRPTR)"printer.device", 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("OpenDevice succeeds");
    else
    {
        test_fail_msg("OpenDevice succeeds");
        print("    error=");
        print_num(error);
        print("\n");
        return 20;
    }

    if (req->io_Device != NULL && req->io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    req->io_Command = NSCMD_DEVICEQUERY;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &query;
    req->io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        req->io_Actual == sizeof(query) &&
        query.nsdqr_DeviceType == NSDEVTYPE_PRINTER &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports printer capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports printer capabilities");

    req->io_Command = PRD_QUERY;
    req->io_Flags = IOF_QUICK;
    req->io_Data = status;
    req->io_Length = sizeof(status);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && req->io_Actual == 1 && status[0] == 0x04 && status[1] == 0)
        test_ok("PRD_QUERY reports default parallel-style status");
    else
        test_fail_msg("PRD_QUERY reports default parallel-style status");

    req->io_Command = CMD_WRITE;
    req->io_Flags = IOF_QUICK;
    req->io_Data = write_buf;
    req->io_Length = (ULONG)-1;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && req->io_Actual == 6)
        test_ok("CMD_WRITE accepts null-terminated payload");
    else
        test_fail_msg("CMD_WRITE accepts null-terminated payload");

    raw_req->io_Device = req->io_Device;
    raw_req->io_Unit = req->io_Unit;
    raw_req->io_Command = PRD_RAWWRITE;
    raw_req->io_Flags = IOF_QUICK;
    raw_req->io_Data = write_buf;
    raw_req->io_Length = 3;
    DoIO((struct IORequest *)raw_req);
    if (raw_req->io_Error == 0 && raw_req->io_Actual == 3)
        test_ok("PRD_RAWWRITE accepts raw bytes");
    else
        test_fail_msg("PRD_RAWWRITE accepts raw bytes");

    cmd_req->io_Device = req->io_Device;
    cmd_req->io_Unit = req->io_Unit;
    cmd_req->io_Command = PRD_PRTCOMMAND;
    cmd_req->io_Flags = IOF_QUICK;
    cmd_req->io_PrtCommand = aRIS;
    DoIO((struct IORequest *)cmd_req);
    if (cmd_req->io_Error == 0)
        test_ok("PRD_PRTCOMMAND supports mapped reset command");
    else
        test_fail_msg("PRD_PRTCOMMAND supports mapped reset command");

    cmd_req->io_Command = PRD_PRTCOMMAND;
    cmd_req->io_Flags = IOF_QUICK;
    cmd_req->io_PrtCommand = 200;
    DoIO((struct IORequest *)cmd_req);
    if (cmd_req->io_Error == -1)
        test_ok("PRD_PRTCOMMAND rejects unsupported command");
    else
        test_fail_msg("PRD_PRTCOMMAND rejects unsupported command");

    dump_req->io_Device = req->io_Device;
    dump_req->io_Unit = req->io_Unit;
    dump_req->io_Command = PRD_DUMPRPORT;
    dump_req->io_Flags = IOF_QUICK;
    dump_req->io_SrcWidth = 320;
    dump_req->io_SrcHeight = 200;
    dump_req->io_DestCols = 0;
    dump_req->io_DestRows = 0;
    dump_req->io_Special = SPECIAL_NOPRINT;
    DoIO((struct IORequest *)dump_req);
    if (dump_req->io_Error == 0 && dump_req->io_DestCols == 320 && dump_req->io_DestRows == 200)
        test_ok("PRD_DUMPRPORT computes dimensions for SPECIAL_NOPRINT");
    else
        test_fail_msg("PRD_DUMPRPORT computes dimensions for SPECIAL_NOPRINT");

    tags_req->io_Device = req->io_Device;
    tags_req->io_Unit = req->io_Unit;
    tags_req->io_Command = PRD_DUMPRPORTTAGS;
    tags_req->io_Flags = IOF_QUICK;
    tags_req->io_SrcWidth = 640;
    tags_req->io_SrcHeight = 256;
    tags_req->io_DestCols = 0;
    tags_req->io_DestRows = 0;
    tags_req->io_Special = SPECIAL_NOPRINT;
    DoIO((struct IORequest *)tags_req);
    if (tags_req->io_Error == 0 && tags_req->io_DestCols == 640 && tags_req->io_DestRows == 256)
        test_ok("PRD_DUMPRPORTTAGS computes dimensions for SPECIAL_NOPRINT");
    else
        test_fail_msg("PRD_DUMPRPORTTAGS computes dimensions for SPECIAL_NOPRINT");

    dump_req->io_Command = PRD_DUMPRPORT;
    dump_req->io_Flags = IOF_QUICK;
    dump_req->io_Special = 0;
    DoIO((struct IORequest *)dump_req);
    if (dump_req->io_Error == PDERR_NOTGRAPHICS)
        test_ok("PRD_DUMPRPORT reports no graphics driver");
    else
        test_fail_msg("PRD_DUMPRPORT reports no graphics driver");

    req->io_Command = CMD_STOP;
    req->io_Flags = IOF_QUICK;
    req->io_Data = NULL;
    req->io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_STOP succeeds");
    else
        test_fail_msg("CMD_STOP succeeds");

    raw_req->io_Command = PRD_RAWWRITE;
    raw_req->io_Flags = 0;
    raw_req->io_Data = write_buf;
    raw_req->io_Length = 2;
    SendIO((struct IORequest *)raw_req);
    if (CheckIO((struct IORequest *)raw_req) == NULL)
        test_ok("PRD_RAWWRITE queues while stopped");
    else
        test_fail_msg("PRD_RAWWRITE queues while stopped");

    cmd_req->io_Command = PRD_PRTCOMMAND;
    cmd_req->io_Flags = IOF_QUICK;
    cmd_req->io_PrtCommand = aRIN;
    DoIO((struct IORequest *)cmd_req);
    if (cmd_req->io_Error == IOERR_UNITBUSY)
        test_ok("Second stopped request reports unit busy");
    else
        test_fail_msg("Second stopped request reports unit busy");

    req->io_Command = CMD_START;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_START succeeds");
    else
        test_fail_msg("CMD_START succeeds");

    WaitIO((struct IORequest *)raw_req);
    if (raw_req->io_Error == 0 && raw_req->io_Actual == 2)
        test_ok("CMD_START completes queued write");
    else
        test_fail_msg("CMD_START completes queued write");

    req->io_Command = CMD_STOP;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    raw_req->io_Command = PRD_RAWWRITE;
    raw_req->io_Flags = 0;
    raw_req->io_Data = write_buf;
    raw_req->io_Length = 4;
    SendIO((struct IORequest *)raw_req);
    if (CheckIO((struct IORequest *)raw_req) == NULL)
        test_ok("Stopped write can pend for abort test");
    else
        test_fail_msg("Stopped write can pend for abort test");

    AbortIO((struct IORequest *)raw_req);
    WaitIO((struct IORequest *)raw_req);
    if (raw_req->io_Error == IOERR_ABORTED)
        test_ok("AbortIO aborts pending stopped write");
    else
        test_fail_msg("AbortIO aborts pending stopped write");

    req->io_Command = CMD_START;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);

    req->io_Command = CMD_STOP;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    raw_req->io_Command = PRD_RAWWRITE;
    raw_req->io_Flags = 0;
    raw_req->io_Data = write_buf;
    raw_req->io_Length = 1;
    SendIO((struct IORequest *)raw_req);
    req->io_Command = CMD_FLUSH;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_FLUSH succeeds");
    else
        test_fail_msg("CMD_FLUSH succeeds");
    WaitIO((struct IORequest *)raw_req);
    if (raw_req->io_Error == IOERR_ABORTED)
        test_ok("CMD_FLUSH aborts pending stopped write");
    else
        test_fail_msg("CMD_FLUSH aborts pending stopped write");

    req->io_Command = CMD_RESET;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_RESET succeeds");
    else
        test_fail_msg("CMD_RESET succeeds");

    req->io_Command = PRD_QUERY;
    req->io_Flags = IOF_QUICK;
    req->io_Data = status;
    req->io_Length = sizeof(status);
    status[0] = 0;
    status[1] = 1;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && req->io_Actual == 1 && status[0] == 0x04 && status[1] == 0)
        test_ok("CMD_RESET restores default status");
    else
        test_fail_msg("CMD_RESET restores default status");

    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice handle");

    DeleteIORequest((struct IORequest *)tags_req);
    DeleteIORequest((struct IORequest *)dump_req);
    DeleteIORequest((struct IORequest *)cmd_req);
    DeleteIORequest((struct IORequest *)small_req);
    DeleteIORequest((struct IORequest *)raw_req);
    DeleteIORequest((struct IORequest *)req);
    CloseLibrary((struct Library *)DOSBase);
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
