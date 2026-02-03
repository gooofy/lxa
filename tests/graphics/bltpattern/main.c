/*
 * Test: graphics/bltpattern
 * Tests BltPattern() for pattern blitting operations
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

/* Count pixels with specific color in a region */
static int CountPixels(struct RastPort *rp, LONG x1, LONG y1, LONG x2, LONG y2, LONG color)
{
    int count = 0;
    LONG x, y;
    
    for (y = y1; y <= y2; y++) {
        for (x = x1; x <= x2; x++) {
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
    UBYTE mask[8];  /* 8x8 mask */
    int errors = 0;
    int count;
    int i;

    print("Testing BltPattern()...\n");

    /* Setup: 64x64 bitmap with 1 plane */
    InitBitMap(&bm, 1, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);

    if (!bm.Planes[0]) {
        print("FAIL: Could not allocate raster plane\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Test 1: BltPattern without mask (should act like RectFill) */
    print("\n=== Test 1: BltPattern without mask ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Blit a rectangle without mask - should fill solid */
    BltPattern(&rp, NULL, 10, 10, 30, 30, 0);
    
    /* Check that the rectangle is filled */
    count = CountPixels(&rp, 10, 10, 30, 30, 1);
    if (count < 400) {  /* Should be 21x21 = 441 */
        print("FAIL: Rectangle not filled properly\n");
        errors++;
    } else {
        print("OK: Rectangle filled without mask\n");
    }
    
    /* Check outside is not filled */
    count = CountPixels(&rp, 0, 0, 9, 9, 1);
    if (count > 0) {
        print("FAIL: Pixels outside rectangle were filled\n");
        errors++;
    } else {
        print("OK: Outside rectangle not filled\n");
    }

    /* Test 2: BltPattern with mask (checkerboard pattern) */
    print("\n=== Test 2: BltPattern with mask ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Create a checkerboard mask (8x8) */
    /* Each byte is a row: 0xAA = 10101010, 0x55 = 01010101 */
    mask[0] = 0xAA;  /* 10101010 */
    mask[1] = 0x55;  /* 01010101 */
    mask[2] = 0xAA;
    mask[3] = 0x55;
    mask[4] = 0xAA;
    mask[5] = 0x55;
    mask[6] = 0xAA;
    mask[7] = 0x55;
    
    /* Blit 8x8 pattern */
    BltPattern(&rp, (PLANEPTR)mask, 10, 10, 17, 17, 1);
    
    /* Check that some pixels are set (not all, due to mask) */
    count = CountPixels(&rp, 10, 10, 17, 17, 1);
    if (count < 20 || count > 50) {  /* Should be ~32 (half of 64) */
        print("FAIL: Mask pattern not applied correctly\n");
        errors++;
    } else {
        print("OK: Mask pattern applied\n");
    }
    
    /* Verify checkerboard: pixel (10,10) should be set (first bit of 0xAA) */
    if (ReadPixel(&rp, 10, 10) != 1) {
        print("FAIL: Checkerboard pattern incorrect at (10,10)\n");
        errors++;
    } else {
        print("OK: Checkerboard pattern correct\n");
    }

    /* Test 3: BltPattern with all-zero mask (should draw nothing) */
    print("\n=== Test 3: BltPattern with zero mask ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Clear mask */
    for (i = 0; i < 8; i++)
        mask[i] = 0x00;
    
    BltPattern(&rp, (PLANEPTR)mask, 10, 10, 17, 17, 1);
    
    /* Nothing should be drawn */
    count = CountPixels(&rp, 10, 10, 17, 17, 1);
    if (count > 0) {
        print("FAIL: Zero mask drew pixels\n");
        errors++;
    } else {
        print("OK: Zero mask drew nothing\n");
    }

    /* Test 4: BltPattern with all-one mask (should fill completely) */
    print("\n=== Test 4: BltPattern with full mask ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Set all bits in mask */
    for (i = 0; i < 8; i++)
        mask[i] = 0xFF;
    
    BltPattern(&rp, (PLANEPTR)mask, 10, 10, 17, 17, 1);
    
    /* All pixels should be drawn */
    count = CountPixels(&rp, 10, 10, 17, 17, 1);
    if (count != 64) {  /* 8x8 = 64 */
        print("FAIL: Full mask did not fill all pixels\n");
        errors++;
    } else {
        print("OK: Full mask filled all pixels\n");
    }

    /* Test 5: BltPattern with larger area */
    print("\n=== Test 5: BltPattern with larger area ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Use checkerboard mask */
    mask[0] = 0xAA;
    mask[1] = 0x55;
    mask[2] = 0xAA;
    mask[3] = 0x55;
    mask[4] = 0xAA;
    mask[5] = 0x55;
    mask[6] = 0xAA;
    mask[7] = 0x55;
    
    /* Blit 16x16 area - BltTemplate should handle wrapping if supported */
    BltPattern(&rp, (PLANEPTR)mask, 10, 10, 25, 25, 1);
    
    /* Check that some pixels were set */
    count = CountPixels(&rp, 10, 10, 25, 25, 1);
    if (count < 50) {  /* At least some pixels should be set */
        print("FAIL: Not enough pixels drawn\n");
        errors++;
    } else {
        print("OK: Larger area pattern applied\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("\nPASS: BltPattern() all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: BltPattern() had errors\n");
        return 20;
    }
}
