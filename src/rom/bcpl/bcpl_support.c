/*
    Copyright (C) 2010-2014, The AROS Development Team. All rights reserved.

    Desc: BCPL support

    Ported to lxa (Phase 239) from AROS arch/m68k-all/dos/bcpl_support.c,
    bcpl_patches.c (the system global vector set-up), bcpl_readargs.c,
    bcpl_putpkt.c and callglobvec.c.  Licence: AROS Public License 1.1,
    see LICENSE in this directory.

    lxa changes:
    - the system segments of lxa's pr_SegList ([1] and [2], see
      U_isSystemSegment()) stand for AROS' -1/-2 markers: installing one
      fills a global vector with the system entries;
    - the routines are called from bcpl.S with stack arguments (amigaos
      gcc ABI) instead of AROS_UFH register macros;
    - helpers for the entries that called AROS internals: sendPacket
      (dopacket), NoReqLoadSeg, openWindow, settime;
    - the entries AROS left as debug-print dummies are visible stubs
      (LXA_UNIMPLEMENTED);
    - BCPL_CreateProcBCPL grows lxa's fixed-size segment array when the
      caller's has more entries.
*/
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>

#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <dos/rdargs.h>
#include <clib/dos_protos.h>
#include <inline/dos.h>

#include <devices/timer.h>
#include <intuition/intuition.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

#include <stddef.h>

#include "../util.h"
#include "bcpl.h"
#include "bcpl_support.h"

extern struct ExecBase      *SysBase;
extern struct DosLibrary    *DOSBase;
extern struct IntuitionBase *IntuitionBase;

/* The offsets bcpl.S uses */
_Static_assert(offsetof(struct ExecBase, ThisTask) == OFS_ThisTask, "ThisTask");
_Static_assert(offsetof(struct Process, pr_MsgPort) == OFS_pr_MsgPort, "pr_MsgPort");
_Static_assert(offsetof(struct Process, pr_SegList) == OFS_pr_SegList, "pr_SegList");
_Static_assert(offsetof(struct Process, pr_Result2) == OFS_pr_Result2, "pr_Result2");
_Static_assert(offsetof(struct Process, pr_CurrentDir) == OFS_pr_CurrentDir, "pr_CurrentDir");
_Static_assert(offsetof(struct Process, pr_ConsoleTask) == OFS_pr_ConsoleTask, "pr_ConsoleTask");
_Static_assert(offsetof(struct Process, pr_FileSystemTask) == OFS_pr_FileSystemTask, "pr_FileSystemTask");
_Static_assert(offsetof(struct Process, pr_ReturnAddr) == OFS_pr_ReturnAddr, "pr_ReturnAddr");
_Static_assert(offsetof(struct DosLibrary, dl_Root) == OFS_dl_Root, "dl_Root");
_Static_assert(offsetof(struct DosLibrary, dl_IntuitionBase) == OFS_dl_IntuitionBase, "dl_IntuitionBase");
_Static_assert(offsetof(struct MsgPort, mp_SigTask) == OFS_mp_SigTask, "mp_SigTask");
_Static_assert(offsetof(struct Node, ln_Name) == OFS_ln_Name, "ln_Name");
_Static_assert(offsetof(struct DosPacket, dp_Type) == OFS_dp_Type, "dp_Type");
_Static_assert(offsetof(struct DosPacket, dp_Res1) == OFS_dp_Res1, "dp_Res1");
_Static_assert(offsetof(struct IORequest, io_Command) == 28, "io_Command");
_Static_assert(offsetof(struct timerequest, tr_time) == 32, "tr_time");

/* Externs */
#define BCPL(id, name)  extern void BCPL_##name(void);
#include "bcpl.inc"
#undef BCPL

#define BCPL_SlotCount  (BCPL_GlobVec_PosSize>>2)

/* Default Global Vector */
#define BCPL(id, name) \
        [(BCPL_GlobVec_NegSize + id)>>2] = (ULONG)BCPL_##name,

static const ULONG BCPL_GlobVec[(BCPL_GlobVec_NegSize + BCPL_GlobVec_PosSize) >> 2] = {
#include "bcpl.inc"
};
#undef BCPL

#define BCPL_ENTRY(proc)        (((APTR *)(proc)->pr_GlobVec)[1])

