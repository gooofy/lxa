/*
 * Probe (Phase 237d): an asynchronous System() runs its command line in a
 * shell - redirections are taken out of the arguments.  Fred Fish
 * Recorder (disk 673) starts itself with SYS_Asynch and "<name> <NIL:
 * >>file PROCESS n"; lxa handed the redirections to the program.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <utility/tagitem.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct TagItem tags[4];
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    BPTR f;
    LONG rc, n, i;
    char buf[64];

    DeleteFile((STRPTR)"T:probe-sysasync");
    tags[0].ti_Tag = SYS_Input;  tags[0].ti_Data = in;
    tags[1].ti_Tag = SYS_Output; tags[1].ti_Data = out;
    tags[2].ti_Tag = SYS_Asynch; tags[2].ti_Data = TRUE;
    tags[3].ti_Tag = TAG_DONE;   tags[3].ti_Data = 0;
    rc = SystemTagList((STRPTR)"Echo >>T:probe-sysasync hello <NIL: world", tags);
    P_LONG("SystemTagList", rc);
    Delay(100);
    f = Open((STRPTR)"T:probe-sysasync", MODE_OLDFILE);
    P_BOOL("output file exists", f != 0);
    if (f) {
        n = Read(f, buf, sizeof(buf) - 1);
        if (n < 0)
            n = 0;
        buf[n] = 0;
        for (i = 0; i < n; i++)
            if (buf[i] == '\n')
                buf[i] = '|';
        probe_s("output: ");
        probe_s(buf);
        probe_ch('\n');
        Close(f);
    }
    DeleteFile((STRPTR)"T:probe-sysasync");
    return 0;
}
