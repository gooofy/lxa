/*
 * Probe (Phase 237d): a command's own name.  Fred Fish Recorder (disk 673)
 * starts a copy of itself with SystemTagList("<its name> <NIL: >>file
 * PROCESS n"); on lxa the name was empty and the command line failed.
 * Prints the file part of GetProgramName() and of cli_CommandName.
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

int main(void)
{
    char buf[256];
    struct Process *me = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(me->pr_CLI);

    probe_s("GetProgramName: ");
    if (GetProgramName((STRPTR)buf, sizeof(buf)))
        probe_s((char *)FilePart((STRPTR)buf));
    else
        probe_s("(fails)");
    probe_ch('\n');
    probe_s("cli_CommandName: ");
    if (!cli)
        probe_s("(no CLI)");
    else {
        UBYTE *b = (UBYTE *)BADDR(cli->cli_CommandName);
        int i, n = b ? b[0] : 0;
        for (i = 0; i < n && i < 255; i++)
            buf[i] = b[1 + i];
        buf[i] = 0;
        probe_s((char *)FilePart((STRPTR)buf));
    }
    probe_ch('\n');
    return 0;
}
