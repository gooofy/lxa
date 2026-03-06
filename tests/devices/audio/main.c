/*
 * Test for audio.device - basic open/close
 */

#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <devices/audio.h>
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
    if (num == 0) {
        buf[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        BOOL neg = FALSE;
        if (num < 0) { neg = TRUE; num = -num; }
        while (num > 0) {
            temp[j++] = '0' + (num % 10);
            num /= 10;
        }
        if (neg) buf[i++] = '-';
        while (j > 0) buf[i++] = temp[--j];
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
    struct IOAudio *req;
    LONG error;
    UBYTE alloc_map[] = { 1, 8, 2, 4 };

    out = Output();
    print("Testing audio.device\n\n");

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
    req = (struct IOAudio *)AllocMem(sizeof(struct IOAudio), MEMF_PUBLIC | MEMF_CLEAR);
    if (!req) {
        print("FAIL: Cannot allocate IO request\n");
        FreeMem(port, sizeof(struct MsgPort));
        return 20;
    }
    req->ioa_Request.io_Message.mn_ReplyPort = port;
    req->ioa_Request.io_Message.mn_Length = sizeof(struct IOAudio);

    /* Set up allocation map for channel allocation */
    req->ioa_Data = alloc_map;
    req->ioa_Length = sizeof(alloc_map);

    /* Test: Open audio.device */
    error = OpenDevice("audio.device", 0, (struct IORequest *)req, 0);
    if (error == 0) {
        test_ok("OpenDevice audio.device");

        /* Verify io_Device is set */
        if (req->ioa_Request.io_Device != NULL)
            test_ok("io_Device is set");
        else
            test_fail_msg("io_Device is NULL");

        /* Close device */
        CloseDevice((struct IORequest *)req);
        test_ok("CloseDevice");

        /* After close, pointers should be cleared */
        if (req->ioa_Request.io_Device == NULL)
            test_ok("io_Device cleared after close");
        else
            test_fail_msg("io_Device not cleared");
    } else {
        test_fail_msg("OpenDevice audio.device");
        print("    error="); print_num(error); print("\n");
    }

    /* Test: Open/Close cycle - should be able to open again */
    error = OpenDevice("audio.device", 0, (struct IORequest *)req, 0);
    if (error == 0) {
        test_ok("Second OpenDevice succeeds");
        CloseDevice((struct IORequest *)req);
        test_ok("Second CloseDevice");
    } else {
        test_fail_msg("Second OpenDevice");
    }

    /* Cleanup */
    FreeMem(req, sizeof(struct IOAudio));
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
