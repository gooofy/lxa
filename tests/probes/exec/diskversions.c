/*
 * Probe (Phase 222g): version, revision and IdString of the Workbench 3.1
 * disk libraries (LIBS:) lxa implements at the 3.1 level - an application
 * asking for a version (OpenLibrary("iffparse.library", 40)) fails when
 * lxa reports less.  Not listed: libraries lxa implements beyond 3.1
 * (diskfont V45, locale V47, icon/workbench V44) and the math/rexxsyslib
 * libraries whose 3.1 disk versions are older than lxa's (V36-V38).
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <exec/resident.h>
#include <clib/exec_protos.h>
#include <inline/exec.h>
#include "probe.h"

extern struct ExecBase *SysBase;

static void pesc(const char *s)
{
    probe_ch('"');
    while (s && *s) {
        UBYTE c = *s++;
        if (c == '\r')
            probe_s("\\r");
        else if (c == '\n')
            probe_s("\\n");
        else
            probe_ch(c);
    }
    probe_ch('"');
}

int main(void)
{
    static const char *const libs[] = {
        "asl.library", "commodities.library", "datatypes.library", "iffparse.library",
        "mathieeesingbas.library", "amigaguide.library",
    };
    int i;

    P_SECTION("disk libraries");
    for (i = 0; i < (int)(sizeof(libs) / sizeof(libs[0])); i++) {
        struct Library *l = OpenLibrary((STRPTR)libs[i], 0);
        probe_s(libs[i]);
        if (l) {
            probe_s(": ");
            probe_dec(l->lib_Version);
            probe_ch('.');
            probe_dec(l->lib_Revision);
            probe_s(" ");
            pesc((char *)l->lib_IdString);
            probe_ch('\n');
            CloseLibrary(l);
        } else
            probe_s(": OpenLibrary failed\n");
    }
    /* in the A4000 Kickstart 3.1 ROM */
    {
        struct Resident *r = FindResident((STRPTR)"mathieeesingbas.library");
        probe_s("mathieeesingbas.library resident:");
        if (r) {
            probe_s(" rt_Version ");
            probe_dec(r->rt_Version);
            probe_s(" rt_IdString ");
            pesc((char *)r->rt_IdString);
        } else
            probe_s(" none");
        probe_ch('\n');
    }
    return 0;
}
