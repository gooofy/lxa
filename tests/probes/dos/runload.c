/*
 * Probe (Phase 237d): a program started with Run is loaded by the new CLI
 * process, and loading from disk lets the starting program run on.  Fred
 * Fish Wicon (disk 154) creates a port, starts a copy of itself with
 * Execute("run ..."), deletes the port and waits for the copy's port; the
 * copy, if it still finds the first port, takes it for a running Wicon and
 * quits.  The probe does the same with a copy of itself (argument CHILD),
 * which writes what it found to T:probe-runload.
 */
#include <exec/types.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static char name[] = "probe.runload";
static char result[] = "T:probe-runload";

int main(int argc, char **argv)
{
    struct MsgPort *port;
    BPTR f, nil;
    char cmd[200], buf[32];
    LONG i;

    if (argc > 1) {         /* the copy */
        f = Open((STRPTR)result, MODE_NEWFILE);
        if (f) {
            Write(f, (APTR)(FindPort((STRPTR)name) ? "port found" : "no port"), FindPort((STRPTR)name) ? 10 : 7);
            Close(f);
        }
        return 0;
    }
    DeleteFile((STRPTR)result);
    port = CreatePort((STRPTR)name, 0);
    if (!port)
        return 20;
    if (!GetProgramName((STRPTR)buf, sizeof(buf)))
        return 20;
    {
        const char *a = "run <NIL: >NIL: \"", *b = (const char *)buf;
        char *d = cmd;
        while (*a)
            *d++ = *a++;
        while (*b)
            *d++ = *b++;
        a = "\" CHILD";
        while (*a)
            *d++ = *a++;
        *d = 0;
    }
    nil = Open((STRPTR)"NIL:", MODE_NEWFILE);
    Execute((STRPTR)cmd, nil, nil);
    DeletePort(port);
    Delay(100);
    f = Open((STRPTR)result, MODE_OLDFILE);
    for (i = 0; i < 32; i++)
        buf[i] = 0;
    if (f) {
        Read(f, buf, 31);
        Close(f);
    }
    probe_s("the copy: ");
    probe_s(f ? buf : "(did not run)");
    probe_ch('\n');
    if (nil)
        Close(nil);
    DeleteFile((STRPTR)result);
    return 0;
}
