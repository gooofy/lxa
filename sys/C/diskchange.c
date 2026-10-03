/*
 * DISKCHANGE - tell a file system that its disk has changed
 *
 * Template: DEVICE/A
 *
 * Inhibits the file system and releases it again (dos Inhibit()), which
 * makes it re-read the disk.  Silent on success (AmigaOS 3.1, verified on
 * the reference, Phase 221); a handler that refuses gives its fault text
 * and RC 20.
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

int main(void)
{
    LONG args[1] = { 0 };
    struct RDArgs *rda;
    LONG rc = 0;

    rda = ReadArgs((STRPTR)"DEVICE/A", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    if (!Inhibit((STRPTR)args[0], DOSTRUE) || !Inhibit((STRPTR)args[0], DOSFALSE)) {
        LONG err = IoErr();
        PrintFault(err, (STRPTR)args[0]);
        SetIoErr(err);
        rc = RETURN_FAIL;
    }
    FreeArgs(rda);
    return rc;
}
