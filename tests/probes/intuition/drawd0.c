/*
 * Probe (Phase 237c): what D0 holds after the VOID drawing functions of
 * intuition.library - PrintIText(), DrawBorder(), DrawImage().  Fred
 * Fish BizCalc (disks 493, 535) opens its main window in a C function
 * without a return statement whose last call is PrintIText(); the caller
 * tests D0 and quits with "Can't open main window!" when it is 0.
 */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/intuition.h>
#include "probe.h"

extern struct ExecBase *SysBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

/* icall(lvo, a0, a1, d0, d1) -> d0 */
ULONG icall(LONG lvo, ULONG a0, ULONG a1, ULONG d0, ULONG d1);
__asm__(
    "    .text\n"
    "    .globl _icall\n"
    "_icall:\n"
    "    movem.l d2-d7/a2-a6,-(sp)\n"
    "    move.l _IntuitionBase,a6\n"
    "    move.l a6,a2\n"
    "    add.l 48(sp),a2\n"
    "    move.l 52(sp),a0\n"
    "    move.l 56(sp),a1\n"
    "    move.l 60(sp),d0\n"
    "    move.l 64(sp),d1\n"
    "    moveq #-1,d2\n"
    "    jsr (a2)\n"
    "    movem.l (sp)+,d2-d7/a2-a6\n"
    "    rts\n");

static struct RastPort rp;
static struct BitMap bm;
static WORD pts[] = { 0, 0, 10, 0, 10, 5, 0, 5, 0, 0 };
static UWORD imgdata[] = { 0xffff, 0x8001, 0xffff, 0x0000, 0x7ffe, 0x0000 };

static void show(const char *name, ULONG d0, ULONG arg)
{
    probe_s(name);
    probe_s(": d0 ");
    if (d0 == 0) probe_s("0");
    else if (d0 == (ULONG)&rp) probe_s("= rp");
    else if (d0 == arg) probe_s("= the structure");
    else if (d0 == (ULONG)rp.Font) probe_s("= rp->Font");
    else if (d0 == (ULONG)GfxBase) probe_s("= GfxBase");
    else if (d0 == (ULONG)IntuitionBase) probe_s("= IntuitionBase");
    else if (d0 < 0x10000) { probe_s("small "); probe_hex(d0, 8); }
    else { probe_s("nonzero "); probe_hex(d0, 8); }
    probe_ch('\n');
}

int main(void)
{
    struct IntuiText it1, it2;
    struct Border b1, b2;
    struct Image im;
    UWORD *chipimg = AllocMem(sizeof(imgdata), MEMF_CHIP);
    int i;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 0);
    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR)"graphics.library", 0);
    if (!IntuitionBase || !GfxBase || !chipimg)
        return 20;
    for (i = 0; i < 6; i++)
        chipimg[i] = imgdata[i];
    InitBitMap(&bm, 2, 64, 32);
    bm.Planes[0] = AllocRaster(64, 32);
    bm.Planes[1] = AllocRaster(64, 32);
    InitRastPort(&rp);
    rp.BitMap = &bm;

    it2.FrontPen = 2; it2.BackPen = 0; it2.DrawMode = JAM1;
    it2.LeftEdge = 0; it2.TopEdge = 10; it2.ITextFont = NULL;
    it2.IText = (UBYTE *)"b"; it2.NextText = NULL;
    it1 = it2;
    it1.TopEdge = 0; it1.IText = (UBYTE *)"Hello"; it1.NextText = &it2;

    b2.LeftEdge = 1; b2.TopEdge = 1; b2.FrontPen = 2; b2.BackPen = 0;
    b2.DrawMode = JAM1; b2.Count = 5; b2.XY = pts; b2.NextBorder = NULL;
    b1 = b2;
    b1.LeftEdge = 0; b1.NextBorder = &b2;

    im.LeftEdge = 2; im.TopEdge = 2; im.Width = 16; im.Height = 3;
    im.Depth = 2; im.ImageData = chipimg; im.PlanePick = 3; im.PlaneOnOff = 0;
    im.NextImage = NULL;

    show("PrintIText (2 texts)", icall(-216, (ULONG)&it1, (ULONG)&rp, 4, 4), (ULONG)&it1);
    show("PrintIText (1 text)", icall(-216, (ULONG)&it2, (ULONG)&rp, 0, 0), (ULONG)&it2);
    it1.NextText = NULL;
    show("PrintIText (1 text, 5 chars)", icall(-216, (ULONG)&it1, (ULONG)&rp, 0, 0), (ULONG)&it1);
    it1.IText = (UBYTE *)"";
    show("PrintIText (empty text)", icall(-216, (ULONG)&it1, (ULONG)&rp, 0, 0), (ULONG)&it1);
    show("DrawBorder (2 borders)", icall(-108, (ULONG)&rp, (ULONG)&b1, 4, 4), (ULONG)&b1);
    show("DrawImage", icall(-114, (ULONG)&rp, (ULONG)&im, 4, 4), (ULONG)&im);

    FreeRaster(bm.Planes[0], 64, 32);
    FreeRaster(bm.Planes[1], 64, 32);
    FreeMem(chipimg, sizeof(imgdata));
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
