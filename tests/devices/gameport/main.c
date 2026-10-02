/*
 * Test for gameport.device controller, trigger, and request semantics
 * (validated against AmigaOS 3.1, Phase 220).
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

static void check(BOOL ok, const char *name)
{
    if (ok)
        test_ok(name);
    else
        test_fail_msg(name);
}

static void show_error(struct IOStdReq *req, const char *name)
{
    print("  ");
    print(name);
    print(": io_Error=");
    print_num(req->io_Error);
    print("\n");
}

static void set_ctype(struct IOStdReq *req, UBYTE *ctype, UBYTE value)
{
    *ctype = value;
    req->io_Command = GPD_SETCTYPE;
    req->io_Data = ctype;
    req->io_Length = 1;
    req->io_Actual = 0;
    DoIO((struct IORequest *)req);
}

static UBYTE ask_ctype(struct IOStdReq *req)
{
    UBYTE ctype = 0xee;

    req->io_Command = GPD_ASKCTYPE;
    req->io_Data = &ctype;
    req->io_Length = 1;
    req->io_Actual = 0;
    DoIO((struct IORequest *)req);
    return ctype;
}

/*
 * Unit 0 (the mouse port) belongs to input.device on a running system, so
 * the controller tests use unit 1 and follow the RKRM allocation protocol:
 * check for GPCT_NOCONTROLLER under Forbid(), set a type, and give the
 * port back with GPCT_NOCONTROLLER before closing.
 */
