/*
 * Probe (Phase 222d): intuition.library OpenWindowTagList/OpenWindow/
 * CloseWindow - window borders for every system gadget combination,
 * WA_* tag effects (inner size, limits, titles, GZZ, borderless, backdrop,
 * zoom, auto-adjust), the system gadgets' fields and NewWindow defaults,
 * on the Workbench screen and on a custom public screen.
 */
#include <exec/memory.h>
#include <intuition/gadgetclass.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/diskfont_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *DiskfontBase;

static struct Screen *g_scr;

static void show(const char *name, struct Window *w)
{
    P_SECTION(name);
    if (!w)
    {
        probe_s("OpenWindow = NULL\n");
        return;
    }
    dump_window("", w);
    dump_glist("", w->FirstGadget);
    CloseWindow(w);
}

static WORD zoom[4] = { 10, 20, 150, 40 };

static void tag_variants(const char *pfx)
{
    char name[80];
    struct Screen *s = g_scr;
#define NAME(x) (strcpy(name, pfx), strcat(name, x), name)

    show(NAME("plain"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                          TAG_END));
    show(NAME("title"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                          WA_Title, (ULONG)"Probe", TAG_END));
    show(NAME("close"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                          WA_CloseGadget, TRUE, TAG_END));
    show(NAME("depth"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                          WA_DepthGadget, TRUE, TAG_END));
    show(NAME("drag"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                         WA_DragBar, TRUE, TAG_END));
    show(NAME("size"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                         WA_SizeGadget, TRUE, TAG_END));
    show(NAME("size bright"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                WA_Height, 80, WA_SizeGadget, TRUE, WA_SizeBRight, TRUE, TAG_END));
    show(NAME("size bbottom"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                 WA_Height, 80, WA_SizeGadget, TRUE, WA_SizeBBottom, TRUE, TAG_END));
    show(NAME("all"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                        WA_Title, (ULONG)"All", WA_CloseGadget, TRUE, WA_DepthGadget, TRUE,
                        WA_DragBar, TRUE, WA_SizeGadget, TRUE, WA_Activate, TRUE, TAG_END));
    show(NAME("all zoom"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                             WA_Title, (ULONG)"Zoom", WA_CloseGadget, TRUE, WA_DepthGadget, TRUE,
                             WA_DragBar, TRUE, WA_SizeGadget, TRUE, WA_Zoom, (ULONG)zoom, TAG_END));
    show(NAME("gzz"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                        WA_Title, (ULONG)"GZZ", WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
                        WA_GimmeZeroZero, TRUE, TAG_END));
    show(NAME("borderless"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                               WA_Height, 80, WA_Borderless, TRUE, TAG_END));
    show(NAME("borderless title"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                     WA_Height, 80, WA_Borderless, TRUE, WA_Title, (ULONG)"B",
                                     WA_DragBar, TRUE, TAG_END));
    show(NAME("backdrop"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Backdrop, TRUE, WA_Borderless, TRUE, TAG_END));
    show(NAME("inner size"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_InnerWidth, 100,
                               WA_InnerHeight, 50, WA_Title, (ULONG)"Inner", WA_SizeGadget, TRUE,
                               WA_CloseGadget, TRUE, TAG_END));
    show(NAME("limits"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200, WA_Height, 80,
                           WA_SizeGadget, TRUE, WA_MinWidth, 50, WA_MinHeight, 30, WA_MaxWidth, -1,
                           WA_MaxHeight, 150, TAG_END));
    show(NAME("limits clamp"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                 WA_Height, 80, WA_SizeGadget, TRUE, WA_MinWidth, 300, WA_MinHeight, 5,
                                 WA_MaxWidth, 100, WA_MaxHeight, 10, TAG_END));
    show(NAME("defaults"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, TAG_END));
    show(NAME("no top"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Width, 200, WA_Height, 80,
                           WA_Title, (ULONG)"NoTop", TAG_END));
    show(NAME("autoadjust"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 600, WA_Top, 200, WA_Width, 200,
                               WA_Height, 120, WA_AutoAdjust, TRUE, TAG_END));
    show(NAME("too big autoadjust"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 0, WA_Top, 0, WA_Width, 2000,
                                       WA_Height, 2000, WA_AutoAdjust, TRUE, TAG_END));
    show(NAME("idcmp simple refresh"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                         WA_Height, 80, WA_SimpleRefresh, TRUE, WA_NoCareRefresh, TRUE,
                                         WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_RAWKEY,
                                         WA_RMBTrap, TRUE, WA_ReportMouse, TRUE, TAG_END));
    show(NAME("screen title pens"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                      WA_Height, 80, WA_ScreenTitle, (ULONG)"ST", WA_DetailPen, 2,
                                      WA_BlockPen, 3, WA_DragBar, TRUE, TAG_END));
    show(NAME("newlookmenus"), OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 20, WA_Top, 30, WA_Width, 200,
                                 WA_Height, 80, WA_NewLookMenus, TRUE, WA_DragBar, TRUE, TAG_END));
