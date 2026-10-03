/*
 * ADDBUFFERS - add buffers to a file system's cache
 *
 * Template: DRIVE/A,BUFFERS/N
 *
 * Sends ACTION_MORE_CACHE (dos AddBuffers()) and reports the new total:
 * "DH0: has 50 buffers".  A handler that does not know the packet is
 * reported with the fault text (AmigaOS 3.1, verified on the reference,
 * Phase 221: RAM: prints "packet request type unknown", RC 0).
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rda;
    LONG n;

    rda = ReadArgs((STRPTR)"DRIVE/A,BUFFERS/N", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    n = args[1] ? *(LONG *)args[1] : 0;
    if (AddBuffers((STRPTR)args[0], n)) {
        Printf((STRPTR)"%s has %ld buffers\n", args[0], IoErr());
    } else {
        PrintFault(IoErr(), NULL);
    }
    FreeArgs(rda);
    SetIoErr(0);
    return 0;
}
