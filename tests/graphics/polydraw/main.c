/*
 * Test: graphics/polydraw
 * Tests PolyDraw() for connected line drawing
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
    LONG color;
    int errors = 0;
    WORD polyTable[8];

    print("Testing PolyDraw()...\n");

    /* Setup: 64x64 bitmap with 1 plane */
    InitBitMap(&bm, 1, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);

    if (!bm.Planes[0]) {
        print("FAIL: Could not allocate raster plane\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Clear bitmap */
    SetRast(&rp, 0);

    /* Test: Draw a square using PolyDraw */
    SetAPen(&rp, 1);
    Move(&rp, 10, 10);  /* Start at (10,10) */
    
    /* Define square: (20,10), (20,20), (10,20), (10,10) */
    polyTable[0] = 20; polyTable[1] = 10;
    polyTable[2] = 20; polyTable[3] = 20;
    polyTable[4] = 10; polyTable[5] = 20;
    polyTable[6] = 10; polyTable[7] = 10;
    
    PolyDraw(&rp, 4, polyTable);

    /* Verify corners are set */
    color = ReadPixel(&rp, 10, 10);
    if (color != 1) {
        print("FAIL: Corner (10,10) not set\n");
        errors++;
    } else {
        print("OK: Corner (10,10) set\n");
    }

    color = ReadPixel(&rp, 20, 10);
    if (color != 1) {
        print("FAIL: Corner (20,10) not set\n");
        errors++;
    } else {
        print("OK: Corner (20,10) set\n");
    }

    color = ReadPixel(&rp, 20, 20);
    if (color != 1) {
        print("FAIL: Corner (20,20) not set\n");
        errors++;
    } else {
        print("OK: Corner (20,20) set\n");
    }

    color = ReadPixel(&rp, 10, 20);
    if (color != 1) {
        print("FAIL: Corner (10,20) not set\n");
        errors++;
    } else {
        print("OK: Corner (10,20) set\n");
    }

    /* Verify current position is at last point */
    if (rp.cp_x != 10 || rp.cp_y != 10) {
        print("FAIL: Current position not at (10,10)\n");
        errors++;
    } else {
        print("OK: Current position at (10,10)\n");
    }

    /* Test: Single point PolyDraw */
    SetRast(&rp, 0);
    Move(&rp, 30, 30);
    polyTable[0] = 35;
    polyTable[1] = 35;
    PolyDraw(&rp, 1, polyTable);

    color = ReadPixel(&rp, 35, 35);
    if (color != 1) {
        print("FAIL: Single point at (35,35) not set\n");
        errors++;
    } else {
        print("OK: Single point at (35,35) set\n");
    }

    /* Test: Zero count PolyDraw (should do nothing) */
    SetRast(&rp, 0);
    Move(&rp, 40, 40);
    PolyDraw(&rp, 0, polyTable);

    if (rp.cp_x != 40 || rp.cp_y != 40) {
        print("FAIL: Zero count changed position\n");
        errors++;
    } else {
        print("OK: Zero count preserved position\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("PASS: PolyDraw() all tests passed\n");
        return 0;
    } else {
        print("FAIL: PolyDraw() had errors\n");
        return 20;
    }
}