#undef NAME
}

static void newwindow_variants(void)
{
    struct NewWindow nw;

    memset(&nw, 0, sizeof(nw));
    nw.LeftEdge = 10; nw.TopEdge = 20; nw.Width = 150; nw.Height = 60;
    nw.DetailPen = 0; nw.BlockPen = 1;
    nw.Type = WBENCHSCREEN;
    show("NewWindow plain", OpenWindow(&nw));

    nw.DetailPen = (UBYTE)-1; nw.BlockPen = (UBYTE)-1;
    nw.Flags = WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SIZEGADGET;
    nw.Title = (UBYTE *)"NW";
    nw.MinWidth = 0; nw.MinHeight = 0; nw.MaxWidth = 0; nw.MaxHeight = 0;
    show("NewWindow all, limits 0", OpenWindow(&nw));

    nw.MinWidth = 20; nw.MinHeight = 20; nw.MaxWidth = 400; nw.MaxHeight = 200;
    nw.Flags |= WFLG_ACTIVATE | WFLG_NOCAREREFRESH;
    nw.IDCMPFlags = IDCMP_CLOSEWINDOW;
    show("NewWindow limits", OpenWindow(&nw));

    nw.Title = NULL;
    nw.Flags = WFLG_SIZEGADGET | WFLG_SIZEBBOTTOM;
    show("NewWindow size bbottom no title", OpenWindow(&nw));
}

int main(void)
{
    struct TextAttr t11 = { (STRPTR)"topaz.font", 8, 0, 0 };
    struct TextFont *f11;
    struct Screen *cs;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    DiskfontBase = OpenLibrary((STRPTR)"diskfont.library", 37);
    if (!IntuitionBase || !GfxBase || !DiskfontBase)
        return 20;

    g_scr = LockPubScreen(NULL);
    if (!g_scr)
        return 20;
    P_LONG("wb WBorTop", g_scr->WBorTop);
    P_LONG("wb WBorLeft", g_scr->WBorLeft);
    P_LONG("wb WBorRight", g_scr->WBorRight);
    P_LONG("wb WBorBottom", g_scr->WBorBottom);
    P_LONG("wb BarHeight", g_scr->BarHeight);
    P_LONG("wb font ysize", g_scr->Font->ta_YSize);
    tag_variants("wb ");
    newwindow_variants();
    UnlockPubScreen(NULL, g_scr);

    f11 = OpenDiskFont(&t11);
    cs = OpenScreenTags(NULL, SA_Width, 640, SA_Height, 256, SA_Depth, 2, SA_DisplayID, 0x8000,
                        SA_Font, (ULONG)&t11, SA_Title, (ULONG)"Probe", SA_Pens, (ULONG)"\xff\xff",
                        SA_PubName, (ULONG)"PROBE.1", TAG_END);
    if (cs)
    {
        PubScreenStatus(cs, 0);
        g_scr = LockPubScreen((STRPTR)"PROBE.1");
        P_LONG("cs WBorTop", cs->WBorTop);
        P_LONG("cs BarHeight", cs->BarHeight);
        P_LONG("cs font ysize", cs->Font->ta_YSize);
        tag_variants("custom ");
        show("custom customscreen", OpenWindowTags(NULL, WA_CustomScreen, (ULONG)cs, WA_Left, 0, WA_Top, 0,
                                                     WA_Width, 100, WA_Height, 50, WA_Title, (ULONG)"C",
                                                     WA_CloseGadget, TRUE, TAG_END));
        UnlockPubScreen(NULL, g_scr);
        CloseScreen(cs);
    }
    else
        probe_s("OpenScreen failed\n");
    if (f11)
        CloseFont(f11);

    CloseLibrary(DiskfontBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
