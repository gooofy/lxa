/*
 * Probe (Phase 252): the gadgets of the system requesters.  AmigaOS 3.1
 * builds BuildSysRequest/BuildEasyRequestArgs buttons as frbuttonclass
 * objects whose GadgetText is an IntuiText (for BuildSysRequest the
 * caller's pos/neg IntuiText) and appends an empty gadget.  Also: the
 * body measured with the body IntuiText's ITextFont, and where the body
 * text lands in the window (bounding box of the non-background pixels
 * above the buttons).
 */
#include <exec/memory.h>
#include <graphics/text.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
extern struct DosLibrary *DOSBase;

static struct TextAttr topaz9 = { (STRPTR)"topaz.font", 9, 0, 0 };
static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };

/* bounding box of the pixels that differ from the body background (the
 * pen inside the recessed text box) within that box; the box ends three
 * rows above the buttons at row maxy */
static void textbox(struct Window *w, LONG maxy)
{
    LONG x, y, x0 = 9999, y0 = 9999, x1 = -1, y1 = -1;
    LONG bg = ReadPixel(w->RPort, w->BorderLeft + 6, w->BorderTop + 4);
    for (y = w->BorderTop + 3; y < maxy - 4; y++)
        for (x = w->BorderLeft + 5; x < w->Width - w->BorderRight - 6; x++)
            if (ReadPixel(w->RPort, x, y) != bg)
            {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    probe_s("  body");
    ps_kv("bg", bg);
    if (x1 < 0)
        probe_s(" empty");
    else
        ps_box(x0, y0, x1 - x0 + 1, y1 - y0 + 1);
    probe_ch('\n');
}

static void show(const char *name, struct Window *w, struct IntuiText *pos, struct IntuiText *neg)
{
    struct Gadget *g;
    LONG top = 9999;
    int n = 0;

    P_SECTION(name);
    if (!w || (ULONG)w == 1)
    {
        probe_s("no window\n");
        return;
    }
    Delay(5);
    dump_window("", w);
    for (g = w->FirstGadget; g; g = g->NextGadget, n++)
    {
        if (g->GadgetType & GTYP_SYSGADGET)
            continue;
        dump_gadget("  ", n, g);
        if (g->GadgetText)
            probe_s(g->GadgetText == pos ? "    text=pos\n" : g->GadgetText == neg ? "    text=neg\n" : "    text=own\n");
        if (g->Height > 0 && g->TopEdge < top)
            top = g->TopEdge;
    }
    textbox(w, top == 9999 ? w->Height - w->BorderBottom : top);
}

/* what the trailing gadget's image draws: clear the inner window to pen
 * 3, refresh only that gadget and print one character per 4x2 cell */
static void bgimage(struct Window *w)
{
    struct Gadget *g = w->FirstGadget;
    LONG x, y;
    while (g && g->NextGadget)
        g = g->NextGadget;
    if (!g || g->GadgetType != 0)
        return;
    SetAPen(w->RPort, 3);
    RectFill(w->RPort, w->BorderLeft, w->BorderTop, w->Width - w->BorderRight - 1,
             w->Height - w->BorderBottom - 1);
    RefreshGList(g, w, NULL, 1);
    probe_s("  bgimage\n");
    for (y = 0; y < w->Height; y += 2)
    {
        probe_s("   ");
        for (x = 0; x < w->Width; x += 4)
            probe_ch((char)('0' + ReadPixel(w->RPort, x, y)));
        probe_ch('\n');
    }
}

int main(void)
{
    struct IntuiText body3[3] = {
        { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"First line of the body", &body3[1] },
        { 0, 1, JAM2, 6, 13, NULL, (UBYTE *)"Second", &body3[2] },
        { 0, 1, JAM2, 6, 23, NULL, (UBYTE *)"Third line", NULL } };
    struct IntuiText body9 = { 0, 1, JAM2, 6, 3, &topaz9, (UBYTE *)"A body text in topaz nine", NULL };
    struct IntuiText body1 = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"Only a body", NULL };
    struct IntuiText pos = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"Retry", NULL };
    struct IntuiText neg = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"Cancel", NULL };
    struct IntuiText pos9 = { 0, 1, JAM2, 6, 3, &topaz9, (UBYTE *)"Okay", NULL };
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Probe",
                             (UBYTE *)"Easy body %ld\nsecond line", (UBYTE *)"One|Two|Three" };
    struct EasyStruct es1 = { sizeof(struct EasyStruct), 0, (UBYTE *)"Probe", (UBYTE *)"Single", (UBYTE *)"Ok" };
    struct Window *w;
    LONG arg = 42;

    (void)topaz8;
    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;

    w = BuildSysRequest(NULL, &body1, &pos, &neg, 0, 300, 50);
    show("sys body1 pos neg", w, &pos, &neg);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    w = BuildSysRequest(NULL, &body1, NULL, &neg, 0, 300, 50);
    show("sys body1 neg only", w, &pos, &neg);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    w = BuildSysRequest(NULL, body3, &pos, &neg, 0, 300, 50);
    show("sys body3", w, &pos, &neg);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    w = BuildSysRequest(NULL, &body9, &pos9, &neg, 0, 300, 50);
    show("sys body topaz9", w, &pos9, &neg);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    w = BuildEasyRequestArgs(NULL, &es, 0, &arg);
    show("easy 3 buttons", w, NULL, NULL);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    w = BuildEasyRequestArgs(NULL, &es1, 0, NULL);
    show("easy 1 button", w, NULL, NULL);
    if (w && (ULONG)w != 1) bgimage(w);
    if (w && (ULONG)w != 1) FreeSysRequest(w);

    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
