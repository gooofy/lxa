/*
 * Test for parallel.device opens, parameter updates, loopback I/O, and pending reads.
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
    struct IOExtPar *req;
    struct IOExtPar *shared_req;
    struct IOExtPar *exclusive_req;
    struct IOStdReq *small_req;
    struct NSDeviceQueryResult query;
    UBYTE write_buf[7] = { 'P', 'R', 'I', 'N', 'T', '\n', 0 };
    UBYTE read_buf[8];
    LONG error;

    print("Testing parallel.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOExtPar *)CreateIORequest(port, sizeof(struct IOExtPar));
    shared_req = (struct IOExtPar *)CreateIORequest(port, sizeof(struct IOExtPar));
    exclusive_req = (struct IOExtPar *)CreateIORequest(port, sizeof(struct IOExtPar));
    small_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!req || !shared_req || !exclusive_req || !small_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)PARALLELNAME, 1, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)req);
    }

    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)small_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)small_req);
    }

    req->io_ParFlags = 0;
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)req, 0);
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

    if (req->IOPar.io_Device != NULL && req->IOPar.io_Unit != NULL)
        test_ok("OpenDevice stores device and unit pointers");
    else
        test_fail_msg("OpenDevice stores device and unit pointers");

    if (req->io_PExtFlags == 0 && req->io_ParFlags == 0)
        test_ok("OpenDevice loads default parallel parameters");
    else
        test_fail_msg("OpenDevice loads default parallel parameters");

    shared_req->io_ParFlags = PARF_SHARED;
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)shared_req, 0);
    if (error == ParErr_DevBusy)
        test_ok("Exclusive open blocks shared reopen");
    else
        test_fail_msg("Exclusive open blocks shared reopen");

    req->IOPar.io_Command = NSCMD_DEVICEQUERY;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = &query;
    req->IOPar.io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0 &&
        req->IOPar.io_Actual == sizeof(query) &&
        query.nsdqr_DeviceType == NSDEVTYPE_PARALLEL &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports parallel capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports parallel capabilities");

    req->io_PExtFlags = 0;
    req->io_ParFlags = PARF_EOFMODE | PARF_FASTMODE;
    req->io_PTermArray.PTermArray0 = 0x0a000000UL;
    req->io_PTermArray.PTermArray1 = 0;
    req->IOPar.io_Command = PDCMD_SETPARAMS;
    req->IOPar.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0)
        test_ok("PDCMD_SETPARAMS accepts valid parameters");
    else
        test_fail_msg("PDCMD_SETPARAMS accepts valid parameters");

    if ((req->io_ParFlags & PARF_EOFMODE) && (req->io_ParFlags & PARF_FASTMODE))
        test_ok("PDCMD_SETPARAMS updates request configuration");
    else
        test_fail_msg("PDCMD_SETPARAMS updates request configuration");

    req->IOPar.io_Command = CMD_WRITE;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = write_buf;
    req->IOPar.io_Length = (ULONG)-1;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0 && req->IOPar.io_Actual == 6)
        test_ok("CMD_WRITE accepts null-terminated payload");
    else
        test_fail_msg("CMD_WRITE accepts null-terminated payload");

    req->IOPar.io_Command = PDCMD_QUERY;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = NULL;
    req->IOPar.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0 && (req->io_Status & IOPTF_RWDIR) != 0)
        test_ok("PDCMD_QUERY reports write direction status");
    else
        test_fail_msg("PDCMD_QUERY reports write direction status");

    req->IOPar.io_Command = CMD_READ;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = read_buf;
    req->IOPar.io_Length = sizeof(read_buf);
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0 &&
        req->IOPar.io_Actual == 6 &&
        read_buf[0] == 'P' && read_buf[4] == 'T' && read_buf[5] == '\n')
        test_ok("CMD_READ returns looped-back bytes up to terminator");
    else
        test_fail_msg("CMD_READ returns looped-back bytes up to terminator");

    req->IOPar.io_Command = PDCMD_QUERY;
    req->IOPar.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if ((req->io_Status & IOPTF_RWDIR) == 0)
        test_ok("CMD_READ clears write direction status");
    else
        test_fail_msg("CMD_READ clears write direction status");

    req->IOPar.io_Command = CMD_READ;
    req->IOPar.io_Flags = 0;
    req->IOPar.io_Data = read_buf;
    req->IOPar.io_Length = 1;
    SendIO((struct IORequest *)req);
    if (CheckIO((struct IORequest *)req) == NULL)
        test_ok("CMD_READ stays pending with no buffered data");
    else
        test_fail_msg("CMD_READ stays pending with no buffered data");

    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    if (req->IOPar.io_Error == IOERR_ABORTED)
        test_ok("AbortIO aborts pending exclusive CMD_READ");
    else
        test_fail_msg("AbortIO aborts pending exclusive CMD_READ");

    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice exclusive handle");

    req->io_ParFlags = PARF_SHARED;
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("Shared open succeeds after close");
    else
        test_fail_msg("Shared open succeeds after close");

    shared_req->io_ParFlags = PARF_SHARED;
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)shared_req, 0);
    if (error == 0)
        test_ok("Second shared open succeeds");
    else
        test_fail_msg("Second shared open succeeds");

    shared_req->IOPar.io_Command = CMD_READ;
    shared_req->IOPar.io_Flags = 0;
    shared_req->IOPar.io_Data = read_buf;
    shared_req->IOPar.io_Length = 1;
    SendIO((struct IORequest *)shared_req);
    if (CheckIO((struct IORequest *)shared_req) == NULL)
        test_ok("Shared CMD_READ can pend");
    else
        test_fail_msg("Shared CMD_READ can pend");

    req->io_PExtFlags = 0;
    req->io_ParFlags = PARF_SHARED | PARF_EOFMODE;
    req->io_PTermArray.PTermArray0 = 0;
    req->io_PTermArray.PTermArray1 = 0;
    req->IOPar.io_Command = PDCMD_SETPARAMS;
    req->IOPar.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == ParErr_DevBusy)
        test_ok("PDCMD_SETPARAMS reports busy while read is pending");
    else
        test_fail_msg("PDCMD_SETPARAMS reports busy while read is pending");

    AbortIO((struct IORequest *)shared_req);
    WaitIO((struct IORequest *)shared_req);
    if (shared_req->IOPar.io_Error == IOERR_ABORTED)
        test_ok("AbortIO aborts pending CMD_READ");
    else
        test_fail_msg("AbortIO aborts pending CMD_READ");

    exclusive_req->io_ParFlags = 0;
    error = OpenDevice((STRPTR)PARALLELNAME, 0, (struct IORequest *)exclusive_req, 0);
    if (error == ParErr_DevBusy)
        test_ok("Shared users block exclusive reopen");
    else
        test_fail_msg("Shared users block exclusive reopen");

    req->IOPar.io_Command = CMD_STOP;
    req->IOPar.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0)
        test_ok("CMD_STOP succeeds");
    else
        test_fail_msg("CMD_STOP succeeds");

    shared_req->IOPar.io_Command = CMD_READ;
    shared_req->IOPar.io_Flags = 0;
    shared_req->IOPar.io_Data = read_buf;
    shared_req->IOPar.io_Length = 6;
    SendIO((struct IORequest *)shared_req);
    if (CheckIO((struct IORequest *)shared_req) == NULL)
        test_ok("CMD_STOP keeps reads pending");
    else
        test_fail_msg("CMD_STOP keeps reads pending");

    req->IOPar.io_Command = CMD_WRITE;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = write_buf;
    req->IOPar.io_Length = 6;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0)
        test_ok("Shared CMD_WRITE succeeds while stopped");
    else
        test_fail_msg("Shared CMD_WRITE succeeds while stopped");

    if (CheckIO((struct IORequest *)shared_req) == NULL)
        test_ok("Stopped read remains pending after write");
    else
        test_fail_msg("Stopped read remains pending after write");

    req->IOPar.io_Command = CMD_START;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = NULL;
    req->IOPar.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0)
        test_ok("CMD_START succeeds");
    else
        test_fail_msg("CMD_START succeeds");

    WaitIO((struct IORequest *)shared_req);
    if (shared_req->IOPar.io_Error == 0 && shared_req->IOPar.io_Actual == 6)
        test_ok("CMD_START completes pending read");
    else
        test_fail_msg("CMD_START completes pending read");

    req->IOPar.io_Command = CMD_WRITE;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = write_buf;
    req->IOPar.io_Length = 6;
    DoIO((struct IORequest *)req);

    req->IOPar.io_Command = CMD_FLUSH;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = NULL;
    req->IOPar.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0)
        test_ok("CMD_FLUSH succeeds");
    else
        test_fail_msg("CMD_FLUSH succeeds");

    shared_req->IOPar.io_Command = CMD_READ;
    shared_req->IOPar.io_Flags = 0;
    shared_req->IOPar.io_Data = read_buf;
    shared_req->IOPar.io_Length = 1;
    SendIO((struct IORequest *)shared_req);
    if (CheckIO((struct IORequest *)shared_req) == NULL)
        test_ok("CMD_FLUSH clears buffered input");
    else
        test_fail_msg("CMD_FLUSH clears buffered input");
    AbortIO((struct IORequest *)shared_req);
    WaitIO((struct IORequest *)shared_req);

    req->io_PExtFlags = 1;
    req->io_ParFlags = PARF_SHARED;
    req->IOPar.io_Command = PDCMD_SETPARAMS;
    req->IOPar.io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == ParErr_InvParam)
        test_ok("PDCMD_SETPARAMS rejects io_PExtFlags");
    else
        test_fail_msg("PDCMD_SETPARAMS rejects io_PExtFlags");

    req->IOPar.io_Command = CMD_RESET;
    req->IOPar.io_Flags = IOF_QUICK;
    req->IOPar.io_Data = NULL;
    req->IOPar.io_Length = 0;
    DoIO((struct IORequest *)req);
    if (req->IOPar.io_Error == 0 && req->io_ParFlags == PARF_SHARED)
        test_ok("CMD_RESET restores default parameters");
    else
        test_fail_msg("CMD_RESET restores default parameters");

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
