/*
 * Probe (Phase 222a): version, revision and IdString of the Kickstart 3.1
 * libraries whose interface lxa implements at the 3.1 level - lib_Version,
 * lib_Revision, lib_IdString and the resident tag's rt_Version and
 * rt_IdString (C:Version and programs that check revisions read these).
 */
#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/libraries.h>
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
        "exec.library", "graphics.library", "intuition.library", "layers.library", "utility.library",
        "gadtools.library", "expansion.library", "keymap.library", "mathffp.library",
    };
    int i;

    P_SECTION("ROM libraries");
    for (i = 0; i < (int)(sizeof(libs) / sizeof(libs[0])); i++) {
        struct Resident *r = FindResident((STRPTR)libs[i]);
        struct Library *l = OpenLibrary((STRPTR)libs[i], 0);
        probe_s(libs[i]);
        if (r) {
            probe_s(": rt_Version ");
            probe_dec(r->rt_Version);
            probe_s(" rt_IdString ");
            pesc((char *)r->rt_IdString);
        } else
            probe_s(": no resident tag");
        probe_ch('\n');
        if (l) {
            probe_s("  ");
            probe_dec(l->lib_Version);
            probe_ch('.');
            probe_dec(l->lib_Revision);
            probe_s(" ");
            pesc((char *)l->lib_IdString);
            probe_ch('\n');
            CloseLibrary(l);
        } else
            probe_s("  OpenLibrary failed\n");
    }
    return 0;
}
