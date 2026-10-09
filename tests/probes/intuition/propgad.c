/*
 * Probe (Phase 234): intuition.library proportional gadget rendering -
 * AUTOKNOB prop gadgets with and without PROPNEWLOOK / PROPBORDERLESS,
 * FREEHORIZ / FREEVERT, several Pot/Body values, AddGList + RefreshGList
 * and NewModifyProp (as BlitzBasic 2's editor uses them).  Prints the
 * PropInfo container fields and the knob Image Intuition fills in, and the
 * pens of the gadget box plus a 2 pixel margin, run-length encoded per
 * row (identical consecutive rows collapsed).
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

#define GX 20
#define GY 20

static void row_rle(WORD y, WORD x0, WORD x1, char *out)
{
    struct RastPort *rp = g_win->RPort;
    LONG cur = -2, n = 0;
    WORD x;
    int len = 0;

    for (x = x0; x <= x1 + 1; x++)
    {
        LONG p = x <= x1 ? ReadPixel(rp, x, y) : -3;
        if (p == cur)
        {
            n++;
            continue;
        }
        if (cur != -2)
        {
            /* "<pen>x<count> " */
            char t[12];
            int k = 0;
            LONG v = n;
            out[len++] = (char)('0' + (cur & 15));
            out[len++] = 'x';
            do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (k)
                out[len++] = t[--k];
            out[len++] = ' ';
        }
        cur = p;
        n = 1;
    }
    out[len] = 0;
}

static void dump_pixels(struct Gadget *g)
{
    static char prev[512], cur[512];
    WORD y, y0 = g->TopEdge - 2, y1 = g->TopEdge + g->Height + 1, first = y0;

    prev[0] = 0;
    for (y = y0; y <= y1 + 1; y++)
    {
        if (y <= y1)
            row_rle(y, g->LeftEdge - 2, g->LeftEdge + g->Width + 1, cur);
        if (y > y0 && (y > y1 || strcmp(cur, prev)))
        {
            probe_s("  y");
            probe_dec(first - g->TopEdge);
            if (y - 1 != first)
            {
                probe_ch('-');
                probe_dec(y - 1 - g->TopEdge);
            }
            probe_s(": ");
            probe_s(prev);
            probe_ch('\n');
            first = y;
        }
        strcpy(prev, cur);
    }
}

static void dump_prop(struct Gadget *g, struct PropInfo *pi, struct Image *im)
{
    probe_s("  propinfo");
    ps_kx("flags", pi->Flags, 4);
    ps_kx("hpot", pi->HorizPot, 4);
    ps_kx("vpot", pi->VertPot, 4);
    ps_kx("hbody", pi->HorizBody, 4);
    ps_kx("vbody", pi->VertBody, 4);
    ps_kv("cw", pi->CWidth);
    ps_kv("ch", pi->CHeight);
    ps_kv("lb", pi->LeftBorder);
    ps_kv("tb", pi->TopBorder);
    probe_ch('\n');
    probe_s("  knob");
    ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
    probe_ch('\n');
    dump_pixels(g);
}