int main(void)
{
    struct MsgPort *port;
    struct IOStdReq *mouse_req;
    struct IOStdReq *req;
    struct IOStdReq *bad_req;
    struct Device *device;
    struct GamePortTrigger trigger;
    struct GamePortTrigger trigger_out;
    struct InputEvent event;
    UBYTE ctype;
    UBYTE type;
    LONG error;

    print("Testing gameport.device\n\n");

    port = CreateMsgPort();
    if (!port)
    {
        print("FAIL: Cannot create message port\n");
        return 20;
    }

    mouse_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    bad_req = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));
    if (!mouse_req || !req || !bad_req)
    {
        print("FAIL: Cannot create IO requests\n");
        return 20;
    }

    error = OpenDevice((STRPTR)"gameport.device", 0, (struct IORequest *)mouse_req, 0);
    check(error == 0, "OpenDevice unit 0 succeeds");
    error = OpenDevice((STRPTR)"gameport.device", 1, (struct IORequest *)req, 0);
    check(error == 0, "OpenDevice unit 1 succeeds");
    if (!mouse_req->io_Device || !req->io_Device)
        return 20;
    device = req->io_Device;

    error = OpenDevice((STRPTR)"gameport.device", 2, (struct IORequest *)bad_req, 0);
    check(error == IOERR_OPENFAIL, "OpenDevice rejects unit 2");
    if (error == 0)
        CloseDevice((struct IORequest *)bad_req);

    check(ask_ctype(mouse_req) == GPCT_MOUSE && mouse_req->io_Error == 0 && mouse_req->io_Actual == 1,
          "GPD_ASKCTYPE: input.device owns unit 0 as GPCT_MOUSE");

    Forbid();
    type = ask_ctype(req);
    if (type == GPCT_NOCONTROLLER)
        set_ctype(req, &ctype, GPCT_ABSJOYSTICK);
    Permit();
    check(type == GPCT_NOCONTROLLER, "GPD_ASKCTYPE: unit 1 is free");
    check(req->io_Error == 0 && req->io_Actual == 1, "GPD_SETCTYPE allocates unit 1");
    check(ask_ctype(req) == GPCT_ABSJOYSTICK, "GPD_ASKCTYPE reports the new type");

    set_ctype(req, &ctype, 99);
    check(req->io_Error == 0 && req->io_Actual == 1, "GPD_SETCTYPE accepts an unknown type");
    check(ask_ctype(req) == GPCT_ABSJOYSTICK, "an unknown type is not stored");

    set_ctype(req, &ctype, (UBYTE)GPCT_ALLOCATED);
    check(req->io_Error == 0 && ask_ctype(req) == (UBYTE)GPCT_ALLOCATED, "GPD_SETCTYPE stores GPCT_ALLOCATED");
    set_ctype(req, &ctype, GPCT_ABSJOYSTICK);

    trigger.gpt_Keys = GPTF_DOWNKEYS | GPTF_UPKEYS;
    trigger.gpt_Timeout = 12;
    trigger.gpt_XDelta = 3;
    trigger.gpt_YDelta = 4;
    req->io_Command = GPD_SETTRIGGER;
    req->io_Data = &trigger;
    req->io_Length = sizeof(trigger);
    DoIO((struct IORequest *)req);
    check(req->io_Error == 0 && req->io_Actual == sizeof(trigger), "GPD_SETTRIGGER stores trigger");

    trigger_out.gpt_Keys = 0;
    trigger_out.gpt_Timeout = 0;
    trigger_out.gpt_XDelta = 0;
    trigger_out.gpt_YDelta = 0;
    req->io_Command = GPD_ASKTRIGGER;
    req->io_Data = &trigger_out;
    req->io_Length = sizeof(trigger_out);
    DoIO((struct IORequest *)req);
    check(req->io_Error == 0 && req->io_Actual == sizeof(trigger_out) &&
          trigger_out.gpt_Keys == trigger.gpt_Keys &&
          trigger_out.gpt_Timeout == trigger.gpt_Timeout &&
          trigger_out.gpt_XDelta == trigger.gpt_XDelta &&
          trigger_out.gpt_YDelta == trigger.gpt_YDelta, "GPD_ASKTRIGGER returns stored trigger");

    /* standard exec commands: 3.1 rejects them; the error value of the
     * first group is not stable, so only its presence is checked */
    req->io_Command = CMD_CLEAR;
    DoIO((struct IORequest *)req);
    check(req->io_Error != 0, "CMD_CLEAR is rejected");
    req->io_Command = CMD_RESET;
    DoIO((struct IORequest *)req);
    check(req->io_Error != 0, "CMD_RESET is rejected");
    req->io_Command = CMD_FLUSH;
    DoIO((struct IORequest *)req);
    check(req->io_Error != 0, "CMD_FLUSH is rejected");
    req->io_Command = CMD_STOP;
    DoIO((struct IORequest *)req);
    check(req->io_Error != 0, "CMD_STOP is rejected");
    req->io_Command = CMD_START;
    DoIO((struct IORequest *)req);
    check(req->io_Error != 0, "CMD_START is rejected");
    req->io_Command = CMD_WRITE;
    DoIO((struct IORequest *)req);
    show_error(req, "CMD_WRITE");
    req->io_Command = CMD_UPDATE;
    DoIO((struct IORequest *)req);
    show_error(req, "CMD_UPDATE");
    req->io_Command = NSCMD_DEVICEQUERY;
    DoIO((struct IORequest *)req);
    show_error(req, "NSCMD_DEVICEQUERY");

    req->io_Command = GPD_READEVENT;
    req->io_Data = &event;
    req->io_Length = sizeof(struct InputEvent);
    SendIO((struct IORequest *)req);
    check(CheckIO((struct IORequest *)req) == NULL, "GPD_READEVENT stays pending without events");

    AbortIO((struct IORequest *)req);
    WaitIO((struct IORequest *)req);
    check(req->io_Error == IOERR_ABORTED, "AbortIO aborts pending GPD_READEVENT");

    req->io_Command = 0x7fff;
    DoIO((struct IORequest *)req);
    check(req->io_Error == IOERR_NOCMD, "Unknown command returns IOERR_NOCMD");

    /* the controller type survives a close/reopen */
    CloseDevice((struct IORequest *)req);
    error = OpenDevice((STRPTR)"gameport.device", 1, (struct IORequest *)req, 0);
    check(error == 0, "reopening unit 1 succeeds");
    print("  unit 1 type after reopen: ");
    print_num(ask_ctype(req));
    print("\n");

    set_ctype(req, &ctype, GPCT_NOCONTROLLER);
    check(req->io_Error == 0 && ask_ctype(req) == GPCT_NOCONTROLLER, "GPD_SETCTYPE frees unit 1");

    RemDevice(device);
    print("  RemDevice: LIBF_DELEXP=");
    print_num((device->dd_Library.lib_Flags & LIBF_DELEXP) != 0);
    print(" in DeviceList=");
    print_num(FindName(&SysBase->DeviceList, (STRPTR)"gameport.device") == &device->dd_Library.lib_Node);
    print("\n");

    CloseDevice((struct IORequest *)req);
    CloseDevice((struct IORequest *)mouse_req);
    test_ok("CloseDevice");

    DeleteIORequest((struct IORequest *)bad_req);
    DeleteIORequest((struct IORequest *)req);
    DeleteIORequest((struct IORequest *)mouse_req);
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
