/*
 * Probe (Phase 237e): Lock(":") - the root of the current directory's
 * volume.  Fred Fish Seeker (DeckBrowser, disk 590) locks ":" to list the
 * volume it runs from.  Prints whether Lock(":") succeeds, whether it is
 * the same as the root reached by ParentDir() from the current directory,
 * and the name of that root.
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

int main(void)
{
    BPTR cur = CurrentDir(0), root, l, p;
    char name[128];

    CurrentDir(cur);
    root = DupLock(cur);
    while ((p = ParentDir(root)) != 0) {
        UnLock(root);
        root = p;
    }
    l = Lock((STRPTR)":", ACCESS_READ);
    P_BOOL("Lock(\":\") succeeded", l != 0);
    if (l) {
        P_BOOL("same as the root of the current directory", SameLock(l, root) == LOCK_SAME);
        if (NameFromLock(l, (STRPTR)name, sizeof(name))) {
            int n = 0;
            while (name[n])
                n++;
            P_BOOL("its name ends in ':'", n > 0 && name[n - 1] == ':');
        }
        UnLock(l);
    } else
        P_LONG("IoErr()", IoErr());
    UnLock(root);
    return 0;
}
