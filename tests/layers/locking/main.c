/*
 * Test: layers/locking
 * Tests LockLayer, UnlockLayer, LockLayerInfo, UnlockLayerInfo, 
 * LockLayers, UnlockLayers functions
 *
 * Tests:
 * 1. Basic locking/unlocking of single layer
 * 2. LockLayerInfo/UnlockLayerInfo
 * 3. LockLayers/UnlockLayers (locks all layers)
 * 4. Nested locking (same task can lock multiple times)
 * 5. Layer creation requires locking
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

int main(void)
{
    struct Layer_Info *li;
    struct BitMap *bm;
    struct Layer *layer1, *layer2;
    int errors = 0;

    print("Testing layers.library locking functions...\n");
    
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

    /* Test 1: LockLayerInfo / UnlockLayerInfo */
    print("\nTest 1: LockLayerInfo/UnlockLayerInfo...\n");
    LockLayerInfo(li);
    print("OK: LockLayerInfo() called\n");
    
    /* Check LockLayersCount is still 0 (LockLayerInfo doesn't increment it) */
    if (li->LockLayersCount != 0) {
        print("FAIL: LockLayerInfo changed LockLayersCount\n");
        errors++;
    } else {
        print("OK: LockLayersCount unchanged by LockLayerInfo\n");
    }
    
    UnlockLayerInfo(li);
    print("OK: UnlockLayerInfo() called\n");

    /* Test 2: Create layers for testing */
    print("\nTest 2: Creating test layers...\n");
    layer1 = CreateUpfrontLayer(li, bm, 10, 10, 100, 100, LAYERSIMPLE, NULL);
    if (!layer1) {
        print("FAIL: Could not create layer1\n");
        errors++;
    } else {
        print("OK: Layer1 created\n");
    }

    layer2 = CreateUpfrontLayer(li, bm, 50, 50, 150, 150, LAYERSIMPLE, NULL);
    if (!layer2) {
        print("FAIL: Could not create layer2\n");
        errors++;
    } else {
        print("OK: Layer2 created\n");
    }

    /* Test 3: LockLayer / UnlockLayer */
    print("\nTest 3: LockLayer/UnlockLayer...\n");
    if (layer1) {
        LockLayer(0, layer1);
        print("OK: LockLayer() on layer1\n");
        
        /* Lock again (nested locking) */
        LockLayer(0, layer1);
        print("OK: Nested LockLayer() on layer1\n");
        
        /* Unlock both */
        UnlockLayer(layer1);
        print("OK: First UnlockLayer()\n");
        
        UnlockLayer(layer1);
        print("OK: Second UnlockLayer()\n");
    }

    /* Test 4: LockLayers / UnlockLayers */
    print("\nTest 4: LockLayers/UnlockLayers...\n");
    LockLayers(li);
    print("OK: LockLayers() called\n");
    
    /* Check LockLayersCount is incremented */
    if (li->LockLayersCount != 1) {
        print("FAIL: LockLayersCount should be 1, got ");
        print_num(li->LockLayersCount);
        print("\n");
        errors++;
    } else {
        print("OK: LockLayersCount is 1\n");
    }

    /* Lock again (nested) */
    LockLayers(li);
    if (li->LockLayersCount != 2) {
        print("FAIL: Nested LockLayersCount should be 2, got ");
        print_num(li->LockLayersCount);
        print("\n");
        errors++;
    } else {
        print("OK: Nested LockLayersCount is 2\n");
    }

    /* Unlock */
    UnlockLayers(li);
    if (li->LockLayersCount != 1) {
        print("FAIL: After unlock LockLayersCount should be 1, got ");
        print_num(li->LockLayersCount);
        print("\n");
        errors++;
    } else {
        print("OK: After first unlock, LockLayersCount is 1\n");
    }

    UnlockLayers(li);
    if (li->LockLayersCount != 0) {
        print("FAIL: After second unlock LockLayersCount should be 0, got ");
        print_num(li->LockLayersCount);
        print("\n");
        errors++;
    } else {
        print("OK: After second unlock, LockLayersCount is 0\n");
    }

    /* Test 5: WhichLayer while locked */
    print("\nTest 5: WhichLayer with locking...\n");
    LockLayerInfo(li);
    {
        struct Layer *found = WhichLayer(li, 75, 75);
        if (found == layer2) {
            print("OK: WhichLayer found layer2 at (75,75)\n");
        } else if (found == layer1) {
            /* layer1 is behind layer2, so this shouldn't happen */
            print("FAIL: WhichLayer found layer1 instead of layer2\n");
            errors++;
        } else if (!found) {
            print("FAIL: WhichLayer returned NULL\n");
            errors++;
        } else {
            print("FAIL: WhichLayer returned unexpected layer\n");
            errors++;
        }
        
        /* Test point outside all layers */
        found = WhichLayer(li, 200, 200);
        if (!found) {
            print("OK: WhichLayer returns NULL for point outside layers\n");
        } else {
            print("FAIL: WhichLayer should return NULL for (200,200)\n");
            errors++;
        }
    }
    UnlockLayerInfo(li);
    print("OK: WhichLayer test complete\n");

    /* Test 6: Layer ordering (layer2 should be in front of layer1) */
    print("\nTest 6: Layer ordering...\n");
    if (li->top_layer == layer2) {
        print("OK: Layer2 is top layer (most recently created upfront)\n");
    } else {
        print("FAIL: Top layer is not layer2\n");
        errors++;
    }
    
    if (layer2 && layer2->back == layer1) {
        print("OK: Layer1 is behind layer2\n");
    } else {
        print("FAIL: Layer order incorrect\n");
        errors++;
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
        print("\nPASS: layers/locking all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: layers/locking had ");
        print_num(errors);
        print(" errors\n");
        return 20;
    }
}
