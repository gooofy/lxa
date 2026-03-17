/*
 * Test for ramdrive.device opens, geometry/status queries, memory-backed I/O,
 * and KillRAD helpers.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/trackdisk.h>

#define RAMDRIVE_BASE_NAME RamdriveBase
#include <proto/ramdrive.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Device *RamdriveBase;

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

static BOOL buffer_matches(const UBYTE *buf, const UBYTE *expected, ULONG len)
{
    ULONG i;

    for (i = 0; i < len; i++)
    {
        if (buf[i] != expected[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

int main(void)
{
    struct MsgPort *port;
    struct IOExtTD *req;
    struct IOExtTD *protected_req;
    struct IOStdReq *small_req;
    struct NSDeviceQueryResult query;
    struct DriveGeometry geometry;
    UBYTE write_buf[6] = { 'R', 'A', 'M', 'D', 'I', 'S' };
    UBYTE format_buf[4] = { 'T', 'E', 'S', 'T' };
    UBYTE read_buf[8] = { 0 };
    STRPTR name;
    LONG error;

    print("Testing ramdrive.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOExtTD *)CreateIORequest(port, sizeof(struct IOExtTD));
    protected_req = (struct IOExtTD *)CreateIORequest(port, sizeof(struct IOExtTD));
    small_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!req || !protected_req || !small_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice("ramdrive.device", 1, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)req);
    }

    small_req->io_Message.mn_Length = sizeof(struct Message);
    error = OpenDevice("ramdrive.device", 0, (struct IORequest *)small_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)small_req);
    }

    error = OpenDevice("ramdrive.device", 0, (struct IORequest *)req, 0);
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

    if (req->iotd_Req.io_Device != NULL && req->iotd_Req.io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    RamdriveBase = (struct Device *)req->iotd_Req.io_Device;

    req->iotd_Req.io_Command = NSCMD_DEVICEQUERY;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = &query;
    req->iotd_Req.io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        req->iotd_Req.io_Actual == sizeof(query) &&
        query.nsdqr_DeviceType == NSDEVTYPE_TRACKDISK &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports ramdrive capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports ramdrive capabilities");

    req->iotd_Req.io_Command = TD_PROTSTATUS;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == 0)
        test_ok("TD_PROTSTATUS defaults to writable");
    else
        test_fail_msg("TD_PROTSTATUS defaults to writable");

    req->iotd_Req.io_Command = CMD_WRITE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = write_buf;
    req->iotd_Req.io_Length = sizeof(write_buf);
    req->iotd_Req.io_Offset = 4;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == sizeof(write_buf))
        test_ok("CMD_WRITE stores bytes in RAM backing");
    else
        test_fail_msg("CMD_WRITE stores bytes in RAM backing");

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = read_buf;
    req->iotd_Req.io_Length = sizeof(write_buf);
    req->iotd_Req.io_Offset = 4;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        req->iotd_Req.io_Actual == sizeof(write_buf) &&
        buffer_matches(read_buf, write_buf, sizeof(write_buf)))
        test_ok("CMD_READ returns stored bytes");
    else
        test_fail_msg("CMD_READ returns stored bytes");

    req->iotd_Req.io_Command = TD_FORMAT;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = format_buf;
    req->iotd_Req.io_Length = sizeof(format_buf);
    req->iotd_Req.io_Offset = 16;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == sizeof(format_buf))
        test_ok("TD_FORMAT aliases CMD_WRITE");
    else
        test_fail_msg("TD_FORMAT aliases CMD_WRITE");

    req->iotd_Req.io_Command = ETD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = read_buf;
    req->iotd_Req.io_Length = sizeof(format_buf);
    req->iotd_Req.io_Offset = 16;
    req->iotd_Count = 1;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        req->iotd_Req.io_Actual == sizeof(format_buf) &&
        buffer_matches(read_buf, format_buf, sizeof(format_buf)))
        test_ok("ETD_READ accepts current change count");
    else
        test_fail_msg("ETD_READ accepts current change count");

    req->iotd_Req.io_Command = ETD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = read_buf;
    req->iotd_Req.io_Length = 1;
    req->iotd_Req.io_Offset = 0;
    req->iotd_Count = 2;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == TDERR_DiskChanged)
        test_ok("ETD_READ rejects stale change count");
    else
        test_fail_msg("ETD_READ rejects stale change count");

    req->iotd_Req.io_Command = CMD_UPDATE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = NULL;
    req->iotd_Req.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("CMD_UPDATE succeeds");
    else
        test_fail_msg("CMD_UPDATE succeeds");

    req->iotd_Req.io_Command = CMD_CLEAR;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0)
        test_ok("CMD_CLEAR succeeds");
    else
        test_fail_msg("CMD_CLEAR succeeds");

    req->iotd_Req.io_Command = TD_CHANGENUM;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == 1)
        test_ok("TD_CHANGENUM reports constant change count");
    else
        test_fail_msg("TD_CHANGENUM reports constant change count");

    req->iotd_Req.io_Command = TD_CHANGESTATE;
    req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == 0)
        test_ok("TD_CHANGESTATE reports medium present");
    else
        test_fail_msg("TD_CHANGESTATE reports medium present");

    req->iotd_Req.io_Command = TD_GETGEOMETRY;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = &geometry;
    req->iotd_Req.io_Length = sizeof(geometry);
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        geometry.dg_SectorSize == 512 &&
        geometry.dg_TotalSectors == 1760 &&
        geometry.dg_DeviceType == DG_DIRECT_ACCESS &&
        geometry.dg_Flags == DGF_REMOVABLE)
        test_ok("TD_GETGEOMETRY reports RAD geometry");
    else
        test_fail_msg("TD_GETGEOMETRY reports RAD geometry");

    req->iotd_Req.io_Command = TD_MOTOR;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 && req->iotd_Req.io_Actual == 1)
        test_ok("TD_MOTOR reports always-on virtual motor");
    else
        test_fail_msg("TD_MOTOR reports always-on virtual motor");

    name = KillRAD(1);
    if (name == NULL)
        test_ok("KillRAD rejects invalid unit");
    else
        test_fail_msg("KillRAD rejects invalid unit");

    name = KillRAD0();
    if (name != NULL && name[0] == 'R' && name[1] == 'A' && name[2] == 'D' && name[3] == ':')
        test_ok("KillRAD0 returns RAD: device name");
    else
        test_fail_msg("KillRAD0 returns RAD: device name");

    req->iotd_Req.io_Command = CMD_READ;
    req->iotd_Req.io_Flags = IOF_QUICK;
    req->iotd_Req.io_Data = read_buf;
    req->iotd_Req.io_Length = sizeof(write_buf);
    req->iotd_Req.io_Offset = 4;
    DoIO((struct IORequest *)req);
    if (req->iotd_Req.io_Error == 0 &&
        req->iotd_Req.io_Actual == sizeof(write_buf) &&
        read_buf[0] == 0 && read_buf[5] == 0)
        test_ok("KillRAD0 clears RAM backing store");
    else
        test_fail_msg("KillRAD0 clears RAM backing store");

    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice writable handle");

    error = OpenDevice("ramdrive.device", 0, (struct IORequest *)protected_req, 1);
    if (error == 0)
        test_ok("OpenDevice write-protected open succeeds");
    else
        test_fail_msg("OpenDevice write-protected open succeeds");

    RamdriveBase = (struct Device *)protected_req->iotd_Req.io_Device;

    protected_req->iotd_Req.io_Command = TD_PROTSTATUS;
    protected_req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)protected_req);
    if (protected_req->iotd_Req.io_Error == 0 && protected_req->iotd_Req.io_Actual != 0)
        test_ok("OpenDevice flags enable write protection");
    else
        test_fail_msg("OpenDevice flags enable write protection");

    protected_req->iotd_Req.io_Command = CMD_WRITE;
    protected_req->iotd_Req.io_Flags = IOF_QUICK;
    protected_req->iotd_Req.io_Data = write_buf;
    protected_req->iotd_Req.io_Length = sizeof(write_buf);
    protected_req->iotd_Req.io_Offset = 0;
    DoIO((struct IORequest *)protected_req);
    if (protected_req->iotd_Req.io_Error == TDERR_WriteProt)
        test_ok("CMD_WRITE respects write protection");
    else
        test_fail_msg("CMD_WRITE respects write protection");

    name = KillRAD(0);
    if (name != NULL && name[0] == 'R' && name[1] == 'A' && name[2] == 'D' && name[3] == ':')
        test_ok("KillRAD accepts unit 0");
    else
        test_fail_msg("KillRAD accepts unit 0");

    protected_req->iotd_Req.io_Command = TD_PROTSTATUS;
    protected_req->iotd_Req.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)protected_req);
    if (protected_req->iotd_Req.io_Error == 0 && protected_req->iotd_Req.io_Actual == 0)
        test_ok("KillRAD clears write protection");
    else
        test_fail_msg("KillRAD clears write protection");

    CloseDevice((struct IORequest *)protected_req);
    test_ok("CloseDevice write-protected handle");

    DeleteIORequest((struct IORequest *)small_req);
    DeleteIORequest((struct IORequest *)protected_req);
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
