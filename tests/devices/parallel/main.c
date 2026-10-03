/*
 * Test for parallel.device opens, parameters and the command set
 * (validated against AmigaOS 3.1, Phase 220).
 *
 * Only behaviour that does not depend on a peer at the port is tested
 * here: the reference has nothing connected, so a CMD_WRITE never
 * completes there.  Loopback I/O through lxa's emulated peer is covered by
 * the lxa-only Tests/Devices/ParallelLoopback.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/newstyle.h>
#include <devices/parallel.h>
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

/* run a command that completes without a peer; print io_Error/io_Actual */
static void run(struct IOExtPar *req, const char *name, UWORD cmd)
{
    req->IOPar.io_Command = cmd;
    req->IOPar.io_Data = NULL;
    req->IOPar.io_Length = 0;
    req->IOPar.io_Actual = 12345;
    DoIO((struct IORequest *)req);
    print("  ");
    print(name);
    print(": io_Error=");
    print_num(req->IOPar.io_Error);
    print(" io_Actual=");
    print_num(req->IOPar.io_Actual);
    print("\n");
}

static LONG open_par(struct IOExtPar *req, ULONG unit, UBYTE parflags)
{
    req->io_ParFlags = parflags;
    return OpenDevice((STRPTR)PARALLELNAME, unit, (struct IORequest *)req, 0);
}

int main(void)
{
    struct MsgPort *port;
    struct IOExtPar *req;
    struct IOExtPar *req2;
    struct IOStdReq *small_req;
    UBYTE read_buf[8];
    LONG error;

    print("Testing parallel.device\n\n");

    port = CreateMsgPort();
    req = (struct IOExtPar *)CreateIORequest(port, sizeof(struct IOExtPar));
    req2 = (struct IOExtPar *)CreateIORequest(port, sizeof(struct IOExtPar));
    small_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!port || !req || !req2 || !small_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    /* the unit number is ignored and the request size is not checked */
    error = open_par(req, 1, 0);
    check(error == 0, "OpenDevice accepts unit 1");
    if (error == 0)
        CloseDevice((struct IORequest *)req);
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)small_req, 0);
    check(error == 0, "OpenDevice accepts an IOStdReq-sized request");
    if (error == 0)
        CloseDevice((struct IORequest *)small_req);

    error = open_par(req, 0, 0);
    check(error == 0, "OpenDevice exclusive open succeeds");
    if (error != 0)
        return 20;
    check(req->io_PExtFlags == 0 && req->io_ParFlags == 0 &&
          req->io_PTermArray.PTermArray0 == 0 && req->io_PTermArray.PTermArray1 == 0,
          "OpenDevice loads default parameters");

    error = open_par(req2, 0, PARF_SHARED);
    check(error == ParErr_DevBusy, "exclusive open blocks a shared open");
    if (error == 0)
        CloseDevice((struct IORequest *)req2);

    req->io_PExtFlags = 0;
    req->io_ParFlags = PARF_EOFMODE | PARF_FASTMODE;
    req->io_PTermArray.PTermArray0 = 0x0a000000UL;
    req->io_PTermArray.PTermArray1 = 0;
    run(req, "PDCMD_SETPARAMS", PDCMD_SETPARAMS);
    check(req->io_ParFlags == (PARF_EOFMODE | PARF_FASTMODE), "PDCMD_SETPARAMS keeps the new flags");

    /* nothing to read: CMD_READ waits until aborted */
    req->IOPar.io_Command = CMD_READ;
    req->IOPar.io_Data = read_buf;
    req->IOPar.io_Length = 1;
    SendIO((struct IORequest *)req);
    check(CheckIO((struct IORequest *)req) == NULL, "CMD_READ stays pending without data");
    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    check(req->IOPar.io_Error == IOERR_ABORTED && req->IOPar.io_Actual == 0,
          "AbortIO aborts the pending CMD_READ");

    run(req, "CMD_STOP", CMD_STOP);
    run(req, "CMD_START", CMD_START);
    run(req, "CMD_FLUSH", CMD_FLUSH);
    run(req, "CMD_CLEAR", CMD_CLEAR);
    run(req, "CMD_UPDATE", CMD_UPDATE);
    run(req, "CMD_INVALID", CMD_INVALID);
    run(req, "NSCMD_DEVICEQUERY", NSCMD_DEVICEQUERY);
    run(req, "command 0x7fff", 0x7fff);

    req->io_PExtFlags = 1;
    run(req, "PDCMD_SETPARAMS with io_PExtFlags", PDCMD_SETPARAMS);

    run(req, "CMD_RESET", CMD_RESET);
    print("  after CMD_RESET: io_ParFlags=");
    print_num(req->io_ParFlags);
    print(" io_PExtFlags=");
    print_num(req->io_PExtFlags);
    print("\n");

    CloseDevice((struct IORequest *)req);

    error = open_par(req, 0, PARF_SHARED);
    check(error == 0, "shared open succeeds");
    error = open_par(req2, 0, PARF_SHARED);
    check(error == 0, "second shared open succeeds");
    if (error == 0)
        CloseDevice((struct IORequest *)req2);
    error = open_par(req2, 0, 0);
    check(error == ParErr_DevBusy, "shared users block an exclusive open");
    if (error == 0)
        CloseDevice((struct IORequest *)req2);
    CloseDevice((struct IORequest *)req);

    DeleteIORequest((struct IORequest *)small_req);
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