static void show(const char *name, UWORD flags, WORD w, WORD h,
                 UWORD hpot, UWORD vpot, UWORD hbody, UWORD vbody, BOOL modify)
{
    struct Gadget g;
    struct PropInfo pi;
    struct Image im;

    P_SECTION(name);
    memset(&g, 0, sizeof(g));
    memset(&pi, 0, sizeof(pi));
    memset(&im, 0, sizeof(im));
    pi.Flags = flags;
    if (modify)
    {
        pi.HorizPot = 0;
        pi.VertPot = 0;
        pi.HorizBody = MAXBODY;
        pi.VertBody = MAXBODY;
    }
    else
    {
        pi.HorizPot = hpot;
        pi.VertPot = vpot;
        pi.HorizBody = hbody;
        pi.VertBody = vbody;
    }
    g.LeftEdge = GX;
    g.TopEdge = GY;
    g.Width = w;
    g.Height = h;
    g.Flags = GFLG_GADGHCOMP | GFLG_GADGIMAGE;
    g.Activation = GACT_RELVERIFY | GACT_IMMEDIATE;
    g.GadgetType = GTYP_PROPGADGET;
    g.GadgetRender = &im;
    g.SpecialInfo = &pi;
    g.GadgetID = 1;

    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, 8, 12, 150, 90);

    AddGList(g_win, &g, -1, 1, NULL);
    RefreshGList(&g, g_win, NULL, 1);
    if (modify)
        NewModifyProp(&g, g_win, NULL, flags, hpot, vpot, hbody, vbody, 1);
    Delay(2);
    dump_prop(&g, &pi, &im);

    RemoveGList(g_win, &g, 1);
    SetAPen(g_win->RPort, 0);
    RectFill(g_win->RPort, 8, 12, 150, 90);
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
    g_win = OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 0, WA_Top, 20, WA_Width, 200,
                           WA_Height, 100, WA_Title, (ULONG)"PropGad", WA_DragBar, TRUE,
                           WA_Activate, TRUE, WA_IDCMP, IDCMP_GADGETUP, TAG_END);
    if (!g_win)
        return 20;
    Delay(10);

    /* vertical, 12x30 */
    show("vert full", AUTOKNOB | FREEVERT, 12, 30, 0, 0, MAXBODY, MAXBODY, FALSE);
    show("vert half top", AUTOKNOB | FREEVERT, 12, 30, 0, 0, MAXBODY, 0x8000, FALSE);
    show("vert half bottom", AUTOKNOB | FREEVERT, 12, 30, 0, MAXPOT, MAXBODY, 0x8000, FALSE);
    show("vert borderless full", AUTOKNOB | FREEVERT | PROPBORDERLESS, 12, 30, 0, 0, MAXBODY, MAXBODY, FALSE);
    show("vert borderless half", AUTOKNOB | FREEVERT | PROPBORDERLESS, 12, 30, 0, MAXPOT, MAXBODY, 0x8000, FALSE);
    show("vert newlook full", AUTOKNOB | FREEVERT | PROPNEWLOOK, 12, 30, 0, 0, MAXBODY, MAXBODY, FALSE);
    show("vert newlook half", AUTOKNOB | FREEVERT | PROPNEWLOOK, 12, 30, 0, 0x8000, MAXBODY, 0x4000, FALSE);
    show("vert newlook borderless half", AUTOKNOB | FREEVERT | PROPNEWLOOK | PROPBORDERLESS, 12, 30,
         0, MAXPOT, MAXBODY, 0x8000, FALSE);
    /* horizontal, 60x8 */
    show("horiz half", AUTOKNOB | FREEHORIZ, 60, 8, 0, 0, 0x8000, MAXBODY, FALSE);
    show("horiz borderless half", AUTOKNOB | FREEHORIZ | PROPBORDERLESS, 60, 8, MAXPOT, 0, 0x8000, MAXBODY, FALSE);
    show("horiz newlook half", AUTOKNOB | FREEHORIZ | PROPNEWLOOK, 60, 8, 0x8000, 0, 0x4000, MAXBODY, FALSE);
    show("horiz tiny body", AUTOKNOB | FREEHORIZ, 60, 8, 0, 0, 0x0100, MAXBODY, FALSE);
    /* both directions */
    show("both", AUTOKNOB | FREEHORIZ | FREEVERT, 40, 30, 0x8000, 0x8000, 0x8000, 0x8000, FALSE);
    show("both borderless newlook", AUTOKNOB | FREEHORIZ | FREEVERT | PROPBORDERLESS | PROPNEWLOOK,
         40, 30, MAXPOT, 0, 0x4000, 0x8000, FALSE);
    /* BlitzBasic 2's editor scrollers: NewModifyProp after AddGList */
    show("modify vert borderless", AUTOKNOB | FREEVERT | PROPBORDERLESS, 10, 40, 0xffff, 0xffff, 0xffff, 0xffff, TRUE);
    show("modify horiz borderless", AUTOKNOB | FREEHORIZ | PROPBORDERLESS, 80, 4, 0, 0xffff, 0x9c00, 0xffff, TRUE);
    show("modify horiz borderless pot", AUTOKNOB | FREEHORIZ | PROPBORDERLESS, 80, 4, 0x4024, 0xffff, 0x9c00, 0xffff, TRUE);

    CloseWindow(g_win);
    UnlockPubScreen(NULL, s);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
