/*
 * Test: graphics/draw_ellipse
 * Tests DrawEllipse() for ellipse drawing
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

int main(void)
{
    struct BitMap bm;
    struct RastPort rp;
    int errors = 0;
    LONG color;

    print("Testing DrawEllipse()...\n");

    /* Setup: 64x64 bitmap with 1 plane */
    InitBitMap(&bm, 1, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);

    if (!bm.Planes[0]) {
        print("FAIL: Could not allocate raster plane\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Test 1: Draw circle (a=b) */
    print("\n=== Test 1: Draw circle (r=10) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Draw circle centered at (32,32) with radius 10 */
    DrawEllipse(&rp, 32, 32, 10, 10);
    
    /* Check cardinal points are set */
    color = ReadPixel(&rp, 42, 32);  /* Right */
    if (color != 1) {
        print("FAIL: Right point not set\n");
        errors++;
    } else {
        print("OK: Right point set\n");
    }
    
    color = ReadPixel(&rp, 22, 32);  /* Left */
    if (color != 1) {
        print("FAIL: Left point not set\n");
        errors++;
    } else {
        print("OK: Left point set\n");
    }
    
    color = ReadPixel(&rp, 32, 22);  /* Top */
    if (color != 1) {
        print("FAIL: Top point not set\n");
        errors++;
    } else {
        print("OK: Top point set\n");
    }
    
    color = ReadPixel(&rp, 32, 42);  /* Bottom */
    if (color != 1) {
        print("FAIL: Bottom point not set\n");
        errors++;
    } else {
        print("OK: Bottom point set\n");
    }

    /* Test 2: Draw ellipse (a>b) - horizontal */
    print("\n=== Test 2: Draw horizontal ellipse ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Draw ellipse: a=15, b=8 */
    DrawEllipse(&rp, 32, 32, 15, 8);
    
    /* Check horizontal extent */
    color = ReadPixel(&rp, 47, 32);  /* Right (32+15) */
    if (color != 1) {
        print("FAIL: Horizontal extent not correct\n");
        errors++;
    } else {
        print("OK: Horizontal extent correct\n");
    }
    
    /* Check vertical extent */
    color = ReadPixel(&rp, 32, 40);  /* Bottom (32+8) */
    if (color != 1) {
        print("FAIL: Vertical extent not correct\n");
        errors++;
    } else {
        print("OK: Vertical extent correct\n");
    }

    /* Test 3: Draw ellipse (a<b) - vertical */
    print("\n=== Test 3: Draw vertical ellipse ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    /* Draw ellipse: a=6, b=12 */
    DrawEllipse(&rp, 32, 32, 6, 12);
    
    /* Check horizontal extent */
    color = ReadPixel(&rp, 38, 32);  /* Right (32+6) */
    if (color != 1) {
        print("FAIL: Horizontal extent not correct\n");
        errors++;
    } else {
        print("OK: Horizontal extent correct\n");
    }
    
    /* Check vertical extent */
    color = ReadPixel(&rp, 32, 44);  /* Bottom (32+12) */
    if (color != 1) {
        print("FAIL: Vertical extent not correct\n");
        errors++;
    } else {
        print("OK: Vertical extent correct\n");
    }

    /* Test 4: Degenerate case - point (a=0, b=0) */
    print("\n=== Test 4: Degenerate ellipse (point) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    DrawEllipse(&rp, 32, 32, 0, 0);
    
    /* Only center point should be set */
    color = ReadPixel(&rp, 32, 32);
    if (color != 1) {
        print("FAIL: Center point not set\n");
        errors++;
    } else {
        print("OK: Point drawn at center\n");
    }

    /* Test 5: Degenerate case - horizontal line (b=0) */
    print("\n=== Test 5: Degenerate ellipse (horizontal line) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    DrawEllipse(&rp, 32, 32, 10, 0);
    
    /* Should draw horizontal line */
    color = ReadPixel(&rp, 42, 32);
    if (color != 1) {
        print("FAIL: Horizontal line not drawn\n");
        errors++;
    } else {
        print("OK: Horizontal line drawn\n");
    }

    /* Test 6: Degenerate case - vertical line (a=0) */
    print("\n=== Test 6: Degenerate ellipse (vertical line) ===\n");
    SetRast(&rp, 0);
    SetAPen(&rp, 1);
    
    DrawEllipse(&rp, 32, 32, 0, 10);
    
    /* Should draw vertical line */
    color = ReadPixel(&rp, 32, 42);
    if (color != 1) {
        print("FAIL: Vertical line not drawn\n");
        errors++;
    } else {
        print("OK: Vertical line drawn\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("\nPASS: DrawEllipse() all tests passed\n");
        return 0;
    } else {
        print("\nFAIL: DrawEllipse() had errors\n");
        return 20;
    }
}
