/*
 * Probe (Phase 238): what a process created with the old-style
 * CreateProc() inherits from its creator - I/O streams, current
 * directory, CLI.  Detaching programs (SIGMAth, BeckerText II) start
 * their real work this way; lxa gave the child the creator's
 * Input()/Output(), which the child then closed under the creator.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static volatile LONG g_done, g_in, g_out, g_dir, g_cli, g_home, g_cis, g_cos;

static void child(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    g_in = Input() ? 1 : 0;
    g_out = Output() ? 1 : 0;
    g_dir = me->pr_CurrentDir ? 1 : 0;
    g_home = me->pr_HomeDir ? 1 : 0;
    g_cli = me->pr_CLI ? 1 : 0;
    g_cis = me->pr_CIS ? 1 : 0;
    g_cos = me->pr_COS ? 1 : 0;
    Forbid();
    g_done = 1;
}

int main(void)
{
    /* a one-hunk seglist: [next BPTR = 0] [jmp child] */
    ULONG *seg = AllocMem(16, MEMF_PUBLIC | MEMF_CLEAR);
    struct MsgPort *port;
    int i;

    if (!seg)
        return 20;
    seg[0] = 16;                          /* size, as LoadSeg stores it */
    seg[1] = 0;                           /* next segment */
    ((UWORD *)&seg[2])[0] = 0x4ef9;       /* jmp child.l */
    *(ULONG *)((UBYTE *)&seg[2] + 2) = (ULONG)child;
    CacheClearU();

    P_SECTION("CreateProc() child");
    P_BOOL("creator has Input()", Input() != 0);
    P_BOOL("creator has Output()", Output() != 0);
    port = CreateProc((CONST_STRPTR)"probe child", 0, MKBADDR(&seg[1]), 4096);
    P_NULL("CreateProc", port);
    for (i = 0; i < 100 && !g_done; i++)
        Delay(1);
    P_LONG("child ran", g_done);
    P_BOOL("child Input()", g_in);
    P_BOOL("child Output()", g_out);
    P_BOOL("child pr_CIS", g_cis);
    P_BOOL("child pr_COS", g_cos);
    P_BOOL("child pr_CurrentDir", g_dir);
    P_BOOL("child pr_HomeDir", g_home);
    P_BOOL("child pr_CLI", g_cli);
    Delay(5);
    /* the seglist stays allocated (CreateProc does not free it) */
    return 0;
}
