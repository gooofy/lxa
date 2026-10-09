/*
 * Probe (Phase 237): the library bases dos.library keeps in DosLibrary.
 * SAS/C 6 startup code (cs.o) takes UtilityBase from DOSBase->dl_UtilityBase
 * instead of opening utility.library (Fish MeMeter: SMult32 through it).
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

int main(void)
{
    struct Library *ub = OpenLibrary("utility.library", 0);
    struct Library *ib = OpenLibrary("intuition.library", 0);

    P_SECTION("DosLibrary bases");
    P_NULL("dl_UtilityBase", DOSBase->dl_UtilityBase);
    P_BOOL("dl_UtilityBase == utility.library", (struct Library *)DOSBase->dl_UtilityBase == ub);
    P_NULL("dl_IntuitionBase", DOSBase->dl_IntuitionBase);
    P_BOOL("dl_IntuitionBase == intuition.library", (struct Library *)DOSBase->dl_IntuitionBase == ib);
    P_NULL("dl_Root", DOSBase->dl_Root);
    if (ib)
        CloseLibrary(ib);
    if (ub)
        CloseLibrary(ub);
    return 0;
}
