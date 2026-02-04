/* RGBBoxes.c - Adapted from RKM sample */
#include <exec/types.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <clib/graphics_protos.h>
#include <clib/exec_protos.h>
#include <stdio.h>

#define WIDTH 320
#define HEIGHT 200
#define DEPTH 2

struct GfxBase *GfxBase;

int main(void)
{
    struct BitMap bitMap;
    struct ViewPort viewPort;
    struct RasInfo rasInfo;
    struct View view, *oldView;
    int i;

    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 33);
    if (!GfxBase) return 1;

    oldView = GfxBase->ActiView;

    InitView(&view);
    InitBitMap(&bitMap, DEPTH, WIDTH, HEIGHT);

    for (i = 0; i < DEPTH; i++) {
        bitMap.Planes[i] = AllocRaster(WIDTH, HEIGHT);
        if (!bitMap.Planes[i]) return 1;
        BltClear(bitMap.Planes[i], (WIDTH / 8) * HEIGHT, 0);
    }

    rasInfo.BitMap = &bitMap;
    rasInfo.RxOffset = 0;
    rasInfo.RyOffset = 0;
    rasInfo.Next = NULL;

    InitVPort(&viewPort);
    view.ViewPort = &viewPort;
    viewPort.RasInfo = &rasInfo;
    viewPort.DWidth = WIDTH;
    viewPort.DHeight = HEIGHT;

    MakeVPort(&view, &viewPort);
    MrgCop(&view);

    LoadView(&view);
    
    printf("Custom view loaded. Waiting 5 seconds...\n");
    
    /* Draw something manually in the bitplanes? 
     * RKM sample does that, but let's just wait for now to verify display switching.
     */
    
    for (i = 0; i < 50; i++) {
        WaitTOF();
    }

    LoadView(oldView);
    WaitTOF();

    /* Cleanup */
    FreeVPortCopLists(&viewPort);
    FreeCprList(view.LOFCprList);
    for (i = 0; i < DEPTH; i++) {
        FreeRaster(bitMap.Planes[i], WIDTH, HEIGHT);
    }
    CloseLibrary((struct Library *)GfxBase);

    printf("Done.\n");
    return 0;
}
