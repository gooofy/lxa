/* wbclone.c - Adapted from RKM WBClone sample
 * Demonstrates low-level graphics primitives:
 * - LockPubScreen/UnlockPubScreen for screen access
 * - GetVPModeID to get display mode
 * - LockIBase/UnlockIBase for IntuitionBase access
 * - InitBitMap, AllocRaster, FreeRaster for bitmap creation
 * - InitRastPort, SetRast, SetAPen, Move, Draw for rendering
 * - GetColorMap, FreeColorMap, GetRGB4, SetRGB4CM for colors
 * 
 * Modified for automated testing (simplified, auto-exits).
 * Note: Some low-level View/ViewPort functions may not be fully
 * implemented in lxa, so we focus on what's available.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <graphics/rastport.h>

#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/graphics_protos.h>

#include <stdio.h>

struct IntuitionBase *IntuitionBase = NULL;
struct GfxBase *GfxBase = NULL;

/* Create a BitMap with allocated planes */
struct BitMap *CreateBitMap(SHORT width, SHORT height, SHORT depth)
{
    struct BitMap *bm;
    int i;
    BOOL success = TRUE;

    printf("WBClone: Allocating BitMap (%dx%dx%d)...\n", width, height, depth);
    
    bm = AllocVec(sizeof(struct BitMap), MEMF_CLEAR | MEMF_PUBLIC);
    if (!bm)
    {
        printf("ERROR: Could not allocate BitMap structure\n");
        return NULL;
    }
    
    InitBitMap(bm, depth, width, height);
    printf("WBClone: BitMap initialized at 0x%08lx\n", (ULONG)bm);
    printf("WBClone: BytesPerRow=%d, Rows=%d, Depth=%d\n", 
           bm->BytesPerRow, bm->Rows, bm->Depth);
    
    for (i = 0; i < depth && success; i++)
    {
        bm->Planes[i] = AllocRaster(width, height);
        if (!bm->Planes[i])
        {
            printf("ERROR: Could not allocate plane %d\n", i);
            success = FALSE;
        }
        else
        {
            printf("WBClone: Plane %d allocated at 0x%08lx\n", i, (ULONG)bm->Planes[i]);
        }
    }
    
    if (!success)
    {
        /* Free any allocated planes */
        for (i = 0; i < depth; i++)
        {
            if (bm->Planes[i])
                FreeRaster(bm->Planes[i], width, height);
        }
        FreeVec(bm);
        return NULL;
    }
    
    return bm;
}

/* Free a BitMap and its planes */
void DestroyBitMap(struct BitMap *bm, SHORT width, SHORT height, SHORT depth)
{
    int i;
    
    if (bm)
    {
        for (i = 0; i < depth; i++)
        {
            if (bm->Planes[i])
            {
                FreeRaster(bm->Planes[i], width, height);
                printf("WBClone: Plane %d freed\n", i);
            }
        }
        FreeVec(bm);
        printf("WBClone: BitMap freed\n");
    }
}

