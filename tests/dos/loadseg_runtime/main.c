#include <exec/types.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEST_PORT_NAME "LoadSegRuntime.Parent"
#define TEST_MESSAGE_MAGIC 0x4c535247UL
#define GSLI_DUMMY (TAG_USER + 4000)
#define GSLI_68KHUNK (GSLI_DUMMY + 5)

struct LoaderChildMessage {
    struct Message msg;
    ULONG magic;
};

static ULONG get_seg_list_info(BPTR seglist, const struct TagItem *taglist)
{
    register char *bn __asm("a6") = (char *)DOSBase;
    register BPTR segreg __asm("d0") = seglist;
    register const struct TagItem *tagreg __asm("a0") = taglist;
    ULONG result;

    __asm__ volatile (
        "jsr -1176(a6)"
        : "=d"(result)
        : "a"(bn), "d"(segreg), "a"(tagreg)
        : "d1", "a1", "cc", "memory");

    return result;
}

static int tests_passed = 0;
static int tests_failed = 0;

static void print(const char *s)
{
    Write(Output(), (CONST APTR)s, strlen(s));
}

static void print_num(LONG n)
{
    char buf[32];
    char *p = buf + sizeof(buf) - 1;
    ULONG value;

    *p = '\0';
    value = (n < 0) ? (ULONG)(-n) : (ULONG)n;

    do {
        *--p = '0' + (value % 10);
        value /= 10;
    } while (value);

    if (n < 0)
        *--p = '-';

    print(p);
}

static void test_pass(const char *name)
{
    print("  PASS: ");
    print(name);
    print("\n");
    tests_passed++;
}

static void test_fail(const char *name, const char *reason)
{
    print("  FAIL: ");
    print(name);
    print(" - ");
    print(reason);
    print("\n");
    tests_failed++;
}

static void cleanup_message(struct LoaderChildMessage *msg)
{
    if (msg)
        FreeMem(msg, sizeof(*msg));
}

int main(void)
{
    BPTR seg;
    BPTR fh;
    ULONG seginfo = 0;
    ULONG count;
    LONG rc;
    struct TagItem segtags[] = {
        { GSLI_68KHUNK, (ULONG)&seginfo },
        { TAG_DONE, 0 }
    };

    print("DOS LoadSeg/RunCommand Test\n");
    print("===========================\n\n");

    print("Test 1: NewLoadSeg returns a hunk seglist\n");
    seg = NewLoadSeg((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild", NULL);
    if (seg) {
        seginfo = 0;
        count = get_seg_list_info(seg, segtags);
        if (count == 1 && seginfo == (ULONG)seg)
            test_pass("NewLoadSeg + GetSegListInfo");
        else
            test_fail("NewLoadSeg + GetSegListInfo", "Seglist info mismatch");
        UnLoadSeg(seg);
    } else {
        test_fail("NewLoadSeg", "Could not load LoaderChild");
    }

    print("\nTest 2: InternalLoadSeg uses caller file handle\n");
    fh = Open((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild", MODE_OLDFILE);
    if (fh) {
        seg = InternalLoadSeg(fh, 0, NULL, NULL);
        if (seg) {
            seginfo = 0;
            count = get_seg_list_info(seg, segtags);
            if (count == 1 && seginfo == (ULONG)seg)
                test_pass("InternalLoadSeg + GetSegListInfo");
            else
                test_fail("InternalLoadSeg + GetSegListInfo", "Seglist info mismatch");
            InternalUnLoadSeg(seg, NULL);
        } else {
            test_fail("InternalLoadSeg", "Could not load LoaderChild");
        }
        Close(fh);
    } else {
        test_fail("InternalLoadSeg", "Could not open LoaderChild");
    }

    print("\nTest 3: RunCommand executes loaded program with arguments\n");
    seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild");
    if (seg) {
        rc = RunCommand(seg, 8192, (CONST_STRPTR)"RETURN=23\n", 10);
        if (rc == 23)
            test_pass("RunCommand returns child exit code");
        else
            test_fail("RunCommand returns child exit code", "Unexpected return code");
        UnLoadSeg(seg);
    } else {
        test_fail("RunCommand", "Could not load LoaderChild");
    }

    print("\nTest 4: CreateProc launches child process with message port\n");
    seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild");
    if (seg) {
        struct MsgPort *parent_port = CreateMsgPort();
        if (parent_port) {
            struct MsgPort *proc_port;
            struct LoaderChildMessage *msg;

            parent_port->mp_Node.ln_Name = (char *)TEST_PORT_NAME;
            AddPort(parent_port);

            proc_port = CreateProc((CONST_STRPTR)"LoaderChild", 0, seg, 8192);
            if (proc_port) {
                WaitPort(parent_port);
                msg = (struct LoaderChildMessage *)GetMsg(parent_port);
                if (msg && msg->magic == TEST_MESSAGE_MAGIC && msg->msg.mn_ReplyPort == proc_port)
                    test_pass("CreateProc returns child message port");
                else
                    test_fail("CreateProc returns child message port", "Did not receive expected child message");
                cleanup_message(msg);
            } else {
                test_fail("CreateProc", "CreateProc returned NULL");
            }

            RemPort(parent_port);
            DeleteMsgPort(parent_port);
        } else {
            test_fail("CreateProc", "Could not create parent message port");
        }

        Delay(2);
        UnLoadSeg(seg);
    } else {
        test_fail("CreateProc", "Could not load LoaderChild");
    }

    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 10 : 0;
}
