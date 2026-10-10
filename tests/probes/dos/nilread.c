/*
 * Probe (Phase 237d): reading and querying NIL:, the input every program
 * gets in the Fred Fish mass run on the reference (lxaprobe RUN).  Prints
 * what Read(), IsInteractive(), WaitForChar() and Seek() return on it.
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
    BPTR f = Open((STRPTR)"NIL:", MODE_OLDFILE);
    char buf[16];
    LONG r;

    P_BOOL("Open(NIL:)", f != 0);
    if (!f)
        return 0;
    SetIoErr(0);
    r = Read(f, buf, 10);
    P_LONG("Read", r);
    P_LONG("IoErr", IoErr());
    r = Read(f, buf, 1);
    P_LONG("Read again", r);
    P_LONG("IsInteractive", IsInteractive(f) ? 1 : 0);
    P_LONG("Write", Write(f, (APTR)"abc", 3));
    P_BOOL("fh_Type set", ((struct FileHandle *)BADDR(f))->fh_Type != NULL);
    Close(f);
    return 0;
}
