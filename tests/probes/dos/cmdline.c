/*
 * Probe (Phase 222a): where a command finds its argument line.  The probe
 * runs a "command" (a function of its own, behind a hand-made segment)
 * with RunCommand() and an input file as Input(), and shows what
 * ReadArgs(), ReadItem(), FGetC(), Read() and GetArgStr() see inside it,
 * and what is left of the input afterwards.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <dos/dostags.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

#define IN "T:lxaprobe_cmdline"

static int mode;

static void show(const char *label, const char *s, LONG n)
{
    LONG i;
    probe_s(label);
    probe_s(" \"");
    for (i = 0; n < 0 ? s[i] != 0 : i < n; i++) {
        if (s[i] == '\n')
            probe_s("\\n");
        else if ((UBYTE)s[i] < 32)
            probe_ch('?');
        else
            probe_ch(s[i]);
    }
    probe_s("\"\n");
}

static void rest_of_line(const char *label)
{
    char buf[64];
    if (FGets(Input(), (STRPTR)buf, sizeof(buf)))
        show(label, buf, -1);
    else {
        probe_s(label);
        probe_s(" EOF\n");
    }
}

static void readargs(const char *tmpl)
{
    LONG a[4] = {0, 0, 0, 0};
    struct RDArgs *r = ReadArgs((STRPTR)tmpl, a, NULL);
    probe_s("  ReadArgs ");
    probe_s(tmpl);
    if (!r) {
        probe_s(" FAIL ");
        probe_dec(IoErr());
        probe_ch('\n');
        return;
    }
    probe_s(" OK");
    if (a[0]) { probe_s(" \""); probe_s((char *)a[0]); probe_ch('"'); } else probe_s(" -");
    if (a[1]) { probe_s(" \""); probe_s((char *)a[1]); probe_ch('"'); } else probe_s(" -");
    probe_ch('\n');
    FreeArgs(r);
}

/* the "command": entered by RunCommand() */
static LONG command(void)
{
    char buf[64];
    LONG n, c, i;

    switch (mode) {
    case 0:
        readargs("FROM,TO");
        rest_of_line("  then FGets");
        break;
    case 1:
        for (i = 0; i < 63 && (c = FGetC(Input())) >= 0; i++) {
            buf[i] = (char)c;
            if (c == '\n') {
                i++;
                break;
            }
        }
        show("  FGetC line", buf, i);
        break;
    case 2:
        n = Read(Input(), buf, 20);
        probe_s("  Read = ");
        probe_dec(n);
        probe_ch('\n');
        if (n > 0)
            show("  Read data", buf, n);
        break;
    case 3:
        show("  GetArgStr", (char *)GetArgStr(), -1);
        break;
    case 4:
        readargs("FROM,TO");
        readargs("FROM,TO");
        break;
    case 5:
        for (i = 0; i < 5; i++) {
            LONG r = ReadItem((STRPTR)buf, sizeof(buf), NULL);
            probe_s("  ReadItem ");
            probe_dec(r);
            show("", buf, -1);
            if (r == ITEM_NOTHING || r == ITEM_ERROR)
                break;
        }
        rest_of_line("  then FGets");
        break;
    case 6:
        break;
    case 7:
        c = FGetC(Input());
        probe_s("  FGetC = ");
        probe_dec(c);
        probe_ch('\n');
        n = Read(Input(), buf, 6);
        probe_s("  Read = ");
        probe_dec(n);
        probe_ch('\n');
        if (n > 0)
            show("  Read data", buf, n);
        break;
    case 8:
        readargs("FROM/F");
        rest_of_line("  then FGets");
        break;
    case 9:
        probe_flush();
        FPuts(Output(), (STRPTR)"  [buffered by the command]");
        break;
    }
    return 7;
}

static struct Task *parent;

static int childmode;

