/*
 * Probe (Phase 237): registers a command sees at entry, started through
 * RunCommand() and through the shell (System()).  Old compilers rely on
 * them: Lattice 3.03 startup computes its stack base from the saved D2
 * (sp - 4(sp) after movem.l d1-d6/a0-a6,-(sp)).
 *
 * The command is built here: "movem.l d0-d7/a0-a7,regs; move.l 4(sp),regs+64;
 * moveq #0,d0; rts", written to T: as a one-hunk executable for System().
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

static ULONG regs[17];
static const char args[] = "abc def\n";

static void build(UWORD *code)
{
    code[0] = 0x48f9;                   /* movem.l d0-d7/a0-a7,regs */
    code[1] = 0xffff;
    *(ULONG *)&code[2] = (ULONG)regs;
    code[4] = 0x23ef;                   /* move.l 4(sp),regs+64 */
    code[5] = 0x0004;
    *(ULONG *)&code[6] = (ULONG)&regs[16];
    code[8] = 0x7000;                   /* moveq #0,d0 */
    code[9] = 0x4e75;                   /* rts */
}

static void show(const char *how, LONG stack, const char *argp, LONG arglen)
{
    static const char *names[16] = { "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                                     "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7" };
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);
    int i;
    P_SECTION(how);
    for (i = 0; i < 16; i++) {
        ULONG v = regs[i];
        probe_s(names[i]);
        probe_s(" = ");
        if (v == 0)
            probe_s("0");
        else if (stack && v == (ULONG)stack)
            probe_s("stack size");
        else if (arglen >= 0 && v == (ULONG)arglen)
            probe_s("arg length");
        else if (argp && v == (ULONG)argp)
            probe_s("args (caller's buffer)");
        else if (v == (ULONG)SysBase)
            probe_s("SysBase");
        else if (v == (ULONG)DOSBase)
            probe_s("DOSBase");
        else if (v == (ULONG)me)
            probe_s("this process");
        else if (cli && v == (ULONG)cli)
            probe_s("CLI struct");
        else if (v == (ULONG)me->pr_CLI)
            probe_s("pr_CLI (BPTR)");
        else if (i != 15 && v + 64 > regs[15] && v < regs[15] + 64)
            probe_s("near sp");
        else if (v < 256)
            probe_dec((LONG)v);
        else
            probe_s("other");
        probe_ch('\n');
    }
    probe_s("4(sp) = ");
    if (stack && regs[16] == (ULONG)stack)
        probe_s("stack size\n");
    else if (!stack)
        probe_s("(not compared)\n");
    else {
        probe_dec((LONG)regs[16]);
        probe_ch('\n');
    }
    P_BOOL("d2 == 4(sp)", regs[2] == regs[16]);
}

int main(void)
{
    ULONG *mem = AllocMem(32, MEMF_PUBLIC | MEMF_CLEAR);
    BPTR seg, fh;
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);
    ULONG hunk[4 + 1 + 2 + 5 + 1];
    LONG i;

    if (!mem)
        return 20;
    /* in-memory seglist for RunCommand: size, next, code */
    mem[0] = 32;
    mem[1] = 0;
    build((UWORD *)&mem[2]);
    CacheClearU();
    seg = MKBADDR(&mem[1]);

    for (i = 0; i < 17; i++)
        regs[i] = 0xdeadbeef;
    RunCommand(seg, 8000, (STRPTR)args, 8);
    show("RunCommand(seg, 8000, \"abc def\\n\", 8)", 8000, args, 8);

    /* the same code as a file, started through the shell */
    hunk[0] = 0x3f3; hunk[1] = 0; hunk[2] = 1; hunk[3] = 0; hunk[4] = 0;
    hunk[5] = 5;                        /* hunk size table: 5 longs */
    hunk[6] = 0x3e9; hunk[7] = 5;
    build((UWORD *)&hunk[8]);
    /* code is 20 bytes = 5 longs: hunk[8..12] */
    fh = Open((STRPTR)"T:probe-entryregs", MODE_NEWFILE);
    if (!fh)
        return 20;
    Write(fh, hunk, 13 * 4);
    {
        ULONG end = 0x3f2;
        Write(fh, &end, 4);
    }
    Close(fh);
    for (i = 0; i < 17; i++)
        regs[i] = 0xdeadbeef;
    SystemTags((STRPTR)"T:probe-entryregs abc def", TAG_END);
    /* the stack size of a System() command is the shell's default - compare
       d2 with 4(sp) only (the two systems' shells differ in that default) */
    (void)cli;
    show("System(\"T:probe-entryregs abc def\")", 0, NULL, 8);
    DeleteFile((STRPTR)"T:probe-entryregs");
    FreeMem(mem, 32);
    return 0;
}
