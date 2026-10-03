/*
 * SETPATCH - install system patches
 *
 * Template: QUIET/S,NOCACHE/S,REVERSE/S
 *
 * On AmigaOS SetPatch corrects bugs of the Kickstart ROM and installs CPU
 * support code.  lxa's ROM needs no such patches, so SetPatch has nothing
 * to install: it only reports that (unless QUIET) and succeeds, which keeps
 * Startup-Sequences working ("SetPatch QUIET" is silent with RC 0, as on
 * AmigaOS 3.1, verified on the reference, Phase 221).  NOCACHE disables
 * the CPU caches like on AmigaOS; REVERSE is accepted.
 */

#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

int main(void)
{
    LONG args[3] = { 0, 0, 0 };
    struct RDArgs *rda;

    rda = ReadArgs((STRPTR)"QUIET/S,NOCACHE/S,REVERSE/S", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    if (args[1])
        CacheControl(0, CACRF_EnableI | CACRF_EnableD);
    if (!args[0])
        PutStr((STRPTR)"SetPatch: the lxa ROM needs no patches.\n");
    FreeArgs(rda);
    return 0;
}
