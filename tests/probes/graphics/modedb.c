/*
 * Probe (Phase 238): every entry of the display database - DisplayInfo
 * property flags, availability, DimensionInfo depth/nominal/raster ranges
 * and the overscan rectangles.  asl.library's screen-mode requester builds
 * its list, names and property window from these.
 */
#include <exec/types.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include "probe.h"

static void rect(const char *n, struct Rectangle *r)
{
    probe_s(" ");
    probe_s(n);
    probe_s("=");
    probe_dec(r->MinX);
    probe_ch(',');
    probe_dec(r->MinY);
    probe_ch(',');
    probe_dec(r->MaxX);
    probe_ch(',');
    probe_dec(r->MaxY);
}

int main(void)
{
    ULONG id = INVALID_ID;

    while ((id = NextDisplayInfo(id)) != INVALID_ID)
    {
        struct DisplayInfo di;
        struct DimensionInfo dims;

        probe_hex(id, 8);
        if (GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof(di), DTAG_DISP, id) >= sizeof(struct QueryHeader))
        {
            probe_s(" props=");
            probe_hex(di.PropertyFlags, 8);
            probe_s(" na=");
            probe_dec(di.NotAvailable);
            probe_s(" res=");
            probe_dec(di.Resolution.x);
            probe_ch(',');
            probe_dec(di.Resolution.y);
            probe_s(" ticks=");
            probe_dec(di.PixelSpeed);
            probe_s(" sprres=");
            probe_dec(di.SpriteResolution.x);
            probe_ch(',');
            probe_dec(di.SpriteResolution.y);
        }
        else
            probe_s(" (no DTAG_DISP)");
        if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, id) >= sizeof(struct QueryHeader))
        {
            probe_s(" depth=");
            probe_dec(dims.MaxDepth);
            probe_s(" raster=");
            probe_dec(dims.MinRasterWidth);
            probe_ch(',');
            probe_dec(dims.MinRasterHeight);
            probe_ch('-');
            probe_dec(dims.MaxRasterWidth);
            probe_ch(',');
            probe_dec(dims.MaxRasterHeight);
            rect("nom", &dims.Nominal);
            rect("txt", &dims.TxtOScan);
            rect("std", &dims.StdOScan);
            rect("max", &dims.MaxOScan);
            rect("vid", &dims.VideoOScan);
        }
        probe_ch('\n');
    }
    return 0;
}
