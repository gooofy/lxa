/*
 * Probe (Phase 246): the 256 colours of a fresh ColorMap (GetColorMap(256))
 * and of 8 bit screens - the Workbench of the rtg profile, an RTG screen
 * (plain and SA_LikeWorkbench) and an AGA screen: pens 32-251 keep
 * graphics.library's default colours.
 *
 * rdd: profile rtg
 */
#include <exec/types.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include "probe.h"

struct IntuitionBase *IntuitionBase;

static void dump(const char *l, struct ColorMap *cm, int n)
{
    ULONG c[3];
    int i;
    probe_s(l); probe_ch('\n');
    for (i = 0; i < n; i++)
    {
        GetRGB32(cm, i, 1, c);
        probe_hex(c[0] >> 24, 2); probe_hex(c[1] >> 24, 2); probe_hex(c[2] >> 24, 2);
        probe_ch((i & 7) == 7 ? '\n' : ' ');
    }
}

int main(void)
{
    struct Screen *s;
    struct ColorMap *cm;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    s = LockPubScreen(NULL);
    if (s) { dump("wb", s->ViewPort.ColorMap, 256); UnlockPubScreen(NULL, s); }
    s = OpenScreenTags(NULL, SA_DisplayID, 0x50031303, SA_Depth, 8, SA_Width, 640, SA_Height, 480, TAG_END);
    if (s) { dump("rtg8", s->ViewPort.ColorMap, 256); CloseScreen(s); }
    s = OpenScreenTags(NULL, SA_DisplayID, 0x50031303, SA_Depth, 8, SA_Width, 640, SA_Height, 480, SA_LikeWorkbench, TRUE, TAG_END);
    if (s) { dump("rtg8like", s->ViewPort.ColorMap, 256); CloseScreen(s); }
    s = OpenScreenTags(NULL, SA_DisplayID, 0x00029000, SA_Depth, 8, SA_Width, 640, SA_Height, 256, TAG_END);
    if (s) { dump("aga8", s->ViewPort.ColorMap, 256); CloseScreen(s); }
    cm = GetColorMap(256);
    if (cm) { dump("getcm", cm, 256); FreeColorMap(cm); }
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
