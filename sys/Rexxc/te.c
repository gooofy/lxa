/*
 * TE - end interactive tracing of ARexx programs (Phase 250)
 *
 * The flag lives in the rexxsyslib.library base; the interpreters test
 * it before every clause.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>

struct RxsLib *RexxSysBase = NULL;

static const char version[] = "$VER: TE 36.21 (01.11.2026)";

int main(void)
{
    struct RxsLib *rl;

    (void)version;
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase)
        return RETURN_FAIL;
    rl = RexxSysBase;
    LockRexxBase(0);
    rl->rl_Flags &= ~(1 << RLFB_TRACE);
    UnlockRexxBase(0);
    CloseLibrary((struct Library *)RexxSysBase);
    return RETURN_OK;
}
