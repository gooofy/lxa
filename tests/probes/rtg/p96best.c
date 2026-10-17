/*
 * Probe (Phase 246): p96BestModeIDTagList on the uaegfx board (Picasso96
 * 2.0, rtg profile) - a grid of nominal sizes for each depth, nominal
 * width or height alone.  Each query prints the resolution byte of the
 * mode ID (bits 16..23, "--" for no mode, "pp" for the P96Mode entry)
 * and the format suffix once per row when it changes; the same grids for
 * cybergraphics.library BestCModeIDTagList.
 *
 * rdd: profile rtg
 */
#define LXA_P96_INLINES
#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/displayinfo.h>
#include <clib/exec_protos.h>
#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <lxa_p96.h>
#include "probe.h"

struct Library *P96Base = NULL;
struct Library *CyberGfxBase = NULL;
static int use_cgx;

static ULONG query(LONG w, LONG h, LONG d)
{
    struct TagItem t[4];
    int n = 0;
    if (use_cgx) {
        if (w >= 0) { t[n].ti_Tag = CYBRBIDTG_NominalWidth; t[n].ti_Data = w; n++; }
        if (h >= 0) { t[n].ti_Tag = CYBRBIDTG_NominalHeight; t[n].ti_Data = h; n++; }
        if (d >= 0) { t[n].ti_Tag = CYBRBIDTG_Depth; t[n].ti_Data = d; n++; }
        t[n].ti_Tag = TAG_DONE;
        return BestCModeIDTagList(t);
    }
    if (w >= 0) { t[n].ti_Tag = P96BIDTAG_NominalWidth; t[n].ti_Data = w; n++; }
    if (h >= 0) { t[n].ti_Tag = P96BIDTAG_NominalHeight; t[n].ti_Data = h; n++; }
    if (d >= 0) { t[n].ti_Tag = P96BIDTAG_Depth; t[n].ti_Data = d; n++; }
    t[n].ti_Tag = TAG_DONE;
    return p96BestModeIDTagList(t);
}

static void cell(ULONG id, ULONG *fmt)
{
    static const char hx[] = "0123456789abcdef";
    if (id == INVALID_ID) { probe_s("--"); return; }
    if ((id >> 16) == 0xfff0) { probe_s("pp"); }
    else { probe_ch(hx[(id >> 20) & 15]); probe_ch(hx[(id >> 16) & 15]); }
    if (*fmt != (id & 0xffff)) { *fmt = id & 0xffff; probe_ch('/'); probe_hex(*fmt, 4); }
}

static void grid(LONG d, LONG w0, LONG w1, LONG ws, LONG h0, LONG h1, LONG hs)
{
    LONG w, h;
    probe_s("-- grid depth "); probe_dec(d); probe_s(" w "); probe_dec(w0); probe_ch('-'); probe_dec(w1);
    probe_ch('/'); probe_dec(ws); probe_s(" h "); probe_dec(h0); probe_ch('-'); probe_dec(h1);
    probe_ch('/'); probe_dec(hs); probe_ch('\n');
    for (h = h0; h <= h1; h += hs) {
        ULONG fmt = 0;
        probe_dec(h); probe_ch(':');
        for (w = w0; w <= w1; w += ws) { probe_ch(' '); cell(query(w, h, d), &fmt); }
        probe_ch('\n');
    }
}

int main(void)
{
    LONG v, d;
    P96Base = OpenLibrary(P96NAME, 2);
    if (!P96Base) return 0;

    grid(8, 25, 1500, 25, 25, 1200, 25);
    grid(16, 100, 1500, 50, 100, 1200, 50);
    grid(24, 100, 1500, 50, 100, 1200, 50);
    {
        ULONG fmt = 0;
        probe_s("-- width only\n");
        for (v = 25; v <= 2000; v += 25) { probe_ch(' '); cell(query(v, -1, 8), &fmt); }
        probe_ch('\n');
        fmt = 0;
        probe_s("-- height only\n");
        for (v = 25; v <= 2000; v += 25) { probe_ch(' '); cell(query(-1, v, 8), &fmt); }
        probe_ch('\n');
        fmt = 0;
        probe_s("-- depth only\n");
        for (d = 1; d <= 33; d++) { probe_ch(' '); cell(query(-1, -1, d), &fmt); }
        probe_ch('\n');
    }
    CyberGfxBase = OpenLibrary(CYBERGFXNAME, 40);
    if (CyberGfxBase) {
        ULONG fmt = 0;
        use_cgx = 1;
        probe_s("-- cgx\n");
        grid(8, 50, 1500, 50, 50, 1200, 50);
        grid(16, 100, 1500, 100, 100, 1200, 100);
        probe_s("-- cgx width only\n");
        for (v = 50; v <= 2000; v += 50) { probe_ch(' '); cell(query(v, -1, 8), &fmt); }
        probe_ch('\n');
        fmt = 0;
        probe_s("-- cgx height only\n");
        for (v = 50; v <= 2000; v += 50) { probe_ch(' '); cell(query(-1, v, 8), &fmt); }
        probe_ch('\n');
        fmt = 0;
        probe_s("-- cgx depth only\n");
        for (d = 1; d <= 33; d++) { probe_ch(' '); cell(query(-1, -1, d), &fmt); }
        probe_ch('\n');
        CloseLibrary(CyberGfxBase);
    }
    CloseLibrary(P96Base);
    return 0;
}
