/*
 * Probe (Phase 246): RTG screens in depth order (Picasso96 2.0, rtg
 * profile).  The board shows only its front screen: every other RTG screen
 * has VP_HIDE in ViewPort.Modes and the private Screen.Flags bit 0x2000; a
 * screen opened behind has DHeight 0 until it first comes to the front.
 * Covers OpenScreen (in front and SA_Behind), ScreenToBack, ScreenToFront,
 * ScreenDepth and CloseScreen, with the Workbench as the other screen.
 *
 * rdd: profile rtg
 */
#include <exec/types.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include "probe.h"

struct IntuitionBase *IntuitionBase;

static void show(const char *l, struct Screen *s)
{
    probe_s(l);
    probe_s(": flags = "); probe_hex(s->Flags, 4);
    probe_s(" modes = "); probe_hex(s->ViewPort.Modes, 4);
    probe_s(" dheight = "); probe_dec(s->ViewPort.DHeight);
    probe_ch('\n');
}

static void wb(const char *l)
{
    struct Screen *s = LockPubScreen(NULL);
    if (s)
    {
        show(l, s);
        UnlockPubScreen(NULL, s);
    }
}

int main(void)
{
    struct Screen *a, *b;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    wb("wb alone");
    a = OpenScreenTags(NULL, SA_DisplayID, 0x50031303, SA_Depth, 8, SA_Width, 640, SA_Height, 480, TAG_END);
    P_NULL("a", a);
    if (a)
    {
        show("a front", a);
        wb("wb under a");
        ScreenToBack(a);
        show("a back", a);
        wb("wb over a");
        ScreenToFront(a);
        show("a front again", a);
        wb("wb under a again");
        ScreenDepth(a, SDEPTH_TOBACK, NULL);
        show("a depth back", a);
        ScreenDepth(a, SDEPTH_TOFRONT, NULL);
        show("a depth front", a);
        b = OpenScreenTags(NULL, SA_DisplayID, 0x50031000, SA_Depth, 8, SA_Width, 320, SA_Height, 240,
                           SA_Behind, TRUE, TAG_END);
        P_NULL("b", b);
        if (b)
        {
            show("b behind", b);
            show("a over b", a);
            ScreenToFront(b);
            show("b front", b);
            show("a under b", a);
            wb("wb under b");
            ScreenToBack(b);
            show("b back", b);
            CloseScreen(b);
        }
        show("a after b", a);
        CloseScreen(a);
    }
    wb("wb at end");
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