#define BSTR_ADDR(s)    ((STRPTR)BADDR(s) + 1)
#define BSTR_LEN(s)     (*(UBYTE *)BADDR(s))

/* Fill a global vector with the system entries (AROS: segment -1) */
static void bcpl_install_system(ULONG *globvec)
{
    ULONG slots = globvec[0];
    ULONG i;

    if (slots > BCPL_SlotCount)
        slots = BCPL_SlotCount;

    /* Copy over the negative entries from the default global vector */
    CopyMem((APTR)&BCPL_GlobVec[0], &globvec[-(BCPL_GlobVec_NegSize>>2)], BCPL_GlobVec_NegSize);

    for (i = 2; i < slots; i++) {
        ULONG gv = BCPL_GlobVec[(BCPL_GlobVec_NegSize>>2) + i];
        if (gv == 0)
            continue;

        globvec[i] = gv;
    }

    if ((GV_DOSBase >> 2) < globvec[0])
        globvec[GV_DOSBase >> 2] = (ULONG)DOSBase;
}

/*
 * Set up the process's initial global vector
 */
ULONG BCPL_InstallSeg(BPTR seg, ULONG *globvec)
{
    ULONG *segment;
    ULONG *table;

    if (seg == 0)
        return DOSTRUE;

    if (seg == (BPTR)-1 || U_isSystemSegment(seg)) {
        bcpl_install_system(globvec);
        return DOSTRUE;
    }

    if (seg == (BPTR)-2)
        return DOSTRUE;

    while (seg) {
        segment = BADDR(seg);
        seg = (BPTR)segment[0]; /* next segment */

        /* segment[-1] is the hunk's allocation size, segment[1] the
           length in longs of a BCPL section (an instruction otherwise) */
        if ((segment[-1] < segment[1]))
            continue;

        table = &segment[segment[1]];

        for (; table[-1] != 0; table = &table[-2])
            globvec[table[-2]] = (ULONG)((APTR)&segment[1] + table[-1]);
    }

    return DOSTRUE;
}

/* The system global vector: dl_GV, every process's pr_GlobVec */
BOOL lxa_bcpl_init(struct DosLibrary *dosbase)
{
    UBYTE *mem;
    ULONG *globvec;

    mem = AllocMem(sizeof(BCPL_GlobVec), MEMF_PUBLIC | MEMF_CLEAR);
    if (mem == NULL)
        return FALSE;

    globvec = (ULONG *)(mem + BCPL_GlobVec_NegSize);
    globvec[0] = BCPL_SlotCount;
    bcpl_install_system(globvec);
    globvec[GV_DOSBase >> 2] = (ULONG)dosbase;

    dosbase->dl_GV = (APTR)globvec;
    dosbase->dl_A5 = (LONG)BCPL_jsr;
    dosbase->dl_A6 = (LONG)BCPL_rts;

    return TRUE;
}

/* Create the global vector for a process
 */
static BOOL BCPL_AllocGlobVec(struct Process *me)
{
    UBYTE *mem;
    ULONG *globvec;
    ULONG i, n;
    BPTR *seglist = BADDR(me->pr_SegList);

    mem = AllocMem(sizeof(BCPL_GlobVec), MEMF_ANY | MEMF_CLEAR);
    if (mem == NULL)
        return FALSE;

    globvec = (ULONG *)(mem + BCPL_GlobVec_NegSize);
    globvec[0] = BCPL_SlotCount;

    /* Install the segments into the Global Vector: lxa's array has the
       process's own segment at [3] even though [0] says 2 (as on 3.1) */
    n = seglist ? seglist[0] : 0;
    if (seglist && n < 3)
        n = 3;
    for (i = 1; i <= n; i++)
        BCPL_InstallSeg(seglist[i], globvec);

    me->pr_GlobVec = globvec;

    return TRUE;
}

static void BCPL_FreeGlobVec(struct Process *me)
{
    UBYTE *globvec = me->pr_GlobVec;

    FreeMem(globvec - BCPL_GlobVec_NegSize, sizeof(BCPL_GlobVec));

    me->pr_GlobVec = DOSBase->dl_GV;
}

/* Under AOS, BCPL handlers expect the OS to build
 * their GlobalVector, and to receive a pointer to their
 * startup packet in D1.
 *
 * Both filesystem handlers and CLI shells use this routine.
 */
