/*
 * RELABEL - rename a volume
 *
 * Template: DRIVE/A,NAME/A,INTL/S,NOINTL/S,FFS/S,OFS/S
 *
 * dos Relabel() (ACTION_RENAME_DISK).  The file system options only apply
 * to FFS partitions and are accepted for compatibility.  As AmigaOS 3.1
 * (verified on the reference, Phase 221): silent on success,
 *   Invalid device or volume name                  (RC 20)
 */

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

int main(void)
{
    LONG args[6] = { 0, 0, 0, 0, 0, 0 };
    struct RDArgs *rda;
    char name[32];
    int n;

    rda = ReadArgs((STRPTR)"DRIVE/A,NAME/A,INTL/S,NOINTL/S,FFS/S,OFS/S", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    /* the new name may be given with a colon */
    n = strlen((char *)args[1]);
    if (n > (int)sizeof(name) - 1)
        n = sizeof(name) - 1;
    CopyMem((APTR)args[1], name, n);
    name[n] = '\0';
    if (n && name[n - 1] == ':')
        name[n - 1] = '\0';

    if (!Relabel((STRPTR)args[0], (STRPTR)name)) {
        LONG err = IoErr();
        if (err == ERROR_DEVICE_NOT_MOUNTED || err == ERROR_OBJECT_NOT_FOUND ||
            err == ERROR_INVALID_COMPONENT_NAME) {
            PutStr((STRPTR)"Invalid device or volume name\n");
            err = 0;
        } else {
            PrintFault(err, NULL);
        }
        FreeArgs(rda);
        SetIoErr(err);
        return RETURN_FAIL;
    }
    FreeArgs(rda);
    return 0;
}
