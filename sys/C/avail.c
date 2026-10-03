/*
 * AVAIL - show free memory
 *
 * Template: CHIP/S,FAST/S,TOTAL/S,FLUSH/S
 *
 * Layout of AmigaOS 3.1 (verified on the reference, Phase 221):
 *   Type  Available    In-Use   Maximum   Largest
 *   chip    2011960     81096   2093056   2011832
 *   fast   24583824    582000  25165824  16777184
 *   total  26595784    663096  27258880  16777184
 * CHIP/FAST/TOTAL print only the available bytes of that kind; FLUSH
 * first expunges unused libraries, devices and fonts.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>

#include <string.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;

#define TEMPLATE "CHIP/S,FAST/S,TOTAL/S,FLUSH/S"

enum { A_CHIP, A_FAST, A_TOTAL, A_FLUSH, A_COUNT };

static void row(const char *name, ULONG flags)
{
    ULONG avail = AvailMem(flags);
    ULONG max = AvailMem(flags | MEMF_TOTAL);
    ULONG largest = AvailMem(flags | MEMF_LARGEST);

    Printf((STRPTR)"%-6s%9ld%10ld%10ld%10ld\n", (LONG)name, avail, max - avail, max, largest);
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;

    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rda) {
        PrintFault(IoErr(), NULL);
        return RETURN_FAIL;
    }
    if (args[A_FLUSH]) {
        /* an impossible allocation makes exec expunge what it can */
        APTR p = AllocMem(0x7FFFFFF0, MEMF_PUBLIC);
        if (p)
            FreeMem(p, 0x7FFFFFF0);
    }
    if (args[A_CHIP])
        Printf((STRPTR)"%ld\n", AvailMem(MEMF_CHIP));
    else if (args[A_FAST])
        Printf((STRPTR)"%ld\n", AvailMem(MEMF_FAST));
    else if (args[A_TOTAL])
        Printf((STRPTR)"%ld\n", AvailMem(MEMF_ANY));
    else {
        PutStr((STRPTR)"Type  Available    In-Use   Maximum   Largest\n");
        row("chip", MEMF_CHIP);
        row("fast", MEMF_FAST);
        row("total", MEMF_ANY);
    }
    FreeArgs(rda);
    return 0;
}
