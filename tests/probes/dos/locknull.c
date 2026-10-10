/*
 * Probe (Phase 237e): Lock() with a NULL name.  Fred Fish Slideshow (disk
 * 151) locks its own name, unlocks it and then calls Lock(NULL,
 * ACCESS_READ) to Examine() the directory it shows.  Prints whether
 * Lock(NULL) and Lock("") return a lock, whether it is the current
 * directory (SameLock()), and IoErr().
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

static void try(const char *label, STRPTR name)
{
    BPTR cur = CurrentDir(0), l;

    CurrentDir(cur);
    SetIoErr(0);
    l = Lock(name, ACCESS_READ);
    probe_s(label);
    probe_s(": ");
    if (!l) {
        probe_s("no lock, IoErr ");
        probe_dec(IoErr());
        probe_ch('\n');
        return;
    }
    probe_s(SameLock(l, cur) == LOCK_SAME ? "the current directory" : "another object");
    probe_ch('\n');
    UnLock(l);
}

int main(void)
{
    try("Lock(NULL)", NULL);
    try("Lock(\"\")", (STRPTR)"");
    return 0;
}