int main(void)
{
    struct Screen *wb = NULL;
    struct BitMap *myBitMap = NULL;
    struct RastPort *rp = NULL;
    ULONG modeID;
    ULONG iBaseLock;
    SHORT width, height, depth;
    UWORD color;
    int i;
    
    printf("WBClone: Workbench screen cloning demonstration\n\n");
    
    /* Open libraries */
    printf("WBClone: Opening intuition.library...\n");
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    if (!IntuitionBase)
    {
        printf("ERROR: Could not open intuition.library\n");
        return 1;
    }
    printf("WBClone: intuition.library v%d opened\n", IntuitionBase->LibNode.lib_Version);
    
    printf("WBClone: Opening graphics.library...\n");
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
    if (!GfxBase)
    {
        printf("ERROR: Could not open graphics.library\n");
        CloseLibrary((struct Library *)IntuitionBase);
        return 1;
    }
    printf("WBClone: graphics.library v%d opened\n\n", GfxBase->LibNode.lib_Version);
    
    /* Lock the Workbench screen */
    printf("WBClone: Locking Workbench screen...\n");
    wb = LockPubScreen("Workbench");
    if (!wb)
    {
        printf("ERROR: Could not lock Workbench screen\n");
        goto cleanup;
    }
    printf("WBClone: Workbench screen locked at 0x%08lx\n", (ULONG)wb);
    
    /* Get screen properties */
    width = wb->Width;
    height = wb->Height;
    depth = wb->BitMap.Depth;
    printf("WBClone: Screen dimensions: %dx%dx%d\n", width, height, depth);
    
    /* Get the ModeID */
    modeID = GetVPModeID(&wb->ViewPort);
    printf("WBClone: Screen ModeID: 0x%08lx\n", modeID);
    
    /* Lock IntuitionBase to read ViewLord */
    printf("\nWBClone: Locking IntuitionBase...\n");
    iBaseLock = LockIBase(0);
    printf("WBClone: IntuitionBase locked (lock=0x%08lx)\n", iBaseLock);
    
    /* Read ViewLord info (Intuition's main View) */
    printf("WBClone: ViewLord at 0x%08lx\n", (ULONG)&IntuitionBase->ViewLord);
    printf("WBClone: ViewLord.Modes: 0x%04x\n", IntuitionBase->ViewLord.Modes);
    printf("WBClone: ViewLord.DxOffset: %d, DyOffset: %d\n", 
           IntuitionBase->ViewLord.DxOffset, 
           IntuitionBase->ViewLord.DyOffset);
    
    UnlockIBase(iBaseLock);
    printf("WBClone: IntuitionBase unlocked\n");
    
    /* Read some ColorMap colors */
    printf("\nWBClone: Reading Workbench ColorMap...\n");
    if (wb->ViewPort.ColorMap)
    {
        printf("WBClone: ColorMap at 0x%08lx\n", (ULONG)wb->ViewPort.ColorMap);
        for (i = 0; i < 4 && i < (1 << depth); i++)
        {
            color = GetRGB4(wb->ViewPort.ColorMap, i);
            printf("WBClone: Color %d: 0x%03x (R=%d G=%d B=%d)\n",
                   i, color,
                   (color >> 8) & 0xF,
                   (color >> 4) & 0xF,
                   color & 0xF);
        }
    }
    else
    {
        printf("WBClone: No ColorMap found\n");
    }
    
    /* Create a bitmap matching the screen */
    printf("\nWBClone: Creating matching BitMap...\n");
    myBitMap = CreateBitMap(width, height, depth);
    if (!myBitMap)
    {
        printf("ERROR: Could not create BitMap\n");
        goto cleanup;
    }
    
    /* Create a RastPort for rendering */
    printf("\nWBClone: Creating RastPort for rendering...\n");
    rp = AllocVec(sizeof(struct RastPort), MEMF_CLEAR | MEMF_PUBLIC);
    if (!rp)
    {
        printf("ERROR: Could not allocate RastPort\n");
        goto cleanup;
    }
    
    InitRastPort(rp);
    rp->BitMap = myBitMap;
    printf("WBClone: RastPort initialized at 0x%08lx\n", (ULONG)rp);
    
    /* Clear the bitmap */
    printf("WBClone: Clearing bitmap with SetRast(0)...\n");
    SetRast(rp, 0);
    printf("WBClone: BitMap cleared\n");
    
    /* Draw a simple border pattern */
    printf("WBClone: Drawing border pattern...\n");
    SetAPen(rp, (1 << depth) - 1);  /* Use highest pen */
    Move(rp, 0, 0);
    Draw(rp, width - 1, 0);
    Draw(rp, width - 1, height - 1);
    Draw(rp, 0, height - 1);
    Draw(rp, 0, 0);
    printf("WBClone: Border drawn with pen %d\n", (1 << depth) - 1);
    
    /* Draw diagonal lines */
    Move(rp, 0, 0);
    Draw(rp, width - 1, height - 1);
    Move(rp, width - 1, 0);
    Draw(rp, 0, height - 1);
    printf("WBClone: Diagonal lines drawn\n");
    
    printf("\nWBClone: Rendering complete (bitmap not displayed on screen)\n");
    printf("WBClone: In a full implementation, MakeVPort/MrgCop/LoadView would display this.\n");
    
cleanup:
    /* Free resources */
    printf("\nWBClone: Cleaning up...\n");
    
    if (rp)
    {
        FreeVec(rp);
        printf("WBClone: RastPort freed\n");
    }
    
    if (myBitMap)
        DestroyBitMap(myBitMap, width, height, depth);
    
    if (wb)
    {
        UnlockPubScreen(NULL, wb);
        printf("WBClone: Workbench screen unlocked\n");
    }
    
    CloseLibrary((struct Library *)GfxBase);
    CloseLibrary((struct Library *)IntuitionBase);
    printf("WBClone: Libraries closed\n");
    
    printf("\nWBClone: Demo complete.\n");
    return 0;
}
