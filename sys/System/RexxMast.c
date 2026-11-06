/*
 * RexxMast - the ARexx resident process (Phase 250)
 *
 * Activates the public "REXX" port (rl_RexxPort in the rexxsyslib.library
 * base) and an "AREXX" port for asynchronous requests.  Every RXCOMM or
 * RXFUNC packet starts an interpreter process (SYS:System/ARexx: Regina
 * with lxa's ARexx glue), which runs the program and replies the packet
 * itself.  The resource packets (RXADDFH, RXADDLIB, RXREMLIB, RXADDCON,
 * RXREMCON, RXTCOPN, RXTCCLS) and RXCLOSE are handled here.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <rexx/errors.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>

struct RxsLib *RexxSysBase = NULL;

#include "../Rexxc/rexx_common.h"

static const char version[] = "$VER: RexxMast 36.21 (01.11.2026)";

static struct MsgPort *arexx_port;   /* "AREXX": asynchronous requests */
static struct MsgPort *own_port;     /* replies of our async copies */
static LONG pending_copies;

static void reply_rc(struct RexxMsg *rm, LONG rc, LONG rc2)
{
    rm->rm_Result1 = rc;
    rm->rm_Result2 = rc2;
    ReplyMsg((struct Message *)rm);
}

/* the input/output streams a program inherits: the packet's, else the
 * client process's, else NIL: */
static BPTR client_stream(struct RexxMsg *rm, BOOL out)
{
    struct Task *t;
    struct Process *pr;
    BPTR fh = (BPTR)(out ? rm->rm_Stdout : rm->rm_Stdin);

    if (fh)
        return fh;
    if (!(rm->rm_Action & RXFF_NOIO) && rm->rm_Node.mn_ReplyPort) {
        t = (struct Task *)rm->rm_Node.mn_ReplyPort->mp_SigTask;
        if (t && t->tc_Node.ln_Type == NT_PROCESS) {
            pr = (struct Process *)t;
            fh = out ? pr->pr_COS : pr->pr_CIS;
            if (fh)
                return fh;
        }
    }
    return (BPTR)RexxSysBase->rl_NIL;
}

static BPTR client_dir(struct RexxMsg *rm)
{
    struct Task *t = NULL;

    if (rm->rm_Node.mn_ReplyPort)
        t = (struct Task *)rm->rm_Node.mn_ReplyPort->mp_SigTask;
    if (t && t->tc_Node.ln_Type == NT_PROCESS)
        return DupLock(((struct Process *)t)->pr_CurrentDir);
    return DupLock(0);
}

/* ---- the interpreter pool ------------------------------------------------
 * Loading SYS:System/ARexx (Regina, ~500 KB) for every program took longer
 * than the program itself.  RexxMast keeps the seglists it has loaded: a
 * seglist whose process has ended runs the next program, after its data
 * and BSS hunks got back the contents they had right after LoadSeg(). */
struct interp {
    struct interp *next;
    BPTR seg;
    struct Process *proc;       /* running on it; NULL: idle */
    UBYTE *snap;                /* hunks 2..n as loaded */
    ULONG snap_size;
};

static struct interp *pool;

#define HUNK_DATA(h) ((UBYTE *)BADDR(h) + 4)
#define HUNK_SIZE(h) (((ULONG *)BADDR(h))[-1] - 8)

/* copy the hunks after the code hunk to (save) or from the snapshot */
static void interp_copy(struct interp *ip, BOOL save)
{
    BPTR h = *(BPTR *)BADDR(ip->seg);
    UBYTE *p = ip->snap;

    for (; h; h = *(BPTR *)BADDR(h)) {
        ULONG n = HUNK_SIZE(h);
        if (save)
            CopyMem(HUNK_DATA(h), p, n);
        else
            CopyMem(p, HUNK_DATA(h), n);
        p += n;
    }
}

static struct interp *interp_new(void)
{
    struct interp *ip = AllocVec(sizeof(*ip), MEMF_PUBLIC | MEMF_CLEAR);
    BPTR h;

