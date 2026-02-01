/*
 * Test: layers/layer_info
 * Tests NewLayerInfo(), DisposeLayerInfo(), and InitLayers() functions
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

int main(void)
{
    struct Layer_Info *li;
    int errors = 0;

    print("Testing layers.library Layer_Info management...\n");
    
    /* Check LayersBase is available */
    if (!LayersBase) {
        print("FAIL: LayersBase is NULL\n");
        return 20;
    }
    print("OK: LayersBase available\n");

    /* Test NewLayerInfo() */
    print("\nTesting NewLayerInfo()...\n");
    li = NewLayerInfo();
    if (!li) {
        print("FAIL: NewLayerInfo() returned NULL\n");
        return 20;
    }
    print("OK: NewLayerInfo() returned non-NULL\n");

    /* Check Layer_Info is properly initialized */
    if (li->top_layer != NULL) {
        print("FAIL: top_layer is not NULL\n");
        errors++;
    } else {
        print("OK: top_layer is NULL (no layers yet)\n");
    }

    if (li->FreeClipRects != NULL) {
        print("FAIL: FreeClipRects is not NULL\n");
        errors++;
    } else {
        print("OK: FreeClipRects is NULL\n");
    }

    /* Check bounds are set to maximum */
    if (li->bounds.MinX != 0) {
        print("FAIL: bounds.MinX != 0\n");
        errors++;
    } else {
        print("OK: bounds.MinX = 0\n");
    }

    if (li->bounds.MinY != 0) {
        print("FAIL: bounds.MinY != 0\n");
        errors++;
    } else {
        print("OK: bounds.MinY = 0\n");
    }

    if (li->bounds.MaxX < 1000) {
        print("FAIL: bounds.MaxX too small\n");
        errors++;
    } else {
        print("OK: bounds.MaxX reasonable\n");
    }

    if (li->bounds.MaxY < 1000) {
        print("FAIL: bounds.MaxY too small\n");
        errors++;
    } else {
        print("OK: bounds.MaxY reasonable\n");
    }

    /* Check Flags */
    if (!(li->Flags & NEWLAYERINFO_CALLED)) {
        print("FAIL: NEWLAYERINFO_CALLED flag not set\n");
        errors++;
    } else {
        print("OK: NEWLAYERINFO_CALLED flag set\n");
    }

    /* Check LockLayersCount */
    if (li->LockLayersCount != 0) {
        print("FAIL: LockLayersCount != 0\n");
        errors++;
    } else {
        print("OK: LockLayersCount = 0\n");
    }

    /* Test LockLayerInfo/UnlockLayerInfo */
    print("\nTesting LockLayerInfo/UnlockLayerInfo...\n");
    LockLayerInfo(li);
    print("OK: LockLayerInfo() called\n");
    UnlockLayerInfo(li);
    print("OK: UnlockLayerInfo() called\n");

    /* Test DisposeLayerInfo() */
    print("\nTesting DisposeLayerInfo()...\n");
    DisposeLayerInfo(li);
    print("OK: DisposeLayerInfo() completed\n");

    /* Test InitLayers on pre-allocated structure */
    print("\nTesting InitLayers() on allocated memory...\n");
    li = AllocMem(sizeof(struct Layer_Info), MEMF_PUBLIC | MEMF_CLEAR);
    if (!li) {
        print("FAIL: Could not allocate Layer_Info\n");
        return 20;
    }

    InitLayers(li);
    if (li->top_layer != NULL) {
        print("FAIL: InitLayers didn't clear top_layer\n");
        errors++;
    } else {
        print("OK: InitLayers initialized top_layer to NULL\n");
    }

    if (!(li->Flags & NEWLAYERINFO_CALLED)) {
        print("FAIL: InitLayers didn't set NEWLAYERINFO_CALLED\n");
        errors++;
    } else {
        print("OK: InitLayers set NEWLAYERINFO_CALLED\n");
    }

    FreeMem(li, sizeof(struct Layer_Info));
    print("OK: Freed manually allocated Layer_Info\n");

    /* Final result */
    if (errors == 0) {
        print("\nPASS: layers/layer_info all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: layers/layer_info had errors\n");
        return 20;
    }
}
