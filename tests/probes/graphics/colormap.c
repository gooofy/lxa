/*
 * colormap - the default colours of a fresh ColorMap and the overscan
 * rectangles of the display database (Phase 236, prefs defaults).
 *
 * GetColorMap() fills the colour table with the system's default palette;
 * QueryOverscan() and DTAG_DIMS report the text/standard/max/video overscan
 * that Workbench screens and STDSCREENWIDTH screens are sized from.
 */
#include "gfxprobe.h"
#include <graphics/view.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/intuition_protos.h>
#include <inline/intuition.h>

struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static void p_rect(const char *label, const struct Rectangle *r)
{
    probe_s(label);
    probe_s(" = ");
    probe_dec(r->MinX); probe_ch(',');
    probe_dec(r->MinY); probe_ch(' ');
    probe_dec(r->MaxX); probe_ch(',');
    probe_dec(r->MaxY); probe_ch('\n');
}

static void p_mode(ULONG id)
{
    struct DimensionInfo dims;
    struct Rectangle r;
    WORD t;
    static const char *names[] = { "", "text", "standard", "max", "video" };
    char label[48];

    P_HEX("mode", id);
    if (GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof(dims), DTAG_DIMS, id)) {
        p_rect("  dims.txt", &dims.TxtOScan);
        p_rect("  dims.std", &dims.StdOScan);
        p_rect("  dims.max", &dims.MaxOScan);
        p_rect("  dims.video", &dims.VideoOScan);
    } else
        probe_s("  dims = none\n");
    for (t = OSCAN_TEXT; t <= OSCAN_VIDEO; t++) {
        char *p = label;
        const char *s = "  query.";
        while (*s) *p++ = *s++;
        s = names[t];
        while (*s) *p++ = *s++;
        *p = 0;
        if (QueryOverscan(id, &r, t))
            p_rect(label, &r);
        else
            P_LONG(label, 0);
    }
}

int main(void)
{
    struct ColorMap *cm;
    ULONG rgb[3];
    LONG i;
    char label[16];

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!GfxBase || !IntuitionBase)
        return 20;

    P_SECTION("GetColorMap(32)");
    cm = GetColorMap(32);
    if (cm) {
        for (i = 0; i < 32; i++) {
            label[0] = 'c';
            label[1] = (char)('0' + i / 10);
            label[2] = (char)('0' + i % 10);
            label[3] = 0;
            GetRGB32(cm, i, 1, rgb);
            P_HEX(label, ((rgb[0] >> 8) & 0xff0000) | ((rgb[1] >> 16) & 0xff00) | (rgb[2] >> 24));
        }
        FreeColorMap(cm);
    }

    P_SECTION("overscan");
    p_mode(HIRES_KEY);
    p_mode(PAL_MONITOR_ID | HIRES_KEY);
    p_mode(PAL_MONITOR_ID | LORES_KEY);
    p_mode(PAL_MONITOR_ID | HIRESLACE_KEY);
    p_mode(PAL_MONITOR_ID | SUPER_KEY);
    p_mode(NTSC_MONITOR_ID | HIRES_KEY);
    p_mode(LORES_KEY);

    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
