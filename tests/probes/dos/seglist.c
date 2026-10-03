/*
 * Probe (Phase 224): pr_SegList of a CLI process and of processes created
 * with CreateNewProc(NP_Seglist), CreateNewProc(NP_Entry) and CreateProc().
 *
 * Detaching programs (Directory Opus 4, "cback" startup code) find their
 * own segment in the child at pr_SegList[3] and unlink it from there.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static struct Task *parent;
static BPTR child_segs;          /* pr_SegList as seen by the child */
static BPTR child_array[8];
static LONG child_count;

static void child(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    LONG i;
    child_segs = me->pr_SegList;
    child_count = -1;
    if (child_segs) {
        BPTR *a = (BPTR *)BADDR(child_segs);
        child_count = (LONG)a[0];
        for (i = 0; i < 5; i++)
            child_array[i] = a[i];
    }
    Signal(parent, SIGBREAKF_CTRL_F);
}

static void show(const char *what, BPTR segs, BPTR *a, LONG count, BPTR mine, BPTR *ref)
{
    LONG i;
    probe_s(what);
    if (!segs) {
        probe_s(": pr_SegList = 0\n");
        return;
    }
    probe_s(": count = ");
    probe_dec(count);
    for (i = 1; i <= 4; i++) {          /* [3] lies beyond the count on 3.1 */
        probe_s(" [");
        probe_dec(i);
        probe_s("]=");
        if (!a[i])
            probe_s("0");
        else if (mine && a[i] == mine)
            probe_s("seglist");
        else if ((LONG)a[i] < 0 && (LONG)a[i] > -16)
            probe_dec((LONG)a[i]);
        else if (ref && a[i] == ref[i])
            probe_s("same");
        else
            probe_s("other");
    }
    probe_ch('\n');
}

static BPTR make_seg(void)
{
    /* a one-hunk seglist: size, next BPTR, then "jmp child" */
    ULONG *m = AllocMem(16, MEMF_PUBLIC | MEMF_CLEAR);
    UWORD *code;
    if (!m)
        return 0;
    m[0] = 16;
    m[1] = 0;
    code = (UWORD *)&m[2];
    code[0] = 0x4ef9;
    *(ULONG *)&code[1] = (ULONG)child;
    CacheClearU();
    return MKBADDR(&m[1]);
}

static void run(const char *what, int how, BPTR *parent_array)
{
    BPTR seg = how == 1 ? 0 : make_seg();
    struct Process *p = NULL;
    child_segs = 0;
    child_count = 0;
    SetSignal(0, SIGBREAKF_CTRL_F);
    if (how == 0)
        p = CreateNewProcTags(NP_Seglist, seg, NP_FreeSeglist, FALSE, NP_Name, (ULONG)"probe-child",
                              NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END);
    else if (how == 1)
        p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"probe-child",
                              NP_Output, 0, NP_Input, 0, NP_CloseOutput, FALSE, NP_CloseInput, FALSE, TAG_END);
    else {
        struct MsgPort *port = CreateProc((STRPTR)"probe-child", 0, seg, 4096);
        p = port ? (struct Process *)((UBYTE *)port - sizeof(struct Task)) : NULL;
    }
    if (!p) {
        probe_s(what);
        probe_s(": create failed\n");
        return;
    }
    Wait(SIGBREAKF_CTRL_F);
    Delay(5);                   /* let the child finish exiting */
    show(what, child_segs, child_array, child_count, seg, parent_array);
}

int main(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR *a = me->pr_SegList ? (BPTR *)BADDR(me->pr_SegList) : NULL;
    BPTR module = 0;
    parent = &me->pr_Task;
    if (me->pr_CLI)
        module = ((struct CommandLineInterface *)BADDR(me->pr_CLI))->cli_Module;

    P_SECTION("CLI process");
    P_BOOL("cli_Module set", module != 0);
    show("cli", me->pr_SegList, a, a ? (LONG)a[0] : 0, module, NULL);

    P_SECTION("children");
    run("CreateNewProc NP_Seglist", 0, a);
    run("CreateNewProc NP_Entry", 1, a);
    run("CreateProc", 2, a);
    return 0;
}
