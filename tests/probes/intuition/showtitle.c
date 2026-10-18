/*
 * Probe (Phase 243): when does Intuition repaint a screen's title bar?
 * Personal Paint draws its own title ("Cloanto Personal Paint ...") into the
 * bar of its screen and then calls ShowTitle(screen, TRUE); on AmigaOS 3.1
 * its text stays.  A mark is drawn into the bar through Screen.RastPort,
 * then the calls that might repaint the bar are made and the mark's pixel
 * is read back.
 */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct Library *IntuitionBase;
struct GfxBase *GfxBase;

static struct Screen *scr;
static UWORD pens[] = { (UWORD)~0 };

static void mark(void)
{
    SetAPen(&scr->RastPort, 3);
    SetDrMd(&scr->RastPort, JAM1);
    RectFill(&scr->RastPort, 100, 2, 140, 6);
}

static void barmark(void)
{
    struct RastPort *rp = scr->BarLayer ? scr->BarLayer->rp : NULL;
    if (!rp)
        return;
    SetAPen(rp, 3);
    SetDrMd(rp, JAM1);
    RectFill(rp, 150, 2, 190, 6);
}

static void show(const char *label)
{
    probe_s(label);
    probe_s(": mark ");
    probe_dec(ReadPixel(&scr->RastPort, 120, 4));
    probe_s(" barlayer ");
    probe_dec(ReadPixel(&scr->RastPort, 170, 4));
    probe_s(" bar ");
    probe_dec(ReadPixel(&scr->RastPort, 300, 4));
    probe_s(" below ");
    probe_dec(ReadPixel(&scr->RastPort, 120, 20));
    probe_ch('\n');
}

int main(void)
{
    struct Window *bd, *w;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 0;
    scr = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 200, SA_Depth, 2,
                         SA_DisplayID, 0x8000, SA_Title, (ULONG)"Probe", SA_ShowTitle, TRUE,
                         SA_Pens, (ULONG)pens, TAG_END);
    if (!scr)
    {
        probe_s("screen: NULL\n");
        return 0;
    }
    P_HEX("flags & SHOWTITLE", scr->Flags & SHOWTITLE);
    P_NULL("BarLayer", scr->BarLayer);
    Delay(5);
    show("open");
    mark();
    barmark();
    show("marked");
    ShowTitle(scr, TRUE);
    Delay(5);
    show("ShowTitle TRUE (shown)");
    ShowTitle(scr, FALSE);
    Delay(5);
    show("ShowTitle FALSE, no backdrop");
    P_HEX("flags & SHOWTITLE", scr->Flags & SHOWTITLE);
    ShowTitle(scr, TRUE);
    Delay(5);
    show("ShowTitle TRUE again");

    bd = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 0, WA_Top, 0, WA_Width, 640,
                        WA_Height, 200, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
                        WA_RMBTrap, TRUE, TAG_END);
    if (!bd)
    {
        probe_s("backdrop: NULL\n");
        CloseScreen(scr);
        return 0;
    }
    Delay(10);
    show("backdrop opened");
    SetAPen(bd->RPort, 1);
    RectFill(bd->RPort, 0, 0, 639, 199);
    Delay(5);
    show("backdrop filled");
    mark();
    barmark();
    show("marked");
    ActivateWindow(bd);
    Delay(10);
    show("ActivateWindow (active)");
    ShowTitle(scr, TRUE);
    Delay(5);
    show("ShowTitle TRUE (shown)");
    ShowTitle(scr, FALSE);
    Delay(5);
    show("ShowTitle FALSE");
    ShowTitle(scr, TRUE);
    Delay(5);
    show("ShowTitle TRUE");
    barmark();
    show("barlayer marked");
    mark();
    SetWindowTitles(bd, (UBYTE *)~0, (UBYTE *)~0);
    Delay(5);
    show("SetWindowTitles -1 -1");

    w = OpenWindowTags(NULL, WA_CustomScreen, (ULONG)scr, WA_Left, 200, WA_Top, 60, WA_Width, 200,
                       WA_Height, 60, WA_Title, (ULONG)"W", WA_DragBar, TRUE, WA_Activate, TRUE,
                       TAG_END);
    Delay(10);
    show("window opened (activated)");
    mark();
    if (w)
    {
        ActivateWindow(bd);
        Delay(10);
        show("backdrop activated");
        mark();
        barmark();
        CloseWindow(w);
        Delay(10);
        show("window closed");
    }
    CloseWindow(bd);
    CloseScreen(scr);
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
