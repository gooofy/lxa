/*
 * Test: graphics/layer_clipping
 * Tests that graphics primitives (WritePixel, Draw, RectFill) properly clip
 * to layer ClipRects when windows overlap.
 *
 * Tests:
 * 1. Single layer - drawing works normally
 * 2. Overlapping layers - drawing to back layer clips to visible ClipRects
 * 3. Drawing outside layer bounds is clipped
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/layers_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/layers.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;
extern struct Library *LayersBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;
    
    *p = '\0';
    if (n < 0) {
        neg = TRUE;
        n = -n;
    }
    if (n == 0) {
        *--p = '0';
    } else {
        while (n > 0) {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *layer1, *layer2;
    struct RastPort rp1, rp2;
    int errors = 0;
    LONG color;

    print("Testing graphics layer clipping...\n");
    
    /* Check LayersBase is available */
    if (!LayersBase) {
        print("FAIL: LayersBase is NULL\n");
        return 20;
    }
    print("OK: LayersBase available\n");

    /* Create a Layer_Info structure */
    print("\nSetting up test environment...\n");
    li = NewLayerInfo();
    if (!li) {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }
    print("OK: NewLayerInfo() succeeded\n");

    /* Create a bitmap for layer operations - 2 planes for 4 colors */
    bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!bm) {
        print("FAIL: Could not allocate BitMap\n");
        DisposeLayerInfo(li);
        return 20;
    }
    InitBitMap(bm, 2, 320, 200);
    bm->Planes[0] = AllocRaster(320, 200);
    bm->Planes[1] = AllocRaster(320, 200);
    if (!bm->Planes[0] || !bm->Planes[1]) {
        print("FAIL: Could not allocate raster planes\n");
        if (bm->Planes[0]) FreeRaster(bm->Planes[0], 320, 200);
        if (bm->Planes[1]) FreeRaster(bm->Planes[1], 320, 200);
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }
    print("OK: BitMap allocated with 2 planes\n");

    /* Clear the bitmap */
    {
        struct RastPort tmpRp;
        InitRastPort(&tmpRp);
        tmpRp.BitMap = bm;
        SetRast(&tmpRp, 0);
    }
    print("OK: BitMap cleared\n");

    /*
     * Test 1: Single layer - drawing works normally
     *
     * Layer1: [20,20]-[120,120] (100x100 pixels)
     */
    print("\nTest 1: Single layer drawing...\n");
    layer1 = CreateUpfrontLayer(li, bm, 20, 20, 120, 120, LAYERSIMPLE, NULL);
    if (!layer1) {
        print("FAIL: Could not create layer1\n");
        errors++;
        goto cleanup;
    }
    print("OK: Layer1 created at [20,20]-[120,120]\n");

    /* Initialize RastPort for layer1 */
    InitRastPort(&rp1);
    rp1.BitMap = bm;
    rp1.Layer = layer1;

    /* Draw a pixel at layer-relative (10,10) = absolute (30,30) */
    SetAPen(&rp1, 1);
    WritePixel(&rp1, 10, 10);

    /* Read the pixel back from absolute position */
    color = ReadPixel(&rp1, 10, 10);
    if (color == 1) {
        print("OK: WritePixel on single layer works\n");
    } else {
        print("FAIL: WritePixel returned wrong color: ");
        print_num(color);
        print("\n");
        errors++;
    }

    /* Draw a rectangle at layer-relative (5,5)-(15,15) */
    SetAPen(&rp1, 2);
    RectFill(&rp1, 5, 5, 15, 15);

    /* Verify inside rectangle */
    color = ReadPixel(&rp1, 7, 7);
    if (color == 2) {
        print("OK: RectFill on single layer works\n");
    } else {
        print("FAIL: RectFill color wrong at (7,7): ");
        print_num(color);
        print("\n");
        errors++;
    }

    /*
     * Test 2: Overlapping layers - drawing to back layer clips
     *
     * Layer2 (front): [60,60]-[140,140] - partially overlaps layer1
     * The overlap region is [60,60]-[120,120] of layer1
     *
     * When drawing to layer1, pixels in the overlap region should NOT be drawn
     * because they're obscured by layer2.
     */
    print("\nTest 2: Overlapping layers clipping...\n");
    
    /* First, clear layer1's area */
    SetAPen(&rp1, 0);
    RectFill(&rp1, 0, 0, 100, 100);
    
    /* Create overlapping layer2 */
    layer2 = CreateUpfrontLayer(li, bm, 60, 60, 140, 140, LAYERSIMPLE, NULL);
    if (!layer2) {
        print("FAIL: Could not create layer2\n");
        errors++;
        goto cleanup;
    }
    print("OK: Layer2 created at [60,60]-[140,140] (overlaps layer1)\n");

    /* Initialize RastPort for layer2 */
    InitRastPort(&rp2);
    rp2.BitMap = bm;
    rp2.Layer = layer2;

    /* Fill layer2 with color 3 so we can see it */
    SetAPen(&rp2, 3);
    RectFill(&rp2, 0, 0, 80, 80);
    print("OK: Filled layer2 with color 3\n");

    /* Now try to fill ALL of layer1 with color 1 */
    SetAPen(&rp1, 1);
    RectFill(&rp1, 0, 0, 100, 100);
    print("OK: Attempted to fill layer1 with color 1\n");

    /*
     * Check visible part of layer1 (outside overlap):
     * Layer1 absolute: [20,20]-[120,120]
     * Layer2 absolute: [60,60]-[140,140]
     * Overlap: [60,60]-[120,120]
     * 
     * Visible portions of layer1:
     * - Left strip: [20,20]-[59,120]
     * - Top strip: [60,20]-[120,59]
     *
     * In layer-relative coords:
     * - Pixel at (5,5) = absolute (25,25) - should be color 1 (visible)
     * - Pixel at (50,50) = absolute (70,70) - should be color 3 (obscured by layer2)
     */

    /* Check visible part of layer1 */
    color = ReadPixel(&rp1, 5, 5);  /* Should be 1 (we drew here, visible) */
    if (color == 1) {
        print("OK: Visible part of layer1 has color 1\n");
    } else {
        print("FAIL: Visible part of layer1 has wrong color: ");
        print_num(color);
        print("\n");
        errors++;
    }

    /* Check obscured part - should still be layer2's color 3 */
    /* Note: Reading from rp1 at (50,50) reads absolute (70,70) which is in layer2's area */
    /* When a layer is LAYERSIMPLE, the bitmap at that location holds layer2's pixel */
    {
        struct RastPort tmpRp;
        InitRastPort(&tmpRp);
        tmpRp.BitMap = bm;
        /* Read directly from bitmap at absolute position (70,70) */
        color = ReadPixel(&tmpRp, 70, 70);
        if (color == 3) {
            print("OK: Obscured area preserved (layer2 color 3 intact)\n");
        } else {
            print("FAIL: Obscured area was overwritten! Got color: ");
            print_num(color);
            print(" (expected 3)\n");
            errors++;
        }
    }

    /*
     * Test 3: Drawing a line across both visible and obscured regions
     *
     * Draw a horizontal line from layer1-relative (5,50) to (95,50)
     * Layer1 at [20,20]-[120,120], so absolute y = 70
     * Layer2 at [60,60]-[140,140]
     * The line at absolute y=70 starts in visible area (x < 60) and crosses into
     * the obscured area (x >= 60).
     */
    print("\nTest 3: Line clipping across visible/obscured regions...\n");
    
    /* Clear layer1 first */
    SetAPen(&rp1, 0);
    RectFill(&rp1, 0, 0, 100, 100);
    
    /* Re-fill layer2 to be sure */
    SetAPen(&rp2, 3);
    RectFill(&rp2, 0, 0, 80, 80);

    /* Draw horizontal line at y=50 (absolute y=70) across layer1 */
    SetAPen(&rp1, 2);
    Move(&rp1, 5, 50);
    Draw(&rp1, 95, 50);
    print("OK: Drew line from (5,50) to (95,50)\n");

    /* Check visible portion: at x=10 (absolute x=30), should be color 2 */
    color = ReadPixel(&rp1, 10, 50);
    if (color == 2) {
        print("OK: Line visible at (10,50)\n");
    } else {
        print("FAIL: Line not visible at (10,50), color: ");
        print_num(color);
        print("\n");
        errors++;
    }

    /* Check obscured portion: at absolute (70,70), should still be layer2's color 3 */
    {
        struct RastPort tmpRp;
        InitRastPort(&tmpRp);
        tmpRp.BitMap = bm;
        color = ReadPixel(&tmpRp, 70, 70);
        if (color == 3) {
            print("OK: Line clipped in obscured area (still color 3)\n");
        } else {
            print("FAIL: Line drew into obscured area! Got color: ");
            print_num(color);
            print(" (expected 3)\n");
            errors++;
        }
    }

    /*
     * Test 4: WritePixel clipping
     *
     * Try to write a pixel in the obscured region - should not change bitmap
     */
    print("\nTest 4: WritePixel clipping in obscured region...\n");
    
    /* Re-fill layer2 */
    SetAPen(&rp2, 3);
    RectFill(&rp2, 0, 0, 80, 80);

    /* Try to write pixel at layer1-relative (60,60) = absolute (80,80), which is in layer2 */
    SetAPen(&rp1, 1);
    WritePixel(&rp1, 60, 60);

    /* Check the pixel - should still be 3 */
    {
        struct RastPort tmpRp;
        InitRastPort(&tmpRp);
        tmpRp.BitMap = bm;
        color = ReadPixel(&tmpRp, 80, 80);
        if (color == 3) {
            print("OK: WritePixel clipped in obscured region\n");
        } else {
            print("FAIL: WritePixel drew in obscured region! Got color: ");
            print_num(color);
            print(" (expected 3)\n");
            errors++;
        }
    }

cleanup:
    /* Clean up */
    print("\nCleaning up...\n");
    if (layer2) {
        DeleteLayer(0, layer2);
        print("OK: Deleted layer2\n");
    }
    if (layer1) {
        DeleteLayer(0, layer1);
        print("OK: Deleted layer1\n");
    }

    FreeRaster(bm->Planes[0], 320, 200);
    FreeRaster(bm->Planes[1], 320, 200);
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    /* Final result */
    if (errors == 0) {
        print("\nPASS: graphics/layer_clipping all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: graphics/layer_clipping had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
