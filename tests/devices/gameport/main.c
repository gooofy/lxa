/*
 * Test for gameport.device controller, trigger, and request semantics.
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <devices/gameport.h>
#include <devices/inputevent.h>
#include <devices/newstyle.h>
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
            buf[i++] = '-';

        while (j > 0)
            buf[i++] = temp[--j];
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
    struct IOStdReq *req2;
    struct IOStdReq *bad_req;
    struct NSDeviceQueryResult query;
    struct GamePortTrigger trigger;
    struct GamePortTrigger trigger_out;
    struct InputEvent event;
    UBYTE ctype;
    LONG error;

    print("Testing gameport.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    req2 = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    bad_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct Message));
    if (!req || !req2 || !bad_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"gameport.device", 0, (struct IORequest *)req, 0);
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

    error = OpenDevice((STRPTR)"gameport.device", 1, (struct IORequest *)req2, 0);
    if (error == 0)
        test_ok("OpenDevice unit 1 succeeds");
    else
        test_fail_msg("OpenDevice unit 1 succeeds");

    error = OpenDevice((STRPTR)"gameport.device", 2, (struct IORequest *)bad_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects invalid unit");
    else
    {
        test_fail_msg("OpenDevice rejects invalid unit");
        CloseDevice((struct IORequest *)bad_req);
    }

    error = OpenDevice((STRPTR)"gameport.device", 0, (struct IORequest *)bad_req, 0);
    if (error != 0)
        test_ok("OpenDevice rejects undersized IORequest");
    else
    {
        test_fail_msg("OpenDevice rejects undersized IORequest");
        CloseDevice((struct IORequest *)bad_req);
    }

    req->io_Command = GPD_ASKCTYPE;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &ctype;
    req->io_Length = sizeof(ctype);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && ctype == GPCT_NOCONTROLLER && req->io_Actual == sizeof(ctype))
        test_ok("GPD_ASKCTYPE reports default type");
    else
        test_fail_msg("GPD_ASKCTYPE reports default type");

    ctype = GPCT_MOUSE;
    req->io_Command = GPD_SETCTYPE;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &ctype;
    req->io_Length = sizeof(ctype);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && req->io_Actual == sizeof(ctype))
        test_ok("GPD_SETCTYPE accepts mouse type");
    else
        test_fail_msg("GPD_SETCTYPE accepts mouse type");

    ctype = 99;
    req->io_Command = GPD_SETCTYPE;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &ctype;
    req->io_Length = sizeof(ctype);
    DoIO((struct IORequest *)req);
    if (req->io_Error == GPDERR_SETCTYPE)
        test_ok("GPD_SETCTYPE rejects invalid type");
    else
        test_fail_msg("GPD_SETCTYPE rejects invalid type");

    ctype = 0;
    req->io_Command = GPD_ASKCTYPE;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &ctype;
    req->io_Length = sizeof(ctype);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && ctype == GPCT_MOUSE)
        test_ok("GPD_ASKCTYPE reports updated type");
    else
        test_fail_msg("GPD_ASKCTYPE reports updated type");

    trigger.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
    trigger.gpt_Timeout = 12;
    trigger.gpt_XDelta = 3;
    trigger.gpt_YDelta = 4;
    req->io_Command = GPD_SETTRIGGER;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &trigger;
    req->io_Length = sizeof(trigger);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 && req->io_Actual == sizeof(trigger))
        test_ok("GPD_SETTRIGGER stores trigger");
    else
        test_fail_msg("GPD_SETTRIGGER stores trigger");

    trigger_out.gpt_Keys = 0;
    trigger_out.gpt_Timeout = 0;
    trigger_out.gpt_XDelta = 0;
    trigger_out.gpt_YDelta = 0;
    req->io_Command = GPD_ASKTRIGGER;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &trigger_out;
    req->io_Length = sizeof(trigger_out);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        trigger_out.gpt_Keys == trigger.gpt_Keys &&
        trigger_out.gpt_Timeout == trigger.gpt_Timeout &&
        trigger_out.gpt_XDelta == trigger.gpt_XDelta &&
        trigger_out.gpt_YDelta == trigger.gpt_YDelta)
        test_ok("GPD_ASKTRIGGER returns stored trigger");
    else
        test_fail_msg("GPD_ASKTRIGGER returns stored trigger");

    req->io_Command = NSCMD_DEVICEQUERY;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &query;
    req->io_Length = sizeof(query);
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0 &&
        req->io_Actual == sizeof(query) &&
        query.nsdqr_DeviceType == NSDEVTYPE_GAMEPORT &&
        query.nsdqr_SupportedCommands != NULL)
        test_ok("NSCMD_DEVICEQUERY reports gameport capabilities");
    else
        test_fail_msg("NSCMD_DEVICEQUERY reports gameport capabilities");

    req->io_Command = CMD_CLEAR;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_CLEAR succeeds");
    else
        test_fail_msg("CMD_CLEAR succeeds");

    req->io_Command = GPD_READEVENT;
    req->io_Flags = IOF_QUICK;
    req->io_Data = &event;
    req->io_Length = sizeof(struct InputEvent);
    SendIO((struct IORequest *)req);
    if (CheckIO((struct IORequest *)req) == NULL)
        test_ok("GPD_READEVENT stays pending without events");
    else
        test_fail_msg("GPD_READEVENT stays pending without events");

    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    if (req->io_Error == IOERR_ABORTED)
        test_ok("AbortIO aborts pending GPD_READEVENT");
    else
        test_fail_msg("AbortIO aborts pending GPD_READEVENT");

    req->io_Command = 0x7fff;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == IOERR_NOCMD)
        test_ok("Unknown command returns IOERR_NOCMD");
    else
        test_fail_msg("Unknown command returns IOERR_NOCMD");

    CloseDevice((struct IORequest *)req2);
    test_ok("CloseDevice unit 1");

    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice unit 0");

    DeleteIORequest((struct IORequest *)bad_req);
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
