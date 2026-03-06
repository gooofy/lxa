/*
 * Test for trackdisk.device - basic device open/close and commands
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/trackdisk.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

#define NEWLIST(l) ((l)->lh_Head = (struct Node *)&(l)->lh_Tail, \
                    (l)->lh_Tail = NULL, \
                    (l)->lh_TailPred = (struct Node *)&(l)->lh_Head)

static BPTR out;
static LONG test_pass = 0;
static LONG test_fail = 0;

static void print(const char *s)
{
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG num)
{
    char buf[16];
    int i = 0;
    BOOL neg = FALSE;

    if (num < 0) {
        neg = TRUE;
        num = -num;
    }
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        while (j > 0) buf[i++] = temp[--j];
    }
    if (neg) Write(out, "-", 1);
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
    LONG error;

    out = Output();
    print("Testing trackdisk.device\n\n");

    /* Create message port */
    port = (struct MsgPort *)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    if (!port) {
        print("FAIL: Cannot allocate message port\n");
        return 20;
    }
    port->mp_Node.ln_Type = NT_MSGPORT;
    port->mp_Flags = PA_IGNORE;
    port->mp_SigTask = FindTask(NULL);
    NEWLIST(&port->mp_MsgList);

    /* Create IO request */
    req = (struct IOStdReq *)AllocMem(sizeof(struct IOStdReq), MEMF_PUBLIC | MEMF_CLEAR);
    if (!req) {
        print("FAIL: Cannot allocate IO request\n");
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }
    req->io_Message.mn_ReplyPort = port;
    req->io_Message.mn_Length = sizeof(struct IOStdReq);

    /* Test: Open unit 0 */
    error = OpenDevice("trackdisk.device", 0, (struct IORequest *)req, 0);
    if (error == 0)
        test_ok("OpenDevice unit 0");
    else {
        test_fail_msg("OpenDevice unit 0");
        print("    error="); print_num(error); print("\n");
        FreeMem(req, sizeof(struct IOStdReq));
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }

    /* Test: TD_CHANGENUM */
    req->io_Command = TD_CHANGENUM;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("TD_CHANGENUM");
    else
        test_fail_msg("TD_CHANGENUM");

    /* Test: TD_CHANGESTATE */
    req->io_Command = TD_CHANGESTATE;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("TD_CHANGESTATE");
    else
        test_fail_msg("TD_CHANGESTATE");

    /* Test: TD_PROTSTATUS */
    req->io_Command = TD_PROTSTATUS;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("TD_PROTSTATUS");
    else
        test_fail_msg("TD_PROTSTATUS");

    /* Test: TD_GETDRIVETYPE */
    req->io_Command = TD_GETDRIVETYPE;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("TD_GETDRIVETYPE");
    else
        test_fail_msg("TD_GETDRIVETYPE");

    /* Test: TD_GETNUMTRACKS */
    req->io_Command = TD_GETNUMTRACKS;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0) {
        test_ok("TD_GETNUMTRACKS");
        if (req->io_Actual == 160) /* 80 cylinders * 2 heads */
            test_ok("TD_GETNUMTRACKS value=160");
        else {
            test_fail_msg("TD_GETNUMTRACKS value");
            print("    got: "); print_num(req->io_Actual); print(", expected: 160\n");
        }
    } else
        test_fail_msg("TD_GETNUMTRACKS");

    /* Test: TD_MOTOR off */
    req->io_Command = TD_MOTOR;
    req->io_Flags = IOF_QUICK;
    req->io_Length = 0;  /* motor off */
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("TD_MOTOR off");
    else
        test_fail_msg("TD_MOTOR off");

    /* Test: CMD_RESET */
    req->io_Command = CMD_RESET;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_RESET");
    else
        test_fail_msg("CMD_RESET");

    /* Test: CMD_FLUSH */
    req->io_Command = CMD_FLUSH;
    req->io_Flags = IOF_QUICK;
    DoIO((struct IORequest *)req);
    if (req->io_Error == 0)
        test_ok("CMD_FLUSH");
    else
        test_fail_msg("CMD_FLUSH");

    /* Test: Close device */
    CloseDevice((struct IORequest *)req);
    test_ok("CloseDevice");

    /* Test: Open unit 1 */
    error = OpenDevice("trackdisk.device", 1, (struct IORequest *)req, 0);
    if (error == 0) {
        test_ok("OpenDevice unit 1");
        CloseDevice((struct IORequest *)req);
        test_ok("CloseDevice unit 1");
    } else {
        test_fail_msg("OpenDevice unit 1");
    }

    /* Test: Invalid unit (4) should fail */
    error = OpenDevice("trackdisk.device", 4, (struct IORequest *)req, 0);
    if (error != 0)
        test_ok("OpenDevice unit 4 rejected (expected)");
    else {
        test_fail_msg("OpenDevice unit 4 should have failed");
        CloseDevice((struct IORequest *)req);
    }

    /* Cleanup */
    FreeMem(req, sizeof(struct IOStdReq));
    FreeMem(port, sizeof(struct MsgPort));
    test_ok("Cleanup complete");

    print("\n");
    if (test_fail == 0) {
        print("PASS: All ");
        print_num(test_pass);
        print(" tests passed!\n");
        return 0;
    } else {
        print("FAIL: ");
        print_num(test_fail);
        print(" of ");
        print_num(test_pass + test_fail);
        print(" tests failed\n");
        return 20;
    }
}
