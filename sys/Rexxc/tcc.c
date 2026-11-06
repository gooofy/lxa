/*
 * TCC - close the ARexx trace console (Phase 250)
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
#include "rexx_packet.h"

static const char version[] = "$VER: TCC 36.21 (01.11.2026)";

int main(void)
{
    LONG rc;

    (void)version;
    rc = rexx_packet(RXTCCLS);
    if (rc < 0) {
        PutStr((STRPTR)"TCC: ARexx is not active\n");
        return RETURN_WARN;
    }
    return rc ? RETURN_WARN : RETURN_OK;
}
