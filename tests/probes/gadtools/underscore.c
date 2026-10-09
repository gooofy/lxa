/*
 * Probe (Phase 244): the label IntuiTexts GadTools builds with
 * GT_Underscore when the label has no underscore, a trailing one or
 * several.  ADPro's and SnoopDos' TEXT_KIND labels carry a second, empty
 * IntuiText on AmigaOS 3.1.
 */
#include <exec/memory.h>
#include <libraries/gadtools.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;
struct Library *GadToolsBase;

static struct TextAttr topaz8 = { (STRPTR)"topaz.font", 8, 0, 0 };

static void one(APTR vi, ULONG kind, const char *label, ULONG us, ULONG place)
{
    struct Gadget *glist = NULL, *ctx, *g;
    struct NewGadget ng;
    struct IntuiText *it;
    int n = 0;

    ng.ng_LeftEdge = 100;
    ng.ng_TopEdge = 40;
    ng.ng_Width = 120;
    ng.ng_Height = 14;
    ng.ng_GadgetText = (UBYTE *)label;
    ng.ng_TextAttr = &topaz8;
    ng.ng_GadgetID = 1;
    ng.ng_Flags = place;
    ng.ng_VisualInfo = vi;
    ng.ng_UserData = NULL;

    probe_s("kind "); probe_dec(kind);
    ps_str("label", label);
    ps_kv("us", us);
    ps_kx("place", place, 2);
    probe_ch('\n');
    ctx = CreateContext(&glist);
    g = ctx ? CreateGadget(kind, ctx, &ng, us ? GT_Underscore : TAG_IGNORE, us, TAG_END) : NULL;
    if (!g)
        probe_s("  result = NULL\n");
    else
        for (it = g->GadgetText; it && n < 4; it = it->NextText, n++)
        {
            probe_s("  itext");
            ps_str("text", (const char *)it->IText);
            ps_kv("left", it->LeftEdge);
            ps_kv("top", it->TopEdge);
            ps_kv("fpen", it->FrontPen);
            probe_s(it->ITextFont ? " font" : " nofont");
            if (it->ITextFont)
                ps_kx("style", it->ITextFont->ta_Style, 2);
            probe_ch('\n');
        }
    FreeGadgets(glist);
}

int main(void)
{
    struct Screen *scr;
    APTR vi;
    static const char *labels[] = { "Plain", "Trail_", "Mid_dle", "__Dbl", "_", "a_b_c", NULL };
    int i;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    GadToolsBase = OpenLibrary((STRPTR)"gadtools.library", 37);
    if (!IntuitionBase || !GfxBase || !GadToolsBase)
        return 0;
    scr = LockPubScreen(NULL);
    vi = scr ? GetVisualInfo(scr, TAG_END) : NULL;
    if (!vi)
        return 0;
    for (i = 0; labels[i]; i++)
    {
        one(vi, BUTTON_KIND, labels[i], '_', 0);
        one(vi, TEXT_KIND, labels[i], '_', PLACETEXT_LEFT);
    }
    one(vi, TEXT_KIND, "Plain", 0, PLACETEXT_LEFT);
    one(vi, TEXT_KIND, "Plain", '~', PLACETEXT_ABOVE);
    one(vi, STRING_KIND, "Plain", '_', 0);
    one(vi, CHECKBOX_KIND, "Plain", '_', 0);
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);
    CloseLibrary(GadToolsBase);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
