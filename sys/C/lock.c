/*
 * LOCK - write-protect a volume
 *
 * Template: DRIVE/A,ON/S,OFF/S,PASSKEY/K
 *
 * Sends ACTION_WRITE_PROTECT to the drive's handler (ON is the default;
 * OFF with the same passkey releases it).  On failure, as AmigaOS 3.1
 * (verified on the reference, Phase 221):
 *   Attempt to lock drive <drive> failed            (RC 20)
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

enum { A_DRIVE, A_ON, A_OFF, A_PASSKEY, A_COUNT };

/* the passkey is passed to the handler as a hash of its characters */
static LONG passkey_hash(const char *s)
{
    ULONG h = 0;
    if (!s)
        return 0;
    while (*s)
        h = h * 31 + (UBYTE)*s++;
    return (LONG)(h ? h : 1);
}

int main(void)
{
    LONG args[A_COUNT] = { 0, 0, 0, 0 };
    struct RDArgs *rda;
    struct DevProc *dvp;
    LONG ok = DOSFALSE, err = 0;
    const char *p;

    rda = ReadArgs((STRPTR)"DRIVE/A,ON/S,OFF/S,PASSKEY/K", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    for (p = (char *)args[A_DRIVE]; *p && *p != ':'; p++)
        ;
    dvp = *p == ':' ? GetDeviceProc((STRPTR)args[A_DRIVE], NULL) : NULL;
    if (dvp && dvp->dvp_Port && !(dvp->dvp_Flags & DVPF_ASSIGN)) {
        ok = DoPkt(dvp->dvp_Port, ACTION_WRITE_PROTECT, args[A_OFF] ? DOSFALSE : DOSTRUE,
                   passkey_hash((char *)args[A_PASSKEY]), 0, 0, 0);
        err = IoErr();
    } else {
        err = dvp ? ERROR_ACTION_NOT_KNOWN : IoErr();
    }
    if (dvp)
        FreeDeviceProc(dvp);
    if (!ok) {
        Printf((STRPTR)"Attempt to lock drive %s failed\n", args[A_DRIVE]);
        FreeArgs(rda);
        SetIoErr(err);
        return RETURN_FAIL;
    }
    FreeArgs(rda);
    return 0;
}
