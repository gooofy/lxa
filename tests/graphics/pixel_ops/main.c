/*
 * Test: graphics/pixel_ops
 * Tests WritePixel()/ReadPixel() for various colors and bounds
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

/* Drawing modes */
#ifndef JAM1
#define JAM1        0
#define JAM2        1
#define COMPLEMENT  2
#endif

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
    LONG result, color;
    int errors = 0;

    print("Testing WritePixel()/ReadPixel()...\n");

    /* Setup: 64x64 bitmap with 2 planes (4 colors) */
    InitBitMap(&bm, 2, 64, 64);
    bm.Planes[0] = AllocRaster(64, 64);
    bm.Planes[1] = AllocRaster(64, 64);

    if (!bm.Planes[0] || !bm.Planes[1]) {
        print("FAIL: Could not allocate raster planes\n");
        return 20;
    }

    InitRastPort(&rp);
    rp.BitMap = &bm;

    /* Clear using SetRast */
    SetRast(&rp, 0);

    /* Test WritePixel/ReadPixel with color 3 */
    SetAPen(&rp, 3);
    result = WritePixel(&rp, 10, 10);
    if (result != 0) {
        print("FAIL: WritePixel returned non-zero\n");
        errors++;
    }
    color = ReadPixel(&rp, 10, 10);
    if (color != 3) {
        print("FAIL: ReadPixel(10,10) != 3\n");
        errors++;
    } else {
        print("OK: WritePixel/ReadPixel color 3 at (10,10)\n");
    }

    /* Test with color 1 */
    SetAPen(&rp, 1);
    WritePixel(&rp, 20, 20);
    color = ReadPixel(&rp, 20, 20);
    if (color != 1) {
        print("FAIL: ReadPixel(20,20) != 1\n");
        errors++;
    } else {
        print("OK: WritePixel/ReadPixel color 1 at (20,20)\n");
    }

    /* Test with color 2 */
    SetAPen(&rp, 2);
    WritePixel(&rp, 30, 30);
    color = ReadPixel(&rp, 30, 30);
    if (color != 2) {
        print("FAIL: ReadPixel(30,30) != 2\n");
        errors++;
    } else {
        print("OK: WritePixel/ReadPixel color 2 at (30,30)\n");
    }

    /* Test with color 0 */
    SetAPen(&rp, 0);
    WritePixel(&rp, 40, 40);
    color = ReadPixel(&rp, 40, 40);
    if (color != 0) {
        print("FAIL: ReadPixel(40,40) != 0\n");
        errors++;
    } else {
        print("OK: WritePixel/ReadPixel color 0 at (40,40)\n");
    }

    /* Test ReadPixel out of bounds (negative X) */
    color = ReadPixel(&rp, -1, 0);
    if (color != -1) {
        print("FAIL: ReadPixel(-1,0) != -1\n");
        errors++;
    } else {
        print("OK: ReadPixel out of bounds X returns -1\n");
    }

    /* Test ReadPixel out of bounds (Y too large) */
    color = ReadPixel(&rp, 0, 100);
    if (color != -1) {
        print("FAIL: ReadPixel(0,100) != -1\n");
        errors++;
    } else {
        print("OK: ReadPixel out of bounds Y returns -1\n");
    }

    /* Test WritePixel out of bounds */
    result = WritePixel(&rp, -1, 0);
    if (result != -1) {
        print("FAIL: WritePixel(-1,0) != -1\n");
        errors++;
    } else {
        print("OK: WritePixel out of bounds returns -1\n");
    }

    /* Test corner pixels */
    SetAPen(&rp, 3);
    WritePixel(&rp, 0, 0);
    color = ReadPixel(&rp, 0, 0);
    if (color != 3) {
        print("FAIL: Corner pixel (0,0) != 3\n");
        errors++;
    } else {
        print("OK: Corner pixel (0,0) works\n");
    }

    WritePixel(&rp, 63, 63);
    color = ReadPixel(&rp, 63, 63);
    if (color != 3) {
        print("FAIL: Corner pixel (63,63) != 3\n");
        errors++;
    } else {
        print("OK: Corner pixel (63,63) works\n");
    }

    /* Test COMPLEMENT mode */
    SetAPen(&rp, 1);
    WritePixel(&rp, 50, 50);  /* Set pixel to color 1 */
    color = ReadPixel(&rp, 50, 50);
    if (color != 1) {
        print("FAIL: Initial color at (50,50) != 1\n");
        errors++;
    }

    SetDrMd(&rp, COMPLEMENT);
    SetAPen(&rp, 3);  /* XOR with 3 */
    WritePixel(&rp, 50, 50);
    color = ReadPixel(&rp, 50, 50);
    /* 1 XOR 3 = 2 */
    if (color == 1) {
        print("FAIL: COMPLEMENT mode did not change pixel\n");
        errors++;
    } else {
        print("OK: COMPLEMENT mode toggled pixel\n");
    }

    /* Cleanup */
    FreeRaster(bm.Planes[0], 64, 64);
    FreeRaster(bm.Planes[1], 64, 64);

    /* Final result */
    if (errors == 0) {
        print("PASS: WritePixel/ReadPixel all tests passed\n");
        return 0;
    } else {
        print("FAIL: WritePixel/ReadPixel had errors\n");
        return 20;
    }
}
