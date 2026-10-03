/*
 * Test for printer.device opens, the command set and PRD_DUMPRPORT
 * (validated against AmigaOS 3.1 with the Generic printer driver, Phase 220).
 *
 * Only behaviour that needs no printer is tested here: the reference has
 * no printer at its parallel port, so anything sent to the printer never
 * completes there.  Output to lxa's emulated printer is covered by the
 * lxa-only Tests/Devices/PrinterPeer.
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

/* run a command that completes without a printer; print io_Error/io_Actual */
static void run(struct IOStdReq *req, const char *name, UWORD cmd)
{
    req->io_Command = cmd;
    req->io_Data = NULL;
    req->io_Length = 0;
    req->io_Actual = 12345;
    DoIO((struct IORequest *)req);
    print("  ");
    print(name);
    print(": io_Error=");
    print_num(req->io_Error);
    print(" io_Actual=");
    print_num(req->io_Actual);
    print("\n");
}

static void dump(struct IODRPReq *dump_req, const char *name, UWORD special)
{
    dump_req->io_Command = PRD_DUMPRPORT;
    dump_req->io_RastPort = NULL;
    dump_req->io_ColorMap = NULL;
    dump_req->io_Modes = 0;
    dump_req->io_SrcX = 0;
    dump_req->io_SrcY = 0;
    dump_req->io_SrcWidth = 320;
    dump_req->io_SrcHeight = 200;
    dump_req->io_DestCols = 0;
    dump_req->io_DestRows = 0;
    dump_req->io_Special = special;
    DoIO((struct IORequest *)dump_req);
    print("  ");
    print(name);
    print(": io_Error=");
    print_num(dump_req->io_Error);
    print(" io_DestCols=");
    print_num(dump_req->io_DestCols);
    print(" io_DestRows=");
    print_num(dump_req->io_DestRows);
    print("\n");
}

int main(void)
{
    struct MsgPort *port;
    struct IOStdReq *req;
    struct IOStdReq *req2;
    struct IODRPReq *dump_req;
    LONG error;

    print("Testing printer.device\n\n");

    port = CreateMsgPort();
    req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    req2 = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    dump_req = (struct IODRPReq *)CreateIORequest(port, sizeof(struct IODRPReq));
    if (!port || !req || !req2 || !dump_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    /* the unit number is ignored */
    error = OpenDevice((STRPTR)"printer.device", 1, (struct IORequest *)req2, 0);
    check(error == 0, "OpenDevice accepts unit 1");
    if (error == 0)
        CloseDevice((struct IORequest *)req2);

    error = OpenDevice((STRPTR)"printer.device", 0, (struct IORequest *)req, 0);
    check(error == 0 && req->io_Device != NULL && req->io_Unit != NULL,
          "OpenDevice stores device and unit pointers");
    if (error != 0)
        return 20;

    error = OpenDevice((STRPTR)"printer.device", 0, (struct IORequest *)req2, 0);
    print("  second OpenDevice: error=");
    print_num(error);
    print("\n");
    if (error == 0)
        CloseDevice((struct IORequest *)req2);

    run(req, "NSCMD_DEVICEQUERY", NSCMD_DEVICEQUERY);
    run(req, "CMD_STOP", CMD_STOP);
    run(req, "CMD_START", CMD_START);
    run(req, "CMD_FLUSH", CMD_FLUSH);
    run(req, "CMD_RESET", CMD_RESET);
    run(req, "CMD_CLEAR", CMD_CLEAR);
    run(req, "CMD_UPDATE", CMD_UPDATE);
    run(req, "CMD_READ", CMD_READ);
    run(req, "CMD_INVALID", CMD_INVALID);
    run(req, "command 0x7fff", 0x7fff);

    dump_req->io_Device = req->io_Device;
    dump_req->io_Unit = req->io_Unit;
    dump_req->io_Message.mn_ReplyPort = port;
    dump(dump_req, "PRD_DUMPRPORT SPECIAL_NOPRINT", SPECIAL_NOPRINT);
    dump(dump_req, "PRD_DUMPRPORT", 0);

    CloseDevice((struct IORequest *)req);
    check(TRUE, "CloseDevice");

    DeleteIORequest((struct IORequest *)dump_req);
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
