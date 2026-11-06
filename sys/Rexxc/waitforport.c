/*
 * WaitForPort - wait up to 10 seconds for a public message port (Phase 250)
 *
 *   WaitForPort <portname>
 *
 * Returns 0 when the port appeared, 5 (WARN) otherwise.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

static const char version[] = "$VER: WaitForPort 36.21 (01.11.2026)";

int main(void)
{
    LONG args[1] = { 0 };
    struct RDArgs *rda;
    struct MsgPort *port = NULL;
    int i;

    (void)version;
    rda = ReadArgs((STRPTR)"PORT/A", args, NULL);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"WaitForPort");
        return RETURN_ERROR;
    }
    for (i = 0; i <= 10 * 50; i += 10) {
        Forbid();
        port = FindPort((STRPTR)args[0]);
        Permit();
        if (port || (SetSignal(0, 0) & SIGBREAKF_CTRL_C))
            break;
        Delay(10);
    }
    FreeArgs(rda);
    return port ? RETURN_OK : RETURN_WARN;
}
