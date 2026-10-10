/*
 * bcpl_cli.c - BCPL processes: createProcBCPL and the console segment
 *
 * KS 1.3 C:Run and C:NewCLI create their CLI process with the BCPL global
 * createProcBCPL on the RootNode's console segment and then send it a
 * start-up packet.  Such a process builds a global vector from its segment
 * array and calls global 1 with the packet.  lxa's console segment
 * (bcpl_entry.S) runs lxa_bcpl_cli_main(): it sets the CLI up from the
 * packet and runs lxa's shell.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include "../util.h"
#include "lxa_bcpl.h"

extern struct ExecBase   *SysBase;
extern struct DosLibrary *DOSBase;

CONST_STRPTR lxa_dos_shell_name(void);

/* lxa's segment array behind the Process has room for [0]..[4] */
#define OWN_SEGARRAY_TOP    4

/* ------------------------------------------------------ the global vector */

/* A private global vector for the process, built from its segment array:
 * [1], [2] are the system segments, [3] onwards the program.  lxa's array
 * has the process's own segment at [3] even while [0] says 2 (as on 3.1). */
static ULONG *build_gv(struct Process *me)
{
    BPTR *segs = BADDR(me->pr_SegList);
    ULONG n = segs ? segs[0] : 0, i, slots = LXA_BCPL_POS_SLOTS;
    ULONG *mem, *gv;

    if (segs && n < 3)
        n = 3;
    for (i = 1; i <= n; i++) {
        ULONG h = lxa_bcpl_highest_global(segs[i]);
        if (h + 1 > slots)
            slots = h + 1;
    }

    mem = AllocVec(LXA_BCPL_NEG_BYTES + slots * 4, MEMF_PUBLIC | MEMF_CLEAR);
    if (!mem)
        return NULL;
    gv = mem + LXA_BCPL_NEG_BYTES / 4;
    gv[0] = slots;
    for (i = 1; i <= n; i++)
        lxa_bcpl_install(segs[i], gv);
    return gv;
}

static void free_gv(ULONG *gv)
{
    FreeVec(gv - LXA_BCPL_NEG_BYTES / 4);
}

/* -------------------------------------------------- the process start-up */

/* NP_Entry of a createProcBCPL process: wait for the start-up packet, then
 * run global 1 with it */
static LONG bcpl_process(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct Message *msg;
    struct DosPacket *dp;
    APTR old_return;
    ULONG *gv;
    LONG rc;

    WaitPort(&me->pr_MsgPort);
    msg = GetMsg(&me->pr_MsgPort);
    dp = (struct DosPacket *)msg->mn_Node.ln_Name;

    gv = build_gv(me);
    if (!gv || !gv[1]) {
        if (gv)
            free_gv(gv);
        if (dp)
            ReplyPkt(dp, DOSFALSE, ERROR_NO_FREE_STORE);
        return ERROR_NO_FREE_STORE;
    }

    me->pr_GlobVec = gv;
    old_return = me->pr_ReturnAddr;
    rc = lxa_bcpl_enter((APTR)gv[1], gv, (LONG *)me->pr_Task.tc_SPLower, &me->pr_ReturnAddr,
                        (LONG)MKBADDR(dp), 0, 0, 0);
    me->pr_ReturnAddr = old_return;
    me->pr_GlobVec = DOSBase->dl_GV;
    free_gv(gv);
    return rc;
}

