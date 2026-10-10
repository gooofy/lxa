/*
 * Probe (Phase 244): which font sizes the system requesters.  A program
 * may leave another font in the Workbench screen's RastPort (ACE's AIDE
 * does); AmigaOS 3.1 measures the text with that font.  A requester
 * without a reference window is a visitor window on the Workbench.
 */
#include <exec/memory.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static struct TextAttr topaz9 = { (STRPTR)"topaz.font", 9, 0, 0 };

static BOOL g_heights = TRUE;   /* heights with another RastPort font: Phase 256 */

static void size(const char *name, struct Window *w)
{
    probe_s(name);
    if (!w || w == (struct Window *)1)
    {
        probe_s(": no window\n");
        return;
    }
    ps_kv("w", w->Width);
    if (g_heights)
        ps_kv("h", w->Height);
    ps_kx("visitor", w->Flags & WFLG_VISITOR, 8);
    probe_ch('\n');
}

static void both(const char *name)
{
    struct IntuiText b = { 0, 1, JAM2, 0, 5, NULL, (UBYTE *)"thinpaz.font not installed.", NULL };
    struct IntuiText n = { 0, 1, JAM2, 6, 3, NULL, (UBYTE *)"I see", NULL };
    struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"T",
                             (UBYTE *)"thinpaz.font not installed.", (UBYTE *)"I see" };
    struct Window *w;

    probe_s("-- "); probe_s(name); probe_ch('\n');
    w = BuildSysRequest(NULL, &b, NULL, &n, IDCMP_GADGETUP, 0, 0);
    size("sys", w);
    if (w && w != (struct Window *)1)
        FreeSysRequest(w);
    w = BuildEasyRequestArgs(NULL, &es, 0, NULL);
    size("easy", w);
    if (w && w != (struct Window *)1)
        FreeSysRequest(w);
    es.es_TextFormat = (UBYTE *)"line one\nline two\nthree";
    w = BuildEasyRequestArgs(NULL, &es, 0, NULL);
    size("easy 3 lines", w);
    if (w && w != (struct Window *)1)
        FreeSysRequest(w);
    es.es_GadgetFormat = (UBYTE *)"A|B";
    w = BuildEasyRequestArgs(NULL, &es, 0, NULL);
    size("easy 3 lines 2 buttons", w);
    if (w && w != (struct Window *)1)
        FreeSysRequest(w);
}

int main(void)
{
    struct Screen *s;
    struct TextFont *f, *old;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    s = LockPubScreen(NULL);
    if (!s)
        return 20;
    both("screen font");
    f = OpenFont(&topaz9);
    if (f)
    {
        old = s->RastPort.Font;
        SetFont(&s->RastPort, f);
        g_heights = FALSE;
        both("topaz 9 in the screen's RastPort");
        SetFont(&s->RastPort, old);
        CloseFont(f);
    }
    UnlockPubScreen(NULL, s);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
