/*
 * Test: graphics/flood
 * Tests Flood() for flood fill operations
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/dos_protos.h>
#include <inline/exec.h>
#include <inline/graphics.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;
extern struct ExecBase *SysBase;
extern struct GfxBase *GfxBase;

static void print(const char *s)
{
    BPTR out = Output();
    LONG len = 0;
    const char *p = s;
    while (*p++) len++;
    Write(out, (CONST APTR)s, len);
}

/* Count pixels with specific color in entire bitmap */
static int CountAllPixels(struct RastPort *rp, LONG color)
{
    int count = 0;
    LONG x, y;
    
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            if (ReadPixel(rp, x, y) == color) {
                count++;
            }
        }
    }
    return count;
}

int main(void)
{
    struct BitMap bm;
    struct RastPort rp;
    struct TmpRas tmpras;
    PLANEPTR tmpras_buffer;
    BOOL result;
    int errors = 0;
    int count;

    print("Testing Flood()...\n");

    /* Setup: 64x64 bitmap with 1 plane */
    InitBitMap(&bm, 1, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);

    if (!bm.Planes[0]) {
        print("FAIL: Could not allocate raster plane\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;
    
    /* Allocate and initialize TmpRas (required for Flood) */
    tmpras_buffer = AllocRaster(64, 64);
    if (!tmpras_buffer) {
        print("FAIL: Could not allocate tmpras buffer\n");
        FreeRaster(bm.Planes[0], 64, 64);
        return 20;
    }
    InitTmpRas(&tmpras, tmpras_buffer, RASSIZE(64, 64));
    rp.TmpRas = &tmpras;

    /* Test 1: Simple flood fill - fill entire empty bitmap */
    print("\n=== Test 1: Fill empty bitmap ===\n");
    SetRast(&rp, 0);  /* Clear to black */
    SetAPen(&rp, 1);  /* Fill pen = white */
    
    result = Flood(&rp, 0, 32, 32);  /* mode 0 = fill until outline color */
    
    if (!result) {
        print("FAIL: Flood returned FALSE\n");
        errors++;
    } else {
        print("OK: Flood returned TRUE\n");
    }
    
    /* Should have filled entire bitmap */
    count = CountAllPixels(&rp, 1);
    if (count != 64*64) {
        print("FAIL: Not all pixels filled\n");
        errors++;
    } else {
        print("OK: All 4096 pixels filled\n");
    }

    /* Test 2: Flood with mode 1 - fill same color region */
    print("\n=== Test 2: Mode 1 flood (fill same color) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Draw a small filled rectangle */
    RectFill(&rp, 20, 20, 30, 30);
    
    /* Count white pixels before flood */
    count = CountAllPixels(&rp, 1);
    print("OK: Before flood, white pixel count known\n");
    
    /* Now flood-fill with black starting from within the white rectangle */
    SetAPen(&rp, 0);
    result = Flood(&rp, 1, 25, 25);  /* mode 1 = fill while same color */
    
    if (!result) {
        print("FAIL: Mode 1 flood returned FALSE\n");
        errors++;
    } else {
        print("OK: Mode 1 flood returned TRUE\n");
    }
    
    /* The white rectangle should now be black - check if all black */
    count = CountAllPixels(&rp, 1);
    if (count > 0) {
        print("FAIL: Rectangle not completely filled with mode 1\n");
        errors++;
    } else {
        print("OK: Rectangle filled correctly with mode 1\n");
    }

    /* Test 3: Flood at boundary pixel */
    print("\n=== Test 3: Flood at filled pixel (boundary) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    WritePixel(&rp, 32, 32);
    
    /* Try to flood from the white pixel itself with mode 0 */
    result = Flood(&rp, 0, 32, 32);
    
    /* In mode 0, starting at outline pen should not fill */
    count = CountAllPixels(&rp, 1);
    if (count > 10) {
        print("FAIL: Flood at boundary filled too much\n");
        errors++;
    } else {
        print("OK: Flood at boundary behaved correctly\n");
    }

    /* Test 4: Flood with no TmpRas (should fail gracefully) */
    print("\n=== Test 4: Flood without TmpRas ===\n");
    rp.TmpRas = NULL;
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    result = Flood(&rp, 0, 32, 32);
    
    if (result) {
        print("FAIL: Flood without TmpRas should return FALSE\n");
        errors++;
    } else {
        print("OK: Flood without TmpRas returned FALSE\n");
    }
    
    /* Restore TmpRas for remaining tests */
    rp.TmpRas = &tmpras;

    /* Test 5: Flood out of bounds */
    print("\n=== Test 5: Flood out of bounds ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    result = Flood(&rp, 0, -10, -10);
    
    if (result) {
        print("FAIL: Out of bounds flood should return FALSE\n");
        errors++;
    } else {
        print("OK: Out of bounds flood returned FALSE\n");
    }

    /* Cleanup */
    FreeRaster(tmpras_buffer, 64, 64);
    FreeRaster(bm.Planes[0], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("\nPASS: Flood() all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: Flood() had errors\n");
        return 20;
    }
}