/* createProcBCPL: a process on a segment array, started by a packet */
struct MsgPort *lxa_bcpl_create_proc(CONST_STRPTR name, BPTR *segarray,
                                     ULONG stackbytes, LONG pri)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct Process *proc;
    BPTR *segs;
    ULONG n, i;

    proc = CreateNewProcTags(NP_Name, (ULONG)name,
                             NP_Entry, (ULONG)bcpl_process,
                             NP_StackSize, stackbytes,
                             NP_Priority, pri,
                             NP_Input, 0, NP_CloseInput, FALSE,
                             NP_Output, 0, NP_CloseOutput, FALSE,
                             NP_Error, 0, NP_CloseError, FALSE,
                             NP_CurrentDir, 0,
                             NP_HomeDir, 0,
                             NP_WindowPtr, (ULONG)me->pr_WindowPtr,
                             NP_Cli, TRUE,
                             TAG_END);
    if (!proc)
        return NULL;

    /* the caller's segments from [3] on; the new process only runs once
     * the caller sends it the start-up packet */
    segs = BADDR(proc->pr_SegList);
    n = segarray[0];
    if (n > OWN_SEGARRAY_TOP) {
        BPTR *more = AllocVec((n + 1) * sizeof(BPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (more) {
            more[1] = segs[1];
            more[2] = segs[2];
            segs = more;
            proc->pr_SegList = MKBADDR(more);
        } else {
            n = OWN_SEGARRAY_TOP;
        }
    }
    for (i = 3; i <= n; i++)
        segs[i] = segarray[i];
    if (n > segs[0])
        segs[0] = n;

    return &proc->pr_MsgPort;
}

/* ------------------------------------------------------ the console segment */

/* KS 1.3: the packet's dp_Type is the caller's own BCPL CliInit routine,
 * which returns the routine that replies the packet */
static void bcpl_cliinit(struct Process *me, BPTR pkt, LONG type, LONG *freestack)
{
    APTR old_return = me->pr_ReturnAddr;
    LONG next;

    next = lxa_bcpl_enter((APTR)type, me->pr_GlobVec, freestack, &me->pr_ReturnAddr,
                          (LONG)pkt, 0, 0, 0);
    if (next > 0)
        lxa_bcpl_enter((APTR)next, me->pr_GlobVec, freestack, &me->pr_ReturnAddr,
                       IoErr(), 0, 0, 0);
    me->pr_ReturnAddr = old_return;
}

/* the CLI number the 1.3 Run entered into rn_TaskArray is free again */
static void release_cli_number(struct Process *me)
{
    struct RootNode *root = (struct RootNode *)DOSBase->dl_Root;
    ULONG *tasks = root ? BADDR(root->rn_TaskArray) : NULL;
    ULONG i;

    if (!tasks)
        return;
    Forbid();
    for (i = 1; i <= tasks[0]; i++)
        if (tasks[i] == (ULONG)&me->pr_MsgPort)
            tasks[i] = 0;
    Permit();
}

static LONG run_shell(struct CommandLineInterface *cli)
{
    CONST_STRPTR shell = lxa_dos_shell_name();
    BPTR seg;
    LONG rc, stack;

    if (!shell || !(seg = LoadSeg(shell)))
        return RETURN_FAIL;
    stack = (cli && cli->cli_DefaultStack > 0) ? cli->cli_DefaultStack * 4 : 4096;
    rc = RunCommand(seg, stack, (STRPTR)"\n", 1);
    UnLoadSeg(seg);
    return rc;
}

/*
 * Global 1 of the console segment.  dp_Type 0/1/-1/-2 are the 2.0+ style
 * start-ups: CliInitNewcli() (dp_Res1 set) or CliInitRun(); -2 is a
 * synchronous System(), replied when the shell ends.
 */
LONG lxa_bcpl_cli_main(BPTR pkt, LONG *freestack)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DosPacket *dp = BADDR(pkt);
    struct CommandLineInterface *cli;
    LONG flags = 0, rc;
    BOOL reply_last = FALSE;

    if (!dp)
        return RETURN_FAIL;

    if (dp->dp_Type > 1) {
        bcpl_cliinit(me, pkt, dp->dp_Type, freestack);
    } else {
        flags = dp->dp_Res1 ? CliInitNewcli(dp) : CliInitRun(dp);
        if (!flags && IoErr() == (LONG)me)
            return RETURN_FAIL;             /* CliInit replied the packet */
        reply_last = flags && dp->dp_Type == -2;
        if (!reply_last)
            ReplyPkt(dp, dp->dp_Res1, dp->dp_Res2);
    }

    cli = Cli();
    if (cli) {
        if (!me->pr_CIS)
            me->pr_CIS = cli->cli_CurrentInput ? cli->cli_CurrentInput : cli->cli_StandardInput;
        if (!me->pr_COS)
            me->pr_COS = cli->cli_StandardOutput;
    }

    rc = run_shell(cli);

    /* an output stream CliInit opened is closed here */
    if (cli && (flags & 0x80000000UL) && (flags & 1) && cli->cli_StandardOutput) {
        Close(cli->cli_StandardOutput);
        cli->cli_StandardOutput = 0;
        me->pr_COS = 0;
    }
    release_cli_number(me);

    if (reply_last)
        ReplyPkt(dp, rc, IoErr());
    return rc;
}
