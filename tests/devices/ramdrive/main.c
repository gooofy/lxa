/*
 * Test for ramdrive.device (validated against AmigaOS 3.1, Phase 220).
 *
 * A ramdrive unit only opens once a DOS device node for it exists: the
 * device takes its size from that node's DosEnvec, as set up by
 * "Mount RAD:".  The test therefore creates its own node (RAE:, unit 1,
 * not started) with MakeDosNode()/AddDosEntry(), exercises the unit, frees
 * it with KillRAD() and removes the node again.  After KillRAD() the unit
 * reports TDERR_DiskChanged; a second KillRAD() of the unit hangs 3.1.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/trackdisk.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>

#define RAMDRIVE_BASE_NAME RamdriveBase
#include <proto/ramdrive.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/expansion_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/expansion.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct Device *RamdriveBase;
struct Library *ExpansionBase;

#define TEST_UNIT       1
#define TEST_CYLINDERS  10
#define TEST_SIZE       (TEST_CYLINDERS * 2 * 11 * 512)

static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;

    while (*p++)
        len++;

    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;

    if (num < 0)
    {
        buf[i++] = '-';
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

        while (j > 0)
            buf[i++] = temp[--j];
    }

    buf[i] = '\0';
    print(buf);
}

static void check(BOOL ok, const char *name)
{
    print(ok ? "  OK: " : "  FAIL: ");
    print(name);
    print("\n");
    if (ok)
        test_pass++;
    else
        test_fail++;
}

static BOOL buffer_matches(const UBYTE *buf, const UBYTE *expected, ULONG len)
{
    ULONG i;

    for (i = 0; i < len; i++)
    {
        if (buf[i] != expected[i])
            return FALSE;
    }

    return TRUE;
}

/* run a command and print io_Error / io_Actual */
static void run(struct IOExtTD *req, const char *name, UWORD cmd, APTR data, ULONG len, ULONG offset)
{
    req->iotd_Req.io_Command = cmd;
    req->iotd_Req.io_Data = data;
    req->iotd_Req.io_Length = len;
    req->iotd_Req.io_Offset = offset;
    req->iotd_Req.io_Actual = 12345;
    req->iotd_Count = 1;
    DoIO((struct IORequest *)req);
    print("  ");
    print(name);
    print(": io_Error=");
    print_num(req->iotd_Req.io_Error);
    print(" io_Actual=");
    print_num(req->iotd_Req.io_Actual);
    print("\n");
}

static void print_name(const char *what, STRPTR name)
{
    print("  ");
    print(what);
    print(": ");
    print(name ? (const char *)name : "(NULL)");
    print("\n");
}

static struct DeviceNode *add_node(void)
{
    ULONG params[4 + DE_DOSTYPE + 1];
    struct DeviceNode *node;
    int i;

    for (i = 0; i < (int)(sizeof(params) / sizeof(params[0])); i++)
        params[i] = 0;

    params[0] = (ULONG)"RAE";
    params[1] = (ULONG)"ramdrive.device";
    params[2] = TEST_UNIT;
    params[3] = 0;
    params[4 + DE_TABLESIZE] = DE_DOSTYPE;
    params[4 + DE_SIZEBLOCK] = 128;
    params[4 + DE_NUMHEADS] = 2;
    params[4 + DE_SECSPERBLK] = 1;
    params[4 + DE_BLKSPERTRACK] = 11;
    params[4 + DE_RESERVEDBLKS] = 2;
    params[4 + DE_LOWCYL] = 0;
    params[4 + DE_UPPERCYL] = TEST_CYLINDERS - 1;
    params[4 + DE_NUMBUFFERS] = 5;
    params[4 + DE_BUFMEMTYPE] = 1;
    params[4 + DE_MAXTRANSFER] = 0x7fffffff;
    params[4 + DE_MASK] = 0xfffffffe;
    params[4 + DE_BOOTPRI] = -128;
    params[4 + DE_DOSTYPE] = 0x444f5300;

    node = MakeDosNode(params);
    if (node && !AddDosEntry((struct DosList *)node))
        return NULL;
    return node;
}

static void remove_node(struct DeviceNode *node)
{
    LockDosList(LDF_DEVICES | LDF_WRITE);
    RemDosEntry((struct DosList *)node);
    UnLockDosList(LDF_DEVICES | LDF_WRITE);
}

