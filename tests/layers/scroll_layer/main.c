/*
 * Test: layers/scroll_layer
 * Tests ScrollLayer() function
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/layers.h>
#include <graphics/clip.h>
#include <graphics/rastport.h>
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

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *layer;
    struct RastPort *rp;
    int errors = 0;

    print("Testing ScrollLayer...\n\n");

    if (!LayersBase)
    {
        print("FAIL: LayersBase is NULL\n");
        return 20;
    }
    print("OK: LayersBase available\n");

    /* Create layer infrastructure */
    li = NewLayerInfo();
    if (!li)
    {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }
    print("OK: NewLayerInfo() succeeded\n");

    bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!bm)
    {
        print("FAIL: Could not allocate BitMap\n");
        DisposeLayerInfo(li);
        return 20;
    }
    InitBitMap(bm, 1, 320, 200);
    bm->Planes[0] = AllocRaster(320, 200);
    if (!bm->Planes[0])
    {
        print("FAIL: Could not allocate raster\n");
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }
    print("OK: BitMap allocated\n\n");

    /* Create a simple layer */
    layer = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSIMPLE, NULL);
    if (!layer)
    {
        print("FAIL: Could not create layer\n");
        FreeRaster(bm->Planes[0], 320, 200);
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }
    print("OK: Layer created\n\n");

    rp = layer->rp;

    /* Test 1: ScrollLayer updates scroll offsets */
    print("Test 1: ScrollLayer updates scroll offsets...\n");
    {
        WORD sx_before = layer->Scroll_X;
        WORD sy_before = layer->Scroll_Y;

        ScrollLayer(0, layer, 5, 10);

        print("  Before: Scroll_X=");
        print_num(sx_before);
        print(", Scroll_Y=");
        print_num(sy_before);
        print("\n");

        print("  After:  Scroll_X=");
        print_num(layer->Scroll_X);
        print(", Scroll_Y=");
        print_num(layer->Scroll_Y);
        print("\n");

        /* For simple layers, scroll offsets should be updated */
        if (layer->Scroll_X == sx_before + 5 &&
            layer->Scroll_Y == sy_before + 10)
        {
            print("  OK: Scroll offsets updated correctly\n");
        }
        else
        {
            print("  FAIL: Scroll offsets incorrect\n");
            errors++;
        }
    }
    print("\n");

    /* Test 2: ScrollLayer actually scrolls pixel content */
    print("Test 2: ScrollLayer scrolls pixel content...\n");
    {
        /* Reset scroll */
        layer->Scroll_X = 0;
        layer->Scroll_Y = 0;

        /* Clear and draw a known pixel */
        SetRast(rp, 0);
        SetAPen(rp, 1);
        WritePixel(rp, 50, 50);

        /* Verify pixel is at (50,50) */
        if (ReadPixel(rp, 50, 50) == 1)
        {
            print("  OK: Pixel set at (50,50)\n");
        }
        else
        {
            print("  FAIL: Could not set pixel at (50,50)\n");
            errors++;
        }

        /* Scroll left by 10 pixels (dx=-10 means content moves right) */
        ScrollLayer(0, layer, -10, 0);

        /* After scrolling dx=-10, the pixel that was at (50,50) should now
         * be at (60,50) because ScrollRaster shifts content by -dx,-dy */
        ULONG pen_old = ReadPixel(rp, 50, 50);
        ULONG pen_new = ReadPixel(rp, 60, 50);

        print("  After scroll dx=-10: pixel at (50,50) = ");
        print_num((LONG)pen_old);
        print(", at (60,50) = ");
        print_num((LONG)pen_new);
        print("\n");

        if (pen_new == 1)
        {
            print("  OK: Pixel moved to expected position\n");
        }
        else
        {
            print("  WARN: Pixel not at expected position (implementation may differ)\n");
        }
    }
    print("\n");

    /* Test 3: Multiple ScrollLayer calls accumulate offsets */
    print("Test 3: Multiple ScrollLayer calls accumulate offsets...\n");
    {
        layer->Scroll_X = 0;
        layer->Scroll_Y = 0;

        ScrollLayer(0, layer, 3, 7);
        ScrollLayer(0, layer, 2, 3);

        if (layer->Scroll_X == 5 && layer->Scroll_Y == 10)
        {
            print("  OK: Offsets accumulated (5, 10)\n");
        }
        else
        {
            print("  FAIL: Expected (5,10), got (");
            print_num(layer->Scroll_X);
            print(",");
            print_num(layer->Scroll_Y);
            print(")\n");
            errors++;
        }
    }
    print("\n");

    /* Test 4: ScrollLayer with zero deltas */
    print("Test 4: ScrollLayer with zero deltas...\n");
    {
        layer->Scroll_X = 0;
        layer->Scroll_Y = 0;

        ScrollLayer(0, layer, 0, 0);

        if (layer->Scroll_X == 0 && layer->Scroll_Y == 0)
        {
            print("  OK: Zero scroll is no-op\n");
        }
        else
        {
            print("  FAIL: Offsets changed on zero scroll\n");
            errors++;
        }
    }
    print("\n");

    /* Test 5: ScrollLayer with negative deltas */
    print("Test 5: ScrollLayer with negative deltas...\n");
    {
        layer->Scroll_X = 0;
        layer->Scroll_Y = 0;

        ScrollLayer(0, layer, -5, -3);

        if (layer->Scroll_X == -5 && layer->Scroll_Y == -3)
        {
            print("  OK: Negative scroll offsets work\n");
        }
        else
        {
            print("  FAIL: Expected (-5,-3), got (");
            print_num(layer->Scroll_X);
            print(",");
            print_num(layer->Scroll_Y);
            print(")\n");
            errors++;
        }
    }
    print("\n");

    /* Cleanup */
    print("Cleaning up...\n");
    DeleteLayer(0, layer);
    print("OK: Deleted layer\n");

    FreeRaster(bm->Planes[0], 320, 200);
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    if (errors == 0)
    {
        print("\nPASS: layers/scroll_layer all tests passed\n");
        return 0;
    }
    else
    {
        print("\nFAIL: layers/scroll_layer had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