    if (!ip)
        return NULL;
    ip->seg = LoadSeg((STRPTR)LXA_REXX_INTERPRETER);
    if (ip->seg) {
        for (h = *(BPTR *)BADDR(ip->seg); h; h = *(BPTR *)BADDR(h))
            ip->snap_size += HUNK_SIZE(h);
        ip->snap = AllocVec(ip->snap_size ? ip->snap_size : 1, MEMF_PUBLIC);
    }
    if (!ip->seg || !ip->snap) {
        if (ip->seg)
            UnLoadSeg(ip->seg);
        FreeVec(ip);
        return NULL;
    }
    interp_copy(ip, TRUE);
    return ip;
}

static void interp_free(struct interp *ip)
{
    UnLoadSeg(ip->seg);
    FreeVec(ip->snap);
    FreeVec(ip);
}

/* NP_ExitCode of an interpreter process: its seglist is idle again */
static void interp_exit(void)
{
    struct Task *me = FindTask(NULL);
    struct interp *ip;

    Forbid();
    for (ip = pool; ip; ip = ip->next)
        if (ip->proc == (struct Process *)me)
            ip->proc = NULL;
    Permit();
}

/* an idle seglist, ready to run (NULL: out of memory) */
static struct interp *interp_get(void)
{
    struct interp *ip;

    Forbid();
    for (ip = pool; ip; ip = ip->next)
        if (!ip->proc) {
            ip->proc = (struct Process *)-1;    /* taken */
            break;
        }
    Permit();
    if (ip) {
        interp_copy(ip, FALSE);
        return ip;
    }
    ip = interp_new();
    if (ip) {
        ip->proc = (struct Process *)-1;
        Forbid();
        ip->next = pool;
        pool = ip;
        Permit();
    }
    return ip;
}

/* RexxMast ends: wait for the last interpreter process, free the pool */
static void interp_free_all(void)
{
    struct interp *ip;
    BOOL busy = TRUE;

    while (busy) {
        busy = FALSE;
        Forbid();
        for (ip = pool; ip; ip = ip->next)
            if (ip->proc)
                busy = TRUE;
        Permit();
        if (busy)
            Delay(1);
    }
    while ((ip = pool)) {
        pool = ip->next;
        interp_free(ip);
    }
}

/* start an interpreter process for a program packet; the packet goes to
 * the process in its tc_UserData (set before it can run): NP_Arguments
 * would go into the client's input buffer, which the process shares */
static BOOL launch(struct RexxMsg *rm)
{
    struct RxsLib *rl = RexxSysBase;
    struct Process *pr;
    struct interp *ip;
    BPTR dir;

    ip = interp_get();
    if (!ip)
        return FALSE;
    dir = client_dir(rm);

    Forbid();
    rl->rl_NumTask++;
    pr = CreateNewProcTags(NP_Seglist, (ULONG)ip->seg,
                           NP_FreeSeglist, FALSE,
                           NP_ExitCode, (ULONG)interp_exit,
                           NP_Name, (ULONG)rl->rl_TaskName,
                           NP_Priority, rl->rl_TaskPri,
                           NP_StackSize, 16384,
                           NP_Cli, TRUE,
                           NP_CommandName, (ULONG)"ARexx",
                           NP_Input, (ULONG)client_stream(rm, FALSE),
                           NP_Output, (ULONG)client_stream(rm, TRUE),
                           NP_CloseInput, FALSE,
                           NP_CloseOutput, FALSE,
                           NP_CurrentDir, (ULONG)dir,
                           TAG_DONE);
    if (pr) {
        pr->pr_Task.tc_UserData = (APTR)rm;
        ip->proc = pr;
    } else {
        rl->rl_NumTask--;
        ip->proc = NULL;
    }
    Permit();
    if (!pr) {
        UnLock(dir);
        return FALSE;
    }
    return TRUE;
}