static ULONG BCPL_RunHandler(void)
{
    struct DosPacket *dp;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR oldReturnAddr;
    ULONG ret;

    WaitPort(&me->pr_MsgPort);
    dp = (struct DosPacket *)(GetMsg(&me->pr_MsgPort)->mn_Node.ln_Name);

    if (!BCPL_AllocGlobVec(me)) {
        if (dp != NULL)
            ReplyPkt(dp, DOSFALSE, ERROR_NO_FREE_STORE);
        return ERROR_NO_FREE_STORE;
    }

    oldReturnAddr = me->pr_ReturnAddr;
    ret = lxa_bcpl_call(BCPL_ENTRY(me), me->pr_GlobVec, me->pr_Task.tc_SPLower,
                        &me->pr_ReturnAddr, (LONG)MKBADDR(dp), 0, 0, 0);
    me->pr_ReturnAddr = oldReturnAddr;

    BCPL_FreeGlobVec(me);

    return ret;
}

/* Create the necessary process wrappings for a BCPL
 * segment. Only needed by Workbench's C:Run, C:NewCLI,
 * C:NewShell, and a few other applications.
 */
struct MsgPort *BCPL_CreateProcBCPL(CONST_STRPTR name, BPTR *segarray, ULONG stacksize, LONG pri)
{
    struct Process *proc, *me = (struct Process *)FindTask(NULL);

    proc = CreateNewProcTags(
            NP_Name, (ULONG)name,
            NP_Entry, (ULONG)BCPL_RunHandler,
            NP_Input, 0,
            NP_Output, 0,
            NP_Error, 0,
            NP_CloseInput, FALSE,
            NP_CloseOutput, FALSE,
            NP_CloseError, FALSE,
            NP_StackSize, stacksize,
            NP_Priority, pri,
            NP_WindowPtr, (ULONG)me->pr_WindowPtr,
            NP_CurrentDir, 0,
            NP_Cli, TRUE,
            NP_HomeDir, 0,
            TAG_END);

    /* Fix up the segarray before the first packet gets
     * to it (BCPL_RunHandler waits for it).
     */
    if (proc) {
        BPTR *procsegs = BADDR(proc->pr_SegList);
        ULONG n = segarray[0], i;

        if (n > 4) {
            /* lxa's own array has room for [0]..[4] */
            BPTR *grown = AllocVec(sizeof(BPTR) * (n + 1), MEMF_PUBLIC | MEMF_CLEAR);
            if (grown) {
                grown[0] = n;
                grown[1] = procsegs[1];
                grown[2] = procsegs[2];
                procsegs = grown;
                proc->pr_SegList = MKBADDR(grown);
            } else {
                n = 4;
            }
        }
        for (i = 3; i <= n; i++)
            procsegs[i] = segarray[i];
        if (n > procsegs[0])
            procsegs[0] = n;
    }

    return proc ? &proc->pr_MsgPort : NULL;
}

/*
 * BCPL rdargs (global vector 0x138)
 *
 * Parses the commandline according to the options template and fills
 * the vector: switches and numbers as values, strings as BSTRs stored
 * behind the argument slots.  Returns the number of longs used, 0 on
 * failure.
 */
