/*
 * Test: layers/smart_refresh
 *
 * Tests SMART_REFRESH backing store behavior:
 * - Content drawn into a SMART_REFRESH layer should be preserved
 *   when another layer obscures it, and restored when uncovered.
 * - Verifies that RebuildClipRects correctly saves/restores
 *   backing store bitmaps for SMART_REFRESH layers.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/regions.h>
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

    while (*p++)
        len++;
    Write(out, (CONST APTR)s, len);
}

static void print_num(LONG n)
{
    char buf[16];
    char *p = buf + sizeof(buf) - 1;
    BOOL neg = FALSE;

    *p = '\0';
    if (n < 0)
    {
        neg = TRUE;
        n = -n;
    }
    if (n == 0)
    {
        *--p = '0';
    }
    else
    {
        while (n > 0)
        {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    if (neg)
        *--p = '-';
    print(p);
}

static int count_cliprects(struct Layer *layer)
{
    int count = 0;
    struct ClipRect *cr;

    if (!layer)
        return 0;

    cr = layer->ClipRect;
    while (cr)
    {
        count++;
        cr = cr->Next;
    }

    return count;
}

static int count_obscured_cliprects(struct Layer *layer)
{
    int count = 0;
    struct ClipRect *cr;

    if (!layer)
        return 0;

    cr = layer->ClipRect;
    while (cr)
    {
        if (cr->obscured)
            count++;
        cr = cr->Next;
    }

    return count;
}

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *back_layer;
    struct Layer *front_layer;
    struct RastPort rp;
    int errors = 0;
    LONG pixel;
    WORD depth = 4; /* 16 colors */
    WORD width = 320;
    WORD height = 200;
    WORD i;

    print("Testing SMART_REFRESH backing store...\n");

    if (!LayersBase)
    {
        print("FAIL: LayersBase is NULL\n");
        return 20;
    }

    li = NewLayerInfo();
    if (!li)
    {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }

    /* Allocate screen bitmap */
    bm = AllocBitMap(width, height, depth, BMF_CLEAR, NULL);
    if (!bm)
    {
        print("FAIL: Could not allocate BitMap\n");
        DisposeLayerInfo(li);
        return 20;
    }

    /*
     * Test 1: Basic backing store - obscure and uncover
     *
     * Create a SMART_REFRESH back layer at [10,10]-[100,100].
     * Draw a filled rectangle with pen 3 at layer-relative coords.
     * Create a front layer that obscures part of it.
     * Move the front layer away.
     * Verify the original content is restored.
     */
    print("\nTest 1: Basic SMART_REFRESH backing store...\n");

    /* Create back layer (SMART_REFRESH = LAYERSMART) */
    back_layer = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSMART, NULL);
    if (!back_layer)
    {
        print("FAIL: Could not create back layer\n");
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Draw into back layer: fill a region with pen 3 */
    InitRastPort(&rp);
    rp.BitMap = bm;
    rp.Layer = back_layer;

    SetAPen(&rp, 3);
    /* Fill layer-relative [20,20]-[60,60] = screen-absolute [30,30]-[70,70] */
    RectFill(&rp, 20, 20, 60, 60);

    /* Verify the drawn content before obscuring */
    /* Layer-relative (30,30) = screen-absolute (40,40) */
    pixel = ReadPixel(&rp, 30, 30);
    if (pixel == 3)
    {
        print("OK: Initial content pen=3 at (30,30)\n");
    }
    else
    {
        print("FAIL: Expected pen 3 at (30,30), got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Also verify pen=0 outside the filled area */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 0)
    {
        print("OK: Background pen=0 at (5,5)\n");
    }
    else
    {
        print("FAIL: Expected pen 0 at (5,5), got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Create front layer that obscures the center of back layer.
     * Front layer at screen coords [30,30]-[70,70] overlaps with
     * the filled area of the back layer. */
    front_layer = CreateUpfrontLayer(li, bm, 30, 30, 70, 70, LAYERSMART, NULL);
    if (!front_layer)
    {
        print("FAIL: Could not create front layer\n");
        DeleteLayer(0, back_layer);
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    print("OK: Front layer created, back layer partially obscured\n");

    /* Verify back layer has some obscured ClipRects */
    {
        int total = count_cliprects(back_layer);
        int obscured = count_obscured_cliprects(back_layer);

        print("  Back layer ClipRects: total=");
        print_num(total);
        print(" obscured=");
        print_num(obscured);
        print("\n");

        if (obscured > 0)
        {
            print("OK: Back layer has obscured ClipRects (backing store allocated)\n");
        }
        else
        {
            print("FAIL: Back layer has no obscured ClipRects\n");
            errors++;
        }
    }

    /* Draw into the front layer with pen 5 to fill it */
    {
        struct RastPort front_rp;
        InitRastPort(&front_rp);
        front_rp.BitMap = bm;
        front_rp.Layer = front_layer;
        SetAPen(&front_rp, 5);
        RectFill(&front_rp, 0, 0,
                 front_layer->bounds.MaxX - front_layer->bounds.MinX,
                 front_layer->bounds.MaxY - front_layer->bounds.MinY);
    }

    /* Verify front layer content covers the screen area */
    {
        struct RastPort front_rp;
        InitRastPort(&front_rp);
        front_rp.BitMap = bm;
        front_rp.Layer = front_layer;
        pixel = ReadPixel(&front_rp, 5, 5);
        if (pixel == 5)
        {
            print("OK: Front layer drawn with pen=5\n");
        }
        else
        {
            print("FAIL: Front layer pixel expected 5, got ");
            print_num(pixel);
            print("\n");
            errors++;
        }
    }

    /* Now delete the front layer — back layer should be restored */
    DeleteLayer(0, front_layer);
    front_layer = NULL;

    print("OK: Front layer deleted, checking back layer restoration...\n");

    /* Verify the back layer's content was restored */
    /* The filled area (pen 3) should be back */
    pixel = ReadPixel(&rp, 30, 30);
    if (pixel == 3)
    {
        print("OK: Restored content pen=3 at (30,30) after uncover\n");
    }
    else
    {
        print("FAIL: Expected pen 3 at (30,30) after uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify another point in the filled area */
    pixel = ReadPixel(&rp, 25, 25);
    if (pixel == 3)
    {
        print("OK: Restored content pen=3 at (25,25)\n");
    }
    else
    {
        print("FAIL: Expected pen 3 at (25,25) after uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify background area (pen 0) is preserved */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 0)
    {
        print("OK: Background pen=0 at (5,5) after uncover\n");
    }
    else
    {
        print("FAIL: Expected pen 0 at (5,5) after uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Clean up test 1 */
    DeleteLayer(0, back_layer);

    /*
     * Test 2: MoveLayer triggers backing store save/restore
     *
     * Create two layers. Draw content in back. Move front layer
     * to obscure, then move it away. Content should be preserved.
     */
    print("\nTest 2: MoveLayer backing store save/restore...\n");

    /* Create back layer */
    back_layer = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSMART, NULL);
    if (!back_layer)
    {
        print("FAIL: Could not create back layer for test 2\n");
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Draw with pen 7 into back layer at [30,30]-[50,50] layer-relative */
    rp.Layer = back_layer;
    SetAPen(&rp, 7);
    RectFill(&rp, 30, 30, 50, 50);

    /* Verify initial content */
    pixel = ReadPixel(&rp, 40, 40);
    if (pixel == 7)
    {
        print("OK: Initial content pen=7 at (40,40)\n");
    }
    else
    {
        print("FAIL: Expected pen 7 at (40,40), got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Create front layer at non-overlapping position first */
    front_layer = CreateUpfrontLayer(li, bm, 150, 150, 200, 200, LAYERSMART, NULL);
    if (!front_layer)
    {
        print("FAIL: Could not create front layer for test 2\n");
        DeleteLayer(0, back_layer);
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Fill front layer with pen 9 */
    {
        struct RastPort front_rp;
        InitRastPort(&front_rp);
        front_rp.BitMap = bm;
        front_rp.Layer = front_layer;
        SetAPen(&front_rp, 9);
        RectFill(&front_rp, 0, 0,
                 front_layer->bounds.MaxX - front_layer->bounds.MinX,
                 front_layer->bounds.MaxY - front_layer->bounds.MinY);
    }

    /* Move front layer to overlap with back layer's drawn content.
     * Move to screen [35,35]-[85,85] — dx = 35-150 = -115, dy = 35-150 = -115 */
    MoveLayer(0, front_layer, -115, -115);

    print("OK: Front layer moved to overlap back layer\n");

    /* Verify back layer has obscured CRs */
    {
        int obscured = count_obscured_cliprects(back_layer);
        if (obscured > 0)
        {
            print("OK: Back layer has obscured ClipRects after MoveLayer\n");
        }
        else
        {
            print("FAIL: Back layer has no obscured ClipRects after MoveLayer\n");
            errors++;
        }
    }

    /* Move front layer back to non-overlapping position */
    MoveLayer(0, front_layer, 115, 115);

    print("OK: Front layer moved away, checking restoration...\n");

    /* Verify content was restored */
    pixel = ReadPixel(&rp, 40, 40);
    if (pixel == 7)
    {
        print("OK: Restored content pen=7 at (40,40) after MoveLayer\n");
    }
    else
    {
        print("FAIL: Expected pen 7 at (40,40) after MoveLayer, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify another point in the filled area */
    pixel = ReadPixel(&rp, 35, 35);
    if (pixel == 7)
    {
        print("OK: Restored content pen=7 at (35,35)\n");
    }
    else
    {
        print("FAIL: Expected pen 7 at (35,35) after MoveLayer, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify background outside the filled area */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 0)
    {
        print("OK: Background pen=0 at (5,5) preserved\n");
    }
    else
    {
        print("FAIL: Expected pen 0 at (5,5) after MoveLayer, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Clean up test 2 */
    DeleteLayer(0, front_layer);
    DeleteLayer(0, back_layer);

    /*
     * Test 3: SIMPLE_REFRESH should NOT have backing store
     *
     * Verify that LAYERSIMPLE layers do NOT get backing store
     * (they should get damage/REFRESHWINDOW instead).
     */
    print("\nTest 3: SIMPLE_REFRESH has no backing store...\n");

    back_layer = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSIMPLE, NULL);
    front_layer = CreateUpfrontLayer(li, bm, 30, 30, 70, 70, LAYERSIMPLE, NULL);
    if (!back_layer || !front_layer)
    {
        print("FAIL: Could not create layers for test 3\n");
        if (front_layer) DeleteLayer(0, front_layer);
        if (back_layer) DeleteLayer(0, back_layer);
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    {
        int obscured = count_obscured_cliprects(back_layer);
        if (obscured == 0)
        {
            print("OK: SIMPLE_REFRESH layer has no obscured ClipRects (no backing store)\n");
        }
        else
        {
            print("FAIL: SIMPLE_REFRESH layer should not have obscured ClipRects, got ");
            print_num(obscured);
            print("\n");
            errors++;
        }
    }

    DeleteLayer(0, front_layer);
    DeleteLayer(0, back_layer);

    /*
     * Test 4: Full obscure and uncover
     *
     * A front layer that completely covers the back layer.
     * Content should still be restored when the front is deleted.
     */
    print("\nTest 4: Full obscure/uncover...\n");

    back_layer = CreateUpfrontLayer(li, bm, 40, 40, 60, 60, LAYERSMART, NULL);
    if (!back_layer)
    {
        print("FAIL: Could not create back layer for test 4\n");
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Draw pen 11 into the entire back layer */
    rp.Layer = back_layer;
    SetAPen(&rp, 11);
    RectFill(&rp, 0, 0,
             back_layer->bounds.MaxX - back_layer->bounds.MinX,
             back_layer->bounds.MaxY - back_layer->bounds.MinY);

    /* Verify */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 11)
    {
        print("OK: Back layer filled with pen=11\n");
    }
    else
    {
        print("FAIL: Expected pen 11, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Create front layer that fully covers back */
    front_layer = CreateUpfrontLayer(li, bm, 30, 30, 70, 70, LAYERSMART, NULL);
    if (!front_layer)
    {
        print("FAIL: Could not create front layer for test 4\n");
        DeleteLayer(0, back_layer);
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Fill front with pen 13 */
    {
        struct RastPort front_rp;
        InitRastPort(&front_rp);
        front_rp.BitMap = bm;
        front_rp.Layer = front_layer;
        SetAPen(&front_rp, 13);
        RectFill(&front_rp, 0, 0,
                 front_layer->bounds.MaxX - front_layer->bounds.MinX,
                 front_layer->bounds.MaxY - front_layer->bounds.MinY);
    }

    /* Verify back layer is fully obscured */
    {
        int total = count_cliprects(back_layer);
        int obscured = count_obscured_cliprects(back_layer);

        if (total > 0 && total == obscured)
        {
            print("OK: Back layer is fully obscured\n");
        }
        else
        {
            print("FAIL: Expected all CRs obscured, total=");
            print_num(total);
            print(" obscured=");
            print_num(obscured);
            print("\n");
            errors++;
        }
    }

    /* Delete front layer */
    DeleteLayer(0, front_layer);
    front_layer = NULL;

    /* Verify restoration */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 11)
    {
        print("OK: Fully obscured content pen=11 restored\n");
    }
    else
    {
        print("FAIL: Expected pen 11 after full uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Check center point too */
    pixel = ReadPixel(&rp, 10, 10);
    if (pixel == 11)
    {
        print("OK: Center pixel pen=11 restored\n");
    }
    else
    {
        print("FAIL: Expected pen 11 at center after full uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    DeleteLayer(0, back_layer);

    /*
     * Test 5: Drawing while partially obscured
     *
     * Create a SMART_REFRESH layer, partially obscure it,
     * then draw into the back layer (some drawing goes to the
     * obscured area via backing store). When the front layer is
     * removed, the drawn content should be visible everywhere.
     */
    print("\nTest 5: Drawing while partially obscured...\n");

    back_layer = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSMART, NULL);
    if (!back_layer)
    {
        print("FAIL: Could not create back layer for test 5\n");
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Create front layer that obscures [40,40]-[80,80] of the back layer's
     * screen area, which overlaps with layer-relative [30,30]-[70,70] */
    front_layer = CreateUpfrontLayer(li, bm, 40, 40, 80, 80, LAYERSMART, NULL);
    if (!front_layer)
    {
        print("FAIL: Could not create front layer for test 5\n");
        DeleteLayer(0, back_layer);
        FreeBitMap(bm);
        DisposeLayerInfo(li);
        return 20;
    }

    /* Now draw into the back layer WHILE the front layer obscures it.
     * RectFill with pen 6 at layer-relative [20,20]-[60,60] which
     * spans both visible and obscured areas. */
    rp.Layer = back_layer;
    SetAPen(&rp, 6);
    RectFill(&rp, 20, 20, 60, 60);

    /* Delete the front layer to uncover */
    DeleteLayer(0, front_layer);
    front_layer = NULL;

    /* Verify content in the area that was visible when drawn */
    pixel = ReadPixel(&rp, 25, 25);
    if (pixel == 6)
    {
        print("OK: Visible-area draw pen=6 at (25,25)\n");
    }
    else
    {
        print("FAIL: Expected pen 6 at (25,25), got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify content in the area that was OBSCURED when drawn.
     * Layer-relative (40,40) = screen-absolute (50,50) which was
     * covered by the front layer at [40,40]-[80,80]. The RectFill
     * should have gone into the backing store and then been restored. */
    pixel = ReadPixel(&rp, 40, 40);
    if (pixel == 6)
    {
        print("OK: Obscured-area draw pen=6 at (40,40) after uncover\n");
    }
    else
    {
        print("FAIL: Expected pen 6 at (40,40) after uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify corner of the filled area that was also obscured */
    pixel = ReadPixel(&rp, 50, 50);
    if (pixel == 6)
    {
        print("OK: Obscured corner pen=6 at (50,50) after uncover\n");
    }
    else
    {
        print("FAIL: Expected pen 6 at (50,50) after uncover, got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    /* Verify area outside the RectFill is still 0 */
    pixel = ReadPixel(&rp, 5, 5);
    if (pixel == 0)
    {
        print("OK: Background pen=0 at (5,5)\n");
    }
    else
    {
        print("FAIL: Expected pen 0 at (5,5), got ");
        print_num(pixel);
        print("\n");
        errors++;
    }

    DeleteLayer(0, back_layer);

    /* Final cleanup */
    print("\nCleaning up...\n");
    FreeBitMap(bm);
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    if (errors == 0)
    {
        print("\nPASS: layers/smart_refresh all tests passed\n");
        return 0;
    }

    print("\nFAIL: layers/smart_refresh had ");
    print_num(errors);
    print(" errors\n");
    return 20;
}
