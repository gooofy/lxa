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

struct LoaderChildMessage {
    struct Message msg;
    ULONG magic;
};

/* InternalLoadSeg() function array (dos.doc):
 *   ReadFunc(readhandle, buffer, length)(d1/d2/d3)
 *   AllocFunc(size, flags)(d0/d1)
 *   FreeFunc(memory, size)(a1/d0) */
static LONG read_calls, alloc_calls, free_calls;
static LONG alloc_bytes, free_bytes;

static LONG ReadFunc(register BPTR fh __asm("d1"), register APTR buf __asm("d2"), register LONG len __asm("d3"))
{
    read_calls++;
    return Read(fh, buf, len);
}

static APTR AllocFunc(register ULONG size __asm("d0"), register ULONG flags __asm("d1"))
{
    alloc_calls++;
    alloc_bytes += size;
    return AllocMem(size, flags);
}

static void FreeFunc(register APTR mem __asm("a1"), register ULONG size __asm("d0"))
{
    free_calls++;
    free_bytes += size;
    FreeMem(mem, size);
}

static LONG seg_hunks(BPTR seg)
{
    LONG n = 0;

    while (seg) {
        n++;
        seg = *(BPTR *)BADDR(seg);
    }
    return n;
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
    LONG rc;
    LONG funcs[3];

    print("DOS LoadSeg/RunCommand Test\n");
    print("===========================\n\n");

    print("Test 1: NewLoadSeg returns a runnable hunk seglist\n");
    seg = NewLoadSeg((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild", NULL);
    if (seg) {
        /* each hunk: size longword, link BPTR, data */
        if (((ULONG *)BADDR(seg))[-1] > 8)
            test_pass("NewLoadSeg hunk carries its allocation size");
        else
            test_fail("NewLoadSeg", "Hunk size longword missing");
        rc = RunCommand(seg, 8192, (CONST_STRPTR)"RETURN=7\n", 9);
        if (rc == 7)
            test_pass("NewLoadSeg seglist runs");
        else
            test_fail("NewLoadSeg seglist runs", "Unexpected return code");
        UnLoadSeg(seg);
    } else {
        test_fail("NewLoadSeg", "Could not load LoaderChild");
    }

    print("\nTest 2: InternalLoadSeg uses the caller's function array\n");
    funcs[0] = (LONG)ReadFunc;
    funcs[1] = (LONG)AllocFunc;
    funcs[2] = (LONG)FreeFunc;
    fh = Open((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild", MODE_OLDFILE);
    if (fh) {
        seg = InternalLoadSeg(fh, 0, funcs, NULL);
        if (seg) {
            LONG hunks = seg_hunks(seg);
            print("  PROBE hunks="); print_num(hunks); print(" allocs="); print_num(alloc_calls);
            print(" frees="); print_num(free_calls); print(" abytes="); print_num(alloc_bytes); print(" fbytes="); print_num(free_bytes);
            print(" s0="); print_num(((ULONG *)BADDR(seg))[-1]); print("\n");

            if (read_calls > 0)
                test_pass("ReadFunc used");
            else
                test_fail("InternalLoadSeg", "ReadFunc not called");
            if (alloc_calls == hunks)
                test_pass("One AllocFunc call per hunk");
            else
                test_fail("InternalLoadSeg", "AllocFunc calls do not match the hunk count");
            if (((ULONG *)BADDR(seg))[-1] > 8)
                test_pass("Hunk carries its allocation size");
            else
                test_fail("InternalLoadSeg", "Hunk size longword missing");
            rc = RunCommand(seg, 8192, (CONST_STRPTR)"RETURN=9\n", 9);
            if (rc == 9)
                test_pass("InternalLoadSeg seglist runs");
            else
                test_fail("InternalLoadSeg seglist runs", "Unexpected return code");
            InternalUnLoadSeg(seg, (void (*)())FreeFunc);
            print("  PROBE after unload frees="); print_num(free_calls); print(" fbytes="); print_num(free_bytes); print("\n");
            if (free_calls == alloc_calls && free_bytes == alloc_bytes)
                test_pass("InternalUnLoadSeg frees every hunk via FreeFunc");
            else
                test_fail("InternalUnLoadSeg", "FreeFunc calls/sizes do not match the allocations");
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

    print("\nTest 4: CreateNewProc launches child process with message port\n");
    seg = LoadSeg((CONST_STRPTR)"SYS:Tests/Dos/LoaderChild");
    if (seg) {
        struct MsgPort *parent_port = CreateMsgPort();
        if (parent_port) {
            struct Process *child;
            struct MsgPort *proc_port;
            struct LoaderChildMessage *msg;

            parent_port->mp_Node.ln_Name = (char *)TEST_PORT_NAME;
            AddPort(parent_port);

            /*
             * Use CreateNewProc with NP_Cli so the child is a CLI process.
             * LoaderChild uses libnix C startup which checks pr_CLI:
             * if pr_CLI==0, libnix calls WaitPort(&pr_MsgPort) expecting
             * a WBStartup message (Workbench launch convention).
             * NP_Cli=TRUE ensures pr_CLI!=0, so libnix takes the CLI path.
             */
            {
                struct TagItem procTags[] = {
                    { NP_Seglist,     (ULONG)seg },
                    { NP_Name,        (ULONG)"LoaderChild" },
                    { NP_Priority,    0 },
                    { NP_StackSize,   8192 },
                    { NP_FreeSeglist, FALSE },
                    { NP_Cli,         TRUE },
                    { TAG_DONE,       0 }
                };
                child = CreateNewProc(procTags);
            }
            proc_port = child ? &child->pr_MsgPort : NULL;
            if (proc_port) {
                WaitPort(parent_port);
                msg = (struct LoaderChildMessage *)GetMsg(parent_port);
                if (msg && msg->magic == TEST_MESSAGE_MAGIC && msg->msg.mn_ReplyPort == proc_port)
                    test_pass("CreateNewProc child sends message via port");
                else
                    test_fail("CreateNewProc child sends message via port", "Did not receive expected child message");
                cleanup_message(msg);
            } else {
                test_fail("CreateNewProc", "CreateNewProc returned NULL");
            }

            RemPort(parent_port);
            DeleteMsgPort(parent_port);
        } else {
            test_fail("CreateNewProc", "Could not create parent message port");
        }

        Delay(2);
        UnLoadSeg(seg);
    } else {
        test_fail("CreateNewProc", "Could not load LoaderChild");
    }

    print("\n=== Test Summary ===\n");
    print("Passed: ");
    print_num(tests_passed);
    print("\nFailed: ");
    print_num(tests_failed);
    print("\n");

    return tests_failed ? 10 : 0;
}
