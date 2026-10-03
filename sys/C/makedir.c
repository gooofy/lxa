/*
 * MAKEDIR - create directories
 *
 * Template: NAME/M
 *
 * Silent on success.  Messages and return codes of AmigaOS 3.1 (verified
 * on the reference, Phase 221):
 *   No name given                          (RC 20)
 *   <name> already exists                  (RC 10)
 *   Can't create directory <name>
 *   <fault text>                           (RC 10)
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
    LONG args[1] = { 0 };
    struct RDArgs *rda;
    STRPTR *names;
    LONG rc = 0;

    rda = ReadArgs((STRPTR)"NAME/M", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    names = (STRPTR *)args[0];
    if (!names || !*names) {
        PutStr((STRPTR)"No name given\n");
        FreeArgs(rda);
        SetIoErr(0);
        return RETURN_FAIL;
    }
    for (; *names; names++) {
        BPTR lock = CreateDir(*names);
        if (lock) {
            UnLock(lock);
            continue;
        }
        {
            LONG err = IoErr();
            if (err == ERROR_OBJECT_EXISTS) {
                Printf((STRPTR)"%s already exists\n", (LONG)*names);
                SetIoErr(0);
            } else {
                Printf((STRPTR)"Can't create directory %s\n", (LONG)*names);
                PrintFault(err, NULL);
                SetIoErr(err);
            }
            rc = RETURN_ERROR;
            break;
        }
    }
    FreeArgs(rda);
    return rc;
}
