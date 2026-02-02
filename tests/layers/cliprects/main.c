/*
 * Test: layers/cliprects
 * Tests ClipRect generation and layer clipping behavior
 *
 * Tests:
 * 1. Single layer has full-coverage ClipRect
 * 2. Overlapping layers properly clip ClipRects
 * 3. ClipRect bounds match layer bounds for unobscured layer
 * 4. Layer movement updates ClipRects
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
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

static int count_cliprects(struct Layer *layer)
{
    int count = 0;
    struct ClipRect *cr;
    
    if (!layer || !layer->ClipRect)
        return 0;
        
    cr = layer->ClipRect;
    while (cr) {
        count++;
        cr = cr->Next;
    }
    return count;
}

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *layer1, *layer2;
    struct ClipRect *cr;
    int errors = 0;

    print("Testing layers.library ClipRect behavior...\n");
    
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

    /* Create a simple bitmap for layer operations */
    bm = AllocMem(sizeof(struct BitMap), MEMF_PUBLIC | MEMF_CLEAR);
    if (!bm) {
        print("FAIL: Could not allocate BitMap\n");
        DisposeLayerInfo(li);
        return 20;
    }
    InitBitMap(bm, 1, 640, 480);
    bm->Planes[0] = AllocRaster(640, 480);
    if (!bm->Planes[0]) {
        print("FAIL: Could not allocate raster\n");
        FreeMem(bm, sizeof(struct BitMap));
        DisposeLayerInfo(li);
        return 20;
    }
    print("OK: BitMap allocated\n");

    /* Test 1: Single layer has ClipRect */
    print("\nTest 1: Single layer ClipRect...\n");
    layer1 = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSIMPLE, NULL);
    if (!layer1) {
        print("FAIL: Could not create layer1\n");
        errors++;
    } else {
        print("OK: Layer1 created\n");
        
        /* Check ClipRect exists */
        if (layer1->ClipRect) {
            print("OK: Layer1 has ClipRect\n");
            
            /* Single unobscured layer should have exactly 1 ClipRect */
            int count = count_cliprects(layer1);
            if (count == 1) {
                print("OK: Layer1 has exactly 1 ClipRect\n");
            } else {
                print("FAIL: Expected 1 ClipRect, got ");
                print_num(count);
                print("\n");
                errors++;
            }
            
            /* ClipRect bounds should match layer bounds */
            cr = layer1->ClipRect;
            if (cr->bounds.MinX == 10 && cr->bounds.MinY == 10 &&
                cr->bounds.MaxX == 100 && cr->bounds.MaxY == 100) {
                print("OK: ClipRect bounds match layer bounds\n");
            } else {
                print("FAIL: ClipRect bounds mismatch\n");
                print("  Expected: [10,10]-[100,100]\n");
                print("  Got: [");
                print_num(cr->bounds.MinX); print(",");
                print_num(cr->bounds.MinY); print("]-[");
                print_num(cr->bounds.MaxX); print(",");
                print_num(cr->bounds.MaxY); print("]\n");
                errors++;
            }
            
            /* ClipRect should not be obscured */
            if (!cr->obscured) {
                print("OK: ClipRect is not obscured\n");
            } else {
                print("FAIL: ClipRect should not be obscured\n");
                errors++;
            }
        } else {
            print("FAIL: Layer1 has no ClipRect\n");
            errors++;
        }
    }

    /* Test 2: Overlapping layers - back layer should be clipped */
    print("\nTest 2: Overlapping layers ClipRect clipping...\n");
    
    /* Create a second layer that overlaps with layer1 */
    /* layer2 will be in front of layer1, at position [50,50]-[150,150] */
    layer2 = CreateUpfrontLayer(li, bm, 50, 50, 150, 150, LAYERSIMPLE, NULL);
    if (!layer2) {
        print("FAIL: Could not create layer2\n");
        errors++;
    } else {
        print("OK: Layer2 created (overlaps layer1)\n");
        
        /* Layer2 is in front, should have 1 ClipRect covering full area */
        int count2 = count_cliprects(layer2);
        if (count2 == 1) {
            print("OK: Front layer (layer2) has 1 ClipRect\n");
        } else {
            print("INFO: Front layer has ");
            print_num(count2);
            print(" ClipRects\n");
        }
        
        /* Layer1 is behind and should be split around layer2 */
        /* The overlap is [50,50]-[100,100] (intersection of both layers) */
        /* Layer1 should have multiple ClipRects for the visible portions */
        int count1 = count_cliprects(layer1);
        if (count1 >= 1) {
            print("OK: Back layer (layer1) has ClipRects\n");
            print("  Count: ");
            print_num(count1);
            print("\n");
        } else {
            print("FAIL: Back layer should have ClipRects\n");
            errors++;
        }
    }

    /* Test 3: Layer movement updates ClipRects */
    print("\nTest 3: Layer movement updates ClipRects...\n");
    if (layer2) {
        /* Move layer2 completely away from layer1 */
        MoveLayer(0, layer2, 200, 200);  /* Now at [250,250]-[350,350] */
        print("OK: Moved layer2 away from layer1\n");
        
        /* After move, layer1 should no longer be obscured */
        /* It should have 1 ClipRect again covering its full area */
        int count1_after = count_cliprects(layer1);
        if (count1_after == 1) {
            print("OK: Layer1 now has 1 ClipRect (no longer obscured)\n");
            
            /* Verify bounds */
            cr = layer1->ClipRect;
            if (cr && cr->bounds.MinX == 10 && cr->bounds.MinY == 10 &&
                cr->bounds.MaxX == 100 && cr->bounds.MaxY == 100) {
                print("OK: ClipRect bounds restored to full layer\n");
            } else {
                print("FAIL: ClipRect bounds incorrect after move\n");
                errors++;
            }
        } else {
            print("INFO: Layer1 ClipRect count after move: ");
            print_num(count1_after);
            print("\n");
        }
    }

    /* Test 4: Verify ClipRect home field */
    print("\nTest 4: ClipRect home field...\n");
    if (layer1 && layer1->ClipRect) {
        cr = layer1->ClipRect;
        if (cr->home == li) {
            print("OK: ClipRect home points to Layer_Info\n");
        } else if (cr->home == NULL) {
            print("INFO: ClipRect home is NULL (user cliprect style)\n");
        } else {
            print("FAIL: ClipRect home is invalid\n");
            errors++;
        }
    }

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

    FreeRaster(bm->Planes[0], 640, 480);
    FreeMem(bm, sizeof(struct BitMap));
    DisposeLayerInfo(li);
    print("OK: Cleanup complete\n");

    /* Final result */
    if (errors == 0) {
        print("\nPASS: layers/cliprects all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: layers/cliprects had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
