/*
 * Probe (Phase 240): the MonitorInfo (DTAG_MNTR) of every entry of the
 * display database - view position and its range, view resolution, mouse
 * ticks, rows and colour clocks.  Programs centre their views and scale
 * mouse movement with these.  The MonitorSpec pointer is reduced to its
 * name.
 */
#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/monitor.h>
#include <graphics/modeid.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include "probe.h"

static void point(const char *n, Point *p)
{
    probe_s(" ");
    probe_s(n);
    probe_s("=");
    probe_dec(p->x);
    probe_ch(',');
    probe_dec(p->y);
}

int main(void)
{
    ULONG id = INVALID_ID;

    while ((id = NextDisplayInfo(id)) != INVALID_ID)
    {
        struct MonitorInfo mi;

        probe_hex(id, 8);
        if (GetDisplayInfoData(NULL, (UBYTE *)&mi, sizeof(mi), DTAG_MNTR, id) >= sizeof(struct QueryHeader))
        {
            probe_s(" mspc=");
            probe_s(mi.Mspc && mi.Mspc->ms_Node.xln_Name ? mi.Mspc->ms_Node.xln_Name : "NULL");
            point("pos", &mi.ViewPosition);
            point("res", &mi.ViewResolution);
            probe_s(" range=");
            probe_dec(mi.ViewPositionRange.MinX);
            probe_ch(',');
            probe_dec(mi.ViewPositionRange.MinY);
            probe_ch(',');
            probe_dec(mi.ViewPositionRange.MaxX);
            probe_ch(',');
            probe_dec(mi.ViewPositionRange.MaxY);
            probe_s(" rows=");
            probe_dec(mi.TotalRows);
            probe_s(" cclks=");
            probe_dec(mi.TotalColorClocks);
            probe_s(" minrow=");
            probe_dec(mi.MinRow);
            probe_s(" compat=");
            probe_dec(mi.Compatibility);
            point("ticks", &mi.MouseTicks);
            point("defpos", &mi.DefaultViewPosition);
            probe_s(" pref=");
            probe_hex(mi.PreferredModeID, 8);
        }
        else
            probe_s(" (no DTAG_MNTR)");
        probe_ch('\n');
    }
    return 0;
}
