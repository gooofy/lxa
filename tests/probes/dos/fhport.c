/*
 * Probe (Phase 237e): the longword at offset 4 of a FileHandle (fh_Port,
 * the interactive flag) and the CLI's standard handles.  The GFA-BASIC
 * runtime of Fred Fish A-GENE (disk 702) reads a file byte by byte when
 * it finds offset 4 non-zero; Fred Fish SiliCon (disk 102) sends packets
 * to the handler of cli_StandardOutput.  Prints offset 4 and IsInteractive()
 * of a file, NIL:, Input() and Output(), and whether cli_StandardInput /
 * cli_StandardOutput / cli_CurrentInput / cli_CurrentOutput are set and
 * equal to Input() / Output().
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

static void show(const char *label, BPTR f)
{
    struct FileHandle *fh = (struct FileHandle *)BADDR(f);
    LONG v;

    probe_s(label);
    if (!fh) {
        probe_s(": none\n");
        return;
    }
    v = (LONG)fh->fh_Port;
    probe_s(": offset 4 = ");
    probe_s(v == 0 ? "0" : v == -1 ? "-1" : "other");
    probe_s(", IsInteractive ");
    probe_dec(IsInteractive(f) ? 1 : 0);
    probe_ch('\n');
}

static void cli(const char *label, BPTR v, BPTR want)
{
    probe_s(label);
    probe_s(v == 0 ? ": 0\n" : v == want ? ": the same\n" : ": another handle\n");
}

int main(void)
{
    struct CommandLineInterface *c = Cli();
    BPTR f = Open((STRPTR)"T:probe-fhport", MODE_NEWFILE), n = Open((STRPTR)"NIL:", MODE_OLDFILE);

    show("file", f);
    show("NIL:", n);
    show("Input()", Input());
    show("Output()", Output());
    if (c) {
        cli("cli_StandardInput vs Input()", c->cli_StandardInput, Input());
        cli("cli_CurrentInput vs Input()", c->cli_CurrentInput, Input());
        cli("cli_StandardOutput vs Output()", c->cli_StandardOutput, Output());
        cli("cli_CurrentOutput vs Output()", c->cli_CurrentOutput, Output());
    }
    if (f) {
        Close(f);
        DeleteFile((STRPTR)"T:probe-fhport");
    }
    if (n)
        Close(n);
    return 0;
}