ULONG lxa_bcpl_readargs(BSTR btemplate, BPTR bvec, ULONG upb)
{
    STRPTR template;
    ULONG *vec = BADDR(bvec);
    struct RDArgs *rd;
    BSTR bstr;
    CONST_STRPTR cp;
    int arg, args, seen_key, seen_enough, svec;
    int tlen = BSTR_LEN(btemplate);
#define RA_FLAG(x)      ((x)&0x1f)

    template = AllocVec(tlen + 1, MEMF_ANY);
    if (template == NULL) {
        SetIoErr(ERROR_NO_FREE_STORE);
        return 0;
    }

    CopyMem(BSTR_ADDR(btemplate), template, tlen);
    template[tlen] = 0;

    /* Count args in the template */
    for (args = 1, cp = template; *cp; cp++) {
        if (*cp == ',')
            args++;
    }

    /* BCPL readargs appears to want the vector to be zeroed */
    memset(vec, 0, upb * sizeof(ULONG));

    rd = ReadArgs(template, (LONG *)vec, NULL);
    if (rd == NULL) {
        FreeVec(template);
        return 0;
    }

    svec = args;
    seen_enough = 0;
    for (arg = 0, seen_key = 0, cp = template; ; cp++) {
        int len, left;

        if (*cp == ',' || (cp - template) >= tlen) {
            if (!seen_enough && vec[arg] != 0) {
                /* Ok, it's probably a string. Convert it to BCPL */
                len = strlen((const char *)vec[arg]);
                left = (upb - svec) * sizeof(ULONG);
                if (len + 1 > left) {
                    SetIoErr(ERROR_NO_FREE_STORE);
                    FreeArgs(rd);
                    FreeVec(template);
                    return 0;
                }

                bstr = MKBADDR(&vec[svec]);
                CopyMem((APTR)vec[arg], BSTR_ADDR(bstr), len);
                *(UBYTE *)BADDR(bstr) = len;
                vec[arg] = bstr;
                svec += ((len + 1 + 3) & ~3) / sizeof(ULONG);
            }
            if (*cp != ',')
                break;
            arg++;
            seen_key = 0;
            continue;
        }

        if (!seen_key && *cp == '/') {
            seen_key = 1;
            seen_enough = 0;
            continue;
        }

        if (!seen_key)
            continue;

        if (seen_enough)
            continue;

        if (RA_FLAG(*cp) == RA_FLAG('N')) {
            seen_enough=1;
            continue;
        }
        if (RA_FLAG(*cp) == RA_FLAG('S')) {
            seen_enough=1;
            continue;
        }
        if (RA_FLAG(*cp) == RA_FLAG('T')) {
            seen_enough=1;
            continue;
        }
    }

    FreeVec(template);
    FreeArgs(rd);

    SetIoErr(0);
    return svec;
#undef RA_FLAG
}

/* BCPL putPkt (global vector 0xa8): send a packet whose dp_Link points
 * at an allocated, but empty, message */
ULONG lxa_bcpl_putpkt(BPTR bdospacket)
{
    struct DosPacket *dp = BADDR(bdospacket);

    dp->dp_Link->mn_Node.ln_Name = (char *)dp;
    dp->dp_Link->mn_Length = sizeof(*dp->dp_Link);

    PutMsg(dp->dp_Port, dp->dp_Link);
    return DOSTRUE;
}

/* BCPL sendPacket (global vector 0xc0): send a packet and wait for the
 * reply; args points at Arg1..Arg7 in the BCPL frame */
LONG lxa_bcpl_sendpacket(struct MsgPort *port, LONG type, const LONG *args)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct StandardPacket *sp;
    LONG res1;

    sp = AllocVec(sizeof(*sp), MEMF_PUBLIC | MEMF_CLEAR);
    if (!sp) {
        SetIoErr(ERROR_NO_FREE_STORE);
        return DOSFALSE;
    }

    sp->sp_Msg.mn_Node.ln_Name = (char *)&sp->sp_Pkt;
    sp->sp_Pkt.dp_Link = &sp->sp_Msg;
    sp->sp_Pkt.dp_Type = type;
    sp->sp_Pkt.dp_Arg1 = args[0];
    sp->sp_Pkt.dp_Arg2 = args[1];
    sp->sp_Pkt.dp_Arg3 = args[2];
    sp->sp_Pkt.dp_Arg4 = args[3];
    sp->sp_Pkt.dp_Arg5 = args[4];
    sp->sp_Pkt.dp_Arg6 = args[5];
    sp->sp_Pkt.dp_Arg7 = args[6];

    SendPkt(&sp->sp_Pkt, port, &me->pr_MsgPort);
    WaitPkt();

    res1 = sp->sp_Pkt.dp_Res1;
    SetIoErr(sp->sp_Pkt.dp_Res2);
    FreeVec(sp);
    return res1;
}

/* BCPL NoReqLoadSeg (global vector -0x54): LoadSeg() of a BSTR name
 * without "please insert volume" requesters */
BPTR lxa_bcpl_noreqloadseg(BSTR name)
{
    BPTR ret = 0;

    if (name != 0) {
        struct Process *me = (struct Process *)FindTask(NULL);
        int len = BSTR_LEN(name);
        char buff[256];
        APTR oldWindowPtr;

        CopyMem(BSTR_ADDR(name), buff, len);
        buff[len] = 0;

        oldWindowPtr = me->pr_WindowPtr;
        me->pr_WindowPtr = (APTR)-1;
        ret = LoadSeg((STRPTR)buff);
        me->pr_WindowPtr = oldWindowPtr;
    } else {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
    }

    return ret;
}