/* an asynchronous request: reply at once, run a copy */
static void launch_async(struct RexxMsg *rm)
{
    struct RexxMsg *copy;
    LONG i;

    copy = CreateRexxMsg(own_port, (STRPTR)rm->rm_FileExt, (STRPTR)rm->rm_CommAddr);
    if (!copy) {
        reply_rc(rm, RC_FATAL, ERR10_003);
        return;
    }
    copy->rm_Action = rm->rm_Action & ~RXFF_RESULT;
    for (i = 0; i <= MAXRMARG; i++)
        if (rm->rm_Args[i])
            copy->rm_Args[i] = (STRPTR)CreateArgstring(rm->rm_Args[i], lxa_rexx_strlen((char *)rm->rm_Args[i]));
    /* the client may go away: the program inherits our streams */
    copy->rm_Action |= RXFF_NOIO;
    reply_rc(rm, 0, 0);
    if (launch(copy)) {
        pending_copies++;
    } else {
        ClearRexxMsg(copy, MAXRMARG + 1);
        DeleteRexxMsg(copy);
    }
}

static void handle(struct RexxMsg *rm, BOOL async, BOOL *closing)
{
    LONG code = rm->rm_Action & RXCODEMASK;
    LONG nargs = rm->rm_Action & RXARGMASK;
    char *a0 = (char *)rm->rm_Args[0];
    struct RxsLib *rl = RexxSysBase;

    switch (code) {
    case RXCOMM:
    case RXFUNC:
        if (*closing) {
            reply_rc(rm, RC_FATAL, ERR10_010);
        } else if (async) {
            launch_async(rm);
        } else if (!launch(rm)) {
            reply_rc(rm, RC_FATAL, ERR10_003);
        }
        return;

    case RXCLOSE:
        *closing = TRUE;
        reply_rc(rm, 0, 0);
        return;

    case RXADDFH:
        /* ARG0 host name, ARG1 priority */
        if (!a0)
            break;
        reply_rc(rm, lxa_rexx_addlib(a0, lxa_rexx_strlen(a0), lxa_rexx_arg_long(rm->rm_Args[1]),
                                     FALSE, -1, 0) ? 0 : RC_WARN, 0);
        return;

    case RXADDLIB:
        /* ARG0 name, ARG1 priority, ARG2 entry offset, ARG3 version; a
         * library without an offset is a function host */
        if (!a0)
            break;
        reply_rc(rm, lxa_rexx_addlib(a0, lxa_rexx_strlen(a0), lxa_rexx_arg_long(rm->rm_Args[1]),
                                     nargs >= 2 || rm->rm_Args[2] != NULL,
                                     lxa_rexx_arg_long(rm->rm_Args[2]),
                                     lxa_rexx_arg_long(rm->rm_Args[3])) ? 0 : RC_WARN, 0);
        return;

    case RXREMLIB:
        if (!a0)
            break;
        reply_rc(rm, lxa_rexx_remlib(a0, lxa_rexx_strlen(a0)) ? 0 : RC_WARN, 0);
        return;

    case RXADDCON:
        /* ARG0 name, ARG1 value, ARG2 the value's length */
        if (!a0 || !rm->rm_Args[1])
            break;
        reply_rc(rm, lxa_rexx_setclip(a0, lxa_rexx_strlen(a0), (char *)rm->rm_Args[1],
                                      (LONG)rm->rm_Args[2]) ? 0 : RC_ERROR, 0);
        return;

    case RXREMCON:
        if (!a0)
            break;
        reply_rc(rm, lxa_rexx_setclip(a0, lxa_rexx_strlen(a0), NULL, 0) ? 0 : RC_WARN, 0);
        return;

    case RXTCOPN:
        if (!rl->rl_TraceFH)
            rl->rl_TraceFH = (LONG)Open((STRPTR)"CON:0/0/640/100/ARexx Trace Console/AUTO", MODE_NEWFILE);
        reply_rc(rm, rl->rl_TraceFH ? 0 : RC_ERROR, 0);
        return;

    case RXTCCLS:
        if (rl->rl_TraceFH) {
            Close((BPTR)rl->rl_TraceFH);
            rl->rl_TraceFH = 0;
        }
        reply_rc(rm, 0, 0);
        return;
    }
    reply_rc(rm, RC_FATAL, ERR10_010);
}

