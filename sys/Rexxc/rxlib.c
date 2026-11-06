/*
 * RXLIB - add or list ARexx function libraries and hosts (Phase 250)
 *
 *   RXLIB                                  lists the Library List
 *   RXLIB name priority offset version     adds a function library
 *   RXLIB name priority                    adds a function host
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>

struct RxsLib *RexxSysBase = NULL;

#include "rexx_common.h"

static const char version[] = "$VER: RXLIB 36.21 (01.11.2026)";

int main(void)
{
    LONG args[4] = { 0, 0, 0, 0 };
    struct RDArgs *rda;
    int rc = RETURN_OK;

    (void)version;
    rda = ReadArgs((STRPTR)"NAME,PRI/N,OFFSET/N,VERSION/N", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"RXLIB");
        return RETURN_ERROR;
    }
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase) {
        FreeArgs(rda);
        return RETURN_FAIL;
    }
    if (!args[0]) {
        struct Node *n;
        LockRexxBase(0);
        for (n = RexxSysBase->rl_LibList.lh_Head; n->ln_Succ; n = n->ln_Succ) {
            struct RexxRsrc *rr = (struct RexxRsrc *)n;
            if (n->ln_Type == RRT_LIB)
                Printf((STRPTR)"%s (library, priority %ld, offset %ld, version %ld)\n",
                       (LONG)n->ln_Name, (LONG)n->ln_Pri, rr->rr_Arg1, rr->rr_Arg2);
            else
                Printf((STRPTR)"%s (host, priority %ld)\n", (LONG)n->ln_Name, (LONG)n->ln_Pri);
        }
        UnlockRexxBase(0);
    } else {
        const char *name = (const char *)args[0];
        LONG pri = args[1] ? *(LONG *)args[1] : 0;
        BOOL is_lib = args[2] != 0;
        if (!lxa_rexx_addlib(name, lxa_rexx_strlen(name), pri, is_lib,
                             is_lib ? *(LONG *)args[2] : -1, args[3] ? *(LONG *)args[3] : 0))
            rc = RETURN_WARN;
    }
    CloseLibrary((struct Library *)RexxSysBase);
    FreeArgs(rda);
    return rc;
}
