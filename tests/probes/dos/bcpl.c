/*
 * Probe (Phase 239): a BCPL command.  AmigaOS 3.1 still runs BCPL
 * programs (1.x C: commands): they are entered with a2 = the global
 * vector, a5/a6 = the BCPL call/return routines and a1 = the BCPL stack,
 * and call system globals with "movea.l n(a2),a4; moveq #k,d0; jsr (a5)".
 *
 * The command below uses writes, newline, writef, writen, wrch, toupper,
 * the multiply entry (jsr 16(a5)), divide, mod and stop (the Exit
 * global, return code 3).  It runs through RunCommand() and, written to
 * T: as a one-hunk executable, through the shell (System()).
 *
 * Source (GNU as, m68k), assembled into the words below:
 *     lea s_hello(pc),a3; move.l a3,d1; lsr.l #2,d1
 *     moveq #12,d0; movea.l 0x124(a2),a4; jsr (a5)       writes(s_hello)
 *     moveq #12,d0; movea.l 0x110(a2),a4; jsr (a5)       newline()
 *     lea s_fmt(pc),a3; ...; moveq #2,d2; moveq #3,d3; moveq #5,d4
 *     moveq #12,d0; movea.l 0x128(a2),a4; jsr (a5)       writef(s_fmt, 2, 3, 5)
 *     moveq #6,d1; moveq #7,d2; jsr 16(a5)               6 * 7
 *     writen(d1); newline()
 *     divide(100, 7) (0x10); writen; wrch(' ') (0xe0); mod(100, 7) (0x14);
 *     writen; newline(); writen(-42); wrch(toupper('q')) (0x12c); newline()
 *     moveq #3,d1; moveq #12,d0; movea.l 8(a2),a4; jsr (a5)   stop(3)
 *     moveq #0,d0; rts
 * s_hello: BSTR "BCPL writes", s_fmt: BSTR "%N + %N = %N\n"
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define CODE_LONGS 52

static const ULONG code[CODE_LONGS] = {
    0x47fa00b2, 0x220be489, 0x700c286a, 0x01244e95, 0x700c286a, 0x01104e95,
    0x47fa00a6, 0x220be489, 0x74027603, 0x7805700c, 0x286a0128, 0x4e957206,
    0x74074ead, 0x0010700c, 0x286a0118, 0x4e95700c, 0x286a0110, 0x4e957264,
    0x7407700c, 0x286a0010, 0x4e95700c, 0x286a0118, 0x4e957220, 0x700c286a,
    0x00e04e95, 0x72647407, 0x700c286a, 0x00144e95, 0x700c286a, 0x01184e95,
    0x700c286a, 0x01104e95, 0x72d6700c, 0x286a0118, 0x4e957271, 0x700c286a,
    0x012c4e95, 0x700c286a, 0x00e04e95, 0x700c286a, 0x01104e95, 0x7203700c,
    0x286a0008, 0x4e957000, 0x4e750000, 0x0b424350, 0x4c207772, 0x69746573,
    0x0d254e20, 0x2b20254e, 0x203d2025, 0x4e0a0000,
};

int main(void)
{
    ULONG *mem = AllocMem((CODE_LONGS + 2) * 4, MEMF_PUBLIC | MEMF_CLEAR);
    ULONG hunk[7 + CODE_LONGS + 1];
    BPTR fh;
    LONG rc;

    if (!mem)
        return 20;

    /* in-memory seglist for RunCommand: size, next, code */
    mem[0] = (CODE_LONGS + 2) * 4;
    mem[1] = 0;
    CopyMem((APTR)code, &mem[2], sizeof(code));
    CacheClearU();

    P_SECTION("RunCommand");
    rc = RunCommand(MKBADDR(&mem[1]), 8000, (STRPTR)"\n", 1);
    Flush(Output());
    P_LONG("rc", rc);

    /* the same code as a file, started through the shell */
    hunk[0] = 0x3f3; hunk[1] = 0; hunk[2] = 1; hunk[3] = 0; hunk[4] = 0;
    hunk[5] = CODE_LONGS;
    hunk[6] = 0x3e9;
    fh = Open((STRPTR)"T:probe-bcpl", MODE_NEWFILE);
    if (!fh)
        return 20;
    Write(fh, hunk, 7 * 4);
    {
        ULONG n = CODE_LONGS;
        Write(fh, &n, 4);
    }
    Write(fh, (APTR)code, sizeof(code));
    {
        ULONG end = 0x3f2;
        Write(fh, &end, 4);
    }
    Close(fh);

    P_SECTION("System");
    rc = SystemTags((STRPTR)"T:probe-bcpl", TAG_END);
    Flush(Output());
    P_LONG("rc", rc);
    DeleteFile((STRPTR)"T:probe-bcpl");

    FreeMem(mem, (CODE_LONGS + 2) * 4);
    return 0;
}