int main(void)
{
    struct RxsLib *rl;
    struct MsgPort *rexx;
    struct RexxMsg *rm;
    BOOL closing = FALSE;
    BYTE sig;

    (void)version;
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase)
        return RETURN_FAIL;
    rl = RexxSysBase;

    Forbid();
    if (FindPort((STRPTR)"REXX")) {
        Permit();
        CloseLibrary((struct Library *)RexxSysBase);
        return RETURN_OK;           /* already active */
    }
    sig = AllocSignal(-1);
    if (sig < 0) {
        Permit();
        CloseLibrary((struct Library *)RexxSysBase);
        return RETURN_FAIL;
    }
    rexx = &rl->rl_RexxPort;
    rexx->mp_Node.ln_Name = (char *)"REXX";
    rexx->mp_Node.ln_Type = NT_MSGPORT;
    rexx->mp_Node.ln_Pri = 0;
    rexx->mp_Flags = PA_SIGNAL;
    rexx->mp_SigBit = (UBYTE)sig;
    rexx->mp_SigTask = FindTask(NULL);
    rexx->mp_MsgList.lh_Head = (struct Node *)&rexx->mp_MsgList.lh_Tail;
    rexx->mp_MsgList.lh_Tail = NULL;
    rexx->mp_MsgList.lh_TailPred = (struct Node *)&rexx->mp_MsgList.lh_Head;
    AddPort(rexx);
    Permit();

    own_port = CreateMsgPort();
    arexx_port = CreateMsgPort();
    if (arexx_port) {
        arexx_port->mp_Node.ln_Name = (char *)"AREXX";
        arexx_port->mp_Node.ln_Pri = 0;
        AddPort(arexx_port);
    }

    for (;;) {
        ULONG mask = 1UL << rexx->mp_SigBit;
        if (arexx_port)
            mask |= 1UL << arexx_port->mp_SigBit;
        if (own_port)
            mask |= 1UL << own_port->mp_SigBit;

        while ((rm = (struct RexxMsg *)GetMsg(rexx)))
            handle(rm, FALSE, &closing);
        while (arexx_port && (rm = (struct RexxMsg *)GetMsg(arexx_port)))
            handle(rm, own_port != NULL, &closing);
        while (own_port && (rm = (struct RexxMsg *)GetMsg(own_port))) {
            ClearRexxMsg(rm, MAXRMARG + 1);
            DeleteRexxMsg(rm);
            pending_copies--;
        }
        if (closing)
            break;
        Wait(mask | SIGBREAKF_CTRL_C);
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            closing = TRUE;
    }

    /* RXC: the ports go away at once, running programs finish */
    Forbid();
    RemPort(rexx);
    if (arexx_port)
        RemPort(arexx_port);
    Permit();
    while ((rm = (struct RexxMsg *)GetMsg(rexx)))
        reply_rc(rm, RC_FATAL, ERR10_010);
    while (arexx_port && (rm = (struct RexxMsg *)GetMsg(arexx_port)))
        reply_rc(rm, RC_FATAL, ERR10_010);
    while (rl->rl_NumTask > 0 || pending_copies > 0) {
        while (own_port && (rm = (struct RexxMsg *)GetMsg(own_port))) {
            ClearRexxMsg(rm, MAXRMARG + 1);
            DeleteRexxMsg(rm);
            pending_copies--;
        }
        Delay(2);
    }

    interp_free_all();

    Forbid();
    rexx->mp_Node.ln_Name = NULL;
    rexx->mp_Node.ln_Type = 0;
    rexx->mp_SigTask = NULL;
    rexx->mp_Flags = PA_IGNORE;
    Permit();
    FreeSignal(sig);
    if (arexx_port)
        DeleteMsgPort(arexx_port);
    if (own_port)
        DeleteMsgPort(own_port);
    if (rl->rl_TraceFH) {
        Close((BPTR)rl->rl_TraceFH);
        rl->rl_TraceFH = 0;
    }
    CloseLibrary((struct Library *)RexxSysBase);
    return RETURN_OK;
}