static void child(void)
{
    if (childmode)
        rest_of_line("  FGets");
    else
        readargs("FROM,TO");
    show("  GetArgStr", (char *)GetArgStr(), -1);
    rest_of_line("  then FGets");
    Forbid();
    Signal(parent, SIGBREAKF_CTRL_F);
}

static void newproc(int m, const char *args)
{
    BPTR in = Open((STRPTR)IN, MODE_OLDFILE);
    struct Process *p;
    probe_s("CreateNewProc");
    show(" NP_Arguments", args, -1);
    parent = FindTask(NULL);
    childmode = m;
    SetSignal(0, SIGBREAKF_CTRL_F);
    p = CreateNewProcTags(NP_Entry, (ULONG)child, NP_Name, (ULONG)"lxaprobe cmdline child",
                          NP_Arguments, (ULONG)args, NP_Input, in, NP_CloseInput, FALSE,
                          NP_Output, Output(), NP_CloseOutput, FALSE, NP_StackSize, 8000, TAG_END);
    if (p)
        Wait(SIGBREAKF_CTRL_F);
    else
        probe_s("  CreateNewProc failed\n");
    Delay(5);
    Close(in);
}

static void run(int m, const char *what, const char *args)
{
    ULONG *segmem = (ULONG *)AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);
    BPTR in, old;
    LONG len = 0, rc;

    while (args[len])
        len++;
    /* a one-hunk segment list whose code jumps to command() */
    segmem[0] = 0;
    ((UWORD *)&segmem[1])[0] = 0x4ef9;      /* jmp abs.l */
    *(ULONG *)((UBYTE *)&segmem[1] + 2) = (ULONG)command;
    CacheClearU();

    in = Open((STRPTR)IN, MODE_OLDFILE);
    old = SelectInput(in);
    if (m >= 100) {
        /* the caller has read ahead: its buffer holds the rest of the file */
        m -= 100;
        probe_s("(caller FGetC = ");
        probe_dec(FGetC(in));
        probe_s(") ");
    }
    mode = m;
    probe_s(what);
    show(" args", args, len);
    rc = RunCommand(MKBADDR(segmem), 8000, (STRPTR)args, len);
    SelectInput(old);
    probe_s("  rc = ");
    probe_dec(rc);
    probe_ch('\n');
    {
        /* what the caller reads from the input afterwards */
        char buf[64];
        BPTR o = SelectInput(in);
        if (FGets(in, (STRPTR)buf, sizeof(buf)))
            show("  caller FGets", buf, -1);
        else
            probe_s("  caller FGets EOF\n");
        SelectInput(o);
    }
    Close(in);
    FreeVec(segmem);
}

int main(void)
{
    BPTR fh = Open((STRPTR)IN, MODE_NEWFILE);
    Write(fh, (APTR)"line1 x\nline2\n", 14);
    Close(fh);

    P_SECTION("command line via Input()");
    run(0, "ReadArgs", "a b\n");
    run(0, "ReadArgs", "a b c\n");
    run(0, "ReadArgs", "a b");
    run(0, "ReadArgs", "");
    run(0, "ReadArgs", "a ; c\n");
    run(4, "ReadArgs twice", "a b\n");
    run(8, "ReadArgs /F", "a b\n");
    run(5, "ReadItem", "x \"y z\"\n");
    run(1, "FGetC", "a b\n");
    run(1, "FGetC", "");
    run(2, "Read", "a b\n");
    run(3, "GetArgStr", "a b\n");
    run(6, "nothing read", "a b\n");
    run(7, "FGetC then Read", "a b\n");
    run(9, "unflushed output", "\n");
    run(106, "nothing read", "a b\n");
    run(101, "FGetC", "a b\n");
    run(101, "FGetC", "");
    run(100, "ReadArgs", "a b\n");

    P_SECTION("CreateNewProc NP_Arguments");
    newproc(0, "a b\n");
    newproc(1, "a b\n");

    DeleteFile((STRPTR)IN);
    return 0;
}
