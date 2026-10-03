/*
 * JOIN - concatenate files
 *
 * Template: FROM/A/M,AS=TO/K/A
 *
 * Silent on success; "Can't open <file>" (RC 20, the partial destination
 * stays) - AmigaOS 3.1 behaviour, verified on the reference (Phase 221).
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "FROM/A/M,AS=TO/K/A"

int main(void)
{
    LONG args[2] = { 0, 0 };
    struct RDArgs *rda;
    STRPTR *from;
    BPTR out;
    UBYTE *buf;
    LONG rc = 0;

    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    buf = AllocVec(8192, MEMF_PUBLIC);
    out = Open((STRPTR)args[1], MODE_NEWFILE);
    if (!buf || !out) {
        LONG err = buf ? IoErr() : ERROR_NO_FREE_STORE;
        Printf((STRPTR)"Can't open %s\n", args[1]);
        if (out)
            Close(out);
        if (buf)
            FreeVec(buf);
        FreeArgs(rda);
        SetIoErr(err);
        return RETURN_FAIL;
    }
    for (from = (STRPTR *)args[0]; *from; from++) {
        BPTR in = Open(*from, MODE_OLDFILE);
        LONG n;
        if (!in) {
            LONG err = IoErr();
            Printf((STRPTR)"Can't open %s\n", (LONG)*from);
            SetIoErr(err);
            rc = RETURN_FAIL;
            break;
        }
        while ((n = Read(in, buf, 8192)) > 0) {
            if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
                PutStr((STRPTR)"***Break\n");
                rc = RETURN_WARN;
                break;
            }
            if (Write(out, buf, n) != n) {
                LONG err = IoErr();
                PrintFault(err, (STRPTR)args[1]);
                SetIoErr(err);
                rc = RETURN_FAIL;
                break;
            }
        }
        Close(in);
        if (rc)
            break;
    }
    /* AmigaOS 3.1 keeps the (partial) destination on errors */
    {
        LONG err = IoErr();
        Close(out);
        SetIoErr(err);
    }
    FreeVec(buf);
    FreeArgs(rda);
    return rc;
}
