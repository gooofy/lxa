/*
 * Probe (Phase 222d): intuition.library BuildEasyRequestArgs and
 * BuildSysRequest layout rules - window size and button boxes for body
 * texts of 0..4 lines and several widths, 1..5 buttons in narrow and wide
 * requesters, and AutoRequest IntuiText placements.
 */
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static void show(struct Window *w)
{
    struct Gadget *g;

    if (!w || w == (struct Window *)1)
    {
        probe_s("no window\n");
        return;
    }
    probe_s("size");
    ps_kv("w", w->Width);
    ps_kv("h", w->Height);
    probe_ch('\n');
    for (g = w->FirstGadget; g; g = g->NextGadget)
        if (!(g->GadgetType & GTYP_SYSGADGET) && (g->Activation & GACT_RELVERIFY))
        {
            probe_s("button");
            ps_kv("id", g->GadgetID);
            ps_box(g->LeftEdge, g->TopEdge, g->Width, g->Height);
            probe_ch('\n');
        }
    FreeSysRequest(w);
}

static void easy(struct Window *ref, const char *title, const char *body, const char *gads)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(es);
    es.es_Flags = 0;
    es.es_Title = (UBYTE *)title;
    es.es_TextFormat = (UBYTE *)body;
    es.es_GadgetFormat = (UBYTE *)gads;
    probe_s("-- easy ");
    probe_s(title ? title : "(null)");
    probe_s(" | ");
    {
        const char *c;
        for (c = body; *c; c++)
            probe_ch(*c == '\n' ? '/' : *c);
    }
    probe_s(" | ");
    probe_s(gads);
    probe_ch('\n');
    show(BuildEasyRequestArgs(ref, &es, 0, NULL));
}

static void sys(struct Window *ref, WORD bl, WORD bt, WORD bl2, WORD bt2, const char *t1, const char *t2,
                const char *pos, const char *neg)
{
    struct IntuiText b2 = { 0, 1, JAM2, 0, 0, NULL, NULL, NULL };
    struct IntuiText b1 = { 0, 1, JAM2, 0, 0, NULL, NULL, NULL };
    struct IntuiText p = { 0, 1, JAM2, 6, 3, NULL, NULL, NULL };
    struct IntuiText n = { 0, 1, JAM2, 6, 3, NULL, NULL, NULL };

    b1.LeftEdge = bl; b1.TopEdge = bt; b1.IText = (UBYTE *)t1;
    b2.LeftEdge = bl2; b2.TopEdge = bt2; b2.IText = (UBYTE *)t2;
    if (t2)
        b1.NextText = &b2;
    p.IText = (UBYTE *)pos;
    n.IText = (UBYTE *)neg;
    probe_s("-- sys ");
    probe_dec(bl); probe_ch(','); probe_dec(bt); probe_ch(' '); probe_s(t1);
    if (t2)
    {
        probe_s(" + "); probe_dec(bl2); probe_ch(','); probe_dec(bt2); probe_ch(' '); probe_s(t2);
    }
    probe_s(" | "); probe_s(pos ? pos : "-"); probe_s(" | "); probe_s(neg);
    probe_ch('\n');
    show(BuildSysRequest(ref, &b1, pos ? &p : NULL, &n, 0, 100, 50));
}

int main(void)
{
    struct Screen *s;
    struct Window *win;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen(NULL);
    if (!s)
        return 20;
    win = OpenWindowTags(NULL, WA_PubScreen, (ULONG)s, WA_Left, 40, WA_Top, 30, WA_Width, 300, WA_Height, 120,
                         WA_Title, (ULONG)"P", WA_DragBar, TRUE, TAG_END);
    if (!win)
        return 20;

    /* body lines */
    easy(win, "T", "", "OK");
    easy(win, "T", "a", "OK");
    easy(win, "T", "a\nb", "OK");
    easy(win, "T", "a\nb\nc", "OK");
    easy(win, "T", "a\nb\nc\nd", "OK");
    easy(win, "T", "a\n\nb", "OK");
    easy(win, "T", "\n", "OK");
    /* body widths */
    easy(win, "T", "abcdefghij", "OK");
    easy(win, "T", "abcdefghijabcdefghij", "OK");
    easy(win, "T", "abcdefghijabcdefghijabcdefghijabcdefghij", "OK");
    easy(win, "T", "short\nabcdefghijabcdefghijabcdefghij", "OK");
    /* buttons */
    easy(win, "T", "x", "A|B");
    easy(win, "T", "x", "A|B|C");
    easy(win, "T", "x", "A|B|C|D|E");
    easy(win, "T", "x", "Longer|B");
    easy(win, "T", "abcdefghijabcdefghijabcdefghijabcdefghij", "A|B");
    easy(win, "T", "abcdefghijabcdefghijabcdefghijabcdefghij", "A|B|C");
    easy(win, "T", "abcdefghijabcdefghijabcdefghijabcdefghij", "A|B|C|D");
    easy(win, "T", "abcdefghijabcdefghijabcdefghijabcdefghij", "One|Two|Three|Four|Five");
    /* titles */
    easy(win, "", "x", "OK");
    easy(win, "abcdefghijabcdefghij", "x", "OK");
    easy(NULL, NULL, "x", "OK");

    /* AutoRequest texts */
    sys(win, 0, 0, 0, 0, "x", NULL, "A", "B");
    sys(win, 0, 0, 0, 0, "abcdefghijabcdefghij", NULL, "A", "B");
    sys(win, 10, 0, 0, 0, "x", NULL, "A", "B");
    sys(win, 0, 10, 0, 0, "x", NULL, "A", "B");
    sys(win, 0, 0, 0, 10, "x", "y", "A", "B");
    sys(win, 0, 0, 0, 20, "x", "y", "A", "B");
    sys(win, 0, 0, 100, 0, "x", "y", "A", "B");
    sys(win, 0, 0, 0, 0, "x", NULL, NULL, "B");
    sys(win, 0, 0, 0, 0, "x", NULL, "Longer text", "B");
    sys(win, 0, 0, 0, 0, "abcdefghijabcdefghijabcdefghijabcdefghij", NULL, "A", "B");

    CloseWindow(win);
    UnlockPubScreen(NULL, s);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
