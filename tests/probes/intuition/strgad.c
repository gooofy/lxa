/*
 * Probe (Phase 222d): intuition.library string gadget rendering -
 * AddGList, RefreshGList and ActivateGadget on plain (non-GadTools) string
 * gadgets whose buffer the application changes after AddGList (NumChars
 * then stale, as DirectoryOpus does).  Prints the StringInfo fields and,
 * independent of the font's glyph shapes (Phase 225), which 8 pixel cells
 * of the gadget hold text, the pens used and the first/last lit row.
 */
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct Window *g_win;
static WORD border_xy[] = { -2, -2, 161, -2, 161, 11, -2, 11, -2, -2 };
static struct Border border = { 0, 0, 1, 0, JAM1, 5, border_xy, NULL };

static void scan(struct Gadget *g)
{
    struct RastPort *rp = g_win->RPort;
    UBYTE pens[256];
    WORD x, y, first = -1, last = -1, outside = 0;
    int i, cells = (g->Width + 7) / 8;

    memset(pens, 0, sizeof(pens));
    probe_s("cells=");
    for (i = 0; i < cells; i++)
    {
        BOOL lit = FALSE;
        for (y = 0; y < g->Height; y++)
            for (x = i * 8; x < i * 8 + 8 && x < g->Width; x++)
            {
                LONG p = ReadPixel(rp, g->LeftEdge + x, g->TopEdge + y);
                if (p > 0)
                {
                    lit = TRUE;
                    pens[p & 255] = 1;
                    if (first < 0 || y < first)
                        first = y;
                    if (y > last)
                        last = y;
                }
            }
        probe_ch(lit ? '#' : '.');
    }
    for (y = -3; y < g->Height + 3; y++)
        for (x = -4; x < g->Width + 4; x++)
            if ((y < 0 || y >= g->Height || x < 0 || x >= g->Width) &&
                ReadPixel(rp, g->LeftEdge + x, g->TopEdge + y) > 0)
                outside++;
    probe_s(" pens=");
    for (i = 0; i < 256; i++)
        if (pens[i])
        {
            probe_dec(i);
            probe_ch(' ');
        }
    ps_kv("rows", first);
    ps_kv("to", last);
    ps_kv("outside", outside);
    probe_ch('\n');
}

static void show(const char *name, UWORD flags, UWORD act, LONG disp, BOOL with_border, BOOL activate)
{
    struct Gadget g;
    struct StringInfo si;
    UBYTE buf[64], undo[64];

    P_SECTION(name);
    memset(&g, 0, sizeof(g));
    memset(&si, 0, sizeof(si));
    memset(buf, 0, sizeof(buf));
    strcpy((char *)buf, "abc");
    si.Buffer = buf;
    si.UndoBuffer = undo;
    si.MaxChars = sizeof(buf);
    g.LeftEdge = 24;
    g.TopEdge = 30;
    g.Width = 160;
    g.Height = 10;
    g.Flags = GFLG_GADGHCOMP | flags;
    g.Activation = GACT_RELVERIFY | act;
    g.GadgetType = GTYP_STRGADGET;
    g.SpecialInfo = &si;
    g.GadgetID = 1;
    if (with_border)
        g.GadgetRender = &border;

    /* clear the area */
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, 8, 20, 200, 50);

    AddGList(g_win, &g, -1, 1, NULL);
    P_LONG("after AddGList NumChars", si.NumChars);
    P_LONG("after AddGList BufferPos", si.BufferPos);
    P_LONG("after AddGList DispPos", si.DispPos);

    /* the application changes the buffer behind Intuition's back */
    strcpy((char *)buf, "SYS:x/");
    si.DispPos = disp;
    RefreshGList(&g, g_win, NULL, 1);
    if (activate)
        P_BOOL("ActivateGadget", ActivateGadget(&g, g_win, NULL));
    Delay(5);
    P_LONG("NumChars", si.NumChars);
    P_LONG("BufferPos", si.BufferPos);
    P_LONG("DispPos", si.DispPos);
    P_HEX("Flags", g.Flags);
    scan(&g);

    RemoveGList(g_win, &g, 1);
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, 8, 20, 200, 50);
}

int main(void)
{
    struct Screen *s;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen(NULL);
    if (!s)
        return 20;
    g_win = OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 0, WA_Top, 20, WA_Width, 240,
                           WA_Height, 70, WA_Title, (ULONG)"StrGad", WA_DragBar, TRUE, WA_Activate, TRUE,
                           WA_IDCMP, IDCMP_GADGETUP, TAG_END);
    if (!g_win)
        return 20;
    Delay(10);

    show("plain", 0, 0, 0, FALSE, FALSE);
    show("disppos 2", 0, 0, 2, FALSE, FALSE);
    show("center", 0, GACT_STRINGCENTER, 0, FALSE, FALSE);
    show("right", 0, GACT_STRINGRIGHT, 0, FALSE, FALSE);
    show("border", 0, 0, 0, TRUE, FALSE);
    show("selected flag", GFLG_SELECTED, 0, 0, FALSE, FALSE);
    show("activated", 0, 0, 0, FALSE, TRUE);

    CloseWindow(g_win);
    UnlockPubScreen(NULL, s);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