/* BCPL openWindow (global vector 0xc8) */
struct Window *lxa_bcpl_openwindow(LONG left, LONG top, LONG width, LONG height, STRPTR title)
{
    struct TagItem tags[] = {
        { WA_Left, (ULONG)left },
        { WA_Top, (ULONG)top },
        { WA_Width, (ULONG)width },
        { WA_Height, (ULONG)height },
        { WA_Title, (ULONG)title },
        { WA_PubScreen, 0 },
        { WA_AutoAdjust, TRUE },
        { TAG_END, 0 }
    };

    if (!IntuitionBase)
        return NULL;
    return OpenWindowTagList(NULL, tags);
}

/* BCPL settime (global vector 0x200): set the system time from a
 * DateStamp */
LONG lxa_bcpl_settime(BPTR bds)
{
    struct DateStamp *ds = BADDR(bds);
    struct MsgPort *port;
    struct timerequest *tr;
    LONG ok = DOSFALSE;

    port = CreateMsgPort();
    if (!port)
        return DOSFALSE;
    tr = (struct timerequest *)CreateIORequest(port, sizeof(*tr));
    if (tr) {
        if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_VBLANK, (struct IORequest *)tr, 0) == 0) {
            tr->tr_node.io_Command = TR_SETSYSTIME;
            tr->tr_time.tv_secs = ds->ds_Days * 86400 + ds->ds_Minute * 60
                                  + ds->ds_Tick / TICKS_PER_SECOND;
            tr->tr_time.tv_micro = (ds->ds_Tick % TICKS_PER_SECOND) * (1000000 / TICKS_PER_SECOND);
            ok = DoIO((struct IORequest *)tr) == 0 ? DOSTRUE : DOSFALSE;
            CloseDevice((struct IORequest *)tr);
        }
        DeleteIORequest((struct IORequest *)tr);
    }
    DeleteMsgPort(port);
    return ok;
}

/*
 * Global vector entries lxa does not implement (AROS: debug dummies).
 * Each returns 0 (DOSFALSE).
 */
ULONG lxa_bcpl_stub_sysRequest(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL sysRequest", "stub: BCPL system requester (GV -0x84)");
    return 0;
}

ULONG lxa_bcpl_stub_makeGVarea(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL makeGVarea", "stub: BCPL makeGVarea (GV 0x34)");
    return 0;
}

ULONG lxa_bcpl_stub_longjump(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL longjump", "stub: BCPL longjump (GV 0x50)");
    return 0;
}

ULONG lxa_bcpl_stub_createco(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL createco", "stub: BCPL coroutines (GV 0x5c)");
    return 0;
}

ULONG lxa_bcpl_stub_deleteco(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL deleteco", "stub: BCPL coroutines (GV 0x60)");
    return 0;
}

ULONG lxa_bcpl_stub_callco(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL callco", "stub: BCPL coroutines (GV 0x64)");
    return 0;
}

ULONG lxa_bcpl_stub_cowait(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL cowait", "stub: BCPL coroutines (GV 0x68)");
    return 0;
}

ULONG lxa_bcpl_stub_resumeco(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL resumeco", "stub: BCPL coroutines (GV 0x6c)");
    return 0;
}

ULONG lxa_bcpl_stub_holdTask(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL holdTask", "stub: BCPL holdTask (GV 0xb8)");
    return 0;
}

ULONG lxa_bcpl_stub_systemRequest(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL systemRequest", "stub: BCPL system requester (GV 0xd0)");
    return 0;
}

ULONG lxa_bcpl_stub_tidyup(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL tidyup", "stub: BCPL tidyup (GV 0x150)");
    return 0;
}

ULONG lxa_bcpl_stub_openDevInfo(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL openDevInfo", "stub: BCPL handler start-up (GV 0x1c0)");
    return 0;
}

ULONG lxa_bcpl_stub_compareTime(void)
{
    LXA_UNIMPLEMENTED("dos", "BCPL compareTime", "stub: BCPL compareTime (GV 0x1f8)");
    return 0;
}
