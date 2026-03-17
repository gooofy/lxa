/*
 * Test for serial.device opens, parameter updates, loopback I/O, and pending reads.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/serial.h>
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
    struct IOExtSer *req;
    struct IOExtSer *shared_req;
    struct IOExtSer *exclusive_req;
    struct IOStdReq *small_req;
    struct NSDeviceQueryResult query;
    UBYTE write_buf[6] = { 'H', 'E', 'L', 'L', 'O', '\n' };
    UBYTE read_buf[8];
    LONG error;

    print("Testing serial.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOExtSer *)CreateIORequest(port, sizeof(struct IOExtSer));
    shared_req = (struct IOExtSer *)CreateIORequest(port, sizeof(struct IOExtSer));
    exclusive_req = (struct IOExtSer *)CreateIORequest(port, sizeof(struct IOExtSer));
    small_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!req || !shared_req || !exclusive_req || !small_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)SERIALNAME, 1, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)req);
    }

    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)small_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)small_req);
    }

    req->io_SerFlags = 0;
    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("OpenDevice exclusive open succeeds");
    else
    {
        test_fail_msg("OpenDevice exclusive open succeeds");
        print("    error=");
        print_num(error);
        print("\n");
        return 20;
    }

    if (req->IOSer.io_Device != NULL && req->IOSer.io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    if (req->io_Baud == 9600 && req->io_ReadLen == 8 && req->io_WriteLen == 8 && req->io_RBufLen == 1024)
        test_ok("OpenDevice loads default serial parameters");
    else
        test_fail_msg("OpenDevice loads default serial parameters");

    shared_req->io_SerFlags = SERF_SHARED;
    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)shared_req, 0);
    if (error == SerErr_DevBusy)
        test_ok("Exclusive open blocks shared reopen");
    else
        test_fail_msg("Exclusive open blocks shared reopen");

    req->IOSer.io_Command = NSCMD_DEVICEQUERY;
    req->IOSer.io_Flags = IOF_QUICK;
    req->IOSer.io_Data = &query;
    req->IOSer.io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0 &&
        req->IOSer.io_Actual == sizeof(query) &&
        query.nsdqr_DeviceType == NSDEVTYPE_SERIAL &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports serial capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports serial capabilities");

    req->io_Baud = 19200;
    req->io_RBufLen = 2048;
    req->io_ReadLen = 7;
    req->io_WriteLen = 7;
    req->io_StopBits = 2;
    req->io_SerFlags = SERF_EOFMODE | SERF_XDISABLED;
    req->io_ExtFlags = 0;
    req->io_TermArray.TermArray0 = 0x0a000000UL;
    req->io_TermArray.TermArray1 = 0;
    req->IOSer.io_Command = SDCMD_SETPARAMS;
    req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0)
        test_ok("SDCMD_SETPARAMS accepts valid parameters");
    else
        test_fail_msg("SDCMD_SETPARAMS accepts valid parameters");

    if (req->io_Baud == 19200 && req->io_RBufLen == 2048 && req->io_StopBits == 2 && (req->io_SerFlags & SERF_EOFMODE))
        test_ok("SDCMD_SETPARAMS updates request configuration");
    else
        test_fail_msg("SDCMD_SETPARAMS updates request configuration");

    req->IOSer.io_Command = CMD_WRITE;
    req->IOSer.io_Flags = IOF_QUICK;
    req->IOSer.io_Data = write_buf;
    req->IOSer.io_Length = sizeof(write_buf);
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0 && req->IOSer.io_Actual == sizeof(write_buf))
        test_ok("CMD_WRITE accepts loopback payload");
    else
        test_fail_msg("CMD_WRITE accepts loopback payload");

    req->IOSer.io_Command = SDCMD_QUERY;
    req->IOSer.io_Flags = IOF_QUICK;
    req->IOSer.io_Data = NULL;
    req->IOSer.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0 && req->IOSer.io_Actual == sizeof(write_buf))
        test_ok("SDCMD_QUERY reports unread byte count");
    else
        test_fail_msg("SDCMD_QUERY reports unread byte count");

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Flags = IOF_QUICK;
    req->IOSer.io_Data = read_buf;
    req->IOSer.io_Length = sizeof(read_buf);
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0 &&
        req->IOSer.io_Actual == sizeof(write_buf) &&
        read_buf[0] == 'H' && read_buf[4] == 'O' && read_buf[5] == '\n')
        test_ok("CMD_READ returns looped-back bytes up to terminator");
    else
        test_fail_msg("CMD_READ returns looped-back bytes up to terminator");

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Flags = 0;
    req->IOSer.io_Data = read_buf;
    req->IOSer.io_Length = 1;
    SendIO((struct IORequest *)req);
    if (CheckIO((struct IORequest *)req) == NULL)
        test_ok("CMD_READ stays pending with no buffered data");
    else
        test_fail_msg("CMD_READ stays pending with no buffered data");

    shared_req->IOSer.io_Device = req->IOSer.io_Device;
    shared_req->IOSer.io_Unit = req->IOSer.io_Unit;
    shared_req->io_SerFlags = SERF_XDISABLED;
    shared_req->IOSer.io_Command = SDCMD_SETPARAMS;
    shared_req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)shared_req);
    if (shared_req->IOSer.io_Error == SerErr_DevBusy)
        test_ok("SDCMD_SETPARAMS reports busy while read is pending");
    else
        test_fail_msg("SDCMD_SETPARAMS reports busy while read is pending");

    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    if (req->IOSer.io_Error == IOERR_ABORTED)
        test_ok("AbortIO aborts pending CMD_READ");
    else
        test_fail_msg("AbortIO aborts pending CMD_READ");

    req->IOSer.io_Command = SDCMD_BREAK;
    req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0)
        test_ok("SDCMD_BREAK succeeds");
    else
        test_fail_msg("SDCMD_BREAK succeeds");

    req->IOSer.io_Command = SDCMD_QUERY;
    req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if ((req->io_Status & IO_STATF_WROTEBREAK) != 0)
        test_ok("SDCMD_QUERY reports break status");
    else
        test_fail_msg("SDCMD_QUERY reports break status");

    req->IOSer.io_Command = CMD_FLUSH;
    req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Error == 0)
        test_ok("CMD_FLUSH succeeds");
    else
        test_fail_msg("CMD_FLUSH succeeds");

    req->IOSer.io_Command = SDCMD_QUERY;
    req->IOSer.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOSer.io_Actual == 0)
        test_ok("CMD_FLUSH clears buffered input");
    else
        test_fail_msg("CMD_FLUSH clears buffered input");

    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice exclusive handle");

    req->io_SerFlags = SERF_SHARED;
    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("Shared open succeeds after close");
    else
        test_fail_msg("Shared open succeeds after close");

    shared_req->io_SerFlags = SERF_SHARED;
    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)shared_req, 0);
    if (error == 0)
        test_ok("Second shared open succeeds");
    else
        test_fail_msg("Second shared open succeeds");

    exclusive_req->io_SerFlags = 0;
    error = OpenDevice((STRPTR)SERIALNAME, 0, (struct IORequest *)exclusive_req, 0);
    if (error == SerErr_DevBusy)
        test_ok("Shared users block exclusive reopen");
    else
        test_fail_msg("Shared users block exclusive reopen");

    CloseDevice((struct IORequest *)shared_req);
    test_ok("CloseDevice shared handle 2");
    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice shared handle 1");

    DeleteIORequest((struct IORequest *)small_req);
    DeleteIORequest((struct IORequest *)exclusive_req);
    DeleteIORequest((struct IORequest *)shared_req);
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
