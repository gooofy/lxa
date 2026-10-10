/*
 * Probe (Phase 244): Screen.DetailPen/BlockPen and the pens a window with
 * DetailPen/BlockPen ~0 inherits.  ReSource opens its screen with
 * OpenScreen() and its window with pens ~0; on AmigaOS 3.1 the window
 * draws with 1/2.
 */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;

static void win(const char *name, struct Screen *s, UBYTE dp, UBYTE bp)
{
    struct NewWindow nw = { 0, 1, 200, 50, dp, bp, 0, WFLG_BORDERLESS | WFLG_BACKDROP,
                            NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, CUSTOMSCREEN };
    struct Window *w;

    nw.Screen = s;
    w = OpenWindow(&nw);
    probe_s(name);
    if (!w)
    {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": detail ");
    probe_dec(w->DetailPen);
    probe_s(" block ");
    probe_dec(w->BlockPen);
    probe_ch('\n');
    CloseWindow(w);
}

static void scr(const char *name, UBYTE dp, UBYTE bp, UWORD modes, UWORD type)
{
    struct NewScreen ns = { 0, 0, 640, 252, 2, 0, 1, HIRES, CUSTOMSCREEN, NULL,
                            (UBYTE *)"", NULL, NULL };
    struct Screen *s;

    ns.DetailPen = dp;
    ns.BlockPen = bp;
    ns.ViewModes = modes;
    ns.Type = type;
    s = OpenScreen(&ns);
    probe_s(name);
    if (!s)
    {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": screen detail ");
    probe_dec(s->DetailPen);
    probe_s(" block ");
    probe_dec(s->BlockPen);
    probe_ch('\n');
    win("  window ~0", s, 0xFF, 0xFF);
    win("  window 0/1", s, 0, 1);
    win("  window 3/0", s, 3, 0);
    CloseScreen(s);
}

static UWORD pens_def[] = { (UWORD)~0 };
static UWORD pens_odd[] = { 2, 3, 1, 2, 1, 3, 1, 0, 2, (UWORD)~0 };

static void tagscr(const char *name, ULONG t1, ULONG d1, ULONG t2, ULONG d2)
{
    struct Screen *s = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 252, SA_Depth, 2,
                                      SA_DisplayID, 0x8000, SA_Title, (ULONG)"", t1, d1, t2, d2, TAG_END);
    struct Window *w;

    probe_s(name);
    if (!s)
    {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": screen detail ");
    probe_dec(s->DetailPen);
    probe_s(" block ");
    probe_dec(s->BlockPen);
    probe_ch('\n');
    w = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)s, WA_Top, 1, WA_Width, 200, WA_Height, 50,
                       WA_Borderless, TRUE, WA_Backdrop, TRUE, TAG_END);
    if (w)
    {
        probe_s("  tag window: detail ");
        probe_dec(w->DetailPen);
        probe_s(" block ");
        probe_dec(w->BlockPen);
        probe_ch('\n');
        CloseWindow(w);
    }
    win("  window ~0", s, 0xFF, 0xFF);
    CloseScreen(s);
}

int main(void)
{
    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    if (!IntuitionBase)
        return 0;
    scr("ns 0/1", 0, 1, HIRES, CUSTOMSCREEN);
    scr("ns 1/2", 1, 2, HIRES, CUSTOMSCREEN);
    scr("ns 0/0", 0, 0, HIRES, CUSTOMSCREEN);
    scr("ns ~0/~0", 0xFF, 0xFF, HIRES, CUSTOMSCREEN);
    tagscr("tags", TAG_IGNORE, 0, TAG_IGNORE, 0);
    tagscr("tags pens default", SA_Pens, (ULONG)pens_def, TAG_IGNORE, 0);
    tagscr("tags pens odd", SA_Pens, (ULONG)pens_odd, TAG_IGNORE, 0);
    tagscr("tags detail 1 block 2", SA_DetailPen, 1, SA_BlockPen, 2);
    tagscr("tags detail 1 pens", SA_DetailPen, 1, SA_Pens, (ULONG)pens_def);
    CloseLibrary(IntuitionBase);
    return 0;
}
