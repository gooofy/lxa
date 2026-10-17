/*
 * Probe (Phase 240): bitmap layouts and GetBitMapAttr().
 *
 * AllocBitMap() with and without BMF_INTERLEAVED/BMF_DISPLAYABLE, with a
 * friend bitmap, InitBitMap() bitmaps laid out by hand, and the bitmaps of
 * screens opened in different ways.  For each: BytesPerRow, the Flags byte,
 * the BMA_* attributes and how the planes lie relative to each other.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <graphics/gfx.h>
#include <graphics/modeid.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;

static void dump(const char *label, struct BitMap *bm)
{
    LONG delta;

    probe_s(label);
    if (!bm) {
        probe_s(": NULL\n");
        return;
    }
    probe_s(": bpr=");
    probe_dec(bm->BytesPerRow);
    probe_s(" rows=");
    probe_dec(bm->Rows);
    probe_s(" depth=");
    probe_dec(bm->Depth);
    probe_s(" flags=");
    probe_hex(bm->Flags, 2);
    probe_s(" pad=");
    probe_hex(bm->pad, 4);
    probe_s(" BMA_WIDTH=");
    probe_dec(GetBitMapAttr(bm, BMA_WIDTH));
    probe_s(" BMA_HEIGHT=");
    probe_dec(GetBitMapAttr(bm, BMA_HEIGHT));
    probe_s(" BMA_DEPTH=");
    probe_dec(GetBitMapAttr(bm, BMA_DEPTH));
    probe_s(" BMA_FLAGS=");
    probe_hex(GetBitMapAttr(bm, BMA_FLAGS), 8);
    probe_s(" planes=");
    if (bm->Depth < 2)
        probe_s("single");
    else {
        delta = (LONG)bm->Planes[1] - (LONG)bm->Planes[0];
        if (delta == bm->BytesPerRow / bm->Depth)
            probe_s("interleaved");
        else if (delta == (LONG)bm->BytesPerRow * bm->Rows)
            probe_s("contiguous");
        else
            probe_s("separate");
    }
    probe_s(" chip=");
    probe_dec((TypeOfMem(bm->Planes[0]) & MEMF_CHIP) != 0);
    probe_ch('\n');
}

static void alloc(const char *label, ULONG w, ULONG h, ULONG d, ULONG flags, struct BitMap *friend)
{
    struct BitMap *bm = AllocBitMap(w, h, d, flags, friend);
    dump(label, bm);
    if (bm) {
        WaitBlit();
        FreeBitMap(bm);
    }
}

static void screen(const char *label, struct TagItem *tags)
{
    struct Screen *s = OpenScreenTagList(NULL, tags);
    dump(label, s ? s->RastPort.BitMap : NULL);
    if (s)
        CloseScreen(s);
}

int main(void)
{
    struct Screen *wb;
    struct BitMap hand;
    PLANEPTR mem;

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 39);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 39);
    if (!GfxBase || !IntuitionBase)
        return 20;

    alloc("alloc 320x200x3", 320, 200, 3, 0, NULL);
    alloc("alloc 320x200x3 CLEAR", 320, 200, 3, BMF_CLEAR, NULL);
    alloc("alloc 320x200x3 DISPLAYABLE", 320, 200, 3, BMF_DISPLAYABLE, NULL);
    alloc("alloc 320x200x3 INTERLEAVED", 320, 200, 3, BMF_INTERLEAVED, NULL);
    alloc("alloc 320x200x3 DISPLAYABLE|INTERLEAVED", 320, 200, 3, BMF_DISPLAYABLE | BMF_INTERLEAVED, NULL);
    alloc("alloc 100x50x1 INTERLEAVED", 100, 50, 1, BMF_INTERLEAVED, NULL);
    alloc("alloc 100x50x8 INTERLEAVED", 100, 50, 8, BMF_INTERLEAVED, NULL);
    alloc("alloc 17x5x2 INTERLEAVED", 17, 5, 2, BMF_INTERLEAVED, NULL);

    wb = LockPubScreen(NULL);
    if (wb) {
        dump("Workbench", wb->RastPort.BitMap);
        alloc("alloc 64x64x2 friend=WB", 64, 64, 2, 0, wb->RastPort.BitMap);
        alloc("alloc 64x64x2 friend=WB INTERLEAVED", 64, 64, 2, BMF_INTERLEAVED, wb->RastPort.BitMap);
        alloc("alloc 64x64x3 friend=WB", 64, 64, 3, 0, wb->RastPort.BitMap);
        UnlockPubScreen(NULL, wb);
    }

    /* hand-made bitmaps: planes one after another, and interleaved */
    mem = (PLANEPTR)AllocMem(40 * 10 * 2, MEMF_CHIP | MEMF_CLEAR);
    if (mem) {
        InitBitMap(&hand, 2, 320, 10);
        hand.Planes[0] = mem;
        hand.Planes[1] = mem + 40 * 10;
        dump("InitBitMap contiguous", &hand);
        InitBitMap(&hand, 2, 640, 10);
        hand.BytesPerRow = 80;
        hand.Planes[0] = mem;
        hand.Planes[1] = mem + 40;
        dump("InitBitMap interleaved by hand", &hand);
        FreeMem(mem, 40 * 10 * 2);
    }

    {
        struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 3},
                               {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE}, {TAG_DONE, 0} };
        screen("screen tags", t);
    }
    {
        struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 3},
                               {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE},
                               {SA_Interleaved, TRUE}, {TAG_DONE, 0} };
        screen("screen SA_Interleaved TRUE", t);
    }
    {
        struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 3},
                               {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE},
                               {SA_Interleaved, FALSE}, {TAG_DONE, 0} };
        screen("screen SA_Interleaved FALSE", t);
    }
    {
        struct TagItem t[] = { {SA_Width, 640}, {SA_Height, 256}, {SA_Depth, 8},
                               {SA_DisplayID, HIRES_KEY}, {SA_Quiet, TRUE},
                               {SA_Interleaved, TRUE}, {TAG_DONE, 0} };
        screen("screen 256 colours SA_Interleaved TRUE", t);
    }
    {
        struct TagItem t[] = { {SA_Width, 320}, {SA_Height, 200}, {SA_Depth, 2},
                               {SA_DisplayID, LORES_KEY}, {SA_Quiet, TRUE},
                               {SA_LikeWorkbench, TRUE}, {TAG_DONE, 0} };
        screen("screen SA_LikeWorkbench", t);
    }
    {
        struct NewScreen ns = { 0, 0, 320, 200, 2, 0, 1, 0, CUSTOMSCREEN | SCREENQUIET, NULL,
                                (UBYTE *)"probe", NULL, NULL };
        struct Screen *s = OpenScreen(&ns);
        dump("OpenScreen NewScreen", s ? s->RastPort.BitMap : NULL);
        if (s)
            CloseScreen(s);
    }

    CloseLibrary((struct Library *)IntuitionBase);
    CloseLibrary((struct Library *)GfxBase);
    return 0;
}