int main(void)
{
    struct MsgPort *port;
    struct IOExtTD *req;
    struct IOExtTD *protected_req;
    struct DeviceNode *node;
    UBYTE write_buf[6] = { 'R', 'A', 'M', 'D', 'I', 'S' };
    UBYTE format_buf[4] = { 'T', 'E', 'S', 'T' };
    UBYTE read_buf[512];
    UBYTE dummy[64];
    LONG error;

    print("Testing ramdrive.device\n\n");

    ExpansionBase = OpenLibrary((STRPTR)"expansion.library", 36);
    port = CreateMsgPort();
    req = (struct IOExtTD *)CreateIORequest(port, sizeof(struct IOExtTD));
    protected_req = (struct IOExtTD *)CreateIORequest(port, sizeof(struct IOExtTD));
    if (!ExpansionBase || !port || !req || !protected_req)
    {
        print("FAIL: Cannot set up test resources\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)req, 0);
    check(error == IOERR_OPENFAIL, "OpenDevice fails without a DOS device node");
    if (error == 0)
        CloseDevice((struct IORequest *)req);

    node = add_node();
    check(node != NULL, "MakeDosNode/AddDosEntry create RAE: for unit 1");
    if (!node)
        return 20;

    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)req, 0);
    check(error == 0, "OpenDevice succeeds once the node exists");
    if (error != 0)
    {
        remove_node(node);
        return 20;
    }
    RamdriveBase = (struct Device *)req->iotd_Req.io_Device;

    run(req, "TD_PROTSTATUS", TD_PROTSTATUS, NULL, 0, 0);
    run(req, "CMD_WRITE", CMD_WRITE, write_buf, sizeof(write_buf), 4);
    run(req, "CMD_READ", CMD_READ, read_buf, sizeof(write_buf), 4);
    check(buffer_matches(read_buf, write_buf, sizeof(write_buf)), "CMD_READ returns the written bytes");
    run(req, "TD_FORMAT", TD_FORMAT, format_buf, sizeof(format_buf), 16);
    run(req, "ETD_READ (count 1)", ETD_READ, read_buf, sizeof(format_buf), 16);
    check(buffer_matches(read_buf, format_buf, sizeof(format_buf)), "ETD_READ returns the formatted bytes");
    req->iotd_Req.io_Command = ETD_READ;
    req->iotd_Req.io_Data = read_buf;
    req->iotd_Req.io_Length = 1;
    req->iotd_Req.io_Offset = 0;
    req->iotd_Count = 2;
    DoIO((struct IORequest *)req);
    print("  ETD_READ (count 2): io_Error=");
    print_num(req->iotd_Req.io_Error);
    print("\n");
    run(req, "CMD_READ last block", CMD_READ, read_buf, 512, TEST_SIZE - 512);
    run(req, "CMD_READ past the end", CMD_READ, read_buf, 512, TEST_SIZE);
    run(req, "CMD_UPDATE", CMD_UPDATE, NULL, 0, 0);
    run(req, "CMD_CLEAR", CMD_CLEAR, NULL, 0, 0);
    run(req, "CMD_RESET", CMD_RESET, NULL, 0, 0);
    run(req, "CMD_FLUSH", CMD_FLUSH, NULL, 0, 0);
    run(req, "TD_CHANGENUM", TD_CHANGENUM, NULL, 0, 0);
    run(req, "TD_CHANGESTATE", TD_CHANGESTATE, NULL, 0, 0);
    run(req, "TD_MOTOR", TD_MOTOR, NULL, 0, 0);
    run(req, "TD_REMOVE", TD_REMOVE, NULL, 0, 0);
    run(req, "TD_SEEK", TD_SEEK, NULL, 0, 0);
    run(req, "TD_GETGEOMETRY", TD_GETGEOMETRY, dummy, sizeof(struct DriveGeometry), 0);
    run(req, "NSCMD_DEVICEQUERY", NSCMD_DEVICEQUERY, dummy, sizeof(dummy), 0);
    CloseDevice((struct IORequest *)req);

    /* the data survives closing; KillRAD() frees it */
    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)req, 0);
    check(error == 0, "OpenDevice reopens the unit");
    run(req, "CMD_READ after reopen", CMD_READ, read_buf, sizeof(write_buf), 4);
    check(buffer_matches(read_buf, write_buf, sizeof(write_buf)), "data survives close and reopen");
    CloseDevice((struct IORequest *)req);

    print_name("KillRAD(1)", KillRAD(TEST_UNIT));

    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)req, 0);
    check(error == 0, "OpenDevice after KillRAD succeeds");
    run(req, "CMD_READ after KillRAD", CMD_READ, read_buf, sizeof(write_buf), 4);
    CloseDevice((struct IORequest *)req);

    /* OpenDevice flags bit 0 write-protects the unit */
    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)protected_req, 1);
    check(error == 0, "OpenDevice with write protection succeeds");
    run(protected_req, "TD_PROTSTATUS (protected)", TD_PROTSTATUS, NULL, 0, 0);
    run(protected_req, "CMD_WRITE (protected)", CMD_WRITE, write_buf, sizeof(write_buf), 0);
    CloseDevice((struct IORequest *)protected_req);

    remove_node(node);
    error = OpenDevice((STRPTR)"ramdrive.device", TEST_UNIT, (struct IORequest *)req, 0);
    check(error == 0, "the unit stays known once the node is gone");
    if (error == 0)
        CloseDevice((struct IORequest *)req);

    DeleteIORequest((struct IORequest *)protected_req);
    DeleteIORequest((struct IORequest *)req);
    DeleteMsgPort(port);
    CloseLibrary(ExpansionBase);

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
