/*
 * Probe (Phase 244): fillrectclass, the built-in image class that fills
 * its box with a pattern (IA_APattern/IA_APatSize/IA_Mode, IA_FGPen,
 * IA_BGPen).  reqtools.library's requesters need it: without it
 * FontView's "Unregistered..." requester never opens.
 */
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <graphics/gfxmacros.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>
#include "idump.h"

struct Library *IntuitionBase;
struct Library *GfxBase;

static UWORD pat[] = { 0xAAAA, 0x5555 };

static ULONG hash(struct RastPort *rp)
{
    LONG x, y;
    ULONG h = 0;
    for (y = 0; y < 24; y++)
        for (x = 0; x < 48; x++)
            h = h * 31 + ReadPixel(rp, x, y);
    return h;
}

static void draw(const char *name, struct RastPort *rp, Object *o, ULONG state, BOOL frame)
{
    ULONG v;

    SetRast(rp, 0);
    SetAPen(rp, 2);
    SetBPen(rp, 1);
    SetDrMd(rp, JAM1);
    if (frame)
        DrawImageState(rp, (struct Image *)o, 2, 3, state, NULL);
    else
        DrawImage(rp, (struct Image *)o, 2, 3);
    probe_s(name);
    ps_kx("hash", hash(rp), 8);
    ps_kv("apen", rp->FgPen);
    ps_kv("bpen", rp->BgPen);
    ps_kv("drmd", rp->DrawMode);
    probe_s(rp->AreaPtrn ? " areaptrn" : " noareaptrn");
    probe_ch('\n');
    v = 0;
    GetAttr(IA_Width, o, &v);
    probe_s("  IA_Width");
    ps_kv("=", (LONG)v);
    probe_ch('\n');
}

int main(void)
{
    struct BitMap *bm;
    struct RastPort rp;
    Object *a, *b, *c;
    struct Image *im;

    IntuitionBase = OpenLibrary((STRPTR)"intuition.library", 37);
    GfxBase = OpenLibrary((STRPTR)"graphics.library", 37);
    if (!IntuitionBase || !GfxBase)
        return 20;
    a = NewObject(NULL, (STRPTR)"fillrectclass", IA_Width, 30, IA_Height, 12, IA_FGPen, 3, TAG_END);
    b = NewObject(NULL, (STRPTR)"fillrectclass", IA_Width, 40, IA_Height, 16, IA_FGPen, 3, IA_BGPen, 1,
                  IA_APattern, (ULONG)pat, IA_APatSize, 1, IA_Mode, JAM2, TAG_END);
    c = NewObject(NULL, (STRPTR)"fillrectclass", IA_Left, 4, IA_Top, 2, IA_Width, 20, IA_Height, 10,
                  IA_APattern, (ULONG)pat, IA_APatSize, 1, IA_Mode, JAM1, TAG_END);
    P_NULL("plain", a);
    P_NULL("pattern", b);
    if (!a || !b || !c)
        return 0;
    im = (struct Image *)a;
    probe_s("image");
    ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
    ps_kv("depth", im->Depth);
    ps_kv("pick", im->PlanePick);
    ps_kv("onoff", im->PlaneOnOff);
    probe_ch('\n');
    im = (struct Image *)c;
    probe_s("image c");
    ps_box(im->LeftEdge, im->TopEdge, im->Width, im->Height);
    ps_kv("pick", im->PlanePick);
    ps_kv("onoff", im->PlaneOnOff);
    probe_ch('\n');

    bm = AllocBitMap(48, 24, 2, BMF_CLEAR, NULL);
    if (bm)
    {
        InitRastPort(&rp);
        rp.BitMap = bm;
        draw("plain", &rp, a, IDS_NORMAL, FALSE);
        draw("pattern jam2", &rp, b, IDS_NORMAL, FALSE);
        draw("pattern jam1 offset", &rp, c, IDS_NORMAL, FALSE);
        draw("pattern selected", &rp, b, IDS_SELECTED, TRUE);
        WaitBlit();
        FreeBitMap(bm);
    }
    DisposeObject(a);
    DisposeObject(b);
    DisposeObject(c);
    CloseLibrary(GfxBase);
    CloseLibrary(IntuitionBase);
    return 0;
}
