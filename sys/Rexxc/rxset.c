/*
 * RXSET - set, remove or list ARexx Clip List entries (Phase 250)
 *
 *   RXSET                 lists every clip as "name = value"
 *   RXSET name [=] value  adds or replaces a clip
 *   RXSET name            removes it
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

#include "rexx_common.h"

static const char version[] = "$VER: RXSET 36.21 (01.11.2026)";

int main(void)
{
    char *line = (char *)GetArgStr();
    LONG len, n0, n1, v0;
    int rc = RETURN_OK;

    (void)version;
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 36);
    if (!RexxSysBase)
        return RETURN_FAIL;
    if (!line)
        line = (char *)"";
    len = lxa_rexx_strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t'))
        len--;
    n0 = 0;
    while (n0 < len && (line[n0] == ' ' || line[n0] == '\t'))
        n0++;

    if (n0 >= len) {
        struct Node *n;
        LockRexxBase(0);
        for (n = RexxSysBase->rl_ClipList.lh_Head; n->ln_Succ; n = n->ln_Succ)
            Printf((STRPTR)"%s = %s\n", (LONG)n->ln_Name,
                   (LONG)(((struct RexxRsrc *)n)->rr_Arg1 ? ((struct RexxRsrc *)n)->rr_Arg1 : (LONG)""));
        UnlockRexxBase(0);
    } else {
        n1 = n0;
        while (n1 < len && line[n1] != ' ' && line[n1] != '\t' && line[n1] != '=')
            n1++;
        v0 = n1;
        while (v0 < len && (line[v0] == ' ' || line[v0] == '\t'))
            v0++;
        if (v0 < len && line[v0] == '=') {
            v0++;
            while (v0 < len && (line[v0] == ' ' || line[v0] == '\t'))
                v0++;
        }
        if (v0 >= len) {
            if (!lxa_rexx_setclip(line + n0, n1 - n0, NULL, 0))
                rc = RETURN_WARN;
        } else if (!lxa_rexx_setclip(line + n0, n1 - n0, line + v0, len - v0)) {
            rc = RETURN_ERROR;
        }
    }
    CloseLibrary((struct Library *)RexxSysBase);
    return rc;
}
